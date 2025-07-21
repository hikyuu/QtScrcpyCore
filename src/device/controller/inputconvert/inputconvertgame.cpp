#include <QCursor>
#include <QDebug>
#include <QGuiApplication>
#include <QRandomGenerator>
#include <QTime>
#include <QTimer>

#include "inputconvertgame.h"
#include <QHash>
#include <QHashData>
#include <QHashDummyValue>
#include <QLine>
#include <QLineF>
#include <Windows.h>
#include <xlocale>
#include <QApplicationStateChangeEvent>
#include <QtMath>

#define CURSOR_POS_CHECK_WIDTH 250
#define CURSOR_POS_CHECK_HEIGHT 150

InputConvertGame::InputConvertGame(Controller *controller) : InputConvertNormal(controller)
{
    m_ctrlSteerWheel.delayData.timer = new QTimer(this);
    m_ctrlSteerWheel.delayData.timer->setSingleShot(true);
    connect(m_ctrlSteerWheel.delayData.timer, &QTimer::timeout, this, &InputConvertGame::onSteerWheelTimer);

    m_ctrlMouseMove.resetMoveTimer.setInterval(500);
    connect(&m_ctrlMouseMove.resetMoveTimer, &QTimer::timeout, this, &InputConvertGame::onResetMoveTimer);

    m_ctrlSteerWheel.mobaWheel.stopTimer = new QTimer(this);
    m_ctrlSteerWheel.mobaWheel.stopTimer->setSingleShot(true);
    connect(m_ctrlSteerWheel.mobaWheel.stopTimer, &QTimer::timeout, this, &InputConvertGame::onStopMobaWheelTimer);
}

InputConvertGame::~InputConvertGame() {}

void InputConvertGame::mouseEvent(const QMouseEvent *from, const QSize &frameSize, const QSize &showSize)
{
    updateSize(frameSize, showSize);
    if (m_gameMap) {
        // mouse move
        if (!m_pointerMode && m_keyMap.isValidMouseMoveMap()) {
            if (processMouseMove(from)) {
                return;
            }
        }

        if (m_keyMap.isValidMobaWheel()) {
            if (processMobaMouseMove(from)) {
                return;
            }
        }

        // mouse click
        if (processMouseClick(from)) {
            return;
        }
    }
    if (m_customNormalMouseClick) {
        processCustomMouseClick(from, frameSize, showSize);
        return;
    }
    // 处理开关按键
    if (!m_keyMap.isSwitchOnKeyboard() && m_keyMap.getSwitchKey() == from->button()) {
        if (from->type() != QEvent::MouseButtonPress) {
            return;
        }
        if (!switchGameMap()) {
            m_pointerMode = false;
        }
        return;
    }
    InputConvertNormal::mouseEvent(from, frameSize, showSize);
}

void InputConvertGame::wheelEvent(const QWheelEvent *from, const QSize &frameSize, const QSize &showSize)
{
    if (m_gameMap) {
        // start this
        qDebug() << "wheel begin:" << m_pointerMode;

        if (!m_pointerMode) {
            return;
        }
        int id = attachTouchID(QEvent::Wheel);
        QPointF startPos;
        if (m_wheelDelayData.wheeling) {
            m_wheelDelayData.upTimer->stop();
            startPos = m_wheelDelayData.endPos;
        } else {
            // pos
            QPointF pos = from->position();
            // convert pos
            startPos.setX(pos.x() / showSize.width());
            startPos.setY(pos.y() / showSize.height());
            if (from->angleDelta().y() > 0) {
                startPos.setY(0.1);
            } else {
                startPos.setY(0.9);
            }
        }
        QPointF endPos = QPointF(startPos.x(), startPos.y());

        double relativeDistanceY = 0.1;
        if (from->angleDelta().y() > 0) {
            endPos.setY(startPos.y() + relativeDistanceY);
        } else {
            endPos.setY(startPos.y() - relativeDistanceY);
        }
        if (endPos.y() > 0.95 || endPos.y() < 0.05) {
            if (m_wheelDelayData.wheeling) {
                m_wheelDelayData.upTimer->stop();
                m_wheelDelayData.upTimer->start(0);
            }
            return;
        }

        if (!m_wheelDelayData.wheeling) {
            m_wheelDelayData.pressKey = QEvent::Wheel;
            m_wheelDelayData.wheeling = true;
            sendTouchDownEvent(id, startPos);
            m_wheelDelayData.timer = new QTimer(this);
            m_wheelDelayData.upTimer = new QTimer(this);
            m_wheelDelayData.timer->setSingleShot(true);
            m_wheelDelayData.upTimer->setSingleShot(true);
            m_wheelDelayData.wheelDelayUpTime = 1000;
            connect(m_wheelDelayData.timer, &QTimer::timeout, this, &InputConvertGame::onWheelScrollTimer);
            connect(m_wheelDelayData.upTimer, &QTimer::timeout, this, &InputConvertGame::onWheelUpTimer);
        }
        getDelayQueue(startPos, endPos, 0.001f, 0.002f, 2, 4,
                      m_wheelDelayData.queuePos, m_wheelDelayData.queueTimer);
        m_wheelDelayData.endPos = endPos;
        m_wheelDelayData.timer->start();
    } else {
        InputConvertNormal::wheelEvent(from, frameSize, showSize);
    }
}

void InputConvertGame::keyEvent(const QKeyEvent *from, const QSize &frameSize, const QSize &showSize)
{
    if (!from || from->isAutoRepeat()) {
        return;
    }
    // 处理开关按键
    if (m_keyMap.isSwitchOnKeyboard() && m_keyMap.getSwitchKey() == from->key()) {
        if (QEvent::KeyPress != from->type()) {
            return;
        }
        if (!switchGameMap()) {
            m_pointerMode = false;
        }
        return;
    }

    const KeyMap::KeyMapNode &node = getNode(from);

    // 处理特殊按键：可以释放出鼠标的按键
    if (m_gameMap && node.switchMap) {
        updateSize(frameSize, showSize);
        // Qt::Key_Tab Qt::Key_M for PUBG mobile
        switchMouse(node, from);
    }

    if (m_gameMap) {
        updateSize(frameSize, showSize);
        // small eyes
        if (m_keyMap.isValidMouseMoveMap() && from->key() == m_keyMap.getMouseMoveMap().data.mouseMove.smallEyes.key) {
            m_ctrlMouseMove.smallEyes = (QEvent::KeyPress == from->type());
            if (QEvent::KeyPress == from->type()) {
                m_currentSpeedRatio = QPointF(5, 2.5);
                stopMouseMoveTimer();
                resetMouseMove(*new QPointF);
            } else {
                m_currentSpeedRatio = m_keyMap.getMouseMoveMap().data.mouseMove.speedRatio;
                resetMouseMove(*new QPointF);
            }
            return;
        }
        processType(node, from);
    } else {
        InputConvertNormal::keyEvent(from, frameSize, showSize);
    }
}

void InputConvertGame::processType(KeyMap::KeyMapNode node, const QKeyEvent *from)
{
    switch (node.type) {
    // 处理方向盘
    case KeyMap::KMT_STEER_WHEEL:
        processSteerWheel(node, from);
        break;
    // 处理普通按键
    case KeyMap::KMT_CLICK:
        processKeyClick(false, node, from);
        processAndroidKey(node.data.click.keyNode.androidKey, from);
        break;
    case KeyMap::KMT_CLICK_TWICE:
        processKeyClick(true, node, from);
        processAndroidKey(node.data.clickTwice.keyNode.androidKey, from);
        break;
    case KeyMap::KMT_CLICK_MULTI:
        processKeyClickMulti(
                node.data.clickMulti.keyNode.delayClickNodes,
                node.data.clickMulti.keyNode.delayClickNodesCount,
                node.data.clickMulti.pressTime,
                from);
        break;
    case KeyMap::KMT_DRAG:
        processKeyDrag(node.data.drag.keyNode.pos, node.data.drag.keyNode.extendPos, from);
        break;
    case KeyMap::KMT_ANDROID_KEY:
        processAndroidKey(node.data.androidKey.keyNode.androidKey, from);
        break;
    case KeyMap::KMT_ROTARY_TABLE:
        processRotaryTable(node, from);
        break;
    case KeyMap::KMT_DUAL_MODE:
        processDualMode(node, from);
        break;
    case KeyMap::KMT_PRESS_RELEASE:
        processPressRelease(node, from);
        break;
    case KeyMap::KMT_MOBA_SKILL:
        processMobaSkill(node, from);
        break;
    case KeyMap::KMT_BURST_CLICK:
        processBurstClick(node, from);
        break;
    default:
//        qDebug() << "Invalid key map type";
        break;
    }
    bool freshMouseMove = node.freshMouseMove;
    if (freshMouseMove) {
        stopMouseMoveTimer();
        mouseMoveStopTouch();
    }
}

bool InputConvertGame::isCurrentCustomKeymap()
{
    return m_gameMap;
}

void InputConvertGame::loadKeyMap(const QString &json)
{
    m_keyMap.loadKeyMap(json);
    if (m_keyMap.isValidMouseMoveMap()) {
        m_currentSpeedRatio = m_keyMap.getMouseMoveMap().data.mouseMove.speedRatio;
        m_customNormalMouseClick = m_keyMap.getCustomMouseClick();
    }
}

void InputConvertGame::updateSize(const QSize &frameSize, const QSize &showSize)
{
    if (showSize != m_showSize) {
        if (m_gameMap && m_keyMap.isValidMouseMoveMap()) {
#ifdef QT_NO_DEBUG
            // show size change, resize grab cursor
            emit grabCursor(true);
#endif
        }
        m_frameSize = frameSize;
        m_showSize = showSize;
        m_showSizeRatio = m_showSize.width() / m_showSize.height();
        m_ctrlMouseMove.centerPos = QPoint(m_showSize.width()/2, m_showSize.height()/2);
        qDebug() << "updateSize frameSize:" << m_frameSize << "showSize:" << m_showSize;
    }
}

void InputConvertGame::sendTouchDownEvent(int id, QPointF pos)
{
    sendTouchEvent(id, pos, AMOTION_EVENT_ACTION_DOWN);
}

void InputConvertGame::sendTouchMoveEvent(int id, QPointF pos)
{
    sendTouchEvent(id, pos, AMOTION_EVENT_ACTION_MOVE);
}

void InputConvertGame::sendTouchUpEvent(int id, QPointF pos)
{
    sendTouchEvent(id, pos, AMOTION_EVENT_ACTION_UP);
}

void InputConvertGame::sendTouchEvent(int id, QPointF pos, AndroidMotioneventAction action)
{
    if (0 > id || MULTI_TOUCH_MAX_NUM - 1 < id) {
        //        Q_ASSERT(0);
        return;
    }
    auto *controlMsg = new ControlMsg(ControlMsg::CMT_INJECT_TOUCH);
    if (!controlMsg) {
        return;
    }

    QPoint absolutePos = calcFrameAbsolutePos(pos).toPoint();
    static QPoint lastAbsolutePos = absolutePos;
    if (AMOTION_EVENT_ACTION_MOVE == action && lastAbsolutePos == absolutePos) {
        delete controlMsg;
        return;
    }
    lastAbsolutePos = absolutePos;

    controlMsg->setInjectTouchMsgData(
            static_cast<quint64>(id),
            action,
            static_cast<AndroidMotioneventButtons>(0),
            static_cast<AndroidMotioneventButtons>(0),
            QRect(absolutePos, m_frameSize),
            AMOTION_EVENT_ACTION_DOWN == action ? 1.0f : 0.0f);
    sendControlMsg(controlMsg);
}

void InputConvertGame::sendKeyEvent(AndroidKeyeventAction action, AndroidKeycode keyCode)
{
    auto *controlMsg = new ControlMsg(ControlMsg::CMT_INJECT_KEYCODE);
    if (!controlMsg) {
        return;
    }

    controlMsg->setInjectKeycodeMsgData(action, keyCode, 0, AMETA_NONE);
    sendControlMsg(controlMsg);
}

QPointF InputConvertGame::calcFrameAbsolutePos(QPointF relativePos)
{
    QPointF absolutePos;
    absolutePos.setX(m_frameSize.width() * relativePos.x());
    absolutePos.setY(m_frameSize.height() * relativePos.y());
    return absolutePos;
}

QPointF InputConvertGame::calcScreenAbsolutePos(QPointF relativePos)
{
    QPointF absolutePos;
    absolutePos.setX(m_showSize.width() * relativePos.x());
    absolutePos.setY(m_showSize.height() * relativePos.y());
    return absolutePos;
}

int InputConvertGame::attachTouchID(int key)
{
    for (int i = 0; i < MULTI_TOUCH_MAX_NUM; i++) {
        if (0 == m_multiTouchID[i]) {
            m_multiTouchID[i] = key;
            return i;
        }
    }
    return -1;
}

void InputConvertGame::detachTouchID(int key)
{
    for (int i = 0; i < MULTI_TOUCH_MAX_NUM; i++) {
        if (key == m_multiTouchID[i]) {
            m_multiTouchID[i] = 0;
            return;
        }
    }
}


void InputConvertGame::detachIndexID(int i){
    m_multiTouchID[i] = Qt::Key_unknown;
    QTimer::singleShot(30, this, [this,i]() {
        m_multiTouchID[i] = 0;
    });
}

int InputConvertGame::getTouchID(int key) const {
    for (int i = 0; i < MULTI_TOUCH_MAX_NUM; i++) {
        if (key == m_multiTouchID[i]) {
            return i;
        }
    }
    return -1;
}

int InputConvertGame::getTouchIDNumber(int key) const {
    int number = 0;
    for (int i : m_multiTouchID) {
        if (key == i) {
            number++;
        }
    }
    return number;
}


// -------- steer wheel event --------

void InputConvertGame::getDelayQueue(
    const QPointF &start,
    const QPointF &end,
    const double &distanceStep,
    const double &randomRange,
    quint32 lowestTimer,
    quint32 highestTimer,
    QQueue<QPointF> &queuePos,
    QQueue<quint32> &queueTimer)
{
    double x1 = start.x();
    double y1 = start.y();
    double x2 = end.x();
    double y2 = end.y();

    double dx = x2 - x1;
    double dy = y2 - y1;
    double e = (fabs(dx) > fabs(dy)) ? fabs(dx) : fabs(dy);
    e /= distanceStep;
    dx /= e;
    dy /= e;

    QQueue<QPointF> queue;
    QQueue<quint32> queue2;
    for (int i = 1; i <= e; i++) {
        QPointF pos(x1 + (QRandomGenerator::global()->bounded(randomRange * 2) - randomRange),
                y1 + (QRandomGenerator::global()->bounded(randomRange * 2) - randomRange));
        queue.enqueue(pos);
        queue2.enqueue(QRandomGenerator::global()->bounded(lowestTimer, highestTimer + 1));
        x1 += dx;
        y1 += dy;
    }

    queue.enqueue(QPointF(end.x() + QRandomGenerator::global()->bounded(randomRange * 2) - randomRange,
                          end.y() + QRandomGenerator::global()->bounded(randomRange * 2) - randomRange));
    queue2.enqueue(QRandomGenerator::global()->bounded(lowestTimer, highestTimer + 1));
    queuePos = queue;
    queueTimer = queue2;
}

void InputConvertGame::onResetMoveTimer() {
    if (!m_ctrlMouseMove.needResetTouch) {
        return;
    }
    if (m_ctrlMouseMove.startPos == m_ctrlMouseMove.lastConvertPos) {
//        qDebug() << "reset move timer, but no move";
        return;
    }
    if (!m_ctrlMouseMove.mouseMutex.try_lock()) {
        qDebug() << "reset move timer, but mouse mutex is locked";
        return;
    }
    m_ctrlMouseMove.resetMoveTimer.stop();
    if (m_ctrlMouseMove.touching) {
        m_ctrlMouseMove.touching = false;
        int id = getTouchID(Qt::ExtraButton24);
        detachIndexID(id);
        sendTouchUpEvent(id, m_ctrlMouseMove.lastConvertPos);
    }
    m_ctrlMouseMove.mouseMutex.unlock();
    QTimer::singleShot(QRandomGenerator::global()->bounded(50,100), this, [this]() {
        mouseMoveStartTouch(*new QPointF());
    });

    m_ctrlMouseMove.resetMoveDelay = QRandomGenerator::global()->bounded(500, 1000);
//    qDebug() << "reset move timer next delay:" << m_ctrlMouseMove.resetMoveDelay;

}

void InputConvertGame::onSteerWheelTimer()
{
    onWheelTimer(m_ctrlSteerWheel.touchKey);
}

void InputConvertGame::onStopMobaWheelTimer(){
    qDebug() << "stop move";
    if (m_ctrlSteerWheel.delayData.timer->isActive()) {
        m_ctrlSteerWheel.delayData.timer->stop();
        m_ctrlSteerWheel.delayData.queueTimer.clear();
        m_ctrlSteerWheel.delayData.queuePos.clear();
    }
    int id = getTouchID(m_ctrlSteerWheel.touchKey);
    sendTouchUpEvent(id, m_ctrlSteerWheel.delayData.currentPos);
    detachIndexID(id);
}

void InputConvertGame::onWheelTimer(int key) {
    if (m_ctrlSteerWheel.delayData.queuePos.empty()) {
        return;
    }
    int id = getTouchID(key);
    m_ctrlSteerWheel.delayData.currentPos = m_ctrlSteerWheel.delayData.queuePos.dequeue();
    sendTouchMoveEvent(id, m_ctrlSteerWheel.delayData.currentPos);

    if (m_ctrlSteerWheel.delayData.queuePos.empty() && m_ctrlSteerWheel.delayData.pressedNum == 0) {
        QMutexLocker locker(&m_ctrlSteerWheel.steerMutex);
        if (!m_ctrlSteerWheel.wheeling) {
            qDebug() << "wheel concurrent: no wheeling, return";
            return;
        }
        sendTouchUpEvent(id, m_ctrlSteerWheel.delayData.currentPos);
        detachIndexID(id);
        m_ctrlSteerWheel.wheeling = false;
        return;
    }

    if (!m_ctrlSteerWheel.delayData.queuePos.empty()) {
        m_ctrlSteerWheel.delayData.timer->start(m_ctrlSteerWheel.delayData.queueTimer.dequeue());
    }
}

void InputConvertGame::processSteerWheel(const KeyMap::KeyMapNode &node, const QKeyEvent *from)
{
    int key = from->key();
    // 是否按下
    bool keyPress = from->type() == QEvent::KeyPress;
    bool boostKey = key == node.data.steerWheel.boost.key;
    if (keyPress && key == node.data.steerWheel.switchKey.key) {
        m_ctrlSteerWheel.clickMode = !m_ctrlSteerWheel.clickMode;
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
    } else if (key == node.data.steerWheel.left.key){ // left
        m_ctrlSteerWheel.pressedLeft = keyPress;
        m_ctrlSteerWheel.clickPos = node.data.steerWheel.left.pos;
    }
    if (m_ctrlSteerWheel.clickMode) {
        if (QEvent::KeyPress == from->type()) {
            int id = attachTouchID(from->key());
            sendTouchDownEvent(id, m_ctrlSteerWheel.clickPos);
        } else if (QEvent::KeyRelease == from->type()) {
            int id = getTouchID(from->key());
            sendTouchUpEvent(id, m_ctrlSteerWheel.clickPos);
            detachIndexID(id);
        }
        return;
    }
    // calc offset and wheelPressed number
    QPointF offset(0.0, 0.0);
    int pressedNum = 0;
    double range = 0.01;
    qreal shakeNumber = QRandomGenerator::global()->bounded(range * 2) - range;

    if (m_ctrlSteerWheel.pressedUp) {
        ++pressedNum;
        offset.ry() -= (node.data.steerWheel.up.extendOffset + shakeNumber);
    }
    if (m_ctrlSteerWheel.pressedRight) {
        ++pressedNum;
        offset.rx() += (node.data.steerWheel.right.extendOffset + shakeNumber);
    }
    if (m_ctrlSteerWheel.pressedDown) {
        ++pressedNum;
        offset.ry() += (node.data.steerWheel.down.extendOffset + shakeNumber);
    }
    if (m_ctrlSteerWheel.pressedLeft) {
        ++pressedNum;
        offset.rx() -= (node.data.steerWheel.left.extendOffset + shakeNumber);
    }
    m_ctrlSteerWheel.delayData.pressedNum = pressedNum;
    if (pressedNum > 1) {
        offset /= 1.5;
    }
    if (boostKey) {
        if(!keyPress) return;
        if (!m_ctrlSteerWheel.pressedUp) {
            m_ctrlSteerWheel.pressedBoost = true;
            qDebug() << "boost key pressed, but no up key pressed";
            return;
        };
        m_ctrlSteerWheel.pressedBoost = !m_ctrlSteerWheel.pressedBoost;
    }else if (key == node.data.steerWheel.up.key) {
        if (keyPress) {
            if (from->modifiers() & Qt::ShiftModifier) {
                m_ctrlSteerWheel.pressedBoost = true; // 按下shift键持续奔跑
            }
        } else if (pressedNum>0){
            m_ctrlSteerWheel.pressedBoost = false; // 取消持续奔跑
        }
    }

    if (m_ctrlSteerWheel.pressedBoost) {
        if (m_ctrlSteerWheel.pressedUp) {
            offset.ry() -= node.data.steerWheel.boost.extendOffset;
        }
        if (m_ctrlSteerWheel.pressedRight) {
            offset.rx() += node.data.steerWheel.boost.extendOffset;
        }
        if (m_ctrlSteerWheel.pressedLeft) {
            offset.rx() -= node.data.steerWheel.boost.extendOffset;
        }
    }

    // last key release and timer no active, active timer to detouch
    if (pressedNum == 0) {
        if (m_ctrlSteerWheel.delayData.timer->isActive()) {
            m_ctrlSteerWheel.delayData.timer->stop();
            m_ctrlSteerWheel.delayData.queueTimer.clear();
            m_ctrlSteerWheel.delayData.queuePos.clear();
        }
        //取消持续奔跑
//        qDebug()<< "boost key released, cancel boost"<< m_ctrlSteerWheel.pressedBoost;
        if (m_ctrlSteerWheel.pressedBoost) {
            m_ctrlSteerWheel.pressedBoost = false;
            getDelayQueue(
                    m_ctrlSteerWheel.delayData.currentPos,
                    m_keyPosMap[Qt::Key_sterling],
                    0.02f,
                    0.002f,
                    4,
                    4,
                    m_ctrlSteerWheel.delayData.queuePos,
                    m_ctrlSteerWheel.delayData.queueTimer);
            m_ctrlSteerWheel.delayData.timer->start();
            return;
        }
        QMutexLocker locker(&m_ctrlSteerWheel.steerMutex);
        if (!m_ctrlSteerWheel.wheeling) {
            qDebug() << "wheel concurrent: no wheeling";
            return;
        }
        int touchKeyID = getTouchID(m_ctrlSteerWheel.touchKey);
        sendTouchUpEvent(touchKeyID, m_ctrlSteerWheel.delayData.currentPos);
        detachIndexID(touchKeyID);
        m_ctrlSteerWheel.wheeling = false;
        return;
    }

    // process steer wheel key event
    m_ctrlSteerWheel.delayData.timer->stop();
    m_ctrlSteerWheel.delayData.queueTimer.clear();
    m_ctrlSteerWheel.delayData.queuePos.clear();
    // 只有一个按键按下，且没有正在操作方向盘
    // 如果是boost按键，则不处理
    // 如果是第一个按键，则开始操作方向盘
    if (pressedNum == 1 && keyPress && !m_ctrlSteerWheel.wheeling && !boostKey) {
//        qDebug() << "first press";
        QMutexLocker locker(&m_ctrlSteerWheel.steerMutex);
        if (m_ctrlSteerWheel.wheeling) {
            qDebug() << "wheel concurrent: already wheeling";
            return;
        }
        m_ctrlSteerWheel.wheeling = true;
        int id = attachTouchID(m_ctrlSteerWheel.touchKey);
        const QPointF centerPos = shakePos(node.data.steerWheel.centerPos, 0.025, 0.025);
        m_keyPosMap[Qt::Key_sterling] = centerPos;
        sendTouchDownEvent(id, centerPos);
        getDelayQueue(centerPos,
                      centerPos + offset,
                      0.02f,
                      0.002f,
                      4,
                      4,
                      m_ctrlSteerWheel.delayData.queuePos,
                      m_ctrlSteerWheel.delayData.queueTimer);

    } else {
//        qDebug() << "change press";
        getDelayQueue(
                m_ctrlSteerWheel.delayData.currentPos,
                m_keyPosMap[Qt::Key_sterling] + offset,
                0.02f,
                0.002f,
                4,
                4,
                m_ctrlSteerWheel.delayData.queuePos,
                m_ctrlSteerWheel.delayData.queueTimer);
    }
    m_ctrlSteerWheel.delayData.timer->start();
}
// -------- key event --------

void InputConvertGame::processKeyClick(bool clickTwice, const KeyMap::KeyMapNode &node, const QKeyEvent *from)
{
    if (QEvent::KeyPress == from->type()) {
        if (from->key() == Qt::Key_F2) {
            QList<int> stuckArray;
            int stuckNumber = 0;
            for (int i = 0; i < MULTI_TOUCH_MAX_NUM; i++) {
                if (0 != m_multiTouchID[i]) {
                    stuckNumber++;
                    stuckArray.append(m_multiTouchID[i]);
                }
            }
            qDebug()<<"stuckNumber:"<<stuckNumber <<"exceptionButton:"<< stuckArray;
        }

        QPointF processedPos = shakePos(node.data.click.keyNode.pos, 0.005, 0.005);
        if (from->key() == Qt::Key_F) {
            if (from->modifiers() & Qt::ShiftModifier) {
                processedPos.setX(processedPos.x() - 0.1);
            }
            if (from->modifiers() & Qt::AltModifier) {
                processedPos.setX(processedPos.x() + 0.05);
            }
        }
        m_keyPosMap[from->key()] = processedPos;
        int id = attachTouchID(from->key());
        sendTouchDownEvent(id, processedPos);
        if (clickTwice) {
            int touchId = getTouchID(from->key());
            sendTouchUpEvent(touchId, processedPos);
            detachIndexID(touchId);
        }
    } else if (QEvent::KeyRelease == from->type()) {
        if (clickTwice) {
            int id = attachTouchID(from->key());
            sendTouchDownEvent(id, m_keyPosMap[from->key()]);
        }
        int id = getTouchID(from->key());
        sendTouchUpEvent(id, m_keyPosMap[from->key()]);
        detachIndexID(id);
    }
}
void InputConvertGame::switchMouse(const KeyMap::KeyMapNode &node, const QKeyEvent *from)
{
    bool forceSwitchOn = node.forceSwitchOn;
    bool forceSwitchOff = node.forceSwitchOff;
    bool switchMap = node.switchMap;
    if (!switchMap || QEvent::KeyRelease != from->type())
        return;
    switchMouse(node, forceSwitchOn, forceSwitchOff);
}
void InputConvertGame::switchMouse(const KeyMap::KeyMapNode &node, bool forceSwitchOn, bool forceSwitchOff)
{
    bool lastPointerMode = m_pointerMode;
    // 只显示鼠标不关闭鼠标
    if (forceSwitchOn) {
        m_pointerMode = true;
    }else if (forceSwitchOff) {
        m_pointerMode = false;
    }else {
        m_pointerMode = !m_pointerMode;
    }

    setMousePos(m_pointerMode, node);
    if (lastPointerMode != m_pointerMode) {
        hideMouseCursor(!m_pointerMode);
    }
}

void
InputConvertGame::processKeyClickMulti(const KeyMap::DelayClickNode *nodes, const int count, const double pressTime,
                                       const QKeyEvent *from)
{
    if (QEvent::KeyPress != from->type()) {
        return;
    }

    int key = from->key();
    int delay = 0;
    QPointF clickPos;

    for (int i = 0; i < count; i++) {
        if (nodes[i].delay > 0) {
            delay += QRandomGenerator::global()->bounded(nodes[i].delay, nodes[i].delay + 10);
        }
        clickPos = shakePos(nodes[i].pos, 0.01, 0.01);
        QTimer::singleShot(delay, this, [this, key, clickPos]() {
            int id = attachTouchID(key);
            sendTouchDownEvent(id, clickPos);
        });
//        qDebug() << "pressTime:" << pressTime;
        if (pressTime > 0) {
            delay += (int)pressTime;
        }
        if(nodes[i].pressTime > 0){
            qDebug() << "pressTime:" << nodes[i].pressTime;
            delay += nodes[i].pressTime;
        }
        // Don't up it too fast
        delay += QRandomGenerator::global()->bounded(10, 20);

        QTimer::singleShot(delay, this, [this, key, clickPos]() {
            int id = getTouchID(key);
            sendTouchUpEvent(id, clickPos);
            detachIndexID(id);
        });
    }
}

void InputConvertGame::onDragTimer()
{
    if (m_dragDelayData.queuePos.empty()) {
        return;
    }
    int id = getTouchID(m_dragDelayData.pressKey);
    m_dragDelayData.currentPos = m_dragDelayData.queuePos.dequeue();
    sendTouchMoveEvent(id, m_dragDelayData.currentPos);

    if (m_dragDelayData.queuePos.empty()&&m_dragDelayData.allowUp) {
        delete m_dragDelayData.timer;
        m_dragDelayData.timer = nullptr;
        QTimer::singleShot(m_dragDelayData.dragDelayUpTime, this, [this, id]() {
            sendTouchUpEvent(id, m_dragDelayData.currentPos);
            detachIndexID(id);
            m_dragDelayData.currentPos = QPointF();
            m_dragDelayData.pressKey = 0;
            m_dragDelayData.dragDelayUpTime = 0;
        });
        return;
    }

    if (!m_dragDelayData.queuePos.empty()) {
        m_dragDelayData.timer->start(m_dragDelayData.queueTimer.dequeue());
    }
}

void InputConvertGame::onWheelScrollTimer()
{
    if (m_wheelDelayData.queuePos.empty()) {
        return;
    }
    int id = getTouchID(m_wheelDelayData.pressKey);
    m_wheelDelayData.currentPos = m_wheelDelayData.queuePos.dequeue();
    sendTouchMoveEvent(id, m_wheelDelayData.currentPos);

    if (m_wheelDelayData.queuePos.empty()) {
        m_wheelDelayData.upTimer->stop();
        m_wheelDelayData.upTimer->start(m_wheelDelayData.wheelDelayUpTime);
        return;
    } else {
        m_wheelDelayData.timer->start(m_wheelDelayData.queueTimer.dequeue());
    }
}

void InputConvertGame::onWheelUpTimer(){
    qDebug() << "WheelUp";
    int id = getTouchID(m_wheelDelayData.pressKey);
    delete m_wheelDelayData.timer;
    m_wheelDelayData.timer = nullptr;
    sendTouchUpEvent(id, m_wheelDelayData.currentPos);
    detachIndexID(id);
    m_wheelDelayData.currentPos = QPointF();
    m_wheelDelayData.pressKey = 0;
    m_wheelDelayData.wheeling = false;
}

void InputConvertGame::processKeyDrag(const QPointF &startPos, QPointF endPos, const QKeyEvent *from)
{
    if (QEvent::KeyPress == from->type()) {
        // stop last
        dragStop();
        // start this
        int id = attachTouchID(from->key());
        sendTouchDownEvent(id, startPos);

        m_dragDelayData.timer = new QTimer(this);
        m_dragDelayData.timer->setSingleShot(true);
        connect(m_dragDelayData.timer, &QTimer::timeout, this, &InputConvertGame::onDragTimer);
        m_dragDelayData.pressKey = from->key();
        m_dragDelayData.currentPos = startPos;
        m_dragDelayData.queuePos.clear();
        m_dragDelayData.queueTimer.clear();
        getDelayQueue(startPos, endPos, 0.01f, 0.002f, 3, 5, m_dragDelayData.queuePos, m_dragDelayData.queueTimer);
        m_dragDelayData.timer->start();
    }
}

void InputConvertGame::dragStop() {
    if (m_dragDelayData.timer && m_dragDelayData.timer->isActive()) {
        m_dragDelayData.timer->stop();
        delete m_dragDelayData.timer;
        m_dragDelayData.timer = nullptr;
        m_dragDelayData.queuePos.clear();
        m_dragDelayData.queueTimer.clear();

        int id = getTouchID(m_dragDelayData.pressKey);
        sendTouchUpEvent(id, m_dragDelayData.currentPos);
        detachIndexID(id);
        m_dragDelayData.currentPos = QPointF();
        m_dragDelayData.pressKey = 0;
    }
}

void InputConvertGame::processAndroidKey(AndroidKeycode androidKey, const QEvent *from)
{
    if (AKEYCODE_UNKNOWN == androidKey) {
        return;
    }
    AndroidKeyeventAction action;
    switch (from->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    case QEvent::KeyPress:
        action = AKEY_EVENT_ACTION_DOWN;
        break;
    case QEvent::MouseButtonRelease:
    case QEvent::KeyRelease:
        action = AKEY_EVENT_ACTION_UP;
        break;
    default:
        return;
    }
    sendKeyEvent(action, androidKey);
}

// -------- mouse event --------

bool InputConvertGame::processMouseClick(const QMouseEvent *from) {
    KeyMap::KeyMapNode node = m_keyMap.getKeyMapNodeMouse(from->button());
    if (KeyMap::KMT_INVALID == node.type) {
        return false;
    }
    KeyMap::KeyMapType type = node.type;
    if (node.type == KeyMap::KMT_DUAL_MODE) {
        if (!m_pointerMode) {
            type = node.data.dualMode.accurateType;
        } else {
            type = node.data.dualMode.mouseType;
        }
        if (type == KeyMap::KMT_INVALID) {
            return false;
        }
    }
    switch (type) {
    case KeyMap::KMT_CLICK:
        if (m_pointerMode) {
            return false;
        }
        if (QEvent::MouseButtonPress == from->type() || QEvent::MouseButtonDblClick == from->type()) {
            int id = attachTouchID(from->button());
            m_keyPosMap[from->button()] = shakePos(node.data.click.keyNode.pos, 0.01, 0.01);
            sendTouchDownEvent(id, m_keyPosMap[from->button()]);

            return true;
        }
        if (QEvent::MouseButtonRelease == from->type()) {
            int id = getTouchID(from->button());
            sendTouchUpEvent(id, m_keyPosMap[from->button()]);
            detachIndexID(id);
            if (node.freshMouseMove) {
                stopMouseMoveTimer();
                mouseMoveStopTouch();
            }
            return true;
        }
        return true;
    case KeyMap::KMT_ANDROID_KEY:
        processAndroidKey(node.data.androidKey.keyNode.androidKey, from);
        return true;
    case KeyMap::KMT_CLICK_MULTI: {
        if (QEvent::MouseButtonPress != from->type()) {
            return true;
        }
        int button = from->button();
        int delay = 0;
        QPointF clickPos;
        KeyMap::DelayClickNode *nodes = node.data.clickMulti.keyNode.delayClickNodes;
        int count = node.data.clickMulti.keyNode.delayClickNodesCount;
        for (int i = 0; i < count; i++) {
            delay += QRandomGenerator::global()->bounded(nodes[i].delay, nodes[i].delay + 10);
            delay += nodes[i].delay;
            clickPos = shakePos(nodes[i].pos, 0.01, 0.01);
            QTimer::singleShot(delay, this, [this, button, clickPos]() {
                int id = attachTouchID(button);
                sendTouchDownEvent(id, clickPos);
            });

            // Don't up it too fast
            delay += QRandomGenerator::global()->bounded(20, 30);

            QTimer::singleShot(delay, this, [this, button, clickPos]() {
                int id = getTouchID(button);
                sendTouchUpEvent(id, clickPos);
                detachIndexID(id);
            });
        }
        return true;
    }
    case KeyMap::KMT_MOBA_WHEEL: {
        return processMobaWheel(from);
    }
    default:
        return false;
    }
}

bool InputConvertGame::processCustomMouseClick(const QMouseEvent *from, const QSize &frameSize, const QSize &showSize) {
    if (QEvent::MouseButtonPress == from->type() || QEvent::MouseButtonDblClick == from->type()) {
//        qDebug() << "customMouseClick "<<from->button();
        if (from->button()==Qt::LeftButton) {
            if (from->modifiers() & Qt::ControlModifier) {
                QPointF pos = from->localPos();
                int key = Qt::ControlModifier + Qt::LeftButton;
                // convert pos
                pos.setX(pos.x() / showSize.width());
                pos.setY(pos.y() / showSize.height());
                int delay = 0;
                QPointF clickPos;
                for (int i = 0; i < 2; i++) {
                    clickPos = shakePos(pos, 0.01, 0.01);
                    QTimer::singleShot(delay, this, [this,key, clickPos]() {
                        int id = attachTouchID(key);
                        sendTouchDownEvent(id, clickPos);
                    });
                    // Don't up it too fast
                    delay += QRandomGenerator::global()->bounded(5, 10);
                    QTimer::singleShot(delay, this, [this, key, clickPos]() {
                        int id = getTouchID(key);
                        sendTouchUpEvent(id, clickPos);
                        detachTouchID(key);
                    });
                    delay += QRandomGenerator::global()->bounded(5, 10);
                }
                return true;
            } else {
                int id = attachTouchID(from->button());
                // pos
                QPointF pos = from->localPos();
                // convert pos
                pos.setX(pos.x() / showSize.width());
                pos.setY(pos.y() / showSize.height());
                m_keyPosMap[from->button()] = pos;
                sendTouchDownEvent(id, m_keyPosMap[from->button()]);
                return true;
            }
        }else if (from->button()==Qt::RightButton) {
            if (from->modifiers() & Qt::ControlModifier) {
                dragStop();
                // pos
                QPointF pos = from->localPos();
                // convert pos
                pos.setX(pos.x() / showSize.width());
                pos.setY(pos.y() / showSize.height());
                // 执行自定义操作（如多选、缩放等）
                // stop last
                if (m_dragDelayData.timer && m_dragDelayData.timer->isActive()) {
                    return false;
                }
                // start this
                int id = attachTouchID(Qt::Key_Control+Qt::RightButton);
                sendTouchDownEvent(id, pos);

                QPointF endPos(0, pos.y());
                if (pos.x() <= 0.5) {
                    endPos.setX(0.01);
                    if (pos.y() >= 0.7) {
                        endPos.setY(QRandomGenerator::global()->generateDouble() * 0.1 + 0.6);
                    }
                } else {
                    endPos.setX(0.99);
                }
                m_dragDelayData.timer = new QTimer(this);
                m_dragDelayData.timer->setSingleShot(true);
                connect(m_dragDelayData.timer, &QTimer::timeout, this, &InputConvertGame::onDragTimer);
                m_dragDelayData.pressKey = Qt::Key_Control+Qt::RightButton;
                m_dragDelayData.currentPos = pos;
                m_dragDelayData.queuePos.clear();
                m_dragDelayData.queueTimer.clear();
                m_dragDelayData.dragDelayUpTime = 0;
                getDelayQueue(pos, endPos, 0.0125f, 0.001f, 3, 5, m_dragDelayData.queuePos, m_dragDelayData.queueTimer);
                m_dragDelayData.timer->start();
            } else {
                qDebug() << "right button click";

            }
        }
    }
    if (QEvent::MouseMove == from->type()) {
        if (!(from->buttons() & Qt::LeftButton)) {
            return false;
        }
        int id = getTouchID(Qt::LeftButton);
        // pos
        QPointF pos = from->localPos();
        // convert pos
        pos.setX(pos.x() / showSize.width());
        pos.setY(pos.y() / showSize.height());
        m_keyPosMap[Qt::LeftButton] = pos;
        sendTouchMoveEvent(id, m_keyPosMap[Qt::LeftButton]);
        return true;
    }
    if (QEvent::MouseButtonRelease == from->type()) {
        if (from->button()==Qt::LeftButton) {
            if (from->modifiers() & Qt::ControlModifier) {
                return true;
            }
            int id = getTouchID(from->button());
            sendTouchUpEvent(id, m_keyPosMap[from->button()]);
            detachIndexID(id);
        }

        return true;
    }
    return true;
}

bool InputConvertGame::processMouseMove(const QMouseEvent *from)
{
    if (QEvent::MouseMove != from->type()) {
        return false;
    }

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
    QPointF distance_raw{from->localPos() - lastPos};

    if (qAbs(distance_raw.x()) > 100) {
        qDebug() << "mouse move distance_raw:" << distance_raw;
    }

    QPointF currentConvertPos(
            m_ctrlMouseMove.lastConvertPos.x() + distance_raw.x() / (speedRatio.x() * m_frameSize.width()),
            m_ctrlMouseMove.lastConvertPos.y() + distance_raw.y() / (speedRatio.y() * m_frameSize.height())
    );
    if (!m_ctrlMouseMove.needResetTouch) {
        bool boundary = false;
        if (currentConvertPos.x() <= 0) {
            currentConvertPos.setX(0);
            boundary = true;
        } else if (currentConvertPos.x()>= 1) {
            currentConvertPos.setX(1);
            boundary = true;
        }
        if (currentConvertPos.y() <= 0) {
            currentConvertPos.setY(0);
            boundary = true;
        }else if (currentConvertPos.y() >= 1) {
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
        if (currentConvertPos.x()>= 1) currentConvertPos.setX(1);
        else if (currentConvertPos.x() <= 0) currentConvertPos.setX(0);
        if (currentConvertPos.y() >= 1) currentConvertPos.setY(1);
        else if (currentConvertPos.y() <= 0) currentConvertPos.setY(0);

        if (currentConvertPos.x()==0||currentConvertPos.x()==1) {
            qDebug() << "out of pad boundary pos:" << currentConvertPos;
        }

        mouseMove(currentConvertPos);
        mouseMoveStopTouch();
        mouseMoveStartTouch(*new QPointF);
        return true;
    }
    mouseMove(currentConvertPos);
    return true;
}

bool InputConvertGame::mouseMove(QPointF &currentConvertPos) {
    if (m_ctrlMouseMove.touching) {
        if (!m_ctrlMouseMove.mouseMutex.try_lock()) {
            qDebug() << "abandon multi thread:move";
            return true;
        }
        m_ctrlMouseMove.lastConvertPos = currentConvertPos;
        sendTouchMoveEvent(m_ctrlMouseMove.focusTouchID, currentConvertPos);
        m_ctrlMouseMove.mouseMutex.unlock();
    }
    return true;
}

bool InputConvertGame::checkBoundary(const QPointF &currentConvertPos) const {
    if (!m_ctrlMouseMove.needResetTouch) {
        return false;
    }
    return currentConvertPos.x() > 0.9 || currentConvertPos.x() < 0.3 || currentConvertPos.y() < 0.1
       || currentConvertPos.y() > 0.9;
}

void InputConvertGame::resetMouseMove(const QPointF pos) {
    int id = getTouchID(Qt::ExtraButton24);
    if (m_ctrlMouseMove.touching) {
        detachIndexID(id);
        sendTouchUpEvent(id, m_ctrlMouseMove.lastConvertPos);
        m_ctrlMouseMove.touching = false;
    }
    mouseMoveStartTouch(pos);
}

bool InputConvertGame::checkCursorPos(const QMouseEvent *from)
{
    QPoint pos = from->pos();

    bool outOfCenter = false;
    if (pos.x() > m_ctrlMouseMove.centerPos.x() + CURSOR_POS_CHECK_WIDTH) {
        outOfCenter = true;
    } else if (pos.x() < m_ctrlMouseMove.centerPos.x() - CURSOR_POS_CHECK_WIDTH) {
        outOfCenter = true;
    } else if (pos.y() > m_ctrlMouseMove.centerPos.y() + CURSOR_POS_CHECK_HEIGHT) {
        outOfCenter = true;
    } else if (pos.y() < m_ctrlMouseMove.centerPos.y() - CURSOR_POS_CHECK_HEIGHT) {
        outOfCenter = true;
    }
    if (!outOfCenter) {
        return false;
    }
    bool outOfBoundary = false;
    if (pos.x() < CURSOR_POS_CHECK_WIDTH) {
        outOfBoundary = true;
    } else if (pos.x() > m_showSize.width() - CURSOR_POS_CHECK_WIDTH) {
        outOfBoundary = true;
    } else if (pos.y() < CURSOR_POS_CHECK_HEIGHT) {
        outOfBoundary = true;
    } else if (pos.y() > m_showSize.height() - CURSOR_POS_CHECK_HEIGHT) {
        outOfBoundary = true;
    }
    if (!outOfBoundary) {
        m_ctrlMouseMove.outOfBoundary = false;
        return false;
    }
    if (!m_ctrlMouseMove.outOfBoundary) {
        m_ctrlMouseMove.cursorPos = m_ctrlMouseMove.lastPos;
//        qDebug() << "first out of boundary, cursorPos:" << m_ctrlMouseMove.cursorPos;
    }
    m_ctrlMouseMove.outOfBoundary = true;
    moveCursorTo(from, m_ctrlMouseMove.centerPos);
    m_ctrlMouseMove.lastPos = m_ctrlMouseMove.centerPos;
    return true;
}

void InputConvertGame::moveCursorTo(const QMouseEvent *from, const QPoint &localPosPixel)
{
    QPoint posOffset = from->pos() - localPosPixel;
    QPoint globalPos = from->globalPos();
    globalPos -= posOffset;
    QCursor::setPos(globalPos);
}

bool InputConvertGame::mouseMoveStartTouch(const QPointF pos)
{
    if (!m_ctrlMouseMove.touching) {
        if (!m_ctrlMouseMove.mouseMutex.try_lock()) {
            qDebug() << "abandon multi thread:touch";
            return true;
        }
//        qDebug() << "mouse touch down";
        QPointF mouseMoveStartPos;
        if (pos.isNull()) {
            mouseMoveStartPos = shakePos(m_keyMap.getMouseMoveMap().data.mouseMove.startPos, 0.05, 0.05);
        } else {
            mouseMoveStartPos = pos;
        }
//        qDebug() << "mouse move start pos:" << mouseMoveStartPos;
        int id = attachTouchID(Qt::ExtraButton24);
        m_ctrlMouseMove.focusTouchID = id;
        m_ctrlMouseMove.startPos = mouseMoveStartPos;
        m_ctrlMouseMove.lastConvertPos = mouseMoveStartPos;
        sendTouchDownEvent(id, mouseMoveStartPos);
        m_ctrlMouseMove.touching = true;
        m_ctrlMouseMove.mouseMutex.unlock();
        return true;
    }
    return false;
}

QPointF InputConvertGame::shakePos(QPointF startPos, double offsetX, double offsetY)
{
    int pixelX = static_cast<int>(startPos.x() * m_frameSize.width());
    int pixelY = static_cast<int>(startPos.y() * m_frameSize.height());
    int maxHShakePixels = static_cast<int>(offsetX * m_frameSize.width());
    int maxVShakePixels = static_cast<int>(offsetY * m_frameSize.height());
    int shakeX = QRandomGenerator::global()->bounded(-maxHShakePixels, maxHShakePixels + 1);
    int shakeY = QRandomGenerator::global()->bounded(-maxVShakePixels, maxVShakePixels + 1);
    int newPixelX = (pixelX + shakeX) % m_frameSize.width();
    int newPixelY = (pixelY + shakeY) % m_frameSize.height();
    return {static_cast<double>(newPixelX) / m_frameSize.width(), static_cast<double>(newPixelY) / m_frameSize.height()};
}

void InputConvertGame::mouseMoveStopTouch()
{
    m_ctrlMouseMove.mouseMutex.lock();
    if (!m_ctrlMouseMove.touching) {
        qDebug() << "mouse move stop concurrent: no touching";
        m_ctrlMouseMove.mouseMutex.unlock();
        return;
    }
    m_ctrlMouseMove.touching = false;
    int id = getTouchID(Qt::ExtraButton24);
    m_multiTouchID[id] = Qt::Key_unknown;
    QPointF touchUpPos = m_ctrlMouseMove.lastConvertPos;
    QTimer::singleShot(QRandomGenerator::global()->bounded(100,200), this, [this, id,touchUpPos]() {
        detachIndexID(id);
        sendTouchUpEvent(id, touchUpPos);
    });
    m_ctrlMouseMove.mouseMutex.unlock();
}

void InputConvertGame::startMouseMoveTimer()
{
    m_ctrlMouseMove.resetMoveTimer.stop();
    if (m_ctrlMouseMove.needResetTouch) {
        m_ctrlMouseMove.resetMoveTimer.start(m_ctrlMouseMove.resetMoveDelay);
    }
}

void InputConvertGame::stopMouseMoveTimer()
{
    if (0 != m_ctrlMouseMove.timer) {
        killTimer(m_ctrlMouseMove.timer);
        m_ctrlMouseMove.timer = 0;
    }
    if (m_ctrlMouseMove.resetMoveTimer.isActive()) {
        m_ctrlMouseMove.resetMoveTimer.stop();
    }
}

bool InputConvertGame::switchGameMap()
{
    m_gameMap = !m_gameMap;
    qInfo() << QString("current keymap mode: %1").arg(m_gameMap ? "custom" : "normal");
    if (!m_keyMap.isValidMouseMoveMap()) {
        return m_gameMap;
    }
#ifdef QT_NO_DEBUG
    // grab cursor and set cursor only mouse move map
    emit grabCursor(m_gameMap);
#endif
    hideMouseCursor(m_gameMap);

    if (!m_gameMap) {
        stopMouseMoveTimer();
        mouseMoveStopTouch();
    } else {
        QPoint globalPos = QCursor::pos(); // 获取屏幕全局坐标
        QPoint windowPos = QApplication::activeWindow()->mapFromGlobal(globalPos); // 转换为当前窗口坐标
        m_ctrlMouseMove.lastPos = windowPos;
        qDebug() << "switchGameMap lastPos:" << m_ctrlMouseMove.lastPos;
    }
    return m_gameMap;
}

void InputConvertGame::hideMouseCursor(bool hide)
{
    if (hide) {
#ifdef QT_NO_DEBUG
        QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
#else
        QGuiApplication::setOverrideCursor(QCursor(Qt::CrossCursor));
#endif
        //防止页面切换时移动视角失效
//        QTimer::singleShot(200, this, [this]() { mouseMoveStopTouch(); });
#ifdef Q_OS_WIN32
        QWidget *activeWindow = QApplication::activeWindow();
        RECT rect; // 定义一个 RECT 结构体，表示锁定的范围
        QPointF topLeft = activeWindow->mapToGlobal(activeWindow->rect().topLeft());
        rect.top = topLeft.y();
        rect.left = topLeft.x();
        rect.bottom = topLeft.y() + m_showSize.height();
        rect.right = topLeft.x() + m_showSize.width();
        ClipCursor(&rect);
        qDebug() << "lock mouse" << rect.top << rect.left << rect.bottom << rect.right << m_showSize;
#endif
    } else {
#ifdef Q_OS_WIN32
        ClipCursor(nullptr); // 解除锁定
#endif
        QGuiApplication::restoreOverrideCursor();
    }
    emit mouseCursorHided(hide);
}
void InputConvertGame::setMousePos(bool b, const KeyMap::KeyMapNode &node)
{
    stopMouseMoveTimer();
    mouseMoveStopTouch();
    if (m_pointerMode) {
        QWidget *activeWindow = QApplication::activeWindow();

        if (node.focusOn) {
            const QPoint &point = { qIntCast(m_showSize.width() * node.focusPos.x()), qIntCast(m_showSize.height() * node.focusPos.y()) };
            if (activeWindow->isFullScreen()) {
                QCursor::setPos(point);
            } else {
                QCursor::setPos(activeWindow->mapToGlobal(point));
            }
        } else {
            if (activeWindow->isFullScreen()) {
                QCursor::setPos(m_showSize.width() / 2, m_showSize.height() / 2);
            } else {
                QPoint center = activeWindow->mapToGlobal(activeWindow->rect().center());
                QCursor::setPos(center);
            }
        }
    } else {
        QPoint globalPos = QCursor::pos(); // 获取屏幕全局坐标
        QPoint windowPos = QApplication::activeWindow()->mapFromGlobal(globalPos); // 转换为当前窗口坐标
        m_ctrlMouseMove.lastPos = windowPos;
        qDebug() << "setMousePos lastPos:" << m_ctrlMouseMove.lastPos;
    }
}

void InputConvertGame::timerEvent(QTimerEvent *event)
{
    if (m_ctrlMouseMove.timer == event->timerId()) {
        stopMouseMoveTimer();
        mouseMoveStopTouch();
        qDebug()<< "mouse move timer stop";
    }

}
void InputConvertGame::processRotaryTable(const KeyMap::KeyMapNode &node, const QKeyEvent *const from)
{
    QPointF pos = node.data.rotaryTable.keyNode.pos;
    QPointF speedRatio = node.data.rotaryTable.speedRatio;
    if (m_keyMap.isValidMouseMoveMap()) {
        if (QEvent::KeyPress == from->type()) {
            m_ctrlMouseMove.needResetTouch = false;
            m_currentSpeedRatio = speedRatio;
            stopMouseMoveTimer();
            mouseMoveStopTouch();
//            qDebug()<< "RotaryTable start touch";
            mouseMoveStartTouch(pos);
        } else {
            m_currentSpeedRatio = m_keyMap.getMouseMoveMap().data.mouseMove.speedRatio;
            m_ctrlMouseMove.needResetTouch = true;
            stopMouseMoveTimer();
            mouseMoveStopTouch();
        }
        return;
    }
}
void InputConvertGame::processDualMode(KeyMap::KeyMapNode &node, const QKeyEvent *from)
{
    if (m_pointerMode) {
        node.type = node.data.dualMode.mouseType;
    } else {
        node.type = node.data.dualMode.accurateType;
    }
    processType(node, from);
}
void InputConvertGame::processPressRelease(const KeyMap::KeyMapNode &node, const QKeyEvent *from)
{
    int key = from->key();
    if (QEvent::KeyPress == from->type()) {
        if (node.switchMap) {
            switchMouse(node, true, false);
        }
        QPointF processedPos = shakePos(node.data.pressRelease.pressPos, 0.01, 0.01);
        int id = attachTouchID(key);
        sendTouchDownEvent(id, processedPos);
        int delay = QRandomGenerator::global()->bounded(20, 30);
        QTimer::singleShot(delay, this, [this, key, processedPos]() {
            int id = getTouchID(key);
            sendTouchUpEvent(id, processedPos);
            detachIndexID(id);
        });
    } else if (QEvent::KeyRelease == from->type()) {
        QPointF processedPos = shakePos(node.data.pressRelease.releasePos, 0.01, 0.01);
        int id = attachTouchID(key);
        sendTouchDownEvent(id, processedPos);
        int delay = QRandomGenerator::global()->bounded(20, 30);
        QTimer::singleShot(delay, this, [this, key, processedPos]() {
            int id = getTouchID(key);
            sendTouchUpEvent(id, processedPos);
            detachIndexID(id);
        });
        if (node.switchMap) {
            switchMouse(node, false, true);
        }
    }
}

bool InputConvertGame::processMobaWheel(const QMouseEvent *from) {
    KeyMap::KeyMapNode node = m_keyMap.getKeyMapNodeMouse(from->button());
    m_ctrlSteerWheel.mobaWheel.skillOffset = node.data.mobaWheel.skillOffset;
    m_ctrlSteerWheel.touchKey = from->button();
    m_ctrlSteerWheel.mobaWheel.speedRatio = node.data.mobaWheel.speedRatio;
    m_ctrlSteerWheel.mobaWheel.centerPos = node.data.mobaWheel.centerPos;
    m_ctrlSteerWheel.mobaWheel.localPos = from->localPos();
    if (QEvent::MouseButtonPress == from->type()) {
        QPointF wheelPos;
        QPointF startPos = wheelPos;
        m_ctrlSteerWheel.mobaWheel.wheelPressed = true;
        qDebug() << " timer:" << m_ctrlSteerWheel.mobaWheel.stopTimer->isActive();
        if (m_ctrlSteerWheel.mobaWheel.stopTimer->isActive()) {
            m_ctrlSteerWheel.mobaWheel.stopTimer->stop();
            wheelPos = m_ctrlSteerWheel.mobaWheel.wheelPos;
            startPos = m_ctrlSteerWheel.delayData.currentPos;
            if (m_ctrlSteerWheel.delayData.timer->isActive()) {
                m_ctrlSteerWheel.delayData.timer->stop();
                m_ctrlSteerWheel.delayData.queueTimer.clear();
                m_ctrlSteerWheel.delayData.queuePos.clear();
            }
        } else{
            wheelPos = shakePos(node.data.mobaWheel.wheelPos, 0.005, 0.005);
            startPos = wheelPos;
            m_ctrlSteerWheel.mobaWheel.wheelPos = wheelPos;
            int id = attachTouchID(from->button());
            sendTouchDownEvent(id, wheelPos);
        }

        QPointF rawPos{from->localPos().x() / m_showSize.width(), from->localPos().y() / m_showSize.height()};
        QPointF rawDistance{rawPos - node.data.mobaWheel.centerPos};
        QPointF distance{rawDistance.x() / m_ctrlSteerWheel.mobaWheel.speedRatio, rawDistance.y() / m_ctrlSteerWheel.mobaWheel.speedRatio * m_showSizeRatio};
        QPointF endPos{wheelPos + distance};

        getDelayQueue(startPos, endPos, 0.01f, 0.0005f, 1, 5,
                      m_ctrlSteerWheel.delayData.queuePos,
                      m_ctrlSteerWheel.delayData.queueTimer);
        m_ctrlSteerWheel.delayData.pressedNum++;
        m_ctrlSteerWheel.delayData.timer->start();
        return true;
    }
    if (QEvent::MouseButtonRelease == from->type()) {
        m_ctrlSteerWheel.mobaWheel.wheelPressed = false;
        QPointF rawPos{from->localPos().x() / m_showSize.width(), from->localPos().y() / m_showSize.height()};
        double distance = calcDistance(rawPos, node.data.mobaWheel.centerPos);
        double delay = distance * 3500;
            stopMobaWheel(delay);
        return true;
    }
    return true;
}

void InputConvertGame::stopMobaWheel(double delay) const {
    if (m_ctrlSteerWheel.delayData.timer->isActive()) {
        m_ctrlSteerWheel.delayData.timer->stop();
    }
    if (getTouchID(m_ctrlSteerWheel.touchKey) != -1) {
        m_ctrlSteerWheel.mobaWheel.stopTimer->start(static_cast<int>(delay));
    }
}

double InputConvertGame::calcDistance(const QPointF &point1, const QPointF &point2) {
    return std::sqrt(std::pow(point2.x() - point1.x(), 2) + std::pow(point2.y() - point1.y(), 2));
}

bool InputConvertGame::processMobaMouseMove(const QMouseEvent *from) {
    if (QEvent::MouseMove != from->type()) {
        return false;
    }
    m_ctrlSteerWheel.mobaWheel.localPos = from->localPos();
    // 处理技能
    if (m_ctrlSteerWheel.mobaWheel.skillPressed && !m_ctrlSteerWheel.mobaWheel.quickCast) {
        double skillRatio = m_ctrlSteerWheel.mobaWheel.skillRatio;
        if (m_dragDelayData.timer->isActive()) {
            m_dragDelayData.timer->stop();
            m_dragDelayData.queueTimer.clear();
            m_dragDelayData.queuePos.clear();
        }

        QPointF rawPos{from->localPos().x() / m_showSize.width(), from->localPos().y() / m_showSize.height()};
        QPointF rawDistance{rawPos - m_ctrlSteerWheel.mobaWheel.centerPos};
        QPointF distance{rawDistance.x() / skillRatio * m_ctrlSteerWheel.mobaWheel.skillOffset, rawDistance.y() / skillRatio};
        QPointF endPos{m_dragDelayData.startPos + distance};
        sendTouchMoveEvent(getTouchID(m_dragDelayData.pressKey), endPos);
        m_dragDelayData.currentPos = endPos;
        return true;
    }
    // 处理轮盘
    if (m_ctrlSteerWheel.mobaWheel.wheelPressed) {
        double speedRatio = m_ctrlSteerWheel.mobaWheel.speedRatio;
        if (m_ctrlSteerWheel.delayData.timer->isActive()) {
            m_ctrlSteerWheel.delayData.timer->stop();
            m_ctrlSteerWheel.delayData.queueTimer.clear();
            m_ctrlSteerWheel.delayData.queuePos.clear();
        }

        QPointF rawPos{from->localPos().x() / m_showSize.width(), from->localPos().y() / m_showSize.height()};
        QPointF rawDistance{rawPos - m_ctrlSteerWheel.mobaWheel.centerPos};
        QPointF distance{rawDistance.x() / speedRatio, rawDistance.y() / speedRatio * m_showSizeRatio};
        QPointF endPos{m_ctrlSteerWheel.mobaWheel.wheelPos + distance};
        m_ctrlSteerWheel.delayData.currentPos = endPos;
        sendTouchMoveEvent(getTouchID(m_ctrlSteerWheel.touchKey), endPos);
        return true;
    }
    return false;
}

void InputConvertGame::processMobaSkill(const KeyMap::KeyMapNode &node, const QKeyEvent *from) {
    bool quickCast = node.data.mobaSkill.quickCast;
    int key = from->key();
    qDebug() << "quickCast:" << quickCast;
    if (QEvent::KeyPress == from->type()) {
        if (node.data.mobaSkill.stopMove) {
            stopMobaWheel(0);
        }
        m_ctrlSteerWheel.mobaWheel.quickCast = quickCast;
        m_ctrlSteerWheel.mobaWheel.skillPressed = true;
        double speedRatio = node.data.mobaSkill.speedRatio;
        m_ctrlSteerWheel.mobaWheel.skillRatio = speedRatio;
        double skillOffset = m_ctrlSteerWheel.mobaWheel.skillOffset;
        int id = attachTouchID(from->key());
        QPointF clickPos = shakePos(node.data.mobaSkill.keyNode.pos, 0.01, 0.01);
        m_dragDelayData.startPos = clickPos;

        sendTouchDownEvent(id, clickPos);
        if (quickCast) {
            QTimer::singleShot(30, this, [this,id,key]() {
                sendTouchUpEvent(id, m_dragDelayData.startPos);
                detachIndexID(id);
                qDebug() << "key:" << key <<1;
                m_ctrlSteerWheel.mobaWheel.skillPressed = false;
            });
            return;
        }
        QPointF &localPos = m_ctrlSteerWheel.mobaWheel.localPos;
        QPointF rawPos{localPos.x() / m_showSize.width(), localPos.y() / m_showSize.height()};
        QPointF rawDistance{rawPos - m_ctrlSteerWheel.mobaWheel.centerPos};
        QPointF distance{rawDistance.x() / speedRatio * skillOffset, rawDistance.y() / speedRatio};
        QPointF endPos{clickPos + distance};
        m_dragDelayData.allowUp = false;
        m_dragDelayData.timer = new QTimer(this);
        m_dragDelayData.timer->setSingleShot(true);
        connect(m_dragDelayData.timer, &QTimer::timeout, this, &InputConvertGame::onDragTimer);
        m_dragDelayData.pressKey = from->key();
        m_dragDelayData.queuePos.clear();
        m_dragDelayData.queueTimer.clear();
        getDelayQueue(clickPos, endPos, 0.01f, 0.002f, 1, 3, m_dragDelayData.queuePos, m_dragDelayData.queueTimer);
        m_dragDelayData.timer->start();
    } else if (QEvent::KeyRelease == from->type() && !quickCast) {
        delete m_dragDelayData.timer;
        m_dragDelayData.timer = nullptr;
        m_dragDelayData.allowUp = true;
        int id = getTouchID(from->key());
        sendTouchMoveEvent(id, m_dragDelayData.currentPos);
        sendTouchUpEvent(id, m_dragDelayData.currentPos);
        detachIndexID(id);
        m_ctrlSteerWheel.mobaWheel.skillPressed = false;
    }
}

void InputConvertGame::processBurstClick(const KeyMap::KeyMapNode &node, const QKeyEvent *from) {
    int key = from->key();
    if (QEvent::KeyPress == from->type()) {
        int clickInterval = 1000 / node.data.burstClick.rate;
        if (clickInterval < 100) {
            clickInterval = 100;
        }
        m_burstClickKeySet.insert(from->key());
        cycleClick(node.data.burstClick.keyNode.pos, clickInterval, key);
    } else if (QEvent::KeyRelease == from->type()) {
        m_burstClickKeySet.remove(from->key());
    }
}

void InputConvertGame::cycleClick(QPointF pos, int clickInterval, int key) {
    QPointF processedPos = shakePos(pos, 0.005, 0.005);
    int id = attachTouchID(key);
    if (!m_ctrlSteerWheel.mobaWheel.skillPressed) {
        sendTouchDownEvent(id, pos);
    }
    int delay = QRandomGenerator::global()->bounded(10, 20);
    int nextClick = clickInterval - delay;
    QTimer::singleShot(delay, this, [this, key,processedPos, pos,nextClick,clickInterval,id]() {
        sendTouchUpEvent(id, processedPos);
        detachIndexID(id);
        if (!m_burstClickKeySet.contains(key)) {
            return;
        }
        QTimer::singleShot(nextClick, this, [this, key, pos,clickInterval]() {
            cycleClick(pos, clickInterval, key);
        });
    });
}

const KeyMap::KeyMapNode InputConvertGame::getNode(const QKeyEvent *from) {
    if (from->key() == Qt::Key_Control ||
        from->key() == Qt::Key_Shift ||
        from->key() == Qt::Key_Alt) {
        return m_keyMap.getKeyMapNodeKey(from->key());
    }
    if (from->modifiers() & Qt::ControlModifier) {
        KeyMap::KeyMapNode node = m_keyMap.getKeyMapNodeKey(from->key() + Qt::Key_Control);
        if (node.type != -1) { return node; }
    }
    if (from->modifiers() & Qt::ShiftModifier) {
        KeyMap::KeyMapNode node = m_keyMap.getKeyMapNodeKey(from->key() + Qt::Key_Shift);
        if (node.type != -1) { return node; }
    }
    if (from->modifiers() & Qt::AltModifier) {
        KeyMap::KeyMapNode node = m_keyMap.getKeyMapNodeKey(from->key() + Qt::Key_Alt);
        if (node.type != -1) { return node; }
    }
    return m_keyMap.getKeyMapNodeKey(from->key());
}
