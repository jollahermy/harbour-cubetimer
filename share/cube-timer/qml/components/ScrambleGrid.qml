import QtQuick 2.0
import Sailfish.Silica 1.0
import "../js/scrambleLayout.js" as Layout

// Scramble renderer shared by the main page and the solve popup. Geometry comes from
// js/scrambleLayout.js (fit to maxWidth x maxHeight, largest size first):
//   generic : fixed-width token cells, rows wrap to the width
//   minx    : megaminx, one WCA line per row, never wrapped
//   sq1     : "(a,b)" moves with a '/' column after each move; numbers right-aligned in
//             sub-cells sized for the widest number, so the commas line up.
// Root size = the fitted block (width x height); the parent centres or positions it.
Item {
    id: grid

    property string text: ""
    property string eventId: ""
    property real maxWidth: 0
    property real maxHeight: 0
    property string fontFamily: ""
    property color ink: "#000000"
    readonly property var sizes: [Theme.fontSizeLarge, Theme.fontSizeMedium, Theme.fontSizeSmall, Theme.fontSizeExtraSmall]

    readonly property var prep: Layout.prepare(text, eventId)
    // Probes at 100 px (same font as the cells). Widths scale linearly in the layout.
    Repeater {
        id: tokProbe
        model: grid.prep.tokens
        Text { visible: false; font.family: grid.fontFamily; font.pixelSize: 100; text: modelData }
    }
    Repeater {
        id: numProbe
        model: grid.prep.nums
        Text { visible: false; font.family: grid.fontFamily; font.pixelSize: 100; text: modelData }
    }
    Text { id: pLine;  visible: false; font.family: grid.fontFamily; font.pixelSize: 100; text: "Ag" }
    Text { id: pParen; visible: false; font.family: grid.fontFamily; font.pixelSize: 100; text: "(" }
    Text { id: pComma; visible: false; font.family: grid.fontFamily; font.pixelSize: 100; text: "," }
    Text { id: pSlash; visible: false; font.family: grid.fontFamily; font.pixelSize: 100; text: "/" }
    Text { id: pDigit; visible: false; font.family: grid.fontFamily; font.pixelSize: 100; text: "-5" }

    function widestOf(rep) {
        var m = 0;
        for (var i = 0; i < rep.count; i++) m = Math.max(m, rep.itemAt(i).implicitWidth);
        return m;
    }
    readonly property var metrics: ({
        tokW: widestOf(tokProbe),
        lineH: pLine.implicitHeight,
        parenW: pParen.implicitWidth,
        commaW: pComma.implicitWidth,
        slashW: pSlash.implicitWidth,
        numW: Math.max(pDigit.implicitWidth, widestOf(numProbe))
    })
    readonly property var fit: Layout.fit(prep, metrics, maxWidth, maxHeight, sizes)

    width: fit ? fit.width : 0
    height: fit ? fit.height : 0

    // Generic and megaminx rows: arrays of token strings.
    Column {
        anchors.top: parent.top
        anchors.left: parent.left
        Repeater {
            model: (grid.fit && grid.fit.mode !== "sq1") ? grid.fit.rows : []
            Row {
                spacing: grid.fit.gap
                Repeater {
                    model: modelData
                    Text {
                        width: grid.fit.cw
                        height: grid.fit.lh
                        text: modelData
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.family: grid.fontFamily
                        font.pixelSize: grid.fit.size
                        color: grid.ink
                    }
                }
            }
        }
    }

    // Square-1 rows: arrays of {a, b, slash} moves. One cell = move + slash column.
    Column {
        anchors.top: parent.top
        anchors.left: parent.left
        Repeater {
            model: (grid.fit && grid.fit.mode === "sq1") ? grid.fit.rows : []
            Row {
                spacing: grid.fit.gap
                Repeater {
                    model: modelData
                    Item {
                        width: grid.fit.cellW
                        height: grid.fit.lh
                        Row {
                            // Fixed sub-cells: numbers right-aligned in the cell sized for the widest number.
                            Repeater {
                                model: [
                                    { w: grid.fit.parenW, t: "(", al: Text.AlignLeft },
                                    { w: grid.fit.numW, t: modelData.a, al: Text.AlignRight },
                                    { w: grid.fit.commaW, t: ",", al: Text.AlignHCenter },
                                    { w: grid.fit.numW, t: modelData.b, al: Text.AlignRight },
                                    { w: grid.fit.parenW, t: ")", al: Text.AlignLeft }
                                ]
                                Text {
                                    width: modelData.w
                                    height: grid.fit.lh
                                    text: modelData.t
                                    horizontalAlignment: modelData.al
                                    verticalAlignment: Text.AlignVCenter
                                    font.family: grid.fontFamily
                                    font.pixelSize: grid.fit.size
                                    color: grid.ink
                                }
                            }
                        }
                        Text {
                            x: grid.fit.moveW
                            width: grid.fit.sepW
                            height: grid.fit.lh
                            text: modelData.slash ? "/" : ""
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.family: grid.fontFamily
                            font.pixelSize: grid.fit.size
                            color: grid.ink
                        }
                    }
                }
            }
        }
    }
}
