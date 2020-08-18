from cmake_build import *

import re
from os import path
import setuptools

root = path.abspath(path.dirname(__file__))

version = "0.0.1"

setuptools.setup(
    name="scratchpad",
    version=version,
    description="Painting environment for RL",
    url="https://github.com/iffiX/scratchpad",
    author="Iffi",
    author_email="iffi@mail.beyond-infinity.com",
    license="MIT",
    python_requires=">=3.5",
    ext_modules=[CMakeExtension("scratchpad", version=version)],
    cmdclass={'build_ext': CMakeBuildExt},
    classifiers=[
        # How mature is this project? Common values are
        #   3 - Alpha
        #   4 - Beta
        #   5 - Production/Stable
        "Development Status :: 3 - Alpha",

        # Indicate who your project is intended for
        "Intended Audience :: Developers",
        "Intended Audience :: Science/Research",
        "Topic :: Scientific/Engineering :: Artificial Intelligence",
        "Topic :: Software Development :: Libraries :: Python Modules",
        # Pick your license as you wish (should match "license" above)
        "License :: OSI Approved :: MIT License",

        # Specify the Python versions you support here. In particular, ensure
        # that you indicate whether you support Python 2, Python 3 or both.
        "Programming Language :: Python :: 3.5",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
    ],
    install_requires=[
        "cmake",
        "numpy<=1.18"
    ]
)
