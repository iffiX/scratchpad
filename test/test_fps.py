from scratchpad import (
    Setting,
    Point,
    ScratchPad,
    get_brushes,
    set_omp_max_threads
)
from time import time
import numpy as np

# 11ms per frame, per layer at 1024 * 1024, using 4 threads, float
# 4.5ms ~ 7.5ms per frame, per layer at 1024 * 1024, using 4 threads, uint8
pad_size = (1024, 1024)


if __name__ == "__main__":
    #set_omp_max_threads(4)
    p = ScratchPad()
    p.load_brush(get_brushes()[0])
    p.reset_pad(*pad_size, 1)

    # draw rectangle, only works when dtime is set to max
    points = [Point(0.3, 0.3), Point(0.3, 0.6), Point(0.6, 0.6), Point(0.6, 0.3), Point(0.3, 0.3)]
    p.draw(0, 0, Setting(1.0, 0.1, 0.5, 0.5, 0.5, 0.5), points)

    begin = time()
    for i in range(2000):
        p.render(np.float32)
    end = time()
    print((end - begin) / 2000)
