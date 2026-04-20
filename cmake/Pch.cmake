message("-- Creating pch: " ${PROJECT_NAME})
target_precompile_headers(${PROJECT_NAME} PRIVATE
    "${PROJECT_SOURCE_DIR}/pch.h"
)