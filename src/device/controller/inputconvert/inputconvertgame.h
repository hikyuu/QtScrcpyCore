#ifndef INPUTCONVERTGAME_H
#define INPUTCONVERTGAME_H

#include <QApplication>
#include <QPointF>
#include <QQueue>
#include <QScreen>
#include <QWidget>
#include <QMutex>
#include <QTimer>
#include <QPainterPath>
#include <QElapsedTimer>

#include "inputconvertnormal.h"
#include "keymap.h"
#include "maskwidget.h"
#include "touchmanager.h"
#include "touchinjectoradapter.h"

#define MULTI_TOUCH_MAX_NUM 10
const int MAX_HISTORY = 3;       // 保留最近3个点
class InputConvertGame : public InputConvertNormal {
Q_OBJECT
public:
    InputConvertGame(Controller *controller);

    virtual ~InputConvertGame();


    virtual void mouseEvent(const QMouseEvent *from, const QSize &frameSize, const QSize &showSize);

    virtual void wheelEvent(const QWheelEvent *from, const QSize &frameSize, const QSize &showSize);

    virtual void rawMouseEvent(int dx, int dy, DWORD buttons);

    virtual void keyEvent(const QKeyEvent *from, const QSize &frameSize, const QSize &showSize);

    virtual bool isCurrentCustomKeymap();

    void loadKeyMap(const QString &json);

protected:
    void updateSize(const QSize &frameSize, const QSize &showSize);

    void sendKeyEvent(AndroidKeyeventAction action, AndroidKeycode keyCode);

    // steer wheel
    void processSteerWheel(const KeyMap::KeyMapNode &node, const QKeyEvent *from);

    // click
    void processKeyClick(bool clickTwice, const KeyMap::KeyMapNode &node, const QKeyEvent *from);

    // click mutil
    void processKeyClickMulti(const KeyMap::DelayClickNode *nodes, const int count, const double pressTime,
                              const QKeyEvent *from);

    // drag
    void processKeyDrag(const QPointF &startPos, QPointF endPos, const QKeyEvent *from);

    // android key
    void processAndroidKey(AndroidKeycode androidKey, QEvent::Type type);

    // mouse
    bool processMouseClick(const QMouseEvent *from);

void processMouseClick(const QMouseEvent *from, const KeyMap::KeyMapNode &node);

bool processCustomMouseClick(const QMouseEvent *from);

    bool processMouseMove(const QMouseEvent *from);

    static void moveCursorTo(const QMouseEvent *from, const QPoint &localPosPixel);

    bool mouseMoveStartTouch(const QPointF pos);

    void mouseMoveStopTouch(bool delay);

    void startMouseMoveTimer();

    void stopMouseMoveTimer();

    bool switchGameMap();

    bool checkCursorPos(const QMouseEvent *from);

    void hideMouseCursor(bool hide);

    void getCurvedDelayQueue(const QPointF &start, const QPointF &end, int stepCount, QQueue<QPointF> &queuePos,
                             QQueue<quint32> &queueTimer);

signals:

    void mouseCursorHided(bool hide);

private slots:

    void onSteerWheelTimer();

    void onDragTimer();

    void onWheelScrollTimer();

    void onWheelUpTimer();

private:

    #include "inputconvertgame_vars.h"

    void processRotaryTable(const KeyMap::KeyMapNode &node, const QKeyEvent *constpos);
    void switchMouse(const KeyMap::KeyMapNode &node, const QKeyEvent *from);
    void processDualMode(KeyMap::KeyMapNode &node, const QKeyEvent *from);
    void processType(KeyMap::KeyMapNode node, const QKeyEvent *from);
    void setMousePos(bool b, const KeyMap::KeyMapNode &node);

    void processPressRelease(const KeyMap::KeyMapNode &node, const QKeyEvent *from);
    void switchMouse(const KeyMap::KeyMapNode &node, bool forceSwitchOn, bool forceSwitchOff);

    bool processMobaWheel(const QMouseEvent *from);

    bool processMobaMouseMove(const QMouseEvent *from);

    void processMobaMouseMoveInternal();

    bool processMobaMouseClick(const QMouseEvent * from);

    static double calcDistance(const QPointF &point1, const QPointF &point2);

    void onWheelTimer(int key);

    void processMobaSkill(const KeyMap::KeyMapNode &node, const QKeyEvent *pEvent);

    void processBurstClick(const KeyMap::KeyMapNode &node, int key, bool press);

    void processKeyBoardBurstClick(const KeyMap::KeyMapNode &node, const QKeyEvent *from);

    void cycleClick(QPointF pos, int clickInterval, int i1);

    void stopMobaWheel();

    void onResetMoveTimer();

    void onStopMobaWheelTimer();

    void dragStop();

    KeyMap::KeyMapNode getNode(const QKeyEvent *from);

    bool checkBoundary(const QPointF &currentConvertPos) const;

    bool mouseMove(const QPointF &currentConvertPos);

    static double generateDouble(double min, double max);

    void generateArcPath(const QPointF &start, const QPointF &end);

    static void getDelayQueue(QQueue<QPointF> &queuePos, QQueue<quint32> &queueTimer, bool detect, quint32 stepTimer,
                       quint32 randomTimer, int endFrame, const QPainterPath &path) ;

    void updatePosition(const QPointF &newPos);

    QVector<QPointF> calculateControlPoints() const;

    QPainterPath generateBezierPath();

    bool checkOutOfBoundary(const QPoint &pos, int oneOfSevenWidth, int oneOfSevenHeight) const;

    static QPointF generatePos(QPointF pos, double radius, double k);

    void activated(bool isActive) override;

    void keyboard(void *pVoid) override;

    void prepareToDelete() override;

    static QPainterPath generateLinePath(QPointF start, QPointF end);

    static QPointF pointAtPercent(const QPointF &start, const QPointF &end, double percent);

    void mouseMove();

    void onLoopTimer();

    static QPointF calcPerspectiveSkillDistance(const QPointF &rawPos, const QPointF &centerPos, double skillRatio);

};

#endif // INPUTCONVERTGAME_H
