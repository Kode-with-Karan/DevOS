from __future__ import annotations

import re
from pathlib import Path
from typing import Iterable

from fastapi import HTTPException

from app.models.response_models import PlanResponse


class FileManager:
    """Safely create project structures within a fixed workspace."""

    def __init__(self, workspace: Path, max_files: int, max_file_size: int) -> None:
        self._workspace = workspace.resolve()
        self._max_files = max_files
        self._max_file_size = max_file_size

    def create_project(self, plan: PlanResponse) -> Path:
        project_name = self._sanitize_project_name(plan.project_name)
        project_root = (self._workspace / project_name).resolve()
        self._ensure_within_workspace(project_root)
        project_root.mkdir(parents=True, exist_ok=True)

        if len(plan.files) > self._max_files:
            raise HTTPException(status_code=400, detail="File count exceeds limit")

        # Create folders
        for folder in self._dedupe(plan.folders):
            target = self._project_path(project_root, folder)
            target.mkdir(parents=True, exist_ok=True)

        # Create files
        for file in plan.files:
            if len(file.content.encode("utf-8")) > self._max_file_size:
                raise HTTPException(status_code=400, detail="File size exceeds limit")

            target_file = self._project_path(project_root, file.path)
            target_file.parent.mkdir(parents=True, exist_ok=True)
            target_file.write_text(file.content, encoding="utf-8")

        return project_root

    def _project_path(self, project_root: Path, rel: str) -> Path:
        candidate = Path(rel)
        if candidate.is_absolute():
            raise HTTPException(status_code=400, detail="Absolute paths are not allowed")
        if ".." in candidate.parts:
            raise HTTPException(status_code=400, detail="Path traversal is not allowed")

        # If the path starts with project name, strip it to avoid duplication
        parts = list(candidate.parts)
        if parts and parts[0].lower() == project_root.name.lower():
            parts = parts[1:]
        safe_rel = Path(*parts) if parts else Path()

        resolved = (project_root / safe_rel).resolve()
        self._ensure_within_workspace(resolved)
        return resolved

    def _ensure_within_workspace(self, path: Path) -> None:
        try:
            path.relative_to(self._workspace)
        except ValueError as exc:
            raise HTTPException(status_code=400, detail="Path escapes workspace") from exc

    def _sanitize_project_name(self, name: str) -> str:
        sanitized = re.sub(r"[^A-Za-z0-9_-]+", "_", name.strip())
        if not sanitized:
            sanitized = "project"
        return sanitized

    @staticmethod
    def _dedupe(items: Iterable[str]) -> Iterable[str]:
        seen = set()
        for item in items:
            if item not in seen:
                seen.add(item)
                yield item
