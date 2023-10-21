#ifndef MOUSE_H
#define MOUSE_H

#include <QCoreApplication>
#include <QCursor>
#include <QHostAddress>
#include <QMouseEvent>
#include <QNetworkDatagram>
#include <QPoint>
#include <QPointer>
#include <QSocketNotifier>
#include <QThread>
#include <QTimer>
#include <QUdpSocket>
#include <QDateTime>
#include <controller.h>
#include <iostream>

class Mouse : public QThread
{
    Q_OBJECT

private:
    QTimer *timer;      // 定时器
    QUdpSocket *socket; // UDP套接字
    QWidget *window;    // 父窗口指针
    QPointer<Mouse> m_mouse = Q_NULLPTR;
    //true为hide,false为show
    bool m_hideMouseCursor = false;
    QPointF m_pos;
    QSize m_frameSize;
    QSize m_showSize;
    qint64 m_timestamp = QDateTime::currentDateTime().toMSecsSinceEpoch();

public:
    Mouse(QObject *parent = Q_NULLPTR);
    void stopCapture();
    bool startCapture();

signals:
    void onMouseStop();

protected:
    void run() override;

public slots:
    void onMouseEvent(const QMouseEvent *from, const QSize &frameSize, const QSize &showSize);
    void sendMousePos(QPointF f,bool gameMap);
    void onHideMouseCursor(bool hide);

};

#endif //MOUSE_H