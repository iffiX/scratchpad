from scratchpad import (
    Setting,
    Point,
    ScratchPad
)
import numpy as np

p = ScratchPad()
p.reset_pad(100, 100, 1)
p.render(np.uint8)
