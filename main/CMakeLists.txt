# Register all your source files with the IDF component manager
# Dodaj tę linię przed poleceniem idf_component_register()

idf_component_register(
    SRCS 
        "../src/main.cpp"
        # Add all your source files here (relative to this CMakeLists.txt)
        # You can use a GLOB pattern but it's generally not recommended by CMake
        # If you have many files, list them individually
    INCLUDE_DIRS 
        "../include"
        "../include/Auxiliary"
        "../include/Interfaces"
        "../include/Modules"
        "../include/Words"
        "../include/ESP32"
)
idf_build_set_property(CXX_COMPILE_OPTIONS "-fcoroutines" APPEND)

#Set compiler flags
target_compile_options(${COMPONENT_LIB} PUBLIC 
    -std=c++23
    -fconcepts
    -Wfatal-errors
    -fcoroutines
)

# For debug builds
if(CONFIG_DEBUG_BUILD)
    target_compile_options(${COMPONENT_LIB} PRIVATE -g)
endif()

spiffs_create_partition_image(storage ../add_ons FLASH_IN_PROJECT)