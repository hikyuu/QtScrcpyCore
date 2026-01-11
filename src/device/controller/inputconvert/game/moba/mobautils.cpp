#include "../../inputconvertgame.h"
#include <cmath>
#include <QDebug>

QPointF InputConvertGame::calcPerspectiveSkillDistance(const QPointF &rawPos, const QPointF &centerPos, double skillRatio)
{
    // 计算鼠标相对于角色中心的屏幕空间向量
    QPointF screenDelta = rawPos - centerPos;

    // 椭圆参数
    const double radiusUp = 100;  // 虚拟↑半径
    const double radiusDown = 75; // 虚拟↓半径
    const double radiusX = 75;    // 虚拟X半径

    // 1. 计算输入向量的极坐标（角度和距离）
    double angle = std::atan2(screenDelta.y(), screenDelta.x());
    double inputDistance = std::sqrt(screenDelta.x() * screenDelta.x() + screenDelta.y() * screenDelta.y());

    if (inputDistance < 1e-6) {
        return { 0, 0 };
    }

    // 2. 根据Y方向选择对应的椭圆半径
    double ellipseRadiusY = (screenDelta.y() < 0) ? radiusUp : radiusDown;

    // 3. 计算该角度下椭圆边界到中心的距离
    double cosAngle = std::cos(angle);
    double sinAngle = std::sin(angle);

    double ellipseX = radiusX * cosAngle;
    double ellipseY = ellipseRadiusY * sinAngle;
    double ellipseRadius = std::sqrt(ellipseX * ellipseX + ellipseY * ellipseY);

    // 4. 计算映射比例
    const double standardCircleRadius = 100.0;
    double mappingScale = ellipseRadius / standardCircleRadius;

    // 5. 应用映射和灵敏度
    QPointF worldDistance{ screenDelta.x() * mappingScale / skillRatio, screenDelta.y() * mappingScale / skillRatio };

    return worldDistance;
}
