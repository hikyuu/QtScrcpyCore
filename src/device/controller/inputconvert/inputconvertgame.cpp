#include <QApplicationStateChangeEvent>
#include <QCursor>
#include <QGuiApplication>
#include <QHash>
#include <QLine>
#include <QRandomGenerator>
#include <QTimeLine>
#include <QTimer>
#include <QtMath>
#include <Windows.h>
#include <xlocale>

#include "inputconvertgame.h"
#include "touchinjectoradapter.h"

InputConvertGame::InputConvertGame(Controller *controller) : InputConvertNormal(controller)
{
    m_touchManager = new TouchManager(controller, this);
    m_touchInjector = new TouchInjectorAdapter(m_touchManager, this);

    loopTimer = new QTimer(this);

    loopTimer->setInterval(8);
    loopTimer->setTimerType(Qt::PreciseTimer);
    connect(loopTimer, &QTimer::timeout, this, &InputConvertGame::onLoopTimer);
    loopTimer->start();

    //     m_ctrlSteerWheel.delayData.timer = new QTimer(this);
    //     m_ctrlSteerWheel.delayData.timer->setSingleShot(true);
    //     connect(m_ctrlSteerWheel.delayData.timer, &QTimer::timeout, this, &InputConvertGame::onSteerWheelTimer);

    m_ctrlMouseMove.resetMoveTimer.setSingleShot(true);
    connect(&m_ctrlMouseMove.resetMoveTimer, &QTimer::timeout, this, &InputConvertGame::onResetMoveTimer);

    m_wheelDelayData.timer = new QTimer(this);
    m_wheelDelayData.timer->setSingleShot(true);
    connect(m_wheelDelayData.timer, &QTimer::timeout, this, &InputConvertGame::onWheelScrollTimer);

    m_wheelDelayData.upTimer = new QTimer(this);
    m_wheelDelayData.upTimer->setSingleShot(true);
    connect(m_wheelDelayData.upTimer, &QTimer::timeout, this, &InputConvertGame::onWheelUpTimer);
}

InputConvertGame::~InputConvertGame() = default;

void InputConvertGame::mouseMove()
{

    if (!m_ctrlMouseMove.waitClick) {
        return;
    }

    if (m_ctrlMouseMove.lastConvertPos != m_ctrlMouseMove.processedPos) {
        return;
    }
    startMouseMoveTimer();
    mouseMove(m_ctrlMouseMove.processedPos);
    m_ctrlMouseMove.waitClick = false;
}

void InputConvertGame::stopAllInteractions(bool restoreCursor)
{
    // Keep this conservative: stop what we already stop today,
    // but centralize it so future refactors are safer.

    // 1) Timers first (avoid new events enqueued while we unwind touches)
    stopMouseMoveTimer();

    if (loopTimer) {
        //        qDebug()<<"停止loopTimer";
        //        loopTimer->stop();
    }

    if (m_dragDelayData.timer) {
        m_dragDelayData.timer->stop();
    }

    if (m_wheelDelayData.timer) {
        m_wheelDelayData.timer->stop();
    }
    if (m_wheelDelayData.upTimer) {
        m_wheelDelayData.upTimer->stop();
    }

    // 2) Release touches best-effort (keep per-feature ordering)

    // mouse-move touch (uses resetTouchID + detach/up ordering in existing code)
    if (m_ctrlMouseMove.touching) {
        const int id = m_touchInjector->idForKey(Qt::ExtraButton24);
        if (id >= 0) {
            m_ctrlMouseMove.touching = false;
            m_touchInjector->resetId(id, Qt::Key_unknown);
            const QPointF touchUpPos = m_ctrlMouseMove.lastConvertPos;
            m_touchInjector->detachId(id);
            m_touchInjector->upId(id, touchUpPos);
        }
    }

    // drag delay touch
    if (m_dragDelayData.pressKey != 0) {
        const int id = m_touchInjector->idForKey(m_dragDelayData.pressKey);
        if (id >= 0) {
            m_touchInjector->upId(id, m_dragDelayData.currentPos);
            m_touchInjector->detachId(id);
        }
        m_dragDelayData.currentPos = QPointF();
        m_dragDelayData.pressKey = 0;
        m_dragDelayData.dragDelayUpTime = 0;
    }
    m_dragDelayData.queuePos.clear();
    m_dragDelayData.queueTimer.clear();

    // wheel drag touches
    for (auto &item : m_wheelDelayData.dragData) {
        const int id = m_touchInjector->idForKey(item.pressKey);
        if (id >= 0) {
            m_touchInjector->upId(id, item.currentPos);
            m_touchInjector->detachId(id);
        }
    }
    m_wheelDelayData.dragData.clear();
    m_wheelDelayData.wheeling = false;

    // steer wheel touch
    if (m_ctrlSteerWheel.wheeling) {
        const int id = m_touchInjector->idForKey(m_ctrlSteerWheel.touchKey);
        if (id >= 0) {
            m_touchInjector->upId(id, m_ctrlSteerWheel.delayData.currentPos);
            m_touchInjector->detachId(id);
        }
        m_ctrlSteerWheel.wheeling = false;
    }

    // moba wheel touch state uses touchKey too
    if (m_ctrlSteerWheel.mobaWheel.mouseWheeling) {
        const int id = m_touchInjector->idForKey(m_ctrlSteerWheel.touchKey);
        if (id >= 0) {
            m_touchInjector->upId(id, m_ctrlSteerWheel.delayData.currentPos);
            m_touchInjector->detachId(id);
        }
        m_ctrlSteerWheel.mobaWheel.mouseWheeling = false;
        m_ctrlSteerWheel.mobaWheel.buttonPressed = false;
    }

    // burst click: stop scheduling and detach any remaining mapping (best-effort)
    for (const int key : std::as_const(m_burstClickKeySet)) {
        m_touchInjector->detachByKey(key);
    }
    m_burstClickKeySet.clear();

    // Also stop any wheel-click mapping
    m_touchInjector->detachByKey(QEvent::Wheel);

    if (restoreCursor) {
        hideMouseCursor(false);
    }
}

void InputConvertGame::activated(const bool isActive)
{
    if (m_gameMap) {
        if (!isActive) {
            m_pointerMode = true;
            stopAllInteractions(true);
        }
    }
    InputConvertNormal::activated(isActive);
}

void InputConvertGame::keyboard(void *pVoid)
{
    if (m_maskWidget.isNull()) {
        auto *pObject = static_cast<QObject *>(pVoid);
        QWidget *pWidget = qobject_cast<QWidget *>(pObject);
        if (!pWidget) {
            qDebug() << "窗口获取失败";
            return;
        }
        m_maskWidget = new MaskWidget(pWidget, &m_keyMap);
        m_maskWidget->setGeometry(pWidget->rect());
    }
    if (m_maskWidget->isVisible()) {
        m_maskWidget->hide();
    } else {
        m_maskWidget->show();
        m_maskWidget->updateMask();
    }
}

void InputConvertGame::prepareToDelete()
{
    stopAllInteractions(true);
    if (!m_maskWidget.isNull()) {
        m_maskWidget->hide();
        m_maskWidget->deleteLater();
        m_maskWidget = nullptr;
    }
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
            if (processMobaMouseClick(from)) {
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

            QPointF processedPos = generatePos(QPointF{ 0.96, 0.28 }, 0.01, 3);
            m_keyPosMap[QEvent::Wheel] = processedPos;

            const auto h = m_touchInjector->begin(QEvent::Wheel, processedPos);

            int delay = 0;
            int wheelDelay = 0;
            if (from->angleDelta().y() > 0) {
                qDebug() << "wheel up";
                delay = QRandomGenerator::global()->bounded(30, 50);
                wheelDelay = 500;
            } else {
                qDebug() << "wheel down";
                delay = QRandomGenerator::global()->bounded(300, 450);
                wheelDelay = 500;
            }
            QTimer::singleShot(delay, this, [this, h]() { m_touchInjector->end(h, m_keyPosMap[QEvent::Wheel]); });
            QTimer::singleShot(wheelDelay, this, [this]() { m_wheelDelayData.wheeling = false; });
        } else {
            if (m_wheelDelayData.wheeling) {
                return;
            }
            m_wheelDelayData.wheeling = true;
            QPointF pos = from->position();
            QPointF startPos = QPointF(pos.x() / showSize.width(), pos.y() / showSize.width());

            if (from->modifiers() & Qt::AltModifier) {

                QPointF leftStartPos = { qBound(0.001, startPos.x() - 0.01, 0.99), startPos.y() };
                QPointF rightStartPos = { qBound(0.001, startPos.x() + 0.01, 0.99), startPos.y() };
                QPointF leftEndPos = { qBound(0.0025, leftStartPos.x() - 0.025, 0.99), leftStartPos.y() };
                QPointF rightEndPos = { qBound(0.0025, rightStartPos.x() + 0.025, 0.99), rightStartPos.y() };

                if (from->angleDelta().x() > 0) {
                    //上滚放大，由中心向外移动
                    qDebug() << "wheel up" << leftStartPos << rightStartPos << leftEndPos << rightEndPos;
                } else {
                    //下滚缩小，由外向中心移动
                    std::swap(leftStartPos, leftEndPos);
                    std::swap(rightStartPos, rightEndPos);
                    qDebug() << "wheel down" << leftStartPos << rightStartPos << leftEndPos << rightEndPos;
                }

                QQueue<QPointF> leftQueuePos;
                QQueue<QPointF> rightQueuePos;
                QQueue<quint32> queueTimer;

                const int leftKey = Qt::AltModifier + Qt::Key_Zoom + Qt::Key_Left;
                const int rightKey = Qt::AltModifier + Qt::Key_Zoom + Qt::Key_Right;

                getCurvedDelayQueue(leftStartPos, leftEndPos, 30, leftQueuePos, queueTimer);
                m_wheelDelayData.dragData.append({ leftKey, leftStartPos, leftStartPos, leftQueuePos });

                getCurvedDelayQueue(rightStartPos, rightEndPos, 30, rightQueuePos, queueTimer);
                m_wheelDelayData.dragData.append({ rightKey, rightStartPos, rightStartPos, rightQueuePos });

                // move TouchManager dependency behind adapter
                const auto hLeft = m_touchInjector->begin(leftKey, leftStartPos);
                Q_UNUSED(hLeft);
                const auto hRight = m_touchInjector->begin(rightKey, rightStartPos);
                Q_UNUSED(hRight);

                m_wheelDelayData.stepTime = 8;
                m_wheelDelayData.wheelDelayUpTime = 200;
            } else {
                startPos.setY(0.5);
                QPointF endPos = startPos;
                if (from->angleDelta().y() > 0) {
                    endPos.setY(startPos.y() + generateDouble(0.49, 0.5));
                    qDebug() << "wheel up" << endPos;
                } else {
                    endPos.setY(startPos.y() - generateDouble(0.49, 0.5));
                    qDebug() << "wheel down" << endPos;
                }

                QQueue<QPointF> QueuePos;
                QQueue<quint32> queueTimer;
                getCurvedDelayQueue(startPos, endPos, 30, QueuePos, queueTimer);

                m_wheelDelayData.dragData.append({ QEvent::Wheel, startPos, startPos, QueuePos });

                const auto h = m_touchInjector->begin(QEvent::Wheel, startPos);
                Q_UNUSED(h);

                m_wheelDelayData.stepTime = 8;
                m_wheelDelayData.wheelDelayUpTime = 150;
            }
            m_wheelDelayData.timer->start(8);
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
    const KeyMap::KeyMapNode &node = getNode(from);

    // 处理开关按键
    if (m_keyMap.isSwitchOnKeyboard() && m_keyMap.getSwitchKey() == from->key()) {
        if (QEvent::KeyPress != from->type()) {
            return;
        }
        if (!switchGameMap()) {
            m_pointerMode = false;
            setMousePos(m_pointerMode, node);
        }
        return;
    }

    // 处理特殊按键：可以释放出鼠标的按键
    if (m_gameMap && node.switchMap) {
        updateSize(frameSize, showSize);
        switchMouse(node, from);
    }

    if (m_gameMap) {
        updateSize(frameSize, showSize);
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
        processAndroidKey(node.data.click.keyNode.androidKey, from->type());
        break;
    case KeyMap::KMT_CLICK_TWICE:
        processKeyClick(true, node, from);
        processAndroidKey(node.data.clickTwice.keyNode.androidKey, from->type());
        break;
    case KeyMap::KMT_CLICK_MULTI:
        processKeyClickMulti(
            node.data.clickMulti.keyNode.delayClickNodes, node.data.clickMulti.keyNode.delayClickNodesCount, node.data.clickMulti.pressTime, from);
        break;
    case KeyMap::KMT_DRAG:
        processKeyDrag(node.data.drag.keyNode.pos, node.data.drag.keyNode.extendPos, from);
        break;
    case KeyMap::KMT_ANDROID_KEY:
        processAndroidKey(node.data.androidKey.keyNode.androidKey, from->type());
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
        processKeyBoardBurstClick(node, from);
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
        m_ctrlSteerWheel.simulateWheel = m_keyMap.getSteerWheelMap().data.steerWheel.simulateWheel;
        m_ctrlSteerWheel.keepMove = m_keyMap.getSteerWheelMap().data.steerWheel.keepMove;
        m_ctrlSteerWheel.fixedStick = m_keyMap.getSteerWheelMap().data.steerWheel.fixedStick;
    }
    if (m_keyMap.isValidMobaWheel()) {
        m_ctrlSteerWheel.simulateWheel = m_keyMap.getSteerWheelMap().data.steerWheel.simulateWheel;
        m_ctrlSteerWheel.scaleRatio = m_keyMap.getSteerWheelMap().data.steerWheel.scaleRatio;
        m_ctrlSteerWheel.mobaWheel.skillOffset = m_keyMap.getSteerWheelMap().data.mobaWheel.skillOffset;
        m_ctrlSteerWheel.mobaWheel.characterCenterPos = m_keyMap.getMobaWheelMap().data.mobaWheel.centerPos;
        m_ctrlSteerWheel.mobaWheel.wheelCenterPos = m_keyMap.getMobaWheelMap().data.mobaWheel.wheelPos;
        m_ctrlSteerWheel.mobaWheel.speedRatio = m_keyMap.getMobaWheelMap().data.mobaWheel.speedRatio;

        qDebug() << "loadKeyMap scaleRatio:" << m_ctrlSteerWheel.scaleRatio;
        qDebug() << "steerWheel simulateWheel:" << m_ctrlSteerWheel.simulateWheel;

        m_ctrlSteerWheel.keepMove = m_keyMap.getSteerWheelMap().data.steerWheel.keepMove;
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
        m_touchManager->updateSize(frameSize);
        m_showSizeRatio = m_showSize.width() / m_showSize.height();
        m_ctrlMouseMove.centerPos = QPoint(m_showSize.width() / 2, m_showSize.height() / 2);
        qDebug() << "updateSize frameSize:" << m_frameSize << "showSize:" << m_showSize;
    }
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

// -------- steer wheel event --------
void InputConvertGame::getCurvedDelayQueue(const QPointF &start, const QPointF &end, int stepCount, QQueue<QPointF> &queuePos, QQueue<quint32> &queueTimer)
{
    QQueue<QPointF> queue;
    QQueue<quint32> queue2;

    QTimeLine timeline;
    timeline.setFrameRange(0, QRandomGenerator::global()->bounded(stepCount - 5, stepCount + 5)); // 500ms / 4ms = 125帧
    timeline.setEasingCurve(QEasingCurve::InOutQuad);                                             // 模拟加速度

    // 垂直偏移量（曲率控制因子）
    double offset = QLineF(start, end).length() * generateDouble(0.05, 0.1);

    // 计算起点到终点的方向向量
    double dx = end.x() - start.x();
    double dy = end.y() - start.y();

    // 计算垂直方向（始终指向坐标系上方）
    QPointF dir;
    if (dx >= 0) {              // 终点在起点右侧或相同x位置
        dir = QPointF(dy, -dx); // 逆时针旋转90度（指向坐标系上方）
    } else {                    // 终点在起点左侧
        dir = QPointF(-dy, dx); // 顺时针旋转90度（指向坐标系上方）
    }

    double len = std::hypot(dir.x(), dir.y());
    if (len > 0)
        dir /= len; // 单位化

    // 修改：在x方向0.3-0.7之间的随机位置设置控制点
    double ratio = generateDouble(0.2, 0.8);           // 生成0.3-0.7之间的随机比例
    QPointF basePoint = start + ratio * (end - start); // 在x方向的指定比例位置建立基准点
    QPointF ctrl1 = basePoint + dir * offset;          // 轻微上凸的控制点

    QPainterPath path;

    path.moveTo(start);
    path.quadTo(ctrl1, end); // 二次贝塞尔曲线

    for (int i = 1; i <= timeline.endFrame(); ++i) {

        qreal progress = timeline.easingCurve().valueForProgress(i / qreal(timeline.endFrame()));
        QPointF newPos = path.pointAtPercent(progress);

        queue.enqueue(newPos);
        queue2.enqueue(8);
    }
    queuePos = queue;
    queueTimer = queue2;
}

void InputConvertGame::onResetMoveTimer()
{

    m_ctrlMouseMove.resetMoveTimer.stop();
    mouseMoveStopTouch(false);

    if (!m_ctrlMouseMove.needResetTouch || m_pointerMode) {
        return;
    }

    if (m_ctrlMouseMove.startPos == m_ctrlMouseMove.lastConvertPos) {
        //        qDebug() << "reset move timer, but no move";
        return;
    }

    QTimer::singleShot(QRandomGenerator::global()->bounded(50, 100), this, [this]() { mouseMoveStartTouch(*new QPointF()); });

    m_ctrlMouseMove.resetMoveDelay = QRandomGenerator::global()->bounded(500, 1000);
    //    qDebug() << "reset move timer next delay:" << m_ctrlMouseMove.resetMoveDelay;
}

void InputConvertGame::onSteerWheelTimer()
{
    onWheelTimer(m_ctrlSteerWheel.touchKey);
}

void InputConvertGame::onLoopTimer()
{
    mouseMove();
    onSteerWheelTimer();
    processMobaMouseMoveInternal();
}

void InputConvertGame::onStopMobaWheelTimer()
{
    m_ctrlSteerWheel.delayData.queueTimer.clear();
    m_ctrlSteerWheel.delayData.queuePos.clear();
    if (!m_ctrlSteerWheel.mobaWheel.mouseWheeling) {
        return;
    }
    m_ctrlSteerWheel.mobaWheel.mouseWheeling = false;
    const int id = m_touchInjector->idForKey(m_ctrlSteerWheel.touchKey);
    m_touchInjector->upId(id, m_ctrlSteerWheel.delayData.currentPos);
    m_touchInjector->detachId(id);
}

void InputConvertGame::onWheelTimer(const int key)
{
    const int id = m_touchInjector->idForKey(key);
    if (!m_ctrlSteerWheel.delayData.queuePos.empty()) {
        m_ctrlSteerWheel.delayData.step++;
        const int headStep = m_ctrlSteerWheel.delayData.queueTimer.head();
        if (headStep > m_ctrlSteerWheel.delayData.step) {
            return;
        }
        m_ctrlSteerWheel.delayData.currentPos = m_ctrlSteerWheel.delayData.queuePos.dequeue();
        m_ctrlSteerWheel.delayData.queueTimer.dequeue();
        m_touchInjector->moveId(id, m_ctrlSteerWheel.delayData.currentPos);
        m_ctrlSteerWheel.delayData.step = 0;
    } else {
        m_ctrlSteerWheel.delayData.step = 0;
        if (m_ctrlSteerWheel.delayData.pressedNum == 0) {
            if (!m_ctrlSteerWheel.wheeling || m_ctrlSteerWheel.keepMove) {
                return;
            }
            if (m_ctrlSteerWheel.mobaWheel.mouseWheeling) {
                return;
            }
            m_touchInjector->upId(id, m_ctrlSteerWheel.delayData.currentPos);
            m_touchInjector->detachId(id);
            m_ctrlSteerWheel.wheeling = false;
            return;
        }
        if (m_ctrlSteerWheel.delayData.pressedNum > 1 && m_ctrlSteerWheel.simulateWheel) {
            // 如果有两个按键按下，则计算中间点
            QPointF startPos = m_ctrlSteerWheel.delayData.shakeEndPos;
            QPointF endPos = m_ctrlSteerWheel.delayData.middlePoint;
            if (m_ctrlSteerWheel.delayData.isEnd) {
                QPointF middlePoint = pointAtPercent(m_ctrlSteerWheel.centerPos + m_ctrlSteerWheel.delayData.offsetY, m_ctrlSteerWheel.delayData.endPos, 0.5);
                middlePoint = generatePos(middlePoint, 0.01, 2);
                endPos = middlePoint;
                m_ctrlSteerWheel.delayData.middlePoint = middlePoint;
            } else {
                const double distance = calcDistance(m_ctrlSteerWheel.centerPos, startPos);
                const double radius = distance * 0.1;
                startPos = generatePos(m_ctrlSteerWheel.delayData.endPos, radius, 2);
                m_ctrlSteerWheel.delayData.shakeEndPos = startPos;
                std::swap(startPos, endPos);
            }
            m_ctrlSteerWheel.delayData.path = generateLinePath(startPos, endPos);
            getDelayQueue(m_ctrlSteerWheel.delayData.queuePos, m_ctrlSteerWheel.delayData.queueTimer, false, 2, 2, 15, m_ctrlSteerWheel.delayData.path);
            m_ctrlSteerWheel.delayData.isEnd = !m_ctrlSteerWheel.delayData.isEnd;
        }
    }
}

// `processSteerWheel` implementation moved to steerwheel/steerwheel.cpp
// -------- key event --------

QPointF InputConvertGame::pointAtPercent(const QPointF &start, const QPointF &end, double percent)
{
    // 1. 计算方向向量（终点 - 起点）
    QPointF direction = end - start;

    // 2. 将方向向量缩放至90%长度
    QPointF scaledDirection = direction * percent;

    // 3. 将缩放向量叠加到起点，得到目标点
    QPointF result = start + scaledDirection;
    return result;
}

void InputConvertGame::updatePosition(const QPointF &newPos)
{
    m_ctrlSteerWheel.delayData.historyPoints.append(newPos);
    if (m_ctrlSteerWheel.delayData.historyPoints.size() > MAX_HISTORY) {
        m_ctrlSteerWheel.delayData.historyPoints.removeFirst(); // 移除最旧的点
    }
}

QPointF InputConvertGame::generatePos(QPointF pos, double radius = 0.01, double k = 3.0)
{
    // 生成[0,1]均匀分布的随机数
    double theta = QRandomGenerator::global()->generateDouble() * 2 * M_PI; // [0, 2π]

    // 非均匀半径生成（k>1时中心密度高）
    double u = QRandomGenerator::global()->generateDouble();
    double r = radius * std::pow(u, 1.0 / k);

    // 极坐标转笛卡尔坐标
    double x = pos.x() + r * std::cos(theta);
    double y = pos.y() + r * std::sin(theta);

    return { x, y };
}

void InputConvertGame::getDelayQueue(
    QQueue<QPointF> &queuePos,
    QQueue<quint32> &queueTimer,
    const bool detect,
    quint32 stepTimer,
    const quint32 randomTimer,
    int endFrame,
    const QPainterPath &path)
{
    QTimeLine timeline;

    if (detect) {
        timeline.setEasingCurve(QEasingCurve::OutCirc);
        endFrame = 60;
    }
    if (stepTimer < 1) {
        stepTimer = 1;
    }
    timeline.setFrameRange(0, endFrame);
    QQueue<QPointF> queue;
    QQueue<quint32> queue2;

    for (int i = 1; i <= timeline.endFrame(); ++i) {
        const qreal progress = timeline.easingCurve().valueForProgress(i / static_cast<qreal>(timeline.endFrame()));
        QPointF pos = path.pointAtPercent(progress);
        queue.enqueue(pos);

        if (randomTimer > 0) {
            const quint32 randomDelay = QRandomGenerator::global()->bounded(randomTimer);
            queue2.enqueue(stepTimer + randomDelay);
        } else {
            // 否则直接使用stepTimer
            queue2.enqueue(stepTimer);
        }
    }

    queuePos = queue;
    queueTimer = queue2;
}

QVector<QPointF> InputConvertGame::calculateControlPoints() const
{
    QVector<QPointF> ctrlPoints;
    const auto &historyPoints = m_ctrlSteerWheel.delayData.historyPoints;

    if (historyPoints.size() < 2)
        return ctrlPoints;

    // 情况1：有2个历史点（生成三次贝塞尔曲线）
    if (historyPoints.size() == 2) {
        const QPointF dir = historyPoints[1] - historyPoints[0]; // 方向向量
        const QPointF perpDir(-dir.y(), dir.x());                // 垂直向量（法线方向）

        auto randSignedOffset = [](const qreal min, const qreal max) -> qreal {
            qreal base = generateDouble(min, max);
            return QRandomGenerator::global()->bounded(2) ? base : -base; // 50% 概率取负值
        };
        // 随机化参数
        qreal alpha = generateDouble(0.3, 0.7);        // [0.3, 0.7)
        qreal gamma = generateDouble(0.3, 0.7);        // [0.3, 0.7)
        qreal betaScale = randSignedOffset(0.1, 0.3);  // [0.05, 0.15)
        qreal deltaScale = randSignedOffset(0.1, 0.3); // [0.05, 0.15)
        // 计算控制点（靠近连线）
        const QPointF control1 = historyPoints[0] + alpha * dir + betaScale * perpDir;
        const QPointF control2 = historyPoints[1] - gamma * dir + deltaScale * perpDir;
        ctrlPoints << control1 << control2 << historyPoints[1];
        return ctrlPoints;
    }

    // 情况2：有3个历史点（生成平滑曲线）
    // 控制点1：P1 + (P2 - P0) * k，保证切线连续
    QPointF tangent = historyPoints[2] - historyPoints[0];
    QPointF ctrl1 = historyPoints[1] + tangent * 0.5; // 0.2为平滑系数

    // 控制点2：P2 - (P2 - P1) * k，避免曲率突变
    QPointF dirPrev = historyPoints[1] - historyPoints[0];
    QPointF ctrl2 = historyPoints[2] - dirPrev * 0.5;

    ctrlPoints << ctrl1 << ctrl2 << historyPoints[2];
    return ctrlPoints;
}

QPainterPath InputConvertGame::generateLinePath(QPointF start, QPointF end)
{
    QPainterPath path;
    path.moveTo(start);
    path.lineTo(end);
    return path;
}

// 生成贝塞尔曲线路径（用于绘制）
QPainterPath InputConvertGame::generateBezierPath()
{
    QPainterPath path;
    if (m_ctrlSteerWheel.delayData.historyPoints.isEmpty())
        return path;

    path.moveTo(m_ctrlSteerWheel.delayData.historyPoints.first());

    if (m_ctrlSteerWheel.delayData.historyPoints.size() >= 3) {
        path.moveTo(m_ctrlSteerWheel.delayData.historyPoints[1]);
    }

    auto ctrlPoints = calculateControlPoints();

    // 根据控制点数量选择曲线类型
    switch (ctrlPoints.size()) {
    case 2: // 二次贝塞尔曲线
        path.quadTo(ctrlPoints[0], ctrlPoints[1]);
        break;
    case 3: // 三次贝塞尔曲线
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
        QPointF processedPos = generatePos(node.data.click.keyNode.pos, node.data.click.keyNode.radius);
        m_keyPosMap[from->key()] = processedPos;
        const auto h = m_touchInjector->begin(from->key(), processedPos);
        if (clickTwice) {
            m_touchInjector->end(h, processedPos);
        }
    } else if (QEvent::KeyRelease == from->type()) {
        if (clickTwice) {
            const auto h = m_touchInjector->begin(from->key(), m_keyPosMap[from->key()]);
            Q_UNUSED(h);
        }
        m_touchInjector->endByKey(from->key(), m_keyPosMap[from->key()]);
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
    bool newPointerMode;
    if (forceSwitchOn) {
        newPointerMode = true;
    } else if (forceSwitchOff) {
        newPointerMode = false;
    } else {
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

    for (int i = 0; i < count; i++) {
        if (nodes[i].delay > 0) {
            delay += QRandomGenerator::global()->bounded(nodes[i].delay, nodes[i].delay + 10);
        }
        QPointF clickPos = generatePos(nodes[i].pos, 0.01);
        QTimer::singleShot(delay, this, [this, key, clickPos]() {
            const auto h = m_touchInjector->begin(key, clickPos);
            Q_UNUSED(h);
        });
        //        qDebug() << "pressTime:" << pressTime;
        if (pressTime > 0) {
            delay += (int)pressTime;
        }
        if (nodes[i].pressTime > 0) {
            qDebug() << "pressTime:" << nodes[i].pressTime;
            delay += nodes[i].pressTime;
        }
        // Don't up it too fast
        delay += QRandomGenerator::global()->bounded(10, 20);

        QTimer::singleShot(delay, this, [this, key, clickPos]() { m_touchInjector->endByKey(key, clickPos); });
    }
}

void InputConvertGame::onDragTimer()
{
    if (m_dragDelayData.queuePos.empty()) {
        return;
    }
    int id = m_touchInjector->idForKey(m_dragDelayData.pressKey);
    m_dragDelayData.currentPos = m_dragDelayData.queuePos.dequeue();
    m_touchInjector->moveId(id, m_dragDelayData.currentPos);

    if (m_dragDelayData.queuePos.empty() && m_dragDelayData.allowUp) {
        delete m_dragDelayData.timer;
        m_dragDelayData.timer = nullptr;
        QTimer::singleShot(m_dragDelayData.dragDelayUpTime, this, [this, id]() {
            m_touchInjector->upId(id, m_dragDelayData.currentPos);
            m_touchInjector->detachId(id);
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
    for (auto &item : m_wheelDelayData.dragData) {
        if (item.queuePos.empty()) {
            continue;
        }
        int id = m_touchInjector->idForKey(item.pressKey);
        item.currentPos = item.queuePos.dequeue();
        m_touchInjector->moveId(id, item.currentPos);
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
}

void InputConvertGame::processKeyDrag(const QPointF &startPos, QPointF endPos, const QKeyEvent *from)
{
    if (QEvent::KeyPress == from->type()) {
        // stop last
        dragStop();
        // start this
        const int id = m_touchInjector->attachIdForKey(from->key());
        m_touchInjector->downId(id, startPos);

        m_dragDelayData.timer = new QTimer(this);
        m_dragDelayData.timer->setSingleShot(true);
        connect(m_dragDelayData.timer, &QTimer::timeout, this, &InputConvertGame::onDragTimer);
        m_dragDelayData.pressKey = from->key();
        m_dragDelayData.currentPos = startPos;
        m_dragDelayData.queuePos.clear();
        m_dragDelayData.queueTimer.clear();
        getCurvedDelayQueue(startPos, endPos, 20, m_dragDelayData.queuePos, m_dragDelayData.queueTimer);
        m_dragDelayData.timer->start();
    }
}

void InputConvertGame::onWheelUpTimer()
{

    for (auto &item : m_wheelDelayData.dragData) {
        int id = m_touchInjector->idForKey(item.pressKey);
        m_touchInjector->upId(id, item.currentPos);
        m_touchInjector->detachId(id);
        qDebug() << "WheelUp id:" << id;
    }

    m_wheelDelayData.dragData.clear();
    m_wheelDelayData.wheeling = false;
}

void InputConvertGame::dragStop()
{
    if (m_dragDelayData.timer && m_dragDelayData.timer->isActive()) {
        m_dragDelayData.timer->stop();
        delete m_dragDelayData.timer;
        m_dragDelayData.timer = nullptr;
        m_dragDelayData.queuePos.clear();
        m_dragDelayData.queueTimer.clear();

        int id = m_touchInjector->idForKey(m_dragDelayData.pressKey);
        m_touchInjector->upId(id, m_dragDelayData.currentPos);
        m_touchInjector->detachId(id);
        m_dragDelayData.currentPos = QPointF();
        m_dragDelayData.pressKey = 0;
    }
}

void InputConvertGame::processAndroidKey(AndroidKeycode androidKey, QEvent::Type type)
{
    if (AKEYCODE_UNKNOWN == androidKey) {
        return;
    }
    AndroidKeyeventAction action;
    switch (type) {
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

bool InputConvertGame::processMouseClick(const QMouseEvent *from)
{
    const KeyMap::KeyMapNode node = m_keyMap.getKeyMapNodeMouse(from->button());
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
    case KeyMap::KMT_CLICK: {
        if (m_pointerMode) {
            return false;
        }
        processMouseClick(from, node);
        return true;
    }
    case KeyMap::KMT_ANDROID_KEY: {
        processAndroidKey(node.data.androidKey.keyNode.androidKey, from->type());
        return true;
    }
    case KeyMap::KMT_CLICK_MULTI: {
        if (QEvent::MouseButtonPress != from->type()) {
            return true;
        }
        int button = from->button();
        int delay = 0;
        const KeyMap::DelayClickNode *nodes = node.data.clickMulti.keyNode.delayClickNodes;
        const int count = node.data.clickMulti.keyNode.delayClickNodesCount;
        for (int i = 0; i < count; i++) {
            delay += QRandomGenerator::global()->bounded(nodes[i].delay, nodes[i].delay + 10);
            delay += nodes[i].delay;
            QPointF clickPos = generatePos(nodes[i].pos, 0.02);
            QTimer::singleShot(delay, this, [this, button, clickPos]() {
                const auto h = m_touchInjector->begin(button, clickPos);
                Q_UNUSED(h);
            });

            // Don't up it too fast
            delay += QRandomGenerator::global()->bounded(20, 30);

            QTimer::singleShot(delay, this, [this, button, clickPos]() { m_touchInjector->endByKey(button, clickPos); });
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

void InputConvertGame::processMouseClick(const QMouseEvent *from, const KeyMap::KeyMapNode &node)
{
    if (QEvent::MouseButtonPress == from->type() || QEvent::MouseButtonDblClick == from->type()) {
        m_keyPosMap[from->button()] = generatePos(node.data.click.keyNode.pos, node.data.click.keyNode.radius);
        const auto h = m_touchInjector->begin(from->button(), m_keyPosMap[from->button()]);
        Q_UNUSED(h);
    }
    if (QEvent::MouseButtonRelease == from->type()) {
        m_touchInjector->endByKey(from->button(), m_keyPosMap[from->button()]);
        if (node.freshMouseMove) {
            stopMouseMoveTimer();
            mouseMoveStopTouch(false);
        }
    }
}

bool InputConvertGame::processCustomMouseClick(const QMouseEvent *from)
{
    if (QEvent::MouseButtonPress == from->type() || QEvent::MouseButtonDblClick == from->type()) {
        //        qDebug() << "customMouseClick "<<from->button();
        if (from->button() == Qt::LeftButton) {
            if (from->modifiers() & Qt::ControlModifier && !m_keyMap.isValidMobaWheel()) {
                QPointF pos = from->localPos();
                int key = Qt::ControlModifier + Qt::LeftButton;
                // convert pos
                pos.setX(pos.x() / m_showSize.width());
                pos.setY(pos.y() / m_showSize.height());
                int delay = 0;
                int clickTime = 2;
                for (int i = 0; i < clickTime; i++) {
                    QPointF clickPos = generatePos(pos, 0.005);
                    QTimer::singleShot(delay, this, [this, key, clickPos]() {
                        const int id = m_touchInjector->attachIdForKey(key);
                        m_touchInjector->downId(id, clickPos);
                    });
                    // Don't up it too fast
                    delay += QRandomGenerator::global()->bounded(10, 20);
                    if (i >= clickTime - 1) {
                        delay = delay * 2;
                    }
                    QTimer::singleShot(delay, this, [this, key, clickPos]() {
                        const int id = m_touchInjector->idForKey(key);
                        m_touchInjector->upId(id, clickPos);
                        m_touchInjector->detachId(id);
                    });
                    delay += QRandomGenerator::global()->bounded(10, 20);
                }
                return true;
            }
            const int id = m_touchInjector->attachIdForKey(from->button());
            // pos
            QPointF pos = from->localPos();
            // convert pos
            pos.setX(pos.x() / m_showSize.width());
            pos.setY(pos.y() / m_showSize.height());
            m_keyPosMap[from->button()] = pos;
            m_touchInjector->downId(id, pos);
            qDebug() << "left button click ";
            return true;
        }
        if (from->button() == Qt::RightButton) {
            if (from->modifiers() & Qt::ControlModifier && !m_keyMap.isValidMobaWheel()) {
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
                int id = m_touchInjector->attachIdForKey(Qt::Key_Control + Qt::RightButton);
                m_touchInjector->downId(id, pos);

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
                getCurvedDelayQueue(pos, endPos, 25, m_dragDelayData.queuePos, m_dragDelayData.queueTimer);
                m_dragDelayData.timer->start(0);
                return true;
            }
            qDebug() << "right button click";
            processAndroidKey(AKEYCODE_BACK, QEvent::KeyPress);
            const int delay = QRandomGenerator::global()->bounded(30, 50);
            QTimer::singleShot(delay, this, [this]() { processAndroidKey(AKEYCODE_BACK, QEvent::KeyRelease); });
        }
    }
    if (QEvent::MouseMove == from->type()) {
        if (!(from->buttons() & Qt::LeftButton)) {
            return false;
        }
        const int id = m_touchInjector->idForKey(Qt::LeftButton);
        // pos
        QPointF pos = from->localPos();
        // convert pos
        pos.setX(pos.x() / m_showSize.width());
        pos.setY(pos.y() / m_showSize.height());
        m_keyPosMap[Qt::LeftButton] = pos;
        m_touchInjector->moveId(id, m_keyPosMap[Qt::LeftButton]);
        return true;
    }
    if (QEvent::MouseButtonRelease == from->type()) {
        if (from->button() == Qt::LeftButton) {
            if (from->modifiers() & Qt::ControlModifier && !m_keyMap.isValidMobaWheel()) {
                return true;
            }
            const int id = m_touchInjector->idForKey(from->button());
            m_touchInjector->upId(id, m_keyPosMap[from->button()]);
            m_touchInjector->detachId(id);
        }
        return true;
    }
    return true;
}

double InputConvertGame::generateDouble(double min, double max)
{
    return QRandomGenerator::global()->generateDouble() * (max - min) + min;
}

bool InputConvertGame::mouseMove(const QPointF &currentConvertPos)
{
    if (m_ctrlMouseMove.touching) {
        m_ctrlMouseMove.lastConvertPos = currentConvertPos;
        m_touchInjector->moveId(m_ctrlMouseMove.focusTouchID, currentConvertPos);
    }
    return true;
}

bool InputConvertGame::checkBoundary(const QPointF &currentConvertPos) const
{
    if (!m_ctrlMouseMove.needResetTouch) {
        return false;
    }
    return currentConvertPos.x() > m_ctrlMouseMove.maxBoundary || currentConvertPos.x() < m_ctrlMouseMove.leftBoundary
           || currentConvertPos.y() < m_ctrlMouseMove.topBoundary || currentConvertPos.y() > m_ctrlMouseMove.maxBoundary;
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
        moveCursorTo(from, m_ctrlMouseMove.centerPos);
        m_ctrlMouseMove.lastPos = m_ctrlMouseMove.centerPos;
        return outOfBoundary;
    }
    //    qDebug() << "mouse move next out of boundary pos:" << pos;
    return outOfBoundary;
}

bool InputConvertGame::checkOutOfBoundary(const QPoint &pos, int oneOfSevenWidth, int oneOfSevenHeight) const
{
    bool outOfBoundary = false;
    if (pos.x() < oneOfSevenWidth * 2) {
        outOfBoundary = true;
    } else if (pos.x() > m_showSize.width() - oneOfSevenWidth * 2) {
        outOfBoundary = true;
    } else if (pos.y() < oneOfSevenHeight * 2) {
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
        m_ctrlMouseMove.touching = true;
        QPointF mouseMoveStartPos;
        if (pos.isNull()) {
            mouseMoveStartPos = generatePos(m_keyMap.getMouseMoveMap().data.mouseMove.startPos, 0.025);
        } else {
            mouseMoveStartPos = pos;
        }
        const int id = m_touchInjector->attachIdForKey(Qt::ExtraButton24);
        m_ctrlMouseMove.focusTouchID = id;
        m_ctrlMouseMove.startPos = mouseMoveStartPos;
        m_ctrlMouseMove.lastConvertPos = mouseMoveStartPos;
        m_touchInjector->downId(id, mouseMoveStartPos);
        return true;
    }
    return false;
}

void InputConvertGame::mouseMoveStopTouch(bool delay)
{
    if (!m_ctrlMouseMove.touching) {
        qDebug() << "mouse move stop concurrent: no touching";
        return;
    }
    int id = m_touchInjector->idForKey(Qt::ExtraButton24);
    if (id < 0)
        return;

    m_ctrlMouseMove.touching = false;
    m_touchInjector->resetId(id, Qt::Key_unknown);
    QPointF touchUpPos = m_ctrlMouseMove.lastConvertPos;
    if (delay) {
        QTimer::singleShot(QRandomGenerator::global()->bounded(50, 100), this, [this, id, touchUpPos]() {
            m_touchInjector->detachId(id);
            m_touchInjector->upId(id, touchUpPos);
        });
    } else {
        m_touchInjector->detachId(id);
        m_touchInjector->upId(id, touchUpPos);
    }
}

void InputConvertGame::startMouseMoveTimer()
{
    m_ctrlMouseMove.resetMoveTimer.start(m_ctrlMouseMove.resetMoveDelay);
}

void InputConvertGame::stopMouseMoveTimer()
{
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
        const QPoint globalPos = QCursor::pos();                                         // 获取屏幕全局坐标
        const QPoint windowPos = QApplication::activeWindow()->mapFromGlobal(globalPos); // 转换为当前窗口坐标
        m_ctrlMouseMove.lastPos = windowPos;
    } else {
        stopMouseMoveTimer();
        mouseMoveStopTouch(false);
    }
    return m_gameMap;
}

void InputConvertGame::hideMouseCursor(const bool hide)
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
        rect.top = topLeft.y() + m_showSize.height() / 2;
        rect.left = topLeft.x() + m_showSize.width() / 2;
        rect.bottom = topLeft.y() + m_showSize.height() / 2 + 1;
        rect.right = topLeft.x() + m_showSize.width() / 2 + 1;
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
        // loopTimer->stop();
    } else {
        QCursor::setPos(QApplication::activeWindow()->mapToGlobal(m_ctrlMouseMove.centerPos));
        m_ctrlMouseMove.lastPos = m_ctrlMouseMove.centerPos;
    }
}

void InputConvertGame::processRotaryTable(const KeyMap::KeyMapNode &node, const QKeyEvent *const from)
{
    int key = Qt::ExtraButton24 + from->key();
    if (m_keyMap.isValidMouseMoveMap()) {
        if (QEvent::KeyPress == from->type()) {
            int delay = node.data.rotaryTable.delay;
            QPointF pos = generatePos(node.data.rotaryTable.keyNode.pos, node.data.rotaryTable.keyNode.radius);
            m_keyPosMap[key] = pos;
            const auto h = m_touchInjector->begin(key, pos);

            QTimer::singleShot(delay, this, [this, node, key, pos]() {
                const int id = m_touchInjector->idForKey(key);
                if (id < 0)
                    return;
                mouseMoveStopTouch(true);
                m_ctrlMouseMove.touching = true;
                m_ctrlMouseMove.focusTouchID = id;
                m_ctrlMouseMove.startPos = pos;
                m_ctrlMouseMove.lastConvertPos = pos;
                QPointF speedRatio = node.data.rotaryTable.speedRatio;
                m_currentSpeedRatio = speedRatio;
            });
        } else {
            const int id = m_touchInjector->idForKey(key);
            if (id < 0)
                return;
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

            // Preserve original ordering: detach then up.
            m_touchInjector->detachId(id);
            m_touchInjector->upId(id, touchUpPos);
            m_touchInjector->resetId(id, Qt::Key_unknown);
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
        const auto h = m_touchInjector->begin(key, processedPos);
        Q_UNUSED(h);
        int delay = QRandomGenerator::global()->bounded(20, 30);
        QTimer::singleShot(delay, this, [this, key, processedPos]() { m_touchInjector->endByKey(key, processedPos); });
    } else if (QEvent::KeyRelease == from->type()) {
        QPointF processedPos = generatePos(node.data.pressRelease.releasePos, 0.01);
        const auto h = m_touchInjector->begin(key, processedPos);
        Q_UNUSED(h);
        int delay = QRandomGenerator::global()->bounded(20, 30);
        QTimer::singleShot(delay, this, [this, key, processedPos]() { m_touchInjector->endByKey(key, processedPos); });
        if (node.switchMap) {
            switchMouse(node, false, true);
        }
    }
}

bool InputConvertGame::processMobaWheel(const QMouseEvent *from)
{
    KeyMap::KeyMapNode node = m_keyMap.getKeyMapNodeMouse(from->button());
    m_ctrlSteerWheel.touchKey = from->button();
    m_ctrlSteerWheel.mobaWheel.localPos = from->localPos();
    //取消技能释放
    if (from->button() == node.data.mobaWheel.cancelSkill.key && QEvent::MouseButtonPress == from->type()) {
        if (m_ctrlSteerWheel.mobaWheel.skillPressed) {
            m_ctrlSteerWheel.mobaWheel.skillPressed = false;
            m_ctrlSteerWheel.mobaWheel.quickCast = false;
            if (m_dragDelayData.timer && m_dragDelayData.timer->isActive()) {
                m_dragDelayData.timer->stop();
                m_dragDelayData.queueTimer.clear();
                m_dragDelayData.queuePos.clear();
            }
            m_dragDelayData.allowUp = true;
            m_dragDelayData.path = generateLinePath(m_dragDelayData.currentPos, node.data.mobaWheel.cancelSkill.pos);
            getDelayQueue(m_dragDelayData.queuePos, m_dragDelayData.queueTimer, false, 1, 0, 3, m_dragDelayData.path);
            m_dragDelayData.dragDelayUpTime = 50;
            m_dragDelayData.timer->start();
        }
        return true;
    }

    if (QEvent::MouseButtonPress == from->type()) {
        QPointF wheelPos;
        m_ctrlSteerWheel.mobaWheel.mouseWheeling = true;
        m_ctrlSteerWheel.mobaWheel.buttonPressed = true;
        QPointF localPos = from->localPos();
        if (m_ctrlSteerWheel.wheeling) {
            m_ctrlSteerWheel.wheeling = false;
            m_ctrlSteerWheel.delayData.queueTimer.clear();
            m_ctrlSteerWheel.delayData.queuePos.clear();
            m_ctrlSteerWheel.mobaWheel.localPos = localPos;
        } else {
            const int id = m_touchInjector->attachIdForKey(m_ctrlSteerWheel.touchKey);
            m_touchInjector->downId(id, m_ctrlSteerWheel.mobaWheel.wheelCenterPos);
            qDebug() << "moba wheel down id:" << id << " pos:" << m_ctrlSteerWheel.mobaWheel.wheelCenterPos;
            m_ctrlSteerWheel.mobaWheel.localPos = localPos;
        }
        m_mobaMouseMovePending = true;
        return true;
    }
    if (QEvent::MouseButtonRelease == from->type()) {
        m_ctrlSteerWheel.mobaWheel.buttonPressed = false;
        //        QPointF rawPos{from->localPos().x() / m_showSize.width(), from->localPos().y() / m_showSize.height()};
        //        double distance = calcDistance(rawPos, node.data.mobaWheel.characterCenterPos);
        //        double delay = distance * 3500;
        stopMobaWheel();
        return true;
    }
    return true;
}

void InputConvertGame::stopMobaWheel()
{

    m_ctrlSteerWheel.delayData.queuePos.clear();
    m_ctrlSteerWheel.delayData.queueTimer.clear();

    if (m_ctrlSteerWheel.delayData.pressedNum > 0) {
        m_ctrlSteerWheel.mobaWheel.mouseWheeling = false;
        if (m_ctrlSteerWheel.wheeling) {
            return;
        }
        m_ctrlSteerWheel.wheeling = true;
        m_ctrlSteerWheel.delayData.path = generateLinePath(m_ctrlSteerWheel.delayData.currentPos, m_ctrlSteerWheel.delayData.endPos);
        getDelayQueue(m_ctrlSteerWheel.delayData.queuePos, m_ctrlSteerWheel.delayData.queueTimer, false, 1, 0, 3, m_ctrlSteerWheel.delayData.path);
    } else {
        onStopMobaWheelTimer();
    }
}

double InputConvertGame::calcDistance(const QPointF &point1, const QPointF &point2)
{
    return std::sqrt(std::pow(point2.x() - point1.x(), 2) + std::pow(point2.y() - point1.y(), 2));
}

bool InputConvertGame::processMobaMouseClick(const QMouseEvent *from)
{
    const KeyMap::KeyMapNode node = m_keyMap.getKeyMapNodeMouse(from->button());
    if (KeyMap::KMT_INVALID == node.type) {
        return false;
    }
    const KeyMap::KeyMapType type = node.type;
    if (from->modifiers() & Qt::ControlModifier) {
        processCustomMouseClick(from);
        return true;
    }
    switch (type) {
    case KeyMap::KMT_CLICK: {
        processMouseClick(from, node);
        return true;
    }
    case KeyMap::KMT_BURST_CLICK: {
        bool press = false;
        if (QEvent::MouseButtonPress == from->type() || QEvent::MouseButtonDblClick == from->type()) {
            press = true;
        } else if (QEvent::MouseButtonRelease == from->type()) {
            press = false;
        } else {
            return false;
        }
        processBurstClick(node, from->button(), press);
        return true;
    }
    default:
        return false;
    }
    return false;
}

bool InputConvertGame::processMobaMouseMove(const QMouseEvent *from)
{
    if (QEvent::MouseMove != from->type()) {
        return false;
    }
    m_ctrlSteerWheel.mobaWheel.localPos = from->localPos();
    m_mobaMouseMovePending = true;
    return true;
}

void InputConvertGame::processMobaMouseMoveInternal()
{
    if (!m_mobaMouseMovePending) {
        return;
    }
    m_mobaMouseMovePending = false;
    // 处理技能
    if (m_ctrlSteerWheel.mobaWheel.skillPressed && !m_ctrlSteerWheel.mobaWheel.quickCast) {
        const double skillRatio = m_ctrlSteerWheel.mobaWheel.skillRatio;
        if (m_dragDelayData.timer->isActive()) {
            m_dragDelayData.timer->stop();
            m_dragDelayData.queueTimer.clear();
            m_dragDelayData.queuePos.clear();
        }

        const QPointF rawPos{ m_ctrlSteerWheel.mobaWheel.localPos.x() / m_showSize.width(), m_ctrlSteerWheel.mobaWheel.localPos.y() / m_showSize.height() };

        const QPointF distance = calcPerspectiveSkillDistance(rawPos, m_ctrlSteerWheel.mobaWheel.characterCenterPos, skillRatio);

        const QPointF endPos{ m_dragDelayData.startPos + distance };
        const int touchId = m_touchInjector->idForKey(m_dragDelayData.pressKey);
        m_touchInjector->moveId(touchId, endPos);
        m_dragDelayData.currentPos = endPos;
        return;
    }
    // 处理轮盘
    if (m_ctrlSteerWheel.mobaWheel.buttonPressed) {

        const double speedRatio = m_ctrlSteerWheel.mobaWheel.speedRatio;

        const QPointF rawPos{ m_ctrlSteerWheel.mobaWheel.localPos.x() / m_showSize.width(), m_ctrlSteerWheel.mobaWheel.localPos.y() / m_showSize.height() };
        const QPointF distance = calcPerspectiveSkillDistance(rawPos, m_ctrlSteerWheel.mobaWheel.characterCenterPos, speedRatio);
        const QPointF endPos{ m_ctrlSteerWheel.mobaWheel.wheelCenterPos + distance };

        m_ctrlSteerWheel.mobaWheel.endPos = endPos;

        if (!m_ctrlSteerWheel.mobaWheel.mouseWheeling) {
            return;
        }

        m_ctrlSteerWheel.delayData.queueTimer.clear();
        m_ctrlSteerWheel.delayData.queuePos.clear();
        m_ctrlSteerWheel.delayData.currentPos = endPos;
        const int touchId = m_touchInjector->idForKey(m_ctrlSteerWheel.touchKey);
        //        qDebug()<< "moba wheel move to endPos:" << endPos<<"id:" << touchId;
        m_touchInjector->moveId(touchId, endPos);
    }
}

void InputConvertGame::processMobaSkill(const KeyMap::KeyMapNode &node, const QKeyEvent *from)
{
    bool quickCast = node.data.mobaSkill.quickCast;
    int key = from->key();
    qDebug() << "quickCast:" << quickCast;
    if (QEvent::KeyPress == from->type()) {
        if (node.data.mobaSkill.stopMove) {
            stopMobaWheel();
        }
        m_ctrlSteerWheel.mobaWheel.quickCast = quickCast;
        m_ctrlSteerWheel.mobaWheel.skillPressed = true;
        const double skillRatio = node.data.mobaSkill.skillRatio;

        m_ctrlSteerWheel.mobaWheel.skillRatio = skillRatio;
        const int id = m_touchInjector->attachIdForKey(from->key());
        const QPointF clickPos = node.data.mobaSkill.keyNode.pos;
        m_dragDelayData.startPos = clickPos;

        m_touchInjector->downId(id, clickPos);
        if (quickCast) {
            QTimer::singleShot(30, this, [this, id, key] {
                m_touchInjector->upId(id, m_dragDelayData.startPos);
                m_touchInjector->detachId(id);
                qDebug() << "key:" << key;
                m_ctrlSteerWheel.mobaWheel.skillPressed = false;
            });
            return;
        }

        const QPointF &localPos = m_ctrlSteerWheel.mobaWheel.localPos;
        const QPointF rawPos{ localPos.x() / m_showSize.width(), localPos.y() / m_showSize.height() };
        const QPointF distance = calcPerspectiveSkillDistance(rawPos, m_ctrlSteerWheel.mobaWheel.characterCenterPos, skillRatio);
        const QPointF endPos{ m_dragDelayData.startPos + distance };

        m_dragDelayData.allowUp = false;
        m_dragDelayData.timer = new QTimer(this);
        m_dragDelayData.timer->setSingleShot(true);
        connect(m_dragDelayData.timer, &QTimer::timeout, this, &InputConvertGame::onDragTimer);
        m_dragDelayData.pressKey = from->key();
        m_dragDelayData.queuePos.clear();
        m_dragDelayData.queueTimer.clear();

        m_dragDelayData.path = generateLinePath(clickPos, endPos);
        getDelayQueue(m_dragDelayData.queuePos, m_dragDelayData.queueTimer, false, 1, 0, 2, m_dragDelayData.path);
        m_dragDelayData.timer->start();
    } else if (QEvent::KeyRelease == from->type() && !quickCast) {
        delete m_dragDelayData.timer;
        m_dragDelayData.timer = nullptr;
        m_dragDelayData.allowUp = true;
        const int id = m_touchInjector->idForKey(from->key());
        // Preserve original ordering: up then detach.
        m_touchInjector->upId(id, m_dragDelayData.currentPos);
        m_touchInjector->detachId(id);
        m_ctrlSteerWheel.mobaWheel.skillPressed = false;
    }
}

void InputConvertGame::processKeyBoardBurstClick(const KeyMap::KeyMapNode &node, const QKeyEvent *from)
{
    bool press = false;
    if (QEvent::KeyPress == from->type()) {
        press = true;
    } else if (QEvent::KeyRelease == from->type()) {
        press = false;
    } else {
        return;
    }
    processBurstClick(node, from->key(), press);
}

void InputConvertGame::processBurstClick(const KeyMap::KeyMapNode &node, const int key, const bool press)
{
    if (press) {
        int clickInterval = 1000 / node.data.burstClick.rate;
        if (clickInterval < 100) {
            clickInterval = 100;
        }
        m_burstClickKeySet.insert(key);
        cycleClick(node.data.burstClick.keyNode.pos, clickInterval, key);
    } else {
        m_burstClickKeySet.remove(key);
    }
}

void InputConvertGame::cycleClick(QPointF pos, int clickInterval, int key)
{
    QPointF processedPos = generatePos(pos, 0.01);

    if (!m_ctrlSteerWheel.mobaWheel.skillPressed) {
        m_touchInjector->begin(key, pos);
    }

    int delay = QRandomGenerator::global()->bounded(10, 20);
    int nextClick = clickInterval - delay;
    QTimer::singleShot(delay, this, [this, key, processedPos, pos, nextClick, clickInterval]() {
        m_touchInjector->endByKey(key, processedPos);
        if (!m_burstClickKeySet.contains(key)) {
            return;
        }
        QTimer::singleShot(nextClick, this, [this, key, pos, clickInterval]() { cycleClick(pos, clickInterval, key); });
    });
}

KeyMap::KeyMapNode InputConvertGame::getNode(const QKeyEvent *from)
{
    if (from->key() == Qt::Key_Control || from->key() == Qt::Key_Shift || from->key() == Qt::Key_Alt) {
        return m_keyMap.getKeyMapNodeKey(from->key());
    }
    if (from->modifiers() & Qt::ControlModifier) {
        KeyMap::KeyMapNode node = m_keyMap.getKeyMapNodeKey(from->key() + Qt::ControlModifier);
        if (node.type != -1) {
            return node;
        }
    }
    if (from->modifiers() & Qt::ShiftModifier) {
        KeyMap::KeyMapNode node = m_keyMap.getKeyMapNodeKey(from->key() + Qt::ShiftModifier);
        if (node.type != -1) {
            return node;
        }
    }
    if (from->modifiers() & Qt::AltModifier) {
        KeyMap::KeyMapNode node = m_keyMap.getKeyMapNodeKey(from->key() + Qt::AltModifier);
        if (node.type != -1) {
            return node;
        }
    }
    return m_keyMap.getKeyMapNodeKey(from->key());
}
