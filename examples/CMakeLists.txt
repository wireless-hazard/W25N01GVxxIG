cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS ${PWD}../../W25N01GVxxIG)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(w25_flash_memory_example)