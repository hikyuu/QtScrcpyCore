#include "mouse.h"
#include "adbprocess.h"
#include <tchar.h>

Mouse::Mouse(QObject *parent, qsc::DeviceParams params) : QThread(parent)
{
    m_serial = params.serial;
    connect(&m_workProcess, &qsc::AdbProcess::adbProcessResult, this, &Mouse::onAdbProcessResult);
    m_workProcess.forward(m_serial, 9999, "mouse-cursor");
}

void Mouse::run() {}

void Mouse::stopCapture()
{
    if (m_socket) {
        m_socket->close();
    }
    disableTunnelForward();
    m_workProcess.kill();
}

void Mouse::onAdbProcessResult(qsc::AdbProcess::ADB_EXEC_RESULT processResult)
{
    if (processResult == qsc::AdbProcess::AER_SUCCESS_EXEC) {
        qDebug() << "adb result" << processResult;
        // 初始化TCP套接字
        m_socket = new QTcpSocket();
        connect(m_socket, &QTcpSocket::stateChanged, m_socket, [](QAbstractSocket::SocketState state) { qInfo() << "mouse m_socket state changed:" << state; });
        m_socket->connectToHost(QHostAddress::LocalHost, 9999);
        if (m_socket->waitForConnected()) {
            qDebug() << "forward success";
            // 连接成功
            m_forward = true;
            QString data = "Hello, server!\n";
            // 将数据转换为字节数组
            QByteArray bytes = data.toUtf8();
            // 将数据写入到QTcpSocket对象中
            m_socket->write(bytes);
        }
    }
}

bool Mouse::disableTunnelForward()
{
    qsc::AdbProcess *adb = new qsc::AdbProcess();
    if (!adb) {
        return false;
    }
    connect(adb, &qsc::AdbProcess::adbProcessResult, this, [this](qsc::AdbProcess::ADB_EXEC_RESULT processResult) {
        if (qsc::AdbProcess::AER_SUCCESS_START != processResult) {
            sender()->deleteLater();
        }
    });
    adb->forwardRemove(m_serial, 9999);
    return true;
}

void Mouse::onMouseEvent(const QMouseEvent *from, const QSize &frameSize, const QSize &showSize)
{
    if (!m_forward)
        return;
    if (m_hideMouseCursor) {
        return;
    }
    QPointF pos = from->localPos();
    pos.setX(pos.x() * frameSize.width() / showSize.width());
    pos.setY(pos.y() * frameSize.height() / showSize.height());
    sendMousePos(pos, m_hideMouseCursor);
}

void Mouse::sendMousePos(QPointF pos, bool gameMap)
{
    QStringList posList;
    posList << QString::number(static_cast<int>(pos.x())) << QString::number(static_cast<int>(pos.y()));
    posList.append(gameMap ? "1" : "0");

    // 将字符串转换为字节数组
    QByteArray data = posList.join(",").append("\n").toUtf8();
    // 向服务端发送
    m_socket->write(data);
    m_socket->flush();
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
