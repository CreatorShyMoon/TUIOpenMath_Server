# Open Math Server (Сервер распространения ПО и профилей)

Серверная часть клиент-серверной архитектуры проекта **SysOpenMath** («Open Math»), предназначенная для централизованного распространения вычислительных сред, профилей математического ПО, бинарных дистрибутивов и offline-пакетов Python (согласно стандарту PEP 503).

---

## 1. Архитектура и структура директорий

```
server/
├── app.py                # FastAPI приложение (REST API + PEP 503 Simple Repository)
├── requirements.txt      # Зависимости сервера (fastapi, uvicorn)
├── start.sh              # Скрипт автоматического развёртывания и запуска
├── mirror.py             # Утилита зеркалирования артефактов и пакетов
├── README.md             # Документация сервера
└── storage/              # Локальное хранилище данных
    ├── profiles/         # Каталог JSON-профилей (*.json)
    ├── artifacts/        # Бинарные дистрибутивы (Julia, Python, ...)
    │   └── julia/
    └── packages/         # Кэш Python wheel-пакетов для автономной установки
```

---

## 2. Быстрый запуск

### Вариант 1. Через Make из корня проекта:
```bash
make server
```

### Вариант 2. Прямой запуск скрипта:
```bash
./server/start.sh
```
*Скрипт автоматически проверит Python 3, создаст виртуальное окружение `server/.venv`, установит зависимости из `requirements.txt` и запустит сервер на `http://0.0.0.0:8000` с поддержкой auto-reload.*

Переменные окружения для настройки:
- `HOST` (по умолчанию `0.0.0.0`)
- `PORT` (по умолчанию `8000`)

Пример:
```bash
PORT=8080 ./server/start.sh
```

---

## 3. Спецификация HTTP API

| Метод | Эндпоинт | Назначение |
|---|---|---|
| `GET` | `/api/v1/health` | Проверка доступности (health-check) и статистика хранилища |
| `GET` | `/api/v1/profiles` | Каталог всех доступных профилей конфигураций |
| `GET` | `/api/v1/profiles/{name}` | Детальная JSON-спецификация профиля |
| `GET` | `/api/v1/artifacts` | Список доступных бинарных дистрибутивов с SHA-256 |
| `GET` | `/artifacts/{category}/{filename}` | Скачивание артефакта с заголовком `X-Checksum-SHA256` |
| `GET` | `/simple/` | PEP 503 Simple Index для `pip install` |
| `GET` | `/simple/{package}/` | Страница пакета со ссылками на wheels и SHA-256 |
| `GET` | `/packages/{filename}` | Скачивание `.whl` или `.tar.gz` пакета Python |
| `GET` | `/docs` | Интерактивная документация Swagger UI |

---

## 4. Утилита зеркалирования (`mirror.py`)

Для подготовки автономного/офлайн репозитория (например, в учебных лабораториях без интернета) используется утилита `mirror.py`:

```bash
# 1. Просмотр состояния репозитория
python3 server/mirror.py list
# или через make:
make mirror-list

# 2. Комплексная синхронизация всего профиля в 1 команду:
#    (качает Julia LTS + wheel-пакеты профиля + JupyterLab runtime)
python3 server/mirror.py sync remote-symbolic
# или через make:
make mirror-sync

# 3. Скачивание только дистрибутива Julia LTS (1.10.10):
python3 server/mirror.py julia --version 1.10.10

# 4. Кэширование базового runtime JupyterLab/notebook/ipykernel:
python3 server/mirror.py runtime

# 5. Кэширование Python wheel-пакетов профиля:
python3 server/mirror.py packages remote-symbolic
```

После наполнения `storage/packages/` клиенты TUIOpenMath могут устанавливать любые Python-пакеты в режиме ОФЛАЙН без доступа к публичному pypi.org:
```bash
pip install --index-url http://<server-ip>:8000/simple/ --trusted-host <server-ip> <package>
```
