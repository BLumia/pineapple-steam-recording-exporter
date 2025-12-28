import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import net.blumia.pineapple.streamrecordingexporter

Page {
    id: root
    
    signal systemCheckPassed()
    
    title: qsTr("System Check")
    
    padding: 20
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 20
        
        // Header
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: headerColumn.implicitHeight
            
            ColumnLayout {
                id: headerColumn
                anchors.centerIn: parent
                spacing: 8
                
                Label {
                    text: qsTr("Pineapple Steam Recording Exporter")
                    font.pixelSize: 28
                    font.weight: Font.Bold
                    color: Material.primary
                    Layout.alignment: Qt.AlignHCenter
                }
                
                Label {
                    id: checkStatusLabel
                    text: qsTr("Checking system requirements...")
                    font.pixelSize: 16
                    color: Material.foreground
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }
        
        // Progress indicator
        ProgressBar {
            Layout.fillWidth: true
            Layout.maximumWidth: 400
            Layout.alignment: Qt.AlignHCenter
            
            indeterminate: systemChecker ? systemChecker.isChecking : false
            value: systemChecker && !systemChecker.isChecking ? 
                   (systemChecker.allChecksPass ? 1.0 : 0.5) : 0.0
        }
        
        // Check results area
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 150
            
            clip: true
            
            TextArea {
                id: resultsArea
                
                text: systemChecker ? systemChecker.checkResults.join('\n') : ""
                readOnly: true
                selectByMouse: true
                wrapMode: Text.WordWrap
                
                font.family: "Consolas, Monaco, monospace"
                font.pixelSize: 12
                
                background: Rectangle {
                    implicitHeight: Material.textFieldHeight
                    color: Material.backgroundDimColor
                    border.color: Material.dividerColor
                    border.width: 1
                    radius: 4
                }
            }
        }
        
        // Status indicators
        Row {
            Layout.alignment: Qt.AlignHCenter
            spacing: 40
            
            // Steam status
            Column {
                spacing: 8
                
                Row {
                    spacing: 8
                    anchors.horizontalCenter: parent.horizontalCenter
                    
                    Rectangle {
                        width: 16
                        height: 16
                        radius: 8
                        color: systemChecker && systemChecker.steamFound ? 
                               Material.color(Material.Green) : 
                               Material.color(Material.Red)
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    
                    Label {
                        text: qsTr("Steam")
                        font.weight: Font.Medium
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                
                Label {
                    text: systemChecker && systemChecker.steamFound ?
                          qsTr("Found") : qsTr("Not Found")
                    font.pixelSize: 12
                    color: Material.hintTextColor
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
            
            // FFmpeg status
            Column {
                spacing: 8
                
                Row {
                    spacing: 8
                    anchors.horizontalCenter: parent.horizontalCenter
                    
                    Rectangle {
                        width: 16
                        height: 16
                        radius: 8
                        color: systemChecker && systemChecker.ffmpegFound ? 
                               Material.color(Material.Green) : 
                               Material.color(Material.Red)
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    
                    Label {
                        text: qsTr("FFmpeg")
                        font.weight: Font.Medium
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                
                Label {
                    text: systemChecker && systemChecker.ffmpegFound ?
                          qsTr("Found") : qsTr("Not Found")
                    font.pixelSize: 12
                    color: Material.hintTextColor
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }
        
        // Action buttons
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 20
            spacing: 16
            
            Button {
                text: qsTr("Start Check")
                enabled: systemChecker ? !systemChecker.isChecking : false
                onClicked: {
                    if (systemChecker) {
                        systemChecker.startSystemCheck()
                    }
                }
                
                Material.background: Material.primary
            }
            
            Button {
                text: qsTr("Recheck Steam")
                enabled: systemChecker ? !systemChecker.isChecking : false
                visible: systemChecker ? !systemChecker.steamFound : true
                onClicked: {
                    if (systemChecker) {
                        systemChecker.recheckSteam()
                    }
                }
            }
            
            Button {
                text: qsTr("Recheck FFmpeg")
                enabled: systemChecker ? !systemChecker.isChecking : false
                visible: systemChecker ? !systemChecker.ffmpegFound : true
                onClicked: {
                    if (systemChecker) {
                        systemChecker.recheckFfmpeg()
                    }
                }
            }
            
            Button {
                text: qsTr("Continue")
                enabled: systemChecker ? (systemChecker.steamFound && systemChecker.ffmpegFound && !systemChecker.isChecking) : false
                highlighted: true
                onClicked: {
                    proceedTimer.stop()
                    // Set up the recording manager with Steam path
                    if (systemChecker && recordingManager) {
                        recordingManager.steamPath = systemChecker.steamPath
                        recordingManager.startScan()
                    }
                    root.systemCheckPassed()
                }
                
                Material.background: Material.accent
            }

            Button {
                text: qsTr("About")
                onClicked: {
                    aboutDialog.open()
                }

                Material.background: Material.primary
            }

            Button {
                text: qsTr("Quit")
                visible: systemChecker ? systemChecker.isSteamGameMode : false
                onClicked: {
                    Qt.quit()
                }

                Material.background: Material.primary
            }
        }
        
        // Help text
        Label {
            Layout.fillWidth: true
            Layout.topMargin: 20
            text: qsTr("This application requires Steam and FFmpeg to be installed on your system.\n\n" +
                      "Steam: Used to locate and read game recording files\n" +
                      "FFmpeg: Used to convert recordings to MP4 format\n\n" +
                      "If FFmpeg is not found, please download it from https://ffmpeg.org/download.html " +
                      "and ensure it's added to your system PATH.")
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            color: Material.hintTextColor
            horizontalAlignment: Text.AlignJustify
        }
    }
    
    // Auto-start system check when page loads or becomes visible
    Component.onCompleted: {
        if (systemChecker) {
            systemChecker.startSystemCheck()
        }
    }
    
    // Handle page visibility changes
    onVisibleChanged: {
        if (visible && systemChecker) {
            // If checks were completed but user returned to this page,
            // refresh the check status
            if (!systemChecker.isChecking && (systemChecker.steamFound || systemChecker.ffmpegFound)) {
                // Don't auto-start, just ensure UI reflects current state
                console.log("SystemCheckPage became visible - current state: Steam=" + systemChecker.steamFound + " FFmpeg=" + systemChecker.ffmpegFound)
            }
        }
    }
    
    // Handle system check completion
    Connections {
        target: systemChecker
        
        function onSystemCheckCompleted() {
            // Auto-proceed if all checks pass
            if (systemChecker.allChecksPass) {
                checkStatusLabel.text = qsTr("All checks passed")
                // Small delay to let user see the success
                proceedTimer.start()
            } else {
                checkStatusLabel.text = qsTr("System check not passed")
            }
        }
    }
    
    Timer {
        id: proceedTimer
        interval: 1000
        repeat: false
        onTriggered: {
            if (systemChecker && systemChecker.allChecksPass) {
                if (recordingManager) {
                    recordingManager.steamPath = systemChecker.steamPath
                    recordingManager.startScan()
                }
                root.systemCheckPassed()
            }
        }
    }
}
