#include "DesktopLayerSurfaceTest.h"

#include <math.h>

#include "Globals.h"

#include "SWRender/SWRender.h"

#define M_PI_2f 1.57079632679489661923f /* pi/2 */
#define M_PI    3.14159265358979323846  /* pi */

static const SWRenderColor color_transition_table[] =
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

SWRenderColor GetTransitionColor(float alpha, float progress)
{
    float progress_total = (float)(color_transition_table_length - 1) * progress;
    float progress_index = progress_total - floorf(progress_total);

    int index = (int)progress_total;
    if (index < 0)
        index = 0;
    if (index >= color_transition_table_length - 1)
        index = color_transition_table_length - 2;

    const SWRenderColor *color_curr = &color_transition_table[index];
    const SWRenderColor *color_next = &color_transition_table[index + 1];

    float r = ((float)color_curr->r + ((float)(color_next->r - color_curr->r) * progress_index));
    float g = ((float)color_curr->g + ((float)(color_next->g - color_curr->g) * progress_index));
    float b = ((float)color_curr->b + ((float)(color_next->b - color_curr->b) * progress_index));
    float a = ((float)color_curr->a + ((float)(color_next->a - color_curr->a) * progress_index));

    return (SWRenderColor){ (uint8_t)a, (uint8_t)r, (uint8_t)g, (uint8_t)b };
}

static void ColorTransitionTo(SWRenderColor *pixel, SWRenderColor *color_dst, float progress)
{
    float a_new = (float)pixel->a + ((float)(color_dst->a - pixel->a) * progress);
    float r_new = (float)pixel->r + ((float)(color_dst->r - pixel->r) * progress);
    float g_new = (float)pixel->g + ((float)(color_dst->g - pixel->g) * progress);
    float b_new = (float)pixel->b + ((float)(color_dst->b - pixel->b) * progress);

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
    SWRenderColor color_bottom = { 127, 127, 127, 255 * alpha_norm };
    color_bottom.r *= alpha_norm;
    color_bottom.g *= alpha_norm;
    color_bottom.b *= alpha_norm;

    SWRenderColor *ptr_row_start = SWRender_GetPointerByPosition(&layer_surface->surface.swr_buf, 0, 0);
    float progress_horz_inv = 1.0F / (float)layer_surface->surface.swr_buf.w;
    float progress_vert_inv = 1.0F / (float)(layer_surface->surface.swr_buf.h - 1);

    for (int y = 0; y < layer_surface->surface.swr_buf.h; y++)
    {
        SWRenderColor *ptr_pixel = ptr_row_start;
        float vert_progress = y * progress_vert_inv;

        for (int x = 0; x < layer_surface->surface.swr_buf.w; x++)
        {
            float horz_progress = x * progress_horz_inv;
            *ptr_pixel = GetTransitionColor(alpha_norm, horz_progress);
            ColorTransitionTo(ptr_pixel, &color_bottom, vert_progress);
            ptr_pixel++;
        }

        ptr_row_start += layer_surface->surface.swr_buf.w;
    }

    /*
    //wl_surface_commit(layer_surface->wl_surface);
    DoubleBufferedSurface_AddDamage(&layer_surface->surface, NULL);
    DoubleBufferedSurface_TryCommit(&layer_surface->surface);

    wl_display_roundtrip(layer_surface->app_state->display);
    */
}

void TestSWRenderOnDesktopLayerSurface(struct DesktopLayerSurface *layer_surface)
{
    if (!layer_surface)
        return;

    SWRenderBuffer *swr_buf = &layer_surface->surface.swr_buf;

    if (!swr_buf->data)
        return;

    float half_w = (float)swr_buf->w / 2.0F;
    float half_h = (float)swr_buf->h / 2.0F;

    // lines
    const int line_count = 24;
    const float line_start_dist = 8.0F;
    const float line_end_dist = 160.0F;
    const float line_thickness = 5.0F;
    const float bg_ring_thickness = 5.0F;
    const float bg_radius = line_end_dist + bg_ring_thickness + line_thickness + line_thickness;
    const float bg_rect_half_size = bg_radius + bg_ring_thickness + bg_ring_thickness;
    float bg_rect_pos_ul = half_w - bg_rect_half_size;
    float bg_rect_pos_dr = half_h + bg_rect_half_size;
    const float bg_rect_corner_radius = 10.0F;

    const float angle_offset = -M_PI_2f;

    SWRender_FillRectRounded(swr_buf, &(SWRenderBoundF){ bg_rect_pos_ul, bg_rect_pos_ul, bg_rect_pos_dr, bg_rect_pos_dr }, bg_rect_corner_radius, (SWRenderColor){ 0x00, 0x00, 0x00, 0x40 });
    SWRender_FillCircle(swr_buf, (SWRenderVec2F){ half_w , half_h }, bg_radius, NULL, (SWRenderColor){ 0x00, 0x00, 0x00, 0x40 });
    SWRender_FillRing(swr_buf, (SWRenderVec2F){ half_w, half_h }, bg_radius, bg_ring_thickness, NULL, (SWRenderColor){ 0x00, 0x00, 0x00, 0x40 });

    for (int i = 0; i < line_count; i++)
    {
        float progress = (float)i * (1.0F / (float)line_count);
        float angle = (progress * 2.0F * M_PI) + angle_offset;

        float x1 = half_w + (cosf(angle) * line_start_dist);
        float y1 = half_h + (sinf(angle) * line_start_dist);
        float x2 = half_w + (cosf(angle) * line_end_dist);
        float y2 = half_h + (sinf(angle) * line_end_dist);

        SWRender_Line(swr_buf, &(SWRenderBoundF){ x1, y1, x2, y2 }, GetTransitionColor((0x80 / 255.0F), progress), line_thickness);
    }
}

void TestDesktopLayerSurface(struct DesktopLayerSurface *layer_surface)
{
    if (!DesktopLayerSurface_Init(layer_surface, NULL))
        return;
    
    TestPatternDrawOnDesktopLayerSurface(layer_surface);

    DoubleBufferedSurface_AddDamage(&layer_surface->surface, NULL);
    DoubleBufferedSurface_TryCommit(&layer_surface->surface);

    wl_display_roundtrip(wl_display);
}
