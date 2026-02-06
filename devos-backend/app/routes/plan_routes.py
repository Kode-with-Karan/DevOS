from __future__ import annotations

import logging

from fastapi import APIRouter, HTTPException

from app.models.request_models import GeneratePlanRequest
from app.models.response_models import PlanResponse
from app.services.plan_service import PlanService

router = APIRouter(tags=["plans"])


def _get_plan_service() -> PlanService:
    return PlanService()


@router.post("/generate-plan", response_model=PlanResponse)
async def generate_plan(payload: GeneratePlanRequest) -> PlanResponse:
    logger = logging.getLogger("devos.plan")
    prompt = payload.prompt.strip()

    if prompt == "":
        raise HTTPException(status_code=400, detail="prompt must not be empty")

    logger.info("Prompt received: %s", prompt)
    service = _get_plan_service()
    try:
        plan = await service.generate_plan(prompt=prompt)
    except HTTPException:
        raise
    except Exception as exc:
        logger.exception("Plan generation failed: %s", exc)
        raise HTTPException(status_code=500, detail="Internal Server Error") from exc

    logger.info("Plan generated for project_name=%s", plan.project_name)
    return plan
