#include "canvas.h"

#include <cmath>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <locale>
#include <codecvt>

#include <iostream>

namespace lunasvg {

static plutovg_matrix_t to_plutovg_matrix(const Transform& transform);
static plutovg_fill_rule_t to_plutovg_fill_rule(WindRule winding);
static plutovg_operator_t to_plutovg_operator(BlendMode mode);
static plutovg_line_cap_t to_plutovg_line_cap(LineCap cap);
static plutovg_line_join_t to_plutovg_line_join(LineJoin join);
static plutovg_spread_method_t to_plutovg_spread_method(SpreadMethod spread);
static plutovg_texture_type_t to_plutovg_texture_type(TextureType type);
static void to_plutovg_stops(plutovg_gradient_t* gradient, const GradientStops& stops);
static void to_plutovg_path(plutovg_t* pluto, const Path& path);

std::shared_ptr<Canvas> Canvas::create(unsigned char* data, unsigned int width, unsigned int height, unsigned int stride)
{
    return std::shared_ptr<Canvas>(new Canvas(data, static_cast<int>(width), static_cast<int>(height), static_cast<int>(stride)));
}

std::shared_ptr<Canvas> Canvas::create(double x, double y, double width, double height)
{
    if(width <= 0.0 || height <= 0.0)
        return std::shared_ptr<Canvas>(new Canvas(0, 0, 1, 1));

    auto l = static_cast<int>(floor(x));
    auto t = static_cast<int>(floor(y));
    auto r = static_cast<int>(ceil(x + width));
    auto b = static_cast<int>(ceil(y + height));
    return std::shared_ptr<Canvas>(new Canvas(l, t, r - l, b - t));
}

std::shared_ptr<Canvas> Canvas::create(const Rect& box)
{
    return create(box.x, box.y, box.w, box.h);
}

Canvas::Canvas(unsigned char* data, int width, int height, int stride)
{
    surface = plutovg_surface_create_for_data(data, width, height, stride);
    pluto = plutovg_create(surface);
    plutovg_matrix_init_identity(&translation);
    plutovg_rect_init(&rect, 0, 0, width, height);
}

Canvas::Canvas(int x, int y, int width, int height)
{
    surface = plutovg_surface_create(width, height);
    pluto = plutovg_create(surface);
    plutovg_matrix_init_translate(&translation, -x, -y);
    plutovg_rect_init(&rect, x, y, width, height);
}

Canvas::~Canvas()
{
    plutovg_surface_destroy(surface);
    plutovg_destroy(pluto);
}

void Canvas::setColor(const Color& color)
{
    plutovg_set_rgba(pluto, color.red() / 255.0, color.green() / 255.0, color.blue() / 255.0, color.alpha() / 255.0);
}

void Canvas::setLinearGradient(double x1, double y1, double x2, double y2, const GradientStops& stops, SpreadMethod spread, const Transform& transform)
{
    auto gradient = plutovg_set_linear_gradient(pluto, x1, y1, x2, y2);
    auto matrix = to_plutovg_matrix(transform);
    to_plutovg_stops(gradient, stops);
    plutovg_gradient_set_spread(gradient, to_plutovg_spread_method(spread));
    plutovg_gradient_set_matrix(gradient, &matrix);
}

void Canvas::setRadialGradient(double cx, double cy, double r, double fx, double fy, const GradientStops& stops, SpreadMethod spread, const Transform& transform)
{
    auto gradient = plutovg_set_radial_gradient(pluto, cx, cy, r, fx, fy, 0);
    auto matrix = to_plutovg_matrix(transform);
    to_plutovg_stops(gradient, stops);
    plutovg_gradient_set_spread(gradient, to_plutovg_spread_method(spread));
    plutovg_gradient_set_matrix(gradient, &matrix);
}

void Canvas::setTexture(const Canvas* source, TextureType type, const Transform& transform)
{
    auto texture = plutovg_set_texture(pluto, source->surface, to_plutovg_texture_type(type));
    auto matrix = to_plutovg_matrix(transform);
    plutovg_texture_set_matrix(texture, &matrix);
}

void Canvas::fill(const Path& path, const Transform& transform, WindRule winding, BlendMode mode, double opacity)
{
    auto matrix = to_plutovg_matrix(transform);
    plutovg_matrix_multiply(&matrix, &matrix, &translation);
    to_plutovg_path(pluto, path);
    plutovg_set_matrix(pluto, &matrix);
    plutovg_set_fill_rule(pluto, to_plutovg_fill_rule(winding));
    plutovg_set_opacity(pluto, opacity);
    plutovg_set_operator(pluto, to_plutovg_operator(mode));
    plutovg_fill(pluto);
}

void Canvas::stroke(const Path& path, const Transform& transform, double width, LineCap cap, LineJoin join, double miterlimit, const DashData& dash, BlendMode mode, double opacity)
{
    auto matrix = to_plutovg_matrix(transform);
    plutovg_matrix_multiply(&matrix, &matrix, &translation);
    to_plutovg_path(pluto, path);
    plutovg_set_matrix(pluto, &matrix);
    plutovg_set_line_width(pluto, width);
    plutovg_set_line_cap(pluto, to_plutovg_line_cap(cap));
    plutovg_set_line_join(pluto, to_plutovg_line_join(join));
    plutovg_set_miter_limit(pluto, miterlimit);
    plutovg_set_dash(pluto, dash.offset, dash.array.data(), static_cast<int>(dash.array.size()));
    plutovg_set_operator(pluto, to_plutovg_operator(mode));
    plutovg_set_opacity(pluto, opacity);
    plutovg_stroke(pluto);
}

void Canvas::blend(const Canvas* source, BlendMode mode, double opacity)
{
    plutovg_set_texture_surface(pluto, source->surface, source->rect.x, source->rect.y);
    plutovg_set_operator(pluto, to_plutovg_operator(mode));
    plutovg_set_opacity(pluto, opacity);
    plutovg_set_matrix(pluto, &translation);
    plutovg_paint(pluto);
}

void Canvas::mask(const Rect& clip, const Transform& transform)
{
    auto matrix = to_plutovg_matrix(transform);
    auto path = plutovg_path_create();
    plutovg_path_add_rect(path, clip.x, clip.y, clip.w, clip.h);
    plutovg_path_transform(path, &matrix);
    plutovg_rect(pluto, rect.x, rect.y, rect.w, rect.h);
    plutovg_add_path(pluto, path);
    plutovg_path_destroy(path);

    plutovg_set_rgba(pluto, 0, 0, 0, 0);
    plutovg_set_fill_rule(pluto, plutovg_fill_rule_even_odd);
    plutovg_set_operator(pluto, plutovg_operator_src);
    plutovg_set_opacity(pluto, 0.0);
    plutovg_set_matrix(pluto, &translation);
    plutovg_fill(pluto);
}

void draw_bitmap(unsigned char **canvas, int width, int height, FT_Bitmap *bitmap, FT_Int x, FT_Int y) {
    FT_Int i, j, p, q;
    FT_Int x_max = x + bitmap->width;
    FT_Int y_max = y + bitmap->rows;

    for (i = x, p = 0; i < x_max; i++, p++) {
        for (j = y, q = 0; j < y_max; j++, q++) {
            if (i < 0 || j < 0 || i >= width || j >= height)
                continue;

            canvas[j][i] |= bitmap->buffer[q * bitmap->width + p];
        }
    }
}

void show_image(unsigned char** canvas, int width, int height) {
    int i, j;

    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++)
            putchar(canvas[i][j] == 0 ? ' ' : canvas[i][j] < 128 ? '+' : '*');
        putchar('\n');
    }
}

void Canvas::text(const double x, const double y, const std::string text)
{
    //TODO TRANSFORME VALUES FOR RESOLUTIONS
    auto width = plutovg_surface_get_width(surface);
    auto height = plutovg_surface_get_height(surface);
    auto data = plutovg_surface_get_data(surface);

    auto stride = plutovg_surface_get_stride(surface);
    std::cout << "Stride : " << stride << std::endl;

    std::cout << "X : " << x << std::endl;
    std::cout << "Y : " << y << std::endl;
    std::cout << "Text : " << text << std::endl;

    //std::cout << "Width : " << width << std::endl;
    //std::cout << "Height : " << height << std::endl;

    FT_Error error;
    FT_Library library;
    FT_Face face;
    FT_GlyphSlot slot;

    const int FONT_SIZE = 12;

    const char *filename = "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf";

    error = FT_Init_FreeType(&library);
    if (error) {
        std::cout << "Error of FT_Init_FreeType : " << error << std::endl;
        exit(1);
    }
    error = FT_New_Face(library, filename, 0, &face);
    if (error) {
        std::cout << "Error of FT_New_Face : " << error << std::endl;
        exit(1);
    }
    error = FT_Set_Char_Size(face, FONT_SIZE * 64, 0, 100, 0);
    if (error) {
        std::cout << "Error of FT_Set_Char_Size : " << error << std::endl;
        exit(1);
    }

    // Convert std::string to std::wstring
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring myWString = converter.from_bytes(text);

    const int num_chars = myWString.size();

    std::cout << "Number of chars : " << num_chars << std::endl;

    // Display the result
    for(int i = 0; i < myWString.size(); i++) {
        std::cout << "Char start : " << myWString[i] << std::endl;
    }

    slot = face->glyph;

    int total_width = 0;
    int max_height =  0;

    for (int n = 0; n < num_chars; n++) {
        error = FT_Load_Char(face, myWString[n], FT_LOAD_RENDER);
        if (error) {
            std::cout << "Error of FT_Load_Char : " << error << std::endl;
            exit(1);
        }

        total_width += slot->advance.x >> 6;

        if(slot->bitmap.rows > (unsigned int) max_height)
            max_height = slot->bitmap.rows;
    }

    int width_canvas = total_width;
    int height_canvas = max_height;

    std::cout << "Width canvas : " << width_canvas << std::endl;
    std::cout << "Height canvas : " << height_canvas << std::endl;

    // Allocate memory for the image array
    unsigned char **text_canvas = (unsigned char **)malloc(height_canvas * sizeof(unsigned char *));
    for (int i = 0; i < height_canvas; i++)
        text_canvas[i] = (unsigned char *)calloc(width_canvas, sizeof(unsigned char));

    int pen_x = 0;
    int pen_y = max_height;

    for (int n = 0; n < num_chars; n++) {
        error = FT_Load_Char(face, myWString[n], FT_LOAD_RENDER);
        if (error) {
            std::cout << "Error of FT_Load_Char : " << error << std::endl;
            exit(1);
        }
        draw_bitmap(text_canvas, width_canvas, height_canvas, &slot->bitmap, pen_x + slot->bitmap_left, pen_y - slot->bitmap_top);
        pen_x += slot->advance.x >> 6;
    }

    //TODO OVERFLOW BORDER, FIX THIS
    show_image(text_canvas, width_canvas, height_canvas);

    int round_x = (int)round(x);
    int round_y = (int)round(y) - 12;

    for (int i = 0; i < height_canvas; i++) {
        for(int j = 0; j < width_canvas; j++) {
            if (text_canvas[i][j] == 0 || (i + round_y) < 0 || (i + round_y) >= height || (j + round_x) < 0 || (j + round_x) >= width)
                continue;


            int index = ((i + round_y) * width + (j + round_x)) * 4;
            data[index] = 255 - text_canvas[i][j];
            data[index + 1] = 255 - text_canvas[i][j];
            data[index + 2] = 255 - text_canvas[i][j];
            data[index + 3] = 255;
        }
    }

    // Delete the array created
    for (int i = 0; i < height_canvas; i++)                       // arrays
        free(text_canvas[i]);
    free(text_canvas);

    FT_Done_Face(face);
    FT_Done_FreeType(library);
}

void Canvas::luminance()
{
    auto width = plutovg_surface_get_width(surface);
    auto height = plutovg_surface_get_height(surface);
    auto stride = plutovg_surface_get_stride(surface);
    auto data = plutovg_surface_get_data(surface);
    for(int y = 0; y < height; y++) {
        auto pixels = reinterpret_cast<uint32_t*>(data + stride * y);
        for(int x = 0; x < width; x++) {
            auto pixel = pixels[x];
            auto r = (pixel >> 16) & 0xFF;
            auto g = (pixel >> 8) & 0xFF;
            auto b = (pixel >> 0) & 0xFF;
            auto l = (2*r + 3*g + b) / 6;

            pixels[x] = l << 24;
        }
    }
}

unsigned int Canvas::width() const
{
    return plutovg_surface_get_width(surface);
}

unsigned int Canvas::height() const
{
    return plutovg_surface_get_height(surface);
}

unsigned int Canvas::stride() const
{
    return plutovg_surface_get_stride(surface);
}

unsigned char* Canvas::data() const
{
    return plutovg_surface_get_data(surface);
}

Rect Canvas::box() const
{
    return Rect(rect.x, rect.y, rect.w, rect.h);
}

plutovg_matrix_t to_plutovg_matrix(const Transform& transform)
{
    plutovg_matrix_t matrix;
    plutovg_matrix_init(&matrix, transform.m00, transform.m10, transform.m01, transform.m11, transform.m02, transform.m12);
    return matrix;
}

plutovg_fill_rule_t to_plutovg_fill_rule(WindRule winding)
{
    return winding == WindRule::EvenOdd ? plutovg_fill_rule_even_odd : plutovg_fill_rule_non_zero;
}

plutovg_operator_t to_plutovg_operator(BlendMode mode)
{
    return mode == BlendMode::Src ? plutovg_operator_src : mode == BlendMode::Src_Over ? plutovg_operator_src_over : mode == BlendMode::Dst_In ? plutovg_operator_dst_in : plutovg_operator_dst_out;
}

plutovg_line_cap_t to_plutovg_line_cap(LineCap cap)
{
    return cap == LineCap::Butt ? plutovg_line_cap_butt : cap == LineCap::Round ? plutovg_line_cap_round : plutovg_line_cap_square;
}

plutovg_line_join_t to_plutovg_line_join(LineJoin join)
{
    return join == LineJoin::Miter ? plutovg_line_join_miter : join == LineJoin::Round ? plutovg_line_join_round : plutovg_line_join_bevel;
}

static plutovg_spread_method_t to_plutovg_spread_method(SpreadMethod spread)
{
    return spread == SpreadMethod::Pad ? plutovg_spread_method_pad : spread == SpreadMethod::Reflect ? plutovg_spread_method_reflect : plutovg_spread_method_repeat;
}

static plutovg_texture_type_t to_plutovg_texture_type(TextureType type)
{
    return type == TextureType::Plain ? plutovg_texture_type_plain : plutovg_texture_type_tiled;
}

static void to_plutovg_stops(plutovg_gradient_t* gradient, const GradientStops& stops)
{
    for(const auto& stop : stops) {
        auto offset = std::get<0>(stop);
        auto& color = std::get<1>(stop);
        plutovg_gradient_add_stop_rgba(gradient, offset, color.red() / 255.0, color.green() / 255.0, color.blue() / 255.0, color.alpha() / 255.0);
    }
}

void to_plutovg_path(plutovg_t* pluto, const Path& path)
{
    PathIterator it(path);
    std::array<Point, 3> p;
    while(!it.isDone()) {
        switch(it.currentSegment(p)) {
        case PathCommand::MoveTo:
            plutovg_move_to(pluto, p[0].x, p[0].y);
            break;
        case PathCommand::LineTo:
            plutovg_line_to(pluto, p[0].x, p[0].y);
            break;
        case PathCommand::CubicTo:
            plutovg_cubic_to(pluto, p[0].x, p[0].y, p[1].x, p[1].y, p[2].x, p[2].y);
            break;
        case PathCommand::Close:
            plutovg_close_path(pluto);
            break;
        }

        it.next();
    }
}

} // namespace lunasvg
