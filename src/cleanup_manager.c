
#include "cleanup_manager.h"

#include <stdio.h>

int cleanup_build_remove_julia_command(char *out_cmd, size_t out_cmd_size)
{
    if (!out_cmd || out_cmd_size == 0)
    {
        return 0;
    }

    int written = snprintf(
        out_cmd,
        out_cmd_size,
        "bash -lc '"
        "rm -f \"tools/bin/julia\"; "
        "rm -rf \"tools/julia\"; "
        "rm -f \"state/julia.version\"; "
        "echo \"[INFO] Julia removed\""
        "'"
    );

    return written >= 0 && (size_t)written < out_cmd_size;
}

int cleanup_build_remove_jupyter_command(char *out_cmd, size_t out_cmd_size)
{
    if (!out_cmd || out_cmd_size == 0)
    {
        return 0;
    }

    int written = snprintf(
        out_cmd,
        out_cmd_size,
        "bash -lc '"
        "if [ -f state/jupyter.pid ]; then "
        "  PID=$(cat state/jupyter.pid); "
        "  kill \"$PID\" >/dev/null 2>&1 || true; "
        "  rm -f state/jupyter.pid; "
        "fi; "
        "pkill -f jupyter-lab >/dev/null 2>&1 || true; "
        "rm -f \"tools/bin/jupyter\"; "
        "rm -f \"tools/bin/jupyter-lab\"; "
        "rm -rf \"tools/jupyter\"; "
        "rm -f \"state/jupyter.version\"; "
        "rm -rf \"share/jupyter\"; "
        "echo \"[INFO] Jupyter removed\""
        "'"
    );

    return written >= 0 && (size_t)written < out_cmd_size;
}

int cleanup_build_clear_kernels_command(char *out_cmd, size_t out_cmd_size)
{
    if (!out_cmd || out_cmd_size == 0)
    {
        return 0;
    }

    int written = snprintf(
        out_cmd,
        out_cmd_size,
        "bash -lc '"
        "FOUND=0; "
        "for dir in "
        "  \"share/jupyter/kernels\" "
        "  \"tools/jupyter/runtime/share/jupyter/kernels\"; "
        "do "
        "  if [ -d \"$dir\" ]; then "
        "    FOUND=1; "
        "    rm -rf \"${dir}\"/*; "
        "    echo \"[INFO] Cleared local kernels in: $dir\"; "
        "  fi; "
        "done; "
        "if [ \"$FOUND\" = \"1\" ]; then "
        "  echo \"[INFO] SysOpenMath local kernels cleared\"; "
        "else "
        "  echo \"[WARN] no local kernels directories found\"; "
        "fi"
        "'"
    );

    return written >= 0 && (size_t)written < out_cmd_size;
}

int cleanup_build_clear_state_command(char *out_cmd, size_t out_cmd_size)
{
    if (!out_cmd || out_cmd_size == 0)
    {
        return 0;
    }

    int written = snprintf(
        out_cmd,
        out_cmd_size,
        "bash -lc '"
        "rm -rf \"state\"; "
        "mkdir -p \"state/profiles\"; "
        "echo \"[INFO] State cleared\""
        "'"
    );

    return written >= 0 && (size_t)written < out_cmd_size;
}
