import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: group
    required property var parameter
    required property var settings
    readonly property real firstRowHeight: {
        for (let index = 0; index < children.length; ++index) {
            let child = children[index]
            if (child.visible && child.firstRowHeight !== undefined)
                return child.firstRowHeight
        }
        return 0
    }

    visible: parameter.visible
    enabled: visible
    spacing: 8

    Repeater {
        model: group.parameter.children.length

        delegate: ParameterRow {
            required property int index
            parameter: group.parameter.children[index]
            settings: group.settings
            grouped: true
        }
    }
}
