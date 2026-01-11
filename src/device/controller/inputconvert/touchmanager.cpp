#include "touchmanager.h"
#include "controlmsg.h"
#include "input.h"
#include <QDebug>

TouchManager::TouchManager(Controller *controller, QObject *parent)
    : QObject(parent), m_controller(controller)
{
    m_multiTouchID.clear();
}

void TouchManager::updateSize(const QSize &frameSize)
{
    m_frameSize = frameSize;
}

int TouchManager::attachTouchID(int key)
{
    for (int i = 0; i < m_multiTouchID.size(); ++i) {
        if (m_multiTouchID[i] == 0) {
            m_multiTouchID[i] = key;
            return i;
        }
    }
    int index = m_multiTouchID.size();
    m_multiTouchID.append(key);
    return index;
}

void TouchManager::detachTouchID(int key)
{
    for (int i = 0; i < m_multiTouchID.size(); ++i) {
        if (m_multiTouchID.at(i) == key) {
            if (i == m_multiTouchID.size() - 1) {
                m_multiTouchID.removeLast();
            } else {
                m_multiTouchID[i] = 0;
            }
            return;
        }
    }
}

void TouchManager::detachIndexID(int i)
{
    if (i < 0 || i >= m_multiTouchID.size()) {
        return;
    }
    m_multiTouchID[i] = Qt::Key_unknown;
    if (i == m_multiTouchID.size() - 1) {
        m_multiTouchID.removeLast();
    } else {
        m_multiTouchID[i] = 0;
    }
}

int TouchManager::getTouchID(int key) const
{
    for (int i = 0; i < m_multiTouchID.size(); ++i) {
        if (key == m_multiTouchID[i]) {
            return i;
        }
    }
    return -1;
}

void TouchManager::sendTouchDownEvent(int id, QPointF pos)
{
    sendTouchEvent(id, pos, AMOTION_EVENT_ACTION_DOWN);
}

void TouchManager::sendTouchMoveEvent(int id, QPointF pos)
{
    sendTouchEvent(id, pos, AMOTION_EVENT_ACTION_MOVE);
}

void TouchManager::sendTouchUpEvent(int id, QPointF pos)
{
    sendTouchEvent(id, pos, AMOTION_EVENT_ACTION_UP);
}

void TouchManager::sendTouchEvent(int id, QPointF pos, AndroidMotioneventAction action)
{
    if (!m_controller) {
        qWarning() << "TouchManager: Controller is null";
        return;
    }
    ControlMsg *msg = new ControlMsg(ControlMsg::CMT_INJECT_TOUCH);
    if (!msg) {
        qWarning() << "TouchManager: Failed to create ControlMsg";
        return;
    }

    const QPointF absPos = calcFrameAbsolutePos(pos);
    const QRect position(static_cast<int>(absPos.x()), static_cast<int>(absPos.y()), m_frameSize.width(), m_frameSize.height());

    const float pressure = (action == AMOTION_EVENT_ACTION_DOWN) ? 1.0f : 0.0f;
    msg->setInjectTouchMsgData(
        static_cast<quint64>(id),
        action,
        static_cast<AndroidMotioneventButtons>(0),
        static_cast<AndroidMotioneventButtons>(0),
        position,
        pressure
    );
    m_controller->postControlMsg(msg);
}

QPointF TouchManager::calcFrameAbsolutePos(QPointF relativePos)
{
    return QPointF(relativePos.x() * m_frameSize.width(), relativePos.y() * m_frameSize.height());
}

void TouchManager::resetTouchID(int id, int key) {
    m_multiTouchID[id] = key;
}
