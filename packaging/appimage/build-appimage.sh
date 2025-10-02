#!/bin/bash

# AppImage build script for Pineapple Steam Recording Exporter
set -e

# Configuration
APP_NAME="PineappleSteamRecordingExporter"
APP_ID="net.blumia.pineapple.steam-recording-exporter"
VERSION="1.0.0"
BUILD_DIR="build-appimage"
INSTALL_DIR="AppDir"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check required tools
check_dependencies() {
    print_status "Checking dependencies..."
    
    local missing_deps=()
    
    if ! command -v cmake &> /dev/null; then
        missing_deps+=("cmake")
    fi
    
    if ! command -v ninja &> /dev/null && ! command -v make &> /dev/null; then
        missing_deps+=("ninja or make")
    fi
    
    if ! command -v qmake6 &> /dev/null && ! command -v qmake &> /dev/null; then
        missing_deps+=("Qt6 development tools")
    fi
    
    if ! command -v ffmpeg &> /dev/null; then
        print_warning "FFmpeg not found in PATH. The application will need FFmpeg at runtime."
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "Missing dependencies: ${missing_deps[*]}"
        print_error "Please install the required dependencies and try again."
        exit 1
    fi
    
    print_status "All required dependencies found."
}

# Download linuxdeploy and qt plugin if needed
download_linuxdeploy() {
    print_status "Setting up linuxdeploy..."
    
    if [ ! -f "linuxdeploy-x86_64.AppImage" ]; then
        print_status "Downloading linuxdeploy..."
        wget -q "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
        chmod +x linuxdeploy-x86_64.AppImage
    fi
    
    if [ ! -f "linuxdeploy-plugin-qt-x86_64.AppImage" ]; then
        print_status "Downloading linuxdeploy Qt plugin..."
        wget -q "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
        chmod +x linuxdeploy-plugin-qt-x86_64.AppImage
    fi
}

# Build the application
build_application() {
    print_status "Building application..."
    
    # Clean previous build
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Configure with CMake
    print_status "Configuring with CMake..."
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_PREFIX_PATH="$(qmake6 -query QT_INSTALL_PREFIX 2>/dev/null || qmake -query QT_INSTALL_PREFIX)"
    
    # Build
    print_status "Compiling..."
    if command -v ninja &> /dev/null; then
        ninja
    else
        make -j$(nproc)
    fi
    
    cd ..
}

# Install to AppDir
install_to_appdir() {
    print_status "Installing to AppDir..."
    
    rm -rf "$INSTALL_DIR"
    
    cd "$BUILD_DIR"
    if command -v ninja &> /dev/null; then
        DESTDIR="../$INSTALL_DIR" ninja install
    else
        DESTDIR="../$INSTALL_DIR" make install
    fi
    cd ..
    
    # Ensure proper directory structure
    mkdir -p "$INSTALL_DIR/usr/share/applications"
    mkdir -p "$INSTALL_DIR/usr/share/icons/hicolor/256x256/apps"
    mkdir -p "$INSTALL_DIR/usr/share/metainfo"
    
    # Copy desktop file
    cp packaging/linux/${APP_ID}.desktop "$INSTALL_DIR/usr/share/applications/"
    
    # Copy icon
    cp qml/app_icon.png "$INSTALL_DIR/usr/share/icons/hicolor/256x256/apps/${APP_ID}.png"
    
    # Copy metainfo
    cp packaging/linux/${APP_ID}.metainfo.xml "$INSTALL_DIR/usr/share/metainfo/"
    
    # Create AppRun script
    cat > "$INSTALL_DIR/AppRun" << 'EOF'
#!/bin/bash
HERE="$(dirname "$(readlink -f "${0}")")"
export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="${HERE}/usr/plugins:${QT_PLUGIN_PATH}"
export QML_IMPORT_PATH="${HERE}/usr/qml:${QML_IMPORT_PATH}"
export QML2_IMPORT_PATH="${HERE}/usr/qml:${QML2_IMPORT_PATH}"
export XDG_DATA_DIRS="${HERE}/usr/share:${XDG_DATA_DIRS}"

# Try to use system FFmpeg first, fallback to bundled if available
if ! command -v ffmpeg &> /dev/null; then
    if [ -f "${HERE}/usr/bin/ffmpeg" ]; then
        export PATH="${HERE}/usr/bin:${PATH}"
    else
        echo "Warning: FFmpeg not found. Please install FFmpeg system-wide for video export functionality."
    fi
fi

exec "${HERE}/usr/bin/PineappleSteamRecordingExporter" "$@"
EOF
    
    chmod +x "$INSTALL_DIR/AppRun"
    
    # Create desktop file in root
    cp packaging/linux/${APP_ID}.desktop "$INSTALL_DIR/"
    
    # Copy icon to root
    cp qml/app_icon.png "$INSTALL_DIR/${APP_ID}.png"
    cp qml/app_icon.png "$INSTALL_DIR/.DirIcon"
}

# Create AppImage
create_appimage() {
    print_status "Creating AppImage..."
    
    export QML_SOURCES_PATHS="$(pwd)/qml"
    export QMAKE="$(which qmake6 2>/dev/null || which qmake)"
    
    # Run linuxdeploy
    ./linuxdeploy-x86_64.AppImage \
        --appdir "$INSTALL_DIR" \
        --plugin qt \
        --output appimage \
        --desktop-file="$INSTALL_DIR/${APP_ID}.desktop"
    
    # Rename the output AppImage
    if [ -f "${APP_NAME}-x86_64.AppImage" ]; then
        mv "${APP_NAME}-x86_64.AppImage" "${APP_NAME}-${VERSION}-x86_64.AppImage"
        print_status "AppImage created successfully: ${APP_NAME}-${VERSION}-x86_64.AppImage"
    else
        print_error "Failed to create AppImage"
        exit 1
    fi
}

# Cleanup function
cleanup() {
    print_status "Cleaning up temporary files..."
    rm -rf "$BUILD_DIR" "$INSTALL_DIR"
}

# Main execution
main() {
    print_status "Starting AppImage build process..."
    print_status "App: $APP_NAME"
    print_status "Version: $VERSION"
    print_status "Build directory: $BUILD_DIR"
    print_status "Install directory: $INSTALL_DIR"
    
    # Change to script directory
    cd "$(dirname "$0")/../.."
    
    check_dependencies
    download_linuxdeploy
    build_application
    install_to_appdir
    create_appimage
    
    print_status "AppImage build completed successfully!"
    print_status "Output: $(pwd)/${APP_NAME}-${VERSION}-x86_64.AppImage"
    
    # Optionally cleanup
    if [ "$1" != "--keep-build" ]; then
        cleanup
    else
        print_status "Build files kept (use without --keep-build to clean up)"
    fi
}

# Handle script arguments
case "$1" in
    --help|-h)
        echo "Usage: $0 [--keep-build] [--help]"
        echo ""
        echo "Options:"
        echo "  --keep-build    Keep build directories after completion"
        echo "  --help, -h      Show this help message"
        exit 0
        ;;
    *)
        main "$@"
        ;;
esac