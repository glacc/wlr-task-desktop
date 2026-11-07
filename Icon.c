#include "Icon.h"

// Freedesktop icon lookup routine
// https://specifications.freedesktop.org/icon-theme/latest/

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "Vector.h"

static const char *dir_pixmaps = "/usr/share/pixmaps";

// mainly for $XDG_DATA_DIRS
static void Icon_SplitEnvDir(struct Vector *search_dirs, const char *env_dir)
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
            char *str_split_out = malloc(this_str_len + 1);

            if (!str_split_out)
            {
                fprintf(stderr, "failed to allocate memory for search dir: %s\n", strerror(errno));
                goto OutDirname;
            }

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
                goto OutDirname;
            }

            // append to search dirs
            if (!Vector_Add(search_dirs, &str_split_out))
            {
                fprintf(stderr, "failed to append search dir: %s\n", strerror(errno));
                free(str_split_out);
            }
        }
    OutDirname:

        // break if this is last string to split
        if (last_split)
            break;

        ptr_split_end++;
        ptr_split_start = ptr_split_end;
    }
}

static bool Icon_GetSearchDirs(struct Vector *search_dirs)
{
    // https://specifications.freedesktop.org/icon-theme/latest/#directory_layout
    char *env_home          = getenv("HOME");
    char *env_xdg_data_home = getenv("XDG_DATA_HOME");
    char *env_xdg_data_dirs = getenv("XDG_DATA_DIRS");

    // home
    if (env_home)
    {
        char *dir_home = strdup(env_home);
        if (dir_home)
            Vector_Add(search_dirs, &dir_home);
    }

    // xdg_data_dirs
    if (env_xdg_data_dirs)
    {
        
    }

    return NULL;
}

static char *Icon_LookupIcon(const char *iconname, int size, int scale, const char *theme)
{
    struct Vector search_dirs;   // typeof(char *)
    if (!Vector_Init(&search_dirs, sizeof(char *)))
    {
        fprintf(stderr, "failed to initialize Vector for icon lookup: %s\n", strerror(errno));
        return NULL;
    }

    return NULL;
}

static char *Icon_FindIconHelper(const char *icon, int size, int scale, const char *theme)
{
    char *filepath = NULL;
    
    if ((filepath = Icon_LookupIcon(icon, size, scale, theme)))
        return filepath;

    // if theme has parents
    // ...

    // not found
    return NULL;
}

bool Icon_Find(const char *icon, int size, int scale, char *out_filepath)
{
    if (!(icon && out_filepath))
        return false;

    


    return true;
}

void Icon_TestFunctions(void)
{
    printf("XDG_DATA_DIRS=");

    const char *xdg_data_dir = getenv("XDG_DATA_DIRS");

    if (!xdg_data_dir)
    {
        puts("NULL");
        return;
    }
    else
        puts(xdg_data_dir);

    struct Vector search_dirs;
    if (!Vector_Init(&search_dirs, sizeof(char *)))
    {
        fprintf(stderr, "failed to initialize Vector for search dirs: %s\n", strerror(errno));
        return;
    }

    Icon_SplitEnvDir(&search_dirs, xdg_data_dir);

    struct Vector_Iter iter = { };
    
    if (Vector_IterInit(&iter, &search_dirs))
    {
        char **ptr_search_dir_ptr = NULL;
        
        while ((ptr_search_dir_ptr = Vector_IterNext(&iter)))
            puts(*ptr_search_dir_ptr);
    }

    Vector_Cleanup(&search_dirs);
}
