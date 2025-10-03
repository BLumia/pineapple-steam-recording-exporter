import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import net.blumia.pineapple.streamrecordingexporter

ItemDelegate {
    id: root
    
    property RecordingClip clip: null
    property int segmentIndex: 0
    property bool isSelected: false
    
    signal clicked()
    
    width: parent ? parent.width : 200
    height: 160
    
    onClicked: root.clicked()
    
    background: Rectangle {
        color: root.isSelected ? 
               Material.color(Material.Primary, Material.Shade900) :
               (root.hovered ? Material.color(Material.Grey, Material.Shade800) : Material.backgroundColor)
        border.color: root.isSelected ? Material.primary : Material.dividerColor
        border.width: root.isSelected ? 2 : 1
        radius: 8
        
        Behavior on color {
            ColorAnimation { duration: 150 }
        }
        
        Behavior on border.color {
            ColorAnimation { duration: 150 }
        }
        
        // Simple shadow effect using rectangle
        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            color: "transparent"
            border.color: root.isSelected ? "#40000000" : "#20000000"
            border.width: 1
            radius: 8
            z: -1
        }
    }
    
    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8
        
        // Segment header
        RowLayout {
            Layout.fillWidth: true
            
            Rectangle {
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                
                color: root.isSelected ? Material.primary : Material.accent
                radius: 16
                
                Label {
                    anchors.centerIn: parent
                    text: (root.segmentIndex + 1).toString()
                    font.pixelSize: 14
                    font.weight: Font.Bold
                    color: "white"
                }
            }
            
            Label {
                text: qsTr("Segment %1").arg(root.segmentIndex + 1)
                font.pixelSize: 14
                font.weight: Font.Medium
                Layout.fillWidth: true
            }
            
            Label {
                visible: root.isSelected
                text: "✓"
                font.pixelSize: 16
                color: Material.primary
            }
        }
        
        // Segment details
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 4
            
            Label {
                text: {
                    if (!root.clip) return qsTr("No data")
                    var mpdPath = root.clip.getSegmentMpdPath(root.segmentIndex)
                    return mpdPath ? qsTr("Ready for export") : qsTr("Data not found")
                }
                font.pixelSize: 12
                color: {
                    if (!root.clip) return Material.hintTextColor
                    var mpdPath = root.clip.getSegmentMpdPath(root.segmentIndex)
                    return mpdPath ? Material.color(Material.Green) : Material.color(Material.Red)
                }
                Layout.fillWidth: true
            }
            
            Label {
                text: {
                    if (!root.clip) return ""
                    var mpdPath = root.clip.getSegmentMpdPath(root.segmentIndex)
                    if (mpdPath) {
                        // Extract and display the segment folder name
                        var pathParts = mpdPath.split('/')
                        if (pathParts.length >= 2) {
                            var folderName = pathParts[pathParts.length - 2]
                            // Extract the timestamp part if it follows the pattern fg_<appid>_<date>_<time>
                            var match = folderName.match(/fg_\d+_(\d{8})_(\d{6})/)
                            if (match) {
                                var date = match[1]
                                var time = match[2]
                                var formattedDate = date.substring(0,4) + "-" + date.substring(4,6) + "-" + date.substring(6,8)
                                var formattedTime = time.substring(0,2) + ":" + time.substring(2,4) + ":" + time.substring(4,6)
                                return formattedDate + " " + formattedTime
                            }
                            return folderName
                        }
                    }
                    return qsTr("Path not available")
                }
                font.pixelSize: 10
                color: Material.hintTextColor
                font.family: "Consolas, Monaco, monospace"
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }
            
            Item { Layout.fillHeight: true }
        }
        
        // Status indicator
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 20
            
            color: {
                if (!root.clip) return Material.color(Material.Grey)
                var mpdPath = root.clip.getSegmentMpdPath(root.segmentIndex)
                return mpdPath ? Material.color(Material.Green, Material.Shade700) : Material.color(Material.Red, Material.Shade700)
            }
            radius: 4
            
            Label {
                anchors.centerIn: parent
                text: {
                    if (!root.clip) return qsTr("No Clip")
                    var mpdPath = root.clip.getSegmentMpdPath(root.segmentIndex)
                    return mpdPath ? qsTr("Available") : qsTr("Unavailable")
                }
                font.pixelSize: 10
                font.weight: Font.Medium
                color: "white"
            }
        }
    }
    
    // Hover and press effects
    states: [
        State {
            name: "pressed"
            when: root.pressed
            PropertyChanges {
                target: root.background
                scale: 0.96
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
    
    // Selection indicator animation
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: Material.primary
        border.width: 2
        radius: 8
        opacity: root.isSelected ? 1 : 0
        
        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }
        
        // Animated selection pulse
        SequentialAnimation on border.width {
            running: root.isSelected
            loops: Animation.Infinite
            NumberAnimation { from: 2; to: 3; duration: 1000 }
            NumberAnimation { from: 3; to: 2; duration: 1000 }
        }
    }
}
