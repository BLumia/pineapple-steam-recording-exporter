import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import net.blumia.pineapple.streamrecordingexporter

ApplicationWindow {
    id: mainWindow
    
    visible: true
    width: 1200
    height: 800
    minimumWidth: 800
    minimumHeight: 600
    
    title: qsTr("Pineapple Steam Recording Exporter")
    
    Material.theme: Material.Dark
    Material.primary: Material.Orange
    Material.accent: Material.DeepOrange
    
    // Global properties
    property string currentPage: "systemCheck"
    property var selectedClip: null
    property int selectedSegmentIndex: -1
    property var selectedExportedVideo: null
    
    // Auto-fullscreen in SteamOS game mode
    Component.onCompleted: {
        if (systemChecker && systemChecker.isSteamGameMode) {
            mainWindow.showFullScreen()
        }
    }
    
    // Page stack for navigation
    StackView {
        id: pageStack
        anchors.fill: parent
        
        initialItem: SystemCheckPage {
            id: systemCheckPage
            
            onSystemCheckPassed: {
                // Move to recording list page
                pageStack.push(recordingListComponent)
                currentPage = "recordingList"
            }
        }
    }
    
    // Components for different pages
    Component {
        id: recordingListComponent
        
        RecordingListPage {
            onClipSelected: function(clip) {
                console.log("main.qml - Received clipSelected signal with clip:", clip)
                console.log("main.qml - Clip appId:", clip ? clip.appId : "null")
                console.log("main.qml - Clip segmentCount:", clip ? clip.segmentCount : "null")
                selectedClip = clip
                if (clip.segmentCount > 1) {
                    // Show clip selection page for multi-segment recordings
                    console.log("main.qml - Navigating to clip selection (multi-segment)")
                    pageStack.push(clipSelectionComponent)
                    currentPage = "clipSelection"
                } else {
                    // Go directly to preview for single-segment recordings
                    console.log("main.qml - Navigating to preview (single segment)")
                    selectedSegmentIndex = 0
                    pageStack.push(previewAndExportComponent)
                    currentPage = "previewAndExport"
                }
            }
            
            onExportedVideosRequested: {
                pageStack.push(exportedVideosComponent)
                currentPage = "exportedVideos"
            }
            
            onBackRequested: {
                pageStack.pop()
                currentPage = "systemCheck"
            }
        }
    }
    
    Component {
        id: clipSelectionComponent
        
        ClipSelectionPage {
            clip: selectedClip
            
            onSegmentSelected: function(segmentIndex) {
                selectedSegmentIndex = segmentIndex
                pageStack.push(previewAndExportComponent)
                currentPage = "previewAndExport"
            }
            
            onBackRequested: {
                pageStack.pop()
                currentPage = "recordingList"
                selectedClip = null
            }
        }
    }
    
    Component {
        id: previewAndExportComponent
        
        PreviewAndExportPage {
            clip: selectedClip
            segmentIndex: selectedSegmentIndex
            
            onBackRequested: {
                if (selectedClip && selectedClip.segmentCount > 1) {
                    pageStack.pop()
                    currentPage = "clipSelection"
                } else {
                    pageStack.pop()
                    currentPage = "recordingList"
                    selectedClip = null
                }
                selectedSegmentIndex = -1
            }
            
            onExportCompleted: {
                // Could show a success message or navigate back
                console.log("Export completed successfully")
            }
        }
    }
    
    Component {
        id: exportedVideosComponent
        
        ExportedVideosPage {
            onVideoSelected: function(video) {
                selectedExportedVideo = video
                pageStack.push(videoPreviewComponent)
                currentPage = "videoPreview"
            }
            
            onBackRequested: {
                pageStack.pop()
                currentPage = "recordingList"
                selectedExportedVideo = null
            }
        }
    }
    
    Component {
        id: videoPreviewComponent
        
        VideoPreviewPage {
            video: selectedExportedVideo
            
            onBackRequested: {
                pageStack.pop()
                currentPage = "exportedVideos"
                selectedExportedVideo = null
            }
            
            onVideoDeleted: {
                // Go back to exported videos list
                pageStack.pop()
                currentPage = "exportedVideos"
                selectedExportedVideo = null
            }
        }
    }
    
    // Global shortcuts
    Shortcut {
        sequences: [StandardKey.Back]
        enabled: pageStack.depth > 1
        onActivated: {
            if (pageStack.depth > 1) {
                pageStack.pop()
                updateCurrentPage()
            }
        }
    }
    
    Shortcut {
        sequences: ["Escape"]
        enabled: pageStack.depth > 1
        onActivated: {
            if (pageStack.depth > 1) {
                pageStack.pop()
                updateCurrentPage()
            }
        }
    }
    
    Shortcut {
        sequences: [StandardKey.Quit]
        onActivated: Qt.quit()
    }
    
    Shortcut {
        sequences: ["Ctrl+R"]
        enabled: currentPage === "recordingList"
        onActivated: {
            if (recordingManager) {
                recordingManager.refreshClips()
            }
        }
    }
    
    // Functions
    function updateCurrentPage() {
        switch (pageStack.depth) {
        case 1:
            currentPage = "systemCheck"
            break
        case 2:
            currentPage = "recordingList"
            break
        case 3:
            if (selectedClip && selectedClip.segmentCount > 1) {
                currentPage = "clipSelection"
            } else if (selectedClip) {
                currentPage = "previewAndExport"
            } else {
                currentPage = "exportedVideos"
            }
            break
        case 4:
            if (selectedExportedVideo) {
                currentPage = "videoPreview"
            } else {
                currentPage = "previewAndExport"
            }
            break
        }
    }
    
    function goToRecordingList() {
        while (pageStack.depth > 2) {
            pageStack.pop()
        }
        if (pageStack.depth === 1) {
            pageStack.push(recordingListComponent)
        }
        currentPage = "recordingList"
        selectedClip = null
        selectedSegmentIndex = -1
        selectedExportedVideo = null
    }
    
    function goToSystemCheck() {
        while (pageStack.depth > 1) {
            pageStack.pop()
        }
        currentPage = "systemCheck"
        selectedClip = null
        selectedSegmentIndex = -1
        selectedExportedVideo = null
    }
    
    // Handle window close
    onClosing: function(close) {
        if (videoExporter && videoExporter.isExporting) {
            // Show confirmation dialog if export is in progress
            close.accepted = false
            confirmExitDialog.open()
        }
    }
    
    // Exit confirmation dialog
    Dialog {
        id: confirmExitDialog
        
        anchors.centerIn: parent
        modal: true
        title: qsTr("Export in Progress")
        
        Label {
            text: qsTr("An export operation is currently in progress. Are you sure you want to exit?")
            wrapMode: Text.WordWrap
        }
        
        standardButtons: Dialog.Yes | Dialog.No
        
        onAccepted: {
            if (videoExporter) {
                videoExporter.cancelExport()
            }
            Qt.quit()
        }
    }
    
    // Status bar
    footer: ToolBar {
        height: 30
        
        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            
            Label {
                text: {
                    switch (currentPage) {
                    case "systemCheck":
                        return qsTr("System Check")
                    case "recordingList":
                        return qsTr("Recording List") + 
                               (recordingManager ? " (" + recordingManager.clipCount + " clips)" : "")
                    case "clipSelection":
                        return qsTr("Clip Selection")
                    case "previewAndExport":
                        return qsTr("Preview & Export")
                    case "exportedVideos":
                        return qsTr("Exported Videos") + 
                               (exportedVideoManager ? " (" + exportedVideoManager.videoCount + " videos)" : "")
                    case "videoPreview":
                        return selectedExportedVideo ? selectedExportedVideo.displayName : qsTr("Video Preview")
                    default:
                        return ""
                    }
                }
                font.pixelSize: 12
                color: Material.foreground
            }
            
            Item { Layout.fillWidth: true }
            
            Label {
                visible: videoExporter && videoExporter.isExporting
                text: videoExporter ? videoExporter.currentOperation + " (" + videoExporter.progress + "%)" : ""
                font.pixelSize: 12
                color: Material.accent
            }
            
            BusyIndicator {
                visible: (systemChecker && systemChecker.isChecking) ||
                         (recordingManager && recordingManager.isScanning) ||
                         (videoExporter && videoExporter.isExporting) ||
                         (exportedVideoManager && exportedVideoManager.isScanning)
                running: visible
                implicitWidth: 20
                implicitHeight: 20
            }
        }
    }
}
