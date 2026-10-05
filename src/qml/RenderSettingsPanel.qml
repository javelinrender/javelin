import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property var settings
    spacing: 8

    function editorSource(type) {
        switch (type) {
        case "enum":        return "EnumParameterEditor.qml"
        case "integer":     return "IntegerParameterEditor.qml"
        case "bool":        return "BoolParameterEditor.qml"
        case "file_path":   return "FilePathParameterEditor.qml"
        default:            return ""
        }
    }

    Repeater {
        model: panel.settings

        delegate: RowLayout {
            id: row
            required property var parameter
            readonly property string editorUrl: panel.editorSource(parameter.type)
            visible: parameter.visible
            enabled: visible
            Layout.fillWidth: true
            spacing: 8

            Label {
                Layout.preferredWidth: 160
                text: row.parameter.label
                horizontalAlignment: Text.AlignRight
            }

            Loader {
                Layout.fillWidth: row.parameter.type !== "bool"
                Layout.alignment: Qt.AlignLeft
                visible: row.editorUrl !== ""
                Component.onCompleted: {
                    if (row.editorUrl !== "")
                        setSource(
                            row.editorUrl,
                            {
                                parameter: Qt.binding(function() { return row.parameter }),
                                settings: panel.settings
                            })
                }
            }

        }
    }

    Item {
        Layout.fillHeight: true
    }
}
