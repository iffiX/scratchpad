#ifndef SCRATCHPAD_H
#define SCRATCHPAD_H

#include <tuple>
#include <vector>
#include <type_traits>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include "mypaint-all.h"

namespace py = pybind11;

struct Setting {
    // a subset of all libmypaint supported settings
    float opacity;
    float radius;
    float hardness;
    float color_h;
    float color_s;
    float color_v;

    explicit Setting(float opacity = 0.0,
                     float radius = 0.5,
                     float hardness = 0.8,
                     float color_h = 0.0,
                     float color_s = 0.0,
                     float color_v = 0.0) :
            opacity(opacity), radius(radius), hardness(hardness),
            color_h(color_h), color_s(color_s), color_v(color_v) {}
};

struct Point {
    float x;
    float y;
    float xtilt;
    float ytilt;
    float pressure;
    float dtime;

    explicit Point(float x = 0.0, float y = 0.0,
                   float xtilt = 0.0, float ytilt = 0.0,
                   float pressure = 0.0, float dtime = 0.1) :
            x(x), y(y), xtilt(xtilt), ytilt(ytilt),
            pressure(pressure), dtime(dtime) {}
};

class ScratchPad {
public:
    ~ScratchPad();

    void loadBrush(const std::string &brush_string);

    void resetPad(int width, int height, int layers = 1);

    void addLayer();

    void popLayer(int layer);

    void setOpacity(int layer, float opacity);

    int getBrushNum();

    int getLayerNum();

    std::tuple<int, int> getPadSize();

    void draw(int layer, int brush, const Setting &setting,
              const std::vector<Point> &points);

    py::array renderLayer(int layer, const py::dtype &dtype = "f4");

    py::array render(const py::dtype &dtype = "f4");

private:
    int _width = 0, _height = 0;
    std::vector<MyPaintBrush *> _brushes;
    std::vector<MyPaintFixedTiledSurface *> _layers;
    std::vector<float> _layer_opacity;

    py::array &&_convertFix15(const uint16_t *in_layer, const py::dtype &dtype);

    template<typename T, std::enable_if_t<std::is_floating_point<T>::value, int> = 0>
    void _convertFix15ToFloat(const uint16_t *in_layer, T *out_layer, int pixel_num);

    template<typename T, std::enable_if_t<std::is_integral<T>::value, int> = 0>
    void _convertFix15ToInt(const uint16_t *in_layer, T *out_layer, int pixel_num);

    void _blend(uint16_t *layer_a, uint16_t *layer_b, uint16_t *out_layer,
                float layer_b_opacity, int pixel_num);
};

#endif //SCRATCHPAD_H