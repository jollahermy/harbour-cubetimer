// Native scramble service for QML without moc: QML writes a JSON request into the property map
// ("request"), a worker thread generates, and the reply comes back as the map's "reply" value.
#ifndef CT_NATIVESCRAMBLER_H
#define CT_NATIVESCRAMBLER_H

#include <QtCore/QObject>
#include <QtCore/QEvent>
#include <QtQml/QQmlPropertyMap>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

class NativeScrambler : public QObject
{
public:
    explicit NativeScrambler(const QString &entropyHex, QObject *parent = nullptr);
    ~NativeScrambler();
    QQmlPropertyMap *map() { return &map_; }

protected:
    bool event(QEvent *e) override;   // replies posted from the worker thread

private:
    struct Req { int id; std::string event; };
    QQmlPropertyMap map_;
    std::thread thread_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<Req> queue_;
    bool stop_ = false;
    std::string seed_;
    void onRequest(const QString &key, const QVariant &value);
    void run();
};

#endif
