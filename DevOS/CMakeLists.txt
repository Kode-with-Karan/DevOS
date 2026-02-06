cmake_minimum_required(VERSION 3.20)
project(DevOS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable Qt automatic features
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Make sure CMake can find Qt6 (your MinGW installation)
set(CMAKE_PREFIX_PATH "C:/Qt/6.10.2/mingw_64")  # <-- update if different

# Find Qt6 packages
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Network WebSockets Concurrent)

# Sources and headers
set(SOURCES
    main.cpp
    core/ApiClient.cpp
    core/WebSocketClient.cpp
    core/IDELauncher.cpp
    core/SandboxValidator.cpp
    ui/MainWindow.cpp
    ui/LogViewerWidget.cpp
    ui/FileTreeWidget.cpp
)

set(HEADERS
    core/ApiClient.h
    core/WebSocketClient.h
    core/IDELauncher.h
    core/SandboxValidator.h
    ui/MainWindow.h
    ui/LogViewerWidget.h
    ui/FileTreeWidget.h
)

# Create executable
add_executable(DevOS ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(DevOS PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link Qt6 libraries
target_link_libraries(DevOS PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::WebSockets
    Qt6::Concurrent
)

# Compiler options for MSVC (optional)
if(MSVC)
    target_compile_options(DevOS PRIVATE /W4 /permissive- /utf-8)
endif()
