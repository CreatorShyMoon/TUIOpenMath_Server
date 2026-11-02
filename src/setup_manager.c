#include "setup_manager.h"
#include "profile_config.h"
#include "profiles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int list_contains(const StringList *list, const char *value)
{
    if (!list || !value)
    {
        return 0;
    }

    for (size_t i = 0; i < list->count; i++)
    {
        if (strcmp(list->items[i], value) == 0)
        {
            return 1;
        }
    }

    return 0;
}

int setup_build_full_profile_command(
    const Profile *profile,
    char *out_cmd,
    size_t out_cmd_size
)
{
    if (!profile || !out_cmd || out_cmd_size == 0)
    {
        return 0;
    }

    if (!profile_name_is_safe(profile->name))
    {
        return 0;
    }

    ProfileConfig config;
    int loaded = 0;
    if (profile->path[0] != '\0')
    {
        loaded = profile_config_load(profile->path, &config);
    }
    if (!loaded)
    {
        loaded = profile_config_load(profile->name, &config);
    }
    if (!loaded)
    {
        return 0;
    }

    char script_path[] = "/tmp/sysopenmath_full_setup_XXXXXX";
    int fd = mkstemp(script_path);
    if (fd == -1)
    {
        profile_config_free(&config);
        return 0;
    }

    FILE *f = fdopen(fd, "wb");
    if (!f)
    {
        close(fd);
        unlink(script_path);
        profile_config_free(&config);
        return 0;
    }

    fprintf(f, "#!/usr/bin/env bash\n");
    fprintf(f, "set -euo pipefail\n");
    fprintf(f, "trap 'rm -f -- \"$0\"' EXIT\n\n");
    fprintf(f, "export PATH=\"$(pwd)/tools/bin:$PATH\"\n");
    fprintf(f, "export JUPYTER_DATA_DIR=\"$(pwd)/share/jupyter\"\n");
    fprintf(f, "mkdir -p share/jupyter/kernels tools/bin\n");
    fprintf(f, "PROFILE=\"%s\"\n", profile->name);
    fprintf(f, "LOG_PREFIX=\"[SETUP]\"\n");
    fprintf(f, "SERVER_URL=$(python3 -c \"import json; print(json.load(open('config/client.json')).get('server_url', ''))\" 2>/dev/null || echo \"\")\n");
    fprintf(f, "OFFLINE_MODE=$(python3 -c \"import json; print(json.load(open('config/client.json')).get('offline_mode', False))\" 2>/dev/null || echo \"False\")\n");
    fprintf(f, "PIP_ARGS=\"\"\n");
    fprintf(f, "if [ -n \"$SERVER_URL\" ]; then\n");
    fprintf(f, "  PIP_HOST=$(python3 -c \"from urllib.parse import urlparse; print(urlparse('$SERVER_URL').hostname or '127.0.0.1')\" 2>/dev/null || echo \"127.0.0.1\")\n");
    fprintf(f, "  if [ \"$OFFLINE_MODE\" = \"True\" ]; then\n");
    fprintf(f, "    PIP_ARGS=\"--index-url $SERVER_URL/simple/ --trusted-host $PIP_HOST\"\n");
    fprintf(f, "  else\n");
    fprintf(f, "    PIP_ARGS=\"--extra-index-url $SERVER_URL/simple/ --trusted-host $PIP_HOST\"\n");
    fprintf(f, "  fi\n");
    fprintf(f, "fi\n\n");

    // 1. Julia LTS
    fprintf(f, "# -------------------------------------------------------------------------\n");
    fprintf(f, "# 1. Julia LTS\n");
    fprintf(f, "# -------------------------------------------------------------------------\n");
    fprintf(f, "if [ -x \"tools/bin/julia\" ]; then\n");
    fprintf(f, "  echo \"$LOG_PREFIX Julia already installed: $(tools/bin/julia --version)\"\n");
    fprintf(f, "else\n");
    fprintf(f, "  echo \"$LOG_PREFIX install Julia LTS\"\n");
    fprintf(f, "  mkdir -p tools/julia tools/bin\n");
    fprintf(f, "  ARCHIVE=\"julia-1.10.10-linux-x86_64.tar.gz\"\n");
    fprintf(f, "  URL=\"https://julialang-s3.julialang.org/bin/linux/x64/1.10/$ARCHIVE\"\n");
    fprintf(f, "  TARGET_DIR=\"tools/julia/lts\"\n");
    fprintf(f, "  TMP_ARCHIVE=$(mktemp /tmp/sysopenmath_julia_XXXXXX.tar.gz)\n");
    fprintf(f, "  rm -rf \"$TARGET_DIR\"\n");
    fprintf(f, "  mkdir -p \"$TARGET_DIR\"\n");
    fprintf(f, "  DOWNLOADED=0\n");
    fprintf(f, "  if [ -n \"$SERVER_URL\" ]; then\n");
    fprintf(f, "    echo \"$LOG_PREFIX Проверка Julia $ARCHIVE на сервере: $SERVER_URL ...\"\n");
    fprintf(f, "    if curl -s -f --connect-timeout 5 -m 300 \"$SERVER_URL/artifacts/julia/$ARCHIVE\" -o \"$TMP_ARCHIVE\"; then\n");
    fprintf(f, "      echo \"$LOG_PREFIX [OK] Загружено с сервера Open Math\"\n");
    fprintf(f, "      DOWNLOADED=1\n");
    fprintf(f, "      if curl -s -f -m 5 \"$SERVER_URL/artifacts/julia/$ARCHIVE.sha256\" -o \"$TMP_ARCHIVE.sha256\"; then\n");
    fprintf(f, "        EXPECTED_SHA=$(awk '{print $1}' \"$TMP_ARCHIVE.sha256\")\n");
    fprintf(f, "        rm -f \"$TMP_ARCHIVE.sha256\"\n");
    fprintf(f, "        if [ -n \"$EXPECTED_SHA\" ] && command -v sha256sum >/dev/null 2>&1; then\n");
    fprintf(f, "          echo \"$LOG_PREFIX Верификация SHA-256: $EXPECTED_SHA ...\"\n");
    fprintf(f, "          REAL_SHA=$(sha256sum \"$TMP_ARCHIVE\" | awk '{print $1}')\n");
    fprintf(f, "          if [ \"$EXPECTED_SHA\" != \"$REAL_SHA\" ]; then\n");
    fprintf(f, "            echo \"$LOG_PREFIX [ERROR] Не совпала контрольная сумма архива Julia!\" >&2\n");
    fprintf(f, "            rm -f \"$TMP_ARCHIVE\"\n");
    fprintf(f, "            exit 1\n");
    fprintf(f, "          fi\n");
    fprintf(f, "          echo \"$LOG_PREFIX [OK] Контрольная сумма SHA-256 подтверждена.\"\n");
    fprintf(f, "        fi\n");
    fprintf(f, "      fi\n");
    fprintf(f, "    fi\n");
    fprintf(f, "  fi\n");
    fprintf(f, "  if [ $DOWNLOADED -eq 0 ]; then\n");
    fprintf(f, "    if [ \"$OFFLINE_MODE\" = \"True\" ]; then\n");
    fprintf(f, "      echo \"$LOG_PREFIX [FAIL] Офлайн-режим: архив $ARCHIVE отсутствует на сервере\" >&2\n");
    fprintf(f, "      exit 1\n");
    fprintf(f, "    fi\n");
    fprintf(f, "    echo \"$LOG_PREFIX Загрузка с сайта julialang.org...\"\n");
    fprintf(f, "    curl -fL \"$URL\" -o \"$TMP_ARCHIVE\"\n");
    fprintf(f, "  fi\n");
    fprintf(f, "  echo \"$LOG_PREFIX Распаковка архива Julia...\"\n");
    fprintf(f, "  tar -xzf \"$TMP_ARCHIVE\" -C \"$TARGET_DIR\" --strip-components=1\n");
    fprintf(f, "  rm -f \"$TMP_ARCHIVE\"\n");
    fprintf(f, "  ln -sfn \"$(pwd)/$TARGET_DIR/bin/julia\" tools/bin/julia\n");
    fprintf(f, "  tools/bin/julia --version\n");
    fprintf(f, "fi\n\n");

    // 2. Jupyter Runtime
    fprintf(f, "# -------------------------------------------------------------------------\n");
    fprintf(f, "# 2. Jupyter Runtime\n");
    fprintf(f, "# -------------------------------------------------------------------------\n");
    fprintf(f, "if [ -x \"tools/bin/jupyter\" ] && [ -x \"tools/bin/jupyter-lab\" ]; then\n");
    fprintf(f, "  echo \"$LOG_PREFIX Jupyter already installed\"\n");
    fprintf(f, "  tools/bin/jupyter --version\n");
    fprintf(f, "else\n");
    fprintf(f, "  echo \"$LOG_PREFIX install Jupyter runtime\"\n");
    fprintf(f, "  mkdir -p tools/jupyter tools/bin share/jupyter/kernels\n");
    fprintf(f, "  rm -rf tools/jupyter/runtime\n");
    fprintf(f, "  python3 -m venv tools/jupyter/runtime\n");
    fprintf(f, "  tools/jupyter/runtime/bin/pip install $PIP_ARGS --upgrade pip || true\n");
    fprintf(f, "  tools/jupyter/runtime/bin/pip install $PIP_ARGS jupyterlab notebook ipykernel bash_kernel || true\n");
    fprintf(f, "  tools/jupyter/runtime/bin/pip install $PIP_ARGS octave_kernel scilab_kernel || true\n");
    fprintf(f, "  ln -sfn \"$(pwd)/tools/jupyter/runtime/bin/jupyter\" tools/bin/jupyter\n");
    fprintf(f, "  ln -sfn \"$(pwd)/tools/jupyter/runtime/bin/jupyter-lab\" tools/bin/jupyter-lab\n");
    fprintf(f, "  tools/bin/jupyter --version\n");
    fprintf(f, "fi\n\n");

    // 3. Profile Python Environment
    fprintf(f, "# -------------------------------------------------------------------------\n");
    fprintf(f, "# 3. Profile Python Environment\n");
    fprintf(f, "# -------------------------------------------------------------------------\n");
    fprintf(f, "echo \"$LOG_PREFIX install profile $PROFILE\"\n");
    fprintf(f, "mkdir -p envs\n");
    fprintf(f, "rm -rf \"envs/$PROFILE\"\n");
    fprintf(f, "python3 -m venv \"envs/$PROFILE\"\n");
    fprintf(f, "\"envs/$PROFILE/bin/pip\" install $PIP_ARGS --upgrade pip || true\n");
    if (config.python.count > 0)
    {
        fprintf(f, "\"envs/$PROFILE/bin/pip\" install $PIP_ARGS");
        for (size_t i = 0; i < config.python.count; i++)
        {
            fprintf(f, " %s", config.python.items[i]);
        }
        fprintf(f, "\n");
    }
    fprintf(f, "\n");

    // 4. System dependencies
    fprintf(f, "# -------------------------------------------------------------------------\n");
    fprintf(f, "# 4. System dependencies\n");
    fprintf(f, "# -------------------------------------------------------------------------\n");
    for (size_t i = 0; i < config.system.count; i++)
    {
        const char *item = config.system.items[i];
        if (strcmp(item, "octave") == 0)
        {
            fprintf(f, "if [ -x \"tools/bin/octave\" ] || command -v octave >/dev/null 2>&1; then\n");
            fprintf(f, "  echo \"[INFO] octave already installed: $(tools/bin/octave --version 2>/dev/null || octave --version | head -n 1)\"\n");
            fprintf(f, "else\n");
            fprintf(f, "  OCT_FILE=$(if [ -n \"$SERVER_URL\" ]; then curl -s \"$SERVER_URL/api/v1/artifacts\" | python3 -c \"import sys, json; items = json.load(sys.stdin); print(next((x['filename'] for x in items if x.get('category') == 'octave' and x.get('size_bytes', 0) > 100*1024), ''))\" 2>/dev/null || echo \"\"; fi)\n");
            fprintf(f, "  if [ -n \"$OCT_FILE\" ] && curl -s -f -m 300 \"$SERVER_URL/artifacts/octave/$OCT_FILE\" -o \"/tmp/$OCT_FILE\"; then\n");
            fprintf(f, "    mkdir -p tools/octave tools/bin\n");
            fprintf(f, "    case \"$OCT_FILE\" in\n");
            fprintf(f, "      *.deb) dpkg-deb -x \"/tmp/$OCT_FILE\" tools/octave 2>/dev/null || true ;\n");
            fprintf(f, "             [ -f tools/octave/usr/bin/octave ] && ln -sfn \"$(pwd)/tools/octave/usr/bin/octave\" tools/bin/octave ;\n");
            fprintf(f, "             [ -f tools/octave/usr/bin/octave-cli ] && ln -sfn \"$(pwd)/tools/octave/usr/bin/octave-cli\" tools/bin/octave-cli ;;\n");
            fprintf(f, "      *.AppImage) mv \"/tmp/$OCT_FILE\" tools/octave/octave.AppImage ; chmod +x tools/octave/octave.AppImage ;\n");
            fprintf(f, "                  ln -sfn \"$(pwd)/tools/octave/octave.AppImage\" tools/bin/octave ;\n");
            fprintf(f, "                  ln -sfn \"$(pwd)/tools/octave/octave.AppImage\" tools/bin/octave-cli ;;\n");
            fprintf(f, "      *) tar -xzf \"/tmp/$OCT_FILE\" -C tools/octave --strip-components=1 2>/dev/null || true ;\n");
            fprintf(f, "         [ -f tools/octave/bin/octave ] && ln -sfn \"$(pwd)/tools/octave/bin/octave\" tools/bin/octave ;\n");
            fprintf(f, "         [ -f tools/octave/bin/octave-cli ] && ln -sfn \"$(pwd)/tools/octave/bin/octave-cli\" tools/bin/octave-cli ;;\n");
            fprintf(f, "    esac\n");
            fprintf(f, "    rm -f \"/tmp/$OCT_FILE\"\n");
            fprintf(f, "    echo \"[OK] Octave установлен из локального репозитория ($OCT_FILE)\"\n");
            fprintf(f, "  elif [ \"$OFFLINE_MODE\" != \"True\" ] && command -v apt-get >/dev/null 2>&1; then\n");
            fprintf(f, "    sudo apt-get install -y octave\n");
            fprintf(f, "  else\n");
            fprintf(f, "    echo \"[WARN] Офлайн-режим: Octave отсутствует в локальном репозитории\" >&2\n");
            fprintf(f, "  fi\n");
            fprintf(f, "fi\n\n");
        }
        else if (strcmp(item, "scilab") == 0)
        {
            fprintf(f, "if [ -x \"tools/bin/scilab-cli\" ] || [ -x \"tools/bin/scilab\" ] || command -v scilab-adv-cli >/dev/null 2>&1 || command -v scilab-cli >/dev/null 2>&1 || command -v scilab >/dev/null 2>&1; then\n");
            fprintf(f, "  echo \"[INFO] scilab already installed\"\n");
            fprintf(f, "else\n");
            fprintf(f, "  SCI_FILES=$(if [ -n \"$SERVER_URL\" ]; then curl -s \"$SERVER_URL/api/v1/artifacts\" | python3 -c \"import sys, json; items = json.load(sys.stdin); sci = [x['filename'] for x in items if x.get('category') == 'scilab' and x.get('size_bytes', 0) > 100*1024]; tars = [f for f in sci if f.endswith('.tar.gz') or f.endswith('.tgz')]; print(' '.join(tars if tars else sci))\" 2>/dev/null || echo \"\"; fi)\n");
            fprintf(f, "  if [ -n \"$SCI_FILES\" ]; then\n");
            fprintf(f, "    mkdir -p tools/scilab tools/bin\n");
            fprintf(f, "    for F in $SCI_FILES; do\n");
            fprintf(f, "      curl -s -f -m 300 \"$SERVER_URL/artifacts/scilab/$F\" -o \"/tmp/$F\"\n");
            fprintf(f, "      case \"$F\" in\n");
            fprintf(f, "        *.deb) dpkg-deb -x \"/tmp/$F\" tools/scilab 2>/dev/null || true ;;\n");
            fprintf(f, "        *) tar -xzf \"/tmp/$F\" -C tools/scilab --strip-components=1 2>/dev/null || true ;;\n");
            fprintf(f, "      esac\n");
            fprintf(f, "      rm -f \"/tmp/$F\"\n");
            fprintf(f, "    done\n");
            fprintf(f, "    [ -x tools/scilab/bin/scilab-cli ] && ln -sfn \"$(pwd)/tools/scilab/bin/scilab-cli\" tools/bin/scilab-cli\n");
            fprintf(f, "    [ -x tools/scilab/usr/bin/scilab-cli ] && ln -sfn \"$(pwd)/tools/scilab/usr/bin/scilab-cli\" tools/bin/scilab-cli\n");
            fprintf(f, "    [ -x tools/scilab/bin/scilab ] && ln -sfn \"$(pwd)/tools/scilab/bin/scilab\" tools/bin/scilab\n");
            fprintf(f, "    echo \"[OK] Scilab установлен из локального репозитория\"\n");
            fprintf(f, "  elif [ \"$OFFLINE_MODE\" != \"True\" ] && command -v apt-get >/dev/null 2>&1; then\n");
            fprintf(f, "    sudo apt-get install -y scilab\n");
            fprintf(f, "  else\n");
            fprintf(f, "    echo \"[WARN] Офлайн-режим: Scilab отсутствует в локальном репозитории\" >&2\n");
            fprintf(f, "  fi\n");
            fprintf(f, "fi\n\n");
        }
        else
        {
            fprintf(f, "if dpkg -s \"%s\" >/dev/null 2>&1; then\n", item);
            fprintf(f, "  echo \"[INFO] package already installed: %s\"\n", item);
            fprintf(f, "elif [ \"$OFFLINE_MODE\" != \"True\" ] && command -v apt-get >/dev/null 2>&1; then\n");
            fprintf(f, "  sudo apt-get install -y \"%s\"\n", item);
            fprintf(f, "else\n");
            fprintf(f, "  echo \"[WARN] Офлайн-режим: пакет %s пропущен\"\n", item);
            fprintf(f, "fi\n\n");
        }
    }

    // 5. Julia packages
    fprintf(f, "# -------------------------------------------------------------------------\n");
    fprintf(f, "# 5. Julia packages\n");
    fprintf(f, "# -------------------------------------------------------------------------\n");
    for (size_t i = 0; i < config.julia.count; i++)
    {
        fprintf(f, "tools/bin/julia -e 'using Pkg; try Pkg.add(\"%s\") catch; @warn \"Offline mode: Pkg.add skipped\" end'\n", config.julia.items[i]);
    }
    fprintf(f, "\n");

    // 6. Kernels registration
    fprintf(f, "# -------------------------------------------------------------------------\n");
    fprintf(f, "# 6. Kernels registration\n");
    fprintf(f, "# -------------------------------------------------------------------------\n");
    if (list_contains(&config.kernels, "python"))
    {
        fprintf(f, "\"envs/$PROFILE/bin/python\" -m ipykernel install --user --name \"sysopenmath-$PROFILE\" --display-name \"SysOpenMath Python ($PROFILE)\"\n");
    }
    if (list_contains(&config.kernels, "julia"))
    {
        fprintf(f, "tools/bin/julia -e 'using Pkg; try Pkg.add(\"IJulia\"); using IJulia; installkernel(\"SysOpenMath Julia\") catch; @warn \"IJulia skipped in offline mode\" end'\n");
    }
    if (list_contains(&config.kernels, "bash"))
    {
        fprintf(f, "tools/jupyter/runtime/bin/python -m bash_kernel.install --user || true\n");
    }
    if (list_contains(&config.kernels, "octave"))
    {
        fprintf(f, "if command -v octave >/dev/null 2>&1 || [ -x \"tools/bin/octave\" ]; then\n");
        fprintf(f, "  tools/jupyter/runtime/bin/python -m octave_kernel install --user || true\n");
        fprintf(f, "else\n");
        fprintf(f, "  echo \"[WARN] octave command not found, octave kernel skipped\"\n");
        fprintf(f, "fi\n");
    }
    if (list_contains(&config.kernels, "scilab"))
    {
        fprintf(f, "if [ -x \"tools/bin/scilab-cli\" ]; then export SCILAB_EXECUTABLE=\"$(pwd)/tools/bin/scilab-cli\";\n");
        fprintf(f, "elif [ -x \"tools/bin/scilab\" ]; then export SCILAB_EXECUTABLE=\"$(pwd)/tools/bin/scilab\";\n");
        fprintf(f, "elif command -v scilab-adv-cli >/dev/null 2>&1; then export SCILAB_EXECUTABLE=\"$(command -v scilab-adv-cli)\";\n");
        fprintf(f, "elif command -v scilab-cli >/dev/null 2>&1; then export SCILAB_EXECUTABLE=\"$(command -v scilab-cli)\";\n");
        fprintf(f, "elif command -v scilab >/dev/null 2>&1; then export SCILAB_EXECUTABLE=\"$(command -v scilab)\"; fi\n");
        fprintf(f, "if [ -n \"${SCILAB_EXECUTABLE:-}\" ]; then\n");
        fprintf(f, "  tools/jupyter/runtime/bin/python -m scilab_kernel install --user || true\n");
        fprintf(f, "else\n");
        fprintf(f, "  echo \"[WARN] scilab command not found, scilab kernel skipped\"\n");
        fprintf(f, "fi\n");
    }
    fprintf(f, "\n");

    // 7. Workspace
    fprintf(f, "# -------------------------------------------------------------------------\n");
    fprintf(f, "# 7. Workspace\n");
    fprintf(f, "# -------------------------------------------------------------------------\n");
    fprintf(f, "echo \"$LOG_PREFIX create workspace\"\n");
    fprintf(f, "WS_DIR=\"workspaces/$PROFILE\"\n");
    fprintf(f, "mkdir -p \"$WS_DIR\"\n");
    fprintf(f, "cat > \"$WS_DIR/README.md\" << 'EOF'\n");
    fprintf(f, "# SysOpenMath Workspace: %s\n\n", profile->name);
    fprintf(f, "Готовый workspace для профиля %s.\n\n", profile->name);
    fprintf(f, "## Что дальше\n");
    fprintf(f, "1. Запусти JupyterLab\n");
    fprintf(f, "2. Открой notebook %s_starter.ipynb\n", profile->name);
    fprintf(f, "3. Проверь kernels и состояние системы\n");
    fprintf(f, "EOF\n");
    fprintf(f, "if [ ! -f \"$WS_DIR/%s_starter.ipynb\" ]; then\n", profile->name);
    fprintf(f, "cat > \"$WS_DIR/%s_starter.ipynb\" << 'EOF'\n", profile->name);
    fprintf(f, "{\n");
    fprintf(f, "  \"cells\": [],\n");
    fprintf(f, "  \"metadata\": {\n");
    fprintf(f, "    \"kernelspec\": {\n");
    fprintf(f, "      \"display_name\": \"SysOpenMath Python (%s)\",\n", profile->name);
    fprintf(f, "      \"language\": \"python\",\n");
    fprintf(f, "      \"name\": \"sysopenmath-%s\"\n", profile->name);
    fprintf(f, "    },\n");
    fprintf(f, "    \"language_info\": {\n");
    fprintf(f, "      \"name\": \"python\"\n");
    fprintf(f, "    }\n");
    fprintf(f, "  },\n");
    fprintf(f, "  \"nbformat\": 4,\n");
    fprintf(f, "  \"nbformat_minor\": 5\n");
    fprintf(f, "}\n");
    fprintf(f, "EOF\n");
    fprintf(f, "fi\n\n");

    // 8. State update
    fprintf(f, "# -------------------------------------------------------------------------\n");
    fprintf(f, "# 8. State update\n");
    fprintf(f, "# -------------------------------------------------------------------------\n");
    fprintf(f, "echo \"$LOG_PREFIX update state\"\n");
    fprintf(f, "mkdir -p state/profiles\n");
    fprintf(f, "tools/bin/julia --version > state/julia.version 2>/dev/null || true\n");
    fprintf(f, "tools/bin/jupyter --version > state/jupyter.version 2>/dev/null || true\n");
    fprintf(f, "printf 'profile=%%s\\npython_env=envs/%%s\\n' \"$PROFILE\" \"$PROFILE\" > \"state/profiles/$PROFILE.state\"\n");
    fprintf(f, "echo \"[INFO] full setup completed for profile: $PROFILE\"\n");

    fclose(f);
    chmod(script_path, 0755);

    snprintf(out_cmd, out_cmd_size, "bash \"%s\"", script_path);

    profile_config_free(&config);
    return 1;
}
