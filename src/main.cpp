#include <sailfishapp.h>
#include <QtGui/QGuiApplication>
#include <QtQuick/QQuickView>
#include <QtQml/QQmlContext>
#include <QtCore/QFile>
#include <QtCore/QScopedPointer>
#include "nativescrambler.h"

// Entropy for the scramble RNG: 32 bytes from the kernel CSPRNG, hex-encoded and handed to QML
// as "entropySeed" (the JS engine has no crypto API). Empty string if /dev/urandom is unreadable.
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
    QScopedPointer<QQuickView> view(SailfishApp::createView());
    view->rootContext()->setContextProperty(QStringLiteral("entropySeed"), entropy);
    view->rootContext()->setContextProperty(QStringLiteral("nativeScrambler"), scrambler.map());
    view->setSource(SailfishApp::pathToMainQml());
    view->show();
    return app->exec();
}
