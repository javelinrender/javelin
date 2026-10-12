import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property var settings
    spacing: 8

    Repeater {
        model: panel.settings

        delegate: ParameterRow {
            settings: panel.settings
        }
    }

    Item {
        Layout.fillHeight: true
    }
}
