#include "path.h"

n_GPath * n_gpath_create(n_GPathInfo * path_info) {
    n_GPath * out = NGFX_PREFERRED_malloc(sizeof(n_GPath));
    out->num_points = path_info->num_points;
    out->points = path_info->points;
    out->angle = 0;
    out->offset = n_GPointZero;
    out->open = false;
    return out;
}

void n_gpath_destroy(n_GPath * path) {
    NGFX_PREFERRED_free(path);
}

// --- //

static void n_prv_bubblesort(short * numbers, unsigned int amt) {
    // Yes, bubblesort is sub-ideal. However, bubblesort is absolutely enough
    // for 2-8 lines which will usually pass through a scanline.
    if (!amt)
        return;
    char swapped;
    short tmp;
    do {
        swapped = false;
        for (unsigned int i = 0; i < amt-1; i++) {
            if (numbers[i] > numbers[i+1]) {
                tmp = numbers[i+1];
                numbers[i+1] = numbers[i];
                numbers[i] = tmp;
                swapped = true;
            }
        }
    } while(swapped);
}

// --- //

void n_graphics_draw_path(n_GContext * ctx, unsigned int num_points, n_GPoint * points, char open) {
    for (unsigned int p = 0; p < num_points - 1; p++)
        n_graphics_draw_line(ctx, points[p], points[p + 1]);
    if (!open)
        n_graphics_draw_line(ctx, points[num_points - 1], points[0]);
}

void n_graphics_draw_ppath(n_GContext * ctx, unsigned int num_points, n_GPoint * points, char open) {
    for (unsigned int p = 0; p < num_points - 1; p++)
        n_graphics_draw_line(ctx,
            n_GPoint((points[p].x + 4) >> 3, (points[p].y + 4) >> 3),
            n_GPoint((points[p + 1].x + 4) >> 3, (points[p + 1].y + 4) >> 3));
    if (!open)
        n_graphics_draw_line(ctx,
            n_GPoint((points[num_points - 1].x + 4) >> 3, (points[num_points - 1].y + 4) >> 3),
            n_GPoint((points[0].x + 4) >> 3, (points[0].y + 4) >> 3));
}

static void n_graphics_fill_path_bounded(n_GContext * ctx, unsigned int num_points, n_GPoint * points,
                                         short minx, short maxx, short miny, short maxy) {
#ifdef PBL_BW
    char color = __ARGB_TO_INTERNAL(ctx->fill_color.argb);
#endif

    // Minimize size we iterate over
    short _maxy = miny, _miny = maxy;
    for (unsigned int i = 0; i < num_points; i++) {
        if (points[i].y < _miny) _miny = points[i].y;
        if (points[i].y > _maxy) _maxy = points[i].y;
    }
    maxy = __BOUND_NUM(miny, _maxy, maxy);
    miny = __BOUND_NUM(miny, _miny, maxy);

    // Further optimization may be possible here to minimize unnecessary allocation
    short * x_positions = NGFX_PREFERRED_malloc(sizeof(short) * num_points);

    for (short y = miny; y <= maxy; y++) {
        unsigned int num_x_positions = 0;
        unsigned int b, i, n; // b, i, n == before, iter, next
        for (i = 0; i < num_points; i++) {
            b = (num_points + i - 1) % num_points;
            n = (             i + 1) % num_points;

            if ( (points[i].y <= y && points[n].y > y) ||
                 (points[i].y >= y && points[n].y < y) ) {
                short dx = points[n].x - points[i].x,
                        dy = points[n].y - points[i].y;
                signed char e = (dx == 0 ? 0 : (dx > 0 ? 1 : -1));

                x_positions[num_x_positions] =
                    points[i].x + (dx * (y - points[i].y) * 2 + e * dy) / (dy * 2);
                num_x_positions += 1;

                // If we're at a corner, add the position twice.
                // We don't have to worry about going out of bounds of
                // the array `x_positions` since being at a corner implies
                // that at least one other point isn't in this scanline.
                if ( (points[i].y == y &&
                      points[b].y  < y && points[n].y < y) ||
                     (points[i].y == y &&
                      points[b].y  > y && points[n].y > y) ) {
                    x_positions[num_x_positions] = x_positions[num_x_positions - 1];
                    num_x_positions += 1;
                }
            }
        }
        n_prv_bubblesort(x_positions, num_x_positions);
        for (unsigned int p = 0; (p + 1) < num_x_positions; p += 2) {
            // We're not going to draw the path. Also, only actually draw if
            // there is something to be drawn.
            if (x_positions[p] <= x_positions[p+1] - 2)
#ifdef PBL_BW
                n_graphics_prv_draw_row(ctx->fbuf, y, x_positions[p] + 1, x_positions[p+1] - 1,
                                        minx, maxx, miny, maxy, color);
#else
                n_graphics_prv_draw_row(ctx->fbuf, y, x_positions[p] + 1, x_positions[p+1] - 1,
                                        minx, maxx, miny, maxy, ctx->fill_color.argb);
#endif
        }
    }
    NGFX_PREFERRED_free(x_positions);
}

static void n_graphics_fill_ppath_bounded(n_GContext * ctx, unsigned int num_points, n_GPoint * _points,
                                         short minx, short maxx, short miny, short maxy) {
    n_GPoint * points = NGFX_PREFERRED_malloc(sizeof(n_GPoint) * num_points);
    for (unsigned int n = 0; n < num_points; n++) {
        points[n] = n_GPoint((_points[n].x + 4) >> 3, (_points[n].y + 4) >> 3);
    }
#ifdef PBL_BW
    char color = __ARGB_TO_INTERNAL(ctx->fill_color.argb);
#endif

    // Minimize size we iterate over
    short _maxy = miny, _miny = maxy;
    for (unsigned int i = 0; i < num_points; i++) {
        if (points[i].y < _miny) _miny = points[i].y;
        if (points[i].y > _maxy) _maxy = points[i].y;
    }
    maxy = __BOUND_NUM(miny, _maxy, maxy);
    miny = __BOUND_NUM(miny, _miny, maxy);

    // Further optimization may be possible here to minimize unnecessary allocation
    short * x_positions = NGFX_PREFERRED_malloc(sizeof(short) * num_points);

    for (short y = miny; y <= maxy; y++) {
        unsigned int num_x_positions = 0;
        unsigned int b, i, n; // b, i, n == before, iter, next
        for (i = 0; i < num_points; i++) {
            b = (num_points + i - 1) % num_points;
            n = (             i + 1) % num_points;

            if ( (points[i].y <= y && points[n].y > y) ||
                 (points[i].y >= y && points[n].y < y) ) {
                short dx = points[n].x - points[i].x,
                        dy = points[n].y - points[i].y;
                signed char e = (dx == 0 ? 0 : (dx > 0 ? 1 : -1));

                x_positions[num_x_positions] =
                    points[i].x + (dx * (y - points[i].y) * 2 + e * dy) / (dy * 2);
                num_x_positions += 1;

                // If we're at a corner, add the position twice.
                // We don't have to worry about going out of bounds of
                // the array `x_positions` since being at a corner implies
                // that at least one other point isn't in this scanline.
                if ( (points[i].y == y &&
                      points[b].y  < y && points[n].y < y) ||
                     (points[i].y == y &&
                      points[b].y  > y && points[n].y > y) ) {
                    x_positions[num_x_positions] = x_positions[num_x_positions - 1];
                    num_x_positions += 1;
                }
            }
        }
        n_prv_bubblesort(x_positions, num_x_positions);
        for (unsigned int p = 0; (p + 1) < num_x_positions; p += 2) {
            // We're not going to draw the path. Also, only actually draw if
            // there is something to be drawn.
            if (x_positions[p] <= x_positions[p+1] - 2)
#ifdef PBL_BW
                n_graphics_prv_draw_row(ctx->fbuf, y, x_positions[p] + 1, x_positions[p+1] - 1,
                                        minx, maxx, miny, maxy, color);
#else
                n_graphics_prv_draw_row(ctx->fbuf, y, x_positions[p] + 1, x_positions[p+1] - 1,
                                        minx, maxx, miny, maxy, ctx->fill_color.argb);
#endif
        }
    }
    NGFX_PREFERRED_free(x_positions);
    NGFX_PREFERRED_free(points);
}

void n_graphics_fill_path(n_GContext * ctx, unsigned int num_points, n_GPoint * points) {
    n_graphics_fill_path_bounded(ctx, num_points, points, 0, __SCREEN_WIDTH, 0, __SCREEN_HEIGHT);
}

void n_graphics_fill_ppath(n_GContext * ctx, unsigned int num_points, n_GPoint * points) {
    n_graphics_fill_ppath_bounded(ctx, num_points, points, 0, __SCREEN_WIDTH, 0, __SCREEN_HEIGHT);
}

// --- //

void n_prv_transform_points(unsigned int num_points, n_GPoint * points_in, n_GPoint * points_out,
                            short angle, n_GPoint offset) {
    for (unsigned int i = 0; i < num_points; i++) {
        points_out[i] = points_in[i];
#ifndef NO_TRIG
        if (angle) {
            long long sine   = sin_lookup(angle),
                    cosine = cos_lookup(angle);
            points_out[i].x = (cosine * points_in[i].x -   sine * points_in[i].y) / TRIG_MAX_RATIO;
            points_out[i].y = (  sine * points_in[i].x + cosine * points_in[i].y) / TRIG_MAX_RATIO;
        }
#endif
        points_out[i].x += offset.x;
        points_out[i].y += offset.y;
    }
}

void n_gpath_draw(n_GContext * ctx, n_GPath * path) {
    if (!(ctx->stroke_color.argb & (0b11 << 6)))
        return;
    n_GPoint * points = NGFX_PREFERRED_malloc(sizeof(n_GPoint) * path->num_points);
    n_prv_transform_points(path->num_points, path->points, points,
                           path->angle, path->offset);
    n_graphics_draw_path(ctx, path->num_points, points, path->open);
    NGFX_PREFERRED_free(points);
}

void n_gpath_fill(n_GContext * ctx, n_GPath * path) {
    if (!(ctx->fill_color.argb & (0b11 << 6)))
        return;
    // n_gpath_fill_bounded(ctx, path, 0, __SCREEN_WIDTH, 0, __SCREEN_HEIGHT);
    n_GPoint * points = NGFX_PREFERRED_malloc(sizeof(n_GPoint) * path->num_points);
    n_prv_transform_points(path->num_points, path->points, points,
        path->angle, path->offset);
    n_graphics_fill_path(ctx, path->num_points, points);
    NGFX_PREFERRED_free(points);
}

// --- //

void n_gpath_rotate_to(n_GPath * path, int angle) {
    path->angle = angle;
}

void n_gpath_move_to(n_GPath * path, n_GPoint offset) {
    path->offset = offset;
}

void n_gpath_set_open(n_GPath * path, char open) {
    path->open = open;
}
