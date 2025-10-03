import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import net.blumia.pineapple.streamrecordingexporter

ItemDelegate {
    id: root
    
    property RecordingClip recordingClip: null
    
    signal cardClicked()
    
    width: parent ? parent.width : 300
    height: 120
    
    onClicked: {
        console.log("RecordingCard clicked - recordingClip:", root.recordingClip)
        console.log("RecordingCard clicked - appId:", root.recordingClip ? root.recordingClip.appId : "null")
        root.cardClicked()
    }
    
    background: Rectangle {
        color: root.hovered ? Material.color(Material.Grey, Material.Shade800) : Material.backgroundColor
        border.color: Material.dividerColor
        border.width: 1
        radius: 8
        
        Behavior on color {
            ColorAnimation { duration: 150 }
        }
        
        // Simple shadow effect using rectangle
        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            color: "transparent"
            border.color: "#20000000"
            border.width: 1
            radius: 8
            z: -1
        }
    }
    
    contentItem: RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16
        
        // Thumbnail
        Rectangle {
            Layout.preferredWidth: 88
            Layout.preferredHeight: 88
            Layout.alignment: Qt.AlignTop
            
            color: Material.backgroundDimColor
            border.color: Material.dividerColor
            border.width: 1
            radius: 4
            
            Image {
                id: thumbnailImage
                anchors.fill: parent
                anchors.margins: 1
                
                source: root.recordingClip && root.recordingClip.thumbnailPath ? 
                        "file:///" + root.recordingClip.thumbnailPath : ""
                fillMode: Image.PreserveAspectCrop
                
                visible: status === Image.Ready
                
                // Simple radius clipping
                clip: true
            }
            
            // Fallback icon when no thumbnail
            Label {
                anchors.centerIn: parent
                text: "🎬"
                font.pixelSize: 32
                visible: !thumbnailImage.visible
                color: Material.hintTextColor
            }
            
            // Segment count indicator
            Rectangle {
                visible: root.recordingClip && root.recordingClip.segmentCount > 1
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 4
                
                width: segmentLabel.implicitWidth + 8
                height: segmentLabel.implicitHeight + 4
                radius: 2
                color: Material.primary
                
                Label {
                    id: segmentLabel
                    anchors.centerIn: parent
                    text: root.recordingClip ? root.recordingClip.segmentCount.toString() : ""
                    font.pixelSize: 10
                    font.weight: Font.Bold
                    color: "white"
                }
            }
        }
        
        // Content
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 4
            
            // Game name
            Label {
                text: {
                    if (!root.recordingClip) return ""
                    if (root.recordingClip.gameName && root.recordingClip.gameName !== "") {
                        return root.recordingClip.gameName
                    }
                    return qsTr("Game ID: %1").arg(root.recordingClip.appId)
                }
                font.pixelSize: 16
                font.weight: Font.Medium
                color: Material.foreground
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            
            // Recording date
            Label {
                text: root.recordingClip ? root.recordingClip.formattedDate : ""
                font.pixelSize: 12
                color: Material.hintTextColor
                Layout.fillWidth: true
            }
            
            // Spacer
            Item { Layout.fillHeight: true }
            
            // Bottom info row
            RowLayout {
                Layout.fillWidth: true
                spacing: 16
                
                // Duration
                Row {
                    spacing: 4
                    
                    Label {
                        text: "⏱"
                        font.pixelSize: 12
                        color: Material.hintTextColor
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    
                    Label {
                        text: root.recordingClip ? root.recordingClip.formattedDuration : "00:00"
                        font.pixelSize: 12
                        color: Material.hintTextColor
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                
                // File size
                Row {
                    spacing: 4
                    
                    Label {
                        text: "💾"
                        font.pixelSize: 12
                        color: Material.hintTextColor
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    
                    Label {
                        text: root.recordingClip ? root.recordingClip.formattedSize : "0 MB"
                        font.pixelSize: 12
                        color: Material.hintTextColor
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                
                Item { Layout.fillWidth: true }
                
                // Segments indicator
                Label {
                    visible: root.recordingClip && root.recordingClip.segmentCount > 1
                    text: qsTr("%1 segments").arg(root.recordingClip ? root.recordingClip.segmentCount : 0)
                    font.pixelSize: 12
                    color: Material.primary
                    font.weight: Font.Medium
                }
            }
        }
        
        // Action indicator
        Label {
            text: "›"
            font.pixelSize: 20
            color: Material.hintTextColor
            Layout.alignment: Qt.AlignCenter
        }
    }

    
    // Hover and press effects
    states: [
        State {
            name: "pressed"
            when: root.pressed
            PropertyChanges {
                target: root.background
                scale: 0.98
            }
        }
    ]
    
    transitions: [
        Transition {
            to: "pressed"
            reversible: true
            NumberAnimation {
                properties: "scale"
                duration: 100
                easing.type: Easing.OutCubic
            }
        }
    ]
}
