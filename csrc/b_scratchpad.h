#ifndef B_SCRATCHPAD_H
#define B_SCRATCHPAD_H

#include <thread_pool/thread_pool.h>
#include "scratchpad.h"

class BatchedScratchPad {
public:
    BatchedScratchPad() = delete;
    explicit BatchedScratchPad(int pad_num);

    void loadBrush(const std::string &brush_string);
    void resetAllPads(int width, int height, int layers=1);
    void resetPad(int pad, int width, int height, int layers=1);
    void addLayer(int pad);
    void popLayer(int pad, int layer);
    void setOpacity(int pad, int layer, float opacity);

    int getPadNum();
    int getBrushNum();
    int getLayerNum(int pad);
    std::tuple<int, int> getPadSize(int pad);

    void draw(const std::vector<int> &pad,
              const std::vector<int> &layer,
              const std::vector<int> &brush,
              const std::vector<Setting> &setting,
              const std::vector<std::vector<Point>> &points);

    std::vector<py::array> renderLayer(const std::vector<int> &pad,
                                       const std::vector<int> &layer,
                                       const py::object& dtype);

    std::vector<py::array> render(const std::vector<int> &pad,
                                  const py::object& dtype);


private:
    int _brush_num = 0;
    std::mutex _py_mutex;
    std::vector<ScratchPad> _pads;
    ThreadPoolConcurrent<> _pool;
};

#endif //B_SCRATCHPAD_H
