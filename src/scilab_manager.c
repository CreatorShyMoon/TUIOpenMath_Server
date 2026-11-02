#include "scilab_manager.h"

#include <stdio.h>

int scilab_build_install_command(char *out_cmd, size_t out_cmd_size)
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
        "echo \"[STEP] detect scilab\"; "
        "if [ -x \"tools/bin/scilab-cli\" ] || [ -x \"tools/bin/scilab\" ]; then "
        "  echo \"[INFO] scilab already installed: $(tools/bin/scilab-cli -version 2>/dev/null || tools/bin/scilab -version | head -n 1)\"; "
        "  exit 0; "
        "elif command -v scilab-adv-cli >/dev/null 2>&1 || command -v scilab-cli >/dev/null 2>&1 || command -v scilab >/dev/null 2>&1; then "
        "  echo \"[INFO] scilab already installed: $(scilab-cli -version 2>/dev/null || scilab -version 2>/dev/null | head -n 1)\"; "
        "  exit 0; "
        "fi; "
        "SERVER_URL=$(python3 -c \"import json; print(json.load(open(\\\"config/client.json\\\")).get(\\\"server_url\\\", \\\"\\\"))\" 2>/dev/null || echo \"\"); "
        "OFFLINE_MODE=$(python3 -c \"import json; print(json.load(open(\\\"config/client.json\\\")).get(\\\"offline_mode\\\", False))\" 2>/dev/null || echo \"False\"); "
        "DOWNLOADED=0; "
        "mkdir -p tools/scilab tools/bin; "
        "if [ -n \"$SERVER_URL\" ]; then "
        "  echo \"[STEP] Запрос дистрибутива Scilab с локального сервера $SERVER_URL ...\"; "
        "  SCI_FILES=$(curl -s \"$SERVER_URL/api/v1/artifacts\" | python3 -c \"import sys, json; items = json.load(sys.stdin); "
        "sci = [x[\\\"filename\\\"] for x in items if x.get(\\\"category\\\") == \\\"scilab\\\" and x.get(\\\"size_bytes\\\", 0) > 100*1024]; "
        "tars = [f for f in sci if f.endswith(\\\".tar.gz\\\") or f.endswith(\\\".tgz\\\")]; "
        "print(\\\" \\\".join(tars if tars else sci))\" 2>/dev/null || echo \"\"); "
        "  if [ -n \"$SCI_FILES\" ]; then "
        "    for F in $SCI_FILES; do "
        "      echo \"[STEP] Загрузка артефакта Scilab ($F) с сервера...\"; "
        "      curl -s -f --connect-timeout 5 -m 300 \"$SERVER_URL/artifacts/scilab/$F\" -o \"/tmp/$F\"; "
        "      if [ -f \"/tmp/$F\" ]; then "
        "        case \"$F\" in "
        "          *.deb) "
        "            if command -v dpkg-deb >/dev/null 2>&1; then "
        "              dpkg-deb -x \"/tmp/$F\" tools/scilab; "
        "            elif command -v dpkg >/dev/null 2>&1; then "
        "              sudo dpkg -i \"/tmp/$F\" || true; "
        "            fi ;; "
        "          *) "
        "            tar -xzf \"/tmp/$F\" -C tools/scilab --strip-components=1 ;; "
        "        esac; "
        "        rm -f \"/tmp/$F\"; "
        "      fi; "
        "    done; "
        "    if [ -x tools/scilab/bin/scilab-cli ]; then "
        "      ln -sfn \"$(pwd)/tools/scilab/bin/scilab-cli\" tools/bin/scilab-cli; "
        "      [ -x tools/scilab/bin/scilab ] && ln -sfn \"$(pwd)/tools/scilab/bin/scilab\" tools/bin/scilab; "
        "      DOWNLOADED=1; "
        "    elif [ -x tools/scilab/usr/bin/scilab-cli ]; then "
        "      ln -sfn \"$(pwd)/tools/scilab/usr/bin/scilab-cli\" tools/bin/scilab-cli; "
        "      [ -x tools/scilab/usr/bin/scilab ] && ln -sfn \"$(pwd)/tools/scilab/usr/bin/scilab\" tools/bin/scilab; "
        "      DOWNLOADED=1; "
        "    elif [ -x tools/scilab/bin/scilab ]; then "
        "      ln -sfn \"$(pwd)/tools/scilab/bin/scilab\" tools/bin/scilab-cli; "
        "      ln -sfn \"$(pwd)/tools/scilab/bin/scilab\" tools/bin/scilab; "
        "      DOWNLOADED=1; "
        "    fi; "
        "    if [ $DOWNLOADED -eq 1 ]; then "
        "      echo \"[OK] Scilab успешно установлен из локального репозитория\"; "
        "    fi; "
        "  fi; "
        "fi; "
        "if [ $DOWNLOADED -eq 0 ]; then "
        "  if [ \"$OFFLINE_MODE\" = \"True\" ]; then "
        "    echo \"[FAIL] Офлайн-режим: дистрибутив Scilab отсутствует на локальном сервере в storage/artifacts/scilab/\" >&2; "
        "    echo \"[HINT] На сервере выполните: python3 server/mirror.py scilab\" >&2; "
        "    exit 1; "
        "  fi; "
        "  echo \"[STEP] Установка Scilab через apt...\"; "
        "  sudo apt-get update && sudo apt-get install -y scilab; "
        "fi; "
        "tools/bin/scilab-cli -version 2>/dev/null || scilab-cli -version 2>/dev/null || scilab -version | head -n 1"
        "'"
    );

    return written >= 0 && (size_t)written < out_cmd_size;
}

int scilab_build_remove_command(char *out_cmd, size_t out_cmd_size)
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
        "echo \"[STEP] remove scilab\"; "
        "if command -v scilab >/dev/null 2>&1 || command -v scilab-cli >/dev/null 2>&1 || command -v scilab-adv-cli >/dev/null 2>&1; then "
        "  sudo apt-get remove -y scilab; "
        "  sudo apt-get autoremove -y; "
        "  echo \"[INFO] scilab removed\"; "
        "else "
        "  echo \"[WARN] scilab is not installed\"; "
        "fi; "
        "if command -v scilab-adv-cli >/dev/null 2>&1 || command -v scilab-cli >/dev/null 2>&1 || command -v scilab >/dev/null 2>&1; then "
        "  echo \"[WARN] Scilab executable still present\"; "
        "else "
        "  echo \"[INFO] scilab command not found\"; "
        "fi"
        "'"
    );

    return written >= 0 && (size_t)written < out_cmd_size;
}

int scilab_build_register_kernel_command(char *out_cmd, size_t out_cmd_size)
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
        "export JUPYTER_DATA_DIR=\"$(pwd)/share/jupyter\"; "
        "mkdir -p share/jupyter/kernels; "
        "PYBIN=\"tools/jupyter/runtime/bin/python\"; "
        "if [ ! -x \"$PYBIN\" ]; then "
        "  echo \"[ERROR] Jupyter runtime not installed\"; "
        "  exit 1; "
        "fi; "
        "if [ -x \"tools/bin/scilab-cli\" ]; then "
        "  export SCILAB_EXECUTABLE=\"$(pwd)/tools/bin/scilab-cli\"; "
        "elif [ -x \"tools/bin/scilab\" ]; then "
        "  export SCILAB_EXECUTABLE=\"$(pwd)/tools/bin/scilab\"; "
        "elif command -v scilab-adv-cli >/dev/null 2>&1; then "
        "  export SCILAB_EXECUTABLE=\"$(command -v scilab-adv-cli)\"; "
        "elif command -v scilab-cli >/dev/null 2>&1; then "
        "  export SCILAB_EXECUTABLE=\"$(command -v scilab-cli)\"; "
        "elif command -v scilab >/dev/null 2>&1; then "
        "  export SCILAB_EXECUTABLE=\"$(command -v scilab)\"; "
        "else "
        "  echo \"[ERROR] Scilab executable not found\"; "
        "  exit 1; "
        "fi; "
        "SERVER_URL=$(python3 -c \"import json; print(json.load(open(\\\"config/client.json\\\")).get(\\\"server_url\\\", \\\"\\\"))\" 2>/dev/null || echo \"\"); "
        "OFFLINE_MODE=$(python3 -c \"import json; print(json.load(open(\\\"config/client.json\\\")).get(\\\"offline_mode\\\", False))\" 2>/dev/null || echo \"False\"); "
        "PIP_ARGS=\"\"; "
        "if [ -n \"$SERVER_URL\" ] && [ \"$OFFLINE_MODE\" = \"True\" ]; then "
        "  PIP_HOST=$(python3 -c \"from urllib.parse import urlparse; print(urlparse(\\\"$SERVER_URL\\\").hostname or \\\"127.0.0.1\\\")\" 2>/dev/null || echo \"127.0.0.1\"); "
        "  PIP_ARGS=\"--index-url $SERVER_URL/simple/ --trusted-host $PIP_HOST\"; "
        "fi; "
        "\"$PYBIN\" -m pip install $PIP_ARGS scilab_kernel || true; "
        "\"$PYBIN\" -m scilab_kernel install --user; "
        "echo \"[INFO] Scilab kernel registered\"; "
        "tools/bin/jupyter kernelspec list"
        "'"
    );

    return written >= 0 && (size_t)written < out_cmd_size;
}
