import QtQuick
import QtQuick.Layouts

// A list of time values: label on the left, milliseconds aligned right, and a
// short note about when to use it filling the rest.
//
// The note is the reason this is a table and not a wall of numbers - knowing
// that 80 ms is the usual drum-bus starting point is what makes the value
// usable.
ColumnLayout {
    id: table

    property var rows: []
    property bool showDetail: true

    spacing: 5

    Repeater {
        model: table.rows

        delegate: RowLayout {
            required property var modelData

            Layout.fillWidth: true
            spacing: 8

            Text {
                Layout.preferredWidth: 62
                text: modelData.label
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: 12
            }

            Text {
                Layout.preferredWidth: 74
                horizontalAlignment: Text.AlignRight
                text: modelData.ms < 10
                      ? modelData.ms.toFixed(2) + " ms"
                      : Math.round (modelData.ms) + " ms"
                color: Theme.accent
                font.family: Theme.fontFamily
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                visible: table.showDetail
                // The note comes from the analysis core in English; Lang maps
                // it to the interface language.
                text: Lang.use (modelData.detail)
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: 11
                elide: Text.ElideRight
            }
        }
    }
}
