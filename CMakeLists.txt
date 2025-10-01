cmake_minimum_required(VERSION 3.21)
project(PineappleSteamRecordingExporter VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required Qt6 components
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Quick QuickControls2 Multimedia)

# Enable Qt tools
qt_standard_project_setup()

# Source files
set(SOURCES
    src/main.cpp
    src/systemchecker.cpp
    src/gameinfo.cpp
    src/recordingclip.cpp
    src/steamrecordingmanager.cpp
    src/videoexporter.cpp
)

# Header files
set(HEADERS
    src/systemchecker.h
    src/gameinfo.h
    src/recordingclip.h
    src/steamrecordingmanager.h
    src/videoexporter.h
)

# Create the executable
qt_add_executable(PineappleSteamRecordingExporter
    WIN32
    ${SOURCES}
    ${HEADERS}
)

# Add QML module
qt_add_qml_module(PineappleSteamRecordingExporter
    URI PineappleSteamRecordingExporter
    VERSION 1.0
    QML_FILES
        qml/main.qml
        qml/SystemCheckPage.qml
        qml/RecordingListPage.qml
        qml/ClipSelectionPage.qml
        qml/PreviewAndExportPage.qml
        qml/components/RecordingCard.qml
        qml/components/ClipCard.qml
)

# Add resources
qt_add_resources(PineappleSteamRecordingExporter "icons"
    FILES
        qml/app_icon.png
)

# Link Qt libraries
target_link_libraries(PineappleSteamRecordingExporter PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
)

# Include directories
target_include_directories(PineappleSteamRecordingExporter PRIVATE src)

# Compiler-specific settings
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # MinGW/MSYS2 settings
    target_compile_options(PineappleSteamRecordingExporter PRIVATE
        -Wall -Wextra -Wpedantic
    )
elseif(MSVC)
    # MSVC settings
    target_compile_options(PineappleSteamRecordingExporter PRIVATE
        /W4
    )
endif()

# Set target properties
set_target_properties(PineappleSteamRecordingExporter PROPERTIES
    WIN32_EXECUTABLE TRUE
)
