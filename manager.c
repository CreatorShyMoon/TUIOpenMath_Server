#define _POSIX_C_SOURCE 200809L

#include "actions.h"
#include "app.h"
#include "logger.h"
#include "process.h"
#include "profiles.h"
#include "ui.h"

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static void ensure_logs_dir(void)
{
    mkdir("logs", 0755);
}

static void ensure_jupyter_env(void)
{
    mkdir("share", 0755);
    mkdir("share/jupyter", 0755);
    mkdir("share/jupyter/kernels", 0755);
    mkdir("tools", 0755);
    mkdir("tools/bin", 0755);

    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)))
    {
        char jdata[2048];
        snprintf(jdata, sizeof(jdata), "%s/share/jupyter", cwd);
        setenv("JUPYTER_DATA_DIR", jdata, 1);

        char path_env[4096];
        const char *old_path = getenv("PATH");
        snprintf(path_env, sizeof(path_env), "%s/tools/bin:%s", cwd, old_path ? old_path : "/usr/bin:/bin");
        setenv("PATH", path_env, 1);
    }
}

int main(void)
{
    setlocale(LC_ALL, "");

    ensure_logs_dir();
    ensure_jupyter_env();
    mkdir("profiles", 0755);
    mkdir("config", 0755);

    AppState app;
    memset(&app, 0, sizeof(app));

    client_config_load("config/client.json", &app.client_cfg);

    app.pipe_fd[0] = -1;
    app.pipe_fd[1] = -1;
    app.active_section = SECTION_PROFILES;
    app.selected_profile = 0;
    app.selected_action = ACTION_INSTALL;

    ui_init(&app);

    if (!logger_init(&app, "logs/sysopenmath.log"))
    {
        endwin();
        return 1;
    }

    log_info(&app, "SysOpenMath запущен.");
    log_info(
        &app,
        "Репозиторий: %s (режим: %s)",
        app.client_cfg.server_url,
        app.client_cfg.offline_mode ? "ОФЛАЙН" : "ОНЛАЙН"
    );
    load_profiles(&app, "profiles");

    while (1)
    {
        check_process(&app);
        read_process_output(&app);
        ui_draw(&app);

        int ch = getch();
        if (ch == ERR)
        {
            continue;
        }

        if (ch == KEY_RESIZE)
        {
            int drained;
            while ((drained = getch()) == KEY_RESIZE)
            {
                /* drop queued duplicate resize events */
            }
            if (drained != ERR)
            {
                ungetch(drained);
            }

            struct winsize ws;
            int new_rows = 0, new_cols = 0;
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0)
            {
                new_rows = ws.ws_row;
                new_cols = ws.ws_col;
            }
            else
            {
                new_rows = LINES;
                new_cols = COLS;
            }

            if (new_rows != app.term_h || new_cols != app.term_w)
            {
                ui_recreate(&app);
            }
            continue;
        }

        ui_handle_input(&app, ch);
    }

    logger_close(&app);
    ui_destroy(&app);
    return 0;
}