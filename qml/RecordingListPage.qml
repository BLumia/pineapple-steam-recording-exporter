import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import net.blumia.pineapple.streamrecordingexporter
import "components"

Page {
    id: root

    signal clipSelected(var clip)
    signal backRequested
    signal exportedVideosRequested

    title: qsTr("Recording List")

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.margins: 8

            ToolButton {
                text: "←"
                font.pixelSize: 18
                onClicked: root.backRequested()
            }

            Label {
                text: qsTr("Steam Game Recordings")
                font.pixelSize: 18
                font.weight: Font.Medium
                Layout.fillWidth: true
            }

            ToolButton {
                text: qsTr("Exported Videos")
                onClicked: root.exportedVideosRequested()

                ToolTip.visible: hovered
                ToolTip.text: qsTr("View and manage exported videos")
            }

            ToolButton {
                text: qsTr("Switch User")
                enabled: recordingManager ? (!recordingManager.isScanning && recordingManager.hasMultipleUsers) : false
                visible: recordingManager ? recordingManager.hasMultipleUsers : false
                onClicked: userSelectionDialog.open()

                ToolTip.visible: hovered
                ToolTip.text: qsTr("Select different Steam account")
            }

            ToolButton {
                icon.name: "view-refresh"
                text: qsTr("Refresh")
                enabled: recordingManager ? !recordingManager.isScanning : false
                onClicked: {
                    if (recordingManager) {
                        recordingManager.refreshClips();
                    }
                }

                BusyIndicator {
                    anchors.centerIn: parent
                    running: recordingManager ? recordingManager.isScanning : false
                    visible: running
                    implicitWidth: 16
                    implicitHeight: 16
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // Status and stats
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: statsColumn.implicitHeight + 32
            color: Material.backgroundColor
            border.color: Material.dividerColor
            border.width: 1
            radius: 8

            ColumnLayout {
                id: statsColumn
                anchors.fill: parent
                anchors.margins: 16

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: qsTr("Found Recordings")
                        font.pixelSize: 16
                        font.weight: Font.Medium
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Label {
                        text: recordingManager ? recordingManager.clipCount.toString() : "0"
                        font.pixelSize: 20
                        font.weight: Font.Bold
                        color: Material.primary
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Label {
                        text: recordingManager && recordingManager.selectedUserId ? qsTr("Current User: %1").arg(recordingManager.getUserDisplayName(recordingManager.selectedUserId)) : qsTr("No user selected")
                        font.pixelSize: 12
                        color: Material.hintTextColor
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }

                    Label {
                        text: recordingManager && recordingManager.gameRecordingsPath ? qsTr("Scanning: %1").arg(recordingManager.gameRecordingsPath) : qsTr("No recordings path configured")
                        font.pixelSize: 12
                        color: Material.hintTextColor
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                }
            }
        }

        // Main content area
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true

            currentIndex: {
                if (!recordingManager)
                    return 2;
                if (recordingManager.isScanning)
                    return 0;
                if (recordingManager.hasClips)
                    return 1;
                return 2;
            }

            // Loading state
            Item {
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 16

                    BusyIndicator {
                        running: true
                        Layout.alignment: Qt.AlignHCenter
                        implicitWidth: 64
                        implicitHeight: 64
                    }

                    Label {
                        text: qsTr("Scanning for recording clips...")
                        font.pixelSize: 16
                        Layout.alignment: Qt.AlignHCenter
                    }

                    ScrollView {
                        Layout.preferredWidth: 600
                        Layout.preferredHeight: 200
                        clip: true

                        TextArea {
                            text: recordingManager ? recordingManager.scanResults.join('\n') : ""
                            readOnly: true
                            wrapMode: Text.WordWrap
                            font.family: "Consolas, Monaco, monospace"
                            font.pixelSize: 11
                            color: Material.hintTextColor

                            background: Rectangle {
                                color: Material.backgroundDimColor
                                radius: 4
                                border.color: Material.dividerColor
                                border.width: 1
                            }
                        }
                    }
                }
            }

            // Clips list
            ScrollView {
                clip: true

                ListView {
                    id: clipsListView

                    model: recordingManager
                    spacing: 12

                    delegate: RecordingCard {
                        width: clipsListView.width
                        recordingClip: model.clip

                        onCardClicked: {
                            console.log("RecordingListPage - Card clicked, model.clip:", model.clip);
                            console.log("RecordingListPage - About to emit clipSelected signal");
                            root.clipSelected(model.clip);
                        }
                    }

                    // Pull to refresh
                    property bool refreshing: false

                    onContentYChanged: {
                        if (contentY < -80 && !refreshing && recordingManager && !recordingManager.isScanning) {
                            refreshing = true;
                            recordingManager.refreshClips();
                            refreshTimer.start();
                        }
                    }

                    Timer {
                        id: refreshTimer
                        interval: 1000
                        onTriggered: clipsListView.refreshing = false
                    }

                    // Add some padding at the bottom
                    footer: Item {
                        height: 20
                    }
                }
            }

            // Empty state
            Item {
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 16

                    Label {
                        text: "🎮"
                        font.pixelSize: 64
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Label {
                        text: qsTr("No recording clips found")
                        font.pixelSize: 18
                        font.weight: Font.Medium
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Label {
                        text: qsTr("Make sure you have game recordings in Steam.\nRecordings are saved automatically during gameplay\nwhen Steam's Game Recording feature is enabled.")
                        font.pixelSize: 14
                        color: Material.hintTextColor
                        horizontalAlignment: Text.AlignHCenter
                        Layout.alignment: Qt.AlignHCenter
                        Layout.maximumWidth: 400
                        wrapMode: Text.WordWrap
                    }

                    Button {
                        text: qsTr("Refresh")
                        onClicked: {
                            if (recordingManager) {
                                recordingManager.refreshClips();
                            }
                        }
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 16
                    }
                }
            }
        }
    }

    // Steam user selection dialog
    SteamUserSelectionDialog {
        id: userSelectionDialog
        recordingManager: root.recordingManager

        onUserSelected: function (userId) {
            if (recordingManager) {
                recordingManager.selectedUserId = userId;
                recordingManager.refreshClips();
            }
        }
    }

    // Auto-start scanning when page loads
    Component.onCompleted: {
        if (recordingManager) {
            // Check if we need to show user selection dialog
            if (recordingManager.hasMultipleUsers && !recordingManager.selectedUserId) {
                userSelectionDialog.open();
            } else if (!recordingManager.hasClips) {
                recordingManager.startScan();
            }
        }
    }

    // Handle recording manager changes
    Connections {
        target: recordingManager

        function onHasMultipleUsersChanged() {
            if (recordingManager && recordingManager.hasMultipleUsers && !recordingManager.selectedUserId) {
                userSelectionDialog.open();
            }
        }
    }
}
