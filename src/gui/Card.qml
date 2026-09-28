import QtQuick
import QtQuick.Layouts

// A titled white panel. Everything on the right-hand side of the window is one
// of these, so the whole page reads as a set of equal-weight sections.
Rectangle {
    id: card

    property string title: ""
    property string subtitle: ""
    default property alias content: body.data

    color: Theme.panel
    radius: Theme.radius
    border.width: 1
    border.color: Theme.border

    implicitHeight: layout.implicitHeight + Theme.pad * 2

    ColumnLayout {
        id: layout

        anchors.fill: parent
        anchors.margins: Theme.pad
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: card.title.length > 0

            Text {
                text: card.title
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                text: card.subtitle
                color: Theme.textFaint
                font.family: Theme.fontFamily
                font.pixelSize: 11
                elide: Text.ElideRight
            }
        }

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            spacing: 4
        }
    }
}
