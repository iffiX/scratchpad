#include "b_scratchpad.h"
#include "util.h"
#include <fmt/format.h>
#include <functional>

#ifdef USE_OPENMP

#include <omp.h>

#endif

extern int omp_max_threads;

BatchedScratchPad::BatchedScratchPad(int pad_num)
: _pool(pad_num), _pads(pad_num) {}


void BatchedScratchPad::loadBrush(const std::string &brush_string) {
    std::vector<std::future<void>> results;
    for(auto &pad: _pads)
        results.emplace_back(_pool.enqueue(&ScratchPad::loadBrush, &pad, brush_string));
    for(auto &fut: results)
        fut.get();
    _brush_num++;
}

void BatchedScratchPad::resetAllPads(int width, int height, int layers) {
    std::vector<std::future<void>> results;
    for(auto &pad: _pads)
        results.emplace_back(_pool.enqueue(&ScratchPad::resetPad, &pad, width, height, layers));
    for(auto &fut: results)
        fut.get();
}

void BatchedScratchPad::resetPad(int pad, int width, int height, int layers) {
    if (pad >= _pads.size() or pad < 0)
        throw py::index_error();
    auto pad_ptr = &_pads[pad];
    _pool.enqueue(&ScratchPad::resetPad, pad_ptr, width, height, layers).get();
}

void BatchedScratchPad::addLayer(int pad) {
    if (pad >= _pads.size() or pad < 0)
        throw py::index_error();
    auto pad_ptr = &_pads[pad];
    _pool.enqueue(&ScratchPad::addLayer, pad_ptr).get();
}

void BatchedScratchPad::setOpacity(int pad, int layer, float opacity) {
    if (pad >= _pads.size() or pad < 0)
        throw py::index_error();
    auto pad_ptr = &_pads[pad];
    _pool.enqueue(&ScratchPad::setOpacity, pad_ptr, layer, opacity).get();
}

void BatchedScratchPad::popLayer(int pad, int layer) {
    if (pad >= _pads.size() or pad < 0)
        throw py::index_error();
    auto pad_ptr = &_pads[pad];
    _pool.enqueue(&ScratchPad::popLayer, pad_ptr, layer).get();
}

int BatchedScratchPad::getPadNum() {
    return _pads.size();
}

int BatchedScratchPad::getBrushNum() {
    return _brush_num;
}

int BatchedScratchPad::getLayerNum(int pad) {
    if (pad >= _pads.size() or pad < 0)
        throw py::index_error();
    auto pad_ptr = &_pads[pad];
    return _pool.enqueue(&ScratchPad::getLayerNum, pad_ptr).get();
}

std::tuple<int, int> BatchedScratchPad::getPadSize(int pad) {
    if (pad >= _pads.size() or pad < 0)
        throw py::index_error();
    auto pad_ptr = &_pads[pad];
    return _pool.enqueue(&ScratchPad::getPadSize, pad_ptr).get();
}

void BatchedScratchPad::draw(const std::vector<int> &pad,
                             const std::vector<int> &layer,
                             const std::vector<int> &brush,
                             const std::vector<Setting> &setting,
                             const std::vector<std::vector<Point>> &points) {
    if (pad.size() != layer.size() or
        pad.size() != brush.size() or
        pad.size() != setting.size() or
        pad.size() != points.size())
        throw std::invalid_argument("Size of pad ids, layer ids, brush ids, settings and points "
                                    "doesn't match!");
    std::vector<std::future<void>> results;
    for(size_t i=0; i<pad.size(); i++) {
        int pad_idx = pad[i];
        if (pad_idx >= _pads.size() or pad_idx < 0)
            throw py::index_error();
        results.emplace_back(
                _pool.enqueue(&ScratchPad::draw,
                              &_pads[pad_idx], layer[i], brush[i], setting[i], points[i]));
    }
    for(auto &fut: results)
        fut.get();
}

std::vector<py::array> BatchedScratchPad::renderLayer(const std::vector<int> &pad,
                                                      const std::vector<int> &layer,
                                                      const py::object &dt) {
    auto dtype = py::dtype::from_args(dt);
    if (dtype.has_fields())
        throw std::invalid_argument("Only support rendering as a flat floating array or integral array!");
    if (pad.size() != layer.size())
        throw std::invalid_argument("Size of pad ids and layer ids doesn't match!");

    std::vector<std::future<void*>> futures;
    std::vector<void*> arrays;
    std::vector<py::array> ret_results;
    {
        py::gil_scoped_release release;
        for (int idx=0; idx < pad.size(); idx++) {
            int pad_idx = pad[idx];
            int layer_idx = layer[idx];
            if (pad_idx >= _pads.size() or pad_idx < 0)
                throw std::out_of_range(fmt::format("Invalid pad index {}", pad_idx));
            futures.emplace_back(
                    _pool.enqueue(
                            [](ScratchPad *pad, int layer, char kind, int item_size, int thread_num) {
                                omp_set_num_threads(thread_num);
                                return pad->renderLayer(layer, kind, item_size);
                            },
                            &_pads[pad_idx], layer_idx, dtype.kind(), dtype.itemsize(), omp_max_threads)
            );
        }
        for (auto &fut: futures) {
            arrays.push_back(fut.get());
        }
    }
    for(int idx = 0; idx<pad.size(); idx++) {
        int width = std::get<0>(_pads[pad[idx]].getPadSize());
        int height = std::get<1>(_pads[pad[idx]].getPadSize());
        int real_width = ALIGN(width, MYPAINT_TILE_SIZE);
        auto capsule = py::capsule(arrays[idx], [](void *v) { free(v); });
        ret_results.emplace_back(py::array(dtype,
                                           {height, width, 4},
                                           {real_width * 4 * dtype.itemsize(),
                                            4 * dtype.itemsize(),
                                            dtype.itemsize()},
                                           arrays[idx], capsule));
    }

    return std::move(ret_results);
}

std::vector<py::array> BatchedScratchPad::render(const std::vector<int> &pad,
                                                 const py::object &dt) {

    auto dtype = py::dtype::from_args(dt);
    if (dtype.has_fields())
        throw std::invalid_argument("Only support rendering as a flat floating array or integral array!");

    std::vector<std::future<void*>> futures;
    std::vector<void*> arrays;
    std::vector<py::array> ret_results;
    {
        py::gil_scoped_release release;
        for (auto pad_idx: pad) {
            if (pad_idx >= _pads.size() or pad_idx < 0)
                throw std::out_of_range(fmt::format("Invalid pad index {}", pad_idx));
            futures.emplace_back(
                    _pool.enqueue(
                            [](ScratchPad *pad, char kind, int item_size, int thread_num) {
                                omp_set_num_threads(thread_num);
                                return pad->render(kind, item_size);
                            },
                            &_pads[pad_idx], dtype.kind(), dtype.itemsize(), omp_max_threads)
                    );
        }
        for (auto &fut: futures) {
            arrays.push_back(fut.get());
        }
    }
    for(int idx = 0; idx<pad.size(); idx++) {
        int width = std::get<0>(_pads[pad[idx]].getPadSize());
        int height = std::get<1>(_pads[pad[idx]].getPadSize());
        int real_width = ALIGN(width, MYPAINT_TILE_SIZE);
        auto capsule = py::capsule(arrays[idx], [](void *v) { free(v); });
        ret_results.emplace_back(py::array(dtype,
                                           {height, width, 4},
                                           {real_width * 4 * dtype.itemsize(),
                                            4 * dtype.itemsize(),
                                            dtype.itemsize()},
                                           arrays[idx], capsule));
    }

    return std::move(ret_results);
}
