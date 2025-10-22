import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import net.blumia.pineapple.streamrecordingexporter

Page {
    id: root
    
    property RecordingClip clip: null
    property int segmentIndex: 0
    
    signal backRequested()
    signal exportCompleted()
    
    title: qsTr("Preview & Export")
    
    // Computed property for media controls enabled state
    readonly property bool mediaControlsEnabled: {
        return mediaPlayer.mediaStatus === MediaPlayer.LoadedMedia || 
               mediaPlayer.playbackState === MediaPlayer.PlayingState ||
               mediaPlayer.playbackState === MediaPlayer.PausedState
    }
    
    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            
            ToolButton {
                icon.name: "go-previous"
                text: qsTr("Back")
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
                    text: root.clip && root.clip.segmentCount > 1 ?
                          qsTr("Segment %1 of %2 • %3").arg(root.segmentIndex + 1).arg(root.clip.segmentCount).arg(root.clip.formattedDate) :
                          root.clip ? root.clip.formattedDate : ""
                    font.pixelSize: 12
                }
            }
        }
    }
    
    ScrollView {
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        
        ColumnLayout {
            width: parent.width
            spacing: 16
        
            // Video preview section
            GroupBox {
                title: qsTr("Preview")
                Layout.fillWidth: true
                Layout.margins: 16
            
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    // Video player
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(width * 9/16, 400)
                        Layout.minimumHeight: 250

                        color: "black"
                        border.color: Material.dividerColor
                        border.width: 1
                        radius: 4

                        MediaPlayer {
                            id: mediaPlayer

                            source: {
                                if (root.clip && root.segmentIndex >= 0) {
                                    return root.clip.getSegmentMpdUrl(root.segmentIndex)
                                }
                                return ""
                            }

                            videoOutput: videoOutput
                            audioOutput: AudioOutput {}

                            onErrorOccurred: function(error, errorString) {
                                console.log("Media player error:", error, errorString)
                                errorLabel.text = qsTr("Preview Error: %1").arg(errorString)
                                errorLabel.visible = true
                            }

                            onMediaStatusChanged: {
                                console.log("Media status changed:", mediaStatus)
                                if (mediaStatus === MediaPlayer.LoadedMedia) {
                                    errorLabel.visible = false
                                }
                            }

                            onPlaybackStateChanged: {
                                console.log("Playback state changed:", playbackState)
                            }

                            onBufferProgressChanged: {
                                console.log("Buffer progress:", bufferProgress)
                            }
                        }

                        VideoOutput {
                            id: videoOutput
                            anchors.fill: parent
                            anchors.margins: 1

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (mediaPlayer.playbackState === MediaPlayer.PlayingState) {
                                        mediaPlayer.pause()
                                    } else {
                                        mediaPlayer.play()
                                    }
                                }
                            }
                        }

                        // Loading/Buffering indicator
                        BusyIndicator {
                            anchors.centerIn: parent
                            running: mediaPlayer.mediaStatus === MediaPlayer.LoadingMedia
                            visible: running
                        }

                        // Error message
                        Label {
                            id: errorLabel
                            anchors.centerIn: parent
                            text: qsTr("Unable to load video preview")
                            color: Material.color(Material.Red)
                            visible: false
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                        }

                        // Fallback message when no media
                        Label {
                            anchors.centerIn: parent
                            text: qsTr("No video source available")
                            color: Material.hintTextColor
                            visible: !root.clip || mediaPlayer.source === ""
                            font.pixelSize: 16
                        }

                        // Play/pause overlay
                        Rectangle {
                            anchors.centerIn: parent
                            width: 80
                            height: 80
                            radius: 40
                            color: "#80000000"
                            visible: mediaPlayer.mediaStatus === MediaPlayer.LoadedMedia &&
                                    mediaPlayer.playbackState !== MediaPlayer.PlayingState

                            Label {
                                anchors.centerIn: parent
                                text: "▶"
                                font.pixelSize: 32
                                color: "white"
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: mediaPlayer.play()
                            }
                        }
                    }

                    // Media controls
                    RowLayout {
                        Layout.fillWidth: true

                        ToolButton {
                            icon.name: mediaPlayer.playbackState === MediaPlayer.PlayingState ? "media-playback-pause" : "media-playback-start"
                            text: mediaPlayer.playbackState === MediaPlayer.PlayingState ? qsTr("Pause") : qsTr("Play")
                            enabled: root.mediaControlsEnabled
                            onClicked: {
                                if (mediaPlayer.playbackState === MediaPlayer.PlayingState) {
                                    mediaPlayer.pause()
                                } else {
                                    mediaPlayer.play()
                                }
                            }
                        }

                        ToolButton {
                            icon.name: "media-playback-stop"
                            text: qsTr("Stop")
                            enabled: root.mediaControlsEnabled
                            onClicked: mediaPlayer.stop()
                        }

                        Slider {
                            id: positionSlider
                            Layout.fillWidth: true
                            enabled: root.mediaControlsEnabled && mediaPlayer.seekable
                            from: 0
                            to: mediaPlayer.duration
                            value: pressed ? value : mediaPlayer.position

                            onMoved: {
                                if (mediaPlayer.seekable) {
                                    mediaPlayer.setPosition(value)
                                }
                            }

                            ToolTip {
                                parent: positionSlider.handle
                                visible: positionSlider.pressed
                                text: formatTime(positionSlider.value)
                            }
                        }

                        Label {
                            text: formatTime(mediaPlayer.position) + " / " + formatTime(mediaPlayer.duration)
                            font.pixelSize: 12
                            color: Material.hintTextColor
                            Layout.minimumWidth: 100
                        }

                        ToolButton {
                            icon.name: mediaPlayer.audioOutput.muted ? "audio-volume-muted" : "audio-volume-high"
                            text: mediaPlayer.audioOutput.muted ? qsTr("Unmute") : qsTr("Mute")
                            onClicked: mediaPlayer.audioOutput.muted = !mediaPlayer.audioOutput.muted
                        }

                        Slider {
                            Layout.preferredWidth: 100
                            from: 0
                            to: 1
                            value: mediaPlayer.audioOutput.volume
                            onMoved: mediaPlayer.audioOutput.volume = value
                        }
                    }
                }
            }
        
            // Export section
            GroupBox {
                title: qsTr("Export")
                Layout.fillWidth: true
                Layout.preferredHeight: exportColumn.implicitHeight + 32
                Layout.margins: 16

                ColumnLayout {
                    id: exportColumn
                    anchors.fill: parent
                    spacing: 12

                    // Export status
                    RowLayout {
                        Layout.fillWidth: true
                        visible: videoExporter && videoExporter.isExporting

                        BusyIndicator {
                            running: true
                            implicitWidth: 24
                            implicitHeight: 24
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Label {
                                text: videoExporter ? videoExporter.currentOperation : ""
                                font.weight: Font.Medium
                            }

                            ProgressBar {
                                Layout.fillWidth: true
                                value: videoExporter ? videoExporter.progress / 100.0 : 0

                                Label {
                                    anchors.centerIn: parent
                                    text: videoExporter ? videoExporter.progress + "%" : ""
                                    font.pixelSize: 10
                                    color: Material.foreground
                                }
                            }
                        }

                        Button {
                            text: qsTr("Cancel")
                            onClicked: {
                                if (videoExporter) {
                                    videoExporter.cancelExport()
                                }
                            }
                        }
                    }

                    // Export controls
                    RowLayout {
                        Layout.fillWidth: true
                        visible: !videoExporter || !videoExporter.isExporting

                        Label {
                            text: qsTr("Output:")
                            Layout.alignment: Qt.AlignTop
                            Layout.topMargin: 8
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            TextField {
                                id: outputPathField
                                Layout.fillWidth: true
                                text: videoExporter && root.clip ?
                                      videoExporter.generateOutputFilename(root.clip, root.segmentIndex) :
                                      "recording.mp4"
                                placeholderText: qsTr("Output filename")


                            }

                            Label {
                                text: videoExporter ?
                                      qsTr("Will be saved to: %1").arg(videoExporter.defaultExportPath) :
                                      ""
                                font.pixelSize: 11
                                color: Material.hintTextColor
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }

                    // Export buttons
                    RowLayout {
                        Layout.fillWidth: true
                        visible: !videoExporter || !videoExporter.isExporting

                        Item { Layout.fillWidth: true }

                        Button {
                            text: qsTr("Browse...")
                            onClicked: {
                                // TODO: Open file dialog
                                console.log("File dialog not yet implemented")
                            }
                            enabled: false

                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("File dialog not yet implemented")
                        }

                        Button {
                            text: qsTr("Export Segment")
                            highlighted: true
                            enabled: root.clip && root.segmentIndex >= 0 &&
                                    (!videoExporter || !videoExporter.isExporting)

                            onClicked: {
                                if (videoExporter && root.clip) {
                                    var outputPath = ""
                                    if (outputPathField.text.trim() !== "") {
                                        outputPath = videoExporter.defaultExportPath + "/" + outputPathField.text.trim()
                                    }
                                    videoExporter.exportClipSegment(root.clip, root.segmentIndex, outputPath)
                                }
                            }
                        }
                    }

                    // Export log
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 150
                        visible: videoExporter && videoExporter.exportLog !== ""
                        clip: true

                        TextArea {
                            text: videoExporter ? videoExporter.exportLog : ""
                            readOnly: true
                            selectByMouse: true
                            wrapMode: Text.WordWrap
                            font.family: "Consolas, Monaco, monospace"
                            font.pixelSize: 10

                            background: Rectangle {
                                implicitHeight: Material.textFieldHeight
                                color: Material.backgroundDimColor
                                border.color: Material.dividerColor
                                border.width: 1
                                radius: 4
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Handle export completion
    Connections {
        target: videoExporter
        
        function onExportCompleted(outputPath) {
            exportCompletedDialog.outputPath = outputPath
            exportCompletedDialog.open()
        }
        
        function onExportFailed(error) {
            exportErrorDialog.errorMessage = error
            exportErrorDialog.open()
        }
    }
    
    // Export completed dialog
    Dialog {
        id: exportCompletedDialog
        
        property string outputPath: ""
        
        anchors.centerIn: parent
        modal: true
        title: qsTr("Export Completed")
        
        ColumnLayout {
            spacing: 16
            anchors.fill: parent
            
            Label {
                text: qsTr("Video has been exported successfully!")
                font.pixelSize: 14
            }
            
            Label {
                text: qsTr("Saved to: %1").arg(exportCompletedDialog.outputPath)
                font.pixelSize: 12
                color: Material.hintTextColor
                wrapMode: Text.WordWrap
                Layout.maximumWidth: 400
            }
        }
        
        standardButtons: Dialog.Open | Dialog.Close
        
        onAccepted: {
            root.exportCompleted()
            if (videoExporter) {
                videoExporter.openExportLocation(exportCompletedDialog.outputPath)
            }
        }

        onRejected: {
            root.exportCompleted()
        }
    }
    
    // Export error dialog
    Dialog {
        id: exportErrorDialog
        
        property string errorMessage: ""
        
        anchors.centerIn: parent
        modal: true
        title: qsTr("Export Failed")
        
        ColumnLayout {
            spacing: 16
            
            Label {
                text: qsTr("An error occurred during export:")
                font.pixelSize: 14
            }
            
            ScrollView {
                Layout.preferredWidth: 400
                Layout.preferredHeight: 100
                clip: true
                
                TextArea {
                    text: exportErrorDialog.errorMessage
                    readOnly: true
                    selectByMouse: true
                    wrapMode: Text.WordWrap
                    font.family: "Consolas, Monaco, monospace"
                    font.pixelSize: 12
                    
                    background: Rectangle {
                        color: Material.backgroundDimColor
                        border.color: Material.dividerColor
                        border.width: 1
                        radius: 4
                    }
                }
            }
        }
        
        standardButtons: Dialog.Ok
    }
    
    // Utility functions
    function formatTime(milliseconds) {
        if (isNaN(milliseconds) || milliseconds < 0) {
            return "00:00"
        }
        
        var seconds = Math.floor(milliseconds / 1000)
        var minutes = Math.floor(seconds / 60)
        var hours = Math.floor(minutes / 60)
        
        seconds = seconds % 60
        minutes = minutes % 60
        
        var timeStr = ""
        if (hours > 0) {
            timeStr = hours.toString().padStart(2, '0') + ":"
        }
        timeStr += minutes.toString().padStart(2, '0') + ":"
        timeStr += seconds.toString().padStart(2, '0')
        
        return timeStr
    }
    
    // Cleanup when page is destroyed
    Component.onDestruction: {
        if (mediaPlayer) {
            mediaPlayer.stop()
        }
    }
}
