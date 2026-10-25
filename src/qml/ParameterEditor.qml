import QtQuick

Loader {
    id: editor
    required property var parameter
    required property var settings

    readonly property real firstRowHeight: {
        if (!item)
            return 0
        if (parameter.type === "group")
            // In a group, we want the first row height so that the label is
            // vertically aligned with it.
            return item.firstRowHeight
        return item.implicitHeight
    }

    readonly property string editorUrl: {
        switch (parameter.type) {
        case "enum":      return "EnumParameterEditor.qml"
        case "integer":   return "IntegerParameterEditor.qml"
        case "bool":      return "BoolParameterEditor.qml"
        case "file_path": return "FilePathParameterEditor.qml"
        case "group":     return "ParameterGroupEditor.qml"
        default:          return ""
        }
    }

    visible: parameter.visible && editorUrl !== ""
    enabled: visible
    Component.onCompleted: {
        if (editorUrl === "")
            return

        let properties = {
            parameter: Qt.binding(function() { return editor.parameter }),
            settings: Qt.binding(function() { return editor.settings })
        }

        setSource(editorUrl, properties)
    }
}
