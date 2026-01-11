// extracted from inputconvertgame.cpp
#include <Windows.h>
#include "inputconvertgame.h"

void InputConvertGame::rawMouseEvent(const int dx, const int dy, DWORD buttons)
{

    if (!m_gameMap || m_pointerMode || !m_keyMap.isValidMouseMoveMap()) {
        return;
    }

    if (dx == 0 && dy == 0) {
        return;
    }

    mouseMoveStartTouch(*new QPointF);

    const QPointF speedRatio = m_currentSpeedRatio;

    QPointF currentConvertPos(
        m_ctrlMouseMove.lastConvertPos.x() + dx / (speedRatio.x() * m_frameSize.width()),
        m_ctrlMouseMove.lastConvertPos.y() + dy / (speedRatio.y() * m_frameSize.height()));

    m_ctrlMouseMove.lastConvertPos = currentConvertPos;
    m_ctrlMouseMove.processedPos = currentConvertPos;
    m_ctrlMouseMove.waitClick = true;

    if (!m_ctrlMouseMove.needResetTouch) {
        if (currentConvertPos.x() <= 0) {
            currentConvertPos.setX(0);
        } else if (currentConvertPos.x() >= 1) {
            currentConvertPos.setX(0.99);
        }
        if (currentConvertPos.y() <= 0) {
            currentConvertPos.setY(0);
        } else if (currentConvertPos.y() >= 1) {
            currentConvertPos.setY(0.99);
        }
        m_ctrlMouseMove.lastConvertPos = currentConvertPos;
        m_ctrlMouseMove.processedPos = currentConvertPos;
        m_ctrlMouseMove.waitClick = true;
        return;
    }

    if (checkBoundary(currentConvertPos)) {

        if (currentConvertPos.x() >= 1) {
            qDebug() << "超出右边界！！！！！！";
            currentConvertPos.setX(0.99);
        } else if (currentConvertPos.x() <= 0) {
            currentConvertPos.setX(0);
        }
        if (currentConvertPos.y() >= 1)
            currentConvertPos.setY(0.99);
        else if (currentConvertPos.y() <= 0)
            currentConvertPos.setY(0);

        mouseMove(currentConvertPos);
        mouseMoveStopTouch(true);
        mouseMoveStartTouch(*new QPointF);
        m_ctrlMouseMove.waitClick = false;
        startMouseMoveTimer();
        loopTimer->start();
        qDebug() << "超出边界，重置坐标";

        const QPointF startPos = m_keyMap.getMouseMoveMap().data.mouseMove.startPos;
        m_ctrlMouseMove.leftBoundary = generateDouble(qBound(0.1, startPos.x() - 0.3, 0.9), qBound(0.1, startPos.x() - 0.25, 0.9));
        m_ctrlMouseMove.maxBoundary = generateDouble(0.9, 0.95);
        m_ctrlMouseMove.topBoundary = generateDouble(0.05, 0.1);
    }
}

bool InputConvertGame::processMouseMove(const QMouseEvent *from)
{
    if (QEvent::MouseMove != from->type()) {
        return false;
    }
    qDebug() << "normal mouse move from:" << from->localPos();
#if defined(Q_OS_WIN32)
    return true;
#endif
    mouseMoveStartTouch(*new QPointF);

    startMouseMoveTimer();

    bool outOfBoundary = checkCursorPos(from);

    QPointF lastPos = m_ctrlMouseMove.lastPos;

    if (outOfBoundary) {
        lastPos = m_ctrlMouseMove.cursorPos;
        m_ctrlMouseMove.cursorPos = from->localPos();
        //        qDebug()<< "mouse move out of boundary, lastPos:" << lastPos << "cursorPos:" << m_ctrlMouseMove.cursorPos;
    } else {
        m_ctrlMouseMove.lastPos = from->localPos();
    }
    //    qDebug() << "mouse move from:" << from->localPos() << "lastPos:" << lastPos << "outOfBoundary:" << outOfBoundary;
    QPointF speedRatio = m_currentSpeedRatio;
    QPointF distance_raw{ from->localPos() - lastPos };

    if (qAbs(distance_raw.x()) > 200) {
        qDebug() << "mouse move distance_raw:" << distance_raw;
    }

    QPointF currentConvertPos(
        m_ctrlMouseMove.lastConvertPos.x() + distance_raw.x() / (speedRatio.x() * m_frameSize.width()),
        m_ctrlMouseMove.lastConvertPos.y() + distance_raw.y() / (speedRatio.y() * m_frameSize.height()));

    if (!m_ctrlMouseMove.needResetTouch) {
        bool boundary = false;
        if (currentConvertPos.x() <= 0) {
            currentConvertPos.setX(0);
            boundary = true;
        } else if (currentConvertPos.x() >= 1) {
            currentConvertPos.setX(1);
            boundary = true;
        }
        if (currentConvertPos.y() <= 0) {
            currentConvertPos.setY(0);
            boundary = true;
        } else if (currentConvertPos.y() >= 1) {
            currentConvertPos.setY(1);
            boundary = true;
        }
        if (boundary) {
            mouseMove(currentConvertPos);
            return true;
        }
    }

    if (checkBoundary(currentConvertPos)) {
        //        qDebug() << "over the boundary";
        if (currentConvertPos.x() >= 1)
            currentConvertPos.setX(1);
        else if (currentConvertPos.x() <= 0)
            currentConvertPos.setX(0);
        if (currentConvertPos.y() >= 1)
            currentConvertPos.setY(1);
        else if (currentConvertPos.y() <= 0)
            currentConvertPos.setY(0);

        if (currentConvertPos.x() == 0 || currentConvertPos.x() == 1) {
            qDebug() << "out of pad boundary pos:" << currentConvertPos;
        }
        mouseMove(currentConvertPos);
        mouseMoveStopTouch(true);
        mouseMoveStartTouch(*new QPointF);
        m_ctrlMouseMove.leftBoundary = generateDouble(0.25, 0.4);
        m_ctrlMouseMove.maxBoundary = generateDouble(0.8, 0.95);
        m_ctrlMouseMove.topBoundary = generateDouble(0.05, 0.2);
        return true;
    }
    mouseMove(currentConvertPos);
    //    qint64 elapsedMs = timer.elapsed(); // 获取毫秒耗时
    //    qDebug() << "mouse move elapsed time:" << elapsedMs << "ms";
    return true;
}
