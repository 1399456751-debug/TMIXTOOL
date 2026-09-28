import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: window

    width: 1180
    height: 800
    minimumWidth: 980
    minimumHeight: 640
    visible: true
    title: Lang.t("appTitle")
    color: Theme.bg

    AnalysisController { id: controller }

    // A file handed to the application on the command line opens immediately,
    // which is also what makes "Open with" work from the shell.
    Component.onCompleted: {
        const args = Qt.application.arguments;
        if (args.length > 1 && args[1].length > 0)
            controller.analysePath (args[1]);
    }

    FileDialog {
        id: fileDialog
        title: Lang.t("dropHint")
        nameFilters: ["Audio (*.wav *.mp3)", "All files (*)"]
        onAccepted: controller.analyse (selectedFile)
    }

    // Dropping anywhere in the window works, so there is no small target to
    // aim at.
    DropArea {
        anchors.fill: parent
        onDropped: function (drop) {
            if (drop.hasUrls && drop.urls.length > 0)
                controller.analyse (drop.urls[0]);
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- header ------------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 52
            color: Theme.panel

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.border
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 14
                spacing: 10

                Text {
                    text: Lang.t("appTitle")
                    color: Theme.text
                    font.family: Theme.fontFamily
                    font.pixelSize: 17
                    font.weight: Font.Bold
                }

                Text {
                    Layout.fillWidth: true
                    text: Lang.t("tagline")
                    color: Theme.textFaint
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                }

                Button {
                    text: Lang.code === "zh" ? "EN" : "中文"
                    flat: true
                    onClicked: Lang.toggle()
                }

                Button {
                    text: Lang.t("about")
                    flat: true
                    onClicked: aboutDialog.open()
                }
            }
        }

        // ---- body --------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Theme.pad + 2
            spacing: Theme.pad + 2

            // ================= left column ================================
            // Preferred width alone was not enough here: the nested layout
            // reports an implicit width from its children and the row gave it
            // the whole window. Clamping the maximum pins it to the intended
            // column width.
            ColumnLayout {
                Layout.preferredWidth: 290
                Layout.minimumWidth: 290
                Layout.maximumWidth: 290
                Layout.fillHeight: true
                spacing: Theme.gap

                // ---- tempo, the headline number --------------------------
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: tempoColumn.implicitHeight + Theme.pad * 2
                    color: hasTempo ? Theme.bpmBg : Theme.panelAlt
                    radius: Theme.radius
                    border.width: 1
                    border.color: hasTempo ? Theme.bpmBorder : Theme.border

                    readonly property bool hasTempo: controller.hasResult && controller.bpm > 0

                    ColumnLayout {
                        id: tempoColumn
                        anchors.fill: parent
                        anchors.margins: Theme.pad
                        spacing: 1

                        Text {
                            text: Lang.t("bpm")
                            color: parent.parent.hasTempo ? Theme.bpmText : Theme.textFaint
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                            font.letterSpacing: 1.4
                            font.capitalization: Font.AllUppercase
                        }

                        Text {
                            text: controller.bpm > 0 ? controller.bpm.toFixed(2) : "--"
                            color: parent.parent.hasTempo ? Theme.bpmText : Theme.textFaint
                            font.family: Theme.fontFamily
                            font.pixelSize: 44
                            font.weight: Font.Bold
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: controller.tempoFolded
                            wrapMode: Text.WordWrap
                            text: Lang.t("folded").arg (controller.searchedBpm.toFixed(2))
                            color: Theme.warn
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: controller.tempoUncertain && controller.hasResult
                            text: Lang.t("uncertain") + "  ·  "
                                  + Math.round (controller.tempoConfidence * 100) + "%"
                            color: Theme.warn
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                        }
                    }
                }

                // ---- key -------------------------------------------------
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: keyColumn.implicitHeight + Theme.pad * 2
                    color: hasKey ? Theme.keyBg : Theme.panelAlt
                    radius: Theme.radius
                    border.width: 1
                    border.color: hasKey ? Theme.keyBorder : Theme.border

                    readonly property bool hasKey: controller.keyName.length > 0

                    ColumnLayout {
                        id: keyColumn
                        anchors.fill: parent
                        anchors.margins: Theme.pad
                        spacing: 1

                        Text {
                            text: Lang.t("key")
                            color: parent.parent.hasKey ? Theme.keyText : Theme.textFaint
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                            font.letterSpacing: 1.4
                            font.capitalization: Font.AllUppercase
                        }

                        Text {
                            text: controller.keyName.length > 0
                                  ? controller.keyName : Lang.t("keyNone")
                            color: parent.parent.hasKey ? Theme.keyText : Theme.textFaint
                            font.family: Theme.fontFamily
                            font.pixelSize: controller.keyName.length > 0 ? 26 : 15
                            font.weight: Font.Bold
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: controller.keyUncertain && controller.keyName.length > 0
                            text: Lang.t("uncertain") + "  ·  "
                                  + Math.round (controller.keyConfidence * 100) + "%"
                            color: Theme.warn
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                        }

                        // The reference pitch sits with the key because that is
                        // where it belongs perceptually: it is a property of
                        // the material's tuning, and it is only meaningful
                        // alongside a tonal centre.
                        Text {
                            Layout.fillWidth: true
                            visible: controller.tuningValid
                            text: controller.tuningValid
                                  ? Lang.t("tuning") + ":  A = "
                                    + controller.referenceHz.toFixed(1) + " Hz"
                                    + (Math.abs (controller.tuningCents) >= 1.5
                                       ? "   "
                                         + (controller.tuningCents > 0 ? "+" : "")
                                         + controller.tuningCents.toFixed (0) + " "
                                         + Lang.t("cents")
                                       : "")
                                  : ""
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }

                        // The runner-up, because relative major and minor share
                        // almost all their notes and the difference is often
                        // worth a glance.
                        Text {
                            Layout.fillWidth: true
                            visible: controller.keyCandidates.length > 1
                            text: controller.keyCandidates.length > 1
                                  ? Lang.t("alternatives") + ": "
                                    + controller.keyCandidates[1].name
                                  : ""
                            color: Theme.textFaint
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }
                }

                // ---- drop / file / progress ------------------------------
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 150
                    color: dropArea.containsDrag ? Theme.panelAlt : Theme.panel
                    radius: Theme.radius
                    border.width: 1
                    border.color: dropArea.containsDrag ? Theme.accent : Theme.border

                    DropArea {
                        id: dropArea
                        anchors.fill: parent
                    }

                    ColumnLayout {
                        anchors.centerIn: parent
                        width: parent.width - Theme.pad * 2
                        spacing: 7
                        visible: !controller.busy

                        Text {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            text: controller.hasResult ? controller.fileName : Lang.t("dropHint")
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            elide: Text.ElideMiddle
                        }

                        Text {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            text: Lang.t("dropSub")
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                        }

                        Button {
                            Layout.alignment: Qt.AlignHCenter
                            text: Lang.t("dropHint")
                            onClicked: fileDialog.open()
                        }
                    }

                    ColumnLayout {
                        anchors.centerIn: parent
                        width: parent.width - Theme.pad * 2
                        spacing: 9
                        visible: controller.busy

                        Text {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            text: Lang.t("analysing")
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }

                        ProgressBar {
                            Layout.fillWidth: true
                            from: 0
                            to: 1
                            value: controller.progress
                        }

                        Text {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            text: controller.stage
                            color: Theme.textMuted
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                        }
                    }
                }

                // ---- manual tempo ----------------------------------------
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: manualRow.implicitHeight + Theme.pad
                    color: Theme.panel
                    radius: Theme.radius
                    border.width: 1
                    border.color: Theme.border
                    visible: controller.hasResult

                    RowLayout {
                        id: manualRow
                        anchors.fill: parent
                        anchors.margins: Theme.pad * 0.7
                        spacing: 6

                        TextField {
                            Layout.fillWidth: true
                            placeholderText: Lang.t("manualBpm")
                            font.family: Theme.fontFamily
                            inputMethodHints: Qt.ImhFormattedNumbersOnly

                            onAccepted: {
                                const value = parseFloat (text);
                                if (! isNaN (value) && value > 0)
                                    controller.setBpm (value);
                            }
                        }

                        Button {
                            text: Lang.t("restore")
                            onClicked: controller.restoreDetectedTempo()
                        }
                    }
                }
            }

            // ================= right column ===============================
            //
            // Two balanced columns rather than a uniform grid: the cards have
            // very different row counts, and pairing the tallest with the
            // shortest is what keeps everything on one screen. The note-value
            // table is deliberately absent - the delay and reverb tables
            // already print note labels beside their millisecond values, so it
            // was 18 rows of duplication taking up most of the page.
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 420

                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min (parent.width * 0.75, 460)
                    spacing: 8
                    visible: !controller.hasResult

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: Lang.t("emptyHint")
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: 16
                    }

                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: Lang.t("emptySub")
                        color: Theme.textFaint
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                    }
                }

                Flickable {
                    id: scroller
                    anchors.fill: parent
                    visible: controller.hasResult
                    clip: true
                    contentWidth: width
                    contentHeight: Math.max (colA.implicitHeight, colB.implicitHeight)
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { }

                    RowLayout {
                        width: scroller.width
                        spacing: Theme.gap

                        ColumnLayout {
                            id: colA
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            spacing: Theme.gap

                            Card {
                                Layout.fillWidth: true
                                title: Lang.t("preDelay")
                                subtitle: Lang.t("preDelaySub")

                                TimeTable {
                                    Layout.fillWidth: true
                                    rows: controller.preDelays
                                }
                            }

                            Card {
                                Layout.fillWidth: true
                                title: Lang.t("attack")

                                TimeTable {
                                    Layout.fillWidth: true
                                    rows: controller.attacks
                                }
                            }
                        }

                        ColumnLayout {
                            id: colB
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            spacing: Theme.gap

                            Card {
                                Layout.fillWidth: true
                                title: Lang.t("release")
                                subtitle: Lang.t("releaseSub")

                                TimeTable {
                                    Layout.fillWidth: true
                                    rows: controller.releases
                                }
                            }

                            Card {
                                Layout.fillWidth: true
                                title: Lang.t("delay")
                                subtitle: Lang.t("delaySub")

                                DelayTable {
                                    Layout.fillWidth: true
                                    rows: controller.delays
                                }
                            }

                            Card {
                                Layout.fillWidth: true
                                title: Lang.t("reverb")

                                TimeTable {
                                    Layout.fillWidth: true
                                    rows: controller.reverbs
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // The project's artwork gets the room here that it cannot have in an icon:
    // at this size its detail is the point rather than a liability.
    Dialog {
        id: aboutDialog
        anchors.centerIn: parent
        modal: true
        padding: 0
        width: 540

        contentItem: ColumnLayout {
            spacing: 0

            Image {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.round (aboutDialog.width * 1116 / 1400)
                source: "qrc:/assets/artwork.png"
                sourceSize.width: 1080
                fillMode: Image.PreserveAspectCrop
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: 22
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Image {
                        source: "qrc:/assets/tmixtool.ico"
                        sourceSize.width: 44
                        sourceSize.height: 44
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        Text {
                            text: Lang.t("appTitle")
                            color: Theme.text
                            font.family: Theme.fontFamily
                            font.pixelSize: 19
                            font.weight: Font.Bold
                        }

                        Text {
                            text: Lang.t("version") + " " + Qt.application.version
                            color: Theme.textFaint
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: Lang.t("aboutBlurb")
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: 12
                    lineHeight: 1.35
                }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: Lang.t("aboutNote")
                    color: Theme.textFaint
                    font.family: Theme.fontFamily
                    font.pixelSize: 11
                    lineHeight: 1.35
                }
            }
        }
    }

    // Analysis failures surface as a dialog rather than silently leaving the
    // previous file's numbers on screen.
    Dialog {
        id: errorDialog
        anchors.centerIn: parent
        title: Lang.t("errorTitle")
        modal: true

        Text {
            width: 320
            wrapMode: Text.WordWrap
            text: controller.error
            color: Theme.text
            font.family: Theme.fontFamily
        }

        standardButtons: Dialog.Ok
    }

    Connections {
        target: controller
        function onFailed (message) { errorDialog.open(); }
    }
}
