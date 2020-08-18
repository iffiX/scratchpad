# Code from https://github.com/raydouglass/cmake_setuptools
# combined with improvements in https://github.com/raydouglass/cmake_setuptools/issues
# and https://www.benjack.io/2017/06/12/python-cpp-tests.html


import os
import re
import subprocess
import shutil
import sys
import zipfile
from shlex import split
from hashlib import sha256
from base64 import urlsafe_b64encode
from distutils.command.install_headers import install_headers
from setuptools import Extension
from setuptools.command.build_ext import build_ext

# None if cmake is not found by shutil and "CMAKE_EXE" is not set
CMAKE_EXE = os.environ.get('CMAKE_EXE', shutil.which('cmake'))

# try using the cmake package
try:
    import cmake
    CMAKE_EXE = os.path.join(cmake.CMAKE_BIN_DIR, "cmake")
except ImportError:
    pass


def check_for_cmake():
    if not CMAKE_EXE:
        print('cmake executable not found. '
              'Set CMAKE_EXE environment or update your cmake path')
        sys.exit(1)


class CMakeExtension(Extension):
    """
    setuptools.Extension for cmake

    The install target will be built, and all files in ${CMAKE_INSTALL_PREFIX} will be copied.
    """

    def __init__(self, name, version="", sourcedir=""):
        """
        Args:
            name: Name of the extension, the python dotted name which will be used to
                determine build location, install location, and import name.
                Eg: "somepachage", or "myPackage.myModule"

            sourcedir: The directory where sources are located, by default it is the
                current working directory.
        """
        check_for_cmake()
        # find sources inside the source directory
        # needed by sdist command, if we would like to publish source distribution
        sources = []
        for dirpath, dirnames, filenames in os.walk(sourcedir):
            for file in filenames:
                if re.search(r"\.(c|c\+\+|cc|cl|cp|cpp|cu|cxx|h|h\+\+|hh|hp|hpp|hxx)$",
                             file, re.IGNORECASE) is not None:
                    sources.append(os.path.join(dirpath, file))

        Extension.__init__(self, name, sources=sources)
        self.version = version
        self.sourcedir = os.path.abspath(sourcedir)


class CMakeBuildExt(build_ext):
    """
    setuptools build_exit which builds using cmake & make
    You can add cmake args by modify CMakeBuildExt.cmake_args
    """
    cmake_args = []

    def copy_extensions_to_source(self):
        """
        Support inplace installation used in::
            setup.py develop
        or::
            pip install -e .
        """
        from distutils.file_util import copy_file
        from distutils.dir_util import copy_tree
        build_py = self.get_finalized_command('build_py')
        for ext in self.extensions:
            fullname = self.get_ext_fullname(ext.name)
            filename = self.get_ext_filename(fullname)
            modpath = fullname.split('.')
            package = '.'.join(modpath[:-1])
            package_dir = build_py.get_package_dir(package)

            if isinstance(ext, CMakeExtension):
                old_inplace, self.inplace = self.inplace, 0
                # copy install directory
                output_dir = os.path.abspath(
                    os.path.dirname(self.get_ext_fullpath(ext.name)))
                self.inplace = old_inplace
                copy_tree(output_dir, package_dir)
            else:

                dest_filename = os.path.join(package_dir,
                                             os.path.basename(filename))
                src_filename = os.path.join(self.build_lib, filename)

                # Always copy, even if source is older than destination, to ensure
                # that the right extensions for the current Python/platform are
                # used.
                copy_file(
                    src_filename, dest_filename, verbose=self.verbose,
                    dry_run=self.dry_run
                )
                if ext._needs_stub:
                    self.write_stub(package_dir or os.curdir, ext, True)

    def build_extension(self, ext):
        check_for_cmake()
        if isinstance(ext, CMakeExtension):
            output_dir = os.path.abspath(
                os.path.dirname(self.get_ext_fullpath(ext.name)))

            build_type = 'Debug' if self.debug else 'Release'

            # PYTHON_EXECUTABLE is added so there is no need to find_python() in cmake
            # VERSION_INFO could be used in the pybind11 interface c++ file.
            cmake_args = [CMAKE_EXE,
                          ext.sourcedir,
                          '-DCMAKE_INSTALL_PREFIX=' + output_dir,
                          '-DCMAKE_BUILD_TYPE=' + build_type,
                          '-DPYTHON_EXECUTABLE=' + sys.executable,
                          '-DVERSION_INFO="{}"'.format(ext.version)]

            # read args defined in CMAKE_COMMON_VARIABLES
            # Split arguments with shlex so '-G "MinGW Makefiles"' may be used.
            cmake_args.extend(
                [x for x in
                 split(os.environ.get('CMAKE_COMMON_VARIABLES', ''))
                 if x])

            cmake_args.extend(self.cmake_args)

            env = os.environ.copy()
            if not os.path.exists(self.build_temp):
                os.makedirs(self.build_temp)
            # let cmake produce make script
            subprocess.check_call(cmake_args,
                                  cwd=self.build_temp,
                                  env=env)
            # let cmake invoke compiler
            subprocess.check_call([CMAKE_EXE,
                                   '--build', '.',
                                   '-j',
                                   '--target', 'install'],
                                  cwd=self.build_temp,
                                  env=env)
            # print an empty line to seperate print results
            print()
        else:
            super().build_extension(ext)


class InstallHeaders(install_headers):
    """
    The built-in install_headers installs all headers in the same directory
    This overrides that to walk a directory tree and copy the tree of headers
    """

    def run(self):
        headers = self.distribution.headers
        if not headers:
            return
        self.mkpath(self.install_dir)
        for header in headers:
            for dirpath, dirnames, filenames in os.walk(header):
                for file in filenames:
                    if file.endswith('.h'):
                        path = os.path.join(dirpath, file)
                        install_target = os.path.join(self.install_dir,
                                                      path.replace(header, ''))
                        self.mkpath(os.path.dirname(install_target))
                        (out, _) = self.copy_file(path, install_target)
                        self.outfiles.append(out)


def convert_to_manylinux(name, version):
    """
    Modifies the arch metadata of a pip package linux_x86_64=>manylinux1_x86_64

    "manylinux1" tag reference: https://www.python.org/dev/peps/pep-0513/

    Note:
        The package must be built with manylinux1 requirements, and locate in
        the "dist" directory, which is the default output location of command::

            python3 setup.py bdist_wheel

        The converted package will also locate in the "dist" directory.

    Args:
        name: Name of the package
        version: Version of the package
    """
    # Get python version as XY (27, 35, 36, etc)
    python_version = str(sys.version_info.major) + str(sys.version_info.minor)
    name_version = '{}-{}'.format(name.replace('-', '_'), version)

    # linux wheel package
    dist_zip = '{0}-cp{1}-cp{1}m-linux_x86_64.whl'.format(name_version,
                                                          python_version)
    dist_zip_path = os.path.join('dist', dist_zip)
    if not os.path.exists(dist_zip_path):
        print('Wheel not found: {}'.format(dist_zip_path))
        return

    unzip_dir = 'dist/unzip/{}'.format(dist_zip)
    os.makedirs(unzip_dir, exist_ok=True)
    with zipfile.ZipFile(dist_zip_path, 'r') as zip_ref:
        zip_ref.extractall(unzip_dir)

    wheel_file = '{}.dist-info/WHEEL'.format(name_version)
    new_wheel_str = ''
    with open(os.path.join(unzip_dir, wheel_file)) as f:
        for line in f.readlines():
            if line.startswith('Tag'):
                # Replace the linux tag
                new_wheel_str += line.replace('linux', 'manylinux1')
            else:
                new_wheel_str += line

    # compute hash & size of the new WHEEL file
    # Follows https://www.python.org/dev/peps/pep-0376/#record
    m = sha256()
    m.update(new_wheel_str.encode('utf-8'))
    w_hash = urlsafe_b64encode(m.digest()).decode('utf-8')
    w_hash = w_hash.replace('=', '')

    with open(os.path.join(unzip_dir, wheel_file), 'w') as f:
        f.write(new_wheel_str)
    statinfo = os.stat(os.path.join(unzip_dir, wheel_file))
    byte_size = statinfo.st_size

    record_file = os.path.join(unzip_dir,
                               '{}.dist-info/RECORD'.format(name_version))
    new_record_str = ''
    with open(record_file) as f:
        for line in f.readlines():
            if line.startswith(wheel_file):
                # Update the record for the WHEEL file
                new_record_str += '{},sha256={},{}'.format(wheel_file, w_hash,
                                                           str(byte_size))
                new_record_str += os.linesep
            else:
                new_record_str += line

    with open(record_file, 'w') as f:
        f.write(new_record_str)

    def zipdir(path, ziph):
        for root, dirs, files in os.walk(path):
            for file in files:
                ziph.write(os.path.join(root, file),
                           os.path.join(root, file).replace(path, ''))

    new_zip_name = dist_zip.replace('linux', 'manylinux1')
    print('Generating new zip {}...'.format(new_zip_name))
    zipf = zipfile.ZipFile(os.path.join('dist', new_zip_name),
                           'w', zipfile.ZIP_DEFLATED)
    zipdir(unzip_dir, zipf)
    zipf.close()

    shutil.rmtree(unzip_dir, ignore_errors=True)
    os.remove(dist_zip_path)
