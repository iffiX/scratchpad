#include "scratchpad.h"
#include "fix15.h"
#include "util.h"
#include <fmt/format.h>
#include <cmath>
#include <cstdlib>
#include <stdexcept>


#define CONVERT_AND_RETURN_F(type) \
    auto tmp = new type[r_w * r_h * 4];\
    void* result =  malloc(sizeof(type) * r_w * r_h * 4);\
    if (result == NULL)\
        throw std::bad_alloc();\
    _convertFix15ToFloat<type>(in_layer, tmp, r_w * r_h);\
    _reformat<type>(tmp, static_cast<type*>(result), r_w, r_h, MYPAINT_TILE_SIZE);\
    delete [] tmp;\
    return result


#define CONVERT_AND_RETURN_I(type) \
    auto tmp = new type[r_w * r_h * 4];\
    void* result =  malloc(sizeof(type) * r_w * r_h * 4);\
    if (result == NULL)\
        throw std::bad_alloc();\
    _convertFix15ToInt<type>(in_layer, tmp, r_w * r_h);\
    _reformat<type>(tmp, static_cast<type*>(result), r_w, r_h, MYPAINT_TILE_SIZE);\
    delete [] tmp;\
    return result


ScratchPad::ScratchPad(const ScratchPad &pad)
: _width(pad._width), _height(pad._width),
  _brushes(pad._brushes), _layers(pad._layers),
  _layer_opacity(pad._layer_opacity) {
    std::cout << "Copy called!" << std::endl;
    for (auto brush: _brushes)
        mypaint_brush_ref(brush);
    for (auto layer: _layers)
        mypaint_surface_ref(mypaint_fixed_tiled_surface_interface(layer));
}

ScratchPad::ScratchPad(ScratchPad &&pad) noexcept {
    std::cout << "Move called!" << std::endl;
    _width = pad._width;
    _height = pad._height;
    _brushes.swap(pad._brushes);
    _layers.swap(pad._layers);
    _layer_opacity.swap(pad._layer_opacity);
}

ScratchPad::~ScratchPad() {
    // will destroy instances completely if ref count > 1
    for (auto brush: _brushes)
        mypaint_brush_unref(brush);
    for (auto layer: _layers)
        mypaint_surface_unref(mypaint_fixed_tiled_surface_interface(layer));
}

void ScratchPad::loadBrush(const std::string &brush_string) {
    MyPaintBrush *brush = mypaint_brush_new();
    if (brush == NULL)
        throw std::bad_alloc();
    if (mypaint_brush_from_string(brush, brush_string.c_str()) == FALSE)
        throw std::invalid_argument("Failed to create brush from string");
    _brushes.push_back(brush);
}

void ScratchPad::resetPad(int width, int height, int layers) {
    if (width < 0 || height < 0 || layers < 0)
        throw std::invalid_argument(
                "Invalid pad configuration, requirements are: width > 0, "
                "height > 0, layers > 0."
        );
    _width = width;
    _height = height;
    // destroy existing layers
    for (auto layer: _layers)
        mypaint_surface_unref(mypaint_fixed_tiled_surface_interface(layer));
    _layers.clear();
    for (int i = 0; i < layers; i++)
        addLayer();
}

void ScratchPad::addLayer() {
    MyPaintFixedTiledSurface *layer = mypaint_fixed_tiled_surface_new(_width, _height);
    if (layer == NULL)
        throw std::bad_alloc();
    _layers.push_back(layer);
    _layer_opacity.push_back(1.0);
}

void ScratchPad::popLayer(int layer) {
    if (layer >= _layers.size() or layer < 0)
        throw std::out_of_range(fmt::format("Invalid layer index {}", layer));
    MyPaintFixedTiledSurface *layer_ptr = _layers[layer];
    mypaint_surface_unref(mypaint_fixed_tiled_surface_interface(layer_ptr));
    _layers.erase(_layers.begin() + layer);
    _layer_opacity.erase(_layer_opacity.begin() + layer);
}

void ScratchPad::setOpacity(int layer, float opacity) {
    if (layer >= _layers.size() or layer < 0)
        throw std::out_of_range(fmt::format("Invalid layer index {}", layer));
    if (opacity < 0 || opacity > 1)
        throw std::invalid_argument("Opacity must be within range [0, 1]!");
    _layer_opacity[layer] = opacity;
}

int ScratchPad::getBrushNum() {
    return _brushes.size();
}

int ScratchPad::getLayerNum() {
    return _layers.size();
}

std::tuple<int, int> ScratchPad::getPadSize() {
    return std::make_tuple(_width, _height);
}

void ScratchPad::draw(int layer, int brush, const Setting &setting,
                      const std::vector<Point> &points) {
    if (layer >= _layers.size() or layer < 0)
        throw std::out_of_range(fmt::format("Invalid layer index {}", layer));
    if (brush >= _brushes.size() or brush < 0)
        throw std::out_of_range(fmt::format("Invalid brush index {}", brush));

    auto layer_ptr = (MyPaintSurface*)_layers[layer];
    auto brush_ptr = _brushes[brush];

    // apply brush settings
    if (IN_RANGE(setting.opacity, 0, 1)
        and IN_RANGE(setting.radius, 0, 1)
        and IN_RANGE(setting.hardness, 0, 1)
        and IN_RANGE(setting.color_h, 0, 1)
        and IN_RANGE(setting.color_s, 0, 1)
        and IN_RANGE(setting.color_v, 0, 1)) {
        // opacity is in [0, 2.0]
        mypaint_brush_set_base_value(brush_ptr, MYPAINT_BRUSH_SETTING_OPAQUE, setting.opacity * 2.0);
        // radius is in [-2.0, 6.0]
        mypaint_brush_set_base_value(brush_ptr, MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC, setting.radius * 8.0 - 2.0);
        // hardness is in [0.0, 1.0]
        mypaint_brush_set_base_value(brush_ptr, MYPAINT_BRUSH_SETTING_HARDNESS, setting.hardness);
        // hue is in [0.0, 1.0]
        mypaint_brush_set_base_value(brush_ptr, MYPAINT_BRUSH_SETTING_COLOR_H, setting.color_h);
        // saturation is in [-0.5, 1.5]
        mypaint_brush_set_base_value(brush_ptr, MYPAINT_BRUSH_SETTING_COLOR_S, setting.color_s * 2.0 - 0.5);
        // value is in [-0.5, 1.5]
        mypaint_brush_set_base_value(brush_ptr, MYPAINT_BRUSH_SETTING_COLOR_V, setting.color_v * 2.0 - 0.5);
    }
    else
        throw std::invalid_argument("Invalid setting value, all point values must be in range of 0.0 to 1.0!");

    // draw
    mypaint_surface_begin_atomic(layer_ptr);
    for (auto &point: points) {
        // check data validity
        if (IN_RANGE(point.x, 0, 1)
            and IN_RANGE(point.y, 0, 1)
            and IN_RANGE(point.xtilt, 0, 1)
            and IN_RANGE(point.ytilt, 0, 1)
            and IN_RANGE(point.pressure, 0, 1)
            and IN_RANGE(point.dtime, 0, 1)) {
            mypaint_brush_stroke_to(brush_ptr, layer_ptr,
                                    point.x * _width,
                                    point.y * _height,
                                    point.pressure,
                                    point.xtilt * 2 - 1,
                                    point.ytilt * 2 - 1,
                                    point.dtime * 0.1);
        } else
            throw std::invalid_argument("Invalid point value, all point values must be in range of 0.0 to 1.0!");
    }
    MyPaintRectangle roi;
    mypaint_surface_end_atomic(layer_ptr, &roi);
}

py::array ScratchPad::renderLayer(int layer, const py::object &dt) {
    auto dtype = py::dtype::from_args(dt);
    if (dtype.has_fields())
        throw std::invalid_argument("Only support rendering as a flat floating array or integral array!");
    auto kind = dtype.kind();
    auto item_size = dtype.itemsize();
    void *array;
    {
        py::gil_scoped_release release;
        array = renderLayer(layer, kind, item_size);
    }
    auto capsule = py::capsule(array, [](void *v) { free(v); });

    int real_width = ALIGN(_width, MYPAINT_TILE_SIZE);

    return std::move(py::array(dtype,
                               {_height, _width, 4},
                               {real_width * 4 * item_size, 4 * item_size, item_size},
                               array, capsule));
}

void* ScratchPad::renderLayer(int layer, char kind, int item_size) {
    if (layer >= _layers.size() or layer < 0)
        throw std::out_of_range(fmt::format("Invalid layer index {}", layer));

    // Start a new operation request
    MyPaintTileRequest request;

    // Note: we are not initializing the request for each tile in the surface
    // because in a fixed tiled surface
    // there is only one chunk of linear memory, with:
    // width aligned to CEIL_DIV(width, TILE_WIDTH) * TILE_WIDTH
    // height aligned to CEIL_DIV(height, TILE_HEIGHT) * TILE_HEIGHT
    // we just need this request to get the pointer to internal linear memory

    // Note: the linear memory is tile by tile, and not row by row! Therefore we need reformat
    // to change the memory layout.

    int real_width = ALIGN(_width, MYPAINT_TILE_SIZE);
    int real_height = ALIGN(_height, MYPAINT_TILE_SIZE);

    auto layer_ptr = (MyPaintTiledSurface*)_layers[layer];
    // request to operate on mipmap_level=0, tile with x=0, ty=0
    mypaint_tile_request_init(&request, 0, 0, 0, TRUE);
    mypaint_tiled_surface_tile_request_start(layer_ptr, &request);
    auto array = _convertFix15(request.buffer, kind, item_size, real_width, real_height);
    mypaint_tiled_surface_tile_request_end(layer_ptr, &request);
    return array;
}

py::array ScratchPad::render(const py::object &dt) {
    auto dtype = py::dtype::from_args(dt);
    if (dtype.has_fields())
        throw std::invalid_argument("Only support rendering as a flat floating array or integral array!");
    auto kind = dtype.kind();
    auto item_size = dtype.itemsize();
    void *array;
    {
        py::gil_scoped_release release;
        array = render(kind, item_size);
    }
    auto capsule = py::capsule(array, [](void *v) { free(v); });

    int real_width = ALIGN(_width, MYPAINT_TILE_SIZE);

    return std::move(py::array(dtype,
                               {_height, _width, 4},
                               {real_width * 4 * item_size, 4 * item_size, item_size},
                               array, capsule));
}

void* ScratchPad::render(char kind, int item_size) {
    // must have one or more layers
    if (_layers.empty())
        throw std::out_of_range("Layers are empty!");

    // copy the first layer to ping-pong buffer.
    MyPaintTileRequest first_request;

    int real_width = ALIGN(_width, MYPAINT_TILE_SIZE);
    int real_height = ALIGN(_height, MYPAINT_TILE_SIZE);

    auto buffer_1 = new uint16_t[real_width * real_height * 4];
    auto buffer_2 = new uint16_t[real_width * real_height * 4];

    // ping-pong buffer, when false, buffer_2 is output, when true, buffer_1 is output
    bool ping_pong = false;

    mypaint_tile_request_init(&first_request, 0, 0, 0, TRUE);
    mypaint_tiled_surface_tile_request_start((MyPaintTiledSurface *) _layers[0], &first_request);
    memcpy(buffer_1, first_request.buffer, real_width * real_height * 4 * sizeof(uint16_t));
    mypaint_tiled_surface_tile_request_end((MyPaintTiledSurface *) _layers[0], &first_request);

    for (size_t i = 1; i < _layers.size(); i++) {
        // Blend current top layer with blended result
        MyPaintTileRequest request;
        auto layer_ptr = (MyPaintTiledSurface *) _layers[i];
        mypaint_tile_request_init(&request, 0, 0, 0, TRUE);
        mypaint_tiled_surface_tile_request_start(layer_ptr, &request);

        if (ping_pong)
            _blend(request.buffer, buffer_2, buffer_1, _layer_opacity[i], real_width * real_height);
        else
            _blend(request.buffer, buffer_1, buffer_2, _layer_opacity[i], real_width * real_height);

        mypaint_tiled_surface_tile_request_end(layer_ptr, &request);
        ping_pong = !ping_pong;
    }

    // Note: ping_pong is inverted here because of ping_pong = !ping_pong
    if (ping_pong) {
        delete [] buffer_1;
        auto array = _convertFix15(buffer_2, kind, item_size, real_width, real_height);
        delete [] buffer_2;
        return array;
    }
    else {
        delete [] buffer_2;
        auto array = _convertFix15(buffer_1, kind, item_size, real_width, real_height);
        delete [] buffer_1;
        return array;
    }
}

void* ScratchPad::_convertFix15(const uint16_t *in_layer, char kind, int item_size, int r_w, int r_h) {
    if (kind == 'f') {
        if (item_size == 4) {
            CONVERT_AND_RETURN_F(float);
        }
        else if (item_size == 8) {
            CONVERT_AND_RETURN_F(double);
        }
        else
            throw std::invalid_argument("Only float32 and float64 are supported in all floating types!");
    }
    else if (kind == 'B') {
        CONVERT_AND_RETURN_I(uint8_t);
    }
    else if (kind == 'i') {
        if (item_size == 2) {
            CONVERT_AND_RETURN_I(int16_t);
        }
        else if (item_size == 4) {
            CONVERT_AND_RETURN_I(int32_t);
        }
        else if (item_size == 8) {
            CONVERT_AND_RETURN_I(int64_t);
        }
        else
            throw std::invalid_argument("Only int16, int32, int64, uint8, uint16, uint32, uint64 are supported "
                                        "in all integral types!");
    }
    else if (kind == 'u') {
        if (item_size == 1) {
            CONVERT_AND_RETURN_I(uint8_t);
        }
        else if (item_size == 2) {
            CONVERT_AND_RETURN_I(uint16_t);
        }
        else if (item_size == 4) {
            CONVERT_AND_RETURN_I(uint32_t);
        }
        else if (item_size == 8) {
            CONVERT_AND_RETURN_I(uint64_t);
        }
        else
            throw std::invalid_argument("Only int16, int32, int64, uint8, uint16, uint32, uint64 are supported "
                                        "in all integral types!");
    }
    else
        throw std::invalid_argument("Only floating types and integral are supported!");
}

template<typename T>
void ScratchPad::_reformat(T *in_layer, T *out_layer, int r_w, int r_h, int tile_size) {
    int stride = tile_size * tile_size * 4;
    int total_size = r_w * r_h * 4;
    int tile_cols = r_w / tile_size;

    #pragma omp parallel for
    for(int offset = 0; offset < total_size; offset += stride) {
        int t_id = offset / stride;
        int g_row = (t_id / tile_cols) * tile_size;
        int g_col = (t_id % tile_cols) * tile_size;

        for(int t_row=0; t_row < tile_size; t_row++) {
            int in_offset = offset + t_row * tile_size * 4;
            int out_offset = (r_w * (g_row + t_row) + g_col) * 4;
            memcpy(out_layer + out_offset, in_layer + in_offset, tile_size * sizeof(T) * 4);
        }
    }
}

template<typename T, std::enable_if_t<std::is_floating_point<T>::value, int>>

void ScratchPad::_convertFix15ToFloat(const uint16_t *in_layer, T *out_layer, int pixel_num) {
    // pixel_num = w * h
    uint32_t r, g, b, a;

    int max = pixel_num * 4;
    #pragma omp parallel for
    for (int offset = 0; offset < max; offset += 4) {
        r = in_layer[offset];
        g = in_layer[offset + 1];
        b = in_layer[offset + 2];
        a = in_layer[offset + 3];

        // un-premultiply alpha (with rounding)
        if (a != 0) {
            r = ((r << 15u) + a / 2) / a;
            g = ((g << 15u) + a / 2) / a;
            b = ((b << 15u) + a / 2) / a;
        } else {
            r = g = b = 0;
        }

        // convert to destination floating point format, in range [0, 1]
        out_layer[offset] = r / T(1u << 15u);
        out_layer[offset + 1] = g / T(1u << 15u);
        out_layer[offset + 2] = b / T(1u << 15u);

        // alpha needs to be divided by 2
        out_layer[offset + 3] = a / T(1u << 16u);
    }
}

template<typename T, std::enable_if_t<std::is_integral<T>::value, int>>

void ScratchPad::_convertFix15ToInt(const uint16_t *in_layer, T *out_layer, int pixel_num) {
    // pixel_num = w * h
    uint32_t r, g, b, a;

    int max = pixel_num * 4;
    #pragma omp parallel for private(r, g, b, a)
    for (int offset = 0; offset < max; offset += 4) {
        r = in_layer[offset];
        g = in_layer[offset + 1];
        b = in_layer[offset + 2];
        a = in_layer[offset + 3];

        // un-premultiply alpha (with rounding)
        if (a != 0) {
            r = ((r << 15u) + a / 2) / a;
            g = ((g << 15u) + a / 2) / a;
            b = ((b << 15u) + a / 2) / a;
        } else {
            r = g = b = 0;
        }

        // convert to destination integer format, in range [0, 255], with rounding
        out_layer[offset] = (r * 255 + (1u << 14u)) / (1u << 15u);
        out_layer[offset + 1] = (g * 255 + (1u << 14u)) / (1u << 15u);
        out_layer[offset + 2] = (b * 255 + (1u << 14u)) / (1u << 15u);

        // alpha needs to be divided by 2
        out_layer[offset + 3] = (a * 255 + (1u << 14u)) / (1u << 16u);
    }
}

void ScratchPad::_blend(uint16_t *layer_a, uint16_t *layer_b, uint16_t *out_layer,
                        float layer_a_opacity, int pixel_num) {
    // layer a is over layer b
    // see https://en.wikipedia.org/wiki/Alpha_compositing

    int max = pixel_num * 4;
    fix15_t a_opac = lroundf(layer_a_opacity * (1u << 15u));

    #pragma omp parallel for
    for (int i = 0; i < max; i += 4) {
        const fix15_t a_pix_opac = fix15_mul(layer_a[i + 3], a_opac);
        const fix15_t minus_opac = fix15_one - a_pix_opac;

        // Note: color channels are pre-multiplied with alpha!
        // Note: the value of all channels are within range [0, 1], in fixed point
        out_layer[i + 0] = fix15_sumprods(layer_a[i], a_opac, layer_b[i], minus_opac);
        out_layer[i + 1] = fix15_sumprods(layer_a[i + 1], a_opac, layer_b[i + 1], minus_opac);
        out_layer[i + 2] = fix15_sumprods(layer_a[i + 2], a_opac, layer_b[i + 2], minus_opac);
        out_layer[i + 3] = fix15_short_clamp(a_pix_opac + fix15_mul(layer_b[i + 3], minus_opac));
    }
}

