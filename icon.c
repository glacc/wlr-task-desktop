/*
    Copyright (C) 2026 Glacc

    This file is part of wlr-task-desktop.

    wlr-task-desktop is free software: you can redistribute it
    and/or modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    wlr-task-desktop is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with wlr-task-desktop. If not,
    see <https://www.gnu.org/licenses/>. 
*/

#define _POSIX_C_SOURCE 200809L

#include "icon.h"

// this code is too messy
// so i'd like to rewrite this one day...

// Freedesktop Icon Lookup Routine
// https://specifications.freedesktop.org/icon-theme/latest/

// .ico extension?

// .svg -> .png -> .xpm -> .ico(?)

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <dirent.h>
#include <assert.h>

#include <fcntl.h>
#include <unistd.h>

#include "vector.h"

#include "strutil.h"

#include <linux/limits.h>

static const char dir_default_append_local_icons[] = "/.local/share/icons";
static const char dir_default_append_doticons[]    = "/.icons";
static const char dir_default_append_icons[]       = "/icons";
static const char dir_default_append_pixmaps[]     = "/pixmaps";

static const char dir_default_append_local_applications[] = "/.local/share/applications";
static const char dir_default_append_applications[]       = "/applications";

static const char desktop_type_prefix[] = "Type=";
static const char desktop_icon_prefix[] = "Icon=";
static const char desktop_startup_wm_class_prefix[] = "StartupWMClass=";

static const char theme_type_prefix[] = "Type=";
static const char theme_size_prefix[] = "Size=";
static const char theme_scale_prefix[] = "Scale=";
static const char theme_context_prefix[] = "Context=";

static const char *icon_extensions[] =
{
    ".svg",
    ".png",
    ".xpm",
    //".ico", // extended .ico support?
};

static struct vector icon_search_dirs_applications = { };
static struct vector icon_search_dirs_icons = { };
static struct vector icon_search_dirs_pixmaps = { };
static bool icon_search_dirs_initialized = false;

// mainly for $XDG_DATA_DIRS
static void icon_split_env_dir(const char *env_dir)
{
    const char *ptr_split_start = env_dir;
    const char *ptr_split_end   = env_dir;

    while (true)
    {
        bool last_split = false;
        
        // find ':' or '\0'
        while (true)
        {
            // can be edited to use multiple splitters
            if (*ptr_split_end == ':')
                break;

            if (*ptr_split_end == '\0')
            {
                last_split = true;
                break;
            }

            ptr_split_end++;
        }

        // trim start
        while (ptr_split_start < ptr_split_end)
        {
            if (*ptr_split_start != ' ')
                break;

            ptr_split_start++;
        }

        // skip empty string
        size_t this_str_len = (size_t)(ptr_split_end - ptr_split_start);

        if (this_str_len > 0)
        {
            char *str_split_out = malloc(this_str_len + sizeof(dir_default_append_icons));
            assert(str_split_out != NULL);

            // duplicate first
            memcpy(str_split_out, ptr_split_start, this_str_len);
            str_split_out[this_str_len] = '\0';

            // then check and adjust its ending
            // remove the routine below almost becomes normal split function
            char *ptr_from_last_char = str_split_out + (this_str_len - 1);

            while (ptr_from_last_char >= str_split_out)
            {
                // trim '/' except root
                if ((ptr_from_last_char != str_split_out) && *ptr_from_last_char == '/')
                {
                    *ptr_from_last_char = '\0';
                    break;
                }

                // trim space
                if (*ptr_from_last_char != ' ')
                    break;

                *ptr_from_last_char = '\0';
                
                ptr_from_last_char--;
            }

            // skip if trimmed into empty string
            if (ptr_from_last_char < str_split_out)
            {
                free(str_split_out);
                goto OutDir;
            }

            size_t trimmed_len = (ptr_from_last_char + 1) - str_split_out;

            // duplicate for applications
            char *str_split_out_icons = str_split_out;
            char *str_split_out_pixmaps = malloc(trimmed_len + sizeof(dir_default_append_pixmaps));
            char *str_split_out_applications = malloc(trimmed_len + sizeof(dir_default_append_applications));

            if (!str_split_out_pixmaps || !str_split_out_applications)
            {
                if (str_split_out_pixmaps)
                    free(str_split_out_pixmaps);

                if (str_split_out_applications)
                    free(str_split_out_applications);

                goto OutDir;
            }

            memcpy(str_split_out_pixmaps, str_split_out, trimmed_len);
            memcpy(str_split_out_applications, str_split_out, trimmed_len);

            // append "/icons"
            strcpy(ptr_from_last_char + 1, dir_default_append_icons);

            // append "/pixmaps"
            strcpy(str_split_out_pixmaps + trimmed_len, dir_default_append_pixmaps);

            // append "/applications"
            strcpy(str_split_out_applications + trimmed_len, dir_default_append_applications);

            // append to search dirs
            vector_insert(&icon_search_dirs_icons, &str_split_out_icons);
            vector_insert(&icon_search_dirs_pixmaps, &str_split_out_pixmaps);
            vector_insert(&icon_search_dirs_applications, &str_split_out_applications);
        }
    OutDir:

        // break if this is last string to split
        if (last_split)
            break;

        ptr_split_end++;
        ptr_split_start = ptr_split_end;
    }
}

static bool icon_init_search_dirs(void)
{
    vector_init(&icon_search_dirs_applications, sizeof(char *));
    vector_init(&icon_search_dirs_icons, sizeof(char *));
    vector_init(&icon_search_dirs_pixmaps, sizeof(char *));
    
    // https://specifications.freedesktop.org/icon-theme/latest/#directory_layout
    const char *env_home          = getenv("HOME");
    const char *env_xdg_data_home = getenv("XDG_DATA_HOME");
    const char *env_xdg_data_dirs = getenv("XDG_DATA_DIRS");

    char buf_dir[PATH_MAX];

    if (env_home)
    {
        // $HOME/.icons
        snprintf(buf_dir, sizeof(buf_dir), "%s%s", env_home, dir_default_append_doticons);

        char *dir_home_icons = strdup(buf_dir);

        if (dir_home_icons)
            vector_insert(&icon_search_dirs_icons, &dir_home_icons);
    }

    // $XDG_DATA_HOME/icons
    // $HOME/.local/share/icons
    const char *home_dir = NULL;
    bool has_home_data_dir = false;

    if (env_xdg_data_home)
        home_dir = env_xdg_data_home;
    else if (env_home)
        home_dir = env_home;

    if (home_dir)
    {
        size_t len_home_dir = strlen(home_dir);

        // icons
        char *dir_xdg_data_home_icons = malloc(len_home_dir + sizeof(dir_default_append_local_icons));

        if (dir_xdg_data_home_icons)
        {
            memcpy(dir_xdg_data_home_icons, home_dir, len_home_dir);
            memcpy(dir_xdg_data_home_icons + len_home_dir, dir_default_append_local_icons, sizeof(dir_default_append_local_icons));

            vector_insert(&icon_search_dirs_icons, &dir_xdg_data_home_icons);
        }

        // applications
        char *dir_xdg_data_home_applications = malloc(len_home_dir + sizeof(dir_default_append_local_applications));

        if (dir_xdg_data_home_applications)
        {
            memcpy(dir_xdg_data_home_applications, home_dir, len_home_dir);
            memcpy(dir_xdg_data_home_applications + len_home_dir, dir_default_append_local_applications, sizeof(dir_default_append_local_applications));

            vector_insert(&icon_search_dirs_applications, &dir_xdg_data_home_applications);
        }
    }

    // $XDG_DATA_DIRS/icons
    if (env_xdg_data_dirs)
        icon_split_env_dir(env_xdg_data_dirs);

    // /usr/share/pixmaps
    // commented out because dir arrangement differs to above
#if 0
    char *dir_pixmaps = strdup(dir_default_pixmaps);

    if (dir_pixmaps)
    {
        if (!vector_insert(&Icon_search_dirs, &dir_pixmaps))
        {
            Icon_PrintSearchDirAppendFailError(dir_pixmaps);
            free(dir_pixmaps);
        }
    }
#endif

    // list
    struct vector_iterator iterator_vector = { };
    char **ptr_dir_ptr = NULL;
    int item_count = 1;

    vector_iterator_init(&iterator_vector, &icon_search_dirs_icons);
    while ((ptr_dir_ptr = vector_iterator_next(&iterator_vector)))
        printf("icon search dir #%d: %s\n", item_count++, *ptr_dir_ptr);

    vector_iterator_init(&iterator_vector, &icon_search_dirs_applications);
    while ((ptr_dir_ptr = vector_iterator_next(&iterator_vector)))
        printf("application search dir #%d: %s\n", item_count++, *ptr_dir_ptr);

    return true;
}

bool icon_init(void)
{
    if (icon_search_dirs_initialized)
        return true;

    if (!icon_init_search_dirs())
        return false;

    icon_search_dirs_initialized = true;

    return true;
}

void icon_cleanup(void)
{
    if (!icon_search_dirs_initialized)
        return;

    struct vector_iterator iter = { };
    char **ptr_dir_ptr = NULL;

    // icons
    vector_iterator_init(&iter, &icon_search_dirs_icons);
    while ((ptr_dir_ptr = vector_iterator_next(&iter)))
        free(*ptr_dir_ptr);
    vector_cleanup(&icon_search_dirs_icons);

    // pixmaps
    vector_iterator_init(&iter, &icon_search_dirs_pixmaps);
    while ((ptr_dir_ptr = vector_iterator_next(&iter)))
        free(*ptr_dir_ptr);
    vector_cleanup(&icon_search_dirs_pixmaps);

    // applications
    vector_iterator_init(&iter, &icon_search_dirs_applications);
    while ((ptr_dir_ptr = vector_iterator_next(&iter)))
        free(*ptr_dir_ptr);
    vector_cleanup(&icon_search_dirs_applications);

    icon_search_dirs_initialized = false;
}

static bool icon_get_type_and_icon_by_app_id(const char *app_id, char **type, char **icon)
{
    size_t len_app_id = strlen(app_id);
    char path[PATH_MAX];

    struct vector_iterator iter = { };
    char **ptr_dir_ptr = NULL;

    // find .desktop file in search dirs by app_id
    bool desktop_file_found = false;
    
    vector_iterator_init(&iter, &icon_search_dirs_applications);
    while ((ptr_dir_ptr = vector_iterator_next(&iter)))
    {
        char *dir = *ptr_dir_ptr;
        size_t len_dir = strlen(dir);

        snprintf(path, sizeof(path), "%s/%s.desktop", dir, app_id);
        
        FILE *dotdesktop_file = fopen(path, "r");

        if (dotdesktop_file)
        {
            char *line = NULL;
            size_t linesize = 0;
            ssize_t bytes_read;
            bool is_in_desktop_entry = false;

            while ((bytes_read = getline(&line, &linesize, dotdesktop_file)) != -1)
            {
                size_t line_len = bytes_read;
                line[line_len - 1] = '\0';
                line_len--;

                if (strutil_is_str_starts_with(line, "[") && strutil_is_str_ends_with(line, "]"))
                {
                    is_in_desktop_entry = !strcmp(line, "[Desktop Entry]");
                    continue;
                }

                if (!is_in_desktop_entry)
                    continue;

                if (strutil_is_str_starts_with(line, desktop_type_prefix) && (line_len > sizeof(desktop_type_prefix) - 1))
                {
                    *type = strutil_duplicate_substring(line, sizeof(desktop_type_prefix) - 1, SIZE_MAX);
                    assert(*type != NULL);
                }

                if (strutil_is_str_starts_with(line, desktop_icon_prefix) && (line_len > sizeof(desktop_icon_prefix) - 1))
                {
                    *icon = strutil_duplicate_substring(line, sizeof(desktop_icon_prefix) - 1, SIZE_MAX);
                    assert(*icon != NULL);
                }
            }

            if (line)
                free(line);

            if (ferror(dotdesktop_file))
            {
                fprintf(stderr, "fail reading file '%s': %s\n", path, strerror(errno));
                fclose(dotdesktop_file);
                break;
            }
            
            // parse done, closing
            fclose(dotdesktop_file);

            desktop_file_found = true;

            return true;
        }
    }

    // find by StartWMClass if not match above
    vector_iterator_init(&iter, &icon_search_dirs_applications);
    while ((ptr_dir_ptr = vector_iterator_next(&iter)))
    {
        char *path_to_dir = *ptr_dir_ptr;

        DIR *directory = opendir(path_to_dir);

        if (!directory)
            continue;

        printf("looking up dir: %s\n", path_to_dir);

        struct dirent *dir_entry;
        while ((dir_entry = readdir(directory)))
        {
            if (!strutil_is_str_ends_with(dir_entry->d_name, ".desktop"))
                continue;

            // printf("%s\n", dir_entry->d_name);

            snprintf(path, sizeof(path), "%s/%s", path_to_dir, dir_entry->d_name);

            FILE *desktop_file = fopen(path, "r");

            if (!desktop_file)
                continue;

            char *line = NULL;
            size_t linesize = 0;
            ssize_t bytes_read;
            bool is_in_desktop_entry = false;
            bool class_match = false;

            while ((bytes_read = getline(&line, &linesize, desktop_file)) != -1)
            {
                size_t line_len = bytes_read;
                line[line_len - 1] = '\0';
                line_len--;

                if (strutil_is_str_starts_with(line, "[") && strutil_is_str_ends_with(line, "]"))
                {
                    is_in_desktop_entry = !strcmp(line, "[Desktop Entry]");
                    continue;
                }

                if (!is_in_desktop_entry)
                    continue;

                if (strutil_is_str_starts_with(line, desktop_startup_wm_class_prefix))
                {
                    if (!strcmp(line + sizeof(desktop_startup_wm_class_prefix) - 1, app_id))
                        class_match = true;
                }

                if (strutil_is_str_starts_with(line, desktop_type_prefix) && (line_len > sizeof(desktop_type_prefix) - 1))
                {
                    *type = strutil_duplicate_substring(line, sizeof(desktop_type_prefix) - 1, SIZE_MAX);
                    assert(*type != NULL);
                }

                if (strutil_is_str_starts_with(line, desktop_icon_prefix) && (line_len > sizeof(desktop_icon_prefix) - 1))
                {
                    *icon = strutil_duplicate_substring(line, sizeof(desktop_icon_prefix) - 1, SIZE_MAX);
                    assert(*icon != NULL);
                }
            }

            if (line)
                free(line);

            if (ferror(desktop_file))
            {
                fprintf(stderr, "fail reading file '%s': %s\n", path, strerror(errno));
                fclose(desktop_file);
                break;
            }

            // parse done, closing
            fclose(desktop_file);

            if (!class_match)
            {
                if (*type)
                {
                    free(*type);
                    *type = NULL;
                }

                if (*icon)
                {
                    free(*icon);
                    *icon = NULL;
                }
            }

            if (class_match)
            {
                desktop_file_found = true;
                break;
            }
        }

        closedir(directory);
    }

    return desktop_file_found;
}

static bool icon_get_path_by_type_and_icon_in_theme_folder(const char *dir, const char *theme, const char *type, const char *icon, int size, int scale, char **out_path)
{
    char path[PATH_MAX];
    FILE *index_file = NULL;

    // open index file
    snprintf(path, sizeof(path), "%s/%s/index.theme", dir, theme);

    if (!(index_file = fopen(path, "r")))
    {
        //fprintf(stderr, "failed to open theme index '%s': %s\n", path, strerror(errno));
        return false;
    }
    
    // read file and most fit select variant
    char icon_path_to_check[PATH_MAX];
    char icon_path[PATH_MAX];

    bool icon_found = false;

    // current state
    char *curr_key = NULL;
    int curr_key_scale = 1;
    int curr_key_size  = 0;
    bool curr_key_scalable = false;

    // chosen icon state
    char *selected_key = NULL;
    int selected_scale = 1;//INT_MAX;
    int selected_size  = 0;//INT_MAX;
    bool selected_scalable = false;

    bool context_is_applications = false;

    char *line_str = NULL;
    size_t linesize = 0;
    ssize_t bytes_read;

    while (true)
    {
        bytes_read = getline(&line_str, &linesize, index_file);
        size_t line_len = bytes_read;

        bool store_state = false;
        bool switch_state = false;

        if (bytes_read > 0)
        {
            line_str[line_len - 1] = '\0';
            line_len--;

            // when new key occurs
            if (strutil_is_str_starts_with(line_str, "[") &&
                strutil_is_str_ends_with(line_str, "]") &&
                strcmp(line_str, "[Icon Theme]"))
            {
                // store state change state when find new key
                if (curr_key)
                    store_state = true;

                switch_state = true;
            }
        }

        // save state for last read
        if (bytes_read < 0)
            store_state = true;

        // try save if context valid and type is Applications 
        if (store_state && curr_key && context_is_applications)
        {
            // check fit and save state if inside a valid key
            if (!selected_scalable)
            {
                bool icon_exists_here = false;

                // check existance
                // TODO: check existance here...
                FILE *icon_file = NULL;
                for (int i = 0; i < sizeof(icon_extensions) / sizeof(icon_extensions[0]); i++)
                {
                    // basedir/theme/key/icon.ext
                    if (snprintf(icon_path_to_check, sizeof(icon_path_to_check), "%s/%s/%s/%s%s", dir, theme, curr_key, icon, icon_extensions[i]) < 0)
                        break;

                    if ((icon_file = fopen(icon_path_to_check, "r")))
                    {
                        icon_exists_here = true;
                        fclose(icon_file);
                        break;
                    }
                }

                // - non-standard implemention- 
                // A. scalable first (FIXME: not implement yet)
                // B. when scalable not available
                //  1. fit scale (increase until equal)
                //  2. fit size  (enlarge to larger then shrink to fit)
                if (icon_exists_here)
                {
                    // fit scale first
                    // from 1x until equal
                    bool scale_fit =
                        (curr_key_scale <= scale) &&
                        (curr_key_scale <= selected_scale);
                    
                    // then fit size;
                    bool size_fit;
                    
                    if (selected_size < size)
                    {
                        // enlarge first
                        size_fit = (curr_key_size > selected_size);
                    }
                    else
                    {
                        // most close to 
                        size_fit =
                            (curr_key_size >= size) &&
                            (curr_key_size < selected_size);
                    }
                    
                    // select when both fit
                    if (scale_fit && size_fit)
                    {
                        if (selected_key)
                            free(selected_key);

                        selected_key = curr_key;

                        selected_scale = curr_key_scale;
                        selected_size  = curr_key_size;

                        strcpy(icon_path, icon_path_to_check);

                        icon_found = true;
                    }
                }
            }
        }

        if (store_state)
        {
            // reset current state
            curr_key_size  = 0;
            curr_key_scale = 1;
            curr_key_scalable = false;
            context_is_applications = false;
        }

        if (switch_state)
        {
            // switch current key
            if (curr_key && (curr_key != selected_key))
                free(curr_key);

            // release when fail
            curr_key = strutil_duplicate_substring(line_str, 1, line_len - 2);
            assert(curr_key != NULL);
        }

        // break if last read
        if (bytes_read < 0)
            break;

        // dont extract value when no valid key pending
        if (!curr_key)
            continue;

        // read values
        if (strutil_is_str_starts_with(line_str, theme_context_prefix))
        {
            if (!strcmp(line_str + sizeof(theme_context_prefix) - 1, "Applications"))//type))
                context_is_applications = true;
        }

        if (strutil_is_str_starts_with(line_str, theme_size_prefix))
            curr_key_size = atoi(line_str + sizeof(theme_size_prefix) - 1);
        
        if (strutil_is_str_starts_with(line_str, theme_scale_prefix))
            curr_key_scale = atoi(line_str + sizeof(theme_scale_prefix) - 1);

        if (strutil_is_str_starts_with(line_str, theme_type_prefix))
        {
            if (!strcmp(line_str + sizeof(theme_type_prefix) - 1, "Scalable"))
                curr_key_scalable = true;
        }
    }

    // cleanup
    if (line_str)
        free(line_str);

    if (curr_key)
        free(curr_key);

    if (selected_key)
        free(selected_key);

    if (ferror(index_file))
    {
        fprintf(stderr, "fail reading theme index '%s': %s\n", path, strerror(errno));
        return false;
    }

    // not found
    if (!icon_found)
        return false;

    // allocate memory for return string
    size_t icon_path_size = strlen(icon_path) + 1;
    char *icon_path_final = malloc(icon_path_size);
    assert(icon_path_final != NULL);

    memcpy(icon_path_final, icon_path, icon_path_size);

    *out_path = icon_path_final;

    return true;
}

static bool icon_get_path_by_type_and_icon(const char *type, const char *icon, int size, int scale, char **out_path)
{
    char icon_path[PATH_MAX];

    // iter icon search path
    struct vector_iterator iter = { };
    char **ptr_dir_ptr = NULL;

    vector_iterator_init(&iter, &icon_search_dirs_icons);
    while ((ptr_dir_ptr = vector_iterator_next(&iter)))
    {
        char *dir = *ptr_dir_ptr;

        // TODO: add theme support?
        if (icon_get_path_by_type_and_icon_in_theme_folder(dir, "hicolor", type, icon, size, scale, out_path))
            return true;
    }

    // pixmaps
    vector_iterator_init(&iter, &icon_search_dirs_pixmaps);
    while ((ptr_dir_ptr = vector_iterator_next(&iter)))
    {
        char *dir = *ptr_dir_ptr;

        for (int i = 0; i < sizeof(icon_extensions) / sizeof(icon_extensions[0]); i++)
        {
            if (snprintf(icon_path, sizeof(icon_path), "%s/%s%s", dir, icon, icon_extensions[i]) < 0)
                return false;

            FILE *icon_file = fopen(icon_path, "r");

            if (icon_file)
            {
                fclose(icon_file);

                size_t icon_path_size = strlen(icon_path) + 1;
                char *icon_path_final = malloc(icon_path_size);
                assert(icon_path_final != NULL);

                memcpy(icon_path_final, icon_path, icon_path_size);

                *out_path = icon_path_final;

                return true;
            }
        }
    }

    // try open icon as file
    FILE *icon_file = fopen(icon, "r");

    if (icon_file)
    {
        fclose(icon_file);

        size_t icon_path_size = strlen(icon) + 1;
        char *icon_path_final = malloc(icon_path_size);
        assert(icon_path_final != NULL);

        memcpy(icon_path_final, icon, icon_path_size);

        *out_path = icon_path_final;

        return true;
    }

    return false;
}

bool icon_find_app_icon_path(const char *app_id, int size, int scale, char **out_path)
{
    if (!icon_search_dirs_initialized)
        return false;

    if (!(app_id && out_path))
        return false;

    bool success = false;
    char *type = NULL;
    char *icon = NULL;

    icon_get_type_and_icon_by_app_id(app_id, &type, &icon);

    // TODO: app_id ends with .exe -> Wine specified icon find method?
    if (!(type && icon))
    {
        success = false;
        goto End;
    }
    
    icon_get_path_by_type_and_icon(type, icon, size, scale, out_path);

End:
    if (type)
        free(type);

    if (icon)
        free(icon);

    return success;
}
