from __future__ import annotations

import asyncio
import logging
import uuid
from pathlib import Path
from typing import Dict

from fastapi import HTTPException

from app.config import get_settings
from app.execution.command_runner import CommandRunner
from app.execution.file_manager import FileManager
from app.execution.models import CommandResult, ExecutionResult, ExecutionStatus
from app.models.response_models import PlanResponse
from app.services.plan_service import validate_commands


EXECUTIONS: Dict[str, ExecutionResult] = {}
EXECUTION_TASKS: Dict[str, asyncio.Task] = {}


class ExecutionEngine:
    MAX_COMMANDS = 20
    MAX_FILES = 200
    MAX_FILE_SIZE = 1_000_000  # 1 MB per file
    MAX_OUTPUT_BYTES = 1_000_000  # total per command

    def __init__(self) -> None:
        self._logger = logging.getLogger("devos.executor")
        settings = get_settings()
        self._workspace = Path(settings.devos_workspace).resolve()
        self._workspace.mkdir(parents=True, exist_ok=True)
        self._file_manager = FileManager(self._workspace, self.MAX_FILES, self.MAX_FILE_SIZE)
        self._runner = CommandRunner(max_output_bytes=self.MAX_OUTPUT_BYTES)

    def start_execution(self, plan: PlanResponse) -> str:
        execution_id = str(uuid.uuid4())
        EXECUTIONS[execution_id] = ExecutionResult(
            execution_id=execution_id,
            status=ExecutionStatus.PENDING,
            project_path="",
            results=[],
            error=None,
        )

        task = asyncio.create_task(self._run_execution(execution_id, plan))
        EXECUTION_TASKS[execution_id] = task
        return execution_id

    def get_execution(self, execution_id: str) -> ExecutionResult:
        result = EXECUTIONS.get(execution_id)
        if not result:
            raise HTTPException(status_code=404, detail="Execution not found")
        return result

    async def _run_execution(self, execution_id: str, plan: PlanResponse) -> None:
        record = EXECUTIONS[execution_id]
        record.status = ExecutionStatus.RUNNING
        EXECUTIONS[execution_id] = record

        try:
            validate_commands(plan.commands)
            project_root = self._file_manager.create_project(plan)
            record.project_path = str(project_root)

            command_results: list[CommandResult] = []
            if plan.commands:
                # Execute commands in batches to respect MAX_COMMANDS per batch
                total = len(plan.commands)
                batch_size = self.MAX_COMMANDS
                batches = (total + batch_size - 1) // batch_size
                for idx in range(batches):
                    start = idx * batch_size
                    end = min(start + batch_size, total)
                    batch = plan.commands[start:end]
                    self._logger.info("Executing batch %d/%d: %d commands", idx + 1, batches, len(batch))
                    batch_results = await self._runner.run_commands(batch, project_root)
                    command_results.extend(batch_results)
                    # brief pause between batches
                    if idx + 1 < batches:
                        await asyncio.sleep(0.25)

            record.results = command_results
            record.status = ExecutionStatus.COMPLETED
            record.error = None
        except HTTPException as exc:
            record.status = ExecutionStatus.FAILED
            record.error = exc.detail if isinstance(exc.detail, str) else str(exc.detail)
        except Exception as exc:
            self._logger.exception("Execution failed: %s", exc)
            record.status = ExecutionStatus.FAILED
            record.error = str(exc)
        finally:
            EXECUTIONS[execution_id] = record
