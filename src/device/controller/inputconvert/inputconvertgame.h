#ifndef INPUTCONVERTGAME_H
#define INPUTCONVERTGAME_H

#include <QApplication>
#include <QPointF>
#include <QQueue>
#include <QScreen>
#include <QWidget>

#include "inputconvertnormal.h"
#include "keymap.h"

#define MULTI_TOUCH_MAX_NUM 10
class InputConvertGame : public InputConvertNormal {
Q_OBJECT
public:
    InputConvertGame(Controller *controller);

    virtual ~InputConvertGame();

    virtual void mouseEvent(const QMouseEvent *from, const QSize &frameSize, const QSize &showSize);

    virtual void wheelEvent(const QWheelEvent *from, const QSize &frameSize, const QSize &showSize);

    virtual void keyEvent(const QKeyEvent *from, const QSize &frameSize, const QSize &showSize);

    virtual bool isCurrentCustomKeymap();

    void loadKeyMap(const QString &json);

protected:
    void updateSize(const QSize &frameSize, const QSize &showSize);

    void sendTouchDownEvent(int id, QPointF pos);

    void sendTouchMoveEvent(int id, QPointF pos);

    void sendTouchUpEvent(int id, QPointF pos);

    void sendTouchEvent(int id, QPointF pos, AndroidMotioneventAction action);

    void sendKeyEvent(AndroidKeyeventAction action, AndroidKeycode keyCode);

    QPointF calcFrameAbsolutePos(QPointF relativePos);

    QPointF calcScreenAbsolutePos(QPointF relativePos);

    // multi touch id
    int attachTouchID(int key);

    void detachTouchID(int key);

    int getTouchID(int key);

    // steer wheel
    void processSteerWheel(const KeyMap::KeyMapNode &node, const QKeyEvent *from);

    // click
    void processKeyClick(bool clickTwice, const KeyMap::KeyMapNode &node, const QKeyEvent *from);

    // click mutil
    void processKeyClickMulti(const KeyMap::DelayClickNode *nodes, const int count, const QKeyEvent *from);

    // drag
    void processKeyDrag(const QPointF &startPos, QPointF endPos, const QKeyEvent *from);

    // android key
    void processAndroidKey(AndroidKeycode androidKey, const QEvent *from);

    // mouse
    bool processMouseClick(const QMouseEvent *from);

    bool processMouseMove(const QMouseEvent *from);

    void moveCursorTo(const QMouseEvent *from, const QPoint &localPosPixel);

    void mouseMoveStartTouch(const QMouseEvent *from, const QPointF pos);

    void mouseMoveStopTouch();

    void startMouseMoveTimer();

    void stopMouseMoveTimer();

    bool switchGameMap();

    bool checkCursorPos(const QMouseEvent *from);

    void hideMouseCursor(bool hide);

    void getDelayQueue(
            const QPointF &start,
            const QPointF &end,
            const double &distanceStep,
            const double &posStepconst,
            quint32 lowestTimer,
            quint32 highestTimer,
            QQueue<QPointF> &queuePos,
            QQueue<quint32> &queueTimer);

signals:

    void mouseCursorHided(bool hide);

protected:
    void timerEvent(QTimerEvent *event);

private slots:

    void onSteerWheelTimer();

    void onDragTimer();

private:
    QSize m_frameSize;
    QSize m_showSize;
    double m_showSizeRatio;
    bool m_gameMap = false;
    //准心模式鼠标移动镜头
    bool m_needBackMouseMove = false;
    int m_multiTouchID[MULTI_TOUCH_MAX_NUM] = {0};
    KeyMap m_keyMap;
    QMap<int, QPointF> m_keyPosMap;
    QSet<int> m_burstClickKeySet;
    QPointF m_currentSpeedRatio;
    bool m_processMouseMove = true;
    // steer wheel
    struct {
        // the first key wheelPressed
        int touchKey = Qt::Key_unknown;
        bool pressedUp = false;
        bool pressedDown = false;
        bool pressedLeft = false;
        bool pressedRight = false;
        bool clickMode = false;
        QPointF clickPos;
        // for delay
        struct {
            QPointF currentPos;
            QTimer *timer = nullptr;
            QQueue<QPointF> queuePos;
            QQueue<quint32> queueTimer;
            int pressedNum = 0;
            QPointF endPos;
        } delayData;
        struct{
            double speedRatio;
            double skillRatio;
            QPointF centerPos;
            QPointF wheelPos;
            bool wheelPressed = false;
            bool skillPressed = false;
            QPointF localPos;
            double skillOffset;
        } mobaWheel;
    } m_ctrlSteerWheel;
    // mouse move
    struct
    {
        QPointF lastConvertPos;
        QPointF lastPos = { 0.0, 0.0 };
        bool touching = false;
        int timer = 0;
        bool needResetTouch = true;
        bool smallEyes = false;
    } m_ctrlMouseMove;

    // for drag delay
    struct
    {
        QPointF startPos;
        QPointF currentPos;
        QTimer *timer = nullptr;
        QQueue<QPointF> queuePos;
        QQueue<quint32> queueTimer;
        int pressKey = 0;
        bool wheeling = false;
        int wheelDelayUpTime = 0;
        bool allowUp = true;
    } m_dragDelayData;
    void processRotaryTable(const KeyMap::KeyMapNode &node, const QKeyEvent *constpos);
    void switchMouse(const KeyMap::KeyMapNode &node, const QKeyEvent *from);
    void processDualMode(KeyMap::KeyMapNode &node, const QKeyEvent *from);
    void processType(KeyMap::KeyMapNode node, const QKeyEvent *from);
    void setMousePos(bool b, const KeyMap::KeyMapNode &node);
    static QPointF shakePos(QPointF pos, double offsetX, double offsetY);
    void processPressRelease(const KeyMap::KeyMapNode &node, const QKeyEvent *from);
    void switchMouse(const KeyMap::KeyMapNode &node, bool forceSwitchOn, bool forceSwitchOff);

    bool processMobaWheel(const QMouseEvent *from);

    bool processMobaMouseMove(const QMouseEvent *from);

    static double calcDistance(const QPointF &point1, const QPointF &point2);

    void onWheelTimer(int key);

    void processMobaSkill(const KeyMap::KeyMapNode &node, const QKeyEvent *pEvent);

    void processBurstClick(const KeyMap::KeyMapNode &node, const QKeyEvent *from);

    void cycleClick(QPointF pos, int clickInterval, int i1);

    void stopMobaWheel(double delay);
};

#endif // INPUTCONVERTGAME_H
