#!/usr/bin/env python3
"""
Open Math Repository Mirroring & Artifact Management Utility.

Позволяет администратору репозитория:
1. Загружать бинарные дистрибутивы Julia в локальное хранилище артефактов (storage/artifacts/julia).
2. Скачивать wheel-пакеты Python для профилей в локальный индекс (storage/packages).
3. Просматривать каталог и проверять контрольные суммы SHA-256.
"""

import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import urllib.request

BASE_DIR = Path(__file__).resolve().parent
STORAGE_DIR = BASE_DIR / "storage"
PROFILES_DIR = STORAGE_DIR / "profiles"
ARTIFACTS_DIR = STORAGE_DIR / "artifacts"
PACKAGES_DIR = STORAGE_DIR / "packages"

JULIA_VERSIONS = {
    "1.10.10": {
        "url": "https://julialang-s3.julialang.org/bin/linux/x64/1.10/julia-1.10.10-linux-x86_64.tar.gz",
        "filename": "julia-1.10.10-linux-x86_64.tar.gz",
    },
    "1.11.4": {
        "url": "https://julialang-s3.julialang.org/bin/linux/x64/1.11/julia-1.11.4-linux-x86_64.tar.gz",
        "filename": "julia-1.11.4-linux-x86_64.tar.gz",
    },
}


def calc_sha256(filepath: Path) -> str:
    hasher = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            hasher.update(chunk)
    return hasher.hexdigest()


def cmd_list(args):
    """Показать состояние локального репозитория."""
    print("=" * 60)
    print("         Состояние Open Math Repository Storage")
    print("=" * 60)

    # Профили
    profiles = list(PROFILES_DIR.glob("*.json"))
    print(f"\n[Профили] Всего: {len(profiles)}")
    for p in sorted(profiles):
        try:
            data = json.loads(p.read_text(encoding="utf-8"))
            desc = data.get("description", "без описания")
            py_pkgs = len(data.get("python", []))
            jl_pkgs = len(data.get("julia", []))
            print(f"  • {p.stem:20} | Py: {py_pkgs:2} | Jl: {jl_pkgs:2} | {desc}")
        except Exception as e:
            print(f"  • {p.name}: ошибка чтения ({e})")

    # Артефакты
    artifacts = [f for f in ARTIFACTS_DIR.rglob("*") if f.is_file() and not f.name.endswith(".tmp")]
    print(f"\n[Артефакты] Всего: {len(artifacts)}")
    for a in sorted(artifacts):
        rel = a.relative_to(ARTIFACTS_DIR)
        sz = a.stat().st_size
        sha = calc_sha256(a)
        if sz < 100 * 1024:
            sz_str = f"{sz / 1024:5.1f} KB [!] МЕТАПАКЕТ"
        else:
            sz_str = f"{sz / (1024 * 1024):6.1f} MB"
        print(f"  • {str(rel):35} | {sz_str:>10} | SHA-256: {sha[:12]}...")

    # Python пакеты (wheels)
    packages = list(PACKAGES_DIR.glob("*.whl")) + list(PACKAGES_DIR.glob("*.tar.gz"))
    print(f"\n[Python Packages (PEP 503)] Всего: {len(packages)}")
    total_sz_mb = sum(p.stat().st_size for p in packages) / (1024 * 1024) if packages else 0
    print(f"  Общий объём пакетов: {total_sz_mb:.1f} MB")
    for p in sorted(packages)[:15]:
        sz_kb = p.stat().st_size / 1024
        print(f"  • {p.name:40} | {sz_kb:6.1f} KB")
    if len(packages) > 15:
        print(f"  ... и ещё {len(packages) - 15} пакетов.")
    print("=" * 60)


def verify_julia_archive(filepath: Path) -> bool:
    """Проверка целостности .tar.gz архива Julia (чтение всего gzip потока)."""
    if not filepath.is_file() or filepath.stat().st_size < 30 * 1024 * 1024:
        return False
    try:
        with gzip.open(filepath, "rb") as f:
            while f.read(1024 * 1024):
                pass
        return True
    except Exception:
        return False


def cmd_download_julia(args):
    """Скачать дистрибутив Julia в хранилище артефактов."""
    version = args.version
    if version not in JULIA_VERSIONS:
        print(f"[ERROR] Неизвестная версия Julia: {version}. Доступны: {list(JULIA_VERSIONS.keys())}")
        return 1

    info = JULIA_VERSIONS[version]
    target_dir = ARTIFACTS_DIR / "julia"
    target_dir.mkdir(parents=True, exist_ok=True)
    target_file = target_dir / info["filename"]

    if target_file.exists() and not args.force:
        if verify_julia_archive(target_file):
            print(f"[INFO] Файл {info['filename']} уже существует и проверен ({target_file.stat().st_size / (1024*1024):.1f} MB).")
            print(f"[INFO] SHA-256: {calc_sha256(target_file)}")
            print("Используйте --force для повторной загрузки.")
            return 0
        else:
            print(f"[WARN] Существующий файл {info['filename']} повреждён или недокачан ({target_file.stat().st_size / (1024*1024):.1f} MB). Перекачиваем...")
            target_file.unlink(missing_ok=True)

    print(f"[STEP] Загрузка Julia {version} из {info['url']} ...")
    tmp_file = target_dir / (info["filename"] + ".tmp")
    if tmp_file.exists():
        tmp_file.unlink(missing_ok=True)

    try:
        req = urllib.request.Request(info["url"], headers={"User-Agent": "SysOpenMath-Mirror/1.0"})
        with urllib.request.urlopen(req) as resp, open(tmp_file, "wb") as out:
            total_size = int(resp.headers.get("Content-Length", 0))
            downloaded = 0
            while chunk := resp.read(65536):
                out.write(chunk)
                downloaded += len(chunk)
                if total_size > 0:
                    pct = downloaded * 100 // total_size
                    print(f"\r[DOWNLOAD] {downloaded // (1024*1024)} MB / {total_size // (1024*1024)} MB ({pct}%)", end="", flush=True)
            print()

        print("[STEP] Проверка целостности загруженного архива (gzip test)...")
        if not verify_julia_archive(tmp_file):
            print("[ERROR] Загруженный архив не прошёл проверку целостности (файл повреждён или не полон)!")
            tmp_file.unlink(missing_ok=True)
            return 1

        tmp_file.replace(target_file)
        sha = calc_sha256(target_file)
        print(f"[OK] Дистрибутив успешно сохранён: {target_file}")
        print(f"[INFO] Размер: {target_file.stat().st_size / (1024*1024):.1f} MB")
        print(f"[INFO] SHA-256: {sha}")
        return 0
    except Exception as e:
        print(f"\n[ERROR] Ошибка при загрузке: {e}")
        if tmp_file.exists():
            tmp_file.unlink(missing_ok=True)
        return 1


CORE_JUPYTER_PACKAGES = [
    "fastapi",
    "uvicorn",
    "jupyterlab",
    "notebook",
    "ipykernel",
    "bash_kernel",
    "octave_kernel",
    "scilab_kernel",
    "wheel",
    "setuptools",
]

SCILAB_URLS = [
    "https://antispam.utc.fr/pub/scilab/desktop/2024.1.0/scilab-2024.1.0.bin.x86_64.tar.gz",
    "https://antispam.utc.fr/pub/scilab/desktop/2024.0.0/scilab-2024.0.0.bin.x86_64.tar.gz",
    "https://www.scilab.org/download/2024.1.0/scilab-2024.1.0.bin.x86_64.tar.gz",
    "https://www.scilab.org/download/2024.0.0/scilab-2024.0.0.bin.x86_64.tar.gz",
    "https://antispam.utc.fr/pub/scilab/desktop/6.1.1/scilab-6.1.1.bin.x86_64.tar.gz",
    "https://www.scilab.org/download/6.1.1/scilab-6.1.1.bin.x86_64.tar.gz",
]

OCTAVE_URLS = [
    "https://github.com/probonopd/Octave.AppImage/releases/download/continuous/Octave-x86_64.AppImage",
]


def download_file_smart(urls: list, target_file: Path, min_size_mb: float = 10.0) -> bool:
    """Умная загрузка файла с поддержкой curl и urllib с проверкой минимального размера."""
    tmp_file = target_file.with_name(target_file.name + ".tmp")
    min_bytes = int(min_size_mb * 1024 * 1024)

    for url in urls:
        print(f"[STEP] Загрузка из {url} ...")
        tmp_file.unlink(missing_ok=True)

        # 1. Попытка через curl (лучше справляется с редиректами и SSL)
        if shutil.which("curl"):
            try:
                res = subprocess.run(
                    ["curl", "-fL", "--connect-timeout", "15", "--retry", "2", "-o", str(tmp_file), url],
                    check=False
                )
                if res.returncode == 0 and tmp_file.exists() and tmp_file.stat().st_size >= min_bytes:
                    tmp_file.replace(target_file)
                    return True
            except Exception:
                pass

        # 2. Попытка через urllib
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "SysOpenMath-Mirror/1.0"})
            with urllib.request.urlopen(req, timeout=30) as resp, open(tmp_file, "wb") as out:
                total_size = int(resp.headers.get("Content-Length", 0))
                downloaded = 0
                while chunk := resp.read(65536):
                    out.write(chunk)
                    downloaded += len(chunk)
                    if total_size > 0:
                        pct = downloaded * 100 // total_size
                        print(f"\r[DOWNLOAD] {downloaded // (1024*1024)} MB / {total_size // (1024*1024)} MB ({pct}%)", end="", flush=True)
                print()
            if tmp_file.exists() and tmp_file.stat().st_size >= min_bytes:
                tmp_file.replace(target_file)
                return True
        except Exception as e:
            print(f"\n[WARN] Ошибка загрузки по ссылке {url}: {e}")
        finally:
            if tmp_file.exists() and tmp_file.stat().st_size < min_bytes:
                tmp_file.unlink(missing_ok=True)

    return False


def cmd_download_scilab(args):
    """Скачать официальный переносимый дистрибутив Scilab или бинарные deb-пакеты."""
    target_dir = ARTIFACTS_DIR / "scilab"
    target_dir.mkdir(parents=True, exist_ok=True)

    # 1. Удаляем пустые метапакеты Debian/Ubuntu (< 100 KB)
    for dummy in target_dir.glob("*.deb"):
        if dummy.stat().st_size < 100 * 1024:
            print(f"[CLEANUP] Удаление пустого метапакета: {dummy.name} ({dummy.stat().st_size} байт)")
            dummy.unlink(missing_ok=True)

    # 2. Проверяем наличие уже скачанного полноценного дистрибутива
    existing = [
        f for f in target_dir.iterdir()
        if f.is_file() and f.stat().st_size > 10 * 1024 * 1024 and not f.name.endswith(".tmp")
    ]
    if existing and not getattr(args, "force", False):
        f = existing[0]
        print(f"[INFO] Scilab уже присутствует: {f.name} ({f.stat().st_size / (1024*1024):.1f} MB)")
        print(f"[INFO] SHA-256: {calc_sha256(f)}")
        return 0

    # 3. Скачиваем официальный переносимый .tar.gz (включает встроенную Java и все CLI утилиты)
    target_file = target_dir / "scilab-portable.bin.x86_64.tar.gz"
    print("[STEP] Попытка загрузки переносимого архива Scilab с официальных зеркал...")
    if download_file_smart(SCILAB_URLS, target_file, min_size_mb=20.0):
        sha = calc_sha256(target_file)
        print(f"[OK] Официальный дистрибутив Scilab успешно сохранён: {target_file.name} ({target_file.stat().st_size / (1024*1024):.1f} MB)")
        print(f"[INFO] SHA-256: {sha}")
        return 0

    # 4. Фолбэк на apt-get download: скачиваем реальные бинарники scilab-cli и scilab-data
    print("[WARN] Не удалось загрузить tar.gz с официальных зеркал Scilab.")
    print("[STEP] Пробуем скачать реальные пакеты Scilab через apt-get download (scilab-cli, scilab-data)...")
    try:
        res = subprocess.run(
            ["apt-get", "download", "scilab-cli", "scilab-data"],
            cwd=str(target_dir),
            capture_output=True,
            text=True
        )
        for dummy in target_dir.glob("scilab_*_all.deb"):
            dummy.unlink(missing_ok=True)

        debs = [f for f in target_dir.glob("*.deb") if f.stat().st_size > 100 * 1024]
        if debs:
            print(f"[OK] Пакеты Scilab ({len(debs)} шт.) успешно загружены в артефакты:")
            for d in debs:
                print(f"     • {d.name} ({d.stat().st_size / (1024*1024):.1f} MB, SHA-256: {calc_sha256(d)[:12]}...)")
            return 0
    except Exception as e:
        print(f"[WARN] Ошибка apt-get download: {e}")

    print("[HINT] Для автономной работы поместите архив 'scilab-*.tar.gz' или пакеты в:")
    print(f"       {target_dir}")
    return 1


def cmd_cache_runtime(args):
    """Скачать wheel-пакеты базового окружения Jupyter Runtime."""
    PACKAGES_DIR.mkdir(parents=True, exist_ok=True)
    print(f"[STEP] Загрузка базовых пакетов Jupyter runtime в {PACKAGES_DIR}:")
    print(f"       {', '.join(CORE_JUPYTER_PACKAGES)}")

    cmd = [
        sys.executable,
        "-m",
        "pip",
        "download",
        "-d",
        str(PACKAGES_DIR),
        *CORE_JUPYTER_PACKAGES,
    ]

    try:
        subprocess.run(cmd, check=True)
        print(f"\n[OK] Пакеты Jupyter runtime успешно сохранены в {PACKAGES_DIR}")
        return 0
    except subprocess.CalledProcessError as e:
        print(f"[ERROR] Ошибка pip download runtime: {e}")
        return e.returncode


def cmd_download_octave(args):
    """Скачать или подготовить переносимый дистрибутив Octave."""
    target_dir = ARTIFACTS_DIR / "octave"
    target_dir.mkdir(parents=True, exist_ok=True)

    for dummy in target_dir.glob("*.deb"):
        if dummy.stat().st_size < 100 * 1024:
            print(f"[CLEANUP] Удаление поврежденного пакета: {dummy.name}")
            dummy.unlink(missing_ok=True)

    existing = [
        f for f in target_dir.iterdir()
        if f.is_file() and f.stat().st_size > 5 * 1024 * 1024 and not f.name.endswith(".tmp")
    ]
    if existing and not getattr(args, "force", False):
        f = existing[0]
        print(f"[INFO] Дистрибутив Octave уже присутствует: {f.name} ({f.stat().st_size / (1024*1024):.1f} MB)")
        print(f"[INFO] SHA-256: {calc_sha256(f)}")
        return 0

    target_file = target_dir / "octave.AppImage"
    print("[STEP] Попытка загрузки Octave AppImage из GitHub...")
    if download_file_smart(OCTAVE_URLS, target_file, min_size_mb=20.0):
        target_file.chmod(0o755)
        sha = calc_sha256(target_file)
        print(f"[OK] Дистрибутив Octave AppImage успешно сохранён: {target_file.name}")
        print(f"[INFO] SHA-256: {sha}")
        return 0

    print("[WARN] Не удалось скачать Octave AppImage по сети.")
    print("[STEP] Пробуем скачать пакеты Octave через apt-get download...")
    try:
        res = subprocess.run(["apt-get", "download", "octave"], cwd=str(target_dir), capture_output=True)
        debs = [f for f in target_dir.glob("octave*.deb") if f.stat().st_size > 1024 * 1024]
        if debs:
            f = debs[0]
            print(f"[OK] Пакет Octave .deb успешно загружен: {f.name} ({f.stat().st_size / (1024*1024):.1f} MB)")
            print(f"[INFO] SHA-256: {calc_sha256(f)}")
            return 0
    except Exception as e:
        print(f"[WARN] Ошибка apt-get download: {e}")

    print("[HINT] Для полностью офлайн-работы с Octave вы можете поместить готовый")
    print("       'octave.AppImage' или 'octave-portable.tar.gz' в папку:")
    print(f"       {target_dir}")
    return 1


def cmd_bundle(args):
    """Собрать полный автономный монолитный архив проекта со всеми исходниками и хранилищем."""
    root_dir = BASE_DIR.parent
    bundle_name = "sysopenmath-monolith-offline.tar.gz"
    bundle_path = root_dir / bundle_name

    print(f"[STEP] Сборка монолитного автономного дистрибутива в {bundle_name} ...")

    # Гарантируем структуру каталогов с .gitkeep
    for d in ["workspaces", "state", "logs", "tools", "envs", "profiles"]:
        dp = root_dir / d
        dp.mkdir(parents=True, exist_ok=True)
        (dp / ".gitkeep").touch(exist_ok=True)

    items_to_pack = [
        "manager.c",
        "src",
        "include",
        "server",
        "config",
        "profiles",
        "Makefile",
        "README.md",
        "workspaces",
        "state",
        "logs",
        "tools",
        "envs",
    ]
    if (root_dir / "manager").is_file():
        items_to_pack.append("manager")

    cmd = [
        "tar",
        "-czf",
        str(bundle_path),
        "--exclude=*.pyc",
        "--exclude=__pycache__",
        "--exclude=.git",
        "--exclude=.DS_Store",
        "--exclude=server/.venv",
        "--exclude=envs/*",
        "--exclude=logs/*",
        "--exclude=workspaces/*",
        "--exclude=state/*",
        "--exclude=tools/*",
        "--exclude=share/jupyter/runtime",
        *items_to_pack,
    ]
    try:
        subprocess.run(cmd, cwd=root_dir, check=True)
        sz_mb = bundle_path.stat().st_size / (1024 * 1024)
        print(f"[OK] Монолитный дистрибутив успешно собран: {bundle_name} ({sz_mb:.1f} MB)")
        print("     Архив содержит ВСЕ исходники (.c/.h), сервер и пакеты для автономного запуска!")
        return 0
    except Exception as e:
        print(f"[ERROR] Ошибка при создании архива: {e}")
        return 1


def cmd_sync(args):
    """Комплексная синхронизация: Julia LTS + Scilab + Octave + пакеты профиля + Jupyter Runtime."""
    print("==================================================")
    print(f"  Комплексная подготовка репозитория (sync: {args.profile})")
    print("==================================================")

    # 1. Julia LTS
    class JuliaArgs:
        version = "1.10.10"
        force = False

    print("\n>>> Этап 1: Проверка дистрибутива Julia LTS...")
    cmd_download_julia(JuliaArgs())

    # 2. Scilab
    class ScilabArgs:
        force = False

    if getattr(args, "with_scilab", True):
        print("\n>>> Этап 2: Проверка переносимого дистрибутива Scilab...")
        cmd_download_scilab(ScilabArgs())

    # 3. Octave
    class OctaveArgs:
        force = False

    if getattr(args, "with_octave", True):
        print("\n>>> Этап 3: Проверка дистрибутива Octave...")
        cmd_download_octave(OctaveArgs())

    # 4. Пакеты профиля
    print(f"\n>>> Этап 4: Кэширование пакетов профилей '{args.profile}'...")
    cmd_cache_packages(args)

    # 5. Jupyter runtime
    if getattr(args, "with_jupyter", True):
        print("\n>>> Этап 5: Кэширование Jupyter runtime...")
        cmd_cache_runtime(args)

    print("\n>>> Этап 6: Итоговое состояние хранилища:")
    cmd_list(args)
    print("\n[OK] Синхронизация репозитория полностью завершена.")
    return 0


def cmd_cache_packages(args):
    """Скачать wheel-пакеты Python для профилей."""
    PACKAGES_DIR.mkdir(parents=True, exist_ok=True)
    packages_to_fetch = set()

    profiles_to_scan = []
    if args.profile == "all":
        profiles_to_scan = list(PROFILES_DIR.glob("*.json"))
    else:
        target = PROFILES_DIR / f"{args.profile}.json"
        if not target.exists():
            print(f"[ERROR] Профиль {args.profile} не найден в {PROFILES_DIR}")
            return 1
        profiles_to_scan = [target]

    for p in profiles_to_scan:
        try:
            data = json.loads(p.read_text(encoding="utf-8"))
            for pkg in data.get("python", []):
                packages_to_fetch.add(pkg)
        except Exception as e:
            print(f"[WARN] Ошибка чтения {p}: {e}")

    if not packages_to_fetch:
        print("[WARN] Список пакетов пуст.")
        return 0

    print(f"[STEP] Подготовка к загрузке {len(packages_to_fetch)} пакетов в {PACKAGES_DIR}:")
    print(f"       {', '.join(sorted(packages_to_fetch))}")

    cmd = [
        sys.executable,
        "-m",
        "pip",
        "download",
        "-d",
        str(PACKAGES_DIR),
        *sorted(packages_to_fetch),
    ]

    try:
        subprocess.run(cmd, check=True)
        print(f"\n[OK] Пакеты успешно кэшированы в {PACKAGES_DIR}")
        return 0
    except subprocess.CalledProcessError as e:
        print(f"[ERROR] Ошибка pip download: {e}")
        return e.returncode


def main():
    parser = argparse.ArgumentParser(
        description="Open Math Repository Mirroring & Artifact Management Utility"
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    # list
    subparsers.add_parser("list", help="Показать состояние хранилища")

    # julia
    p_julia = subparsers.add_parser("julia", help="Скачать архив Julia в артефакты")
    p_julia.add_argument(
        "--version",
        default="1.10.10",
        choices=list(JULIA_VERSIONS.keys()),
        help="Версия Julia для загрузки (по умолчанию 1.10.10 LTS)",
    )
    p_julia.add_argument(
        "--force",
        action="store_true",
        help="Перезаписать существующий файл",
    )

    # packages
    p_pkg = subparsers.add_parser("packages", help="Скачать wheel-пакеты Python для профилей")
    p_pkg.add_argument(
        "profile",
        nargs="?",
        default="all",
        help="Имя профиля (или 'all' для всех профилей)",
    )

    # runtime
    subparsers.add_parser("runtime", help="Скачать пакеты ядра JupyterLab/notebook/ipykernel")

    # sync
    p_sync = subparsers.add_parser("sync", help="Комплексная синхронизация (Julia + пакеты + Jupyter)")
    p_sync.add_argument(
        "profile",
        nargs="?",
        default="all",
        help="Имя профиля для синхронизации (по умолчанию 'all')",
    )
    p_sync.add_argument(
        "--no-jupyter",
        dest="with_jupyter",
        action="store_false",
        help="Не скачивать пакеты Jupyter runtime",
    )

    # scilab
    p_scilab = subparsers.add_parser("scilab", help="Скачать переносимый дистрибутив Scilab")
    p_scilab.add_argument("--force", action="store_true", help="Перезаписать существующий файл")

    # octave
    p_octave = subparsers.add_parser("octave", help="Скачать переносимый дистрибутив Octave (AppImage)")
    p_octave.add_argument("--force", action="store_true", help="Перезаписать существующий файл")

    # bundle
    subparsers.add_parser("bundle", help="Собрать полный монолитный офлайн архив (openmath-monolith)")

    args = parser.parse_args()

    if args.command == "list":
        return cmd_list(args)
    elif args.command == "julia":
        return cmd_download_julia(args)
    elif args.command == "scilab":
        return cmd_download_scilab(args)
    elif args.command == "octave":
        return cmd_download_octave(args)
    elif args.command == "packages":
        return cmd_cache_packages(args)
    elif args.command == "runtime":
        return cmd_cache_runtime(args)
    elif args.command == "sync":
        return cmd_sync(args)
    elif args.command == "bundle":
        return cmd_bundle(args)
    return 0


if __name__ == "__main__":
    sys.exit(main() or 0)
