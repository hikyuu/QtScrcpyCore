#include "touchinjectoradapter.h"

#include <QDebug>

#include "touchmanager.h"

TouchInjectorAdapter::TouchInjectorAdapter(TouchManager *touchManager, QObject *parent)
    : QObject(parent), m_touchManager(touchManager)
{
}

TouchInjectorAdapter::TouchHandle TouchInjectorAdapter::begin(int key, const QPointF &pos)
{
    TouchHandle h;
    h.key = key;
    if (!m_touchManager) {
        return h;
    }
    h.id = m_touchManager->attachTouchID(key);
    m_touchManager->sendTouchDownEvent(h.id, pos);
    return h;
}

void TouchInjectorAdapter::move(const TouchHandle &h, const QPointF &pos)
{
    if (!m_touchManager || !h.valid()) {
        return;
    }
    m_touchManager->sendTouchMoveEvent(h.id, pos);
}

void TouchInjectorAdapter::end(const TouchHandle &h, const QPointF &pos)
{
    if (!m_touchManager || !h.valid()) {
        return;
    }
    m_touchManager->sendTouchUpEvent(h.id, pos);
    m_touchManager->detachIndexID(h.id);
}

int TouchInjectorAdapter::idForKey(int key) const
{
    if (!m_touchManager) {
        return -1;
    }
    return m_touchManager->getTouchID(key);
}

int TouchInjectorAdapter::attachIdForKey(int key)
{
    if (!m_touchManager) {
        return -1;
    }
    return m_touchManager->attachTouchID(key);
}

void TouchInjectorAdapter::endByKey(int key, const QPointF &pos)
{
    if (!m_touchManager) {
        return;
    }
    const int id = m_touchManager->getTouchID(key);
    if (id < 0) {
        return;
    }
    m_touchManager->sendTouchUpEvent(id, pos);
    m_touchManager->detachIndexID(id);
}

void TouchInjectorAdapter::detachByKey(int key)
{
    if (!m_touchManager) {
        return;
    }
    m_touchManager->detachTouchID(key);
}

void TouchInjectorAdapter::downId(int id, const QPointF &pos)
{
    if (!m_touchManager || id < 0) {
        return;
    }
    m_touchManager->sendTouchDownEvent(id, pos);
}

void TouchInjectorAdapter::moveId(int id, const QPointF &pos)
{
    if (!m_touchManager || id < 0) {
        return;
    }
    m_touchManager->sendTouchMoveEvent(id, pos);
}

void TouchInjectorAdapter::upId(int id, const QPointF &pos)
{
    if (!m_touchManager || id < 0) {
        return;
    }
    m_touchManager->sendTouchUpEvent(id, pos);
}

void TouchInjectorAdapter::detachId(int id)
{
    if (!m_touchManager || id < 0) {
        return;
    }
//    qDebug()<< "detachId id:" << id;
    m_touchManager->detachIndexID(id);
}

void TouchInjectorAdapter::resetId(int id, int newKey)
{
    if (!m_touchManager || id < 0) {
        return;
    }
    m_touchManager->resetTouchID(id, newKey);
}
