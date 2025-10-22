import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import net.blumia.pineapple.streamrecordingexporter

ItemDelegate {
    id: root
    
    property var video: null
    signal deleteRequested()
    
    height: 120
    
    background: Rectangle {
        color: root.hovered ? Material.color(Material.Grey, Material.Shade800) : Material.backgroundColor
        border.color: Material.dividerColor
        border.width: 1
        radius: 8
        
        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }
    
    contentItem: RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16
        
        // Video thumbnail placeholder
        Rectangle {
            Layout.preferredWidth: 140
            Layout.preferredHeight: 88
            Layout.alignment: Qt.AlignVCenter
            
            color: Material.backgroundDimColor
            border.color: Material.dividerColor
            border.width: 1
            radius: 4
            
            Text {
                anchors.centerIn: parent
                text: getIconText("media-playback-start")
                font.pixelSize: 32
                color: Material.iconColor
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            
            // Play overlay
            Rectangle {
                anchors.fill: parent
                color: "transparent"
                border.color: root.hovered ? Material.accent : "transparent"
                border.width: 2
                radius: 4
                
                Behavior on border.color {
                    ColorAnimation { duration: 150 }
                }
            }
        }
        
        // Video info
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 4
            
            Label {
                text: video ? video.displayName : ""
                font.pixelSize: 16
                font.bold: true
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                elide: Text.ElideRight
                maximumLineCount: 2
            }
            
            Label {
                text: video ? video.fileName : ""
                font.pixelSize: 12
                opacity: 0.7
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            
            RowLayout {
                Layout.fillWidth: true
                spacing: 16
                
                Label {
                    text: video ? video.fileSizeString : ""
                    font.pixelSize: 11
                    opacity: 0.6
                }
                
                Label {
                    text: video ? video.creationTimeString : ""
                    font.pixelSize: 11
                    opacity: 0.6
                }
            }
        }
        
        // Action buttons
        RowLayout {
            Layout.alignment: Qt.AlignVCenter
            
            ToolButton {
                icon.name: "media-playback-start"
                text: qsTr("Play")
                onClicked: {
                    if (video) {
                        video.playWithDefaultPlayer()
                    }
                }
                
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Play with default player")
            }
            
            ToolButton {
                icon.name: "document-open"
                text: qsTr("Show")
                onClicked: {
                    if (video) {
                        video.openInExplorer()
                    }
                }
                
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Show in file explorer")
            }
            
            ToolButton {
                icon.name: "edit-delete"
                text: qsTr("Delete")
                onClicked: root.deleteRequested()
                
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Delete video file")
            }
        }
    }
    
    // Helper function to get icon text
    function getIconText(iconName) {
        switch (iconName) {
        case "media-playback-start":
            return "▶"
        case "document-open":
            return "📁"
        case "edit-delete":
            return "🗑"
        default:
            return "?"
        }
    }
}
