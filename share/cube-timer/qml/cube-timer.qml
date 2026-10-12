import QtQuick 2.0
import Sailfish.Silica 1.0
import "pages"

ApplicationWindow {
    // Silica shrinks the page by the on-screen keyboard height (VirtualKeyboardObserver, active
    // while _resizeContent). Off: the page keeps its full height and the keyboard overlays the
    // bottom part. The session rename field sits near the top of the screen, so it stays visible.
    _resizeContent: false

    initialPage: Component {
        TimerPage { }
    }
}
