from __future__ import annotations

from contextlib import asynccontextmanager
from typing import Any, Awaitable, Callable

from fastapi import FastAPI, Request
from fastapi.exceptions import RequestValidationError
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse

from app.config import get_settings
from app.routes.plan_routes import router as plan_router
from app.utils.logger import configure_logging


def _is_empty_prompt_validation_error(exc: RequestValidationError) -> bool:
    for err in exc.errors():
        loc = err.get("loc")
        input_value = err.get("input")
        if loc == ("body", "prompt") and isinstance(input_value, str) and input_value.strip() == "":
            return True
    return False


@asynccontextmanager
async def lifespan(_: FastAPI):
    configure_logging()
    settings = get_settings()
    # Ensure workspace exists
    import os
    os.makedirs(settings.devos_workspace, exist_ok=True)
    yield


def create_app() -> FastAPI:
    settings = get_settings()

    app = FastAPI(title="DevOS Backend", version="0.1.0", lifespan=lifespan)

    app.add_middleware(
        CORSMiddleware,
        allow_origins=settings.cors_allow_origins,
        allow_credentials=True,
        allow_methods=["*"],
        allow_headers=["*"],
    )

    @app.middleware("http")
    async def request_logging_middleware(
        request: Request,
        call_next: Callable[[Request], Awaitable[Any]],
    ):
        import time
        import logging

        logger = logging.getLogger("devos.request")
        start = time.perf_counter()
        try:
            response = await call_next(request)
        finally:
            duration_ms = (time.perf_counter() - start) * 1000
            logger.info(
                "%s %s completed in %.2fms",
                request.method,
                request.url.path,
                duration_ms,
            )
        return response

    @app.exception_handler(RequestValidationError)
    async def validation_exception_handler(_: Request, exc: RequestValidationError):
        if _is_empty_prompt_validation_error(exc):
            return JSONResponse(
                status_code=400,
                content={"detail": "prompt must not be empty"},
            )
        return JSONResponse(status_code=422, content={"detail": exc.errors()})

    @app.exception_handler(Exception)
    async def unhandled_exception_handler(_: Request, exc: Exception):
        import logging

        logging.getLogger("devos.error").exception("Unhandled error: %s", exc)
        return JSONResponse(status_code=500, content={"detail": "Internal Server Error"})

    app.include_router(plan_router)
    from app.routes.execution_routes import router as execution_router
    app.include_router(execution_router)
    return app


app = create_app()
