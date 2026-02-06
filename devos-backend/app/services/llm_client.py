from __future__ import annotations

import logging
from typing import Any, Dict, List, Optional

import httpx


class OpenRouterClient:
    """Async client for calling OpenRouter chat completions."""

    def __init__(self, api_key: Optional[str], model: str, base_url: str) -> None:
        self._logger = logging.getLogger("devos.llm_client")
        if not api_key:
            self._logger.warning("No OpenRouter API key provided; LLM calls will fail until configured.")
        self._api_key = api_key
        self._model = model
        self._base_url = base_url.rstrip("/")

    async def chat(self, system_prompt: str, user_prompt: str, max_tokens: int = 1500) -> Dict[str, Any]:
        if not self._api_key:
            raise RuntimeError("OpenRouter API key not configured")

        url = f"{self._base_url}/chat/completions"
        headers = {
            "Authorization": f"Bearer {self._api_key}",
            "Content-Type": "application/json",
            "HTTP-Referer": "http://localhost:8000",
            "X-Title": "DevOS",
        }
        payload: Dict[str, Any] = {
            "model": self._model,
            "messages": [
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_prompt},
            ],
            "temperature": 0.0,
            "max_tokens": max_tokens,
        }

        self._logger.info("Calling OpenRouter model=%s", self._model)
        async with httpx.AsyncClient(timeout=30.0) as client:
            resp = await client.post(url, headers=headers, json=payload)

        if resp.status_code != 200:
            self._logger.error("OpenRouter call failed status=%s body=%s", resp.status_code, resp.text[:500])
            raise RuntimeError(f"OpenRouter call failed with status {resp.status_code}")

        try:
            data = resp.json()
        except Exception as exc:
            self._logger.exception("Failed to decode OpenRouter response JSON")
            raise RuntimeError("Failed to decode OpenRouter response JSON") from exc

        self._logger.info("OpenRouter response received (keys=%s)", list(data.keys()))
        return data
