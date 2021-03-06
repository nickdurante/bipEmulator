#include "text.h"

// TODO possibly move from this to a "true" unicode subsystem

#define __CODEPOINT_IGNORE_AT_LINE_END(a) ((a) == 32)
// Breaking after this character is a _good idea_
// (for example, breaking after spaces or hyphens is good)
// This is preferred to prebreaking. Hyphens after postbreakables are ignored.
// #define __CODEPOINT_GOOD_POSTBREAKABLE(a) (false)
#define __CODEPOINT_GOOD_POSTBREAKABLE(a) ((a) == 32)
// Breaking after this character is allowed
// (for example, you aren't allowed to break immediately before punctuation.)
#define __CODEPOINT_ALLOW_PREBREAKABLE(a) (!((a) == 32 || (a) == 33 || (a) == 34 || (a) == 44 || (a) == 46))
#define __CODEPOINT_NEEDS_HYPHEN_AFTER(a) (\
        /* numbers          */ ((a) >= 0x30 && (a) <= 0x39) ||\
        /* basic uppercase  */ ((a) >= 0x41 && (a) <= 0x5a) ||\
        /* basic lowercase  */ ((a) >= 0x61 && (a) <= 0x7a) ||\
        /* extended a and b */ ((a) >= 0x100 && (a) <= 0x24f) \
    )

static n_GPoint n_graphics_prv_draw_text_line(n_GContext * ctx, const char * text,
        unsigned int idx, unsigned int idx_end,
        n_GFont const font, n_GPoint text_origin) {
    while (idx < idx_end) {
        unsigned int codepoint = 0;
        if (text[idx] & 0b10000000) {
            if ((text[idx] & 0b11100000) == 0b11000000) {
                codepoint = ((text[idx  ] &  0b11111) << 6)
                          +  (text[idx+1] & 0b111111);
                idx += 2;
            } else if ((text[idx] & 0b11110000) == 0b11100000) {
                codepoint = ((text[idx  ] &   0b1111) << 12)
                          + ((text[idx+1] & 0b111111) << 6)
                          +  (text[idx+2] & 0b111111);
                idx += 3;
            } else if ((text[idx] & 0b11111000) == 0b11110000) {
                codepoint = ((text[idx  ] &    0b111) << 18)
                          + ((text[idx+1] & 0b111111) << 12)
                          + ((text[idx+2] & 0b111111) << 6)
                          +  (text[idx+3] & 0b111111);
                idx += 4;
            } else {
                idx += 1;
            }
        } else {
            codepoint = text[idx];
            idx += 1;
        }
        n_GGlyphInfo * glyph = n_graphics_font_get_glyph_info(font, codepoint);
        n_graphics_font_draw_glyph(ctx, glyph, text_origin);
        text_origin.x += glyph->advance;
    }
    return text_origin;
}

void n_graphics_draw_text(
    n_GContext * ctx, const char * text, n_GFont const font, const n_GRect box,
    const n_GTextOverflowMode overflow_mode, const n_GTextAlignment alignment,
    n_GTextAttributes * text_attributes) {
    //TODO alignment
    //TODO attributes

    // Rendering of text is done as follows:
    // - We store the index of the beginning of the line.
    // - We iterate over characters in the line.
    //    - Whenever an after-breakable character occurs, we make a note of it.
    //    - When the width of the line is exceeded, we actually render
    //      the line (up to the breakable character.)
    //    - We then use that character's index as the beginning
    //      of the next line.
    n_GPoint char_origin = box.origin, line_origin = box.origin;
    char line_height = n_graphics_font_get_line_height(font); /* Cache this, because it is expensive on RebbleOS. */
    unsigned int line_begin = 0, index = 0, next_index = 0;
    int last_breakable_index = -1, last_renderable_index = -1,
            lenience = n_graphics_font_get_glyph_info(font, ' ')->advance;
    n_GGlyphInfo * hyphen = n_graphics_font_get_glyph_info(font, '-'),
                 * glyph = NULL;

    unsigned int codepoint = 0, next_codepoint = 0, last_codepoint = 0,
        last_renderable_codepoint = 0, last_breakable_codepoint = 0;
    while (text[index] != '\0') {
        // We're following the 2003 UTF-8 definition:
        // 0b0xxxxxxx
        // 0b110xxxxx 0b10xxxxxx
        // 0b1110xxxx 0b10xxxxxx 0b10xxxxxx
        // 0b11110xxx 0b10xxxxxx 0b10xxxxxx 0b10xxxxxx
        if (text[index] == '\n'
                && (char_origin.x + (__CODEPOINT_NEEDS_HYPHEN_AFTER(codepoint) ? hyphen->advance : 0)
                        <= box.origin.x + box.size.w)) {
            n_graphics_prv_draw_text_line(ctx, text,
                line_begin, index, font, line_origin);
            char_origin.x = box.origin.x, char_origin.y += line_height;
            last_breakable_index = last_renderable_index = -1;
            line_origin = char_origin;
            index = next_index = index + 1;
            line_begin = index;
            continue;
        }

        if (text[index] & 0b10000000) { // begin of multibyte character
            if ((text[index] & 0b11100000) == 0b11000000) {
                next_codepoint = ((text[index  ] &  0b11111) << 6)
                               +  (text[index+1] & 0b111111);
                next_index += 2;
            } else if ((text[index] & 0b11110000) == 0b11100000) {
                next_codepoint = ((text[index  ] &   0b1111) << 12)
                               + ((text[index+1] & 0b111111) << 6)
                               +  (text[index+2] & 0b111111);
                next_index += 3;
            } else if ((text[index] & 0b11111000) == 0b11110000) {
                next_codepoint = ((text[index  ] &    0b111) << 18)
                               + ((text[index+1] & 0b111111) << 12)
                               + ((text[index+2] & 0b111111) << 6)
                               +  (text[index+3] & 0b111111);
                next_index += 4;
            } else {
                next_codepoint = 0;
                next_index += 1;
            }
        } else {
            next_codepoint = text[index];
            next_index += 1;
        }
        n_GGlyphInfo * next_glyph = n_graphics_font_get_glyph_info(font, next_codepoint);


        // Debugging:
        // n_graphics_context_set_text_color(ctx, n_GColorLightGray);
        // n_graphics_font_draw_glyph(ctx, next_glyph, char_origin);
        // n_graphics_context_set_text_color(ctx, n_GColorBlack);

        // We now know what codepoint the next character has.

        if (glyph) {
            if (__CODEPOINT_ALLOW_PREBREAKABLE(codepoint)) {
                if (char_origin.x +
                        (__CODEPOINT_NEEDS_HYPHEN_AFTER(last_codepoint)
                            ? hyphen->advance : 0)
                        <= box.origin.x + box.size.w) {
                    last_renderable_index = index;
                    last_renderable_codepoint = codepoint;
                }
            }
            if (__CODEPOINT_GOOD_POSTBREAKABLE(codepoint) &&
                    ((
                        (__CODEPOINT_IGNORE_AT_LINE_END(codepoint) &&
                        char_origin.x - glyph->advance <= box.origin.x + box.size.w) ||
                    char_origin.x <= box.origin.x + box.size.w))) {
                last_breakable_index = index;
                last_breakable_codepoint = codepoint;
            }
        }

        // Done processing the two available characters.

        index = next_index;
        last_codepoint = codepoint;
        codepoint = next_codepoint;
        glyph = next_glyph;
        char_origin.x += glyph->advance;

        if ((char_origin.x + (__CODEPOINT_NEEDS_HYPHEN_AFTER(codepoint) ? hyphen->advance : 0) - lenience
                > box.origin.x + box.size.w)) {
            if (last_breakable_index > 0) {
                switch (alignment) {
                    case n_GTextAlignmentCenter:
                        n_graphics_prv_draw_text_line(ctx, text,
                            line_begin, last_breakable_index, font,
                            n_GPoint(line_origin.x + (box.size.w - (char_origin.x - line_origin.x))/2,
                                     line_origin.y));
                        break;
                    case n_GTextAlignmentRight:
                        n_graphics_prv_draw_text_line(ctx, text,
                            line_begin, last_breakable_index, font,
                            n_GPoint(line_origin.x + box.size.w - (char_origin.x - line_origin.x),
                                     line_origin.y));
                        break;
                    default:
                        n_graphics_prv_draw_text_line(ctx, text,
                            line_begin, last_breakable_index, font, line_origin);
                }
                index = next_index = last_breakable_index;
                char_origin.x = box.origin.x, char_origin.y += line_height;
                line_begin = last_breakable_index;
                last_breakable_index = last_renderable_index = -1;
                line_origin = char_origin;
            } else if (last_renderable_index > 0) {
                n_GPoint end;
                switch (alignment) {
                    case n_GTextAlignmentCenter:
                        end = n_graphics_prv_draw_text_line(ctx, text,
                            line_begin, last_renderable_index, font,
                            n_GPoint(line_origin.x + (box.size.w - (char_origin.x - line_origin.x))/2,
                                     line_origin.y));
                        break;
                    case n_GTextAlignmentRight:
                        end = n_graphics_prv_draw_text_line(ctx, text,
                            line_begin, last_renderable_index, font,
                            n_GPoint(line_origin.x + box.size.w - (char_origin.x - line_origin.x),
                                     line_origin.y));
                        break;
                    default:
                        end = n_graphics_prv_draw_text_line(ctx, text,
                            line_begin, last_renderable_index, font, line_origin);
                }
                if (__CODEPOINT_NEEDS_HYPHEN_AFTER(last_renderable_codepoint) || true) { // TODO
                    n_graphics_font_draw_glyph(ctx, hyphen, end);
                }
                index = next_index = last_renderable_index;
                char_origin.x = box.origin.x, char_origin.y += line_height;
                line_begin = last_renderable_index;
                last_breakable_index = last_renderable_index = -1;
                line_origin = char_origin;
            } else {
                n_graphics_font_draw_glyph(ctx, hyphen, line_origin);
                line_begin = next_index;
                char_origin.x = box.origin.x, char_origin.y += line_height;
                line_origin = char_origin;
            }
            if (line_origin.y + line_height >= box.origin.y + box.size.h) {
                return;
            }
        }
        index += (0 * line_begin * last_breakable_codepoint);
    }
    if (index != line_begin) {
        switch (alignment) {
            case n_GTextAlignmentCenter:
                n_graphics_prv_draw_text_line(ctx, text,
                    line_begin, index, font,
                    n_GPoint(line_origin.x + (box.size.w - (char_origin.x - line_origin.x))/2,
                             line_origin.y));
                break;
            case n_GTextAlignmentRight:
                n_graphics_prv_draw_text_line(ctx, text,
                    line_begin, index, font,
                    n_GPoint(line_origin.x + box.size.w - (char_origin.x - line_origin.x),
                             line_origin.y));
                break;
            case n_GTextAlignmentLeft:
            default:
                n_graphics_prv_draw_text_line(ctx, text,
                    line_begin, index, font, line_origin);
        }
    }
}
