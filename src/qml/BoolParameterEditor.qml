import QtQuick.Controls

CheckBox {
    required property var parameter
    required property var settings

    checked: parameter.value
    onToggled: settings.setValue(parameter.name, checked)
}
