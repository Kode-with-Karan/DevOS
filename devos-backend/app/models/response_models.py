from __future__ import annotations

from typing import List

from pydantic import BaseModel, ConfigDict, Field


class FileModel(BaseModel):
    """Represents a file to be created as part of the plan."""

    model_config = ConfigDict(extra="forbid", strict=True)

    path: str = Field(..., min_length=1)
    content: str = Field(...)


class PlanResponse(BaseModel):
    """Structured response representing the full execution plan."""

    model_config = ConfigDict(extra="forbid", strict=True)

    project_name: str = Field(..., min_length=1)
    base_path: str = Field(..., min_length=1)
    folders: List[str] = Field(...)
    files: List[FileModel] = Field(...)
    commands: List[str] = Field(...)
    ide: str = Field(..., min_length=1)
