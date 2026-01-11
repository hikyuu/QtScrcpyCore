#include "inputconvertgame.h"
#include <QDebug>
#include <QRandomGenerator>

void InputConvertGame::processSteerWheel(const KeyMap::KeyMapNode &node, const QKeyEvent *from)
{
    const int key = from->key();
    // 是否按下
    const bool keyPress = from->type() == QEvent::KeyPress;
    const bool boostKey = key == node.data.steerWheel.boost.key;

    if (key == node.data.steerWheel.switchKey.key) {
        if (!keyPress)
            return;
        m_ctrlSteerWheel.clickMode = !m_ctrlSteerWheel.clickMode;
        return;
    }
    if (key == node.data.steerWheel.fixedKey.key) {
        if (!keyPress)
            return;
        m_ctrlSteerWheel.fixedStick = !m_ctrlSteerWheel.fixedStick;
        qDebug() << "steer wheel fixed stick:" << m_ctrlSteerWheel.fixedStick;
        return;
    }

    // identify keys
    if (key == node.data.steerWheel.up.key) {
        m_ctrlSteerWheel.pressedUp = keyPress;
        m_ctrlSteerWheel.clickPos = node.data.steerWheel.up.pos;
    } else if (key == node.data.steerWheel.right.key) {
        m_ctrlSteerWheel.pressedRight = keyPress;
        m_ctrlSteerWheel.clickPos = node.data.steerWheel.right.pos;
    } else if (key == node.data.steerWheel.down.key) {
        m_ctrlSteerWheel.pressedDown = keyPress;
        m_ctrlSteerWheel.clickPos = node.data.steerWheel.down.pos;
    } else if (key == node.data.steerWheel.left.key) { // left
        m_ctrlSteerWheel.pressedLeft = keyPress;
        m_ctrlSteerWheel.clickPos = node.data.steerWheel.left.pos;
    }
    if (m_ctrlSteerWheel.clickMode) {
        if (QEvent::KeyPress == from->type()) {
            const auto h = m_touchInjector->begin(from->key(), generatePos(m_ctrlSteerWheel.clickPos, 0.01, 3));
            Q_UNUSED(h);
        } else if (QEvent::KeyRelease == from->type()) {
            m_touchInjector->endByKey(from->key(), m_keyPosMap[from->key()]);
        }
        return;
    }
    // calc offset and wheelPressed number
    QPointF offset(0.0, 0.0);
    int pressedNum = 0;
    if (m_ctrlSteerWheel.pressedUp) {
        ++pressedNum;
        offset.ry() -= node.data.steerWheel.up.extendOffset;
        m_ctrlSteerWheel.delayData.offsetY = { 0, offset.y() };
    }
    if (m_ctrlSteerWheel.pressedRight) {
        ++pressedNum;
        offset.rx() += node.data.steerWheel.right.extendOffset;
    }
    if (m_ctrlSteerWheel.pressedDown) {
        ++pressedNum;
        offset.ry() += node.data.steerWheel.down.extendOffset;
        m_ctrlSteerWheel.delayData.offsetY = { 0, offset.y() };
    }
    if (m_ctrlSteerWheel.pressedLeft) {
        ++pressedNum;
        offset.rx() -= node.data.steerWheel.left.extendOffset;
    }
    m_ctrlSteerWheel.delayData.pressedNum = pressedNum;

    if (boostKey) {
        if (!keyPress)
            return;
        if (!m_ctrlSteerWheel.pressedUp) {
            m_ctrlSteerWheel.pressedBoost = true;
            //            qDebug() << "boost key pressed, but no up key pressed";
            return;
        }
        m_ctrlSteerWheel.pressedBoost = !m_ctrlSteerWheel.pressedBoost;
    } else if (key == node.data.steerWheel.up.key) {
        if (keyPress) {
            if (from->modifiers() & Qt::ShiftModifier && node.data.steerWheel.boost.key == Qt::Key_Shift) {
                m_ctrlSteerWheel.pressedBoost = true; // 按下shift键持续奔跑
            }
            if (from->modifiers() & Qt::ControlModifier && node.data.steerWheel.boost.key == Qt::Key_Control) {
                m_ctrlSteerWheel.pressedBoost = true; // 按下Ctrl键持续奔跑
            }
            if (from->modifiers() & Qt::AltModifier && node.data.steerWheel.boost.key == Qt::Key_Alt) {
                m_ctrlSteerWheel.pressedBoost = true; // 按下Alt键持续奔跑
            }
        } else if (pressedNum > 0) {
            m_ctrlSteerWheel.pressedBoost = false; // 取消持续奔跑
        }
    }

    if (m_ctrlSteerWheel.pressedBoost) {
        if (m_ctrlSteerWheel.pressedUp) {
            offset.ry() -= node.data.steerWheel.boost.extendOffset;
            m_ctrlSteerWheel.delayData.offsetY = { 0, offset.y() };
        }
        if (m_ctrlSteerWheel.pressedRight) {
            offset.rx() += node.data.steerWheel.boost.extendOffset;
        }
        if (m_ctrlSteerWheel.pressedLeft) {
            offset.rx() -= node.data.steerWheel.boost.extendOffset;
        }
        if (m_ctrlSteerWheel.pressedDown) {
            offset.ry() += node.data.steerWheel.boost.extendOffset;
            m_ctrlSteerWheel.delayData.offsetY = { 0, offset.y() };
        }
    }

    if (pressedNum > 1) {
        offset /= generateDouble(1.5, 2);
    }

    int frame = 3;
    if (m_ctrlSteerWheel.simulateWheel) {
        frame = 15;
    }
    m_ctrlSteerWheel.delayData.queueTimer.clear();
    m_ctrlSteerWheel.delayData.queuePos.clear();
    // last key release and timer no active, active timer to detouch
    if (pressedNum == 0) {

        if (m_ctrlSteerWheel.mobaWheel.mouseWheeling)
            return;

        if (m_ctrlSteerWheel.mobaWheel.buttonPressed) {
            //qDebug() << "方向键弹起，鼠标还按下";
            m_ctrlSteerWheel.mobaWheel.mouseWheeling = true;
            m_mobaMouseMovePending = true;
            return;
        }
        //取消持续奔跑
        // qDebug()<< "boost key released, cancel boost"<< m_ctrlSteerWheel.pressedBoost;
        m_ctrlSteerWheel.pressedBoost = false;

        const double distance = calcDistance(m_ctrlSteerWheel.delayData.currentPos, m_ctrlSteerWheel.centerPos);
        // qDebug() << "distance:" << distance;
        if (m_ctrlSteerWheel.simulateWheel && distance >= node.data.steerWheel.up.extendOffset) {
            //qDebug() << "steer move center";
            updatePosition(m_ctrlSteerWheel.centerPos);
            m_ctrlSteerWheel.delayData.path = generateBezierPath();
            getDelayQueue(m_ctrlSteerWheel.delayData.queuePos, m_ctrlSteerWheel.delayData.queueTimer, false, 1, 0, frame, m_ctrlSteerWheel.delayData.path);
            return;
        }

        if (!m_ctrlSteerWheel.wheeling) {
            return;
        }
        const int touchKeyID = m_touchInjector->idForKey(m_ctrlSteerWheel.touchKey);
        m_touchInjector->upId(touchKeyID, m_ctrlSteerWheel.delayData.currentPos);
        m_touchInjector->detachId(touchKeyID);
        m_ctrlSteerWheel.wheeling = false;
        return;
    }

    if (pressedNum > 0) {
        m_ctrlSteerWheel.mobaWheel.mouseWheeling = false;
    }

    // 只有一个按键按下，且没有正在操作方向盘
    // 如果是boost按键，则不处理
    // 如果是第一个按键，则开始操作方向盘
    if (pressedNum == 1 && keyPress && !boostKey && !m_ctrlSteerWheel.wheeling) {
        //        qDebug() << "first press";
        //        QMutexLocker locker(&m_ctrlSteerWheel.steerMutex);
        if (m_ctrlSteerWheel.wheeling) {
            return;
        }
        m_ctrlSteerWheel.wheeling = true;

        if (m_ctrlSteerWheel.mobaWheel.mouseWheeling) {
            onStopMobaWheelTimer();
        }

        const int id = m_touchInjector->attachIdForKey(m_ctrlSteerWheel.touchKey);

        QPointF centerPos = node.data.steerWheel.centerPos;
        if (m_ctrlSteerWheel.simulateWheel && !m_ctrlSteerWheel.fixedStick) {
            centerPos = generatePos(node.data.steerWheel.centerPos, 0.025, 3);
        }
        m_ctrlSteerWheel.centerPos = centerPos;

        QPointF endPos = centerPos + offset;

        if (m_ctrlSteerWheel.simulateWheel) {
            double distance = calcDistance(centerPos, endPos);
            endPos = generatePos(endPos, distance * 0.05, 2);
        }

        m_ctrlSteerWheel.delayData.historyPoints.clear();
        m_ctrlSteerWheel.delayData.historyPoints.append(centerPos);
        updatePosition(endPos);

        m_touchInjector->downId(id, centerPos);
        m_ctrlSteerWheel.delayData.currentPos = centerPos;
        m_ctrlSteerWheel.delayData.path = generateBezierPath();
        getDelayQueue(m_ctrlSteerWheel.delayData.queuePos, m_ctrlSteerWheel.delayData.queueTimer, false, 1, 0, frame, m_ctrlSteerWheel.delayData.path);
    } else {
        bool slowEnd = true;
        if (boostKey) {
            slowEnd = false; // boost 按键不需要慢速结束
        }
        const QPointF startPos = m_ctrlSteerWheel.delayData.currentPos;
        const QPointF centerPos = m_ctrlSteerWheel.centerPos;
        const QPointF endPos = centerPos + offset;

        if (pressedNum > 1) {
            m_ctrlSteerWheel.delayData.isEnd = true;
            slowEnd = false;
        }
        if (!m_ctrlSteerWheel.simulateWheel) {
            slowEnd = false;
        }
        updatePosition(endPos);
        m_ctrlSteerWheel.delayData.endPos = endPos;
        m_ctrlSteerWheel.delayData.shakeEndPos = endPos;

        if (!m_ctrlSteerWheel.wheeling)
            return;

        m_ctrlSteerWheel.delayData.path = generateLinePath(startPos, endPos);
        getDelayQueue(m_ctrlSteerWheel.delayData.queuePos, m_ctrlSteerWheel.delayData.queueTimer, slowEnd, 1, 0, frame, m_ctrlSteerWheel.delayData.path);
    }
}
