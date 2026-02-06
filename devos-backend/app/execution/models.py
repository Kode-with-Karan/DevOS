from __future__ import annotations

from enum import Enum
from typing import Optional, List

from pydantic import BaseModel, Field


class ExecutionStatus(str, Enum):
    PENDING = "PENDING"
    RUNNING = "RUNNING"
    FAILED = "FAILED"
    COMPLETED = "COMPLETED"


class CommandResult(BaseModel):
    command: str = Field(..., description="Executed command string")
    stdout: str = Field(..., description="Captured standard output")
    stderr: str = Field(..., description="Captured standard error")
    return_code: int = Field(..., description="Process return code")


class ExecutionResult(BaseModel):
    execution_id: str = Field(..., description="Unique execution identifier")
    status: ExecutionStatus = Field(..., description="Current execution status")
    project_path: str = Field(..., description="Absolute project path inside workspace")
    results: List[CommandResult] = Field(default_factory=list, description="Per-command results")
    error: Optional[str] = Field(default=None, description="Error message if execution failed")
