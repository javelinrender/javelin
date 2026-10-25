import QtQuick
import QtQuick.Controls

ComboBox {
    required property var parameter
    required property var settings

    model: parameter.choices
    textRole: "label"
    valueRole: "key"
    currentIndex: {
        for (let index = 0; index < parameter.choices.length; ++index) {
            if (parameter.choices[index].key === parameter.value)
                return index
        }
        return -1
    }
    onActivated: settings.setValue(parameter.name, currentValue)
}
