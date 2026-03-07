# cmake/user_code.cmake
# 用于把你自己新增的目录（App/Service/Driver/BSP）加入工程
# 约定结构：
#   App/Inc + App/Src
#   Service/Inc + Service/Src
#   Driver/Inc + Driver/Src
#   BSP/Inc + BSP/Src

set(USER_BASE "${PROJECT_SOURCE_DIR}")

# ---------- sources ----------
file(GLOB_RECURSE USER_SOURCES CONFIGURE_DEPENDS
    "${USER_BASE}/App/Src/*.c"
    "${USER_BASE}/Service/Src/*.c"
    "${USER_BASE}/Driver/Src/*.c"
    "${USER_BASE}/BSP/Src/*.c"
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${USER_SOURCES}
)

# ---------- include dirs ----------
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    "${USER_BASE}/App/Inc"
    "${USER_BASE}/Service/Inc"
    "${USER_BASE}/Driver/Inc"
    "${USER_BASE}/BSP/Inc"
)

# （可选）如果你还有第三方库/公共头文件，可在这里继续加 include 目录
# target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${USER_BASE}/ThirdParty/Inc")
