#pragma once

#include <stdbool.h>

extern bool Icon_Init(void);
extern void Icon_Cleanup(void);

extern bool Icon_FindAppIconPath(const char *app_id, int size, int scale, char **out_path);
