// Based on https://harfbuzz.github.io/ch03s03.html

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include <hb.h>
#include <hb-ft.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "./stb_image_write.h"

void destroy_or_whatever(void *user_data)
{
}

typedef struct {
    uint8_t r, g, b, a;
} Pixel32;

typedef struct {
    size_t width;
    size_t height;
    Pixel32 *pixels;
} Image32;

void save_image32_to_png(Image32 image, const char *filepath)
{
    int ok = stbi_write_png(filepath, image.width, image.height, 4,
                            image.pixels, image.width * sizeof(Pixel32));

    if (!ok) {
        fprintf(stderr, "Could not save file `%s`\n", filepath);
        exit(1);
    }
}

Pixel32 mix_pixels(Pixel32 dst, Pixel32 src)
{
    uint8_t rev_src_a = 255 - src.a;
    Pixel32 result;
    result.r = ((uint16_t) src.r * (uint16_t) src.a + (uint16_t) dst.r * rev_src_a) >> 8;
    result.g = ((uint16_t) src.g * (uint16_t) src.a + (uint16_t) dst.g * rev_src_a) >> 8;
    result.b = ((uint16_t) src.b * (uint16_t) src.a + (uint16_t) dst.b * rev_src_a) >> 8;
    result.a = dst.a;
    return result;
}

void slap_ftbitmap_onto_image32(Image32 dest,
                                FT_Bitmap *src,
                                Pixel32 color,
                                int x, int y)
{
    assert(src->pixel_mode == FT_PIXEL_MODE_GRAY);
    assert(src->num_grays == 256);

    for (size_t row = 0; (row < src->rows); ++row) {
        if (row + y < dest.height) {
            for (size_t col = 0; (col < src->width); ++col) {
                if (col + x < dest.width) {
                    color.a = src->buffer[row * src->pitch + col];
                    dest.pixels[(row + y) * dest.width + col + x] =
                        mix_pixels(
                            dest.pixels[(row + y) * dest.width + col + x],
                            color);
                }
            }
        }
    }
}

void draw_glyph(Image32 surface, FT_Face face, hb_codepoint_t glyphid,
                double x, double y)
{
    printf("Drawing glyph at (%lf, %lf)\n", x, y);

    FT_Error error = FT_Load_Glyph(face, glyphid, FT_LOAD_DEFAULT);
    if (error) {
        fprintf(stderr, "Could not load glyph (codepoint: %d)\n",
                glyphid);
        exit(1);
    }

    error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
    if (error) {
        fprintf(stderr, "Could not render glyph (codepoint: %d)\n",
                glyphid);
        exit(1);
    }

    Pixel32 FONT_COLOR = {200, 200, 200, 255};

    slap_ftbitmap_onto_image32(surface,
                               &face->glyph->bitmap,
                               FONT_COLOR,
                               (int) floor(x) + face->glyph->bitmap_left,
                               (int) floor(y) - face->glyph->bitmap_top);

}

int main(int argc, char *argv[])
{
    hb_buffer_t *buf = hb_buffer_create();
    hb_buffer_add_utf8(buf, "Привет, Мир!", -1, 0, -1);

    hb_buffer_set_direction(buf, HB_DIRECTION_TTB);
    hb_buffer_set_script(buf, HB_SCRIPT_CYRILLIC);
    // hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf, hb_language_from_string("ru", -1));

    FT_Library ft_library;
    FT_Error error = FT_Init_FreeType(&ft_library);
    if (error) {
        fprintf(stderr, "Could not initialize FreeType2\n");
        exit(1);
    }

    FT_Face face;
    const char *font_path = "./fonts-japanese-gothic.ttf";
    size_t index = 0;
    error = FT_New_Face(ft_library, font_path, index, &face);
    if (error) {
        fprintf(stderr, "Could not load font: %s\n", font_path);
        exit(1);
    }

    error = FT_Set_Char_Size(face, 0, 5000, 0, 0);
    if (error) {
        fprintf(stderr, "Could set char size\n");
        exit(1);
    }

    hb_font_t *font = hb_ft_font_create(face, destroy_or_whatever);

    hb_shape(font, buf, NULL, 0);

    unsigned int glyph_count = 0;
    printf("initial glyph_count = %d\n", glyph_count);
    hb_glyph_info_t *glyph_info =
        hb_buffer_get_glyph_infos(buf, &glyph_count);
    printf("glyph_count after infos = %d\n", glyph_count);
    hb_glyph_position_t *glyph_pos =
        hb_buffer_get_glyph_positions(buf, &glyph_count);
    printf("glyph_count after positions = %d\n", glyph_count);


    Image32 surface;
    surface.width = 1000;
    surface.height = 1000;
    surface.pixels = malloc(sizeof(Pixel32) * surface.width * surface.height);
    assert(surface.pixels);

    for (int y = 0; y < surface.height; ++y) {
        for (int x = 0; x < surface.width; ++x) {
            surface.pixels[y * surface.width + x] = (Pixel32) {30, 30, 30, 255};
        }
    }

    double cursor_x = 700;
    double cursor_y = 1000.0;
    for (unsigned int i = 0; i < glyph_count; ++i) {
        hb_codepoint_t glyphid = glyph_info[i].codepoint;
        double x_offset = glyph_pos[i].x_offset / 64.0;
        double y_offset = glyph_pos[i].y_offset / 64.0;
        double x_advance = glyph_pos[i].x_advance / 64.0;
        double y_advance = glyph_pos[i].y_advance / 64.0;
        draw_glyph(surface, face, glyphid, cursor_x + x_offset, cursor_y + y_offset);
        cursor_x += x_advance;
        cursor_y += y_advance;
    }

    const char *output_filepath = "output.png";
    printf("Generating %s\n", output_filepath);
    save_image32_to_png(surface, output_filepath);

    hb_buffer_destroy(buf);
    hb_font_destroy(font);

    return 0;
}
