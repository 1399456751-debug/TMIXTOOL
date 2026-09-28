import QtQuick
import QtQuick.Layouts

// Stereo delay pairings. Both sides are shown because the point of every
// preset except the first is that they differ.
ColumnLayout {
    id: table

    property var rows: []

    spacing: 3

    Repeater {
        model: table.rows

        delegate: RowLayout {
            required property var modelData

            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.preferredWidth: 110
                text: modelData.name
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: 12
            }

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                text: Math.round (modelData.leftMs) + " / "
                      + Math.round (modelData.rightMs) + " ms"
                color: Theme.accent
                font.family: Theme.fontFamily
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }
        }
    }
}
