#include "DesktopLayerSurfaceTest.h"

#include "viewporter-client-protocol.h"

static const struct Buffer_Pixel color_transition_table[] =
{
    { 0xFF, 0xFF, 0x00, 0x00 },
    { 0xFF, 0xFF, 0xFF, 0x00 },
    { 0xFF, 0x00, 0xFF, 0x00 },
    { 0xFF, 0x00, 0xFF, 0xFF },
    { 0xFF, 0x00, 0x00, 0xFF },
    { 0xFF, 0xFF, 0x00, 0xFF },
    { 0xFF, 0xFF, 0x00, 0x00 },
};
const int color_transition_table_length = (sizeof(color_transition_table) / sizeof(color_transition_table[0]));

static struct Buffer_Pixel GetTransitionColor(float alpha, float progress)
{
    float progress_total = (float)(color_transition_table_length - 1) * progress;
    float progress_index = progress_total - floorf(progress_total);

    int index = (int)progress_total;
    if (index < 0)
        index = 0;
    if (index >= color_transition_table_length - 1)
        index = color_transition_table_length - 2;

    const struct Buffer_Pixel *color_curr = &color_transition_table[index];
    const struct Buffer_Pixel *color_next = &color_transition_table[index + 1];

    float r = ((float)color_curr->r + ((float)(color_next->r - color_curr->r) * progress_index)) * alpha;
    float g = ((float)color_curr->g + ((float)(color_next->g - color_curr->g) * progress_index)) * alpha;
    float b = ((float)color_curr->b + ((float)(color_next->b - color_curr->b) * progress_index)) * alpha;
    float a = ((float)color_curr->a + ((float)(color_next->a - color_curr->a) * progress_index)) * 1.0F;

    return (struct Buffer_Pixel){ (uint8_t)a, (uint8_t)r, (uint8_t)g, (uint8_t)b };
}

static void ColorTransitionTo(struct Buffer_Pixel *pixel, struct Buffer_Pixel *color_dst, float progress)
{
    float a_old_norm = (float)pixel->a / 255.0F;
    float r_old = (float)pixel->r / a_old_norm;
    float g_old = (float)pixel->g / a_old_norm;
    float b_old = (float)pixel->b / a_old_norm;

    float a_new = (float)pixel->a + ((float)(color_dst->a - pixel->a) * progress);
    float a_new_norm = a_new / 255.0F;
    float r_new = ((float)pixel->r + ((float)(color_dst->r - pixel->r) * progress)) * a_new_norm;
    float g_new = ((float)pixel->g + ((float)(color_dst->g - pixel->g) * progress)) * a_new_norm;
    float b_new = ((float)pixel->b + ((float)(color_dst->b - pixel->b) * progress)) * a_new_norm;

    pixel->a = (uint8_t)a_new;
    pixel->r = (uint8_t)r_new;
    pixel->g = (uint8_t)g_new;
    pixel->b = (uint8_t)b_new;
}

static inline void VersionMin(uint32_t *version, uint32_t supported)
{
    if (*version > supported)
        *version = supported;
}

void TestPatternDrawOnDesktopLayerSurface(struct DesktopLayerSurface *layer_surface)
{
    float alpha_norm = 0.5F;
    struct Buffer_Pixel color_bottom = { 127, 127, 127, 255 * alpha_norm };
    color_bottom.r *= alpha_norm;
    color_bottom.g *= alpha_norm;
    color_bottom.b *= alpha_norm;

    struct Buffer_Pixel *ptr_row_start = layer_surface->surface.pixels;
    float progress_horz_inv = 1.0F / (float)layer_surface->surface.w;
    float progress_vert_inv = 1.0F / (float)(layer_surface->surface.h - 1);

    for (int y = 0; y < layer_surface->surface.h; y++)
    {
        struct Buffer_Pixel *ptr_pixel = ptr_row_start;
        float vert_progress = y * progress_vert_inv;

        for (int x = 0; x < layer_surface->surface.w; x++)
        {
            float horz_progress = x * progress_horz_inv;
            *ptr_pixel = GetTransitionColor(alpha_norm, horz_progress);
            ColorTransitionTo(ptr_pixel, &color_bottom, vert_progress);
            ptr_pixel++;
        }

        ptr_row_start += layer_surface->surface.w;
    }

    //wl_surface_commit(layer_surface->wl_surface);
    DoubleBufferedSurface_AddDamage(&layer_surface->surface, NULL);
    DoubleBufferedSurface_TryCommit(&layer_surface->surface);

    wl_display_roundtrip(layer_surface->app_state->display);
}

void TestDesktopLayerSurface(struct DesktopLayerSurface *layer_surface, struct AppState *app_state)
{
    if (!DesktopLayerSurface_Init(layer_surface, app_state, NULL))
        return;
    
    TestPatternDrawOnDesktopLayerSurface(layer_surface);
}
