# Патч №5: Полная изоляция Jupyter и ядер в локальном окружении

## Назначение патча

Устранение конфликтов между SysOpenMath / TUIOpenMath и глобальной системой пользователя (`/home/<user>/.local/share/jupyter/kernels/`).

### Проблема, которая была:
1. При установке профилей и ядер (`ipykernel`, `octave_kernel`, `bash_kernel`, `scilab_kernel`, `IJulia`) ядра регистрировались с флагом `--user`, из-за чего попадали напрямую в системную папку пользователя `~/.local/share/jupyter/kernels/`.
2. В коде очистки (`src/cleanup_manager.c`) скрипт обходил `$HOME/.local/share/jupyter/kernels` и выполнял `rm -rf "${dir}/python3"`, что создавало риск удаления системного ядра Python и ядер пользователя.
3. Внешний Jupyter пользователя видел чужие профильные ядра, а ядра с одинаковыми именами (`octave`, `bash`, `scilab`) перезаписывали системные.

### Решение:
Все ядра и данные Jupyter теперь изолированы внутри рабочей папки проекта:
`$(pwd)/share/jupyter/kernels/` через стандартную переменную окружения **`JUPYTER_DATA_DIR`**.

---

## Список изменённых файлов

| Файл | Что изменено |
|---|---|
| `manager.c` | Добавлена функция `ensure_jupyter_env()`, устанавливающая `JUPYTER_DATA_DIR` в `share/jupyter` на уровне процесса. |
| `src/kernel_manager.c` | В команды регистрации ядер Python, Julia, Bash, Octave добавлен `export JUPYTER_DATA_DIR="$(pwd)/share/jupyter"`. |
| `src/scilab_manager.c` | В команду регистрации Scilab-ядра добавлен экспорт `JUPYTER_DATA_DIR`. |
| `src/setup_manager.c` | Добавлен экспорт `JUPYTER_DATA_DIR` при полной установке профиля и его ядер. |
| `src/jupyter_manager.c` | Изолированы команды проверки версии, списка kernels и фонового запуска JupyterLab. |
| `src/actions.c` | В команду перезапуска JupyterLab добавлен экспорт `JUPYTER_DATA_DIR`. |
| `src/cleanup_manager.c` | **Устранена деструктивная очистка**: удален перебор `$HOME/.local/...` и `/usr/...`, удалена команда `rm -rf python3`. Очистка удаляет только локальную `share/jupyter/`. |
| `src/checks.c` | В заголовок диагностических скриптов добавлен экспорт `JUPYTER_DATA_DIR`. |
| `src/install_steps.c` | В заголовок скриптов установки добавлен экспорт `JUPYTER_DATA_DIR`. |
| `Makefile` | Добавлены флаги для корректной сборки как под Linux, так и под macOS. |

---

## Детали изменений по файлам

### 1. `manager.c`
В начале файла добавлен макрос стандарта POSIX (для корректной декларации `setenv` в `glibc` при флаге `-std=c11` на Debian/Ubuntu), а при запуске создаётся локальная директория и выставляется переменная окружения для всех дочерних процессов:
```c
#define _POSIX_C_SOURCE 200809L

#include "actions.h"
...

static void ensure_jupyter_env(void)
{
    mkdir("share", 0755);
    mkdir("share/jupyter", 0755);
    mkdir("share/jupyter/kernels", 0755);

    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)))
    {
        char jdata[2048];
        snprintf(jdata, sizeof(jdata), "%s/share/jupyter", cwd);
        setenv("JUPYTER_DATA_DIR", jdata, 1);
    }
}
```

### 2. `src/kernel_manager.c`
Во все функции регистрации ядер добавлен экспорт локальной директории перед выполнением:
```c
// Было:
"bash -lc '"
"PYBIN=\"envs/%s/bin/python\"; "
...
"\"$PYBIN\" -m ipykernel install --user --name \"sysopenmath-%s\" ... "

// Стало:
"bash -lc '"
"export JUPYTER_DATA_DIR=\"$(pwd)/share/jupyter\"; "
"mkdir -p share/jupyter/kernels; "
"PYBIN=\"envs/%s/bin/python\"; "
...
```
*(Аналогично для `Julia`, `Bash` и `Octave`).*

### 3. `src/cleanup_manager.c`
Полностью исключено вмешательство в домашнюю директорию пользователя:

```c
// Было в cleanup_build_clear_kernels_command:
"for dir in "
"  \"tools/jupyter/runtime/share/jupyter/kernels\" "
"  \"$HOME/.local/share/jupyter/kernels\" "
"  $HOME/snap/code/*/.local/share/jupyter/kernels "
"  \"/usr/local/share/jupyter/kernels\" "
"  \"/usr/share/jupyter/kernels\"; do "
"  rm -rf \"${dir}/python3\"; " // ОПАСНО!

// Стало:
"for dir in "
"  \"share/jupyter/kernels\" "
"  \"tools/jupyter/runtime/share/jupyter/kernels\"; do "
"  if [ -d \"$dir\" ]; then "
"    rm -rf \"${dir}\"/*; "
"  fi; "
"done; "
```

В `cleanup_build_remove_jupyter_command` вместо удаления пользовательских ядер теперь очищается локальная директория:
```c
"rm -rf \"share/jupyter\"; "
```

### 4. `src/jupyter_manager.c` и `src/actions.c`
Запуск JupyterLab и вывод списка `kernelspec` теперь всегда обращаются к локальной папке проекта:
```c
"export JUPYTER_DATA_DIR=\"$(pwd)/share/jupyter\"; "
```

---

## Примечание: визуальный «обрыв» длинных строк лога

В TUIOpenMath строки в окне «Лог» жестко ограничиваются шириной экрана терминала:
```c
mvwprintw(app->log_win, row, 1, "%.*s", max_x - 2, lines[i]);
```
На экранах с шириной 102 колонки (например, ThinkPad 102x27) длинная команда запуска JupyterLab визуально обрезается ровно на 100-м символе (где находится текст `; if`). 

Команда на самом деле не прерывается и выполняется полностью. Чтобы просмотреть результат и URL запуска в интерфейсе:
1. Нажмите клавишу **`Tab`** для перехода в секцию «Лог».
2. Нажмите **`End`** или стрелку **`↓`**, чтобы прокрутить лог до конца.

---

## Как удалить старое ядро из глобальной системы (если успело прописаться)

В терминале пользователя выполнить разово:
```bash
rm -rf ~/.local/share/jupyter/kernels/octave
```
