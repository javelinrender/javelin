import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    minimumWidth: 360
    minimumHeight: Math.ceil(contentLayout.implicitHeight + 24)
    title: "Javelin"
    visible: true

    onMinimumHeightChanged: {
        if (height < minimumHeight)
            height = minimumHeight
    }
    Component.onCompleted: {
        if (height < minimumHeight)
            height = minimumHeight
    }

    ColumnLayout {
        id: contentLayout
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RenderSettingsPanel {
            id: settingsPanel
            Layout.fillWidth: true
            Layout.fillHeight: true
            settings: renderSettings
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: "Restart"
                onClicked: app.startRender()
            }

            Button {
                text: app.paused ? "Resume" : "Pause"
                enabled: app.running || app.paused
                onClicked: {
                    if (app.paused)
                        app.resumeRender()
                    else
                        app.pauseRender()
                }
            }
        }
    }
}
