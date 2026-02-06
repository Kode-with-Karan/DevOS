from __future__ import annotations

from dataclasses import dataclass
from typing import List, Optional

from dotenv import load_dotenv
import os


load_dotenv()


@dataclass(frozen=True, slots=True)
class Settings:
    cors_allow_origins: List[str]
    default_base_path: str
    default_ide: str
    openrouter_api_key: Optional[str]
    openrouter_model: str
    openrouter_base_url: str
    devos_workspace: str


def get_settings() -> Settings:
    return Settings(
        cors_allow_origins=["http://localhost:3000", "http://localhost:5173"],
        default_base_path=os.getenv("DEVOS_BASE_PATH", "C:/DevProjects"),
        default_ide=os.getenv("DEVOS_IDE", "vscode"),
        openrouter_api_key=os.getenv("OPENROUTER_API_KEY"),
        openrouter_model=os.getenv("OPENROUTER_MODEL", "openai/gpt-5.2"),
        openrouter_base_url=os.getenv("OPENROUTER_BASE_URL", "https://openrouter.ai/api/v1"),
        devos_workspace=os.getenv("DEVOS_WORKSPACE", "./workspace"),
    )
