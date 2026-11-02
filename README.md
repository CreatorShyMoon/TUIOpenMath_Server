# SysOpenMath / TUIopenMath

## Управляемая математическая вычислительная платформа

SysOpenMath / TUIopenMath — это терминально-ориентированная система управления математической и научной вычислительной средой для Linux.

Платформа реализована в форме текстового пользовательского интерфейса (TUI) на базе библиотеки `ncurses` и предназначена для централизованного управления вычислительными компонентами, Jupyter-средой и профильными workspace.

Основная идея проекта заключается в переходе от ручной настройки вычисительных инструментов к модели управляемой вычислительной платформы.

Пользователь взаимодействует не с отдельными пакетами и зависимостями, а с готовыми профилями вычислительной среды.

---

# Основные возможности

## Профильная модель вычислительной среды

Система использует встроенные профили вычислительной среды.

Каждый профиль определяет:

- Python-библиотеки;
- Julia-пакеты;
- kernels Jupyter;
- системные зависимости;
- параметры вычислительной среды.

Примеры профилей:

- `symbolic`
- `numerical`
- `scientific`
- `minimal`

Пример структуры профиля:

```json
{
  "name": "symbolic",
  "description": "Профиль для символьной математики",
  "locked": true,
  "python": ["sympy", "matplotlib", "jupyterlab"],
  "julia": ["IJulia", "Symbolics"],
  "system": [],
  "kernels": ["python", "julia", "bash"]
}
```

---

# Что умеет система

Платформа позволяет:

- устанавливать профильные environments;
- устанавливать Python dependencies;
- устанавливать Julia packages;
- устанавливать Jupyter runtime;
- регистрировать kernels;
- запускать JupyterLab;
- выполнять диагностику платформы;
- проверять readiness профилей;
- управлять workspace;
- очищать kernels и runtime state;
- работать с Octave и Scilab;
- централизованно управлять вычислительной средой.

---

# Архитектура платформы

SysOpenMath построен по модульному принципу.

Основные модули:

```text
src/
 ├── ui.c
 ├── profile_manager.c
 ├── diagnostics.c
 ├── jupyter_manager.c
 ├── julia_manager.c
 ├── octave_manager.c
 ├── scilab_manager.c
 ├── kernel_manager.c
 ├── logger.c
 └── workspace_manager.c
```

Каждый компонент отвечает за отдельную часть вычислительной платформы.

---

# Терминальный интерфейс

Платформа использует TUI-интерфейс на основе `ncurses`.

Интерфейс включает:

- splash-screen;
- систему логирования;
- меню управления;
- экран диагностики;
- экран установки;
- управление профилями;
- запуск JupyterLab.

После запуска отображается splash-logo SysOpenMath.

---

# Управление в интерфейсе

Основные клавиши:

- `Tab` — переключение между секциями;
- `↑ / ↓` — навигация;
- `Enter` — выполнение действия;
- `←` или `Backspace` — возврат назад;
- `PgUp / PgDn` — прокрутка логов;
- `q` — выход из программы.

---

# Система диагностики

Платформа реализует централизованную систему диагностики.

Диагностика разделяется на:

## Платформенную диагностику

Проверяет:

- Python runtime;
- Jupyter runtime;
- системные зависимости;
- структуру каталогов;
- runtime-компоненты.

## Профильную диагностику

Проверяет исключительно компоненты активного профиля:

- Python imports;
- Julia packages;
- kernels;
- environment;
- readiness профиля.

---

# InstallStatus

Для оценки состояния компонентов используется единая модель:

```c
typedef enum InstallStatus
{
    INSTALL_STATUS_READY,
    INSTALL_STATUS_BROKEN,
    INSTALL_STATUS_MISMATCH,
    INSTALL_STATUS_NOT_INSTALLED,
    INSTALL_STATUS_SKIP
} InstallStatus;
```

## READY

Компонент полностью готов.

## BROKEN

Компонент установлен, но повреждён или работает некорректно.

## MISMATCH

Обнаружена несовместимая версия.

## NOT_INSTALLED

Компонент отсутствует.

## SKIP

Компонент не используется активным профилем.

---

# Проверка готовности среды

Система поддерживает readiness-модель вычислительной среды.

Проверяются:

- virtual environment;
- Python imports;
- kernels;
- JupyterLab;
- runtime;
- Julia packages;
- системные зависимости.

Пример логов:

```text
[INFO] Checking profile symbolic
[OK] python import: sympy
[OK] julia imports
[SKIP] scilab not required
[READY] profile symbolic
```

---

# JupyterLab Integration

SysOpenMath автоматически:

- запускает JupyterLab;
- регистрирует kernels;
- подготавливает workspace;
- выводит URL подключения;
- предотвращает повторный запуск.

Пример:

```text
[INFO] JupyterLab started
[INFO] URL: http://localhost:8888/lab?token=...
```

---

# Поддерживаемые kernels

| Kernel | Статус |
|---|---|
| Python | Supported |
| Julia | Supported |
| Bash | Supported |
| Octave | Supported |
| Scilab | Experimental |

---

# Работа с Octave

После установки Octave система позволяет:

- зарегистрировать Octave kernel;
- использовать Octave внутри JupyterLab;
- выполнять диагностику Octave environment.

Проверка:

```bash
command -v octave
octave --version | head -n 1
```

---

# Работа со Scilab

После установки Scilab система позволяет:

- зарегистрировать Scilab kernel;
- использовать Scilab внутри JupyterLab;
- выполнять проверку состояния Scilab.

Проверка:

```bash
command -v scilab
```

---

# Workspace

Платформа использует централизованное workspace.

Структура:

```text
workspace/
 ├── notebooks/
 ├── profiles/
 ├── kernels/
 ├── logs/
 └── runtime/
```

Workspace используется для:

- notebooks;
- учебных материалов;
- kernels;
- runtime-конфигураций;
- логов системы.

---

# Установка

## Зависимости Linux

Ubuntu / Debian:

```bash
sudo apt update

sudo apt install -y \
    build-essential \
    gcc \
    make \
    libncursesw5-dev \
    libcjson-dev \
    python3 \
    python3-venv \
    python3-pip \
    curl \
```

Дополнительно:

```bash
sudo apt install octave
sudo apt install scilab
```

---

# Сборка

```bash
make clean
make
```

или:

```bash
gcc src/*.c -o manager -lncursesw -lcjson
```

---

# Запуск

```bash
./manager
```

После запуска пользователь может:

- выбрать профиль;
- установить environment;
- выполнить диагностику;
- зарегистрировать kernels;
- запустить JupyterLab.

---

# Полезные команды

## Проверка Jupyter

```bash
tools/bin/jupyter --version
tools/bin/jupyter kernelspec list
```

## Проверка Julia

```bash
tools/bin/julia --version
```

## Проверка Python environment

```bash
envs/symbolic/bin/python --version
envs/symbolic/bin/pip --version
```

## Проверка kernels

```bash
tools/bin/jupyter kernelspec list
```

---

# Типичные проблемы

## JupyterLab не запускается

Проверка:

```bash
tools/bin/jupyter --version
```

---

## Не отображается kernel

Проверка:

```bash
tools/bin/jupyter kernelspec list
```

---

## Отсутствует profile environment

Если отсутствует:

```text
envs/<profile>/bin/python
```

необходимо повторно установить профиль.

---

## После удаления компонента kernel остался

Следует выполнить:

- очистку kernels;
- повторную диагностику среды.

---

# Принцип работы с профилями

Пользователь не конструирует вычислительную платформу вручную.

Профили рассматриваются как встроенные сценарии развёртывания, подготовленные разработчиком.

Пользователь выбирает профиль, а система автоматически:

- устанавливает зависимости;
- создаёт environment;
- подготавливает kernels;
- конфигурирует JupyterLab;
- выполняет диагностику готовности среды.

---

# Roadmap

Планируемые направления развития:

- dependency graph;
- auto-repair environments;
- plugin-system;
- cloud profiles;
- GUI-interface;
- JupyterHub integration;
- educational workspace templates;
- distributed scientific environments;
- централизованные profile repositories.

---

# Назначение проекта

SysOpenMath создаётся как:

- управляемая математическая среда;
- образовательная вычислительная платформа;
- scientific workspace;
- система стандартизации вычислительных environments.

Проект ориентирован на:

- студентов;
- преподавателей;
- исследователей;
- инженеров;
- разработчиков scientific software.

---

# Текущее состояние проекта

Текущая версия включает:

- профильную систему;
- readiness diagnostics;
- InstallStatus model;
- автоматическую установку компонентов;
- регистрацию kernels;
- TUI-интерфейс;
- splash-screen;
- централизованное логирование;
- запуск JupyterLab;
- workspace management.

Проект продолжает активно развиваться.

---
