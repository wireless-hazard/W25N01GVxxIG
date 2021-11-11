set(COMPONENT_SRCS
    "src/W25N01GV.cpp")
set(COMPONENT_ADD_INCLUDEDIRS "include")

register_component()

component_compile_options(PUBLIC -DMPU_COMPONENT_TRUE=1)

## Defines needed for library code implementation


