import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

RowLayout {
    id: editor
    required property var parameter
    required property var settings
    spacing: 8

    TextField {
        Layout.fillWidth: true
        text: editor.parameter.value
        selectByMouse: true
        onEditingFinished: {
            if (!editor.settings.setValue(editor.parameter.name, text))
                text = editor.parameter.value
        }
    }

    Button {
        text: "Browse..."
        onClicked: fileDialog.open()
    }

    FileDialog {
        id: fileDialog
        title: "Select " + editor.parameter.label
        fileMode: FileDialog.OpenFile
        nameFilters: editor.parameter.name_filters
        onAccepted: editor.settings.setValue(editor.parameter.name, selectedFile)
    }
}
