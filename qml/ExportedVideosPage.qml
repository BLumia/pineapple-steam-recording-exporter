import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import PineappleSteamRecordingExporter
import "components"

Page {
    id: root
    
    signal backRequested()
    signal videoSelected(var video)
    
    property alias model: listView.model
    
    header: ToolBar {
        Material.elevation: 4
        
        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            
            ToolButton {
                icon.name: "go-previous"
                text: qsTr("Back")
                onClicked: root.backRequested()
            }
            
            Label {
                text: qsTr("Exported Videos")
                font.pixelSize: 18
                font.bold: true
                Layout.fillWidth: true
            }
            
            ToolButton {
                icon.name: "view-refresh"
                text: qsTr("Refresh")
                onClicked: exportedVideoManager.refreshVideos()
            }
            
            ToolButton {
                icon.name: "document-open"
                text: qsTr("Open Folder")
                onClicked: exportedVideoManager.openExportDirectory()
            }
            
            ToolButton {
                id: sortButton
                icon.name: "view-sort"
                text: qsTr("Sort")
                onClicked: sortMenu.open()
                
                Menu {
                    id: sortMenu
                    
                    MenuItem {
                        text: qsTr("Sort by Name")
                        onTriggered: exportedVideoManager.sortByName()
                    }
                    
                    MenuItem {
                        text: qsTr("Sort by Date")
                        onTriggered: exportedVideoManager.sortByDate()
                    }
                    
                    MenuItem {
                        text: qsTr("Sort by Size")
                        onTriggered: exportedVideoManager.sortBySize()
                    }
                }
            }
            
            ToolButton {
                icon.name: "edit-delete"
                text: qsTr("Delete All")
                enabled: exportedVideoManager && exportedVideoManager.videoCount > 0
                onClicked: deleteAllDialog.open()
            }
        }
    }
    
    // Main content
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12
        
        // HTTP Server Control
        HttpServerControl {
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
        }
        
        // Stats row
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: Material.backgroundColor
                border.color: Material.dividerColor
                border.width: 1
                radius: 8
                
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 4
                    
                    Label {
                        text: exportedVideoManager ? exportedVideoManager.videoCount.toString() : "0"
                        font.pixelSize: 24
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        Layout.fillWidth: true
                    }
                    
                    Label {
                        text: qsTr("Videos")
                        font.pixelSize: 12
                        opacity: 0.7
                        horizontalAlignment: Text.AlignHCenter
                        Layout.fillWidth: true
                    }
                }
            }
            
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: Material.backgroundColor
                border.color: Material.dividerColor
                border.width: 1
                radius: 8
                
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 4
                    
                    Label {
                        text: exportedVideoManager ? exportedVideoManager.totalSizeString : "0 B"
                        font.pixelSize: 24
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        Layout.fillWidth: true
                    }
                    
                    Label {
                        text: qsTr("Total Size")
                        font.pixelSize: 12
                        opacity: 0.7
                        horizontalAlignment: Text.AlignHCenter
                        Layout.fillWidth: true
                    }
                }
            }
        }
        
        // Videos list
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 200
            
            clip: true
            
            ListView {
                id: listView
                
                model: exportedVideoManager
                spacing: 8
                
                delegate: ExportedVideoCard {
                    width: listView.width
                    video: model.video
                    
                    onClicked: root.videoSelected(model.video)
                    onDeleteRequested: {
                        deleteVideoDialog.videoIndex = index
                        deleteVideoDialog.videoName = model.displayName
                        deleteVideoDialog.open()
                    }
                }
                
                // Empty state
                Label {
                    visible: listView.count === 0 && !exportedVideoManager.isScanning
                    anchors.centerIn: parent
                    text: qsTr("No exported videos found.\nExport some recordings first!")
                    font.pixelSize: 16
                    opacity: 0.6
                    horizontalAlignment: Text.AlignHCenter
                }
                
                // Loading indicator
                BusyIndicator {
                    visible: exportedVideoManager && exportedVideoManager.isScanning
                    anchors.centerIn: parent
                    running: visible
                }
            }
        }
    }
    
    // Delete confirmation dialogs
    Dialog {
        id: deleteVideoDialog
        
        property int videoIndex: -1
        property string videoName: ""
        
        anchors.centerIn: parent
        modal: true
        title: qsTr("Delete Video")
        
        Label {
            text: qsTr("Are you sure you want to delete \"%1\"?\nThis action cannot be undone.").arg(deleteVideoDialog.videoName)
            wrapMode: Text.WordWrap
        }
        
        standardButtons: Dialog.Yes | Dialog.No
        
        onAccepted: {
            if (videoIndex >= 0) {
                exportedVideoManager.deleteVideo(videoIndex)
            }
        }
    }
    
    Dialog {
        id: deleteAllDialog
        
        anchors.centerIn: parent
        modal: true
        title: qsTr("Delete All Videos")
        
        Label {
            text: qsTr("Are you sure you want to delete all exported videos?\nThis action cannot be undone.")
            wrapMode: Text.WordWrap
        }
        
        standardButtons: Dialog.Yes | Dialog.No
        
        onAccepted: exportedVideoManager.deleteAllVideos()
    }
}