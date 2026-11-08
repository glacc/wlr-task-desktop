#pragma once

#include <stddef.h>

#include <wayland-util.h>

#include <SWRender/SWRender.h>

extern void Utility_TryReplaceWithAnotherString(char **dst, const char *src);

extern wl_fixed_t Utility_DistSqBetweenVecWlFixed(wl_fixed_t x1, wl_fixed_t y1, wl_fixed_t x2, wl_fixed_t y2);
extern int Utility_WlFixedToInt(wl_fixed_t wl_fixed);

bool Utility_IsStrStartsWith(const char *str_to_cmp, const char *str);
bool Utility_IsStrEndsWith(const char *str_to_cmp, const char *str);
char *Utility_DupSubString(const char *str, size_t start_index, size_t length);
