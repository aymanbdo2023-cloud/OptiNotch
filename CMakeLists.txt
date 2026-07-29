cmake_minimum_required(VERSION 3.15)
project(OptiNotch VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/window/window.cpp
    src/ui/main_ui.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(${PROJECT_NAME} PRIVATE d3d11 dwmapi d3dcompiler)

add_library(${PROJECT_NAME}_shared SHARED
    src/main.cpp
    src/window/window.cpp
    src/ui/main_ui.cpp
    ${IMGUI_SOURCES}
)

target_compile_definitions(${PROJECT_NAME}_shared PRIVATE
    OPTINOTCH_BUILD_AS_DLL
)

target_include_directories(${PROJECT_NAME}_shared PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(${PROJECT_NAME}_shared PRIVATE d3d11 dwmapi d3dcompiler)

target_link_options(${PROJECT_NAME}_shared PRIVATE
    -static-libstdc++ -static-libgcc -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic
)
