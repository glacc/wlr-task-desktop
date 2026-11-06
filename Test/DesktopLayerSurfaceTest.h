#pragma once

#include "Buffer.h"
#include "DesktopLayerSurface.h"

extern void TestPatternDrawOnDesktopLayerSurface(struct DesktopLayerSurface *layer_surface);
extern void TestSWRenderOnDesktopLayerSurface(struct DesktopLayerSurface *layer_surface);
extern void TestDesktopLayerSurface(struct DesktopLayerSurface *layer_surface, struct AppState *app_state);
