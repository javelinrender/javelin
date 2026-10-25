import QtQuick
import QtQuick.Controls

SpinBox {
    id: editor
    required property var parameter
    required property var settings

    // SpinBox is integer-only, so values are stored scaled.
    readonly property real scale: 1000
    readonly property int decimals: 3

    editable: true
    from: Math.round(parameter.minimum * scale)
    to: Math.round(parameter.maximum * scale)
    stepSize: Math.max(1, Math.round(parameter.step * scale))
    value: Math.round(parameter.value * scale)
    validator: DoubleValidator {
        bottom: editor.parameter.minimum
        top: editor.parameter.maximum
    }
    textFromValue: function(value, locale) {
        return Number(value / editor.scale).toLocaleString(
            locale, "f", editor.decimals)
    }
    valueFromText: function(text, locale) {
        return Math.round(Number.fromLocaleString(locale, text) * editor.scale)
    }
    onValueModified: settings.setValue(parameter.name, value / scale)
}
