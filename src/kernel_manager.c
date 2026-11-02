#include "kernel_manager.h"
#include "julia_manager.h"
#include "profiles.h"

#include <stdio.h>

int kernel_build_register_python_command(
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
        "export JUPYTER_DATA_DIR=\"$(pwd)/share/jupyter\"; "
        "mkdir -p share/jupyter/kernels; "
        "PYBIN=\"envs/%s/bin/python\"; "
        "if [ ! -x \"$PYBIN\" ]; then "
        "  echo \"[ERROR] Python env для профиля %s не найден\"; "
        "  exit 1; "
        "fi; "
        "if ! [ -x \"tools/bin/jupyter\" ]; then "
        "  echo \"[ERROR] Jupyter не установлен\"; "
        "  exit 1; "
        "fi; "
        "\"$PYBIN\" -m ipykernel install --user --name \"sysopenmath-%s\" --display-name \"SysOpenMath Python (%s)\"; "
        "echo \"[INFO] Python kernel registered: sysopenmath-%s\""
        "'",
        profile->name,
        profile->name,
        profile->name,
        profile->name,
        profile->name
    );

    return written >= 0 && (size_t)written < out_cmd_size;
}

int kernel_build_register_julia_command(
    char *out_cmd,
    size_t out_cmd_size
)
{
    if (!out_cmd || out_cmd_size == 0)
    {
        return 0;
    }

    char julia_bin[2048];
    if (!julia_get_binary_path(julia_bin, sizeof(julia_bin)))
    {
        return 0;
    }

    int written = snprintf(
        out_cmd,
        out_cmd_size,
        "bash -lc '"
        "export JUPYTER_DATA_DIR=\"$(pwd)/share/jupyter\"; "
        "mkdir -p share/jupyter/kernels; "
        "if ! [ -x \"%s\" ]; then "
        "  echo \"[ERROR] Julia не установлена\"; "
        "  exit 1; "
        "fi; "
        "if ! [ -x \"tools/bin/jupyter\" ]; then "
        "  echo \"[ERROR] Jupyter не установлен\"; "
        "  exit 1; "
        "fi; "
        "\"%s\" -e \"using Pkg; Pkg.add(\\\"IJulia\\\"); using IJulia; installkernel(\\\"SysOpenMath Julia\\\")\"; "
        "echo \"[INFO] Julia kernel registered: SysOpenMath Julia\""
        "'",
        julia_bin,
        julia_bin
    );

    return written >= 0 && (size_t)written < out_cmd_size;
}

int kernel_build_register_bash_command(
    char *out_cmd,
    size_t out_cmd_size
)
{
    if (!out_cmd || out_cmd_size == 0)
    {
        return 0;
    }

    int written = snprintf(
        out_cmd,
        out_cmd_size,
        "bash -lc '"
        "export JUPYTER_DATA_DIR=\"$(pwd)/share/jupyter\"; "
        "mkdir -p share/jupyter/kernels; "
        "PYBIN=\"tools/jupyter/runtime/bin/python\"; "
        "if [ ! -x \"$PYBIN\" ]; then "
        "  echo \"[ERROR] Jupyter runtime не установлен\"; "
        "  exit 1; "
        "fi; "
        "\"$PYBIN\" -m bash_kernel.install --user; "
        "echo \"[INFO] Bash kernel installed\"; "
        "tools/bin/jupyter kernelspec list"
        "'"
    );

    return written >= 0 && (size_t)written < out_cmd_size;
}

int kernel_build_register_octave_command(
    char *out_cmd,
    size_t out_cmd_size
)
{
    if (!out_cmd || out_cmd_size == 0)
    {
        return 0;
    }

    int written = snprintf(
        out_cmd,
        out_cmd_size,
        "bash -lc '"
        "export JUPYTER_DATA_DIR=\"$(pwd)/share/jupyter\"; "
        "export PATH=\"$(pwd)/tools/bin:$PATH\"; "
        "mkdir -p share/jupyter/kernels; "
        "PYBIN=\"tools/jupyter/runtime/bin/python\"; "
        "if [ ! -x \"$PYBIN\" ]; then "
        "  echo \"[ERROR] Jupyter runtime не установлен\"; "
        "  exit 1; "
        "fi; "
        "if ! command -v octave >/dev/null 2>&1 && [ ! -x \"tools/bin/octave\" ]; then "
        "  echo \"[ERROR] octave не установлен\"; "
        "  exit 1; "
        "fi; "
        "\"$PYBIN\" -m octave_kernel install --user; "
        "echo \"[INFO] Octave kernel installed\"; "
        "tools/bin/jupyter kernelspec list"
        "'"
    );

    return written >= 0 && (size_t)written < out_cmd_size;
}