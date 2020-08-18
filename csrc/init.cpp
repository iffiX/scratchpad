#include "scratchpad.h"
#include "b_scratchpad.h"
#include <fmt/format.h>

PYBIND11_MODULE(scratchpad, m) {
    py::class_<Setting>(m,
                        "Setting",
                        R"(Settings of the used brush.)")
            .def(py::init<>())
            .def(py::init<float>())
            .def(py::init<float, float>())
            .def(py::init<float, float, float>())
            .def(py::init<float, float, float, float>())
            .def(py::init<float, float, float, float, float>())
            .def(py::init<float, float, float, float, float, float>())
            .def_readwrite("opacity",
                           &Setting::opacity,
                           R"(Brush opacity used in the draw call, in range [0, 1],
                              mapped to [0, 1])")
            .def_readwrite("radius",
                           &Setting::radius,
                           R"(Brush radius used in the draw call, in range [0, 1],
                              mapped to [-2, 6])")
            .def_readwrite("hardness",
                           &Setting::hardness,
                           R"(Brush hardness used in the draw call, in range [0, 1],
                              mapped to [0, 1])")
            .def_readwrite("color_h",
                           &Setting::color_h,
                           R"(Brush color hue used in the draw call, in range [0, 1],
                              mapped to [0, 1])")
            .def_readwrite("color_s",
                           &Setting::color_s,
                           R"(Brush color saturation used in the draw call, in range [0, 1],
                              mapped to [-0.5, 1.5])")
            .def_readwrite("color_v",
                           &Setting::color_v,
                           R"(Brush color value used in the draw call, in range [0, 1],
                              mapped to [-0.5, 1.5])")
            .def("__repr__",
                 [](const Setting &s) {
                     return fmt::format("Setting(opacity={}, radius={}, hardness={}, H={}, S={}, V={})",
                                        s.opacity, s.radius, s.hardness, s.color_h, s.color_s, s.color_v);
                 });

    py::class_<Point>(m,
                        "Point",
                        R"(A structure encapsulating data passed by various input devices such as mouse
                           and tablets.)")
            .def(py::init<>())
            .def(py::init<float>())
            .def(py::init<float, float>())
            .def(py::init<float, float, float>())
            .def(py::init<float, float, float, float>())
            .def(py::init<float, float, float, float, float>())
            .def(py::init<float, float, float, float, float, float>())
            .def_readwrite("x",
                           &Point::x,
                           R"(Point x used in the draw call, in range [0, 1],
                              mapped to [0, canvas width].)")
            .def_readwrite("y",
                           &Point::y,
                           R"(Point y used in the draw call, in range [0, 1],
                              mapped to [0, canvas height].)")
            .def_readwrite("xtilt",
                           &Point::xtilt,
                           R"(Point tilt in x axis used in the draw call, in range [0, 1],
                              mapped to [-1, 1].)")
            .def_readwrite("ytilt",
                           &Point::ytilt,
                           R"(Point tilt in y axis used in the draw call, in range [0, 1],
                              mapped to [-1, 1].)")
            .def_readwrite("pressure",
                           &Point::pressure,
                           R"(Point pressure used in the draw call, in range [0, 1],
                              mapped to [0, 1].)")
            .def_readwrite("dtime",
                           &Point::dtime,
                           R"(Time passed (in seconds) from last point to this point, in range [0, 1],
                              mapped to [0, 0.1].)")
            .def("__repr__",
                 [](const Point &p) {
                     return fmt::format("Point(x={}, y={}, xtilt={}, ytilt={}, pressure={}, dtime={})",
                                        p.x, p.y, p.xtilt, p.ytilt, p.pressure, p.dtime);
                 });

    py::class_<ScratchPad>(m, "ScratchPad")
            .def(py::init<>())
            .def("load_brush", &ScratchPad::loadBrush)
            .def("reset_pad", &ScratchPad::resetPad)
            .def("add_layer", &ScratchPad::addLayer)
            .def("pop_layer", &ScratchPad::popLayer)
            .def("set_opacity", &ScratchPad::setOpacity)
            .def("get_brush_num", &ScratchPad::getBrushNum)
            .def("get_layer_num", &ScratchPad::getLayerNum)
            .def("get_pad_size", &ScratchPad::getPadSize)
            .def("draw", &ScratchPad::draw)
            .def("render_layer", &ScratchPad::renderLayer)
            .def("render", &ScratchPad::render);

    py::class_<BatchedScratchPad>(m, "BatchedScratchPad")
            .def(py::init<int>(),
                 py::arg("pad_num"))
            .def("load_brush", &BatchedScratchPad::loadBrush)
            .def("reset_all_pads", &BatchedScratchPad::resetAllPads)
            .def("reset_pad", &BatchedScratchPad::resetPad)
            .def("add_layer", &BatchedScratchPad::addLayer)
            .def("pop_layer", &BatchedScratchPad::popLayer)
            .def("set_opacity", &BatchedScratchPad::setOpacity)
            .def("get_pad_num", &BatchedScratchPad::getPadNum)
            .def("get_brush_num", &BatchedScratchPad::getBrushNum)
            .def("get_layer_num", &BatchedScratchPad::getLayerNum)
            .def("get_pad_size", &BatchedScratchPad::getPadSize)
            .def("draw", &BatchedScratchPad::draw)
            .def("render_layer", &BatchedScratchPad::renderLayer)
            .def("render", &BatchedScratchPad::render);
}
