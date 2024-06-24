#include <QActionEvent>
#include <QCoreApplication>
#include <QDataStream>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QMapData>
#include <QMapDataBase>
#include <QMapIterator>
#include <QMapNode>
#include <QMapNodeBase>
#include <QMetaEnum>
#include <QMultiMap>
#include <QMutableMapIterator>

#include "keymap.h"

KeyMap::KeyMap(QObject *parent) : QObject(parent) {}

KeyMap::~KeyMap() {}

void KeyMap::loadKeyMap(const QString &json)
{
    QString errorString;
    QJsonParseError jsonError;
    QJsonDocument jsonDoc;
    QJsonObject rootObj;
    QPair<ActionType, int> switchKey;

    jsonDoc = QJsonDocument::fromJson(json.toUtf8(), &jsonError);

    if (jsonError.error != QJsonParseError::NoError) {
        errorString = QString("json error: %1").arg(jsonError.errorString());
        goto parseError;
    }

    // switchKey
    rootObj = jsonDoc.object();

    if (!checkItemString(rootObj, "switchKey")) {
        errorString = QString("json error: no find switchKey");
        goto parseError;
    }

    switchKey = getItemKey(rootObj, "switchKey");
    if (switchKey.first == AT_INVALID) {
        errorString = QString("json error: switchKey invalid");
        goto parseError;
    }

    m_switchKey.type = switchKey.first;
    m_switchKey.key = switchKey.second;

    // mouseMoveMap
    if (checkItemObject(rootObj, "mouseMoveMap")) {
        QJsonObject mouseMoveMap = getItemObject(rootObj, "mouseMoveMap");
        KeyMapNode keyMapNode;
        keyMapNode.type = KMT_MOUSE_MOVE;

        bool have_speedRatio = false;

        // General speedRatio (for backwards compatibility)
        if (checkItemDouble(mouseMoveMap, "speedRatio")) {
            float ratio = static_cast<float>(getItemDouble(mouseMoveMap, "speedRatio"));
            keyMapNode.data.mouseMove.speedRatio.setX(ratio);
            keyMapNode.data.mouseMove.speedRatio.setY(ratio / 2.25f); // Phone screens are often FHD+
            have_speedRatio = true;
        }

        // Individual X Ratio
        if (checkItemDouble(mouseMoveMap, "speedRatioX")) {
            keyMapNode.data.mouseMove.speedRatio.setX(static_cast<float>(getItemDouble(mouseMoveMap, "speedRatioX")));
            have_speedRatio = true;
        }

        // Individual Y Ratio
        if (checkItemDouble(mouseMoveMap, "speedRatioY")) {
            keyMapNode.data.mouseMove.speedRatio.setY(static_cast<float>(getItemDouble(mouseMoveMap, "speedRatioY")));
            have_speedRatio = true;
        }

        if (!have_speedRatio) {
            errorString = QString("json error: speedRatio setting is missing in mouseMoveMap!");
            goto parseError;
        }

        // Sanity check: No ratio must be lower than 0.001
        if ((keyMapNode.data.mouseMove.speedRatio.x() < 0.001f) || (keyMapNode.data.mouseMove.speedRatio.x() < 0.001f)) {
            errorString = QString("json error: Minimum speedRatio is 0.001");
            goto parseError;
        }

        if (!checkItemObject(mouseMoveMap, "startPos")) {
            errorString = QString("json error: mouseMoveMap on find startPos");
            goto parseError;
        }
        QJsonObject startPos = mouseMoveMap.value("startPos").toObject();
        if (checkItemDouble(startPos, "x")) {
            keyMapNode.data.mouseMove.startPos.setX(getItemDouble(startPos, "x"));
        }
        if (checkItemDouble(startPos, "y")) {
            keyMapNode.data.mouseMove.startPos.setY(getItemDouble(startPos, "y"));
        }

        // small eyes
        if (checkItemObject(mouseMoveMap, "smallEyes")) {
            QJsonObject smallEyes = mouseMoveMap.value("smallEyes").toObject();
            if (!smallEyes.contains("type") || !smallEyes.value("type").isString()) {
                errorString = QString("json error: smallEyes no find node type");
                goto parseError;
            }

            // type just support KMT_CLICK
            KeyMap::KeyMapType type = getItemKeyMapType(smallEyes, "type");
            if (KeyMap::KMT_CLICK != type) {
                errorString = QString("json error: smallEyes just support KMT_CLICK");
                goto parseError;
            }

            // safe check
            if (!checkForClick(smallEyes)) {
                errorString = QString("json error: smallEyes node format error");
                goto parseError;
            }

            QPair<ActionType, int> key = getItemKey(smallEyes, "key");
            if (key.first == AT_INVALID) {
                errorString = QString("json error: keyMapNodes node invalid key: %1").arg(smallEyes.value("key").toString());
                goto parseError;
            }

            keyMapNode.data.mouseMove.smallEyes.type = key.first;
            keyMapNode.data.mouseMove.smallEyes.key = key.second;
            keyMapNode.data.mouseMove.smallEyes.pos = getItemPos(smallEyes, "pos");
        }

        m_idxMouseMove = m_keyMapNodes.size();
        m_keyMapNodes.push_back(keyMapNode);
    }

    // keyMapNodes
    if (rootObj.contains("keyMapNodes") && rootObj.value("keyMapNodes").isArray()) {
        QJsonArray keyMapNodes = rootObj.value("keyMapNodes").toArray();
        QJsonObject node;
        int size = keyMapNodes.size();
        for (int i = 0; i < size; i++) {
            if (!keyMapNodes.at(i).isObject()) {
                errorString = QString("json error: keyMapNodes node must be json object");
                goto parseError;
            }
            node = keyMapNodes.at(i).toObject();
            if (!node.contains("type") || !node.value("type").isString()) {
                errorString = QString("json error: keyMapNodes no find node type");
                goto parseError;
            }
            KeyMapNode keyMapNode;
            setKeyMapNode(node, keyMapNode, false);
            if (keyMapNode.type != KMT_INVALID) {
                m_keyMapNodes.push_back(keyMapNode);
            }
        }
    }
    // this must be called after m_keyMapNodes is stable
    makeReverseMap();
    qInfo() << "Script updated, current keymap mode:normal, Press ~ key to switch keymap mode";

parseError:
    if (!errorString.isEmpty()) {
        qWarning() << errorString;
    }
    return;
}
void KeyMap::setKeyMapNode(const QJsonObject &node, KeyMapNode &keyMapNode, const QString &dualMode)
{
    KeyMap::KeyMapType type = getItemKeyMapType(node, "type");
    switch (type) {
    case KMT_CLICK: {
        setClickMapNode(keyMapNode, node, type, dualMode);
    } break;
    case KMT_CLICK_TWICE: {
        setClickTwiceMapNode(keyMapNode, node, type, dualMode);
    } break;
    case KMT_CLICK_MULTI: {
        setClickMultiMapNode(keyMapNode, node, type, dualMode);
    } break;
    case KMT_STEER_WHEEL: {
        setSteerWheelMapNode(keyMapNode, node, type);
    } break;
    case KMT_DRAG: {
        setDragMapNode(keyMapNode, node, type, dualMode);
    } break;
    case KMT_ANDROID_KEY: {
        setAndroidKeyMapNode(keyMapNode, node, type, dualMode);
    } break;
    case KMT_ROTARY_TABLE: {
        setRotaryTableMapNode(keyMapNode, node, type, dualMode);
    } break;
    case KMT_DUAL_MODE: {
        setDualModeMapNode(keyMapNode, node, type);
    } break;
    case KMT_PRESS_RELEASE: {
        setPressReleaseMapNode(keyMapNode, node, type, dualMode);
    } break;
    case KMT_MOBA_WHEEL: {
        setMobaWheelMapNode(keyMapNode, node, type);
    }break;
    case KMT_MOBA_SKILL: {
        setMobaSkillMapNode(keyMapNode, node, type);
    }break;
    case KMT_BURST_CLICK:{
        setBurstClickMapNode(keyMapNode, node, type);
    }break;
    default:
        qWarning() << "json error: keyMapNodes invalid node type:" << node.value("type").toString();
    }
}

void KeyMap::setPressReleaseMapNode(KeyMap::KeyMapNode &keyMapNode, const QJsonObject &node, KeyMap::KeyMapType type, const QString &dualMode)
{
    if (!checkForPressRelease(node)) {
        qWarning() << "json error: keyMapNodes node format error" << node;
        return;
    }
    QPair<ActionType, int> key = getItemKey(node, "key");
    if (key.first == AT_INVALID) {
        qWarning() << "json error: keyMapNodes node invalid key: " << node.value("key").toString();
        return;
    }
    keyMapNode.type = type;
    keyMapNode.data.pressRelease.keyNode.type = key.first;
    keyMapNode.data.pressRelease.keyNode.key = key.second;
    keyMapNode.data.pressRelease.pressPos = getItemPos(node, "pressPos");
    keyMapNode.data.pressRelease.releasePos = getItemPos(node, "releasePos");
    keyMapNode.switchMap = true;
    if (checkItemBool(node, "switchMap")) {
        keyMapNode.switchMap = getItemBool(node, "switchMap");
    }
}

void KeyMap::setDualModeMapNode(KeyMap::KeyMapNode &keyMapNode, const QJsonObject &node, const KeyMap::KeyMapType &type)
{
    if (!checkForDualMode(node)) {
        qWarning() << "json error: keyMapNodes node format error" << node;
        return;
    }
    QPair<ActionType, int> key = getItemKey(node, "key");
    if (key.first == AT_INVALID) {
        qWarning() << "json error: keyMapNodes node invalid key: " << node.value("key").toString();
        return;
    }
    QJsonObject accurateNode = node.value("accurate").toObject();
    if (!accurateNode.contains("type") || !accurateNode.value("type").isString()) {
        qWarning() << "json error: DualModeKeyMapNode accurateNode have not find node type";
        return;
    }
    QJsonObject mouse = node.value("mouse").toObject();
    if (!accurateNode.contains("type") || !accurateNode.value("type").isString()) {
        qWarning() << "json error: DualModeKeyMapNode mouse have not find node type";
        return;
    }
    accurateNode.insert("key", node.value("key"));
    setKeyMapNode(accurateNode, keyMapNode, "accurate");

    mouse.insert("key", node.value("key"));
    setKeyMapNode(mouse, keyMapNode, "mouse");
    keyMapNode.type = type;
    setCommonProperties(node, keyMapNode);
}

void KeyMap::setRotaryTableMapNode(KeyMapNode &keyMapNode, const QJsonObject &node, const KeyMapType &type, const QString dualMode)
{
    if (!checkForRotaryTable(node)) {
        qWarning() << "json error: KMT_ROTARY_TABLE keyMapNodes node format error" << node;
        return;
    }
    QPair<ActionType, int> key = getItemKey(node, "key");
    if (key.first == AT_INVALID) {
        qWarning() << "json error: keyMapNodes node invalid key: " << node.value("key").toString();
        return;
    }
    keyMapNode.type = type;
    if (checkItemDouble(node, "speedRatio")) {
        float ratio = static_cast<float>(getItemDouble(node, "speedRatio"));
        keyMapNode.data.rotaryTable.speedRatio.setX(ratio);
        keyMapNode.data.rotaryTable.speedRatio.setY(ratio);
    } else {
        keyMapNode.data.rotaryTable.speedRatio.setX(1.0f);
        keyMapNode.data.rotaryTable.speedRatio.setY(1.0f);
    }
    keyMapNode.data.rotaryTable.keyNode.type = key.first;
    keyMapNode.data.rotaryTable.keyNode.key = key.second;
    keyMapNode.data.rotaryTable.keyNode.pos = getItemPos(node, "pos");

    keyMapNode.data.rotaryTable.keyNode.androidKey = static_cast<AndroidKeycode>(static_cast<int>(getItemDouble(node, "androidKey")));
    setCommonProperties(node, keyMapNode);
    setDualMode(keyMapNode, dualMode, keyMapNode.data.rotaryTable.keyNode, type);
}
void KeyMap::setAndroidKeyMapNode(KeyMapNode &keyMapNode, const QJsonObject &node, const KeyMapType &type, const QString string)
{
    // safe check
    if (!checkForAndroidKey(node)) {
        qWarning() << "json error: KMT_ANDROID_KEY keyMapNodes node format error" << node;
        return;
    }

    QPair<ActionType, int> key = getItemKey(node, "key");
    if (key.first == AT_INVALID) {
        qWarning() << "json error: keyMapNodes node invalid key: " << node.value("key").toString();
        return;
    }
    keyMapNode.type = type;
    keyMapNode.data.androidKey.keyNode.type = key.first;
    keyMapNode.data.androidKey.keyNode.key = key.second;
    keyMapNode.data.androidKey.keyNode.androidKey = static_cast<AndroidKeycode>(static_cast<int>(getItemDouble(node, "androidKey")));
    //    qDebug() << keyMapNode.data.androidKey.keyNode.androidKey;
    setCommonProperties(node, keyMapNode);
    setDualMode(keyMapNode, string, keyMapNode.data.androidKey.keyNode, type);
}
void KeyMap::setDragMapNode(KeyMapNode &keyMapNode, const QJsonObject &node, const KeyMapType &type, const QString dualMode)
{
    // safe check
    if (!checkForDrag(node)) {
        qWarning() << "json error: KMT_DRAG keyMapNodes node format error";
        return;
    }

    QPair<ActionType, int> key = getItemKey(node, "key");
    if (key.first == AT_INVALID) {
        qWarning() << "json error: keyMapNodes node invalid key: " << node.value("key").toString();
        return;
    }
    keyMapNode.type = type;
    keyMapNode.data.drag.keyNode.type = key.first;
    keyMapNode.data.drag.keyNode.key = key.second;
    keyMapNode.data.drag.keyNode.pos = getItemPos(node, "startPos");
    keyMapNode.data.drag.keyNode.extendPos = getItemPos(node, "endPos");
    setCommonProperties(node, keyMapNode);
    setDualMode(keyMapNode, dualMode, keyMapNode.data.drag.keyNode, type);
}
void KeyMap::setSteerWheelMapNode(KeyMapNode &keyMapNode, const QJsonObject &node, const KeyMapType &type)
{
    // safe check
    if (!checkForSteerWhell(node)) {
        qWarning() << "json error: KMT_STEER_WHEEL keyMapNodes node format error";
        return;
    }
    QPair<ActionType, int> leftKey = getItemKey(node, "leftKey");
    QPair<ActionType, int> rightKey = getItemKey(node, "rightKey");
    QPair<ActionType, int> upKey = getItemKey(node, "upKey");
    QPair<ActionType, int> downKey = getItemKey(node, "downKey");

    if (leftKey.first == AT_INVALID) {
        qWarning() << "json error: keyMapNodes node invalid key: " << node.value("leftKey").toString();
        return;
    }
    if (rightKey.first == AT_INVALID) {
        qWarning() << "json error: keyMapNodes node invalid key: " << node.value("rightKey").toString();
        return;
    }
    if (upKey.first == AT_INVALID) {
        qWarning() << "json error: keyMapNodes node invalid key: " << node.value("upKey").toString();
        return;
    }
    if (downKey.first == AT_INVALID) {
        qWarning() << "json error: keyMapNodes node invalid key: " << node.value("downKey").toString();
        return;
    }

    keyMapNode.type = type;

    keyMapNode.data.steerWheel.left = { leftKey.first, leftKey.second, QPointF(0, 0), QPointF(0, 0), getItemDouble(node, "leftOffset") };
    keyMapNode.data.steerWheel.right = { rightKey.first, rightKey.second, QPointF(0, 0), QPointF(0, 0), getItemDouble(node, "rightOffset") };
    keyMapNode.data.steerWheel.up = { upKey.first, upKey.second, QPointF(0, 0), QPointF(0, 0), getItemDouble(node, "upOffset") };
    keyMapNode.data.steerWheel.down = { downKey.first, downKey.second, QPointF(0, 0), QPointF(0, 0), getItemDouble(node, "downOffset") };

    keyMapNode.data.steerWheel.centerPos = getItemPos(node, "centerPos");

    setSteerWheelSwitchMode(node, keyMapNode);
    m_idxSteerWheel = m_keyMapNodes.size();
}
void KeyMap::setClickMultiMapNode(KeyMapNode &keyMapNode, const QJsonObject &node, const KeyMapType &type, const QString &dualMode)
{
    if (!checkForClickMulti(node)) {
        qWarning() << "json error: KMT_CLICK_MULTI keyMapNodes node format error";
        return;
    }
    QPair<ActionType, int> key = getItemKey(node, "key");
    if (key.first == AT_INVALID) {
        qWarning() << "json error: keyMapNodes node invalid key: " << node.value("key").toString();
        return;
    }
    QJsonObject clickNode;
    QJsonArray clickNodes = node.value("clickNodes").toArray();

    if (clickNodes.size() >= MAX_DELAY_CLICK_NODES) {
        qInfo() << "clickNodes too much, up to " << MAX_DELAY_CLICK_NODES;
        return;
    }
    keyMapNode.type = type;
    keyMapNode.data.clickMulti.keyNode.type = key.first;
    keyMapNode.data.clickMulti.keyNode.key = key.second;

    keyMapNode.data.clickMulti.keyNode.delayClickNodesCount = 0;

    for (int _i = 0; _i < clickNodes.size(); _i++) {
        clickNode = clickNodes.at(_i).toObject();
        DelayClickNode delayClickNode;
        delayClickNode.delay = getItemDouble(clickNode, "delay");
        delayClickNode.pos = getItemPos(clickNode, "pos");
        keyMapNode.data.clickMulti.keyNode.delayClickNodes[_i] = delayClickNode;
        keyMapNode.data.clickMulti.keyNode.delayClickNodesCount++;
    }
    setCommonProperties(node, keyMapNode);
    setDualMode(keyMapNode, dualMode, keyMapNode.data.clickMulti.keyNode, type);
}
void KeyMap::setClickTwiceMapNode(KeyMapNode &keyMapNode, const QJsonObject &node, const KeyMapType &type, const QString &dualMode)
{
    if (!checkForClickTwice(node)) {
        qWarning() << "json error: KMT_CLICK_TWICE keyMapNodes node format error";
        return;
    }

    QPair<ActionType, int> key = getItemKey(node, "key");
    if (key.first == AT_INVALID) {
        qWarning() << "json error: keyMapNodes node invalid key: " << node.value("key").toString();
        return;
    }
    keyMapNode.type = type;
    keyMapNode.data.click.keyNode.type = key.first;
    keyMapNode.data.click.keyNode.key = key.second;
    keyMapNode.data.click.keyNode.pos = getItemPos(node, "pos");
    keyMapNode.data.click.keyNode.androidKey = static_cast<AndroidKeycode>(static_cast<int>(getItemDouble(node, "androidKey")));
    setCommonProperties(node, keyMapNode);
    setDualMode(keyMapNode, dualMode, keyMapNode.data.click.keyNode, type);
}

void KeyMap::setClickMapNode(KeyMapNode &keyMapNode, const QJsonObject &node, const KeyMapType &type, const QString &dualMode)
{
    if (!checkForClick(node)) {
        qWarning() << "json error: KMT_CLICK keyMapNodes node format error";
        return;
    }
    QPair<ActionType, int> key = getItemKey(node, "key");
    if (key.first == AT_INVALID) {
        qWarning() << "json error: KMT_CLICK keyMapNodes node invalid key: " << node.value("key").toString();
        return;
    }
    keyMapNode.type = type;
    keyMapNode.data.click.keyNode.type = key.first;
    keyMapNode.data.click.keyNode.key = key.second;
    keyMapNode.data.click.keyNode.pos = getItemPos(node, "pos");
    keyMapNode.data.click.keyNode.androidKey = static_cast<AndroidKeycode>(static_cast<int>(getItemDouble(node, "androidKey")));
    setCommonProperties(node, keyMapNode);
    setDualMode(keyMapNode, dualMode, keyMapNode.data.click.keyNode, type);
}

void KeyMap::setCommonProperties(const QJsonObject &node, KeyMap::KeyMapNode &keyMapNode)
{
    keyMapNode.switchMap = false;
    keyMapNode.freshMouseMove = false;
    keyMapNode.forceSwitchOn = false;
    keyMapNode.forceSwitchOff = false;
    keyMapNode.focusOn = false;
    if (checkItemBool(node, "freshMouseMove")) {
        keyMapNode.freshMouseMove = getItemBool(node, "freshMouseMove");
    }
    if (checkItemBool(node, "forceSwitchOn")) {
        keyMapNode.forceSwitchOn = getItemBool(node, "forceSwitchOn");
    }
    if (checkItemBool(node, "forceSwitchOff")) {
        keyMapNode.forceSwitchOff = getItemBool(node, "forceSwitchOff");
    }
    if (checkItemBool(node, "switchMap")) {
        keyMapNode.switchMap = getItemBool(node, "switchMap");
    }
    if (checkItemBool(node, "focusOn") && checkItemPos(node, "focusPos")) {
        keyMapNode.focusPos = getItemPos(node, "focusPos");
        keyMapNode.focusOn = getItemBool(node, "focusOn");
    }
}
void KeyMap::setSteerWheelSwitchMode(const QJsonObject &node, KeyMap::KeyMapNode &keyMapNode)
{
    if (!checkItemString(node, "switchKey"))
        return;
    QPair<ActionType, int> switchKey = getItemKey(node, "switchKey");
    if (switchKey.first == AT_INVALID) {
        qWarning() << "json error: keyMapNodes node invalid key: " << node.value("switchKey").toString();
        return;
    }
    if (!checkItemPos(node, "leftClickPos")) {
        qWarning() << "json error: keyMapNodes has error: " << "leftClickPos";
        return;
    }
    if (!checkItemPos(node, "rightClickPos")) {
        qWarning() << "json error: keyMapNodes has error: " << "rightClickPos";
        return;
    }
    if (!checkItemPos(node, "upClickPos")) {
        qWarning() << "json error: keyMapNodes has error: " << "upClickPos";
        return;
    }
    if (!checkItemPos(node, "downClickPos")) {
        qWarning() << "json error: keyMapNodes has error: " << "downClickPos";
        return;
    }
    keyMapNode.data.steerWheel.left.pos = getItemPos(node, "leftClickPos");
    keyMapNode.data.steerWheel.right.pos = getItemPos(node, "rightClickPos");
    keyMapNode.data.steerWheel.up.pos = getItemPos(node, "upClickPos");
    keyMapNode.data.steerWheel.down.pos = getItemPos(node, "downClickPos");
    keyMapNode.data.steerWheel.switchKey = { switchKey.first, switchKey.second };
}

const KeyMap::KeyMapNode &KeyMap::getKeyMapNode(int key)
{
    auto p = m_rmapKey.value(key, &m_invalidNode);
    if (p == &m_invalidNode) {
        return *m_rmapMouse.value(key, &m_invalidNode);
    }
    return *p;
}

const KeyMap::KeyMapNode &KeyMap::getKeyMapNodeKey(int key)
{
    return *m_rmapKey.value(key, &m_invalidNode);
}

const KeyMap::KeyMapNode &KeyMap::getKeyMapNodeMouse(int key)
{
    return *m_rmapMouse.value(key, &m_invalidNode);
}

bool KeyMap::isSwitchOnKeyboard()
{
    return m_switchKey.type == AT_KEY;
}

int KeyMap::getSwitchKey()
{
    return m_switchKey.key;
}

const KeyMap::KeyMapNode &KeyMap::getMouseMoveMap()
{
    return m_keyMapNodes[m_idxMouseMove];
}

bool KeyMap::isValidMouseMoveMap()
{
    return m_idxMouseMove != -1;
}

bool KeyMap::isValidSteerWheelMap()
{
    return m_idxSteerWheel != -1;
}

bool KeyMap::isValidMobaWheel() {
    return m_isValidMobaWheel;
}

void KeyMap::makeReverseMap()
{
    m_rmapKey.clear();
    m_rmapMouse.clear();
    for (auto & node : m_keyMapNodes) {
        switch (node.type) {
        case KMT_CLICK: {
            QMultiHash<int, KeyMapNode *> &m = node.data.click.keyNode.type == AT_KEY ? m_rmapKey : m_rmapMouse;
            m.insert(node.data.click.keyNode.key, &node);
        } break;
        case KMT_CLICK_TWICE: {
            QMultiHash<int, KeyMapNode *> &m = node.data.clickTwice.keyNode.type == AT_KEY ? m_rmapKey : m_rmapMouse;
            m.insert(node.data.clickTwice.keyNode.key, &node);
        } break;
        case KMT_CLICK_MULTI: {
            QMultiHash<int, KeyMapNode *> &m = node.data.clickMulti.keyNode.type == AT_KEY ? m_rmapKey : m_rmapMouse;
            m.insert(node.data.clickMulti.keyNode.key, &node);
        } break;
        case KMT_STEER_WHEEL: {
            QMultiHash<int, KeyMapNode *> &ml = node.data.steerWheel.left.type == AT_KEY ? m_rmapKey : m_rmapMouse;
            ml.insert(node.data.steerWheel.left.key, &node);
            QMultiHash<int, KeyMapNode *> &mr = node.data.steerWheel.right.type == AT_KEY ? m_rmapKey : m_rmapMouse;
            mr.insert(node.data.steerWheel.right.key, &node);
            QMultiHash<int, KeyMapNode *> &mu = node.data.steerWheel.up.type == AT_KEY ? m_rmapKey : m_rmapMouse;
            mu.insert(node.data.steerWheel.up.key, &node);
            QMultiHash<int, KeyMapNode *> &md = node.data.steerWheel.down.type == AT_KEY ? m_rmapKey : m_rmapMouse;
            md.insert(node.data.steerWheel.down.key, &node);
            QMultiHash<int, KeyMapNode *> &mc = node.data.steerWheel.switchKey.type == AT_KEY ? m_rmapKey : m_rmapMouse;
            mc.insert(node.data.steerWheel.switchKey.key, &node);
        } break;
        case KMT_DRAG: {
            QMultiHash<int, KeyMapNode *> &m = node.data.drag.keyNode.type == AT_KEY ? m_rmapKey : m_rmapMouse;
            m.insert(node.data.drag.keyNode.key, &node);
        } break;
        case KMT_ANDROID_KEY: {
            QMultiHash<int, KeyMapNode *> &m = node.data.androidKey.keyNode.type == AT_KEY ? m_rmapKey : m_rmapMouse;
            m.insert(node.data.androidKey.keyNode.key, &node);
        } break;
        case KMT_ROTARY_TABLE: {
            QMultiHash<int, KeyMapNode *> &m = node.data.rotaryTable.keyNode.type == AT_KEY ? m_rmapKey : m_rmapMouse;
            m.insert(node.data.rotaryTable.keyNode.key, &node);
        } break;
        case KMT_DUAL_MODE: {
            QMultiHash<int, KeyMapNode *> &m = node.data.dualMode.accurate.type == AT_KEY ? m_rmapKey : m_rmapMouse;
            m.insert(node.data.dualMode.accurate.key, &node);
        } break;
        case KMT_PRESS_RELEASE: {
            QMultiHash<int, KeyMapNode *> &m = node.data.pressRelease.keyNode.type == AT_KEY ? m_rmapKey : m_rmapMouse;
            m.insert(node.data.pressRelease.keyNode.key, &node);
        }break;
        case KMT_MOBA_WHEEL: {
            QMultiHash<int, KeyMapNode *> &m = node.data.mobaWheel.keyNode.type == AT_KEY ? m_rmapKey : m_rmapMouse;
            m.insert(node.data.mobaWheel.keyNode.key, &node);
        } break;
        case KMT_MOBA_SKILL: {
            QMultiHash<int, KeyMapNode *> &m = node.data.mobaSkill.keyNode.type == AT_KEY ? m_rmapKey : m_rmapMouse;
            m.insert(node.data.mobaSkill.keyNode.key, &node);
        }break;
        case KMT_BURST_CLICK: {
            QMultiHash<int, KeyMapNode *> &m = node.data.burstClick.keyNode.type == AT_KEY ? m_rmapKey : m_rmapMouse;
            m.insert(node.data.burstClick.keyNode.key, &node);
        }
        default:
            break;
        }
    }
}

QString KeyMap::getItemString(const QJsonObject &node, const QString &name)
{
    return node.value(name).toString();
}

double KeyMap::getItemDouble(const QJsonObject &node, const QString &name)
{
    return node.value(name).toDouble();
}

bool KeyMap::getItemBool(const QJsonObject &node, const QString &name)
{
    return node.value(name).toBool(false);
}

QJsonObject KeyMap::getItemObject(const QJsonObject &node, const QString &name)
{
    return node.value(name).toObject();
}

QPointF KeyMap::getItemPos(const QJsonObject &node, const QString &name)
{
    QJsonObject pos = node.value(name).toObject();
    return QPointF(pos.value("x").toDouble(), pos.value("y").toDouble());
}

QPair<KeyMap::ActionType, int> KeyMap::getItemKey(const QJsonObject &node, const QString &name)
{
    QString value = getItemString(node, name);
    int key = m_metaEnumKey.keyToValue(value.toStdString().c_str());
    int btn = m_metaEnumMouseButtons.keyToValue(value.toStdString().c_str());
    if (key == -1 && btn == -1) {
        return { AT_INVALID, -1 };
    } else if (key != -1) {
        return { AT_KEY, key };
    } else {
        return { AT_MOUSE, btn };
    }
}

KeyMap::KeyMapType KeyMap::getItemKeyMapType(const QJsonObject &node, const QString &name)
{
    QString value = getItemString(node, name);
    return static_cast<KeyMap::KeyMapType>(m_metaEnumKeyMapType.keyToValue(value.toStdString().c_str()));
}

bool KeyMap::checkItemString(const QJsonObject &node, const QString &name)
{
    return node.contains(name) && node.value(name).isString();
}

bool KeyMap::checkItemDouble(const QJsonObject &node, const QString &name)
{
    return node.contains(name) && node.value(name).isDouble();
}

bool KeyMap::checkItemBool(const QJsonObject &node, const QString &name)
{
    return node.contains(name) && node.value(name).isBool();
}

bool KeyMap::checkItemObject(const QJsonObject &node, const QString &name)
{
    return node.contains(name) && node.value(name).isObject();
}

bool KeyMap::checkItemPos(const QJsonObject &node, const QString &name)
{
    if (node.contains(name) && node.value(name).isObject()) {
        QJsonObject pos = node.value(name).toObject();
        return pos.contains("x") && pos.value("x").isDouble() && pos.contains("y") && pos.value("y").isDouble();
    }
    return false;
}

bool KeyMap::checkForClick(const QJsonObject &node)
{
    return checkForClickTwice(node);
}

bool KeyMap::checkForClickMulti(const QJsonObject &node)
{
    bool ret = true;

    if (!node.contains("clickNodes") || !node.value("clickNodes").isArray()) {
        qWarning("json error: no find clickNodes");
        return false;
    }

    QJsonArray clickNodes = node.value("clickNodes").toArray();
    QJsonObject clickNode;
    int size = clickNodes.size();
    if (0 == size) {
        qWarning("json error: clickNodes is empty");
        return false;
    }

    for (int i = 0; i < size; i++) {
        if (!clickNodes.at(i).isObject()) {
            qWarning("json error: clickNodes node must be json object");
            ret = false;
            break;
        }

        clickNode = clickNodes.at(i).toObject();
        if (!checkForDelayClickNode(clickNode)) {
            ret = false;
            break;
        }
    }

    return ret;
}

bool KeyMap::checkForDelayClickNode(const QJsonObject &node)
{
    return checkItemPos(node, "pos") && checkItemDouble(node, "delay");
}

bool KeyMap::checkForClickTwice(const QJsonObject &node)
{
    return checkItemString(node, "key") && checkItemPos(node, "pos");
}

bool KeyMap::checkForAndroidKey(const QJsonObject &node)
{
    return checkItemString(node, "key") && checkItemDouble(node, "androidKey");
}

bool KeyMap::checkForSteerWhell(const QJsonObject &node)
{
    return checkItemString(node, "leftKey") && checkItemString(node, "rightKey") && checkItemString(node, "upKey") && checkItemString(node, "downKey")
           && checkItemDouble(node, "leftOffset") && checkItemDouble(node, "rightOffset") && checkItemDouble(node, "upOffset")
           && checkItemDouble(node, "downOffset") && checkItemPos(node, "centerPos");
}

bool KeyMap::checkForDrag(const QJsonObject &node)
{
    return checkItemString(node, "key") && checkItemPos(node, "startPos") && checkItemPos(node, "endPos");
}

bool KeyMap::checkForRotaryTable(const QJsonObject &node)
{
    return checkItemString(node, "key") && checkItemPos(node, "pos");
}

bool KeyMap::checkForDualMode(const QJsonObject &node)
{
    return checkItemString(node, "key") && checkItemObject(node, "accurate") && checkItemObject(node, "mouse");
}
void KeyMap::setDualMode(KeyMapNode &node, const QString &dualMode, KeyNode &keyNode, const KeyMapType type)
{
    if (dualMode.contains("accurate")) {
        node.data.dualMode.accurate = keyNode;
        node.data.dualMode.accurateType = type;
    }
    if (dualMode.contains("mouse")) {
        node.data.dualMode.mouse = keyNode;
        node.data.dualMode.mouseType = type;
    }
}
bool KeyMap::checkForPressRelease(const QJsonObject &node)
{
    return checkItemString(node, "key") && checkItemPos(node, "pressPos") && checkItemPos(node, "releasePos");
}

bool KeyMap::checkForMobaWheel(const QJsonObject &node) {
    return checkItemString(node, "key")
    && checkItemPos(node, "centerPos")
    && checkItemDouble(node, "speedRatio")
    && checkItemPos(node,"wheelPos")
    && checkItemDouble(node,"skillOffset");
}

void KeyMap::setMobaWheelMapNode(KeyMapNode &keyMapNode, const QJsonObject &node, const KeyMapType &type) {
    if (!checkForMobaWheel(node)) {
        qWarning() << "json error: KMT_MOBA_WHEEL keyMapNodes node format error";
        return;
    }
    QPair<ActionType, int> key = getItemKey(node, "key");
    if (key.first == AT_INVALID) {
        qWarning() << "json error: keyMapNodes node invalid key: " << node.value("key").toString();
        return;
    }
    keyMapNode.type = type;
    keyMapNode.data.mobaWheel.keyNode.type = key.first;
    keyMapNode.data.mobaWheel.keyNode.key = key.second;
    keyMapNode.data.mobaWheel.centerPos = getItemPos(node, "centerPos");
    keyMapNode.data.mobaWheel.speedRatio = getItemDouble(node, "speedRatio");
    keyMapNode.data.mobaWheel.wheelPos = getItemPos(node, "wheelPos");
    keyMapNode.data.mobaWheel.skillOffset = getItemDouble(node, "skillOffset");
    m_isValidMobaWheel = true;
}

void KeyMap::setMobaSkillMapNode(KeyMap::KeyMapNode &keyMapNode, const QJsonObject &node, KeyMap::KeyMapType type) {
    if (!checkForMobaSkill(node)) {
        qWarning() << "json error: KMT_MOBA_SKILL keyMapNodes node format error";
        return;
    }
    QPair<ActionType, int> key = getItemKey(node, "key");
    if (key.first == AT_INVALID) {
        qWarning() << "json error: keyMapNodes node invalid key: " << node.value("key").toString();
        return;
    }
    keyMapNode.type = type;
    keyMapNode.data.mobaSkill.keyNode.type = key.first;
    keyMapNode.data.mobaSkill.keyNode.key = key.second;
    keyMapNode.data.mobaSkill.keyNode.pos = getItemPos(node, "pos");
    keyMapNode.data.mobaSkill.speedRatio = getItemDouble(node, "speedRatio");
    keyMapNode.data.mobaSkill.stopMove = getItemBool(node, "stopMove");
    keyMapNode.data.mobaSkill.quickCast = getItemBool(node, "quickCast");
}

bool KeyMap::checkForMobaSkill(const QJsonObject &node) {
    return checkItemString(node, "key") && checkItemPos(node, "pos") && checkItemDouble(node, "speedRatio");
}

void KeyMap::setBurstClickMapNode(KeyMap::KeyMapNode &keyMapNode, const QJsonObject &node, KeyMap::KeyMapType type) {
    if (!checkForBurstClick(node)) {
        qWarning() << "json error: KMT_BURST_CLICK keyMapNodes node format error";
        return;
    }
    QPair<ActionType, int> key = getItemKey(node, "key");
    if (key.first == AT_INVALID) {
        qWarning() << "json error: keyMapNodes node invalid key: " << node.value("key").toString();
        return;
    }
    keyMapNode.type = type;
    keyMapNode.data.burstClick.keyNode.type = key.first;
    keyMapNode.data.burstClick.keyNode.key = key.second;
    keyMapNode.data.burstClick.keyNode.pos = getItemPos(node, "pos");
    keyMapNode.data.burstClick.rate = getItemDouble(node, "rate");
}

bool KeyMap::checkForBurstClick(const QJsonObject &node) {
    return checkItemString(node, "key") && checkItemPos(node, "pos")&& checkItemDouble(node, "rate");
}
