cmake_minimum_required(VERSION 3.20)
project(BloodPongNative VERSION 1.0.0 LANGUAGES CXX RC)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/generate_resource_script.py"
            "analysis/extracted" "src/generated_resources.rc"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMAND_ERROR_IS_FATAL ANY
)

add_executable(BloodPong WIN32
    src/main.cpp
    src/resources.rc
    src/generated_resources.rc
)

target_compile_features(BloodPong PRIVATE cxx_std_20)
target_compile_definitions(BloodPong PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(BloodPong PRIVATE user32 gdi32 msimg32 winmm)

if(MINGW)
    target_link_options(BloodPong PRIVATE -static -static-libgcc -static-libstdc++)
endif()

if(MSVC)
    set_property(TARGET BloodPong PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

set_target_properties(BloodPong PROPERTIES
    OUTPUT_NAME "Blood Pong"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
