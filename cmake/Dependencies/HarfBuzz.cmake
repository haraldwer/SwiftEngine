
set(HB_HAVE_FREETYPE OFF CACHE BOOL "" FORCE)
set(HB_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(HB_BUILD_UTILS   OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    harfbuzz
    GIT_REPOSITORY https://github.com/harfbuzz/harfbuzz.git
    GIT_TAG        main
    GIT_PROGRESS   TRUE
    GIT_SHALLOW TRUE
    GIT_SUBMODULES_RECURSE FALSE
)
FetchContent_MakeAvailable(harfbuzz)
