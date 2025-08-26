#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QStaticText>
#include <utility>

#include "keymap.h"
#include "maskwidget.h"

MaskWidget::MaskWidget(QWidget *parent, QPointer<KeyMap> keyMap) : QWidget(parent)
{
    m_keyMap = std::move(keyMap);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
}

void MaskWidget::updateMask() {}

void MaskWidget::paintEvent(QPaintEvent* event) {
//    qDebug() << "绘制键盘布局";
    QPainter painter(this);
    // 半透明背景
    painter.fillRect(rect(), QColor(0, 0, 0, 0));

    QList<KeyMap::KeyMapNode> clickList = m_keyMap->getKeyMapNodeByType(KeyMap::KeyMapType::KMT_CLICK);
    painter.setRenderHint(QPainter::Antialiasing); // 抗锯齿
    for (const auto &item: clickList) {
        // 归一化坐标转实际像素坐标
        QPointF pos = item.data.click.keyNode.pos;

        int x = qRound(pos.x() * width());
        int y = qRound(pos.y() * height());
        int radius = qRound(item.data.click.keyNode.radius * qMax(width(), height()));
//        qDebug() << item.data.click.keyNode.key << "半径：" << radius;

        // 设置半透明画刷
        painter.setBrush(QBrush(QColor(0,0,0,100))); // 黑色半透明
        painter.setPen(Qt::NoPen);
        // 绘制圆（圆心坐标需偏移半径值）
        painter.drawEllipse(QPointF(x, y), radius, radius);

        QPen greenPen(QColor(61, 220, 132,255)); // 纯绿色
        greenPen.setWidth(3);             // 边框宽度2像素
        painter.setPen(greenPen);
        painter.setBrush(Qt::NoBrush);     // 不填充
        painter.drawEllipse(QPointF(x, y), radius, radius);

        // 构造正方形 QRect (宽高=直径)
        QRect rect(x - radius, y - radius, 2 * radius, 2 * radius);
        int flags = Qt::AlignCenter | Qt::TextWordWrap;  // 居中+自动换行
        painter.drawText(rect, flags, QKeySequence(item.data.click.keyNode.key).toString());
    }
}