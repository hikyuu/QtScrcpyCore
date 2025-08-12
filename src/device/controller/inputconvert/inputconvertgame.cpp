#include <QCursor>
#include <QDebug>
#include <QGuiApplication>
#include <QRandomGenerator>
#include <QTimer>

#include "inputconvertgame.h"
#include <QHash>
#include <QLine>
#include <Windows.h>
#include <xlocale>
#include <QApplicationStateChangeEvent>
#include <QtMath>
#include <QTimeLine>

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

    m_wheelDelayData.timer = new QTimer(this);
    m_wheelDelayData.timer->setSingleShot(true);
    connect(m_wheelDelayData.timer, &QTimer::timeout, this, &InputConvertGame::onWheelScrollTimer);

    m_wheelDelayData.upTimer = new QTimer(this);
    m_wheelDelayData.upTimer->setSingleShot(true);
    connect(m_wheelDelayData.upTimer, &QTimer::timeout, this, &InputConvertGame::onWheelUpTimer);
}

InputConvertGame::~InputConvertGame() {}

void InputConvertGame::rawMouseEvent(int dx, int dy, DWORD buttons) {

    if (!m_gameMap || m_pointerMode || !m_keyMap.isValidMouseMoveMap()) {
        return;
    }

    if (dx==0 && dy==0) {
        return;
    }

    mouseMoveStartTouch(*new QPointF);

    startMouseMoveTimer();

    QPointF speedRatio = m_currentSpeedRatio;
    QPointF currentConvertPos(
            m_ctrlMouseMove.lastConvertPos.x() + dx / (speedRatio.x() * m_frameSize.width()),
            m_ctrlMouseMove.lastConvertPos.y() + dy / (speedRatio.y() * m_frameSize.height())
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
            return;
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
        mouseMoveStopTouch(true);
        mouseMoveStartTouch(*new QPointF);
        QPointF startPos = m_keyMap.getMouseMoveMap().data.mouseMove.startPos;
        m_ctrlMouseMove.leftBoundary = generateDouble(qBound(0.0, startPos.x() - 0.3, 1.0),
                                                      qBound(0.0, startPos.x() - 0.25, 1.0));
        m_ctrlMouseMove.maxBoundary = generateDouble(0.85, 0.90);
        m_ctrlMouseMove.topBoundary = generateDouble(0.05, 0.1);
        return;
    }
    mouseMove(currentConvertPos);
}

void InputConvertGame::activated(bool isActive)
{
    if (m_gameMap) {
        if (!isActive) {
            m_pointerMode = true;
            hideMouseCursor(false);
            stopMouseMoveTimer();
            mouseMoveStopTouch(false);
        }
    }
    InputConvertNormal::activated(isActive);
}

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

    if (m_customNormalMouseClick) {
        processCustomMouseClick(from);
    } else {
        InputConvertNormal::mouseEvent(from, frameSize, showSize);
    }
}



void InputConvertGame::wheelEvent(const QWheelEvent *from, const QSize &frameSize, const QSize &showSize)
{
//    qint64 time = m_elapsedTimer.restart();
//    qDebug() << "wheelEvent time:" << time;

    if (m_gameMap) {
        // start this
//        qDebug() << "wheel begin:" << m_pointerMode;

        if (!m_pointerMode) {
            if (m_wheelDelayData.wheeling) {
                return;
            }
            m_wheelDelayData.wheeling = true;

            QPointF processedPos = generatePos(QPointF{0.96, 0.28}, 0.01, 3);
            m_keyPosMap[QEvent::Wheel] = processedPos;
            int id = attachTouchID(QEvent::Wheel);
            sendTouchDownEvent(id, processedPos);
            int delay = 0;
            int wheelDelay = 0;
            if (from->angleDelta().y() > 0) {
                qDebug() << "wheel up";
                delay = QRandomGenerator::global()->bounded(30, 50);
                wheelDelay = 500;
            } else {
                qDebug()<< "wheel down";
                delay = QRandomGenerator::global()->bounded(300, 450);
                wheelDelay = 500;
            }
            QTimer::singleShot(delay, this, [this, id]() {
                sendTouchUpEvent(id, m_keyPosMap[QEvent::Wheel]);
                detachIndexID(id);
            });
            QTimer::singleShot(wheelDelay, this, [this, id]() {
                m_wheelDelayData.wheeling = false;
            });
        }else{
            if (m_wheelDelayData.wheeling) {
                return;
            }
            m_wheelDelayData.wheeling = true;
            QPointF pos = from->position();
            QPointF startPos = QPointF(pos.x() / showSize.width(), pos.y() / showSize.width());

            if (from->modifiers() & Qt::AltModifier) {

                QPointF leftStartPos = {qBound(0.001, startPos.x() - 0.01, 0.99), startPos.y()};
                QPointF rightStartPos = {qBound(0.001, startPos.x() + 0.01, 0.99), startPos.y()};
                QPointF leftEndPos = {qBound(0.0025, leftStartPos.x() - 0.025, 0.99), leftStartPos.y()};
                QPointF rightEndPos = {qBound(0.0025, rightStartPos.x() + 0.025, 0.99), rightStartPos.y()};

                if (from->angleDelta().x() > 0) {
                    //上滚放大，由中心向外移动
                    qDebug() << "wheel up"<< leftStartPos << rightStartPos<< leftEndPos << rightEndPos;
                } else {
                    //下滚缩小，由外向中心移动
                    std::swap(leftStartPos, leftEndPos);
                    std::swap(rightStartPos, rightEndPos);
                    qDebug() << "wheel down"<< leftStartPos << rightStartPos<< leftEndPos << rightEndPos;
                }

                QQueue<QPointF> leftQueuePos;

                QQueue<QPointF> rightQueuePos;

                QQueue<quint32> queueTimer;

                int leftKey = Qt::AltModifier + Qt::Key_Zoom + Qt::Key_Left;
                int rightKey = Qt::AltModifier + Qt::Key_Zoom + Qt::Key_Right;

                getDelayQueue(leftStartPos, leftEndPos, 4, leftQueuePos, queueTimer);

                m_wheelDelayData.dragData.append({leftKey, leftStartPos, leftStartPos, leftQueuePos});

                getDelayQueue(rightStartPos, rightEndPos, 4, rightQueuePos, queueTimer);

                m_wheelDelayData.dragData.append({rightKey, rightStartPos, rightStartPos, rightQueuePos});

                int zoomLeftId = attachTouchID(leftKey);
                sendTouchDownEvent(zoomLeftId, leftStartPos);

                int zoomRightId = attachTouchID(rightKey);
                sendTouchDownEvent(zoomRightId, rightStartPos);
                m_wheelDelayData.stepTime = 4;
                m_wheelDelayData.wheelDelayUpTime = 200;

            } else {
                startPos.setY(0.5);
                QPointF endPos = startPos;
                if (from->angleDelta().y() > 0) {
                    endPos.setY(startPos.y() + generateDouble(0.499, 0.5));
                    qDebug() << "wheel up" << endPos;
                } else {
                    endPos.setY(startPos.y() - generateDouble(0.499, 0.5));
                    qDebug() << "wheel down"<< endPos;
                }
                int id = attachTouchID(QEvent::Wheel);
                QQueue<QPointF> QueuePos;
                QQueue<quint32> queueTimer;

                getDelayQueue(startPos, endPos, 4, QueuePos, queueTimer);

                m_wheelDelayData.dragData.append({QEvent::Wheel, startPos, startPos, QueuePos});
                sendTouchDownEvent(id, startPos);
                m_wheelDelayData.stepTime = 4;
                m_wheelDelayData.wheelDelayUpTime = 150;
            }
            m_wheelDelayData.timer->start();
        }
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
        mouseMoveStopTouch(false);
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
    if (id < 0) {
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
    for (int i = 0; i < m_multiTouchID.size(); ++i) {
        if (i >= MULTI_TOUCH_MAX_NUM - 1) {
            qDebug() << "attachTouchID: multi touch id too much, max is" << MULTI_TOUCH_MAX_NUM;
            return -1;
        }
        if (m_multiTouchID[i] == 0) {
            m_multiTouchID[i] = key;
            return i;
        }
    }
    int index = m_multiTouchID.size();
    m_multiTouchID.append(key);
    return index;
}

void InputConvertGame::detachTouchID(int key)
{
    for (int i = 0; i < m_multiTouchID.size(); ++i) {
        if (m_multiTouchID.at(i) == key) {
            if (i == m_multiTouchID.size() - 1) {
                m_multiTouchID.removeLast();
                qDebug() << "detachTouchID: remove last touch id:" << i;
                return;
            }
            m_multiTouchID[i] = 0;
        }
    }
}


void InputConvertGame::detachIndexID(int i){
    if (i < 0 || i >= m_multiTouchID.size()) {
        return;
    }

    m_multiTouchID[i] = Qt::Key_unknown;

    QTimer::singleShot(30, this, [this,i]() {
        if (i == m_multiTouchID.size() - 1) {
            m_multiTouchID.removeLast();
//            qDebug() << "detachIndexID: remove last touch id:" << i;
            return;
        }
        m_multiTouchID[i] = 0;
    });
}

int InputConvertGame::getTouchID(int key) const {
    for (int i = 0; i < m_multiTouchID.size(); ++i) {
        if (key == m_multiTouchID[i]) {
            return i;
        }
    }
    return -1;
}

// -------- steer wheel event --------

void InputConvertGame::getDelayQueue(const QPointF &start, const QPointF &end, quint32 lowestTimer,
                                     QQueue<QPointF> &queuePos, QQueue<quint32> &queueTimer)
{
    QQueue<QPointF> queue;
    QQueue<quint32> queue2;

    QTimeLine timeline;
    timeline.setFrameRange(0, QRandomGenerator::global()->bounded(40, 60)); // 500ms / 4ms = 125帧
    timeline.setEasingCurve(QEasingCurve::InOutQuad); // 模拟加速度

// 垂直偏移量（曲率控制因子）
    double offset = QLineF(start, end).length() * generateDouble(0.05, 0.2);

// 计算起点到终点的方向向量
    double dx = end.x() - start.x();
    double dy = end.y() - start.y();

// 计算垂直方向（始终指向坐标系上方）
    QPointF dir;
    if (dx >= 0) { // 终点在起点右侧或相同x位置
        dir = QPointF(dy, -dx); // 逆时针旋转90度（指向坐标系上方）
    } else { // 终点在起点左侧
        dir = QPointF(-dy, dx); // 顺时针旋转90度（指向坐标系上方）
    }

    double len = std::hypot(dir.x(), dir.y());
    if (len > 0) dir /= len; // 单位化

// 修改：在x方向0.3-0.7之间的随机位置设置控制点
    double ratio = generateDouble(0.2, 0.8); // 生成0.3-0.7之间的随机比例
    QPointF basePoint = start + ratio * (end - start); // 在x方向的指定比例位置建立基准点
    QPointF ctrl1 = basePoint + dir * offset; // 轻微上凸的控制点

    QPainterPath path;

    path.moveTo(start);
    path.quadTo(ctrl1, end); // 二次贝塞尔曲线

    for (int i = 1; i <= timeline.endFrame(); ++i) {

        qreal progress = timeline.easingCurve().valueForProgress(i / qreal(timeline.endFrame()));
        QPointF newPos = path.pointAtPercent(progress);

        queue.enqueue(newPos);
        queue2.enqueue(lowestTimer);
    }
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

    m_ctrlMouseMove.resetMoveTimer.stop();
    mouseMoveStopTouch(false);
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
            return;
        }
        sendTouchUpEvent(id, m_ctrlSteerWheel.delayData.currentPos);
        detachIndexID(id);
        m_ctrlSteerWheel.wheeling = false;
        return;
    }
    if (!m_ctrlSteerWheel.delayData.queuePos.empty()) {
        m_ctrlSteerWheel.delayData.timer->start(m_ctrlSteerWheel.delayData.queueTimer.dequeue());
    }else {
        if (m_ctrlSteerWheel.delayData.pressedNum > 1 && m_ctrlSteerWheel.simulateWheel) {
            // 如果有两个按键按下，则计算中间点
            QPointF startPos = m_ctrlSteerWheel.delayData.shakeEndPos;
            QPointF endPos = m_ctrlSteerWheel.delayData.middlePoint;
            if (m_ctrlSteerWheel.delayData.isEnd) {
                QPointF middlePoint = pointAtPercent(m_ctrlSteerWheel.centerPos + m_ctrlSteerWheel.delayData.offsetY,
                                                     m_ctrlSteerWheel.delayData.endPos,
                                                     0.5);
                middlePoint = generatePos(middlePoint, 0.01, 2);
                endPos = middlePoint;
                m_ctrlSteerWheel.delayData.middlePoint = middlePoint;
            } else {
                double distance = calcDistance(m_ctrlSteerWheel.centerPos, startPos);
                double radius = distance * 0.1;
                startPos = generatePos(m_ctrlSteerWheel.delayData.endPos, radius, 2);
                m_ctrlSteerWheel.delayData.shakeEndPos = startPos;
                std::swap(startPos, endPos);
            }
            m_ctrlSteerWheel.delayData.path = generateLinePath(startPos, endPos);
            getDelayQueue(
                    m_ctrlSteerWheel.delayData.queuePos,
                    m_ctrlSteerWheel.delayData.queueTimer, false, 16, 16);
            m_ctrlSteerWheel.delayData.isEnd = !m_ctrlSteerWheel.delayData.isEnd;
            m_ctrlSteerWheel.delayData.timer->start(20);
        }
    }
}

void InputConvertGame::processSteerWheel(const KeyMap::KeyMapNode &node, const QKeyEvent *from)
{
    int key = from->key();
    // 是否按下
    bool keyPress = from->type() == QEvent::KeyPress;
    bool boostKey = key == node.data.steerWheel.boost.key;
    m_ctrlSteerWheel.simulateWheel = node.data.steerWheel.simulateWheel;
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
            sendTouchDownEvent(id, generatePos(m_ctrlSteerWheel.clickPos, 0.01,3));
        } else if (QEvent::KeyRelease == from->type()) {
            int id = getTouchID(from->key());
            sendTouchUpEvent(id, m_keyPosMap[from->key()]);
            detachIndexID(id);
        }
        return;
    }
    // calc offset and wheelPressed number
    QPointF offset(0.0, 0.0);
    int pressedNum = 0;
    if (m_ctrlSteerWheel.pressedUp) {
        ++pressedNum;
        offset.ry() -= node.data.steerWheel.up.extendOffset;
        m_ctrlSteerWheel.delayData.offsetY = {0, offset.y()};
    }
    if (m_ctrlSteerWheel.pressedRight) {
        ++pressedNum;
        offset.rx() += node.data.steerWheel.right.extendOffset;
    }
    if (m_ctrlSteerWheel.pressedDown) {
        ++pressedNum;
        offset.ry() += node.data.steerWheel.down.extendOffset;
        m_ctrlSteerWheel.delayData.offsetY = {0, offset.y()};
    }
    if (m_ctrlSteerWheel.pressedLeft) {
        ++pressedNum;
        offset.rx() -= node.data.steerWheel.left.extendOffset;
    }
    m_ctrlSteerWheel.delayData.pressedNum = pressedNum;

    if (boostKey) {
        if(!keyPress) return;
        if (!m_ctrlSteerWheel.pressedUp) {
            m_ctrlSteerWheel.pressedBoost = true;
            qDebug() << "boost key pressed, but no up key pressed";
            return;
        }
        m_ctrlSteerWheel.pressedBoost = !m_ctrlSteerWheel.pressedBoost;
    }else if (key == node.data.steerWheel.up.key) {
        if (keyPress) {
            if (from->modifiers() & Qt::ShiftModifier && node.data.steerWheel.boost.key == Qt::Key_Shift) {
                m_ctrlSteerWheel.pressedBoost = true; // 按下shift键持续奔跑
            }
            if(from->modifiers() & Qt::ControlModifier && node.data.steerWheel.boost.key == Qt::Key_Control) {
                m_ctrlSteerWheel.pressedBoost = true; // 按下Ctrl键持续奔跑
            }
            if( from->modifiers() & Qt::AltModifier && node.data.steerWheel.boost.key == Qt::Key_Alt) {
                m_ctrlSteerWheel.pressedBoost = true; // 按下Alt键持续奔跑
            }
        } else if (pressedNum>0){
            m_ctrlSteerWheel.pressedBoost = false; // 取消持续奔跑
        }
    }

    if (m_ctrlSteerWheel.pressedBoost) {
        if (m_ctrlSteerWheel.pressedUp) {
            offset.ry() -= node.data.steerWheel.boost.extendOffset;
            m_ctrlSteerWheel.delayData.offsetY = {0, offset.y()};
        }
        if (m_ctrlSteerWheel.pressedRight) {
            offset.rx() += node.data.steerWheel.boost.extendOffset;
        }
        if (m_ctrlSteerWheel.pressedLeft) {
            offset.rx() -= node.data.steerWheel.boost.extendOffset;
        }
        if (m_ctrlSteerWheel.pressedDown) {
            offset.ry() += node.data.steerWheel.boost.extendOffset;
            m_ctrlSteerWheel.delayData.offsetY = {0, offset.y()};
        }
    }

    if (pressedNum > 1) {
        offset /= generateDouble(1.5, 2);
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
        m_ctrlSteerWheel.pressedBoost = false;

        double distance = calcDistance(m_ctrlSteerWheel.delayData.currentPos, m_ctrlSteerWheel.centerPos);
//        qDebug() << "distance:" << distance;
        if (distance >= node.data.steerWheel.up.extendOffset) {
            qDebug() << "steer move center";
            updatePosition(m_ctrlSteerWheel.centerPos);
            m_ctrlSteerWheel.delayData.path = generateBezierPath();
            getDelayQueue(
                    m_ctrlSteerWheel.delayData.queuePos,
                    m_ctrlSteerWheel.delayData.queueTimer, false,
                    8, 0);
            m_ctrlSteerWheel.delayData.timer->start(8);
            return;
        }

//        QMutexLocker locker(&m_ctrlSteerWheel.steerMutex);
        if (!m_ctrlSteerWheel.wheeling) {
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
    if (pressedNum == 1 && keyPress && !boostKey && !m_ctrlSteerWheel.wheeling ) {
//        qDebug() << "first press";
//        QMutexLocker locker(&m_ctrlSteerWheel.steerMutex);
        if (m_ctrlSteerWheel.wheeling) {
            return;
        }
        m_ctrlSteerWheel.wheeling = true;
        int id = attachTouchID(m_ctrlSteerWheel.touchKey);

        QPointF centerPos = node.data.steerWheel.centerPos;
        if (node.data.steerWheel.simulateWheel) {
            centerPos = generatePos(node.data.steerWheel.centerPos, 0.025, 3);
        }
        m_ctrlSteerWheel.centerPos = centerPos;
        sendTouchDownEvent(id, centerPos);
        m_ctrlSteerWheel.delayData.historyPoints.clear();
        m_ctrlSteerWheel.delayData.historyPoints.append(centerPos);

        QPointF endPos = centerPos + offset;

        if (m_ctrlSteerWheel.simulateWheel) {
            double distance = calcDistance(centerPos, endPos);
            endPos = generatePos(endPos, distance * 0.05, 2);
        }
        updatePosition(endPos);
        m_ctrlSteerWheel.delayData.path = generateBezierPath();
        getDelayQueue(
                m_ctrlSteerWheel.delayData.queuePos,
                m_ctrlSteerWheel.delayData.queueTimer, false, 8, 0);
    } else {
        bool slowEnd = true;
        if (boostKey) {
            slowEnd = false; // boost 按键不需要慢速结束
        }
        QPointF startPos = m_ctrlSteerWheel.delayData.currentPos;
        QPointF centerPos = m_ctrlSteerWheel.centerPos;
        QPointF endPos = centerPos + offset;

        if (pressedNum > 1) {
            m_ctrlSteerWheel.delayData.isEnd = true;
            slowEnd = false;
        }
        if (!m_ctrlSteerWheel.simulateWheel) {
            slowEnd = false;
        }
        updatePosition(endPos);
        m_ctrlSteerWheel.delayData.path = generateLinePath(startPos, endPos);
        m_ctrlSteerWheel.delayData.endPos = endPos;
        m_ctrlSteerWheel.delayData.shakeEndPos = endPos;
        getDelayQueue(
                m_ctrlSteerWheel.delayData.queuePos,
                m_ctrlSteerWheel.delayData.queueTimer, slowEnd,
                8, 0);
    }
    m_ctrlSteerWheel.delayData.timer->start(8);
}
// -------- key event --------

QPointF InputConvertGame::pointAtPercent(const QPointF& start, const QPointF& end,double percent) {
    // 1. 计算方向向量（终点 - 起点）
    QPointF direction = end - start;

    // 2. 将方向向量缩放至90%长度
    QPointF scaledDirection = direction * percent;

    // 3. 将缩放向量叠加到起点，得到目标点
    QPointF result = start + scaledDirection;
    return result;
}

void InputConvertGame::updatePosition(const QPointF& newPos) {
    m_ctrlSteerWheel.delayData.historyPoints.append(newPos);
    if (m_ctrlSteerWheel.delayData.historyPoints.size() > MAX_HISTORY) {
        m_ctrlSteerWheel.delayData.historyPoints.removeFirst();  // 移除最旧的点
    }
}

QPointF InputConvertGame::generatePos(QPointF pos, double radius = 0.01, double k = 3.0) {
    // 生成[0,1]均匀分布的随机数
    double theta = QRandomGenerator::global()->generateDouble() * 2 * M_PI; // [0, 2π]

    // 非均匀半径生成（k>1时中心密度高）
    double u = QRandomGenerator::global()->generateDouble();
    double r = radius * std::pow(u, 1.0 / k);

    // 极坐标转笛卡尔坐标
    double x = pos.x() + r * std::cos(theta);
    double y = pos.y() + r * std::sin(theta);

    return {x, y};
}

void
InputConvertGame::getDelayQueue(QQueue<QPointF> &queuePos, QQueue<quint32> &queueTimer, bool detect, quint32 stepTimer,
                                quint32 randomTimer = 0) const {
    QTimeLine timeline;
    int endFrame;
    if (detect) {
        timeline.setEasingCurve(QEasingCurve::OutCirc);
        endFrame = 60;
    } else {
        endFrame = 15;
    }
    timeline.setFrameRange(0, endFrame);
    QQueue<QPointF> queue;
    QQueue<quint32> queue2;

    for (int i = 1; i <= timeline.endFrame(); ++i) {
        qreal progress = timeline.easingCurve().valueForProgress(i / qreal(timeline.endFrame()));
        QPointF pos = m_ctrlSteerWheel.delayData.path.pointAtPercent(progress);
        queue.enqueue(pos);

        if (randomTimer > 0) {
            quint32 randomDelay = QRandomGenerator::global()->bounded(randomTimer);
            queue2.enqueue(stepTimer + randomDelay);
        } else {
            // 否则直接使用stepTimer
            queue2.enqueue(stepTimer);
        }
    }

    queuePos = queue;
    queueTimer = queue2;
}

QVector<QPointF> InputConvertGame::calculateControlPoints() {
    QVector<QPointF> ctrlPoints;
    const auto &historyPoints = m_ctrlSteerWheel.delayData.historyPoints;

    if (historyPoints.size() < 2) return ctrlPoints;

    // 情况1：有2个历史点（生成三次贝塞尔曲线）
    if (historyPoints.size() == 2) {
        QPointF dir = historyPoints[1] - historyPoints[0]; // 方向向量
        QPointF perpDir(-dir.y(), dir.x());  // 垂直向量（法线方向）

        auto randSignedOffset = [this](qreal min, qreal max) -> qreal {
            qreal base = generateDouble(min, max);
            return QRandomGenerator::global()->bounded(2) ? base : -base; // 50% 概率取负值
        };
        // 随机化参数
        qreal alpha = generateDouble(0.3, 0.7);       // [0.3, 0.7)
        qreal gamma = generateDouble(0.3, 0.7);       // [0.3, 0.7)
        qreal betaScale = randSignedOffset(0.1, 0.3); // [0.05, 0.15)
        qreal deltaScale = randSignedOffset(0.1, 0.3); // [0.05, 0.15)
        // 计算控制点（靠近连线）
        QPointF control1 = historyPoints[0] + alpha * dir + betaScale * perpDir;
        QPointF control2 = historyPoints[1] - gamma * dir + deltaScale * perpDir;
        ctrlPoints << control1 << control2 << historyPoints[1];
        return ctrlPoints;
    }

    // 情况2：有3个历史点（生成平滑曲线）
    // 控制点1：P1 + (P2 - P0) * k，保证切线连续
    QPointF tangent = historyPoints[2] - historyPoints[0];
    QPointF ctrl1 = historyPoints[1] + tangent * 0.5;  // 0.2为平滑系数

    // 控制点2：P2 - (P2 - P1) * k，避免曲率突变
    QPointF dirPrev = historyPoints[1] - historyPoints[0];
    QPointF ctrl2 = historyPoints[2] - dirPrev * 0.5;

    ctrlPoints << ctrl1 << ctrl2 << historyPoints[2];
    return ctrlPoints;
}

QPainterPath InputConvertGame::generateLinePath(QPointF start, QPointF end) {
    QPainterPath path;
    path.moveTo(start);
    path.lineTo(end);
    return path;
}

// 生成贝塞尔曲线路径（用于绘制）
QPainterPath InputConvertGame::generateBezierPath() {
    QPainterPath path;
    if (m_ctrlSteerWheel.delayData.historyPoints.isEmpty()) return path;

    path.moveTo(m_ctrlSteerWheel.delayData.historyPoints.first());

    if (m_ctrlSteerWheel.delayData.historyPoints.size() >= 3) {
        path.moveTo(m_ctrlSteerWheel.delayData.historyPoints[1]);
    }

    auto ctrlPoints = calculateControlPoints();

    // 根据控制点数量选择曲线类型
    switch (ctrlPoints.size()) {
        case 2:  // 二次贝塞尔曲线
            path.quadTo(ctrlPoints[0], ctrlPoints[1]);
            break;
        case 3:  // 三次贝塞尔曲线
            path.cubicTo(ctrlPoints[0], ctrlPoints[1], ctrlPoints[2]);
            break;
        default: // 线性路径
            path.lineTo(m_ctrlSteerWheel.delayData.historyPoints.last());
    }
    return path;
}

void InputConvertGame::processKeyClick(bool clickTwice, const KeyMap::KeyMapNode &node, const QKeyEvent *from)
{
    if (QEvent::KeyPress == from->type()) {
        if (from->key() == Qt::Key_F2) {
            QList<int> stuckArray;
            int stuckNumber = 0;
            for (int i : m_multiTouchID) {
                if (0 != i) {
                    stuckNumber++;
                    stuckArray.append(i);
                }
            }
            qDebug()<<"stuckNumber:"<<stuckNumber <<"exceptionButton:"<< stuckArray;
        }

        QPointF processedPos = generatePos(node.data.click.keyNode.pos, 0.015);
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
    if (!switchMap || QEvent::KeyRelease != from->type()) return;
    switchMouse(node, forceSwitchOn, forceSwitchOff);
}
void InputConvertGame::switchMouse(const KeyMap::KeyMapNode &node, bool forceSwitchOn, bool forceSwitchOff)
{
    // 只显示鼠标不关闭鼠标
    bool newPointerMode;
    if (forceSwitchOn) {
        newPointerMode = true;
    }else if (forceSwitchOff) {
        newPointerMode = false;
    }else {
        newPointerMode = !m_pointerMode;
    }
    qDebug() << "switchMouse pointerMode:" << newPointerMode;
    if (newPointerMode != m_pointerMode) {
        hideMouseCursor(!newPointerMode);
        setMousePos(newPointerMode, node);
        m_pointerMode = newPointerMode;
    }
}

void InputConvertGame::processKeyClickMulti(const KeyMap::DelayClickNode *nodes, const int count, const double pressTime, const QKeyEvent *from)
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
        clickPos = generatePos(nodes[i].pos, 0.01);
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
    bool hasDrag = false;
    for (auto &item: m_wheelDelayData.dragData) {
        if (item.queuePos.empty()) {
            continue;
        }
        int id = getTouchID(item.pressKey);
        item.currentPos = item.queuePos.dequeue();
        sendTouchMoveEvent(id, item.currentPos);
        if (!item.queuePos.empty()) {
            hasDrag = true;
        }
    }

    if (hasDrag) {
        m_wheelDelayData.timer->start(m_wheelDelayData.stepTime);
        return;
    }
    m_wheelDelayData.upTimer->stop();
    m_wheelDelayData.upTimer->start(m_wheelDelayData.wheelDelayUpTime);
    qDebug()<< "onWheelScrollTimer: no drag data, stop timer";
}

void InputConvertGame::onWheelUpTimer(){

    for (auto &item: m_wheelDelayData.dragData) {
        int id = getTouchID(item.pressKey);
        sendTouchUpEvent(id, item.currentPos);
        detachIndexID(id);
        qDebug() << "WheelUp id:" << id;
    }

    m_wheelDelayData.dragData.clear();
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
        getDelayQueue(startPos, endPos, 3, m_dragDelayData.queuePos, m_dragDelayData.queueTimer);
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
            m_keyPosMap[from->button()] = generatePos(node.data.click.keyNode.pos, 0.01);
            sendTouchDownEvent(id, m_keyPosMap[from->button()]);

            return true;
        }
        if (QEvent::MouseButtonRelease == from->type()) {
            int id = getTouchID(from->button());
            sendTouchUpEvent(id, m_keyPosMap[from->button()]);
            detachIndexID(id);
            if (node.freshMouseMove) {
                stopMouseMoveTimer();
                mouseMoveStopTouch(false);
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
            clickPos = generatePos(nodes[i].pos, 0.01);
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

bool InputConvertGame::processCustomMouseClick(const QMouseEvent *from) {
    if (QEvent::MouseButtonPress == from->type() || QEvent::MouseButtonDblClick == from->type()) {
//        qDebug() << "customMouseClick "<<from->button();
        if (from->button()==Qt::LeftButton) {
            if (from->modifiers() & Qt::ControlModifier) {
                QPointF pos = from->localPos();
                int key = Qt::ControlModifier + Qt::LeftButton;
                // convert pos
                pos.setX(pos.x() / m_showSize.width());
                pos.setY(pos.y() / m_showSize.height());
                int delay = 0;
                QPointF clickPos;
                int clickTime = 2;
                for (int i = 0; i < clickTime; i++) {
                    clickPos = generatePos(pos, 0.005);
                    QTimer::singleShot(delay, this, [this,key, clickPos]() {
                        int id = attachTouchID(key);
                        sendTouchDownEvent(id, clickPos);
                    });
                    // Don't up it too fast
                    delay += QRandomGenerator::global()->bounded(10, 20);
                    if (i >= clickTime - 1) {
                        delay = delay * 2;
                    }
                    QTimer::singleShot(delay, this, [this, key, clickPos]() {
                        int id = getTouchID(key);
                        detachIndexID(id);
                        sendTouchUpEvent(id, clickPos);
                    });
                    delay += QRandomGenerator::global()->bounded(10, 20);
                }
                return true;
            } else {
                int id = attachTouchID(from->button());
                // pos
                QPointF pos = from->localPos();
                // convert pos
                pos.setX(pos.x() / m_showSize.width());
                pos.setY(pos.y() / m_showSize.height());
                m_keyPosMap[from->button()] = pos;
                sendTouchDownEvent(id, pos);
                qDebug() << "left button click ";
                return true;
            }
        }else if (from->button()==Qt::RightButton) {
            if (from->modifiers() & Qt::ControlModifier) {
                dragStop();
                // pos
                QPointF pos = from->localPos();
                // convert pos
                pos.setX(pos.x() / m_showSize.width());
                pos.setY(pos.y() / m_showSize.height());
                // 执行自定义操作（如多选、缩放等）
                // stop last
                if (m_dragDelayData.timer && m_dragDelayData.timer->isActive()) {
                    return false;
                }
                // start this
                int id = attachTouchID(Qt::Key_Control + Qt::RightButton);
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
                m_dragDelayData.pressKey = Qt::Key_Control + Qt::RightButton;
                m_dragDelayData.currentPos = pos;
                m_dragDelayData.queuePos.clear();
                m_dragDelayData.queueTimer.clear();
                m_dragDelayData.dragDelayUpTime = 0;
                getDelayQueue(pos, endPos, 3, m_dragDelayData.queuePos, m_dragDelayData.queueTimer);
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
        pos.setX(pos.x() / m_showSize.width());
        pos.setY(pos.y() / m_showSize.height());
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
    QPointF distance_raw{from->localPos() - lastPos};

    if (qAbs(distance_raw.x()) > 200) {
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

double InputConvertGame::generateDouble(double min, double max) {
    return QRandomGenerator::global()->generateDouble() * (max - min) + min;;
}

bool InputConvertGame::mouseMove(QPointF &currentConvertPos) {
    if (m_ctrlMouseMove.touching) {
        m_ctrlMouseMove.lastConvertPos = currentConvertPos;
        sendTouchMoveEvent(m_ctrlMouseMove.focusTouchID, currentConvertPos);
    }
    return true;
}

bool InputConvertGame::checkBoundary(const QPointF &currentConvertPos) const {
    if (!m_ctrlMouseMove.needResetTouch) {
        return false;
    }
    return currentConvertPos.x() > m_ctrlMouseMove.maxBoundary
    || currentConvertPos.x() < m_ctrlMouseMove.leftBoundary
    || currentConvertPos.y() < m_ctrlMouseMove.topBoundary
    || currentConvertPos.y() > m_ctrlMouseMove.maxBoundary;
}

void InputConvertGame::resetMouseMove(const QPointF pos) {
    mouseMoveStopTouch(false);
    mouseMoveStartTouch(pos);
}

bool InputConvertGame::checkCursorPos(const QMouseEvent *from)
{
    QPoint pos = from->pos();

    int oneOfSevenWidth = m_ctrlMouseMove.centerPos.x() / 7;
    int oneOfSevenHeight = m_ctrlMouseMove.centerPos.y() / 7;

    bool outOfCenter = false;
    if (pos.x() > m_ctrlMouseMove.centerPos.x() + oneOfSevenWidth) {
        outOfCenter = true;
    } else if (pos.x() < m_ctrlMouseMove.centerPos.x() - oneOfSevenWidth) {
        outOfCenter = true;
    } else if (pos.y() > m_ctrlMouseMove.centerPos.y() + oneOfSevenHeight) {
        outOfCenter = true;
    } else if (pos.y() < m_ctrlMouseMove.centerPos.y() - oneOfSevenHeight) {
        outOfCenter = true;
    }

    if (!outOfCenter) {
//        qDebug() << "mouse move in center pos:" << pos;
        return false;
    }

    bool outOfBoundary = checkOutOfBoundary(pos, oneOfSevenWidth, oneOfSevenHeight);

    if (!outOfBoundary) {
        if (m_ctrlMouseMove.outOfBoundary) {
//            qDebug() << "mouse move in boundary pos:" << pos;
        }
        m_ctrlMouseMove.outOfBoundary = false;
        return outOfBoundary;
    }
    if (!m_ctrlMouseMove.outOfBoundary) {
//        qDebug() << "mouse move first out of boundary pos:" << pos;
        m_ctrlMouseMove.cursorPos = m_ctrlMouseMove.lastPos;
        moveCursorTo(from, m_ctrlMouseMove.centerPos);
        m_ctrlMouseMove.lastPos = m_ctrlMouseMove.centerPos;
        m_ctrlMouseMove.outOfBoundary = true;
        return outOfBoundary;
    }
    QPoint windowPos = QApplication::activeWindow()->mapFromGlobal(QCursor::pos()); // 转换为当前窗口坐标
    bool current = checkOutOfBoundary(windowPos, oneOfSevenWidth, oneOfSevenHeight);
    if (current) {
        qDebug() << "mouse move still out of boundary pos:" << pos;
        moveCursorTo(from, m_ctrlMouseMove.centerPos);
        return outOfBoundary;
    }

    if (pos.x() == 0 || pos.x() == m_showSize.width() - 1 || pos.y() == 0 || pos.y() == m_showSize.height() - 1) {
        qDebug() << "mouse move out of edge pos:" << pos;
        m_ctrlMouseMove.outOfBoundary = true;
        QCoreApplication::removePostedEvents(this, QEvent::MouseMove);
        moveCursorTo(from, m_ctrlMouseMove.centerPos);
        m_ctrlMouseMove.lastPos = m_ctrlMouseMove.centerPos;
        return outOfBoundary;
    }
    //    qDebug() << "mouse move next out of boundary pos:" << pos;
    return outOfBoundary;
}

bool InputConvertGame::checkOutOfBoundary(const QPoint &pos, int oneOfSevenWidth, int oneOfSevenHeight) const {
    bool outOfBoundary = false;
    if (pos.x() < oneOfSevenWidth*2) {
        outOfBoundary = true;
    } else if (pos.x() > m_showSize.width() - oneOfSevenWidth * 2) {
        outOfBoundary = true;
    } else if (pos.y() < oneOfSevenHeight*2) {
        outOfBoundary = true;
    } else if (pos.y() > m_showSize.height() - oneOfSevenHeight * 2) {
        outOfBoundary = true;
    }
    return outOfBoundary;
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
    if (!m_ctrlMouseMove.touching && !m_pointerMode) {
        if (!m_ctrlMouseMove.mouseMutex.try_lock()) {
            qDebug() << "abandon multi thread:touch";
            return true;
        }
        m_ctrlMouseMove.touching = true;
        QPointF mouseMoveStartPos;
        if (pos.isNull()) {
            mouseMoveStartPos = generatePos(m_keyMap.getMouseMoveMap().data.mouseMove.startPos, 0.025);
        } else {
            mouseMoveStartPos = pos;
        }
//        qDebug() << "mouse move start pos:" << mouseMoveStartPos;
        int id = attachTouchID(Qt::ExtraButton24);
        m_ctrlMouseMove.focusTouchID = id;
        m_ctrlMouseMove.startPos = mouseMoveStartPos;
        m_ctrlMouseMove.lastConvertPos = mouseMoveStartPos;
        sendTouchDownEvent(id, mouseMoveStartPos);
//        qDebug() << "mouse move start touch id:" << id;
        m_ctrlMouseMove.mouseMutex.unlock();
        return true;
    }
    return false;
}

void InputConvertGame::mouseMoveStopTouch(bool delay)
{
    m_ctrlMouseMove.mouseMutex.lock();
    if (!m_ctrlMouseMove.touching) {
        qDebug() << "mouse move stop concurrent: no touching";
        m_ctrlMouseMove.mouseMutex.unlock();
        return;
    }
    
    int id = getTouchID(Qt::ExtraButton24);
    if (id < 0) {
        m_ctrlMouseMove.mouseMutex.unlock();
        return;
    }
    
    m_ctrlMouseMove.touching = false;
    m_multiTouchID[id] = Qt::Key_unknown;
    QPointF touchUpPos = m_ctrlMouseMove.lastConvertPos;
    if (delay) {
        QTimer::singleShot(QRandomGenerator::global()->bounded(100,200), this, [this, id,touchUpPos]() {
            detachIndexID(id);
            sendTouchUpEvent(id, touchUpPos);
        });
    } else {
        detachIndexID(id);
        sendTouchUpEvent(id, touchUpPos);
    }
//    qDebug()<< "mouse move stop touch id:" << id;
    m_ctrlMouseMove.mouseMutex.unlock();
}


void InputConvertGame::startMouseMoveTimer()
{
    m_ctrlMouseMove.resetMoveTimer.start(m_ctrlMouseMove.resetMoveDelay);
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

    if (m_gameMap) {
        QPoint globalPos = QCursor::pos(); // 获取屏幕全局坐标
        QPoint windowPos = QApplication::activeWindow()->mapFromGlobal(globalPos); // 转换为当前窗口坐标
        m_ctrlMouseMove.lastPos = windowPos;
    } else {
        stopMouseMoveTimer();
        mouseMoveStopTouch(false);
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
#ifdef Q_OS_WIN32
        QWidget *activeWindow = QApplication::activeWindow();
        RECT rect; // 定义一个 RECT 结构体，表示锁定的范围
        QPointF topLeft = activeWindow->mapToGlobal(activeWindow->rect().topLeft());
        rect.top = topLeft.y()+ m_showSize.height()/2;
        rect.left = topLeft.x()+ m_showSize.width()/2;
        rect.bottom = topLeft.y() + m_showSize.height()/2+1;
        rect.right = topLeft.x() + m_showSize.width()/2+1;
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
    mouseMoveStopTouch(false);
    if (m_pointerMode) {
        QWidget *activeWindow = QApplication::activeWindow();

        if (node.focusOn) {
            QPoint point = { static_cast<int>(m_showSize.width() * node.focusPos.x()), static_cast<int>(m_showSize.height() * node.focusPos.y()) };
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
        QCursor::setPos(QApplication::activeWindow()->mapToGlobal(m_ctrlMouseMove.centerPos));
        QCoreApplication::removePostedEvents(this, QEvent::MouseMove);
        m_ctrlMouseMove.lastPos = m_ctrlMouseMove.centerPos;
    }
}

void InputConvertGame::timerEvent(QTimerEvent *event)
{
    if (m_ctrlMouseMove.timer == event->timerId()) {
        stopMouseMoveTimer();
        mouseMoveStopTouch(true);
        qDebug()<< "mouse move timer stop";
    }

}
void InputConvertGame::processRotaryTable(const KeyMap::KeyMapNode &node, const QKeyEvent *const from)
{
    int key = Qt::ExtraButton24 + from->key();
    if (m_keyMap.isValidMouseMoveMap()) {
        if (QEvent::KeyPress == from->type()) {
            int delay = node.data.rotaryTable.delay;
            QPointF pos = generatePos(node.data.rotaryTable.keyNode.pos, 0.025);
            m_keyPosMap[key] = pos;
            int id = attachTouchID(key);
            sendTouchDownEvent(id, pos);

            QTimer::singleShot(delay, this, [this, node, key, pos]() {

                int id = getTouchID(key);
                if (id < 0) return;
                m_ctrlMouseMove.needResetTouch = false;
                stopMouseMoveTimer();
                mouseMoveStopTouch(true);
                m_ctrlMouseMove.touching = true;
                m_ctrlMouseMove.focusTouchID = id;
                m_ctrlMouseMove.startPos = pos;
                m_ctrlMouseMove.lastConvertPos = pos;
                QPointF speedRatio = node.data.rotaryTable.speedRatio;
                m_currentSpeedRatio = speedRatio;
            });
        } else {
            int id = getTouchID(key);
            if (id < 0) return;
            stopMouseMoveTimer();
            m_ctrlMouseMove.needResetTouch = true;
            m_currentSpeedRatio = m_keyMap.getMouseMoveMap().data.mouseMove.speedRatio;
            QPointF touchUpPos;
            if (m_ctrlMouseMove.focusTouchID == id) {
                touchUpPos = m_ctrlMouseMove.lastConvertPos;
                m_ctrlMouseMove.touching = false;
            } else {
                touchUpPos = m_keyPosMap[key];
            }
            m_keyPosMap.remove(key);

            detachIndexID(id);
            sendTouchUpEvent(id, touchUpPos);
        }
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
        QPointF processedPos = generatePos(node.data.pressRelease.pressPos, 0.01);
        int id = attachTouchID(key);
        sendTouchDownEvent(id, processedPos);
        int delay = QRandomGenerator::global()->bounded(20, 30);
        QTimer::singleShot(delay, this, [this, key, processedPos]() {
            int id = getTouchID(key);
            sendTouchUpEvent(id, processedPos);
            detachIndexID(id);
        });
    } else if (QEvent::KeyRelease == from->type()) {
        QPointF processedPos = generatePos(node.data.pressRelease.releasePos, 0.01);
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
            wheelPos = generatePos(node.data.mobaWheel.wheelPos, 0.005);
            startPos = wheelPos;
            m_ctrlSteerWheel.mobaWheel.wheelPos = wheelPos;
            int id = attachTouchID(from->button());
            sendTouchDownEvent(id, wheelPos);
        }

        QPointF rawPos{from->localPos().x() / m_showSize.width(), from->localPos().y() / m_showSize.height()};
        QPointF rawDistance{rawPos - node.data.mobaWheel.centerPos};
        QPointF distance{rawDistance.x() / m_ctrlSteerWheel.mobaWheel.speedRatio, rawDistance.y() / m_ctrlSteerWheel.mobaWheel.speedRatio * m_showSizeRatio};
        QPointF endPos{wheelPos + distance};

        getDelayQueue(startPos, endPos, 1,
                      m_ctrlSteerWheel.delayData.queuePos,
                      m_ctrlSteerWheel.delayData.queueTimer);
        m_ctrlSteerWheel.delayData.pressedNum++;
        m_ctrlSteerWheel.delayData.timer->start(0);
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
        QPointF clickPos = generatePos(node.data.mobaSkill.keyNode.pos, 0.01);
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
        getDelayQueue(clickPos, endPos, 8, m_dragDelayData.queuePos, m_dragDelayData.queueTimer);
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
    QPointF processedPos = generatePos(pos, 0.01);
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
