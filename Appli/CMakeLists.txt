#Appli
cmake_minimum_required(VERSION 3.22)

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Compiler options
set(STM32_MCU_FLAGS "-mcpu=cortex-m55 -mfpu=fpv5-d16 -mfloat-abi=hard -mcmse ")

# Linker options
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(STM32_LINKER_SCRIPT stm32n647xx_ROMxspi2_RAMxspi1.ld)
else()
    set(STM32_LINKER_SCRIPT stm32n647xx_LRUN_RAMxspi1.ld)
endif()

# Set the project name
set(CMAKE_PROJECT_NAME 01led_Appli)
project(${CMAKE_PROJECT_NAME})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Include mx-generated
include("mx-generated.cmake")

# In order to use CMake for cross-compiling
set(CMAKE_CXX_COMPILER_FORCED "true")

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# ===== User：递归扫描.c，递归收集头文件目录（音乐播放器彩蛋新增文件会自动纳入）=====
file(GLOB_RECURSE USER_DRIVER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/User/*.c"
)
file(GLOB_RECURSE USER_DRIVER_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/User/*.h"
)
foreach(header ${USER_DRIVER_HEADERS})
    get_filename_component(dir ${header} DIRECTORY)
    list(APPEND USER_DRIVER_INCLUDE_DIRS ${dir})
endforeach()
list(REMOVE_DUPLICATES USER_DRIVER_INCLUDE_DIRS)

# ===== LVGL 移植文件：在 LVGL 根目录下 =====
file(GLOB LVGL_PORT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/LVGL/lv_port*.c"
)

# ===== LVGL：递归扫描 src 目录 =====
file(GLOB_RECURSE LVGL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/LVGL/src/*.c"
)

# ===== LVGL Demos：递归扫描整个 demos 目录 =====
file(GLOB_RECURSE LVGL_DEMO_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/LVGL/demos/*.c"
)

# 头文件只加顶层目录
set(LVGL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/LVGL")

# ===== X-CUBE-AI App 层（在 ExtMemLoader 下，需显式添加） =====
set(XCUBEAI_APP_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../ExtMemLoader/X-CUBE-AI/App"
)
set(XCUBEAI_APP_SOURCES
    "${XCUBEAI_APP_DIR}/network.c"
)

# ===== X-CUBE-AI NPU 运行时源文件 =====
# Appli 跑 FreeRTOS，OSAL 与 ExtMemLoader（BARE_METAL）不同，必须重新编译一份
set(XCUBEAI_NPU_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/AI/Npu"
)
file(GLOB_RECURSE XCUBEAI_NPU_SOURCES
    "${XCUBEAI_NPU_DIR}/ll_aton/*.c"
    "${XCUBEAI_NPU_DIR}/Devices/STM32N6XX/*.c"
)

# 排除 rtos_template.c —— 它是被 #include 进 osal_freertos.c 的"代码片段"
# 不是独立编译单元，直接编译会导致符号重复定义
list(FILTER XCUBEAI_NPU_SOURCES EXCLUDE REGEX "ll_aton_osal_rtos_template\\.c$")
# 任务#20：排除 ll_aton_stai_internal.c —— stai 主机侧集成文件，ARMCC 语法，GCC 编译不过
list(FILTER XCUBEAI_NPU_SOURCES EXCLUDE REGEX "ll_aton_stai_internal\\.c$")

# ===== X-CUBE-AI 头文件路径 =====
set(XCUBEAI_INCLUDE_DIRS
    "${XCUBEAI_APP_DIR}"
    "${XCUBEAI_NPU_DIR}/ll_aton"
    "${XCUBEAI_NPU_DIR}/Devices/STM32N6XX"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/AI/Inc"
)

# ===== 添加头文件路径 =====
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${USER_DRIVER_INCLUDE_DIRS}
    ${LVGL_INCLUDE_DIR}
    ${XCUBEAI_INCLUDE_DIRS}
)

# ===== 添加源文件 =====
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${USER_DRIVER_SOURCES}
    ${LVGL_SOURCES}
    ${LVGL_PORT_SOURCES}
    ${XCUBEAI_APP_SOURCES}
    ${LVGL_DEMO_SOURCES}
    ${XCUBEAI_NPU_SOURCES}
)

# ===== 宏定义 =====
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    LV_CONF_INCLUDE_SIMPLE
    LL_ATON_PLATFORM=LL_ATON_PLAT_STM32N6
    LL_ATON_OSAL=LL_ATON_OSAL_FREERTOS
    LL_ATON_SW_FALLBACK=1
)
# network.c 有 7 万行，强制单文件 -O2，避免 -O3 下编译吃几个 GB 内存
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/../ExtMemLoader/X-CUBE-AI/App/network.c"
    PROPERTIES COMPILE_FLAGS "-O2"
)


# ===== 链接选项 =====
# -u _printf_float：解除 newlib-nano 对浮点 printf/snprintf 的限制
# 使用 LINKER: 前缀确保 CMake 不会合并多个 -u 选项
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    "LINKER:-u,_printf_float"
)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../Middlewares/ST/AI/Lib"
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    :NetworkRuntime1000_CM55_GCC.a
    m
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Validate that STM32CubeMX code is compatible with C standard
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()