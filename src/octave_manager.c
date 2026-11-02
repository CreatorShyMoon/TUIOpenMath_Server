#include "octave_manager.h"

#include <stdio.h>

int octave_build_install_command(char *out_cmd, size_t out_cmd_size)
{
    if (!out_cmd || out_cmd_size == 0)
    {
        return 0;
    }

    int written = snprintf(
        out_cmd,
        out_cmd_size,
        "bash -lc '"
        "set -euo pipefail; "
        "echo \"[STEP] detect octave\"; "
        "if [ -x \"tools/bin/octave\" ]; then "
        "  echo \"[INFO] octave already installed: $(tools/bin/octave --version | head -n 1)\"; "
        "  exit 0; "
        "elif command -v octave >/dev/null 2>&1; then "
        "  echo \"[INFO] octave already installed: $(octave --version | head -n 1)\"; "
        "  exit 0; "
        "fi; "
        "SERVER_URL=$(python3 -c \"import json; print(json.load(open(\\\"config/client.json\\\")).get(\\\"server_url\\\", \\\"\\\"))\" 2>/dev/null || echo \"\"); "
        "OFFLINE_MODE=$(python3 -c \"import json; print(json.load(open(\\\"config/client.json\\\")).get(\\\"offline_mode\\\", False))\" 2>/dev/null || echo \"False\"); "
        "DOWNLOADED=0; "
        "mkdir -p tools/octave tools/bin; "
        "if [ -n \"$SERVER_URL\" ]; then "
        "  echo \"[STEP] Запрос дистрибутива Octave с сервера $SERVER_URL ...\"; "
        "  OCT_FILE=$(curl -s \"$SERVER_URL/api/v1/artifacts\" | python3 -c \"import sys, json; items = json.load(sys.stdin); print(next((x[\\\"filename\\\"] for x in items if x.get(\\\"category\\\") == \\\"octave\\\" and x.get(\\\"size_bytes\\\", 0) > 100*1024), \\\"\\\"))\" 2>/dev/null || echo \"\"); "
        "  if [ -n \"$OCT_FILE\" ]; then "
        "    echo \"[STEP] Загрузка артефакта Octave: $OCT_FILE ...\"; "
        "    curl -s -f --connect-timeout 5 -m 300 \"$SERVER_URL/artifacts/octave/$OCT_FILE\" -o \"/tmp/$OCT_FILE\"; "
        "    if [ -f \"/tmp/$OCT_FILE\" ]; then "
        "      case \"$OCT_FILE\" in "
        "        *.deb) "
        "          if command -v dpkg-deb >/dev/null 2>&1; then "
        "            dpkg-deb -x \"/tmp/$OCT_FILE\" tools/octave; "
        "            [ -f tools/octave/usr/bin/octave ] && ln -sfn \"$(pwd)/tools/octave/usr/bin/octave\" tools/bin/octave; "
        "            [ -f tools/octave/usr/bin/octave-cli ] && ln -sfn \"$(pwd)/tools/octave/usr/bin/octave-cli\" tools/bin/octave-cli; "
        "            DOWNLOADED=1; "
        "          elif command -v dpkg >/dev/null 2>&1; then "
        "            sudo dpkg -i \"/tmp/$OCT_FILE\" || true; "
        "            DOWNLOADED=1; "
        "          fi ;; "
        "        *.AppImage) "
        "          mv \"/tmp/$OCT_FILE\" tools/octave/octave.AppImage; "
        "          chmod +x tools/octave/octave.AppImage; "
        "          ln -sfn \"$(pwd)/tools/octave/octave.AppImage\" tools/bin/octave; "
        "          ln -sfn \"$(pwd)/tools/octave/octave.AppImage\" tools/bin/octave-cli; "
        "          DOWNLOADED=1 ;; "
        "        *.tar.gz) "
        "          tar -xzf \"/tmp/$OCT_FILE\" -C tools/octave --strip-components=1; "
        "          [ -f tools/octave/bin/octave ] && ln -sfn \"$(pwd)/tools/octave/bin/octave\" tools/bin/octave; "
        "          [ -f tools/octave/bin/octave-cli ] && ln -sfn \"$(pwd)/tools/octave/bin/octave-cli\" tools/bin/octave-cli; "
        "          DOWNLOADED=1 ;; "
        "      esac; "
        "      rm -f \"/tmp/$OCT_FILE\"; "
        "      echo \"[OK] Octave успешно установлен из серверного пакета $OCT_FILE\"; "
        "    fi; "
        "  fi; "
        "fi; "
        "if [ $DOWNLOADED -eq 0 ]; then "
        "  if [ \"$OFFLINE_MODE\" = \"True\" ]; then "
        "    echo \"[FAIL] Офлайн-режим: дистрибутив Octave отсутствует на сервере в storage/artifacts/octave/\" >&2; "
        "    echo \"[HINT] Разместите octave-portable.tar.gz или octave.AppImage в server/storage/artifacts/octave/\" >&2; "
        "    exit 1; "
        "  fi; "
        "  echo \"[STEP] install octave via apt\"; "
        "  sudo apt-get update; "
        "  sudo apt-get install -y octave; "
        "fi; "
        "tools/bin/octave --version 2>/dev/null || octave --version | head -n 1"
        "'"
    );

    return written >= 0 && (size_t)written < out_cmd_size;
}

int octave_build_remove_command(char *out_cmd, size_t out_cmd_size)
{
    if (!out_cmd || out_cmd_size == 0)
    {
        return 0;
    }

    int written = snprintf(
        out_cmd,
        out_cmd_size,
        "bash -lc '"
        "set -euo pipefail; "
        "echo \"[STEP] remove octave\"; "
        "if command -v octave >/dev/null 2>&1; then "
        "  sudo apt-get remove -y octave; "
        "  sudo apt-get autoremove -y; "
        "  echo \"[INFO] octave removed\"; "
        "else "
        "  echo \"[WARN] octave is not installed\"; "
        "fi; "
        "command -v octave >/dev/null 2>&1 && octave --version | head -n 1 || echo \"[INFO] octave command not found\""
        "'"
    );

    return written >= 0 && (size_t)written < out_cmd_size;
}
