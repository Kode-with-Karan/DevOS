# DevOS Backend

FastAPI backend that accepts a natural language project setup prompt and returns a structured JSON execution plan.

Phase 2 integrates OpenRouter (GPT-5.2) for structured JSON plan generation. AI/RAG enhancements can be added later.
Phase 3 adds a secure execution engine that materializes plans, runs commands, and tracks execution status.

## Requirements

- Python 3.11+

## Setup

From the `devos-backend/` directory:

```bash
python -m venv venv
```

### Windows (PowerShell)

```powershell
.\venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
pip install -r requirements.txt
```

### macOS / Linux

```bash
source venv/bin/activate
python -m pip install --upgrade pip
pip install -r requirements.txt
```

## Run

```bash
uvicorn app.main:app --reload --host 127.0.0.1 --port 8000
```

CORS is enabled for:

- http://localhost:3000
- http://localhost:5173

## API

### POST /generate-plan

#### Example request

```bash
curl -X POST http://127.0.0.1:8000/generate-plan \
  -H "Content-Type: application/json" \
  -d "{\"prompt\": \"Create a Django REST API with PostgreSQL and JWT\"}"
```

#### Example response

```json
{
  "project_name": "django_rest_api",
  "base_path": "C:/DevProjects",
  "folders": [
    "django_rest_api",
    "django_rest_api/app",
    "django_rest_api/app/routes"
  ],
  "files": [
    {
      "path": "django_rest_api/manage.py",
      "content": "# Django starter placeholder\n"
    }
  ],
  "commands": [
    "python -m venv venv",
    "venv\\Scripts\\activate",
    "pip install django djangorestframework psycopg2-binary",
    "django-admin startproject django_rest_api",
    "python manage.py runserver"
  ],
  "ide": "vscode"
}
```

## Phase 2: OpenRouter Integration (GPT-5.2)

This project calls OpenRouter's GPT-5.2 to generate structured JSON plans. Configure it as follows:

1. Create a `.env` file in `devos-backend/` (do not commit secrets):

```
OPENROUTER_API_KEY=your_api_key_here
OPENROUTER_MODEL=openai/gpt-5.2
OPENROUTER_BASE_URL=https://openrouter.ai/api/v1
DEVOS_WORKSPACE=./workspace
```

2. Install dependencies:

```powershell
pip install -r requirements.txt
```

3. Security notes:
- The system validates all generated shell `commands` and rejects any plan containing destructive operations such as `rm -rf`, `del /`, `format`, `shutdown`, `sudo`, `chmod 777`, references to `system32`, or PowerShell destructive scripts (including `powershell Remove-Item`).
- Only a strict whitelist of safe command starters is permitted; unsafe commands will cause the service to retry the LLM up to 3 times and fail if a safe plan cannot be produced.

4. Architecture:
- `app.services.llm_client.OpenRouterClient` encapsulates OpenRouter API calls over HTTPX.
- `app.services.plan_service.PlanService` orchestrates prompt creation, LLM calls, JSON parsing, schema validation, and command safety checks with retry logic.
- `app.execution.executor.ExecutionEngine` coordinates file creation and command execution.
- `app.execution.file_manager.FileManager` enforces workspace-safe file writes.
- `app.execution.command_runner.CommandRunner` runs commands with timeouts and output limits.


## Error handling

- Empty prompt returns HTTP 400 with `{"detail": "prompt must not be empty"}`.
- Unexpected errors return HTTP 500 with `{"detail": "Internal Server Error"}`.

## Execution Engine (Phase 3)

- Workspace is configured via `DEVOS_WORKSPACE` (default `./workspace`). The folder is created at startup and all writes/commands are confined inside it.
- To execute a plan:
  1) `POST /generate-plan` to obtain a `PlanResponse`.
  2) `POST /execute-plan` with the `PlanResponse` body. Response returns `execution_id`.
  3) Poll `GET /execution/{execution_id}` for status and per-command results.
- Safety limits: max 20 commands, max 200 files, max 1MB per file, max ~1MB per command output.
- Commands run sequentially with a 5-minute timeout each; failure, timeout, or safety violation marks the execution FAILED.

Example execute request (assuming you already have a plan JSON saved as `plan.json`):

```bash
curl -X POST http://127.0.0.1:8000/execute-plan \
  -H "Content-Type: application/json" \
  --data-binary @plan.json
```

Example status poll:

```bash
curl http://127.0.0.1:8000/execution/{execution_id}
```
