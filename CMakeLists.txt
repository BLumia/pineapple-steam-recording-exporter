cmake_minimum_required(VERSION 3.21)
project(PineappleSteamRecordingExporter VERSION 0.5.0 LANGUAGES CXX)

# Include GNUInstallDirs for standard installation directories
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required Qt6 components
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets Quick QuickControls2 Multimedia HttpServer LinguistTools)
find_package(KF6IconThemes)

include(GNUInstallDirs)

# Enable Qt tools
qt_standard_project_setup(I18N_TRANSLATED_LANGUAGES en zh_CN de nl pl tr)

# Source files
set(SOURCES
    src/main.cpp
    src/systemchecker.cpp
    src/gameinfo.cpp
    src/recordingclip.cpp
    src/steamrecordingmanager.cpp
    src/videoexporter.cpp
    src/exportedvideomanager.cpp
    src/httpserver.cpp
    src/qvdfparser.cpp
    src/languagemanager.cpp
)

# Header files
set(HEADERS
    src/systemchecker.h
    src/gameinfo.h
    src/recordingclip.h
    src/steamrecordingmanager.h
    src/videoexporter.h
    src/exportedvideomanager.h
    src/httpserver.h
    src/qvdfparser.h
    src/languagemanager.h
)

# Create the executable
qt_add_executable(PineappleSteamRecordingExporter
    WIN32
    ${SOURCES}
    ${HEADERS}
)

# Add translations
qt_add_translations(PineappleSteamRecordingExporter
    TS_FILE_DIR translations # Before Qt 6.9, the argument TS_OUTPUT_DIRECTORY was called TS_FILE_DIR. That name is still an alias for TS_OUTPUT_DIRECTORY to keep older project files working.
    RESOURCE_PREFIX "/i18n"
)

# Add QML module
qt_add_qml_module(PineappleSteamRecordingExporter
    URI net.blumia.pineapple.streamrecordingexporter
    VERSION 1.0
    QML_FILES
        qml/main.qml
        qml/SystemCheckPage.qml
        qml/RecordingListPage.qml
        qml/ClipSelectionPage.qml
        qml/PreviewAndExportPage.qml
        qml/VideoPreviewPage.qml
        qml/ExportedVideosPage.qml

        qml/components/RecordingCard.qml
        qml/components/ClipCard.qml
        qml/components/ExportedVideoCard.qml
        qml/components/HttpServerControl.qml
        qml/components/SteamUserSelectionDialog.qml

)

# Add resources
qt_add_resources(PineappleSteamRecordingExporter "icons"
    FILES
        qml/app_icon.png
)

# Add web resources
qt_add_resources(PineappleSteamRecordingExporter "web"
    PREFIX "/web"
    FILES
        web/index.html
)

# Link Qt libraries
target_link_libraries(PineappleSteamRecordingExporter PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Qt6::HttpServer
)

if(TARGET KF6::IconThemes)
    target_link_libraries(PineappleSteamRecordingExporter PRIVATE KF6::IconThemes)
    target_compile_definitions(PineappleSteamRecordingExporter PRIVATE HAVE_KICONTHEME=1)
endif ()

target_compile_definitions(PineappleSteamRecordingExporter PRIVATE PSRE_VERSION="${PROJECT_VERSION}")

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

# Installation rules
if(WIN32)
    # Windows installation
    install(TARGETS PineappleSteamRecordingExporter
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    # FIXME: Figure out why QtMultimedia plugin is not deployed.
    # qt_generate_deploy_app_script(
    #     TARGET PineappleSteamRecordingExporter
    #     OUTPUT_SCRIPT deploy_script
    # )
    # install(SCRIPT ${deploy_script})
else()
    # Linux installation
    install(TARGETS PineappleSteamRecordingExporter
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    # Install desktop file
    install(FILES packaging/linux/net.blumia.pineapple-steam-recording-exporter.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
    )

    # Install AppStream metainfo
    install(FILES packaging/linux/net.blumia.pineapple-steam-recording-exporter.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo
    )

    # Install icon
    install(FILES qml/app_icon.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/128x128/apps
        RENAME net.blumia.pineapple-steam-recording-exporter.png
    )
    # Install SVG icon in scalable directory according to Freedesktop.org icon theme specification
    install(FILES qml/app_icon.svg
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps
        RENAME net.blumia.pineapple-steam-recording-exporter.svg
    )

    # # Install documentation
    # if(EXISTS "${CMAKE_SOURCE_DIR}/README.md")
    #     install(FILES README.md
    #         DESTINATION ${CMAKE_INSTALL_DOCDIR}
    #     )
    # endif()

    # # Install license
    # if(EXISTS "${CMAKE_SOURCE_DIR}/LICENSE")
    #     install(FILES LICENSE
    #         DESTINATION ${CMAKE_INSTALL_DOCDIR}
    #     )
    # endif()
endif()

# CPack configuration for packaging
set(CPACK_PACKAGE_NAME "PineappleSteamRecordingExporter")
set(CPACK_PACKAGE_VENDOR "Pineapple Tools")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Export Steam game recordings to MP4 videos")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

if(WIN32)
    # Windows packaging with NSIS
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "Pineapple Steam Recording Exporter")
    set(CPACK_NSIS_PACKAGE_NAME "Pineapple Steam Recording Exporter")
    set(CPACK_NSIS_CONTACT "opensource@blumia.net")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/pineapple-tools/steam-recording-exporter")
    set(CPACK_NSIS_HELP_LINK "https://github.com/pineapple-tools/steam-recording-exporter/wiki")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
else()
    # Linux packaging
    set(CPACK_GENERATOR "DEB;RPM;TGZ")

    # DEB package configuration
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Gary Wang <opensource@blumia.net>")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "ffmpeg, libqt6core6, libqt6gui6, libqt6qml6, libqt6quick6, libqt6multimedia6")
    set(CPACK_DEBIAN_PACKAGE_SECTION "video")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/pineapple-tools/steam-recording-exporter")

    # RPM package configuration
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_URL "https://github.com/pineapple-tools/steam-recording-exporter")
    set(CPACK_RPM_PACKAGE_REQUIRES "ffmpeg, qt6-qtbase, qt6-qtdeclarative, qt6-qtmultimedia")
endif()

include(CPack)
