from scratchpad import (
    Setting,
    Point,
    ScratchPad,
    get_brushes,
    set_omp_max_threads
)
import numpy as np
import matplotlib.pyplot as plt


def show_image(array, title="image"):
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


pad_size = (1024, 1024)


if __name__ == "__main__":
    set_omp_max_threads(4)
    p = ScratchPad()
    p.load_brush(get_brushes()[0])
    p.reset_pad(*pad_size, 1)
    assert p.get_brush_num() == 1
    assert p.get_pad_size() == pad_size
    assert p.get_layer_num() == 1
    p.set_opacity(0, 1.0)
    p.add_layer()
    p.pop_layer(1)
    assert p.get_layer_num() == 1

    # draw rectangle, only works when dtime is set to max
    points = [Point(0.3, 0.3), Point(0.3, 0.6), Point(0.6, 0.6), Point(0.6, 0.3), Point(0.3, 0.3)]
    print(points)
    p.draw(0, 0, Setting(1.0, 0.1, 0.5, 0.5, 0.5, 0.5), points)

    arr1 = p.render(np.float32)
    arr2 = p.render_layer(0, np.float32)
    assert np.allclose(arr1, arr2)
    show_image(arr1[:, :, 0:3])

    plt.show()
