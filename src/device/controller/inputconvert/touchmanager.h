#ifndef TOUCHMANAGER_H
#define TOUCHMANAGER_H

#include <QObject>
#include <QPointF>
#include <QSize>
#include <QList>
#include "input.h"
#include "controller.h"

class Controller;

class TouchManager : public QObject
{
    Q_OBJECT
public:
    explicit TouchManager(Controller *controller, QObject *parent = nullptr);

    void updateSize(const QSize &frameSize);

    int attachTouchID(int key);
    void detachTouchID(int key);
    void detachIndexID(int index);
    int getTouchID(int key) const;

    void sendTouchDownEvent(int id, QPointF pos);
    void sendTouchMoveEvent(int id, QPointF pos);
    void sendTouchUpEvent(int id, QPointF pos);
    void resetTouchID(int id, int key); // 解绑/重置touchID
private:
    void sendTouchEvent(int id, QPointF pos, AndroidMotioneventAction action);
    QPointF calcFrameAbsolutePos(QPointF relativePos);

    Controller *m_controller;
    QSize m_frameSize;
    QList<int> m_multiTouchID;
};

#endif // TOUCHMANAGER_H
