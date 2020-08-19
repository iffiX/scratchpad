from .internal import *
from .brushes import get_brushes, get_brush_file_names

__all__ = [
    "Point",
    "Setting",
    "ScratchPad",
    "BatchedScratchPad",
    "set_omp_max_threads"
]

set_omp_max_threads(4)
