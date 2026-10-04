import QtQuick
import QtQuick.Controls

ApplicationWindow {
    width: 512
    height: 384
    minimumWidth: 256
    minimumHeight: 192
    visible: true
    title: viewport.samplesPerPixel + " spp"
    color: "black"

    Image {
        anchors.fill: parent
        source: viewport.imageSource
        cache: false
        fillMode: Image.PreserveAspectFit
    }
}
