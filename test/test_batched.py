from scratchpad import (
    Setting,
    Point,
    BatchedScratchPad,
    get_brushes,
    set_omp_max_threads
)
import numpy as np
import matplotlib.pyplot as plt


def show_image(array, title="image"):
    if isinstance(array.dtype, np.floating):
        arr = array.astype(np.float32)
        fig = plt.figure()
        fig.canvas.set_window_title(title)
        ax = fig.add_subplot("121")
        ax.set_facecolor((0.0, 0.0, 0.0))
        ax.imshow(arr, vmin=np.min(arr), vmax=np.max(arr))

        ax2 = fig.add_subplot("122")
        ax2.set_facecolor((0.0, 0.0, 0.0))
        pix_range = np.max(arr) - np.min(arr)
        ax2.imshow((arr - np.min(arr)) / (pix_range + 1e-6), vmin=0, vmax=1)
    else:
        fig = plt.figure()
        fig.canvas.set_window_title(title)
        ax = fig.add_subplot("111")
        ax.set_facecolor((0.0, 0.0, 0.0))
        ax.imshow(array)


pad_size = (1023, 1001)


if __name__ == "__main__":
    set_omp_max_threads(4)
    p = BatchedScratchPad(2)
    p.load_brush(get_brushes()[0])
    p.reset_pad(0, *pad_size, 1)
    p.reset_all_pads(*pad_size, 1)
    assert p.get_pad_num() == 2
    assert p.get_brush_num() == 1
    assert p.get_pad_size(0) == pad_size
    assert p.get_layer_num(0) == 1
    p.set_opacity(0, 0, 1.0)
    p.add_layer(0)
    p.pop_layer(0, 1)
    assert p.get_layer_num(0) == 1

    # draw rectangle, only works when dtime is set to max
    points = [Point(0.3, 0.3), Point(0.3, 0.6), Point(0.6, 0.6), Point(0.6, 0.3), Point(0.3, 0.3)]
    print(points)
    p.draw([0, 1], [0, 0], [0, 0], [Setting(1.0, 0.1, 0.5, 0.5, 0.5, 0.5)] * 2, [points] * 2)

    arr1 = p.render([0, 1], np.float32)
    arr2 = p.render_layer([0, 1], [0, 0], np.float32)
    assert np.allclose(arr1[0], arr2[0]) and np.allclose(arr1[1], arr2[1])
    show_image(arr1[0][:, :, 0:3])

    plt.show()
