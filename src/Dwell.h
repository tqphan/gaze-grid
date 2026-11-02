#pragma once
#include <QString>
#include <QHash>
// Monotonic caller-supplied time keeps dwell behavior deterministic in tests.
class Dwell {
public:
    bool update(const QString &target, qint64 now, int duration, bool repeat=false, int repeatDuration=-1, int graceMs=0);
    bool hasSavedProgress() const { return !saved.isEmpty(); }
    void reset();
    void pause(qint64 now);
    void inhibit(const QString &target, qint64 now);
    void consume(const QString &target, qint64 now);
    double progress(qint64 now, int duration, int repeatDuration=-1) const;
    QHash<QString,qint64> savedElapsed(qint64 now,int graceMs) const;
private:
    struct Saved { qint64 elapsed, left; };
    QHash<QString, Saved> saved;
    QString current;
    qint64 started = 0, last = -1;
    bool fired = false, repeating = false;
};
