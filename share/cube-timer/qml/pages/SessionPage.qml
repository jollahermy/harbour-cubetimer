import QtQuick 2.0
import Sailfish.Silica 1.0

// Session picker for the current event. Tapping a session emits
// sessionPicked(index) (index into the TimerPage sessions array) and pops back.
Page {
    id: sessionPage

    property var sessionList: []     // [{index, name, count}]
    property int currentIndex: 0

    signal sessionPicked(int index)

    SilicaListView {
        id: list
        anchors.fill: parent
        model: sessionPage.sessionList

        // No header title: a spacer keeps the first row clear of the status bar.
        header: Item { width: 1; height: Theme.paddingLarge * 2 }

        delegate: ListItem {
            contentHeight: Theme.itemSizeSmall
            onClicked: {
                sessionPage.sessionPicked(modelData.index);
                pageStack.pop();
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: Theme.horizontalPageMargin
                text: modelData.name
                color: modelData.index === sessionPage.currentIndex ? Theme.highlightColor
                                                                   : Theme.primaryColor
                font.pixelSize: Theme.fontSizeLarge
            }
        }

        VerticalScrollDecorator { flickable: list }
    }
}
