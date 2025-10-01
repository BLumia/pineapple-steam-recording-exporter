import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PineappleSteamRecordingExporter

Page {
    id: root
    
    property RecordingClip clip: null
    
    signal segmentSelected(int segmentIndex)
    signal backRequested()
    
    title: qsTr("Select Segment")
    
    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.margins: 8
            
            ToolButton {
                text: "←"
                font.pixelSize: 18
                onClicked: root.backRequested()
            }
            
            Column {
                Layout.fillWidth: true
                spacing: 2
                
                Label {
                    text: root.clip && root.clip.gameName ? 
                          root.clip.gameName : 
                          qsTr("Game ID: %1").arg(root.clip ? root.clip.appId : "")
                    font.pixelSize: 16
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                }
                
                Label {
                    text: root.clip ? 
                          qsTr("%1 segments • %2").arg(root.clip.segmentCount).arg(root.clip.formattedDate) : 
                          ""
                    font.pixelSize: 12
                    color: Material.hintTextColor
                }
            }
        }
    }
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16
        
        // Info card
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: infoColumn.implicitHeight + 32
            
            color: Material.backgroundColor
            border.color: Material.dividerColor
            border.width: 1
            radius: 8
            
            ColumnLayout {
                id: infoColumn
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8
                
                Label {
                    text: qsTr("Multiple Segments Found")
                    font.pixelSize: 16
                    font.weight: Font.Medium
                }
                
                Label {
                    text: qsTr("This recording contains multiple segments. Please select which segment you want to preview and export.")
                    font.pixelSize: 14
                    color: Material.hintTextColor
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }
        
        // Segments list
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            
            ListView {
                id: segmentsListView
                
                model: root.clip ? root.clip.segmentCount : 0
                spacing: 8
                
                delegate: ItemDelegate {
                    width: segmentsListView.width
                    height: 80
                    
                    onClicked: {
                        root.segmentSelected(index)
                    }
                    
                    background: Rectangle {
                        color: parent.hovered ? Material.color(Material.Grey, Material.Shade800) : Material.backgroundColor
                        border.color: Material.dividerColor
                        border.width: 1
                        radius: 6
                        
                        Behavior on color {
                            ColorAnimation { duration: 150 }
                        }
                    }
                    
                    contentItem: RowLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 16
                        
                        // Segment icon
                        Rectangle {
                            Layout.preferredWidth: 48
                            Layout.preferredHeight: 48
                            Layout.alignment: Qt.AlignCenter
                            
                            color: Material.primary
                            radius: 24
                            
                            Label {
                                anchors.centerIn: parent
                                text: (index + 1).toString()
                                font.pixelSize: 18
                                font.weight: Font.Bold
                                color: "white"
                            }
                        }
                        
                        // Segment info
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            
                            Label {
                                text: qsTr("Segment %1").arg(index + 1)
                                font.pixelSize: 16
                                font.weight: Font.Medium
                                Layout.fillWidth: true
                            }
                            
                            Label {
                                text: {
                                    if (!root.clip) return ""
                                    var mpdPath = root.clip.getSegmentMpdPath(index)
                                    if (mpdPath) {
                                        return qsTr("Ready for export")
                                    } else {
                                        return qsTr("Unable to locate segment data")
                                    }
                                }
                                font.pixelSize: 12
                                color: {
                                    if (!root.clip) return Material.hintTextColor
                                    var mpdPath = root.clip.getSegmentMpdPath(index)
                                    return mpdPath ? Material.color(Material.Green) : Material.color(Material.Red)
                                }
                                Layout.fillWidth: true
                            }
                            
                            Label {
                                text: {
                                    if (!root.clip) return ""
                                    var mpdPath = root.clip.getSegmentMpdPath(index)
                                    if (mpdPath) {
                                        // Extract segment path for display
                                        var pathParts = mpdPath.split('/')
                                        if (pathParts.length >= 2) {
                                            return pathParts[pathParts.length - 2] // Parent directory name
                                        }
                                    }
                                    return ""
                                }
                                font.pixelSize: 11
                                color: Material.hintTextColor
                                font.family: "Consolas, Monaco, monospace"
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                        }
                        
                        // Status indicator
                        Label {
                            text: {
                                if (!root.clip) return "?"
                                var mpdPath = root.clip.getSegmentMpdPath(index)
                                return mpdPath ? "✓" : "✗"
                            }
                            font.pixelSize: 20
                            color: {
                                if (!root.clip) return Material.hintTextColor
                                var mpdPath = root.clip.getSegmentMpdPath(index)
                                return mpdPath ? Material.color(Material.Green) : Material.color(Material.Red)
                            }
                        }
                        
                        // Action indicator
                        Label {
                            text: "›"
                            font.pixelSize: 20
                            color: Material.hintTextColor
                        }
                    }
                    
                    // Press effect
                    states: [
                        State {
                            name: "pressed"
                            when: pressed
                            PropertyChanges {
                                target: background
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
                
                // Add some padding at the bottom
                footer: Item { height: 20 }
            }
        }
        
        // Action buttons
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: qsTr("Export All")
                enabled: root.clip && root.clip.segmentCount > 1
                onClicked: {
                    // TODO: Implement export all functionality
                    console.log("Export all segments not yet implemented")
                }
                
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Export all segments as separate files (Not yet implemented)")
                ToolTip.delay: 1000
            }
        }
    }
    
    // Empty state when no clip is provided
    Label {
        anchors.centerIn: parent
        text: qsTr("No recording clip selected")
        font.pixelSize: 18
        color: Material.hintTextColor
        visible: !root.clip
    }
}