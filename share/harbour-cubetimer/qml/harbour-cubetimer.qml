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

    // Home/switcher cover: the app icon, centred, no actions or text.
    cover: CoverBackground {
        Image {
            anchors.centerIn: parent
            width: parent.width * 0.4
            height: width
            sourceSize.width: width
            sourceSize.height: height
            fillMode: Image.PreserveAspectFit
            source: Qt.resolvedUrl('../../icons/hicolor/256x256/apps/harbour-cubetimer.png')
        }
    }
}
