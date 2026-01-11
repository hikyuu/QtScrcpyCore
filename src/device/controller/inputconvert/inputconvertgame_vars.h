#ifndef INPUTCONVERTGAME_VARS_H
#define INPUTCONVERTGAME_VARS_H

#include <QTimer>
#include <QSize>
#include <QPointer>
#include <QMap>
#include <QSet>
#include <QPointF>
#include <QPainterPath>
#include <QVector>
#include <QQueue>
#include <QList>
#include <QMutex>
#include <QPoint>

#include "keymap.h"
#include "maskwidget.h"
#include "touchmanager.h"
#include "touchinjectoradapter.h"

// This header is expected to be included inside the `private:` section of
// `InputConvertGame` class and provides the private member declarations.

    TouchManager* m_touchManager = nullptr;
    TouchInjectorAdapter* m_touchInjector = nullptr;

    QTimer *loopTimer = nullptr;

    void stopAllInteractions(bool restoreCursor);

    QSize m_frameSize;
    QSize m_showSize;
    double m_showSizeRatio{};
    bool m_gameMap = false;
    QPointer<MaskWidget> m_maskWidget;
    bool m_customNormalMouseClick = false;
    //准心模式鼠标移动镜头
    bool m_pointerMode = false;
    KeyMap m_keyMap;
    QMap<int, QPointF> m_keyPosMap;
    QSet<int> m_burstClickKeySet;
    QPointF m_currentSpeedRatio;
//    QPoint lastAbsolutePos;
    // steer wheel
    struct {
        // the first key wheelPressed
        int touchKey = Qt::Key_sterling;
        bool pressedUp = false;
        bool pressedDown = false;
        bool pressedLeft = false;
        bool pressedRight = false;
        bool pressedBoost = false;
        bool clickMode = false;
        bool wheeling = false;
        bool simulateWheel = true;
        double scaleRatio = 0.6;
        bool fixedStick = false;
        bool keepMove = false;
        QPointF clickPos;
        QPointF centerPos;
        // for delay
        struct {
            QPointF currentPos;
            QPainterPath path;
            QVector<QPointF> historyPoints;
            QQueue<QPointF> queuePos;
            QQueue<quint32> queueTimer;
            int step = 0;
            int pressedNum = 0;
            QPointF endPos;
            QPointF shakeEndPos;
            QPointF offsetY;
            QPointF middlePoint;
            bool isEnd = true;
        } delayData;
        struct{
            double speedRatio{};
            double skillRatio{};
            QPointF characterCenterPos;
            QPointF wheelCenterPos;
            QPointF endPos;
            bool mouseWheeling = false;
            bool buttonPressed = false;
            bool skillPressed = false;
            bool quickCast = false;
            QPointF localPos;
            double skillOffset{};
        } mobaWheel;
    } m_ctrlSteerWheel;
    // mouse move
    struct
    {
        int dx = 0;
        int dy = 0;
        QPointF startPos;
        QPointF lastConvertPos;
        QPointF processedPos;
        bool waitClick = false;
        QPointF cursorPos;
        QPoint centerPos;
        bool outOfBoundary = false;
        QPointF lastPos = { 0.0, 0.0 };
        int focusTouchID = -1;
        bool touching = false;
        double leftBoundary = 0.3;
        double topBoundary = 0.1;
        double maxBoundary = 0.9;
        QTimer resetMoveTimer;
        int resetMoveDelay = 100;
        bool needResetTouch = true;
        bool smallEyes = false;
        bool rotaryTable = false;
        QMutex mouseMutex;
        int count = 0;
    } m_ctrlMouseMove;

    // for drag delay
    struct
    {
        QPointF startPos;
        QPointF currentPos;
        QTimer *timer = nullptr;
        QTimer *upTimer = nullptr;
        QPainterPath path;
        QQueue<QPointF> queuePos;
        QQueue<quint32> queueTimer;
        int pressKey = 0;
        bool allowUp = true;
        int dragDelayUpTime = 0;
    } m_dragDelayData;
    struct DragData {
        int pressKey = 0;
        QPointF startPos;
        QPointF currentPos;
        QQueue<QPointF> queuePos;
    };
    struct  {
        QPointF startPos;
        QPointF currentPos;
        QPointF endPos;
        QPointF zoomPos;
        QList<DragData> dragData;
        QTimer *timer = nullptr;
        QTimer *upTimer = nullptr;
        QQueue<QPointF> queuePos;
        int stepTime = 0;
        int pressKey = 0;
        bool wheeling = false;
        int wheelDelayUpTime = 200;
    } m_wheelDelayData;

    bool m_mobaMouseMovePending = false;

#endif // INPUTCONVERTGAME_VARS_H

