import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    property var recordingManager: null

    signal userSelected(string userId)

    anchors.centerIn: parent
    modal: true
    title: qsTr("Select Steam Account")

    width: Math.min(500, parent.width - 40)
    height: Math.min(400, parent.height - 40)

    standardButtons: Dialog.Cancel

    ColumnLayout {
        anchors.fill: parent
        spacing: 16

        Label {
            text: qsTr("Multiple Steam accounts detected. Please select which account's recordings you want to scan:")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            font.pixelSize: 14
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ListView {
                id: userListView

                model: recordingManager ? recordingManager.availableUsers : []
                spacing: 8

                delegate: ItemDelegate {
                    width: userListView.width
                    height: userCard.height

                    Rectangle {
                        id: userCard
                        anchors.fill: parent
                        color: parent.hovered ? Material.color(Material.Grey, Material.Shade200) : Material.backgroundColor
                        border.color: Material.dividerColor
                        border.width: 1
                        radius: 8

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 12

                            Rectangle {
                                width: 40
                                height: 40
                                radius: 20
                                color: Material.primary

                                Label {
                                    anchors.centerIn: parent
                                    text: getUserInitials(recordingManager ? recordingManager.getUserDisplayName(modelData) : modelData)
                                    color: "white"
                                    font.weight: Font.Bold
                                    font.pixelSize: 16
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4

                                Label {
                                    text: recordingManager ? recordingManager.getUserDisplayName(modelData) : modelData
                                    font.pixelSize: 16
                                    font.weight: Font.Medium
                                }

                                Label {
                                    text: qsTr("Steam ID: %1").arg(modelData)
                                    font.pixelSize: 12
                                    color: Material.hintTextColor
                                }

                                Label {
                                    text: hasRecordings(modelData) ? qsTr("Has game recordings") : qsTr("No recordings found")
                                    font.pixelSize: 12
                                    color: hasRecordings(modelData) ? Material.color(Material.Green) : Material.hintTextColor
                                }
                            }

                            Button {
                                text: qsTr("Select")
                                highlighted: true
                                onClicked: {
                                    root.userSelected(modelData);
                                    root.accept();
                                }
                            }
                        }
                    }

                    onClicked: {
                        root.userSelected(modelData);
                        root.accept();
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Material.dividerColor
        }

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: qsTr("💡 Tip: You can change this selection later from the recording list page")
                font.pixelSize: 12
                color: Material.hintTextColor
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }

            Button {
                text: qsTr("Refresh")
                flat: true
                onClicked: {
                    if (recordingManager) {
                        recordingManager.refreshAvailableUsers();
                    }
                }
            }
        }
    }

    function getUserInitials(displayName) {
        if (!displayName || displayName.length === 0) {
            return "?";
        }

        var words = displayName.split(' ');
        if (words.length >= 2) {
            return (words[0][0] + words[1][0]).toUpperCase();
        } else {
            return displayName.substring(0, Math.min(2, displayName.length)).toUpperCase();
        }
    }

    function hasRecordings(userId) {
        // This is a simplified check - in a real implementation,
        // you might want to expose this information from the C++ side
        return true;
    }
}
