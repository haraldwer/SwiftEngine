
set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)

FetchContent_Declare(
        tinygltf
        GIT_REPOSITORY https://github.com/syoyo/tinygltf
        GIT_TAG        release
        GIT_PROGRESS   TRUE
        GIT_SHALLOW TRUE
        GIT_SUBMODULES_RECURSE FALSE
)
FetchContent_GetProperties(tinygltf)
if(NOT tinygltf_POPULATED)
    FetchContent_Populate(tinygltf)
endif()

list(APPEND DEP_INCLUDES ${tinygltf_SOURCE_DIR})