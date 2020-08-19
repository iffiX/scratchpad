import os

# find all brushes in current directory
brush_dir = os.path.dirname(os.path.abspath(__file__))
brush_files = []
brushes = []
for dirpath, dirnames, filenames in os.walk(brush_dir):
    for file in filenames:
        if file.endswith(".myb"):
            brush_files.append(os.path.join(dirpath, file))
            with open(brush_files[-1], "r") as f:
                brushes.append(f.read())


def get_brush_file_names():
    return brush_files


def get_brushes():
    return brushes
