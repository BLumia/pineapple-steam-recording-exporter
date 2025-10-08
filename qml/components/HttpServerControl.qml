import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import net.blumia.pineapple.streamrecordingexporter

Rectangle {
    id: root
    
    property alias server: serverControl
    
    implicitHeight: mainLayout.implicitHeight + 16
    
    color: Material.backgroundColor
    border.color: Material.dividerColor
    border.width: 1
    radius: 6
    
    ColumnLayout {
        id: mainLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        spacing: 6
        
        // Compact header row
        Item {
            Layout.fillWidth: true
            height: httpServerHeaderRow.implicitHeight
            RowLayout {
                id: httpServerHeaderRow
                width: parent.width
                spacing: 8

                Label {
                    text: qsTr("HTTP Server")
                    font.pixelSize: 13
                    font.weight: Font.Medium
                }

                // Status indicator
                Rectangle {
                    id: httpServerStatusLabel
                    width: 6
                    height: 6
                    radius: 3
                    color: httpServer && httpServer.isRunning ? "#4CAF50" : "#757575"

                    SequentialAnimation {
                        running: httpServer && httpServer.isRunning
                        loops: Animation.Infinite

                        PropertyAnimation {
                            target: httpServerStatusLabel
                            property: "opacity"
                            to: 0.4
                            duration: 1000
                        }
                        PropertyAnimation {
                            target: httpServerStatusLabel
                            property: "opacity"
                            to: 1.0
                            duration: 1000
                        }
                    }
                }

                Label {
                    text: httpServer && httpServer.isRunning ? qsTr("Running") : qsTr("Stopped")
                    font.pixelSize: 11
                    color: httpServer && httpServer.isRunning ? "#4CAF50" : "#757575"
                    opacity: httpServerStatusLabel.opacity
                }

                Item { Layout.fillWidth: true }

                // Connection count
                Label {
                    visible: httpServer && httpServer.connectionCount > 0
                    text: httpServer ? httpServer.connectionCount.toString() : "0"
                    font.pixelSize: 11
                    color: Material.accent
                }

                // Expand/collapse button
                ToolButton {
                    id: expandButton

                    property bool expanded: false

                    icon.name: expanded ? "go-up" : "go-down"

                    implicitWidth: 24
                    implicitHeight: 24

                    onClicked: expanded = !expanded

                    ToolTip.visible: hovered
                    ToolTip.text: expanded ? qsTr("Hide details") : qsTr("Show details")
                }
            }
            MouseArea {
                anchors.fill: parent
                onClicked: expandButton.expanded = !expandButton.expanded
            }
        }
        
        // Expandable details section
        ColumnLayout {
            Layout.fillWidth: true
            visible: expandButton.expanded
            spacing: 6
            
            // Controls row
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                
                Button {
                    id: serverControl
                    text: httpServer && httpServer.isRunning ? qsTr("Stop") : qsTr("Start")
                    
                    onClicked: {
                        if (httpServer) {
                            if (httpServer.isRunning) {
                                httpServer.stopServer()
                            } else {
                                httpServer.startServer()
                            }
                        }
                    }
                    
                    Material.background: httpServer && httpServer.isRunning ? 
                        Material.color(Material.Red, Material.Shade400) : 
                        Material.color(Material.Green, Material.Shade400)
                }
                
                Label {
                    text: qsTr("Port:")
                }
                
                SpinBox {
                    id: portSpinBox
                    from: 1024
                    to: 65535
                    value: httpServer ? httpServer.port : 6487
                    enabled: !httpServer || !httpServer.isRunning
                    
                    onValueChanged: {
                        if (httpServer && value !== httpServer.port) {
                            httpServer.port = value
                        }
                    }
                }
                
                Item { Layout.fillWidth: true }
                
                ToolButton {
                    visible: httpServer && httpServer.isRunning
                    icon.name: "edit-copy"
                    
                    onClicked: {
                        if (httpServer) {
                            // Try to copy to clipboard
                            console.log("Copying URL:", httpServer.serverUrl)
                            copyNotification.show()
                        }
                    }
                    
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Copy server URL")
                }
                
                ToolButton {
                    visible: httpServer && httpServer.isRunning
                    icon.name: Qt.platform.os === "windows" ? "insert-link" : "open-link"
                    
                    onClicked: {
                        if (httpServer && httpServer.isRunning) {
                            Qt.openUrlExternally(httpServer.serverUrl)
                        }
                    }
                    
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Open in browser")
                }
            }
            
            // Server URL display (when running)
            TextField {
                Layout.fillWidth: true
                visible: httpServer && httpServer.isRunning
                text: httpServer ? httpServer.serverUrl : ""
                readOnly: true
                selectByMouse: true
                font.family: "monospace"
                
                background: Rectangle {
                    color: "transparent"
                    border.color: Material.dividerColor
                    border.width: 1
                    radius: 3
                }
            }
            
            // Error message
            Label {
                Layout.fillWidth: true
                visible: httpServer && httpServer.lastError !== ""
                text: httpServer ? httpServer.lastError : ""
                color: Material.color(Material.Red)
                font.pixelSize: 10
                wrapMode: Text.WordWrap
            }
            
            // Quick help
            Rectangle {
                Layout.fillWidth: true
                visible: !httpServer || !httpServer.isRunning
                implicitHeight: helpText.implicitHeight + 12
                color: Material.color(Material.Blue, Material.Shade50)
                border.color: Material.color(Material.Blue, Material.Shade200)
                border.width: 1
                radius: 4
                
                Label {
                    id: helpText
                    anchors.fill: parent
                    anchors.margins: 6
                    text: qsTr("💡 Start the server to share your videos over the network")
                    font.pixelSize: 10
                    wrapMode: Text.WordWrap
                    color: Material.color(Material.Blue, Material.Shade800)
                }
            }
        }
    }
    
    // Copy success notification
    Rectangle {
        id: copyNotification
        
        visible: false
        width: 70
        height: 20
        color: Material.accent
        radius: 10
        anchors.centerIn: parent
        
        Label {
            anchors.centerIn: parent
            text: qsTr("Copied!")
            color: "white"
            font.pixelSize: 9
        }
        
        function show() {
            visible = true
            hideTimer.restart()
        }
        
        Timer {
            id: hideTimer
            interval: 1500
            onTriggered: copyNotification.visible = false
        }
    }
}
