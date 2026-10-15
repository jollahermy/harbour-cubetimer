#include <sailfishapp.h>
#include <QtGui/QGuiApplication>
#include <QtQuick/QQuickView>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlPropertyMap>
#include <QtGui/QStyleHints>
#include <QtCore/QFile>
#include <QtCore/QScopedPointer>
#include "nativescrambler.h"

// Seed for the scramble RNG: 32 bytes from the kernel CSPRNG, hex-encoded (NativeScrambler also
// mixes in the clock). Empty string if /dev/urandom is unreadable.
static QString readEntropy()
{
    QFile f(QStringLiteral("/dev/urandom"));
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromLatin1(f.read(32).toHex());
}

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    const QString entropy = readEntropy();
    NativeScrambler scrambler(entropy);            // outlives the view (declared first)
    // uiHints: QML sets "dragDistance" to change the app-wide drag threshold (used while a hold
    // is still red, so a small finger wobble does not cancel it). No moc: a property map.
    QQmlPropertyMap uiHints;                       // outlives the view (declared first)
    QStyleHints *hints = QGuiApplication::styleHints();
    uiHints.insert(QStringLiteral("defaultDragDistance"), hints->startDragDistance());
    uiHints.insert(QStringLiteral("dragDistance"), hints->startDragDistance());
    QObject::connect(&uiHints, &QQmlPropertyMap::valueChanged, [hints](const QString &key, const QVariant &v) {
        if (key == QLatin1String("dragDistance") && v.toInt() > 0)
            hints->setStartDragDistance(v.toInt());
    });
    QScopedPointer<QQuickView> view(SailfishApp::createView());
    view->rootContext()->setContextProperty(QStringLiteral("nativeScrambler"), scrambler.map());
    view->rootContext()->setContextProperty(QStringLiteral("uiHints"), &uiHints);
    view->setSource(SailfishApp::pathToMainQml());
    view->show();
    return app->exec();
}
