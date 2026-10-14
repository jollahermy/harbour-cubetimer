#include "nativescrambler.h"
#include "scramble/scramble.h"
#include <QtCore/QCoreApplication>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <chrono>
#include <vector>

namespace {
const QEvent::Type ReplyEventType = static_cast<QEvent::Type>(QEvent::User + 731);
struct ReplyEvent : public QEvent {
    ReplyEvent(const QString &j) : QEvent(ReplyEventType), json(j) {}
    QString json;
};
}

NativeScrambler::NativeScrambler(const QString &entropyHex, QObject *parent)
    : QObject(parent), seed_(entropyHex.toStdString())
{
    map_.insert(QStringLiteral("reply"), QString());
    map_.insert(QStringLiteral("request"), QString());
    connect(&map_, &QQmlPropertyMap::valueChanged, this, &NativeScrambler::onRequest);
    thread_ = std::thread(&NativeScrambler::run, this);
}

NativeScrambler::~NativeScrambler()
{
    {
        std::lock_guard<std::mutex> l(mu_);
        stop_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable())
        thread_.join();
}

void NativeScrambler::onRequest(const QString &key, const QVariant &value)
{
    if (key != QLatin1String("request"))
        return;
    QJsonObject o = QJsonDocument::fromJson(value.toString().toUtf8()).object();
    if (o.isEmpty())
        return;
    {
        std::lock_guard<std::mutex> l(mu_);
        queue_.push_back({ o.value(QStringLiteral("reqId")).toInt(), o.value(QStringLiteral("event")).toString().toStdString() });
    }
    cv_.notify_one();
}

void NativeScrambler::run()
{
    ct::Scrambler s;
    // Kernel entropy (passed from main) plus the clock, so two instances never share a stream.
    s.seed(seed_ + "|" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    // Idle warm-up: build each puzzle's tables ahead of time (one discarded scramble per event),
    // but only while no request is waiting, so the first real scramble is never delayed by it.
    std::vector<std::string> warm;
    for (const char *e : { "333", "444", "sq1", "222", "skewb", "pyram" })
        if (ct::Scrambler::supported(e))
            warm.push_back(e);
    size_t warmed = 0;
    for (;;) {
        Req r;
        {
            std::unique_lock<std::mutex> l(mu_);
            if (warmed < warm.size() && queue_.empty() && !stop_) {
                l.unlock();
                s.generate(warm[warmed++]);
                continue;
            }
            cv_.wait(l, [this] { return stop_ || !queue_.empty(); });
            if (stop_)
                return;
            r = queue_.front();
            queue_.pop_front();
        }
        auto t0 = std::chrono::steady_clock::now();
        std::string out = s.generate(r.event);
        int ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count());
        QJsonObject o;
        o.insert(QStringLiteral("reqId"), r.id);
        o.insert(QStringLiteral("event"), QString::fromStdString(r.event));
        o.insert(QStringLiteral("scramble"), QString::fromStdString(out));
        o.insert(QStringLiteral("error"), out.empty() ? QStringLiteral("not available natively") : QString());
        o.insert(QStringLiteral("ms"), ms);
        QCoreApplication::postEvent(this, new ReplyEvent(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact))));
    }
}

bool NativeScrambler::event(QEvent *e)
{
    if (e->type() == ReplyEventType) {
        // insert() updates QML bindings to nativeScrambler.reply (each reply is a distinct string).
        map_.insert(QStringLiteral("reply"), static_cast<ReplyEvent *>(e)->json);
        return true;
    }
    return QObject::event(e);
}
