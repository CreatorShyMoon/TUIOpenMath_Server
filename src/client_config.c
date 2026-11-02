#include "client_config.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void client_config_init_defaults(ClientConfig *cfg)
{
    if (!cfg)
    {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->server_url, sizeof(cfg->server_url), "%s", DEFAULT_SERVER_URL);
    cfg->offline_mode = 0;
    cfg->verify_checksum = 1;
    cfg->timeout_sec = DEFAULT_TIMEOUT_SEC;
}

int client_config_save(const char *path, const ClientConfig *cfg)
{
    if (!path || !cfg)
    {
        return 0;
    }

    mkdir("config", 0755);

    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        return 0;
    }

    cJSON_AddStringToObject(root, "server_url", cfg->server_url);
    cJSON_AddBoolToObject(root, "offline_mode", cfg->offline_mode);
    cJSON_AddBoolToObject(root, "verify_checksum", cfg->verify_checksum);
    cJSON_AddNumberToObject(root, "timeout_sec", cfg->timeout_sec);

    char *rendered = cJSON_Print(root);
    cJSON_Delete(root);

    if (!rendered)
    {
        return 0;
    }

    FILE *f = fopen(path, "w");
    if (!f)
    {
        free(rendered);
        return 0;
    }

    fputs(rendered, f);
    fputc('\n', f);
    fclose(f);
    free(rendered);

    return 1;
}

int client_config_load(const char *path, ClientConfig *cfg)
{
    if (!path || !cfg)
    {
        return 0;
    }

    client_config_init_defaults(cfg);

    FILE *f = fopen(path, "r");
    if (!f)
    {
        /* Конфиг ещё не создан — создаём файл с параметрами по умолчанию */
        return client_config_save(path, cfg);
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0 || sz > 65536)
    {
        fclose(f);
        return 0;
    }

    char *content = malloc(sz + 1);
    if (!content)
    {
        fclose(f);
        return 0;
    }

    size_t read_bytes = fread(content, 1, sz, f);
    fclose(f);
    content[read_bytes] = '\0';

    cJSON *root = cJSON_Parse(content);
    free(content);

    if (!root)
    {
        return 0;
    }

    cJSON *j_url = cJSON_GetObjectItemCaseSensitive(root, "server_url");
    cJSON *j_offline = cJSON_GetObjectItemCaseSensitive(root, "offline_mode");
    cJSON *j_verify = cJSON_GetObjectItemCaseSensitive(root, "verify_checksum");
    cJSON *j_timeout = cJSON_GetObjectItemCaseSensitive(root, "timeout_sec");

    if (cJSON_IsString(j_url) && j_url->valuestring && j_url->valuestring[0] != '\0')
    {
        snprintf(cfg->server_url, sizeof(cfg->server_url), "%s", j_url->valuestring);
        size_t len = strlen(cfg->server_url);
        while (len > 0 && cfg->server_url[len - 1] == '/')
        {
            cfg->server_url[--len] = '\0';
        }
    }

    if (cJSON_IsBool(j_offline))
    {
        cfg->offline_mode = cJSON_IsTrue(j_offline) ? 1 : 0;
    }

    if (cJSON_IsBool(j_verify))
    {
        cfg->verify_checksum = cJSON_IsTrue(j_verify) ? 1 : 0;
    }

    if (cJSON_IsNumber(j_timeout) && j_timeout->valueint > 0)
    {
        cfg->timeout_sec = j_timeout->valueint;
    }

    cJSON_Delete(root);
    return 1;
}

int client_config_get_host(const ClientConfig *cfg, char *host_buf, size_t host_buf_size)
{
    if (!cfg || !host_buf || host_buf_size == 0)
    {
        return 0;
    }

    host_buf[0] = '\0';

    const char *p = cfg->server_url;
    if (strncmp(p, "http://", 7) == 0)
    {
        p += 7;
    }
    else if (strncmp(p, "https://", 8) == 0)
    {
        p += 8;
    }

    size_t i = 0;
    while (*p && *p != ':' && *p != '/' && i + 1 < host_buf_size)
    {
        host_buf[i++] = *p++;
    }
    host_buf[i] = '\0';

    return i > 0;
}

int client_config_build_pip_args(const ClientConfig *cfg, char *args_buf, size_t args_buf_size)
{
    if (!args_buf || args_buf_size == 0)
    {
        return 0;
    }

    args_buf[0] = '\0';

    if (!cfg || cfg->server_url[0] == '\0')
    {
        return 1;
    }

    char host[128];
    if (!client_config_get_host(cfg, host, sizeof(host)))
    {
        return 1;
    }

    if (cfg->offline_mode)
    {
        snprintf(
            args_buf,
            args_buf_size,
            " --index-url %s/simple/ --trusted-host %s",
            cfg->server_url,
            host
        );
    }
    else
    {
        snprintf(
            args_buf,
            args_buf_size,
            " --extra-index-url %s/simple/ --trusted-host %s",
            cfg->server_url,
            host
        );
    }

    return 1;
}

