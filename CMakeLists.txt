cmake_minimum_required(VERSION 3.5)
project(Scratchpad)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}")
add_compile_definitions(FMT_HEADER_ONLY)
if(DEFINED VERSION_INFO)
    add_compile_definitions(VERSION_INFO=${VERSION_INFO})
endif()
find_package(OpenMP)
if (OPENMP_FOUND)
    add_compile_definitions(USE_OPENMP)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Make in release mode!")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Ofast -ftree-vectorize -fopt-info-vec-optimized -march=native -mtune=native -funsafe-math-optimizations -funsafe-loop-optimizations")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast -ftree-vectorize -fopt-info-vec-optimized -march=native -mtune=native -funsafe-math-optimizations -funsafe-loop-optimizations")
elseif (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Make in debug mode!")
    add_compile_definitions(USE_DEBUG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -rdynamic")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -rdynamic")
endif()
################################################################
# Configuration for depended libraries
################################################################
include(ExternalProject)

# Libmypaint configurations
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
        INSTALL_COMMAND make install && make clean && ${CMAKE_COMMAND} -E copy_directory ${MYPAINT_BUILD}/lib ${CMAKE_INSTALL_PREFIX}
        BUILD_BYPRODUCTS ${MYPAINT_DYNAMIC_LIB}
)
add_library(mypaint SHARED IMPORTED GLOBAL)
add_dependencies(mypaint libmypaint)

# make directories in advance to prevent cmake from complaining
file(MAKE_DIRECTORY ${MYPAINT_INCLUDES})
set_target_properties(mypaint PROPERTIES IMPORTED_LOCATION "${MYPAINT_DYNAMIC_LIB}")
set_target_properties(mypaint PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${MYPAINT_INCLUDES}")


# mypaint-brushes configurations
set(MYPAINT_BRUSHES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/mypaint-brushes)
set(MYPAINT_BRUSHES_BUILD ${CMAKE_CURRENT_BINARY_DIR}/mypaint-brushes)
ExternalProject_Add(
        mypaint-brushes
        PREFIX ${MYPAINT_BRUSHES_BUILD}
        SOURCE_DIR ${MYPAINT_BRUSHES_DIR}
        CONFIGURE_COMMAND cd ${MYPAINT_BRUSHES_DIR}
        COMMAND ${MYPAINT_BRUSHES_DIR}/autogen.sh
        COMMAND ${MYPAINT_BRUSHES_DIR}/configure
        --srcdir=${MYPAINT_BRUSHES_DIR} --prefix=${MYPAINT_BRUSHES_BUILD}
        BUILD_COMMAND make VERBOSE=1
        INSTALL_COMMAND make install && make clean
            && ${CMAKE_COMMAND} -E copy_directory ${MYPAINT_BRUSHES_BUILD}/share/mypaint-data/2.0/ ${CMAKE_INSTALL_PREFIX}
)

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
        internal SHARED
        csrc/scratchpad.cpp
        csrc/b_scratchpad.cpp
        csrc/init.cpp
)

target_link_libraries(
        internal PUBLIC
        mypaint
)
target_include_directories(
        internal PUBLIC
        lib
        ${CMAKE_CURRENT_SOURCE_DIR}/csrc
        ${MYPAINT_INCLUDES}
        ${PYTHON_NUMPY_INCLUDE_DIR}
)
add_dependencies(internal mypaint-brushes)
set_target_properties(internal PROPERTIES LINK_FLAGS "-Wl,-rpath,'$ORIGIN'")
install(TARGETS internal
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX})
