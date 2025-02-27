#include "game/settings.h"

#include "config.h"
#include "filesystem.h"
#include "game/input.h"
#include "game/music.h"
#include "game/screen.h"
#include "game/sound.h"
#include "gfx/context.h"
#include "global/const.h"
#include "global/types.h"
#include "json/json_base.h"
#include "json/json_parse.h"
#include "json/json_write.h"
#include "log.h"
#include "memory.h"
#include "util.h"

#include <stdint.h>
#include <string.h>

static const char *m_UserSettingsPath = "cfg/Tomb1Main_runtime.json5";

static bool Settings_ReadFromJSON(const char *cfg_data);

static bool Settings_ReadFromJSON(const char *cfg_data)
{
    bool result = false;
    struct json_value_s *root = NULL;
    struct json_parse_result_s parse_result;

    LOG_INFO("Loading user settings");

    root = json_parse_ex(
        cfg_data, strlen(cfg_data), json_parse_flags_allow_json5, NULL, NULL,
        &parse_result);
    if (!root) {
        LOG_ERROR(
            "failed to parse config file: %s in line %d, char %d",
            json_get_error_description(parse_result.error),
            parse_result.error_line_no, parse_result.error_row_no, cfg_data);
        // continue to supply the default values
    }

    result = true;

    struct json_object_s *root_obj = json_value_as_object(root);
    g_Config.rendering.enable_bilinear_filter =
        json_object_get_bool(root_obj, "bilinear", true);
    g_Config.rendering.enable_perspective_filter =
        json_object_get_bool(root_obj, "perspective", true);

    g_Config.rendering.enable_vsync =
        json_object_get_bool(root_obj, "vsync", true);
    GFX_Context_SetVSync(g_Config.rendering.enable_vsync);

    {
        int32_t resolution_idx =
            json_object_get_int(root_obj, "hi_res", RESOLUTIONS_SIZE - 1);
        CLAMP(resolution_idx, 0, RESOLUTIONS_SIZE - 1);
        Screen_SetResIdx(resolution_idx);
    }

    g_Config.music_volume = json_object_get_int(root_obj, "music_volume", 8);
    CLAMP(g_Config.music_volume, 0, 10);

    g_Config.sound_volume = json_object_get_int(root_obj, "sound_volume", 8);
    CLAMP(g_Config.sound_volume, 0, 10);

    g_Config.input.layout = json_object_get_int(root_obj, "layout_num", 0);
    CLAMP(g_Config.input.layout, 0, 1);

    g_Config.brightness =
        json_object_get_double(root_obj, "brightness", DEFAULT_BRIGHTNESS);
    CLAMP(g_Config.brightness, MIN_BRIGHTNESS, MAX_BRIGHTNESS);

    g_Config.ui.text_scale =
        json_object_get_double(root_obj, "ui_text_scale", DEFAULT_UI_SCALE);
    CLAMP(g_Config.ui.text_scale, MIN_UI_SCALE, MAX_UI_SCALE);

    g_Config.ui.bar_scale =
        json_object_get_double(root_obj, "ui_bar_scale", DEFAULT_UI_SCALE);
    CLAMP(g_Config.ui.bar_scale, MIN_UI_SCALE, MAX_UI_SCALE);

    struct json_array_s *layout_arr =
        json_object_get_array(root_obj, "layout_sdl");
    for (INPUT_ROLE role = 0; role < INPUT_ROLE_NUMBER_OF; role++) {
        INPUT_SCANCODE scancode =
            Input_GetAssignedScancode(INPUT_LAYOUT_USER, role);
        scancode = json_array_get_int(layout_arr, role, scancode);
        Input_AssignScancode(INPUT_LAYOUT_USER, role, scancode);
    }

    if (root) {
        json_value_free(root);
    }
    return result;
}

bool Settings_Read(void)
{
    bool result = false;

    size_t cfg_data_size = 0;
    char *cfg_data = NULL;
    if (File_Load(m_UserSettingsPath, &cfg_data, &cfg_data_size)) {
        result = Settings_ReadFromJSON(cfg_data);
    } else {
        Settings_ReadFromJSON("");
    }

cleanup:
    Memory_FreePointer(&cfg_data);

    Music_SetVolume(g_Config.music_volume);
    Sound_SetMasterVolume(g_Config.sound_volume);
    return result;
}

bool Settings_Write(void)
{
    LOG_INFO("Saving user settings");

    MYFILE *fp = File_Open(m_UserSettingsPath, FILE_OPEN_WRITE);
    if (!fp) {
        return false;
    }

    size_t size;
    struct json_object_s *root_obj = json_object_new();
    json_object_append_bool(
        root_obj, "bilinear", g_Config.rendering.enable_bilinear_filter);
    json_object_append_bool(
        root_obj, "perspective", g_Config.rendering.enable_perspective_filter);
    json_object_append_bool(root_obj, "vsync", g_Config.rendering.enable_vsync);
    json_object_append_int(root_obj, "hi_res", Screen_GetPendingResIdx());
    json_object_append_int(root_obj, "music_volume", g_Config.music_volume);
    json_object_append_int(root_obj, "sound_volume", g_Config.sound_volume);
    json_object_append_int(root_obj, "layout_num", g_Config.input.layout);
    json_object_append_double(
        root_obj, "ui_text_scale", g_Config.ui.text_scale);
    json_object_append_double(root_obj, "ui_bar_scale", g_Config.ui.bar_scale);
    json_object_append_double(root_obj, "brightness", g_Config.brightness);

    struct json_array_s *layout_arr = json_array_new();
    for (INPUT_ROLE role = 0; role < INPUT_ROLE_NUMBER_OF; role++) {
        json_array_append_int(
            layout_arr, Input_GetAssignedScancode(INPUT_LAYOUT_USER, role));
    }
    json_object_append_array(root_obj, "layout_sdl", layout_arr);

    struct json_value_s *root = json_value_from_object(root_obj);
    char *data = json_write_pretty(root, "  ", "\n", &size);
    json_value_free(root);

    File_Write(data, sizeof(char), size - 1, fp);
    File_Close(fp);
    Memory_FreePointer(&data);

    return true;
}
