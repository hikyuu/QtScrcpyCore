#include "mouse.h"
#include <tchar.h>

Mouse::Mouse(QObject *parent) : QThread(parent)
{
    // 初始化定时器
    timer = new QTimer();
    // 初始化UDP套接字
    socket = new QUdpSocket();
}

void Mouse::run()
{

    // 设置定时器间隔为10毫秒，即每秒100次
    timer->setInterval(10);
    // 启动定时器
    timer->start();
    // 进入事件循环
    exec();
}

bool Mouse::startCapture()
{
    if (!m_mouse) {
        return false;
    }
    start();
    return true;
}

void Mouse::stopCapture()
{
    wait();
}

void Mouse::onMouseEvent(const QMouseEvent *from, const QSize &frameSize, const QSize &showSize)
{
    if (m_hideMouseCursor) {
        return;
    }

    qint64 timestamp = QDateTime::currentDateTime().toMSecsSinceEpoch(); //毫秒级
    if (timestamp - m_timestamp < 15) {
        return;
    }
    m_timestamp = timestamp;

    QPointF pos = from->localPos();
    pos.setX(pos.x() * frameSize.width() / showSize.width());
    pos.setY(pos.y() * frameSize.height() / showSize.height());
    sendMousePos(pos, m_hideMouseCursor);
}

void Mouse::sendMousePos(QPointF pos, bool gameMap)
{

    // 将坐标转换为字符串格式，比如"(100, 200)"
    QString posX = QString("%1").arg(static_cast<int>(pos.x()), 4, 10, QChar('0'));
    QString posY = QString("%1").arg(static_cast<int>(pos.y()), 4, 10, QChar('0'));

    QStringList posList;
    posList << posX << posY;
    posList.append(gameMap ? "1" : "0");

    // 将字符串转换为字节数组
    //QString posStr = QString().append(posX).append(",").append(posY).append(',').append();
    QByteArray data = posList.join(",").toUtf8();

    // 向服务端发送UDP数据报，假设服务端的IP和端口是"192.168.31.99"和9999
    socket->writeDatagram(data, QHostAddress("192.168.31.99"), 9999);
}

void Mouse::onHideMouseCursor(bool hide)
{
    m_hideMouseCursor = hide;
    QWidget *activeWindow = QApplication::activeWindow();
    if (activeWindow) {
        QPoint center = activeWindow->mapToGlobal(activeWindow->rect().center());
        sendMousePos(center, m_hideMouseCursor);
    }
}
