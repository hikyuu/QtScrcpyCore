#ifndef INPUTCONVERT_H
#define INPUTCONVERT_H

#include "inputconvertbase.h"

class InputConvertNormal : public InputConvertBase
{
    Q_OBJECT
public:
    InputConvertNormal(Controller *controller);
    ~InputConvertNormal() override;
     void mouseEvent(const QMouseEvent *from, const QSize &frameSize, const QSize &showSize) override;
     void wheelEvent(const QWheelEvent *from, const QSize &frameSize, const QSize &showSize) override;
     void keyEvent(const QKeyEvent *from, const QSize &frameSize, const QSize &showSize) override;

     void rawMouseEvent(int dx, int dy, DWORD buttons) override;
     void activated(bool isActive) override;
     void keyboard(void *pVoid) override;
     void prepareToDelete() override;
private:
    AndroidMotioneventButtons convertMouseButtons(Qt::MouseButtons buttonState);
    AndroidMotioneventButtons convertMouseButton(Qt::MouseButton button);
    AndroidKeycode convertKeyCode(int key, Qt::KeyboardModifiers modifiers);
    AndroidMetastate convertMetastate(Qt::KeyboardModifiers modifiers);
};

#endif // INPUTCONVERT_H
