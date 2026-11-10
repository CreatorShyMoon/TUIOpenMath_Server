CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -Iinclude
LIBS = -lncursesw -lcjson

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    CFLAGS += -I/opt/homebrew/include
    LIBS = -lncurses -lcjson -L/opt/homebrew/lib
endif

TARGET = manager

SRC = manager.c \
	src/ui.c \
	src/logger.c \
	src/process.c \
	src/profiles.c \
	src/actions.c \
	src/checks.c \
	src/profile_config.c \
	src/install_steps.c \
	src/julia_manager.c \
	src/jupyter_manager.c \
	src/kernel_manager.c \
	src/state_manager.c \
	src/cleanup_manager.c \
	src/workspace_manager.c \
	src/setup_manager.c \
	src/octave_manager.c \
	src/profile_registry.c \
	src/scilab_manager.c \
	src/client_config.c 

all:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET)

reset: clean
	@rm -rf envs/* tools/* logs/* workspaces/* state/* 2>/dev/null || true
	@touch envs/.gitkeep tools/.gitkeep logs/.gitkeep workspaces/.gitkeep state/.gitkeep
	@rm -rf /tmp/sysopenmath_* /tmp/*.tar.gz /tmp/*.whl 2>/dev/null || true
	@echo "[OK] Все виртуальные окружения, скачанные среды, ядра и логи очищены."
	@echo "[OK] Проект сброшен до абсолютно чистого (заводского) состояния!"

server:
	@chmod +x server/start.sh
	./server/start.sh

server-stop:
	@pkill -9 -f "uvicorn.*app:app" 2>/dev/null || true
	@(fuser -k 8000/tcp) >/dev/null 2>&1 || true
	@lsof -ti :8000 2>/dev/null | xargs kill -9 2>/dev/null || true
	@echo "[OK] Процессы сервера на порту 8000 остановлены."

mirror-list:
	python3 server/mirror.py list

mirror-sync:
	python3 server/mirror.py sync all

mirror-all:
	python3 server/mirror.py sync all

mirror-julia:
	python3 server/mirror.py julia

mirror-scilab:
	python3 server/mirror.py scilab

mirror-octave:
	python3 server/mirror.py octave

mirror-runtime:
	python3 server/mirror.py runtime

bundle: all
	python3 server/mirror.py bundle

client-dist:
	@mkdir -p dist/sysopenmath-client/workspaces dist/sysopenmath-client/state dist/sysopenmath-client/logs dist/sysopenmath-client/tools dist/sysopenmath-client/envs dist/sysopenmath-client/profiles
	@touch dist/sysopenmath-client/workspaces/.gitkeep dist/sysopenmath-client/state/.gitkeep dist/sysopenmath-client/logs/.gitkeep dist/sysopenmath-client/tools/.gitkeep dist/sysopenmath-client/envs/.gitkeep
	@cp -r src include manager.c config dist/sysopenmath-client/
	@rm -f dist/sysopenmath-client/manager dist/sysopenmath-client/*.o
	@tar -czf sysopenmath-client.tar.gz --exclude='.DS_Store' -C dist sysopenmath-client
	@echo "[OK] Клиентский дистрибутив сформирован: dist/sysopenmath-client/"
	@echo "[OK] Архив для передачи пользователям: sysopenmath-client.tar.gz"

.PHONY: all run clean reset server server-stop mirror-list mirror-sync mirror-all mirror-julia mirror-scilab mirror-octave mirror-runtime bundle client-dist
