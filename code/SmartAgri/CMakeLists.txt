cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME SmartAgri)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME}
        Driver/Src/drv_key.c
        Driver/Inc/drv_key.h
        App/Src/app_tasks.c
        App/Inc/app_tasks.h
        BSP/Src/bsp_led.c
        BSP/Inc/bsp_led.h
        BSP/Inc/bsp_key.h
        BSP/Src/bsp_log.c
        BSP/Inc/bsp_log.h
        Driver/Inc/drv_oled.h
        Driver/Inc/drv_oled_data.h
        App/Src/app_ui.c
        App/Inc/app_ui.h
        BSP/Inc/bsp_buzz.h
        Driver/Src/drv_bh1750.c
        Driver/Inc/drv_rain_adc.h
        Driver/Inc/drv_sht30.h
        Driver/Inc/drv_soil_adc.h
        Driver/Src/drv_esp8266.c
        Driver/Inc/drv_esp8266.h
)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

include(cmake/user_code.cmake)

