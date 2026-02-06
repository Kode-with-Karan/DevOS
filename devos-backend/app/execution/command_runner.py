from __future__ import annotations

import asyncio
import logging
import re
from pathlib import Path
from typing import List

from fastapi import HTTPException

from app.execution.models import CommandResult


class CommandRunner:
    """Runs commands sequentially with safety checks and output capture."""

    def __init__(self, timeout_seconds: int = 300, max_output_bytes: int = 1_000_000) -> None:
        self._timeout = timeout_seconds
        self._max_output_bytes = max_output_bytes
        self._logger = logging.getLogger("devos.command_runner")

    async def run_commands(self, commands: List[str], workdir: Path) -> List[CommandResult]:
        if len(commands) == 0:
            return []
        results: List[CommandResult] = []
        for cmd in commands:
            await self._ensure_safe_command(cmd)
            result = await self._run_single(cmd, workdir)
            results.append(result)
            if result.return_code != 0:
                raise HTTPException(status_code=400, detail=f"Command failed: {cmd}")
        return results

    async def _run_single(self, command: str, workdir: Path) -> CommandResult:
        self._logger.info("Executing command: %s", command)
        process = await asyncio.create_subprocess_shell(
            command,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
            cwd=str(workdir),
        )

        stdout_chunks: List[bytes] = []
        stderr_chunks: List[bytes] = []

        async def _consume(stream, bucket: List[bytes]) -> None:
            while True:
                chunk = await stream.read(4096)
                if not chunk:
                    break
                bucket.append(chunk)
                if sum(len(b) for b in bucket) > self._max_output_bytes:
                    process.kill()
                    raise HTTPException(status_code=400, detail="Output size limit exceeded")

        try:
            await asyncio.wait_for(
                asyncio.gather(_consume(process.stdout, stdout_chunks), _consume(process.stderr, stderr_chunks)),
                timeout=self._timeout,
            )
            return_code = await asyncio.wait_for(process.wait(), timeout=1)
        except asyncio.TimeoutError as exc:
            process.kill()
            raise HTTPException(status_code=400, detail="Command timeout") from exc

        stdout_text = b"".join(stdout_chunks).decode("utf-8", errors="replace")
        stderr_text = b"".join(stderr_chunks).decode("utf-8", errors="replace")

        return CommandResult(
            command=command,
            stdout=stdout_text,
            stderr=stderr_text,
            return_code=return_code,
        )

    async def _ensure_safe_command(self, command: str) -> None:
        if not isinstance(command, str) or not command.strip():
            raise HTTPException(status_code=400, detail="Invalid command")

        text = command.strip()
        # Reject control operators that enable chaining/pipes/redirection.
        if re.search(r"[;&|><]", text):
            raise HTTPException(status_code=400, detail="Command contains forbidden control operators")

        # Reject newlines or carriage returns to prevent multi-command injections.
        if "\n" in text or "\r" in text:
            raise HTTPException(status_code=400, detail="Command contains newline characters")

        # Reject absolute Windows or POSIX paths to reduce escape risk.
        if re.search(r"^[a-zA-Z]:\\", text) or text.startswith("/"):
            raise HTTPException(status_code=400, detail="Absolute paths not allowed in commands")
