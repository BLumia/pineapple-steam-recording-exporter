import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import QtQuick.Dialogs
import net.blumia.pineapple.streamrecordingexporter

Page {
    id: root
    
    property var video: null
    signal backRequested()
    signal videoDeleted()

    // Computed property for media controls enabled state
    readonly property bool mediaControlsEnabled: {
        return mediaPlayer.mediaStatus === MediaPlayer.LoadedMedia ||
               mediaPlayer.playbackState === MediaPlayer.PlayingState ||
               mediaPlayer.playbackState === MediaPlayer.PausedState
    }
    
    header: ToolBar {
        Material.elevation: 4
        
        RowLayout {
            anchors.fill: parent
            
            ToolButton {
                icon.name: "go-previous"
                text: qsTr("Back")
                onClicked: root.backRequested()
            }
            
            Label {
                text: video ? video.displayName : qsTr("Video Preview")
                font.pixelSize: 18
                font.bold: true
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            
            ToolButton {
                icon.name: "document-open"
                text: qsTr("Show in Explorer")
                enabled: video
                onClicked: {
                    if (video) {
                        video.openInExplorer()
                    }
                }
            }
            
            ToolButton {
                icon.name: "edit-delete"
                text: qsTr("Delete")
                enabled: video
                onClicked: deleteDialog.open()
            }
        }
    }
    
    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        
        ColumnLayout {
            width: parent.width
            spacing: 24
            
            // Video player section
            GroupBox {
                title: qsTr("Video Player")
                Layout.fillWidth: true
                Layout.margins: 16
                
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 16
                    
                    // Video output
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(width * 9/16, 400)
                        
                        color: "black"
                        border.color: Material.dividerColor
                        border.width: 1
                        radius: 8
                        
                        VideoOutput {
                            id: videoOutput
                            anchors.fill: parent
                            anchors.margins: 1
                            fillMode: VideoOutput.PreserveAspectFit
                            
                            // Placeholder when no video
                            Rectangle {
                                anchors.centerIn: parent
                                width: Math.min(parent.width * 0.3, 120)
                                height: width
                                color: Material.backgroundColor
                                radius: width / 2
                                visible: !mediaPlayer.hasVideo
                                
                                Label {
                                    anchors.centerIn: parent
                                    text: "📹"
                                    font.pixelSize: parent.width * 0.4
                                }
                            }
                        }
                        
                        MediaPlayer {
                            id: mediaPlayer
                            videoOutput: videoOutput
                            audioOutput: AudioOutput {}
                            source: video ? video.fileUrl : ""
                            
                            onErrorOccurred: function(error, errorString) {
                                console.error("Media player error:", error, errorString)
                                errorLabel.text = qsTr("Error loading video: %1").arg(errorString)
                                errorLabel.visible = true
                            }
                            
                            onMediaStatusChanged: {
                                if (mediaStatus === MediaPlayer.LoadedMedia) {
                                    errorLabel.visible = false
                                }
                            }
                        }
                        
                        // Error message
                        Label {
                            id: errorLabel
                            anchors.centerIn: parent
                            text: ""
                            color: Material.color(Material.Red)
                            visible: false
                            wrapMode: Text.WordWrap
                            width: parent.width * 0.8
                            horizontalAlignment: Text.AlignHCenter
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
                        
                        Label {
                            text: formatTime(mediaPlayer.position)
                            font.family: "monospace"
                            Layout.minimumWidth: 60
                        }
                        
                        Slider {
                            Layout.fillWidth: true
                            from: 0
                            to: mediaPlayer.duration
                            value: mediaPlayer.position
                            enabled: mediaPlayer.seekable
                            
                            onMoved: {
                                mediaPlayer.setPosition(value)
                            }
                        }
                        
                        Label {
                            text: formatTime(mediaPlayer.duration)
                            font.family: "monospace"
                            Layout.minimumWidth: 60
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
                    
                    // Quick actions
                    RowLayout {
                        Layout.fillWidth: true
                        
                        Button {
                            text: qsTr("Play with Default Player")
                            icon.name: "application-default-icon"
                            enabled: video
                            onClicked: {
                                if (video) {
                                    video.playWithDefaultPlayer()
                                }
                            }
                        }
                        
                        Item { Layout.fillWidth: true }
                        
                        Label {
                            text: mediaPlayer.mediaStatus === MediaPlayer.LoadingMedia ? qsTr("Loading...") : ""
                            visible: text !== ""
                        }
                        
                        BusyIndicator {
                            visible: mediaPlayer.mediaStatus === MediaPlayer.LoadingMedia
                            running: visible
                            implicitWidth: 24
                            implicitHeight: 24
                        }
                    }
                }
            }
            
            // Video information section
            GroupBox {
                title: qsTr("Video Information")
                Layout.fillWidth: true
                Layout.margins: 16
                
                GridLayout {
                    anchors.fill: parent
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 8
                    
                    Label {
                        text: qsTr("Display Name:")
                        font.bold: true
                    }
                    Label {
                        text: video ? video.displayName : ""
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    
                    Label {
                        text: qsTr("File Name:")
                        font.bold: true
                    }
                    Label {
                        text: video ? video.fileName : ""
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.family: "monospace"
                    }
                    
                    Label {
                        text: qsTr("File Size:")
                        font.bold: true
                    }
                    Label {
                        text: video ? video.fileSizeString : ""
                    }
                    
                    Label {
                        text: qsTr("Created:")
                        font.bold: true
                    }
                    Label {
                        text: video ? video.creationTimeString : ""
                    }
                    
                    Label {
                        text: qsTr("File Path:")
                        font.bold: true
                    }
                    Label {
                        text: video ? video.filePath : ""
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.family: "monospace"
                        font.pixelSize: 11
                    }
                    
                    // Video metadata (if available)
                    Label {
                        text: qsTr("Duration:")
                        font.bold: true
                        visible: mediaPlayer.duration > 0
                    }
                    Label {
                        text: formatTime(mediaPlayer.duration)
                        visible: mediaPlayer.duration > 0
                    }
                    
                    Label {
                        text: qsTr("Resolution:")
                        font.bold: true
                        visible: videoOutput.sourceRect.width > 0
                    }
                    Label {
                        text: videoOutput.sourceRect.width > 0 ? 
                              qsTr("%1 × %2").arg(videoOutput.sourceRect.width).arg(videoOutput.sourceRect.height) : ""
                        visible: videoOutput.sourceRect.width > 0
                    }
                }
            }
            
            // Spacer
            Item {
                Layout.fillHeight: true
            }
        }
    }
    
    // Delete confirmation dialog
    Dialog {
        id: deleteDialog
        
        anchors.centerIn: parent
        modal: true
        title: qsTr("Delete Video")
        
        Label {
            text: video ? qsTr("Are you sure you want to delete \"%1\"?\nThis action cannot be undone.").arg(video.displayName) : ""
            wrapMode: Text.WordWrap
            width: 300
        }
        
        standardButtons: Dialog.Yes | Dialog.No
        
        onAccepted: {
            if (video && video.deleteVideo()) {
                root.videoDeleted()
                root.backRequested()
            }
        }
    }
    
    // Utility functions
    function formatTime(milliseconds) {
        if (milliseconds <= 0) return "00:00"
        
        var totalSeconds = Math.floor(milliseconds / 1000)
        var hours = Math.floor(totalSeconds / 3600)
        var minutes = Math.floor((totalSeconds % 3600) / 60)
        var seconds = totalSeconds % 60
        
        if (hours > 0) {
            return "%1:%2:%3".arg(hours.toString().padStart(2, '0'))
                             .arg(minutes.toString().padStart(2, '0'))
                             .arg(seconds.toString().padStart(2, '0'))
        } else {
            return "%1:%2".arg(minutes.toString().padStart(2, '0'))
                          .arg(seconds.toString().padStart(2, '0'))
        }
    }
    
    // Handle video changes
    onVideoChanged: {
        if (mediaPlayer.playbackState === MediaPlayer.PlayingState) {
            mediaPlayer.stop()
        }
    }
    
    // Cleanup when leaving the page
    Component.onDestruction: {
        if (mediaPlayer.playbackState === MediaPlayer.PlayingState) {
            mediaPlayer.stop()
        }
    }
}
