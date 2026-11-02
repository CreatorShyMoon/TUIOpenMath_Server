#include "install_steps.h"
#include "profiles.h"
#include "julia_manager.h"
#include "client_config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int mkstemp(char *template);
FILE *fdopen(int fd, const char *mode);

static int ensure_dir_exists(const char *path)
{
    struct stat st;

    if (!path || path[0] == '\0')
    {
        return 0;
    }

    if (stat(path, &st) == 0)
    {
        return S_ISDIR(st.st_mode);
    }

    return mkdir(path, 0755) == 0;
}

static int write_header(FILE *f)
{
    if (!f)
    {
        return 0;
    }

    return fprintf(
        f,
        "#!/usr/bin/env bash\n"
        "set -euo pipefail\n\n"
        "export JUPYTER_DATA_DIR=\"$(pwd)/share/jupyter\"\n"
        "mkdir -p share/jupyter/kernels\n\n"
        "echo \"[INFO] start\"\n"
        "if ! command -v bash >/dev/null 2>&1; then\n"
        "  echo \"[ERROR] bash не найден\" >&2\n"
        "  exit 127\n"
        "fi\n"
        "if ! command -v python3 >/dev/null 2>&1; then\n"
        "  echo \"[ERROR] python3 не найден\" >&2\n"
        "  exit 127\n"
        "fi\n\n"
    ) > 0;
}

static int write_python_section(
    FILE *f,
    const Profile *profile,
    const ProfileConfig *config,
    const ClientConfig *client_cfg,
    int update_mode
)
{
    if (!f || !profile || !config)
    {
        return 0;
    }

    if (fprintf(f, "mkdir -p envs\n") < 0)
    {
        return 0;
    }

    char pip_args[256] = "";
    if (client_cfg)
    {
        client_config_build_pip_args(client_cfg, pip_args, sizeof(pip_args));
    }

    if (update_mode)
    {
        if (fprintf(
                f,
                "if [ ! -d \"envs/%s\" ]; then\n"
                "  echo \"[STEP] create missing python env: %s\"\n"
                "  python3 -m venv \"envs/%s\"\n"
                "fi\n\n",
                profile->name,
                profile->name,
                profile->name
            ) < 0)
        {
            return 0;
        }
    }
    else
    {
        if (fprintf(
                f,
                "echo \"[STEP] create python env: %s\"\n"
                "rm -rf \"envs/%s\"\n"
                "python3 -m venv \"envs/%s\"\n\n",
                profile->name,
                profile->name,
                profile->name
            ) < 0)
        {
            return 0;
        }
    }

    if (config->python.count > 0)
    {
        if (fprintf(
                f,
                "echo \"[STEP] upgrade pip in envs/%s\"\n"
                "\"envs/%s/bin/pip\" install%s --upgrade pip || true\n\n",
                profile->name,
                profile->name,
                pip_args
            ) < 0)
        {
            return 0;
        }

        if (fprintf(
                f,
                "echo \"[STEP] install python packages for %s\"\n"
                "\"envs/%s/bin/pip\" install%s",
                profile->name,
                profile->name,
                pip_args
            ) < 0)
        {
            return 0;
        }

        if (update_mode)
        {
            if (fprintf(f, " --upgrade") < 0)
            {
                return 0;
            }
        }

        for (size_t i = 0; i < config->python.count; i++)
        {
            if (fprintf(f, " %s", config->python.items[i]) < 0)
            {
                return 0;
            }
        }

        if (fprintf(f, "\n\n") < 0)
        {
            return 0;
        }
    }

    return 1;
}

static int write_system_item(FILE *f, const char *item)
{
    if (!f || !item)
    {
        return 0;
    }

    if (strcmp(item, "octave") == 0)
    {
        return fprintf(
            f,
            "if [ -x \"tools/bin/octave\" ] || command -v octave >/dev/null 2>&1; then\n"
            "  echo \"[INFO] octave already installed: $(tools/bin/octave --version 2>/dev/null || octave --version | head -n 1)\"\n"
            "else\n"
            "  echo \"[STEP] install octave\"\n"
            "  OCT_FILE=$(if [ -n \"${SERVER_URL:-}\" ]; then curl -s \"$SERVER_URL/api/v1/artifacts\" | python3 -c \"import sys, json; items = json.load(sys.stdin); print(next((x['filename'] for x in items if x.get('category') == 'octave' and x.get('size_bytes', 0) > 100*1024), ''))\" 2>/dev/null || echo \"\"; fi)\n"
            "  if [ -n \"$OCT_FILE\" ] && curl -s -f -m 300 \"$SERVER_URL/artifacts/octave/$OCT_FILE\" -o \"/tmp/$OCT_FILE\"; then\n"
            "    mkdir -p tools/octave tools/bin\n"
            "    case \"$OCT_FILE\" in\n"
            "      *.deb) dpkg-deb -x \"/tmp/$OCT_FILE\" tools/octave 2>/dev/null || true ;\n"
            "             [ -f tools/octave/usr/bin/octave ] && ln -sfn \"$(pwd)/tools/octave/usr/bin/octave\" tools/bin/octave ;\n"
            "             [ -f tools/octave/usr/bin/octave-cli ] && ln -sfn \"$(pwd)/tools/octave/usr/bin/octave-cli\" tools/bin/octave-cli ;;\n"
            "      *.AppImage) mv \"/tmp/$OCT_FILE\" tools/octave/octave.AppImage ; chmod +x tools/octave/octave.AppImage ;\n"
            "                  ln -sfn \"$(pwd)/tools/octave/octave.AppImage\" tools/bin/octave ;\n"
            "                  ln -sfn \"$(pwd)/tools/octave/octave.AppImage\" tools/bin/octave-cli ;;\n"
            "      *) tar -xzf \"/tmp/$OCT_FILE\" -C tools/octave --strip-components=1 2>/dev/null || true ;\n"
            "         [ -f tools/octave/bin/octave ] && ln -sfn \"$(pwd)/tools/octave/bin/octave\" tools/bin/octave ;\n"
            "         [ -f tools/octave/bin/octave-cli ] && ln -sfn \"$(pwd)/tools/octave/bin/octave-cli\" tools/bin/octave-cli ;;\n"
            "    esac\n"
            "    rm -f \"/tmp/$OCT_FILE\"\n"
            "    echo \"[OK] Octave установлен из локального репозитория ($OCT_FILE)\"\n"
            "  elif [ \"${OFFLINE_MODE:-}\" != \"True\" ] && command -v apt-get >/dev/null 2>&1; then\n"
            "    sudo apt-get install -y octave\n"
            "  else\n"
            "    echo \"[WARN] Офлайн-режим: Octave отсутствует в локальном репозитории\" >&2\n"
            "  fi\n"
            "fi\n\n"
        ) > 0;
    }

    if (strcmp(item, "scilab") == 0)
    {
        return fprintf(
            f,
            "if [ -x \"tools/bin/scilab-cli\" ] || [ -x \"tools/bin/scilab\" ] || command -v scilab-adv-cli >/dev/null 2>&1 || command -v scilab-cli >/dev/null 2>&1 || command -v scilab >/dev/null 2>&1; then\n"
            "  echo \"[INFO] scilab already installed\"\n"
            "else\n"
            "  echo \"[STEP] install scilab\"\n"
            "  SCI_FILES=$(if [ -n \"${SERVER_URL:-}\" ]; then curl -s \"$SERVER_URL/api/v1/artifacts\" | python3 -c \"import sys, json; items = json.load(sys.stdin); sci = [x['filename'] for x in items if x.get('category') == 'scilab' and x.get('size_bytes', 0) > 100*1024]; tars = [f for f in sci if f.endswith('.tar.gz') or f.endswith('.tgz')]; print(' '.join(tars if tars else sci))\" 2>/dev/null || echo \"\"; fi)\n"
            "  if [ -n \"$SCI_FILES\" ]; then\n"
            "    mkdir -p tools/scilab tools/bin\n"
            "    for F in $SCI_FILES; do\n"
            "      curl -s -f -m 300 \"$SERVER_URL/artifacts/scilab/$F\" -o \"/tmp/$F\"\n"
            "      case \"$F\" in\n"
            "        *.deb) dpkg-deb -x \"/tmp/$F\" tools/scilab 2>/dev/null || true ;;\n"
            "        *) tar -xzf \"/tmp/$F\" -C tools/scilab --strip-components=1 2>/dev/null || true ;;\n"
            "      esac\n"
            "      rm -f \"/tmp/$F\"\n"
            "    done\n"
            "    [ -x tools/scilab/bin/scilab-cli ] && ln -sfn \"$(pwd)/tools/scilab/bin/scilab-cli\" tools/bin/scilab-cli\n"
            "    [ -x tools/scilab/usr/bin/scilab-cli ] && ln -sfn \"$(pwd)/tools/scilab/usr/bin/scilab-cli\" tools/bin/scilab-cli\n"
            "    [ -x tools/scilab/bin/scilab ] && ln -sfn \"$(pwd)/tools/scilab/bin/scilab\" tools/bin/scilab\n"
            "    echo \"[OK] Scilab установлен из локального репозитория\"\n"
            "  elif [ \"${OFFLINE_MODE:-}\" != \"True\" ] && command -v apt-get >/dev/null 2>&1; then\n"
            "    sudo apt-get install -y scilab\n"
            "  else\n"
            "    echo \"[WARN] Офлайн-режим: Scilab отсутствует в локальном репозитории\" >&2\n"
            "  fi\n"
            "fi\n\n"
        ) > 0;
    }

    return fprintf(
        f,
        "if dpkg -s \"%s\" >/dev/null 2>&1; then\n"
        "  echo \"[INFO] package already installed: %s\"\n"
        "elif [ \"${OFFLINE_MODE:-}\" != \"True\" ] && command -v apt-get >/dev/null 2>&1; then\n"
        "  echo \"[STEP] install system package: %s\"\n"
        "  sudo apt-get install -y \"%s\"\n"
        "else\n"
        "  echo \"[WARN] Офлайн-режим: пакет %s пропущен\"\n"
        "fi\n\n",
        item, item, item, item, item
    ) > 0;
}

static int write_system_section(FILE *f, const ProfileConfig *config)
{
    if (!f || !config)
    {
        return 0;
    }

    if (config->system.count == 0)
    {
        return 1;
    }

    if (fprintf(
            f,
            "if [ \"${OFFLINE_MODE:-}\" != \"True\" ] && command -v sudo >/dev/null 2>&1 && command -v apt-get >/dev/null 2>&1; then\n"
            "  echo \"[STEP] sync apt package index\"\n"
            "  sudo apt-get update || true\n"
            "fi\n\n"
        ) < 0)
    {
        return 0;
    }

    for (size_t i = 0; i < config->system.count; i++)
    {
        if (!write_system_item(f, config->system.items[i]))
        {
            return 0;
        }
    }

    return 1;
}

static int write_julia_section(FILE *f, const Profile *profile, const ProfileConfig *config, int update_mode)
{
    if (!f || !profile || !config)
    {
        return 0;
    }

    if (config->julia.count == 0)
    {
        return 1;
    }

    char julia_bin[2048];
    if (!julia_get_binary_path(julia_bin, sizeof(julia_bin)))
    {
        return 0;
    }

    if (fprintf(
            f,
            "if ! [ -x \"%s\" ]; then\n"
            "  echo \"[STEP] Julia не обнаружена в tools/bin/julia, выполняется автоматическая загрузка Julia LTS...\"\n"
            "  mkdir -p tools/julia tools/bin\n"
            "  ARCHIVE=\"julia-1.10.10-linux-x86_64.tar.gz\"\n"
            "  TARGET_DIR=\"tools/julia/lts\"\n"
            "  TMP_ARCHIVE=$(mktemp /tmp/sysopenmath_julia_XXXXXX.tar.gz)\n"
            "  rm -rf \"$TARGET_DIR\" && mkdir -p \"$TARGET_DIR\"\n"
            "  SERVER_URL=$(python3 -c \"import json; print(json.load(open('config/client.json')).get('server_url', ''))\" 2>/dev/null || echo \"\")\n"
            "  OFFLINE_MODE=$(python3 -c \"import json; print(json.load(open('config/client.json')).get('offline_mode', False))\" 2>/dev/null || echo \"False\")\n"
            "  DOWNLOADED=0\n"
            "  if [ -n \"$SERVER_URL\" ]; then\n"
            "    echo \"[STEP] Проверка Julia $ARCHIVE на сервере: $SERVER_URL ...\"\n"
            "    if curl -s -f --connect-timeout 5 -m 300 \"$SERVER_URL/artifacts/julia/$ARCHIVE\" -o \"$TMP_ARCHIVE\"; then\n"
            "      echo \"[OK] Архив Julia загружен с локального сервера Open Math\"\n"
            "      DOWNLOADED=1\n"
            "      if curl -s -f -m 5 \"$SERVER_URL/artifacts/julia/$ARCHIVE.sha256\" -o \"$TMP_ARCHIVE.sha256\"; then\n"
            "        EXPECTED_SHA=$(cut -d\" \" -f1 \"$TMP_ARCHIVE.sha256\")\n"
            "        rm -f \"$TMP_ARCHIVE.sha256\"\n"
            "        if [ -n \"$EXPECTED_SHA\" ] && command -v sha256sum >/dev/null 2>&1; then\n"
            "          echo \"[STEP] Верификация SHA-256: $EXPECTED_SHA ...\"\n"
            "          REAL_SHA=$(sha256sum \"$TMP_ARCHIVE\" | cut -d\" \" -f1)\n"
            "          if [ \"$EXPECTED_SHA\" != \"$REAL_SHA\" ]; then\n"
            "            echo \"[ERROR] Не совпала контрольная сумма архива Julia!\" >&2\n"
            "            rm -f \"$TMP_ARCHIVE\"\n"
            "            exit 1\n"
            "          fi\n"
            "          echo \"[OK] Контрольная сумма SHA-256 подтверждена.\"\n"
            "        fi\n"
            "      fi\n"
            "    fi\n"
            "  fi\n"
            "  if [ $DOWNLOADED -eq 0 ]; then\n"
            "    if [ \"$OFFLINE_MODE\" = \"True\" ]; then\n"
            "      echo \"[FAIL] Офлайн-режим: архив $ARCHIVE отсутствует на сервере репозитория\" >&2\n"
            "      exit 1\n"
            "    fi\n"
            "    echo \"[STEP] Загрузка Julia с официального сайта...\"\n"
            "    curl -fL --connect-timeout 10 -m 600 \"https://julialang-s3.julialang.org/bin/linux/x64/1.10/$ARCHIVE\" -o \"$TMP_ARCHIVE\"\n"
            "  fi\n"
            "  if ! tar -tzf \"$TMP_ARCHIVE\" >/dev/null 2>&1; then\n"
            "    echo \"[ERROR] Архив Julia $ARCHIVE повреждён или не полон (битый файл)!\" >&2\n"
            "    echo \"[HINT] На сервере запустите: python3 server/mirror.py julia --force\" >&2\n"
            "    rm -f \"$TMP_ARCHIVE\"\n"
            "    exit 1\n"
            "  fi\n"
            "  echo \"[STEP] Распаковка Julia $ARCHIVE в $TARGET_DIR ...\"\n"
            "  tar -xzf \"$TMP_ARCHIVE\" -C \"$TARGET_DIR\" --strip-components=1\n"
            "  rm -f \"$TMP_ARCHIVE\"\n"
            "  ln -sfn \"$(pwd)/$TARGET_DIR/bin/julia\" tools/bin/julia\n"
            "  echo \"[INFO] Julia LTS успешно установлена: $(tools/bin/julia --version)\"\n"
            "fi\n\n",
            julia_bin
        ) < 0)
    {
        return 0;
    }

    if (fprintf(f, "echo \"[STEP] install julia packages for %s\"\n", profile->name) < 0)
    {
        return 0;
    }

    for (size_t i = 0; i < config->julia.count; i++)
    {
        const char *pkg = config->julia.items[i];

        if (update_mode)
        {
            if (fprintf(
                    f,
                    "\"%s\" -e 'using Pkg; Pkg.add(\"%s\"); Pkg.update(\"%s\")' || echo \"[WARN] Не удалось обновить пакет Julia '%s' (в офлайн-режиме требуется локальное зеркало ~/.julia)\"\n",
                    julia_bin,
                    pkg,
                    pkg,
                    pkg
                ) < 0)
            {
                return 0;
            }
        }
        else
        {
            if (fprintf(
                    f,
                    "\"%s\" -e 'using Pkg; Pkg.add(\"%s\")' || echo \"[WARN] Не удалось загрузить пакет Julia '%s' (в офлайн-режиме требуется локальное зеркало ~/.julia)\"\n",
                    julia_bin,
                    pkg,
                    pkg
                ) < 0)
            {
                return 0;
            }
        }
    }

    if (fprintf(f, "\n") < 0)
    {
        return 0;
    }

    return 1;
}

static int build_temp_script_path(char *buffer, size_t buffer_size, const char *profile_name, int update_mode)
{
    if (!buffer || buffer_size == 0 || !profile_name)
    {
        return 0;
    }

    int written = snprintf(
        buffer,
        buffer_size,
        "/tmp/sysopenmath_%s_%s_XXXXXX",
        profile_name,
        update_mode ? "update" : "install"
    );

    return written >= 0 && (size_t)written < buffer_size;
}

int build_install_command(
    const Profile *profile,
    const ProfileConfig *config,
    const ClientConfig *client_cfg,
    int update_mode,
    char *out_cmd,
    size_t out_cmd_size
)
{
    if (!profile || !config || !out_cmd || out_cmd_size == 0)
    {
        return 0;
    }

    if (!profile_name_is_safe(profile->name))
    {
        return 0;
    }

    if (!ensure_dir_exists("/tmp"))
    {
        return 0;
    }

    char script_template[512];
    if (!build_temp_script_path(script_template, sizeof(script_template), profile->name, update_mode))
    {
        return 0;
    }

    int fd = mkstemp(script_template);
    if (fd == -1)
    {
        return 0;
    }

    FILE *f = fdopen(fd, "wb");
    if (!f)
    {
        close(fd);
        unlink(script_template);
        return 0;
    }

    int ok = write_header(f) &&
             write_python_section(f, profile, config, client_cfg, update_mode) &&
             write_system_section(f, config) &&
             write_julia_section(f, profile, config, update_mode) &&
             fprintf(f, "echo \"[INFO] done\"\n") > 0;

    if (!ok)
    {
        fclose(f);
        unlink(script_template);
        return 0;
    }

    if (fclose(f) != 0)
    {
        unlink(script_template);
        return 0;
    }

    if (chmod(script_template, 0755) != 0)
    {
        unlink(script_template);
        return 0;
    }

    int written = snprintf(out_cmd, out_cmd_size, "bash \"%s\"", script_template);
    if (written < 0 || (size_t)written >= out_cmd_size)
    {
        unlink(script_template);
        return 0;
    }

    return 1;
}

int build_delete_python_env_command(
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

    int written = snprintf(
        out_cmd,
        out_cmd_size,
        "bash -lc '"
        "if [ -d \"envs/%s\" ]; then "
        "  rm -rf \"envs/%s\"; "
        "  echo \"[INFO] deleted envs/%s\"; "
        "else "
        "  echo \"[WARN] envs/%s not found\"; "
        "fi"
        "'",
        profile->name,
        profile->name,
        profile->name,
        profile->name
    );

    return written >= 0 && (size_t)written < out_cmd_size;
}
