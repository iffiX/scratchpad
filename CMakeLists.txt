cmake_minimum_required(VERSION 3.5)
project(Scratchpad)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}")

add_compile_definitions(FMT_HEADER_ONLY)
find_package(OpenMP)
if (OPENMP_FOUND)
    add_compile_definitions(USE_OPENMP)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()
################################################################
# Configuration for depended libraries
################################################################

# Libmypaint configurations

include(ExternalProject)
set(MYPAINT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/libmypaint)
set(MYPAINT_BUILD ${CMAKE_CURRENT_BINARY_DIR}/libmypaint)
set(MYPAINT_DYNAMIC_LIB ${MYPAINT_BUILD}/lib/libmypaint.so)
set(MYPAINT_INCLUDES ${MYPAINT_BUILD}/include/libmypaint)
set(MYPAINT_CFLAGS "-Ofast -ftree-vectorize -fopt-info-vec-optimized -march=native -mtune=native -funsafe-math-optimizations -funsafe-loop-optimizations")

ExternalProject_Add(
        libmypaint
        PREFIX ${MYPAINT_BUILD}
        SOURCE_DIR ${MYPAINT_DIR}
        CONFIGURE_COMMAND cd ${MYPAINT_DIR}
        COMMAND ${MYPAINT_DIR}/autogen.sh
        COMMAND ${MYPAINT_DIR}/configure
        --srcdir=${MYPAINT_DIR} --prefix=${MYPAINT_BUILD}
        BUILD_COMMAND make VERBOSE=1 CFLAGS=${MYPAINT_CFLAGS}
        INSTALL_COMMAND make install && make clean
        BUILD_BYPRODUCTS ${MYPAINT_DYNAMIC_LIB}
)
add_library(mypaint STATIC IMPORTED GLOBAL)
add_dependencies(mypaint libmypaint)

# make directories in advance to prevent cmake from complaining
file(MAKE_DIRECTORY ${MYPAINT_INCLUDES})
set_target_properties(mypaint PROPERTIES IMPORTED_LOCATION "${MYPAINT_DYNAMIC_LIB}")
set_target_properties(mypaint PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${MYPAINT_INCLUDES}")

# pybind11 configurations
add_subdirectory(lib/pybind11)

################################################################
# Configuration for depended system libraries
################################################################
if(CMAKE_VERSION VERSION_LESS 3.14)
    find_package(NumPy REQUIRED)
else()
    find_package(Python3 COMPONENTS Interpreter NumPy)
endif()

################################################################
# Include path
################################################################
pybind11_add_module(
        scratchpad SHARED
        csrc/scratchpad.cpp
        csrc/b_scratchpad.cpp
        csrc/init.cpp
)

target_link_libraries(
        scratchpad PUBLIC
        mypaint
)
target_include_directories(
        scratchpad PUBLIC
        lib
        ${CMAKE_CURRENT_SOURCE_DIR}/csrc
        ${MYPAINT_INCLUDES}
        ${PYTHON_NUMPY_INCLUDE_DIR}
)