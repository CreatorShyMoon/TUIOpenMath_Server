#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$SCRIPT_DIR/.venv"
HOST="${HOST:-0.0.0.0}"
PORT="${PORT:-8000}"

echo "=================================================="
echo "    Запуск Open Math Distribution Server          "
echo "=================================================="
echo "Директория сервера: $SCRIPT_DIR"
echo "Хост: $HOST  Порт: $PORT"

if ! command -v python3 &>/dev/null; then
    echo "[ERROR] python3 не найден в PATH. Установите Python 3."
    exit 1
fi

# Проверка или создание виртуального окружения
if [ ! -d "$VENV_DIR" ]; then
    echo "[INFO] Создание виртуального окружения в $VENV_DIR ..."
    if ! python3 -m venv "$VENV_DIR"; then
        echo "[ERROR] Не удалось создать venv. Убедитесь, что установлен пакет python3-venv:"
        echo "        sudo apt update && sudo apt install -y python3-venv"
        exit 1
    fi
    echo "[INFO] Обновление pip и установка зависимостей..."
    "$VENV_DIR/bin/pip" install --upgrade pip
    "$VENV_DIR/bin/pip" install -r "$SCRIPT_DIR/requirements.txt"
fi

# Проверка наличия uvicorn в venv
if [ ! -x "$VENV_DIR/bin/uvicorn" ]; then
    echo "[INFO] Установка зависимостей из requirements.txt..."
    "$VENV_DIR/bin/pip" install -r "$SCRIPT_DIR/requirements.txt"
fi

# Если порт уже занят предыдущим процессом — гарантированно освобождаем его
free_port() {
    local p="$1"
    # 1. Завершить любые запущенные uvicorn процессы этого сервера
    pkill -9 -f "uvicorn.*app:app" 2>/dev/null || true

    # 2. Попробовать fuser по стандартным путям (включая /usr/sbin и /sbin)
    for fcmd in fuser /usr/sbin/fuser /sbin/fuser; do
        if command -v "$fcmd" &>/dev/null || [ -x "$fcmd" ]; then
            "$fcmd" -k -9 "${p}/tcp" 2>/dev/null || true
            break
        fi
    done

    # 3. Попробовать lsof
    if command -v lsof &>/dev/null; then
        local pids
        pids=$(lsof -ti :"$p" 2>/dev/null || true)
        if [ -n "$pids" ]; then
            echo "[WARN] Порт $p занят (PID: $pids). Завершаем предыдущий процесс сервера..."
            kill -9 $pids 2>/dev/null || true
        fi
    fi

    # 4. Проверить через Python и при необходимости извлечь PID из ss
    python3 -c "
import os, sys, time, socket, subprocess, re

port = int('$p')

try:
    out = subprocess.check_output(['ss', '-lptn', f'sport = :{port}'], stderr=subprocess.DEVNULL).decode()
    for m in re.finditer(r'pid=(\d+)', out):
        try:
            os.kill(int(m.group(1)), 9)
        except OSError:
            pass
except Exception:
    pass

deadline = time.time() + 3.0
while time.time() < deadline:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            s.bind(('0.0.0.0', port))
            break
        except OSError:
            time.sleep(0.3)
" 2>/dev/null || true
}

free_port "$PORT"

echo "[INFO] Сервер запускается на http://$HOST:$PORT"
echo "[INFO] Документация API: http://localhost:$PORT/docs"
echo "[INFO] PEP 503 Simple Index: http://localhost:$PORT/simple/"
echo "[INFO] Для остановки нажмите Ctrl+C"
echo "=================================================="

exec "$VENV_DIR/bin/uvicorn" app:app --app-dir "$SCRIPT_DIR" --host "$HOST" --port "$PORT" --reload
