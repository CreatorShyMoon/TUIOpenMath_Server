#ifndef CLIENT_CONFIG_H
#define CLIENT_CONFIG_H

#include <stddef.h>

#define DEFAULT_SERVER_URL "http://127.0.0.1:8000"
#define DEFAULT_TIMEOUT_SEC 5

typedef struct ClientConfig
{
    char server_url[256];
    int offline_mode;
    int verify_checksum;
    int timeout_sec;
} ClientConfig;

void client_config_init_defaults(ClientConfig *cfg);
int client_config_load(const char *path, ClientConfig *cfg);
int client_config_save(const char *path, const ClientConfig *cfg);
int client_config_get_host(const ClientConfig *cfg, char *host_buf, size_t host_buf_size);
int client_config_build_pip_args(const ClientConfig *cfg, char *args_buf, size_t args_buf_size);

#endif

