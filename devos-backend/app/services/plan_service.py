from __future__ import annotations

import json
import logging
import os
import re
import time
from typing import Any, Dict, List, Optional

from fastapi import HTTPException
from pydantic import ValidationError

from app.config import get_settings
from app.models.response_models import PlanResponse
from app.services.llm_client import OpenRouterClient


class CommandValidationError(ValueError):
    pass


def validate_commands(commands: list[str]) -> None:
    """Validate that commands are safe and match a strict whitelist.

    Raises CommandValidationError if an unsafe or disallowed command is found.
    """
    forbidden_patterns = PlanService.FORBIDDEN_PATTERNS
    allowed_starters = PlanService.ALLOWED_STARTERS

    if not isinstance(commands, list):
        raise CommandValidationError("commands must be a list of strings")

    def is_relative_cd(text: str) -> bool:
        stripped = text.strip()
        if not stripped.lower().startswith("cd "):
            return False
        target = stripped[3:].strip().strip('"').strip("'")
        if not target:
            return True
        lower_target = target.lower()
        if re.match(r"^[a-z]:", lower_target) or lower_target.startswith("/") or lower_target.startswith("\\"):
            return False
        if ".." in target.replace("\\", "/").split("/"):
            return False
        return True

    for cmd in commands:
        if not isinstance(cmd, str):
            raise CommandValidationError("each command must be a string")

        lower = cmd.lower()
        for pat in forbidden_patterns:
            if re.search(pat, lower):
                raise CommandValidationError(f"forbidden command pattern detected: {pat}")

        stripped = lower.strip()

        if is_relative_cd(cmd):
            continue

        if "*" in stripped and any(tok in stripped for tok in ["rm ", "del ", "rmdir", "rd "]):
            raise CommandValidationError("wildcard destructive pattern detected")

        if stripped == "venv\\scripts\\activate" or stripped.startswith("source "):
            continue

        if any(stripped.startswith(starter) for starter in allowed_starters):
            continue

        raise CommandValidationError(f"command not allowed or unsafe: {cmd}")


class PlanService:
    """Service that generates execution plans using OpenRouter (GPT-5.2).

    Responsibilities:
    - Craft a strict system prompt
    - Call the OpenRouter API and obtain JSON-only responses
    - Parse and validate the JSON against Pydantic models
    - Validate command safety
    - Retry on malformed or unsafe responses (max 3 attempts)
    """

    FORBIDDEN_PATTERNS = [
        r"\brm\s+-rf\b",
        r"\bdel\s+/",
        r"\bformat\b",
        r"\bshutdown\b",
        r"\bnet\s+user\b",
        r"\bsudo\b",
        r"\bchmod\s+777\b",
        r"system32",
        r"powershell\s+remove-item",
    ]

    # Strict whitelist: allowed command starters (case-insensitive)
    # NOTE: "git init" is allowed, not arbitrary git commands. Venv activation allowed explicitly.
    ALLOWED_STARTERS = [
        "python",
        "pip",
        "npm",
        "npx",
        "yarn",
        "django-admin",
        "dotnet",
        "cargo",
        "flutter",
        "git init",
        "type",
        "echo",
        "code",
        "mkdir",
        "venv\\scripts\\activate",
        "source",
    ]

    def __init__(self) -> None:
        self._logger = logging.getLogger("devos.plan_service")
        self._settings = get_settings()
        self._llm = OpenRouterClient(
            api_key=self._settings.openrouter_api_key,
            model=self._settings.openrouter_model,
            base_url=self._settings.openrouter_base_url,
        )

    async def generate_plan(self, prompt: str) -> PlanResponse:
        """Generate a validated PlanResponse using the LLM with retries."""
        self._logger.info("Generating plan for prompt")
        self._logger.debug("Prompt: %s", prompt)

        # Fast path: optional env to bypass LLM and use deterministic fallback immediately
        force_fallback = os.getenv("DEVOS_FORCE_FALLBACK", "").lower() in {"1", "true", "yes"}
        if force_fallback:
            self._logger.info("DEVOS_FORCE_FALLBACK=1 set; returning fallback plan without LLM")
            return self._build_fallback_plan(prompt)

        if not self._settings.openrouter_api_key:
            self._logger.error("OPENROUTER_API_KEY is not configured")
            raise HTTPException(status_code=500, detail="OpenRouter API key not configured")

        system_prompt = self._build_system_prompt()

        max_attempts = 1
        last_error: Optional[Exception] = None
        for attempt in range(1, max_attempts + 1):
            self._logger.info("LLM request attempt %d/%d", attempt, max_attempts)
            try:
                raw_content = await self.call_llm(system_prompt=system_prompt, prompt=prompt)
                parsed = self.parse_response(raw_content)
                plan = PlanResponse.model_validate(parsed)
                plan = self._sanitize_plan(plan, prompt)
                validate_commands(plan.commands)
                self._logger.info(
                    "Plan generation successful (structure only): %s commands, %s files, base_path=%s",
                    len(plan.commands),
                    len(plan.files),
                    plan.base_path,
                )
                return plan
            except (json.JSONDecodeError, ValidationError, CommandValidationError, ValueError) as exc:
                last_error = exc
                # Log a short snippet of the raw content to aid debugging
                snippet = raw_content[:300].replace("\n", " ") if 'raw_content' in locals() else "<no content>"
                self._logger.warning(
                    "Attempt %d: Response validation failed: %s | snippet=%s",
                    attempt,
                    exc,
                    snippet,
                )
                time.sleep(0.5 * attempt)
                continue
            except Exception as exc:
                last_error = exc
                self._logger.exception("Attempt %d: Unexpected error", attempt)
                time.sleep(0.5 * attempt)
                continue

        self._logger.error("Failed to generate a valid plan after %d attempts; using fallback", max_attempts)
        fallback = self._build_fallback_plan(prompt)
        return fallback

    def _build_system_prompt(self) -> str:
        return (
            "You are DevOS, a secure development planning engine. "
            "You generate project scaffolding plans with NO source code. "
            "Output ONLY valid JSON. Do not include markdown. Do not include commentary.\n"
            "Required JSON schema: {\n  \"project_name\": \"string\",\n  \"base_path\": \"string\",\n  \"folders\": [\"string\"],\n  \"files\": [{\"path\": \"string\", \"content\": \"string\"}],\n  \"commands\": [\"string\"],\n  \"ide\": \"string\"\n}\n"
            "Rules: file contents must be empty strings; do NOT generate code or boilerplate text. Keep file count small, focus on folder tree. "
            "All commands must be Windows-compatible. Never include destructive commands. Never include system-level modifications. Never use sudo. Never reference system32. Never delete root directories. Never use wildcard destructive patterns. Use only relative paths under the project, never absolute paths. Any 'cd' must be relative (no drive letters, no root, no ..).\n"
            "Return only the JSON object and nothing else."
        )

    async def call_llm(self, system_prompt: str, prompt: str) -> str:
        self._logger.info("LLM request start")
        resp = await self._llm.chat(system_prompt=system_prompt, user_prompt=prompt, max_tokens=1500)
        try:
            content = resp["choices"][0]["message"]["content"]
        except Exception as exc:
            raise ValueError("OpenRouter response missing expected content") from exc
        return content or ""

    def parse_response(self, raw: str) -> Dict[str, Any]:
        text = raw.strip()

        # If wrapped in Markdown fences, strip them.
        if text.startswith("```"):
            # remove first fence
            text = text.split("\n", 1)[-1]
            if "```" in text:
                text = text.rsplit("```", 1)[0]
            text = text.strip()

        # Try to extract the first JSON object in the text.
        if not text.startswith("{"):
            start = text.find("{")
            end = text.rfind("}")
            if start != -1 and end != -1 and end > start:
                text = text[start : end + 1].strip()

        return json.loads(text)

    def _build_fallback_plan(self, prompt: str) -> PlanResponse:
        """Generate a deterministic, safe fallback plan with boilerplate commands."""
        proj_slug = self._slugify(prompt)
        base_path = f"./{proj_slug}"

        # Language-aware minimal scaffold to return quickly and deterministically
        lang = self._detect_language(prompt)
        if lang == "django":
            folders, files, commands = self._scaffold_django(proj_slug)
        elif lang == "flask":
            folders, files, commands = self._scaffold_flask(proj_slug)
        elif lang == "node-react":
            folders, files, commands = self._scaffold_node_react(proj_slug)
        elif lang == "node-express":
            folders, files, commands = self._scaffold_node_express(proj_slug)
        elif lang == "rust":
            folders, files, commands = self._scaffold_rust(proj_slug)
        elif lang == "go":
            folders, files, commands = self._scaffold_go(proj_slug)
        elif lang == "dotnet":
            folders, files, commands = self._scaffold_dotnet(proj_slug)
        elif lang == "flutter":
            folders, files, commands = self._scaffold_flutter(proj_slug)
        else:
            folders = self._default_folders()
            files = self._default_files()
            commands = self._default_commands(proj_slug)

        plan = PlanResponse(
            project_name=proj_slug,
            base_path=base_path,
            folders=folders,
            files=files,
            commands=commands,
            ide="VSCode",
        )
        self._logger.info(
            "Fallback plan generated: %s commands, %s files, base_path=%s",
            len(plan.commands),
            len(plan.files),
            plan.base_path,
        )
        return plan

    def _detect_language(self, prompt: str) -> str:
        text = (prompt or "").lower()
        if "django" in text or "python django" in text:
            return "django"
        if "flask" in text:
            return "flask"
        if "react" in text and ("node" in text or "npm" in text):
            return "node-react"
        if "express" in text or ("node" in text and "api" in text):
            return "node-express"
        if "rust" in text:
            return "rust"
        if "go" in text or "golang" in text:
            return "go"
        if "dotnet" in text or "asp.net" in text or "c#" in text:
            return "dotnet"
        if "flutter" in text or "dart" in text:
            return "flutter"
        return "generic"

    def _scaffold_django(self, slug: str):
        folders = [
            "backend",
            "backend/project",
            "backend/apps",
            "frontend",
            "docs",
        ]
        files = [
            {"path": "README.md", "content": ""},
            {"path": "backend/manage.py", "content": ""},
            {"path": "backend/project/__init__.py", "content": ""},
            {"path": "backend/apps/.gitkeep", "content": ""},
        ]
        commands = [
            f"mkdir {slug}",
            f"cd {slug}",
            "mkdir backend",
            "mkdir frontend",
            "cd backend",
            "python -m venv .venv",
            "venv\\Scripts\\activate",
            "pip install django",
            "django-admin startproject project .",
        ]
        return folders, files, commands

    def _scaffold_flask(self, slug: str):
        folders = ["backend", "backend/app", "frontend", "docs"]
        files = [
            {"path": "README.md", "content": ""},
            {"path": "backend/app/__init__.py", "content": ""},
        ]
        commands = [
            f"mkdir {slug}",
            f"cd {slug}",
            "mkdir backend",
            "mkdir frontend",
            "cd backend",
            "python -m venv .venv",
            "venv\\Scripts\\activate",
            "pip install flask",
        ]
        return folders, files, commands

    def _scaffold_node_react(self, slug: str):
        folders = ["frontend", "frontend/src", "backend", "docs"]
        files = [
            {"path": "README.md", "content": ""},
            {"path": "frontend/src/index.js", "content": ""},
        ]
        commands = [
            f"mkdir {slug}",
            f"cd {slug}",
            "mkdir frontend",
            "cd frontend",
            "npm create vite@latest . -- --template react",
            "npm install",
        ]
        return folders, files, commands

    def _scaffold_node_express(self, slug: str):
        folders = ["backend", "backend/src", "frontend", "docs"]
        files = [
            {"path": "README.md", "content": ""},
            {"path": "backend/src/index.js", "content": ""},
        ]
        commands = [
            f"mkdir {slug}",
            f"cd {slug}",
            "mkdir backend",
            "cd backend",
            "npm init -y",
            "npm install express",
        ]
        return folders, files, commands

    def _scaffold_rust(self, slug: str):
        folders = ["src", "docs"]
        files = [{"path": "README.md", "content": ""}, {"path": "src/main.rs", "content": ""}]
        commands = [f"mkdir {slug}", f"cd {slug}", "cargo init --bin"]
        return folders, files, commands

    def _scaffold_go(self, slug: str):
        folders = ["cmd", "pkg", "internal", "docs"]
        files = [{"path": "README.md", "content": ""}, {"path": "cmd/main.go", "content": ""}]
        commands = [f"mkdir {slug}", f"cd {slug}", "go mod init %s" % slug]
        return folders, files, commands

    def _scaffold_dotnet(self, slug: str):
        folders = ["src", "tests", "docs"]
        files = [{"path": "README.md", "content": ""}]
        commands = [f"mkdir {slug}", f"cd {slug}", "dotnet new console -o src"]
        return folders, files, commands

    def _scaffold_flutter(self, slug: str):
        folders = ["app", "lib", "docs"]
        files = [{"path": "README.md", "content": ""}]
        commands = [f"mkdir {slug}", f"cd {slug}", "flutter create ."]
        return folders, files, commands

    def _slugify(self, name: str) -> str:
        cleaned = re.sub(r"[^a-zA-Z0-9\s_-]", "", name).strip().lower()
        cleaned = re.sub(r"\s+", "-", cleaned)
        return (cleaned or "project")[:40]

    def _default_folders(self) -> List[str]:
        return [
            "frontend",
            "frontend/src",
            "backend",
            "backend/app",
            "docs",
        ]

    def _default_files(self) -> List[Dict[str, str]]:
        return [
            {"path": "README.md", "content": ""},
            {"path": "docs/plan.md", "content": ""},
            {"path": "frontend/src/.gitkeep", "content": ""},
            {"path": "backend/app/__init__.py", "content": ""},
        ]

    def _default_commands(self, slug: str) -> List[str]:
        return [
            f"mkdir {slug}",
            f"cd {slug}",
            "mkdir frontend",
            "mkdir backend",
            "mkdir docs",
            "cd frontend && npm init -y",
            "cd backend && python -m venv .venv",
            "cd backend && venv\\Scripts\\activate",
        ]

    def _sanitize_plan(self, plan: PlanResponse, prompt: str) -> PlanResponse:
        slug = self._slugify(prompt)

        base_path = (plan.base_path or "").strip()
        if not base_path or base_path.startswith("/" ) or re.match(r"^[a-z]:", base_path.lower()):
            base_path = f"./{slug}"
        if base_path.startswith(".\\"):
            base_path = base_path.replace("\\", "/")
        if not base_path.startswith("./"):
            base_path = f"./{slug}"

        folders = plan.folders or []
        if not folders:
            folders = self._default_folders()
        else:
            folders = list(dict.fromkeys(folders))

        files = [{"path": f.path, "content": ""} for f in plan.files] if plan.files else []
        if not files:
            files = self._default_files()

        commands = plan.commands or []
        if not commands:
            commands = self._default_commands(slug)

        # Ensure directories and files from the plan have creation commands.
        existing = [c.strip() for c in commands]
        existing_lc = [c.lower() for c in existing]

        # Collect required parent dirs for files
        required_dirs: set[str] = set()
        for f in files:
            p = f.get("path", "")
            if not p:
                continue
            parent = os.path.dirname(p).replace("/", "\\")
            if parent:
                required_dirs.add(parent)

            # Split creation commands into dir-creation (early) and file-creation (late)
            dir_cmds: list[str] = []
            file_cmds: list[str] = []
            SKIP_TYPE_BASENAMES = {"package.json", "package-lock.json", "yarn.lock", "tailwind.config.js", "postcss.config.js"}

            for d in sorted(required_dirs):
                cmd = f"mkdir {d}"
                d_lc = d.lower()
                if not any(ec.startswith("mkdir ") and ec.endswith(d_lc) for ec in existing_lc):
                    dir_cmds.append(cmd)

            for f in files:
                p = f.get("path", "")
                if not p:
                    continue
                p_bs = p.replace("/", "\\")
                base = os.path.basename(p).lower()
                if base in SKIP_TYPE_BASENAMES:
                    continue
                cmd = f"type nul > {p_bs}"
                if not any(ec.startswith("type ") and ec.endswith(p_bs.lower()) for ec in existing_lc):
                    file_cmds.append(cmd)

            # Insert dir creation commands after the initial `cd <project>` if present, else at front
            insert_at = 0
            for i, c in enumerate(commands):
                if c.strip().lower().startswith("cd "):
                    insert_at = i + 1
                    break
            if dir_cmds:
                commands[insert_at:insert_at] = dir_cmds

            # Append file creation commands at the end to avoid interfering with scaffolders/installers
            if file_cmds:
                commands.extend(file_cmds)

        return plan.model_copy(update={
            "project_name": plan.project_name or slug,
            "base_path": base_path,
            "folders": folders,
            "files": files,
            "commands": commands,
        })



