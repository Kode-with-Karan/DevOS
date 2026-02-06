from __future__ import annotations

from pydantic import BaseModel, ConfigDict, Field


class GeneratePlanRequest(BaseModel):
    """Request payload for generating a project execution plan."""

    model_config = ConfigDict(extra="forbid", strict=True)

    prompt: str = Field(..., description="Natural language project setup prompt")
