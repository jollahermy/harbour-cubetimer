import QtQuick 2.0
import Sailfish.Silica 1.0

// Event picker. Opened from the TimerPage event button.
// Picking an event emits eventPicked(index) and pops back.
Page {
    id: eventPage

    property var events: []
    property int currentIndex: 0

    signal eventPicked(int index)

    SilicaListView {
        id: list
        anchors.fill: parent
        model: eventPage.events

        // No header title: a spacer keeps the first row clear of the status bar.
        header: Item { width: 1; height: Theme.paddingLarge * 2 }

        delegate: ListItem {
            contentHeight: Theme.itemSizeSmall
            onClicked: {
                eventPage.eventPicked(index);
                pageStack.pop();
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: Theme.horizontalPageMargin
                text: modelData.name
                color: index === eventPage.currentIndex ? Theme.highlightColor
                                                       : Theme.primaryColor
                font.pixelSize: Theme.fontSizeLarge
            }
        }

        VerticalScrollDecorator { flickable: list }
    }
}
