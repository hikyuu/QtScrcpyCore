#ifndef MOUSE_H
#define MOUSE_H

#include "QtScrcpyCoreDef.h"
#include "adbprocess.h"
#include <QApplication>
#include <QDateTime>
#include <QPoint>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>
#include <QWidget>
#include <QHostAddress>
#include <controller.h>
#include <iostream>

class Mouse final : public QThread
{
    Q_OBJECT

qsc::AdbProcess m_workProcess;
    bool m_forward = false;
    QTcpSocket *m_socket; // tcp套接字
    QString m_serial;
    QWidget *window; // 父窗口指针
    QPointer<Mouse> m_mouse = Q_NULLPTR;
    //true为hide,false为show
    bool m_hideMouseCursor = false;
    QSize m_frameSize;
    QSize m_showSize;
    QElapsedTimer elapsedTimer;

public:
    Mouse(QObject *parent, qsc::DeviceParams params);
    void stopCapture();
    bool startCapture();

signals:
    void onMouseStop();

protected:
    void run() override;

public slots:
    void onMouseEvent(const QMouseEvent *from, const QSize &frameSize, const QSize &showSize);
    void sendMousePos(QPointF f, bool gameMap) const;
    void onHideMouseCursor(bool hide);
    void onAdbProcessResult(qsc::AdbProcess::ADB_EXEC_RESULT processResult);
    bool disableTunnelForward();

};

#endif //MOUSE_H