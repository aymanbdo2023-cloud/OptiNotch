cmake_minimum_required(VERSION 3.15)
project(OptiNotch VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Fonts are embedded directly in src/main.cpp via #embed, so the app is a
# single self-contained file (no assets/ folder needed at runtime).

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

set(CALENDAR_SOURCES
    src/calendar/calendar.cpp
    src/calendar/http.cpp
    src/calendar/json.cpp
)

set(APP_SOURCES
    src/main.cpp
    src/window/window.cpp
    src/ui/main_ui.cpp
    src/ui/settings_ui.cpp
    src/ui/setup_ui.cpp
    src/settings/settings.cpp
    src/tray/tray.cpp
    src/media/media.cpp
    ${CALENDAR_SOURCES}
    ${IMGUI_SOURCES}
)

set(PLATFORM_LIBS d3d11 dxgi dwmapi d3dcompiler dcomp runtimeobject windowscodecs coremessaging ole32 winhttp ws2_32 shell32 gdi32 comdlg32 advapi32)

add_executable(${PROJECT_NAME} ${APP_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${PLATFORM_LIBS})

# Self-contained release exe (no libstdc++/libgcc DLLs to ship).
target_link_options(${PROJECT_NAME} PRIVATE
    -static-libstdc++ -static-libgcc -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic
)

add_library(${PROJECT_NAME}_shared SHARED ${APP_SOURCES})

target_compile_definitions(${PROJECT_NAME}_shared PRIVATE
    OPTINOTCH_BUILD_AS_DLL
)

target_include_directories(${PROJECT_NAME}_shared PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(${PROJECT_NAME}_shared PRIVATE ${PLATFORM_LIBS})

target_link_options(${PROJECT_NAME}_shared PRIVATE
    -static-libstdc++ -static-libgcc -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic
)
