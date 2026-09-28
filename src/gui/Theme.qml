pragma Singleton

import QtQuick

// The whole palette in one place.
//
// Cool blue-grey, chosen over the warm option because the numbers in this tool
// are read at a glance from across the desk, and a cool background keeps the
// white cards reading as separate panels rather than blending together.
QtObject {
    // surfaces
    readonly property color bg:        "#eef2f6"
    readonly property color panel:     "#ffffff"
    readonly property color panelAlt:  "#f6f9fb"
    readonly property color border:    "#d6e0e8"
    readonly property color borderSoft:"#e4ecf2"

    // text
    readonly property color text:      "#16303f"
    readonly property color textMuted: "#6b8496"
    readonly property color textFaint: "#9db0be"

    // accents
    readonly property color accent:    "#3aa0d8"
    readonly property color bpmBg:     "#e3eff8"
    readonly property color bpmBorder: "#bcd8ec"
    readonly property color bpmText:   "#2b6c9c"
    readonly property color keyBg:     "#e8ecf8"
    readonly property color keyBorder: "#c6cfeb"
    readonly property color keyText:   "#4a5aa8"
    readonly property color warn:      "#c47d2a"
    readonly property color warnBg:    "#fdf4e6"
    readonly property color danger:    "#c0504d"

    readonly property int radius:      10
    readonly property int radiusSmall: 6
    readonly property int gap:         12
    readonly property int pad:         16

    readonly property string fontFamily: "Segoe UI"
}
