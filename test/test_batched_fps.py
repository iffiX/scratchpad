from scratchpad import (
    Setting,
    Point,
    BatchedScratchPad,
    get_brushes,
    set_omp_max_threads
)
from time import time, sleep
import numpy as np

# 19ms per frame (2 pads), per layer at 1024 * 1024, using 2 threads per pad, float
# constrained by memory bandwidth
pad_size = (1024, 1024)


if __name__ == "__main__":
    set_omp_max_threads(2)
    p = BatchedScratchPad(2)
    p.load_brush(get_brushes()[0])
    p.reset_all_pads(*pad_size, 1)

    # draw rectangle, only works when dtime is set to max
    points = [Point(0.3, 0.3), Point(0.3, 0.6), Point(0.6, 0.6), Point(0.6, 0.3), Point(0.3, 0.3)]
    p.draw([0, 1], [0, 0], [0, 0], [Setting(1.0, 0.1, 0.5, 0.5, 0.5, 0.5)] * 2, [points] * 2)

    begin = time()
    for i in range(200):
        p.render([0, 1], np.float32)
    end = time()
    print((end - begin) / 200)
