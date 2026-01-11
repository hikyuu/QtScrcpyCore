#ifndef TOUCHINJECTORADAPTER_H
#define TOUCHINJECTORADAPTER_H

#include <QObject>
#include <QPointF>

class TouchManager;

// A very thin adapter over TouchManager.
// Goal (phase S1): avoid leaking TouchManager's attach/get/detach details into
// higher-level input conversion logic, without changing any behavior.
class TouchInjectorAdapter : public QObject
{
    Q_OBJECT
public:
    struct TouchHandle {
        int key = 0;
        int id = -1;
        bool valid() const { return id >= 0; }
    };

    explicit TouchInjectorAdapter(TouchManager *touchManager, QObject *parent = nullptr);

    TouchHandle begin(int key, const QPointF &pos);
    void move(const TouchHandle &h, const QPointF &pos);
    void end(const TouchHandle &h, const QPointF &pos);

    // Key-based helpers (mirror current call-sites patterns).
    int idForKey(int key) const;
    int attachIdForKey(int key);
    void endByKey(int key, const QPointF &pos);

    // Best-effort stop.
    void detachByKey(int key);

    // Low-level helpers (phase S2): keep ordering identical where required.
    void downId(int id, const QPointF &pos);
    void moveId(int id, const QPointF &pos);
    void upId(int id, const QPointF &pos);
    void detachId(int id);
    void resetId(int id, int newKey);

private:
    TouchManager *m_touchManager = nullptr;
};

#endif // TOUCHINJECTORADAPTER_H
