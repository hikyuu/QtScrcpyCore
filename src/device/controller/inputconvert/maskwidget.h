#ifndef MASKWIDGET_H
#define MASKWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QPaintEvent>

#include "keymap.h"

class MaskWidget : public QWidget {
Q_OBJECT
public:
    MaskWidget(QWidget *parent, QPointer<KeyMap> keyMap);

private:
    void paintEvent(QPaintEvent* event) override;

public:
    void updateMask();
private:
    QPointer<KeyMap> m_keyMap;
};

#endif // MASKWIDGET_H