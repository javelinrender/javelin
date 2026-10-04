import QtQuick.Controls

SpinBox {
    required property var parameter
    required property var settings

    editable: true
    from: parameter.minimum
    to: parameter.maximum
    stepSize: parameter.step
    value: parameter.value
    onValueModified: settings.setValue(parameter.name, value)
}
