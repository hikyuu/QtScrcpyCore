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

#define CURSOR_POS_CHECK 50

InputConvertGame::InputConvertGame(Controller *controller) : InputConvertNormal(controller)
{
    m_ctrlSteerWheel.delayData.timer = new QTimer(this);
    m_ctrlSteerWheel.delayData.timer->setSingleShot(true);
    connect(m_ctrlSteerWheel.delayData.timer, &QTimer::timeout, this, &InputConvertGame::onSteerWheelTimer);
}

InputConvertGame::~InputConvertGame() {}

void InputConvertGame::mouseEvent(const QMouseEvent *from, const QSize &frameSize, const QSize &showSize)
{
    // 处理开关按键
    if (m_keyMap.isSwitchOnKeyboard() == false && m_keyMap.getSwitchKey() == static_cast<int>(from->button())) {
        if (from->type() != QEvent::MouseButtonPress) {
            return;
        }
        if (!switchGameMap()) {
            m_needBackMouseMove = false;
        }
        return;
    }

    if (m_gameMap) {
        updateSize(frameSize, showSize);
        // mouse move
        if (!m_needBackMouseMove && m_keyMap.isValidMouseMoveMap()) {
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
    InputConvertNormal::mouseEvent(from, frameSize, showSize);
}

void InputConvertGame::wheelEvent(const QWheelEvent *from, const QSize &frameSize, const QSize &showSize)
{
    if (m_dragDelayData.wheeling)
        return;
    if (m_gameMap) {
        updateSize(frameSize, showSize);

        // start this
        int id = attachTouchID(QEvent::Wheel);
        QPointF startPos = QPointF(0.6, 0.6);
        if (m_needBackMouseMove) {
            // pos
            QPointF pos = from->position();
            // convert pos
            startPos.setX(pos.x() / showSize.width());
            startPos.setY(pos.y() / showSize.height());
        }
        QPointF endPos = QPointF(startPos.x(), startPos.y());
        double relativeDistanceY = 0.5;
        if (from->angleDelta().y() > 0) {
            endPos.setY(startPos.y() + relativeDistanceY);
        } else {
            endPos.setY(startPos.y() - relativeDistanceY);
        }

        sendTouchDownEvent(id, startPos);
        m_dragDelayData.timer = new QTimer(this);
        m_dragDelayData.timer->setSingleShot(true);
        connect(m_dragDelayData.timer, &QTimer::timeout, this, &InputConvertGame::onDragTimer);
        m_dragDelayData.pressKey = QEvent::Wheel;
        m_dragDelayData.currentPos = startPos;
        m_dragDelayData.queuePos.clear();
        m_dragDelayData.queueTimer.clear();
        getDelayQueue(startPos, endPos, 0.01f, 0.002f, 1, 2, m_dragDelayData.queuePos, m_dragDelayData.queueTimer);
        m_dragDelayData.wheeling = true;
        m_dragDelayData.wheelDelayUpTime = 200;
        m_dragDelayData.timer->start();
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
            m_needBackMouseMove = false;
        }
        return;
    }

    const KeyMap::KeyMapNode &node = m_keyMap.getKeyMapNodeKey(from->key());

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
                m_processMouseMove = false;
                m_currentSpeedRatio = QPointF(5, 2.5);
                int delay = 30;
                QTimer::singleShot(delay, this, [this]() { mouseMoveStopTouch(); });
                QTimer::singleShot(delay * 2, this, [this]() {
                    mouseMoveStartTouch(nullptr, *new QPointF);
                    m_processMouseMove = true;
                });
                stopMouseMoveTimer();
            } else {
                m_currentSpeedRatio = m_keyMap.getMouseMoveMap().data.mouseMove.speedRatio;
                mouseMoveStopTouch();
                mouseMoveStartTouch(nullptr, *new QPointF);
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
        return;
    // 处理普通按键
    case KeyMap::KMT_CLICK:
        processKeyClick(false, node, from);
        processAndroidKey(node.data.click.keyNode.androidKey, from);
        return;
    case KeyMap::KMT_CLICK_TWICE:
        processKeyClick(true, node, from);
        processAndroidKey(node.data.clickTwice.keyNode.androidKey, from);
        return;
    case KeyMap::KMT_CLICK_MULTI:
        processKeyClickMulti(node.data.clickMulti.keyNode.delayClickNodes, node.data.clickMulti.keyNode.delayClickNodesCount, from);
        return;
    case KeyMap::KMT_DRAG:
        processKeyDrag(node.data.drag.keyNode.pos, node.data.drag.keyNode.extendPos, from);
        return;
    case KeyMap::KMT_ANDROID_KEY:
        processAndroidKey(node.data.androidKey.keyNode.androidKey, from);
        return;
    case KeyMap::KMT_ROTARY_TABLE:
        processRotaryTable(node, from);
        return;
    case KeyMap::KMT_DUAL_MODE:
        processDualMode(node, from);
        return;
    case KeyMap::KMT_PRESS_RELEASE:
        processPressRelease(node, from);
        return;
    case KeyMap::KMT_MOBA_SKILL:
        processMobaSkill(node, from);
        return;
    case KeyMap::KMT_BURST_CLICK:
        processBurstClick(node, from);
        return;
    default:
        qDebug() << "Invalid key map type";
        break;
    }
    bool freshMouseMove = node.freshMouseMove;
    if (!node.switchMap && freshMouseMove) {
        QTimer::singleShot(50, this, [this]() { mouseMoveStopTouch(); });
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
    }
    m_frameSize = frameSize;
    m_showSize = showSize;
    m_showSizeRatio = m_showSize.width() / m_showSize.height();
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
    ControlMsg *controlMsg = new ControlMsg(ControlMsg::CMT_INJECT_TOUCH);
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
    ControlMsg *controlMsg = new ControlMsg(ControlMsg::CMT_INJECT_KEYCODE);
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

int InputConvertGame::getTouchID(int key)
{
    for (int i = 0; i < MULTI_TOUCH_MAX_NUM; i++) {
        if (key == m_multiTouchID[i]) {
            return i;
        }
    }
    return -1;
}

// -------- steer wheel event --------

void InputConvertGame::getDelayQueue(
    const QPointF &start,
    const QPointF &end,
    const double &distanceStep,
    const double &posStepconst,
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
        QPointF pos(
            x1 + (QRandomGenerator::global()->bounded(posStepconst * 2) - posStepconst),
            y1 + (QRandomGenerator::global()->bounded(posStepconst * 2) - posStepconst));
        queue.enqueue(pos);
        queue2.enqueue(QRandomGenerator::global()->bounded(lowestTimer, highestTimer));
        x1 += dx;
        y1 += dy;
    }

    queuePos = queue;
    queueTimer = queue2;
}

void InputConvertGame::onSteerWheelTimer()
{
    onWheelTimer(m_ctrlSteerWheel.touchKey);
}

void InputConvertGame::onWheelTimer(int key) {
    if (m_ctrlSteerWheel.delayData.queuePos.empty()) {
        return;
    }
    int id = getTouchID(key);
    m_ctrlSteerWheel.delayData.currentPos = m_ctrlSteerWheel.delayData.queuePos.dequeue();
    sendTouchMoveEvent(id, m_ctrlSteerWheel.delayData.currentPos);

    if (m_ctrlSteerWheel.delayData.queuePos.empty() && m_ctrlSteerWheel.delayData.pressedNum == 0) {
        sendTouchUpEvent(id, m_ctrlSteerWheel.delayData.currentPos);
        detachTouchID(key);
        return;
    }

    if (!m_ctrlSteerWheel.delayData.queuePos.empty()) {
        m_ctrlSteerWheel.delayData.timer->start(m_ctrlSteerWheel.delayData.queueTimer.dequeue());
    }
}

void InputConvertGame::processSteerWheel(const KeyMap::KeyMapNode &node, const QKeyEvent *from)
{
    int key = from->key();
    bool keyPress = from->type() == QEvent::KeyPress;

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
    } else { // left
        m_ctrlSteerWheel.pressedLeft = keyPress;
        m_ctrlSteerWheel.clickPos = node.data.steerWheel.left.pos;
    }
    if (m_ctrlSteerWheel.clickMode) {
        if (QEvent::KeyPress == from->type()) {
            int id = attachTouchID(from->key());
            sendTouchDownEvent(id, m_ctrlSteerWheel.clickPos);
        } else if (QEvent::KeyRelease == from->type()) {
            sendTouchUpEvent(getTouchID(from->key()), m_ctrlSteerWheel.clickPos);
            detachTouchID(from->key());
        }
        return;
    }
    // calc offset and wheelPressed number
    QPointF offset(0.0, 0.0);
    int pressedNum = 0;
    if (m_ctrlSteerWheel.pressedUp) {
        ++pressedNum;
        offset.ry() -= node.data.steerWheel.up.extendOffset;
    }
    if (m_ctrlSteerWheel.pressedRight) {
        ++pressedNum;
        offset.rx() += node.data.steerWheel.right.extendOffset;
    }
    if (m_ctrlSteerWheel.pressedDown) {
        ++pressedNum;
        offset.ry() += node.data.steerWheel.down.extendOffset;
    }
    if (m_ctrlSteerWheel.pressedLeft) {
        ++pressedNum;
        offset.rx() -= node.data.steerWheel.left.extendOffset;
    }
    m_ctrlSteerWheel.delayData.pressedNum = pressedNum;

    // last key release and timer no active, active timer to detouch
    if (pressedNum == 0) {
        if (m_ctrlSteerWheel.delayData.timer->isActive()) {
            m_ctrlSteerWheel.delayData.timer->stop();
            m_ctrlSteerWheel.delayData.queueTimer.clear();
            m_ctrlSteerWheel.delayData.queuePos.clear();
        }

        sendTouchUpEvent(getTouchID(m_ctrlSteerWheel.touchKey), m_ctrlSteerWheel.delayData.currentPos);
        detachTouchID(m_ctrlSteerWheel.touchKey);
        return;
    }

    // process steer wheel key event
    m_ctrlSteerWheel.delayData.timer->stop();
    m_ctrlSteerWheel.delayData.queueTimer.clear();
    m_ctrlSteerWheel.delayData.queuePos.clear();

    // first press, get key and touch down
    if (pressedNum == 1 && keyPress) {
        m_ctrlSteerWheel.touchKey = from->key();
        int id = attachTouchID(m_ctrlSteerWheel.touchKey);
        const QPointF centerPos = shakePos(node.data.steerWheel.centerPos, 0.025, 0.025);
        m_keyPosMap[Qt::Key_sterling] = centerPos;
        sendTouchDownEvent(id, centerPos);
        getDelayQueue(centerPos, centerPos + offset, 0.01f, 0.0005f, 1, 3, m_ctrlSteerWheel.delayData.queuePos, m_ctrlSteerWheel.delayData.queueTimer);
    } else {
        getDelayQueue(
            m_ctrlSteerWheel.delayData.currentPos,
            m_keyPosMap[Qt::Key_sterling] + offset,
            0.01f,
            0.0005f,
            1,
            3,
            m_ctrlSteerWheel.delayData.queuePos,
            m_ctrlSteerWheel.delayData.queueTimer);
    }
    m_ctrlSteerWheel.delayData.timer->start();
}

// -------- key event --------

void InputConvertGame::processKeyClick(bool clickTwice, const KeyMap::KeyMapNode &node, const QKeyEvent *from)
{
    if (QEvent::KeyPress == from->type()) {
        QPointF processedPos = shakePos(node.data.click.keyNode.pos, 0.01, 0.01);
        m_keyPosMap[from->key()] = processedPos;
        int id = attachTouchID(from->key());
        sendTouchDownEvent(id, processedPos);
        if (clickTwice) {
            sendTouchUpEvent(getTouchID(from->key()), processedPos);
            detachTouchID(from->key());
        }
    } else if (QEvent::KeyRelease == from->type()) {
        if (clickTwice) {
            int id = attachTouchID(from->key());
            sendTouchDownEvent(id, m_keyPosMap[from->key()]);
        }
        sendTouchUpEvent(getTouchID(from->key()), m_keyPosMap[from->key()]);
        detachTouchID(from->key());
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
    // 只显示鼠标不关闭鼠标
    if (forceSwitchOn && m_needBackMouseMove)
        return;
    //强制关闭鼠标
    if (forceSwitchOff && !m_needBackMouseMove)
        return;
    setMousePos(m_needBackMouseMove, node);
    hideMouseCursor(m_needBackMouseMove);
    m_needBackMouseMove = !m_needBackMouseMove;
}

void InputConvertGame::processKeyClickMulti(const KeyMap::DelayClickNode *nodes, const int count, const QKeyEvent *from)
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

        // Don't up it too fast
        delay += QRandomGenerator::global()->bounded(10, 20);

        QTimer::singleShot(delay, this, [this, key, clickPos]() {
            int id = getTouchID(key);
            sendTouchUpEvent(id, clickPos);
            detachTouchID(key);
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
        QTimer::singleShot(m_dragDelayData.wheelDelayUpTime, this, [this, id]() {
            sendTouchUpEvent(id, m_dragDelayData.currentPos);
            detachTouchID(m_dragDelayData.pressKey);
            m_dragDelayData.currentPos = QPointF();
            m_dragDelayData.pressKey = 0;
            m_dragDelayData.wheeling = false;
            m_dragDelayData.wheelDelayUpTime = 0;
        });
        return;
    }

    if (!m_dragDelayData.queuePos.empty()) {
        m_dragDelayData.timer->start(m_dragDelayData.queueTimer.dequeue());
    }
}

void InputConvertGame::processKeyDrag(const QPointF &startPos, QPointF endPos, const QKeyEvent *from)
{
    if (QEvent::KeyPress == from->type()) {
        // stop last
        if (m_dragDelayData.timer && m_dragDelayData.timer->isActive()) {
            m_dragDelayData.timer->stop();
            delete m_dragDelayData.timer;
            m_dragDelayData.timer = nullptr;
            m_dragDelayData.queuePos.clear();
            m_dragDelayData.queueTimer.clear();

            sendTouchUpEvent(getTouchID(m_dragDelayData.pressKey), m_dragDelayData.currentPos);
            detachTouchID(m_dragDelayData.pressKey);

            m_dragDelayData.currentPos = QPointF();
            m_dragDelayData.pressKey = 0;
        }

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
        if (!m_needBackMouseMove) {
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
        if (m_needBackMouseMove) {
            return false;
        }
        if (QEvent::MouseButtonPress == from->type() || QEvent::MouseButtonDblClick == from->type()) {
            int id = attachTouchID(from->button());
            m_keyPosMap[from->button()] = shakePos(node.data.click.keyNode.pos, 0.01, 0.01);
            sendTouchDownEvent(id, m_keyPosMap[from->button()]);
            if (node.freshMouseMove) {
                mouseMoveStopTouch();
            }
            return true;
        }
        if (QEvent::MouseButtonRelease == from->type()) {
            int id = getTouchID(from->button());
            sendTouchUpEvent(id, m_keyPosMap[from->button()]);
            detachTouchID(from->button());
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
                detachTouchID(button);
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

bool InputConvertGame::processMouseMove(const QMouseEvent *from)
{
    if (QEvent::MouseMove != from->type()) {
        return false;
    }

    if (checkCursorPos(from)) {
        m_ctrlMouseMove.lastPos = QPointF(0.0, 0.0);
        return true;
    }

    if (!m_ctrlMouseMove.lastPos.isNull() && m_processMouseMove) {
        QPointF distance_raw{ from->localPos() - m_ctrlMouseMove.lastPos };
        QPointF speedRatio = m_currentSpeedRatio;
        QPointF distance{ distance_raw.x() / speedRatio.x(), distance_raw.y() / speedRatio.y() };

        mouseMoveStartTouch(from, *new QPointF);
        startMouseMoveTimer();

        m_ctrlMouseMove.lastConvertPos.setX(m_ctrlMouseMove.lastConvertPos.x() + distance.x() / m_showSize.width());
        m_ctrlMouseMove.lastConvertPos.setY(m_ctrlMouseMove.lastConvertPos.y() + distance.y() / m_showSize.height());

        if (m_ctrlMouseMove.lastConvertPos.x() < 0.05 || m_ctrlMouseMove.lastConvertPos.x() > 0.95 || m_ctrlMouseMove.lastConvertPos.y() < 0.05
            || m_ctrlMouseMove.lastConvertPos.y() > 0.95) {
            if (m_ctrlMouseMove.smallEyes) {
                m_processMouseMove = false;
                m_ctrlMouseMove.needResetTouch = false;
                QTimer::singleShot(0, this, [this]() { mouseMoveStopTouch(); });
                QTimer::singleShot(0, this, [this]() {
                    mouseMoveStartTouch(nullptr, *new QPointF);
                    m_processMouseMove = true;
                });
            } else {
                mouseMoveStopTouch();
                mouseMoveStartTouch(from, *new QPointF);
                m_ctrlMouseMove.needResetTouch = true;
            }
        }
        sendTouchMoveEvent(getTouchID(Qt::ExtraButton24), m_ctrlMouseMove.lastConvertPos);
    }
    m_ctrlMouseMove.lastPos = from->localPos();
    return true;
}

bool InputConvertGame::checkCursorPos(const QMouseEvent *from)
{
    bool moveCursor = false;
    QPoint pos = from->pos();
    if (pos.x() < CURSOR_POS_CHECK) {
        pos.setX(m_showSize.width() - CURSOR_POS_CHECK);
        moveCursor = true;
    } else if (pos.x() > m_showSize.width() - CURSOR_POS_CHECK) {
        pos.setX(CURSOR_POS_CHECK);
        moveCursor = true;
    } else if (pos.y() < CURSOR_POS_CHECK) {
        pos.setY(m_showSize.height() - CURSOR_POS_CHECK);
        moveCursor = true;
    } else if (pos.y() > m_showSize.height() - CURSOR_POS_CHECK) {
        pos.setY(CURSOR_POS_CHECK);
        moveCursor = true;
    }

    if (moveCursor) {
        moveCursorTo(from, pos);
    }

    return moveCursor;
}

void InputConvertGame::moveCursorTo(const QMouseEvent *from, const QPoint &localPosPixel)
{
    QPoint posOffset = from->pos() - localPosPixel;
    QPoint globalPos = from->globalPos();
    globalPos -= posOffset;
    QCursor::setPos(globalPos);
}

void InputConvertGame::mouseMoveStartTouch(const QMouseEvent *from, const QPointF pos)
{
    Q_UNUSED(from)
    QPointF mouseMoveStartPos;
    if (!m_ctrlMouseMove.touching) {
        if (pos.isNull()) {
            if (m_ctrlMouseMove.smallEyes) {
                mouseMoveStartPos = shakePos(m_keyMap.getMouseMoveMap().data.mouseMove.smallEyes.pos, 0.01, 0.01);
            } else {
                mouseMoveStartPos = shakePos(m_keyMap.getMouseMoveMap().data.mouseMove.startPos, 0.025, 0.025);
            }
        } else {
            mouseMoveStartPos = pos;
        }
        int id = attachTouchID(Qt::ExtraButton24);
        sendTouchDownEvent(id, mouseMoveStartPos);
        m_ctrlMouseMove.lastConvertPos = mouseMoveStartPos;
        m_ctrlMouseMove.touching = true;
    }
}
QPointF InputConvertGame::shakePos(QPointF pos, double offsetX, double offsetY)
{
    qreal x = pos.x();
    qreal y = pos.y();
    qreal shakeX = QRandomGenerator::global()->bounded(offsetX * 2) - offsetX;
    qreal shakeY = QRandomGenerator::global()->bounded(offsetY * 2) - offsetY;
    return { x + shakeX, y + shakeY };
}

void InputConvertGame::mouseMoveStopTouch()
{
    if (m_ctrlMouseMove.touching) {
        sendTouchUpEvent(getTouchID(Qt::ExtraButton24), m_ctrlMouseMove.lastConvertPos);
        detachTouchID(Qt::ExtraButton24);
        m_ctrlMouseMove.touching = false;
        m_ctrlMouseMove.lastPos = QPointF(0.0, 0.0);
    }
}

void InputConvertGame::startMouseMoveTimer()
{
    stopMouseMoveTimer();
    if (m_ctrlMouseMove.needResetTouch) {
        m_ctrlMouseMove.timer = startTimer(500);
    }
}

void InputConvertGame::stopMouseMoveTimer()
{
    if (0 != m_ctrlMouseMove.timer) {
        killTimer(m_ctrlMouseMove.timer);
        m_ctrlMouseMove.timer = 0;
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
        QTimer::singleShot(200, this, [this]() { mouseMoveStopTouch(); });
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
    if (!m_needBackMouseMove) {
        mouseMoveStopTouch();
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
    }
}

void InputConvertGame::timerEvent(QTimerEvent *event)
{
    if (m_ctrlMouseMove.timer == event->timerId()) {
        stopMouseMoveTimer();
        mouseMoveStopTouch();
    }
}
void InputConvertGame::processRotaryTable(const KeyMap::KeyMapNode &node, const QKeyEvent *const from)
{
    QPointF pos = node.data.rotaryTable.keyNode.pos;
    QPointF speedRatio = node.data.rotaryTable.speedRatio;
    if (m_keyMap.isValidMouseMoveMap()) {
        if (QEvent::KeyPress == from->type()) {
            m_processMouseMove = false;
            m_ctrlMouseMove.needResetTouch = false;
            int delay = 30;
            m_currentSpeedRatio = speedRatio;
            mouseMoveStopTouch();
            stopMouseMoveTimer();
            QTimer::singleShot(delay, this, [this, pos]() {
                mouseMoveStartTouch(nullptr, pos);
                m_processMouseMove = true;
            });
        } else {
            m_currentSpeedRatio = m_keyMap.getMouseMoveMap().data.mouseMove.speedRatio;
            m_ctrlMouseMove.needResetTouch = true;
            mouseMoveStopTouch();
        }
        return;
    }
}
void InputConvertGame::processDualMode(KeyMap::KeyMapNode &node, const QKeyEvent *from)
{
    if (m_needBackMouseMove) {
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
            detachTouchID(key);
        });
    } else if (QEvent::KeyRelease == from->type()) {
        QPointF processedPos = shakePos(node.data.pressRelease.releasePos, 0.01, 0.01);
        int id = attachTouchID(key);
        sendTouchDownEvent(id, processedPos);
        int delay = QRandomGenerator::global()->bounded(20, 30);
        QTimer::singleShot(delay, this, [this, key, processedPos]() {
            int id = getTouchID(key);
            sendTouchUpEvent(id, processedPos);
            detachTouchID(key);
        });
        if (node.switchMap) {
            switchMouse(node, false, true);
        }
    }
}

bool InputConvertGame::processMobaWheel(const QMouseEvent *from) {
    KeyMap::KeyMapNode node = m_keyMap.getKeyMapNodeMouse(from->button());

    m_ctrlSteerWheel.touchKey = from->button();
    m_ctrlSteerWheel.mobaWheel.speedRatio = node.data.mobaWheel.speedRatio;
    m_ctrlSteerWheel.mobaWheel.centerPos = node.data.mobaWheel.centerPos;
    m_ctrlSteerWheel.mobaWheel.localPos = from->localPos();
    if (QEvent::MouseButtonPress == from->type() || QEvent::MouseButtonDblClick == from->type()) {
        QPointF wheelPos;
        QPointF startPos = wheelPos;
        m_ctrlSteerWheel.mobaWheel.wheelPressed = true;
        if (m_ctrlSteerWheel.delayData.pressedNum <= 0) {
            wheelPos = shakePos(node.data.mobaWheel.wheelPos, 0.005, 0.005);
            startPos = wheelPos;
            m_ctrlSteerWheel.mobaWheel.wheelPos = wheelPos;
            int id = attachTouchID(from->button());
            sendTouchDownEvent(id, wheelPos);
        } else{
            wheelPos = m_ctrlSteerWheel.mobaWheel.wheelPos;
            startPos = m_ctrlSteerWheel.delayData.currentPos;
            if (m_ctrlSteerWheel.delayData.timer->isActive()) {
                m_ctrlSteerWheel.delayData.timer->stop();
                m_ctrlSteerWheel.delayData.queueTimer.clear();
                m_ctrlSteerWheel.delayData.queuePos.clear();
            }
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

void InputConvertGame::stopMobaWheel(double delay) {
    QTimer::singleShot(delay, this, [this]() {
        if(m_ctrlSteerWheel.delayData.pressedNum <= 0){
            m_ctrlSteerWheel.delayData.pressedNum = 0;
            return;
        }
        if (m_ctrlSteerWheel.delayData.pressedNum > 1) {
            m_ctrlSteerWheel.delayData.pressedNum--;
            return;
        }
        if (m_ctrlSteerWheel.delayData.timer->isActive()) {
            m_ctrlSteerWheel.delayData.timer->stop();
            m_ctrlSteerWheel.delayData.queueTimer.clear();
            m_ctrlSteerWheel.delayData.queuePos.clear();
        }
        sendTouchUpEvent(getTouchID(m_ctrlSteerWheel.touchKey), m_ctrlSteerWheel.delayData.currentPos);
        detachTouchID(m_ctrlSteerWheel.touchKey);
        m_ctrlSteerWheel.delayData.pressedNum = 0;
    });
}

double InputConvertGame::calcDistance(const QPointF &point1, const QPointF &point2) {
    return std::sqrt(std::pow(point2.x() - point1.x(), 2) + std::pow(point2.y() - point1.y(), 2));
}

bool InputConvertGame::processMobaMouseMove(const QMouseEvent *from) {
    if (QEvent::MouseMove != from->type()) {
        return false;
    }
    m_ctrlSteerWheel.mobaWheel.localPos = from->localPos();

    if (m_ctrlSteerWheel.mobaWheel.skillPressed) {
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
    if (QEvent::KeyPress == from->type()) {
        if (node.data.mobaSkill.stopMove && m_ctrlSteerWheel.delayData.pressedNum > 0) {
            stopMobaWheel(0);
        }
        m_ctrlSteerWheel.mobaWheel.skillPressed = true;
        double speedRatio = node.data.mobaSkill.speedRatio;
        m_ctrlSteerWheel.mobaWheel.skillRatio = speedRatio;
        double skillOffset = node.data.mobaWheel.skillOffset;
        m_ctrlSteerWheel.mobaWheel.skillOffset = skillOffset;
        int id = attachTouchID(from->key());
        QPointF clickPos = shakePos(node.data.mobaSkill.keyNode.pos, 0.005, 0.005);
        m_dragDelayData.startPos = clickPos;

        sendTouchDownEvent(id, clickPos);
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
    } else if (QEvent::KeyRelease == from->type()) {
        delete m_dragDelayData.timer;
        m_dragDelayData.timer = nullptr;
        m_dragDelayData.allowUp = true;
        int id = getTouchID(from->key());
        sendTouchMoveEvent(id, m_dragDelayData.currentPos);
        sendTouchUpEvent(id, m_dragDelayData.currentPos);
        detachTouchID(from->key());
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
    sendTouchDownEvent(id, pos);
    int delay = QRandomGenerator::global()->bounded(10, 20);
    int nextClick = clickInterval - delay;
    QTimer::singleShot(delay, this, [this, key,processedPos, pos,nextClick,clickInterval,id]() {
        sendTouchUpEvent(id, processedPos);
        detachTouchID(key);
        if (!m_burstClickKeySet.contains(key)) {
            return;
        }
        QTimer::singleShot(nextClick, this, [this, key, pos,clickInterval]() {
            cycleClick(pos, clickInterval, key);
        });
    });
}
