 # DevOS — Project Scaffolding & Bootstrapper

Welcome to DevOS — a local-first project scaffolding tool that turns short goals into complete, safe, and executable project plans. DevOS combines a Python/FastAPI backend with a Qt6 desktop frontend to give you an interactive demo-ready experience.

🚀 Quick demo (recommended)
1. Start the backend (quick fallback mode):

```powershell
cd devos-backend
$env:DEVOS_FORCE_FALLBACK = "1"
D:/python_project/DevOS/.venv/Scripts/python.exe -m uvicorn app.main:app --reload
```
2. Launch the Qt frontend (built executable or run from IDE).
3. Paste this prompt into the UI prompt box and click Generate:

```
Create a Django ecommerce site with a React frontend.
```
4. Click Execute → select a target folder. DevOS will run scaffolders safely and open your IDE when finished.

---

## What makes DevOS special
- Language-aware deterministic scaffolds for instant responses.
- Safe execution: command validation + sandboxing + batched execution for long plans.
- UI-first experience: file tree, logs, command list, and F11 fullscreen toggle.

## Project layout (high level)
- `devos-backend/` — FastAPI backend that generates and executes plans.
- `DevOS/` — Qt6 C++ desktop frontend (UI in `DevOS/ui`, core helpers in `DevOS/core`).

Important backend files:
- `app/services/plan_service.py` — plan generation, language detection, and fallback scaffolds.
- `app/services/llm_client.py` — OpenRouter client.
- `app/routes/plan_routes.py` — `/generate-plan`.
- `app/routes/execution_routes.py` — `/execute-plan` and execution status.
- `app/execution/executor.py` — batches and runs commands.
- `app/execution/command_runner.py` — runs commands with timeouts and output capture.

Frontend touchpoints:
- `DevOS/ui/MainWindow.cpp` — main UI and execute flow.
- `DevOS/ui/FileTreeWidget.cpp` — renders file tree.
- `DevOS/core/ApiClient.cpp` — HTTP client used by the UI.

---

## Try it — Interactive steps & commands

Start backend (normal mode):

```powershell
cd devos-backend
D:/python_project/DevOS/.venv/Scripts/python.exe -m pip install -r requirements.txt
D:/python_project/DevOS/.venv/Scripts/python.exe -m uvicorn app.main:app --reload
```

Generate a plan (example):

```powershell
Invoke-RestMethod -Method Post -Uri http://localhost:8000/generate-plan -ContentType "application/json" -Body '{"prompt":"Create a Django ecommerce site with React frontend"}'
```

Execute the plan from the UI: Generate → Execute → pick a folder → watch the Logs.

If target folder is non-empty and the scaffolder needs an empty directory, DevOS creates a subfolder `<project_name>` and runs the scaffolder there to avoid conflicts.

---

## Recommended prompt template
Paste this into the UI for deterministic, demo-friendly results:

```
You are DevOS. Output ONLY a JSON object matching this schema:
{
	"project_name": "string",
	"base_path": "string",
	"folders": ["string"],
	"files": [{"path":"string","content":"string"}],
	"commands": ["string"],
	"ide": "string"
}

Rules: no source code in `files[].content` unless requested; place scaffolder/installer commands first, then mkdirs, then file-creation commands last. Use Windows-safe commands and relative paths only.
```

---

## Sample plan (what the backend returns)
```json
{
	"project_name": "ecommerce-demo",
	"base_path": "./ecommerce-demo",
	"folders": ["frontend", "frontend/src", "backend", "backend/project"],
	"files": [{"path":"README.md","content":""}],
	"commands": [
		"mkdir ecommerce-demo",
		"cd ecommerce-demo",
		"mkdir backend",
		"cd backend",
		"python -m venv .venv",
		"venv\\Scripts\\activate",
		"pip install django",
		"django-admin startproject project .",
		"cd ../frontend",
		"npm create vite@latest . -- --template react",
		"npm install",
		"mkdir ../shared-assets",
		"type nul > README.md"
	],
	"ide": "VSCode"
}
```

---

## Demo tips & troubleshooting
- Use `DEVOS_FORCE_FALLBACK=1` for instant deterministic scaffolds during demos.
- If `create-react-app` or other scaffolders complain about existing files, DevOS will run them in a new subfolder to avoid conflicts.
- If build/link fails for the Qt app with `Permission denied`, stop the running executable: `Stop-Process -Name DevOS -Force` and rebuild.

---

If you'd like, I can add a one-click demo script (PowerShell) that: starts the backend in fallback mode, launches the frontend, and runs a sample plan. Want that next?

