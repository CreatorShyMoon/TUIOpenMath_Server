#include "profiles.h"
#include "logger.h"
#include "profile_config.h"
#include "profile_registry.h"

#include <cjson/cJSON.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int profile_name_is_safe(const char *name)
{
    return profile_config_name_is_valid(name);
}

int profile_is_user_editable(const Profile *profile)
{
    (void)profile;
    return 0;
}

static void clear_profiles(AppState *app)
{
    app->profile_count = 0;
    memset(app->profiles, 0, sizeof(app->profiles));
}

static int has_profile(const AppState *app, const char *name)
{
    if (!app || !name)
    {
        return 0;
    }

    for (int i = 0; i < app->profile_count; i++)
    {
        if (strcmp(app->profiles[i].name, name) == 0)
        {
            return 1;
        }
    }

    return 0;
}

static int load_profile_from_file(AppState *app, const char *filepath)
{
    if (!app || !filepath)
    {
        return 0;
    }

    if (app->profile_count >= MAX_PROFILES)
    {
        log_warn(app, "Достигнут лимит профилей: %d", MAX_PROFILES);
        return 0;
    }

    FILE *f = fopen(filepath, "r");
    if (!f)
    {
        return 0;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0 || sz > 1048576)
    {
        fclose(f);
        return 0;
    }

    char *buf = malloc(sz + 1);
    if (!buf)
    {
        fclose(f);
        return 0;
    }

    size_t n = fread(buf, 1, sz, f);
    fclose(f);
    buf[n] = '\0';

    cJSON *json = cJSON_Parse(buf);
    free(buf);

    if (!json)
    {
        return 0;
    }

    cJSON *name = cJSON_GetObjectItemCaseSensitive(json, "name");
    cJSON *desc = cJSON_GetObjectItemCaseSensitive(json, "description");
    cJSON *locked = cJSON_GetObjectItemCaseSensitive(json, "locked");

    if (!cJSON_IsString(name) || !name->valuestring || !profile_config_name_is_valid(name->valuestring))
    {
        cJSON_Delete(json);
        return 0;
    }

    if (has_profile(app, name->valuestring))
    {
        /* Профиль с таким именем уже загружен */
        cJSON_Delete(json);
        return 0;
    }

    Profile profile;
    memset(&profile, 0, sizeof(profile));

    snprintf(profile.name, sizeof(profile.name), "%s", name->valuestring);
    if (cJSON_IsString(desc) && desc->valuestring)
    {
        snprintf(profile.description, sizeof(profile.description), "%s", desc->valuestring);
    }
    snprintf(profile.path, sizeof(profile.path), "%s", filepath);

    profile.is_system_profile = 0;
    profile.is_locked = (cJSON_IsBool(locked) && cJSON_IsTrue(locked)) ? 1 : 0;

    cJSON_Delete(json);

    app->profiles[app->profile_count++] = profile;
    return 1;
}

static void load_profiles_from_dir(AppState *app, const char *dir_path)
{
    if (!app || !dir_path)
    {
        return;
    }

    DIR *d = opendir(dir_path);
    if (!d)
    {
        return;
    }

    struct dirent *dir;
    while ((dir = readdir(d)) != NULL)
    {
        if (dir->d_name[0] == '.')
        {
            continue;
        }

        size_t len = strlen(dir->d_name);
        if (len > 5 && strcmp(dir->d_name + len - 5, ".json") == 0)
        {
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, dir->d_name);
            load_profile_from_file(app, full_path);
        }
    }

    closedir(d);
}

int load_profiles(AppState *app, const char *dir_path)
{
    if (!app)
    {
        return 0;
    }

    clear_profiles(app);

    /* 1. Сначала загружаем динамические профили из каталогов (profiles/, state/remote_profiles/, server/storage/profiles/) */
    if (dir_path && dir_path[0] != '\0')
    {
        load_profiles_from_dir(app, dir_path);
    }
    load_profiles_from_dir(app, "state/remote_profiles");
    load_profiles_from_dir(app, "server/storage/profiles");

    /* 2. Дополняем встроенными штатным профилями (если ещё не переопределены файлами) */
    for (size_t i = 0; i < BUILTIN_PROFILES_COUNT; i++)
    {
        if (app->profile_count >= MAX_PROFILES)
        {
            log_warn(app, "Достигнут лимит профилей: %d", MAX_PROFILES);
            break;
        }

        const BuiltinProfile *bp = &BUILTIN_PROFILES[i];

        if (!profile_config_name_is_valid(bp->name))
        {
            log_warn(app, "Профиль пропущен: небезопасное имя '%s'", bp->name);
            continue;
        }

        if (has_profile(app, bp->name))
        {
            /* Профиль уже загружен из внешнего JSON-файла */
            continue;
        }

        Profile profile;
        memset(&profile, 0, sizeof(profile));

        snprintf(profile.name,        sizeof(profile.name),        "%s", bp->name);
        snprintf(profile.description, sizeof(profile.description), "%s", bp->description);
        snprintf(profile.path,        sizeof(profile.path),        "(builtin)");

        profile.is_system_profile = 1;
        profile.is_locked         = bp->locked;

        app->profiles[app->profile_count++] = profile;
    }

    if (app->profile_count == 0)
    {
        log_error(app, "Профили не найдены.");
        return 0;
    }

    app->selected_profile = 0;
    return 1;
}

const Profile *get_selected_profile(const AppState *app)
{
    if (!app || app->profile_count <= 0)
    {
        return NULL;
    }

    if (app->selected_profile < 0 || app->selected_profile >= app->profile_count)
    {
        return NULL;
    }

    return &app->profiles[app->selected_profile];
}

int profiles_unpack_remote_list(AppState *app, const char *list_json_path)
{
    if (!list_json_path)
    {
        return 0;
    }

    FILE *f = fopen(list_json_path, "r");
    if (!f)
    {
        if (app)
        {
            log_warn(app, "Файл со списком профилей не найден: %s", list_json_path);
        }
        return 0;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0 || sz > 2097152)
    {
        fclose(f);
        return 0;
    }

    char *buf = malloc(sz + 1);
    if (!buf)
    {
        fclose(f);
        return 0;
    }

    size_t n = fread(buf, 1, sz, f);
    fclose(f);
    buf[n] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root)
    {
        if (app)
        {
            log_error(app, "Ошибка парсинга JSON из %s", list_json_path);
        }
        return 0;
    }

    cJSON *items = root;
    if (cJSON_IsObject(root))
    {
        cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "profiles");
        if (arr && cJSON_IsArray(arr))
        {
            items = arr;
        }
    }

    if (!cJSON_IsArray(items))
    {
        cJSON_Delete(root);
        return 0;
    }

    mkdir("state", 0755);
    mkdir("state/remote_profiles", 0755);

    int count = 0;
    int size = cJSON_GetArraySize(items);
    for (int i = 0; i < size; i++)
    {
        cJSON *item = cJSON_GetArrayItem(items, i);
        if (!item || !cJSON_IsObject(item))
        {
            continue;
        }

        cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
        if (!name || !cJSON_IsString(name) || !name->valuestring || !profile_config_name_is_valid(name->valuestring))
        {
            continue;
        }

        char out_path[512];
        snprintf(out_path, sizeof(out_path), "state/remote_profiles/%s.json", name->valuestring);

        char *printed = cJSON_Print(item);
        if (!printed)
        {
            continue;
        }

        FILE *out = fopen(out_path, "w");
        if (out)
        {
            fputs(printed, out);
            fputc('\n', out);
            fclose(out);
            count++;
        }
        free(printed);
    }

    cJSON_Delete(root);
    if (app)
    {
        log_info(app, "Сохранено профилей с сервера: %d", count);
    }
    return count;
}
