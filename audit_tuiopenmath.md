# Технический аудит TUI Open Math и план интеграции клиент-серверной инфраструктуры

**Дата проведения аудита:** 2 ноября 2026 г.  
**Объект аудита:** Репозиторий `TUIOpenMath`  
**Цель:** Оценка применимости существующей архитектуры для реализации клиент-серверной инфраструктуры распространения научного ПО (дипломный проект «Open Math»).

---

## 1. Архитектура и назначение основных модулей

Проект реализован на языке **C11** с использованием библиотек `ncursesw` (терминальный интерфейс) и `libcjson` (работа с JSON). Проект собирается через `Makefile` в единый бинарный исполняемый файл `manager`.

```text
                    ┌───────────────────────────────┐
                    │      manager.c (Event Loop)   │
                    └───────────────┬───────────────┘
                                    │
         ┌──────────────────────────┼──────────────────────────┐
         ▼                          ▼                          ▼
┌─────────────────┐        ┌─────────────────┐        ┌─────────────────┐
│     ui.c        │        │   actions.c     │        │   process.c     │
│ 3-оконный TUI   │        │ Диспетчер       │        │ Неблокирующий   │
│ (ncurses)       │        │ действий        │        │ запуск bash     │
└─────────────────┘        └────────┬────────┘        └─────────────────┘
                                    │
       ┌───────────────┬────────────┴───┬────────────────┬───────────────┐
       ▼               ▼                ▼                ▼               ▼
┌──────────────┐┌──────────────┐┌───────────────┐┌──────────────┐┌──────────────┐
│  profiles.c  ││setup_manager ││jupyter_manager││kernel_manager││checks.c      │
│  profile_reg ││генерация     ││управление     ││регистрация   ││диагностика   │
│  профили     ││установки     ││демоном lab    ││ipykernel     ││readiness     │
└──────────────┘└──────────────┘└───────────────┘└──────────────┘└──────────────┘
```

### Основные компоненты:
1. **`manager.c`**:
   * Инициализирует окружение (`ensure_logs_dir`, `ensure_jupyter_env`).
   * Задает переменные окружения процесса (`JUPYTER_DATA_DIR`).
   * Запускает главный цикл: `check_process` -> `read_process_output` -> `ui_draw` -> `getch` -> `ui_handle_input`.
2. **`src/process.c`**:
   * Реализует неблокирующее асинхронное выполнение команд через `pipe()` + `fork()` + `execl("/bin/sh", "sh", "-c", cmd)`.
   * Читает вывод дочернего процесса порциями и построчно отправляет в систему логирования.
3. **`src/ui.c`**:
   * Разделен на 3 рабочие области: «Профили» (слева), «Действия» (справа), «Лог» (снизу) и строка состояния.
   * Управляет навигацией (`Tab`, стрелки, `Enter`, `PageUp/Down`).
4. **`src/profiles.c`** и **`src/profile_registry.c`**:
   * Профили хранятся в статическом массиве `BUILTIN_PROFILES[]` в виде зашитых JSON-строк (`base`, `symbolic`, `numerical`, `scientific`, `diffeq`, `ml`, `stats`).
5. **`src/profile_config.c`**:
   * Парсер cJSON: разбирает поля `name`, `description`, `python`, `julia`, `system`, `kernels`, `workspace`.
6. **`src/setup_manager.c`**:
   * Центральный генератор монолитных bash-сценариев развёртывания.
7. **`src/install_steps.c`**:
   * Альтернативный пошаговый генератор установочных скриптов во временные файлы `/tmp/sysopenmath_*`.
8. **`src/jupyter_manager.c`** & **`src/kernel_manager.c`**:
   * Управление жизненным циклом JupyterLab (`nohup jupyter-lab &`, сохранение PID в `state/jupyter.pid`, извлечение токена).
   * Регистрация ядер `ipykernel`, `bash_kernel`, `octave_kernel`, `scilab_kernel`, `IJulia`.
9. **`src/checks.c`**:
   * Диагностика платформы и проверка статуса readiness профиля.
10. **`src/cleanup_manager.c`** & **`src/state_manager.c`**:
    * Управление состоянием (`state/profiles/*.state`) и локальной очисткой.

---

## 2. Реальные механизмы установки и загрузки компонентов

| Компонент | Как загружается сейчас | Куда устанавливается | Зависимость от внешней сети |
|---|---|---|---|
| **Julia LTS** | `curl -fL "https://julialang-s3.julialang.org/bin/linux/x64/1.10/julia-1.10.10-linux-x86_64.tar.gz"` | `tools/julia/lts/` -> `tools/bin/julia` | Прямой URL на AWS S3 |
| **Jupyter Runtime** | Создаётся `python3 -m venv tools/jupyter/runtime` + `pip install ...` | `tools/jupyter/runtime/` | Публичный PyPI |
| **Python venv профиля** | `python3 -m venv envs/<profile>` + `pip install <packages>` | `envs/<profile>/` | Публичный PyPI |
| **Julia Packages** | `tools/bin/julia -e 'using Pkg; Pkg.add("...")'` | `~/.julia/` | Публичный GitHub + Julia Registry |
| **System (Octave, Scilab)**| `sudo apt-get install -y <package>` | Системные пути ОС | Репозитории Ubuntu / Debian |
| **Jupyter Kernels** | `ipykernel install`, `bash_kernel.install`, `installkernel` | `share/jupyter/kernels/` | Локально в проект |

**Главный вывод по сетевым механизмам:**  
В C-коде **нет прямых HTTP-клиентов**. Все сетевые операции производятся вызовом системных утилит (`curl`, `pip`, `Pkg.add`, `apt-get`) внутри генерируемых bash-команд.

---

## 3. Точки расширения для интеграции с сервером-репозиторием

Архитектура TUIOpenMath исключительно удобна для интеграции с удалённым сервером:

### Точка 1: Перенаправление загрузки дистрибутивов (Julia / Python)
* **Файл:** `src/setup_manager.c:78-83` и `src/julia_manager.c:159-164`
* **Было:**
  ```bash
  URL="https://julialang-s3.julialang.org/bin/linux/x64/1.10/$ARCHIVE"
  curl -fL "$URL" -o "$TMP_ARCHIVE"
  ```
* **Точка расширения:**
  Если задан `OPENMATH_SERVER_URL`:
  ```bash
  URL="$OPENMATH_SERVER_URL/artifacts/julia/$ARCHIVE"
  curl -fL "$URL" -o "$TMP_ARCHIVE"
  ```
  Здесь же легко встраивается проверка контрольной суммы:
  ```bash
  echo "$EXPECTED_SHA256  $TMP_ARCHIVE" | sha256sum -c -
  ```

### Точка 2: Перенаправление Python-пакетов (PEP 503 Simple Index)
* **Файл:** `src/setup_manager.c:110` и `src/install_steps.c:116`
* **Было:**
  ```bash
  "envs/$PROFILE/bin/pip" install <packages>
  ```
* **Точка расширения:**
  Стандартный механизм `pip` позволяет перенаправить загрузку на локальный репозиторий:
  ```bash
  "envs/$PROFILE/bin/pip" install --index-url "$OPENMATH_SERVER_URL/simple/" --trusted-host "$SERVER_HOST" <packages>
  ```
  *Клиенту не нужно самому парсить wheel-файлы или зависимости — сервер просто отдаёт PEP 503, а pip делает всю работу локально!*

### Точка 3: Динамические профили (вместо захардкоженных)
* **Файл:** `src/profiles.c:25` (`load_profiles`)
* **Точка расширения:**
  Функция `load_profiles` сейчас принимает аргумент `const char *dir_path`, но игнорирует его:
  ```c
  int load_profiles(AppState *app, const char *dir_path);
  ```
  Достаточно научить её сначала проверять наличие файлов в каталоге кэша `state/remote_profiles/*.json` (скачанных с сервера), а встроенные `BUILTIN_PROFILES[]` использовать как резервный оффлайн-набор.

### Точка 4: Меню управления источником в TUI
* **Файл:** `src/ui.c:250-265`
* **Точка расширения:**
  В меню «Действия» добавляется новая секция: **«Репозиторий» / «Сервер»**:
  - `Проверить статус сервера (Health Check)`
  - `Обновить каталог профилей`
  - `Режим работы (Локальный сервер / Upstream-интернет)`

---

## 4. Технические ограничения и риски

1. **Отсутствие встроенного HTTP-стека в C:**
   * *Риск:* Если писать HTTP-клиент на C с нуля на сокетах — это трата времени и риск утечек памяти.
   * *Решение:* Для базовых запросов метаданных (JSON каталога) использовать вызов системного `curl -s` через существующий `capture_command_output()` в `src/state_manager.c`, с последующим парсингом через `cJSON`. Это займет минимум строк и сохранит надежность.
2. **Сложность зеркалирования PyPI и Julia:**
   * *Риск:* Написание сервера зеркалирования PyPI с нуля — неподъемная задача для диплома (тысячи пакетов, метаданные, разные версии Python, ABI).
   * *Решение:* Ограничить MVP концепции: сервер зеркалирует **только конкретный перечень пакетов** из активных профилей (`numpy`, `scipy`, `sympy`, `matplotlib` и т.д.), формируя простую директорию файлов и HTML-страницы PEP 503.
3. **Зависимость от root-прав (`sudo apt-get`):**
   * Для Octave и Scilab клиент сейчас вызывает `sudo apt-get install`. В изолированной сети без apt-прокси они не установятся.
   * *Решение:* В дипломном MVP сосредоточиться на Python и Julia (они полностью переносимы без root).

---

## 5. Выполнение плана разработки (Roadmap MVP)

### Этап 1. Конфигурация клиента и динамические профили
* [x] Добавить файл конфигурации клиента `config/client.json` (`server_url`, `offline_mode`, `timeout_sec`, `verify_checksum`).
* [x] Восстановить загрузку профилей из JSON-файлов в `src/profiles.c` (приоритет: кэш сервера `state/remote_profiles/` -> локальные `profiles/` -> встроенный набор).
* [x] Нативный C-распаковщик JSON-каталога профилей через `cJSON` (`profiles_unpack_remote_list`).

### Этап 2. Прототип Open Math Server (Python / FastAPI)
* [x] Создать сервер репозитория (`server/app.py`):
  * `GET /` — интерактивный HTML-дашборд со ссылками на API.
  * `GET /api/v1/health` — проверка доступности и статистика хранилища.
  * `GET /api/v1/profiles` и `/api/v1/profiles/{name}` — каталог и спецификации JSON-профилей.
  * `GET /api/v1/artifacts` и `/artifacts/{category}/{filename}` — список и раздача бинарных дистрибутивов с SHA-256.
  * `GET /artifacts/{category}/{filename}.sha256` — прямой запрос контрольной суммы.
  * `GET /simple/` и `/simple/{package}/` — легковесный PEP 503 PyPI Simple Index.
  * `GET /packages/{filename}` — раздача `.whl` и `.tar.gz` пакетов Python.
* [x] Подготовить локальное файловое хранилище сервера (`server/storage/{profiles,artifacts,packages}`).
* [x] Разработать `server/start.sh` с автосозданием venv и гарантированным освобождением порта 8000.

### Этап 3. Интеграция TUI Open Math с сервером
* [x] Реализовать в TUI действие `Проверить соединение с сервером` (пингует `/api/v1/health`).
* [x] Реализовать действие `Синхронизировать профили с репозиторием` (запрос `/api/v1/profiles` и авто-распаковка).
* [x] Реализовать переключение режимов `[ОНЛАЙН]` / `[ОФЛАЙН]`.
* [x] Маркировка источников профилей в интерфейсе: `[сервер]`, `[файл]`, `[встроено]`.
* [x] Модифицировать генераторы установки (`src/setup_manager.c`, `src/install_steps.c`, `src/julia_manager.c`):
  * Скачивание архивов Julia с локального сервера с верификацией контрольной суммы SHA-256.
  * Передача аргументов `--index-url http://<server>/simple/` (в офлайне) или `--extra-index-url` в `pip install`.

### Этап 4. Прототип Open Math Mirror
* [x] Разработать `server/mirror.py` для наполнения локального хранилища:
  * `python3 server/mirror.py list` (просмотр состояния хранилища).
  * `python3 server/mirror.py julia --version 1.10.10` (скачивание Julia LTS).
  * `python3 server/mirror.py packages [profile]` (скачивание wheel-пакетов профиля).
  * `python3 server/mirror.py runtime` (скачивание базового Jupyter runtime).
  * `python3 server/mirror.py sync [profile]` (комплексная синхронизация профиля в одну команду).
* [x] Добавить цели в Makefile: `make mirror-list`, `make mirror-sync`, `make server`, `make server-stop`.

### Этап 5. Испытания и демонстрационный сценарий диплома
* [ ] Подготовка офлайн-пакетов через `make mirror-sync` (или `python3 server/mirror.py sync remote-symbolic`).
* [ ] Переключение клиента в TUI в режим `[ОФЛАЙН]`.
* [ ] Установка профиля `remote-symbolic` исключительно из локального сервера репозитория.
* [ ] Проверка запуска JupyterLab с изолированными ядрами в `share/jupyter`.
