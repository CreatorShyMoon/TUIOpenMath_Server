"""
Open Math Server (FastAPI Prototype)
Репозиторий распространения научного ПО и вычислительных профилей для TUI Open Math.
"""

from pathlib import Path
import hashlib
import json
import os
import re
from typing import Dict, List, Optional
from fastapi import FastAPI, HTTPException, Request, Response
from fastapi.responses import FileResponse, HTMLResponse, JSONResponse

BASE_DIR = Path(__file__).resolve().parent
STORAGE_DIR = BASE_DIR / "storage"
PROFILES_DIR = STORAGE_DIR / "profiles"
ARTIFACTS_DIR = STORAGE_DIR / "artifacts"
PACKAGES_DIR = STORAGE_DIR / "packages"

for d in (PROFILES_DIR, ARTIFACTS_DIR, PACKAGES_DIR):
    d.mkdir(parents=True, exist_ok=True)

app = FastAPI(
    title="Open Math Server",
    description="HTTP API репозитория для TUI Open Math",
    version="1.0.0",
)

_checksum_cache: Dict[str, str] = {}


@app.middleware("http")
async def normalize_path_middleware(request: Request, call_next):
    """
    Нормализует дублирующиеся слэши (//api/v1/... -> /api/v1/...),
    предотвращая ложные 404 Not Found из-за настроек URL на клиенте.
    """
    path = request.scope.get("path", "")
    if "//" in path:
        request.scope["path"] = re.sub(r"/+", "/", path)
    return await call_next(request)


def calc_sha256(filepath: Path) -> str:
    """Вычисление и кэширование SHA-256 хэша файла."""
    path_str = str(filepath)
    mtime = filepath.stat().st_mtime
    cache_key = f"{path_str}:{mtime}"

    if cache_key in _checksum_cache:
        return _checksum_cache[cache_key]

    hasher = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            hasher.update(chunk)

    digest = hasher.hexdigest()
    _checksum_cache[cache_key] = digest
    return digest


@app.get("/favicon.ico", include_in_schema=False)
def favicon():
    """Пустой ответ на запросы иконки браузером для предотвращения 404 в логах."""
    return Response(status_code=204)


# -------------------------------------------------------------------------
# Главная страница (Dashboard)
# -------------------------------------------------------------------------

@app.get("/", response_class=HTMLResponse)
def index_page():
    """Главная страница сервера репозитория со ссылками на API и документацию."""
    profiles_count = len(list(PROFILES_DIR.glob("*.json")))
    artifacts_count = len([f for f in ARTIFACTS_DIR.rglob("*") if f.is_file()])
    packages_count = len(list(PACKAGES_DIR.glob("*.whl"))) + len(list(PACKAGES_DIR.glob("*.tar.gz")))

    html = f"""<!DOCTYPE html>
<html lang="ru">
<head>
  <meta charset="utf-8">
  <title>Open Math Server</title>
  <style>
    body {{
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      margin: 40px auto;
      max-width: 820px;
      line-height: 1.6;
      color: #2c3e50;
      padding: 0 20px;
    }}
    h1 {{
      border-bottom: 2px solid #3498db;
      padding-bottom: 12px;
      display: flex;
      align-items: center;
      gap: 12px;
    }}
    .status-ok {{
      background: #27ae60;
      color: white;
      font-size: 0.5em;
      padding: 4px 10px;
      border-radius: 12px;
      vertical-align: middle;
      font-weight: bold;
    }}
    .card {{
      background: #f8fafc;
      border: 1px solid #e2e8f0;
      border-radius: 8px;
      padding: 20px;
      margin: 20px 0;
      box-shadow: 0 1px 3px rgba(0,0,0,0.05);
    }}
    ul {{ list-style-type: none; padding-left: 0; }}
    li {{ margin: 12px 0; font-size: 1.05em; }}
    a {{ color: #2980b9; text-decoration: none; font-weight: 500; }}
    a:hover {{ text-decoration: underline; color: #1abc9c; }}
    pre, code {{ background: #edf2f7; padding: 3px 6px; border-radius: 4px; font-family: monospace; }}
    pre {{ padding: 12px; overflow-x: auto; }}
  </style>
</head>
<body>
  <h1>Open Math Repository Server <span class="status-ok">ONLINE</span></h1>
  <p>Репозиторий распространения вычислительных сред, профилей и пакетов для <b>TUI Open Math</b>.</p>

  <div class="card">
    <h3>Доступные ресурсы и эндпоинты:</h3>
    <ul>
      <li>📖 <a href="/docs"><b>/docs</b> — Интерактивная документация Swagger UI</a></li>
      <li>🩺 <a href="/api/v1/health"><b>/api/v1/health</b> — Проверка доступности (Health Check)</a></li>
      <li>📋 <a href="/api/v1/profiles"><b>/api/v1/profiles</b> — Каталог профилей</a> (доступно: <b>{profiles_count}</b>)</li>
      <li>📦 <a href="/api/v1/artifacts"><b>/api/v1/artifacts</b> — Каталог артефактов и дистрибутивов</a> (файлов: <b>{artifacts_count}</b>)</li>
      <li>🐍 <a href="/simple/"><b>/simple/</b> — PEP 503 PyPI Simple Index для pip</a> (пакетов: <b>{packages_count}</b>)</li>
    </ul>
  </div>

  <div class="card">
    <h3>Подключение в TUI Open Math:</h3>
    <p>В конфигурационном файле клиента <code>config/client.json</code>:</p>
    <pre><code>{{
  "server_url": "http://127.0.0.1:8000",
  "offline_mode": false,
  "timeout_sec": 5
}}</code></pre>
  </div>
</body>
</html>"""
    return HTMLResponse(content=html)


# -------------------------------------------------------------------------
# Health & Status
# -------------------------------------------------------------------------

@app.get("/api/v1/health")
@app.get("/api/v1/health/")
def health_check():
    """Проверка доступности сервера клиентом."""
    return {
        "status": "ok",
        "service": "openmath-server",
        "version": "1.0.0",
        "profiles_count": len(list(PROFILES_DIR.glob("*.json"))),
        "artifacts_count": len([f for f in ARTIFACTS_DIR.rglob("*") if f.is_file()]),
        "packages_count": len(list(PACKAGES_DIR.glob("*.whl"))) + len(list(PACKAGES_DIR.glob("*.tar.gz"))),
    }


# -------------------------------------------------------------------------
# Profiles API
# -------------------------------------------------------------------------

@app.get("/api/v1/profiles")
@app.get("/api/v1/profiles/")
def list_profiles() -> List[dict]:
    """Возвращает каталог всех доступных профилей."""
    profiles = []
    for file in sorted(PROFILES_DIR.glob("*.json")):
        try:
            with open(file, "r", encoding="utf-8") as f:
                data = json.load(f)
                if isinstance(data, dict) and "name" in data:
                    profiles.append(data)
        except Exception:
            continue
    return profiles


@app.get("/api/v1/profiles/{name}")
@app.get("/api/v1/profiles/{name}/")
def get_profile(name: str) -> dict:
    """Получить спецификацию конкретного профиля."""
    target_file = PROFILES_DIR / f"{name}.json"
    if not target_file.is_file():
        raise HTTPException(status_code=404, detail=f"Profile '{name}' not found")

    try:
        with open(target_file, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Error reading profile: {e}")


# -------------------------------------------------------------------------
# Artifacts (Julia / Python installers / tarballs)
# -------------------------------------------------------------------------

@app.get("/api/v1/artifacts")
@app.get("/api/v1/artifacts/")
def list_artifacts():
    """Возвращает список доступных бинарных дистрибутивов с размерами и SHA-256."""
    items = []
    for f in ARTIFACTS_DIR.rglob("*"):
        if f.is_file():
            rel = f.relative_to(ARTIFACTS_DIR)
            category = rel.parts[0] if len(rel.parts) > 1 else "misc"
            items.append({
                "category": category,
                "filename": f.name,
                "path": str(rel),
                "size_bytes": f.stat().st_size,
                "sha256": calc_sha256(f),
                "download_url": f"/artifacts/{category}/{f.name}",
            })
    return items


@app.get("/artifacts/{category}/{filename}")
def download_artifact(category: str, filename: str):
    """Скачивание дистрибутива с проверкой пути и отдачей заголовка X-Checksum-SHA256."""
    safe_name = os.path.basename(filename)
    safe_cat = os.path.basename(category)

    # Поддержка прямого запроса контрольной суммы: /artifacts/julia/julia-...tar.gz.sha256
    if safe_name.endswith(".sha256"):
        real_name = safe_name[:-7]
        real_path = ARTIFACTS_DIR / safe_cat / real_name
        if not real_path.is_file():
            raise HTTPException(status_code=404, detail=f"Artifact '{real_name}' not found")
        sha256 = calc_sha256(real_path)
        return Response(content=f"{sha256}  {real_name}\n", media_type="text/plain")

    filepath = ARTIFACTS_DIR / safe_cat / safe_name

    if not filepath.is_file():
        raise HTTPException(status_code=404, detail="Artifact not found")

    sha256 = calc_sha256(filepath)
    return FileResponse(
        path=filepath,
        filename=safe_name,
        headers={"X-Checksum-SHA256": sha256},
    )


# -------------------------------------------------------------------------
# PEP 503 Simple Repository API (для автономного pip install)
# -------------------------------------------------------------------------

def normalize_package_name(name: str) -> str:
    """PEP 503 нормализация имени пакета: замена _ и . на -, перевод в lowercase."""
    return re.sub(r"[-_.]+", "-", name).lower()


@app.get("/simple", response_class=HTMLResponse)
@app.get("/simple/", response_class=HTMLResponse)
def pip_simple_index():
    """Главная страница индекса пакетов для pip."""
    package_names = set()
    for f in PACKAGES_DIR.glob("*.whl"):
        parts = f.name.split("-")
        if parts:
            package_names.add(normalize_package_name(parts[0]))
    for f in PACKAGES_DIR.glob("*.tar.gz"):
        pkg = f.name[:-7].rsplit("-", 1)[0]
        if pkg:
            package_names.add(normalize_package_name(pkg))

    links_html = "\n".join(
        f'    <a href="/simple/{pkg}/">{pkg}</a><br/>'
        for pkg in sorted(package_names)
    )

    html = f"""<!DOCTYPE html>
<html>
  <head><title>Open Math Simple Index</title></head>
  <body>
    <h1>Open Math Package Index</h1>
{links_html}
  </body>
</html>"""
    return HTMLResponse(content=html)


@app.get("/simple/{package}", response_class=HTMLResponse)
@app.get("/simple/{package}/", response_class=HTMLResponse)
def pip_package_page(package: str):
    """Страница версий пакета со ссылками на wheel-файлы и хэшами sha256."""
    norm_pkg = normalize_package_name(package)
    matching_files = []

    for f in PACKAGES_DIR.glob("*.whl"):
        parts = f.name.split("-")
        if parts and normalize_package_name(parts[0]) == norm_pkg:
            matching_files.append(f)

    for f in PACKAGES_DIR.glob("*.tar.gz"):
        pkg = f.name[:-7].rsplit("-", 1)[0]
        if pkg and normalize_package_name(pkg) == norm_pkg:
            matching_files.append(f)

    if not matching_files:
        raise HTTPException(status_code=404, detail="Package not found")

    links = []
    for f in sorted(matching_files, key=lambda x: x.name):
        sha = calc_sha256(f)
        links.append(f'    <a href="/packages/{f.name}#sha256={sha}">{f.name}</a><br/>')

    links_html = "\n".join(links)
    html = f"""<!DOCTYPE html>
<html>
  <head><title>Links for {package}</title></head>
  <body>
    <h1>Links for {package}</h1>
{links_html}
  </body>
</html>"""
    return HTMLResponse(content=html)


@app.get("/packages/{filename}")
def download_package(filename: str):
    """Скачивание файла пакета (wheel / sdist)."""
    safe_name = os.path.basename(filename)
    filepath = PACKAGES_DIR / safe_name
    if not filepath.is_file():
        raise HTTPException(status_code=404, detail="Package file not found")
    return FileResponse(path=filepath, filename=safe_name)
