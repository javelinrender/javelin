import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: row
    required property var parameter
    required property var settings
    property bool grouped: false

    readonly property real firstRowHeight: Math.max(
        label.visible ? label.implicitHeight : 0, editor.firstRowHeight)

    visible: parameter.visible
    enabled: visible
    Layout.fillWidth: true
    spacing: 8

    Label {
        id: label
        visible: row.parameter.type !== "bool"
        Layout.preferredWidth: row.grouped ? 130 : 160
        Layout.preferredHeight: row.firstRowHeight
        Layout.alignment: Qt.AlignTop
        text: row.parameter.label
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
    }

    ParameterEditor {
        id: editor
        parameter: row.parameter
        settings: row.settings

        Layout.fillWidth: row.parameter.type !== "bool"
        Layout.alignment: Qt.AlignLeft | Qt.AlignTop
    }
}
