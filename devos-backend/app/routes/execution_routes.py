from __future__ import annotations

import logging

from fastapi import APIRouter, HTTPException

from app.execution.executor import ExecutionEngine
from app.execution.models import ExecutionResult
from app.models.response_models import PlanResponse

router = APIRouter(tags=["execution"])


_engine = ExecutionEngine()


@router.post("/execute-plan")
async def execute_plan(plan: PlanResponse) -> dict:
    logger = logging.getLogger("devos.execute")
    try:
        execution_id = _engine.start_execution(plan)
        logger.info("Execution started id=%s", execution_id)
        return {"execution_id": execution_id}
    except HTTPException:
        raise
    except Exception as exc:
        logger.exception("Failed to start execution: %s", exc)
        raise HTTPException(status_code=500, detail="Failed to start execution") from exc


@router.get("/execution/{execution_id}", response_model=ExecutionResult)
async def get_execution(execution_id: str) -> ExecutionResult:
    return _engine.get_execution(execution_id)
