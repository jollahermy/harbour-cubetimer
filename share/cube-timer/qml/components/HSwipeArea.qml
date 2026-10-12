import QtQuick 2.0
import Sailfish.Silica 1.0

// Horizontal swipe with direction lock.
// The axis is decided after 12 px of movement:
//   |dx| > 0.7 * |dy|  -> horizontal: this area keeps the gesture (preventStealing) and the
//                         flickable(s) are made non-interactive until release/cancel.
//   otherwise           -> vertical: nothing is taken; the flickable (ListView scroll or the
//                         page PullDownMenu) receives the gesture.
// A plain tap (no lock) reaches the owner through its own onClicked.
MouseArea {
    id: area

    property Flickable flickable           // locked while a horizontal swipe is in progress
    property Flickable flickable2          // optional second flickable (e.g. a list inside a page)
    property int mode: 0                   // 0 undecided, 1 horizontal, 2 vertical
    property bool swiped: false            // true once a horizontal drag was recognised
    property real startX: 0
    property real startY: 0

    signal dragStart()
    signal dragMove(real dx)
    signal dragEnd(real dx)

    // Long press: fires holdMs after the press if the finger stayed within startDragDistance
    // and no swipe was recognised. 0 disables it. Cancelled by any larger move, release or cancel.
    property int holdMs: 0
    signal longPress()

    Timer {
        id: holdPressTimer
        interval: area.holdMs
        repeat: false
        onTriggered: if (area.mode === 0 && area.holdMs > 0) area.longPress()
    }

    function lock() {
        area.preventStealing = true;
        if (area.flickable) area.flickable.interactive = false;
        if (area.flickable2) area.flickable2.interactive = false;
    }
    function unlock() {
        area.preventStealing = false;
        if (area.flickable) area.flickable.interactive = true;
        if (area.flickable2) area.flickable2.interactive = true;
    }

    onPressed: {
        startX = mouse.x;
        startY = mouse.y;
        mode = 0;
        swiped = false;
        holdPressTimer.stop();
        if (area.holdMs > 0) holdPressTimer.restart();
    }
    onPositionChanged: {
        var dx = mouse.x - startX;
        var dy = mouse.y - startY;
        var slop = Theme.startDragDistance || 15;
        if (holdPressTimer.running && (mode !== 0 || Math.abs(dx) >= slop || Math.abs(dy) >= slop))
            holdPressTimer.stop();
        if (mode === 0 && (Math.abs(dx) > 12 || Math.abs(dy) > 12)) {
            if (Math.abs(dx) > 0.7 * Math.abs(dy)) {
                mode = 1;
                swiped = true;
                lock();
                dragStart();
            } else {
                mode = 2;
            }
        }
        if (mode === 1) dragMove(dx);
    }
    onReleased: {
        holdPressTimer.stop();
        var was = mode === 1;
        var dx = mouse.x - startX;
        mode = 0;
        if (was) {
            unlock();
            dragEnd(dx);
        }
    }
    onCanceled: {
        holdPressTimer.stop();
        var was = mode === 1;
        mode = 0;
        if (was) {
            unlock();
            dragEnd(0);      // owner snaps back
        }
    }
}
