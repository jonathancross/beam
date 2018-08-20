import QtQuick 2.3
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2
import "."

CustomButton {
    palette.button: Style.white
    palette.buttonText: Style.white
    textOpacity: 0.1
    width: text.width + 60
    height: text.height + 24

    SFText {
        id: text
        anchors.verticalCenter: parent.verticalCenter
        anchors.horizontalCenter: parent.horizontalCenter

        font.pixelSize: 12
        font.weight: Font.Bold

        color: Style.white

        text: parent.text
        visible: false
    }
}