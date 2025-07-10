#ifndef KEYMAP_H
#define KEYMAP_H
#include <QJsonObject>
#include <QMetaEnum>
#include <QMultiHash>
#include <QObject>
#include <QPair>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <utility>

#include "keycodes.h"

#define MAX_DELAY_CLICK_NODES 50

class KeyMap : public QObject
{
    Q_OBJECT
public:
    enum KeyMapType
    {
        KMT_INVALID = -1,
        KMT_CLICK = 0,
        KMT_CLICK_TWICE,
        KMT_CLICK_MULTI,
        KMT_STEER_WHEEL,
        KMT_DRAG,
        KMT_MOUSE_MOVE,
        KMT_ANDROID_KEY,
        KMT_ROTARY_TABLE,
        KMT_DUAL_MODE,
        KMT_PRESS_RELEASE,
        KMT_MOBA_WHEEL,
        KMT_MOBA_SKILL,
        KMT_BURST_CLICK
    };
    Q_ENUM(KeyMapType)

    enum ActionType
    {
        AT_INVALID = -1,
        AT_KEY = 0,
        AT_MOUSE = 1,
    };
    Q_ENUM(ActionType)

    struct DelayClickNode
    {
        int delay = 0;
        QPointF pos = QPointF(0, 0);
        int pressTime = 0;
    };

    struct SecondNode
    {
        //        std::string comment;
        int time = 0;
    };

    struct KeyNode
    {
        ActionType type = AT_INVALID;
        int key = Qt::Key_unknown;
        int modifier = Qt::Key_unknown;
        QPointF pos = QPointF(0, 0);                           // normal key
        QPointF extendPos = QPointF(0, 0);                     // for drag
        double extendOffset = 0.0;                             // for steerWheel
        DelayClickNode delayClickNodes[MAX_DELAY_CLICK_NODES]; // for multi clicks
        int delayClickNodesCount = 0;
        AndroidKeycode androidKey = AKEYCODE_UNKNOWN; // for key press
                                                      //        bool repeat = false;
        SecondNode secondNodes[MAX_DELAY_CLICK_NODES];
        //        int secondNodesCount = 0;
        KeyNode(
            ActionType type = AT_INVALID,
            int key = Qt::Key_unknown,
            QPointF pos = QPointF(0, 0),
            QPointF extendPos = QPointF(0, 0),
            double extendOffset = 0.0,
            AndroidKeycode androidKey = AKEYCODE_UNKNOWN)
            : type(type), key(key), pos(pos), extendPos(extendPos), extendOffset(extendOffset), androidKey(androidKey)
        {
        }
    };

    struct KeyMapNode
    {
        KeyMapType type = KMT_INVALID;
        bool switchMap = false;
        bool forceSwitchOn = false;
        bool forceSwitchOff = false;
        bool freshMouseMove = false;
        bool focusOn = false;
        QPointF focusPos = { 0.0, 0.0 };
        union DATA
        {
            struct
            {
                KeyNode keyNode;
            } click;
            struct
            {
                KeyNode keyNode;
            } clickTwice;
            struct
            {
                KeyNode keyNode;
                double pressTime = 0;
            } clickMulti;
            struct
            {
                QPointF centerPos = { 0.0, 0.0 };
                KeyNode left, right, up, down, switchKey;
                KeyNode boost;
            } steerWheel;
            struct
            {
                KeyNode keyNode;
            } drag;
            struct
            {
                QPointF startPos = { 0.0, 0.0 };
                QPointF speedRatio = { 1.0, 1.0 };
                KeyNode smallEyes;
            } mouseMove;
            struct
            {
                KeyNode keyNode;
            } androidKey;
            struct
            {
                KeyNode keyNode;
                QPointF speedRatio = { 1.0, 1.0 };
            } rotaryTable;
            struct
            {
                KeyNode accurate;
                KeyMapType accurateType = KMT_INVALID;
                KeyNode mouse;
                KeyMapType mouseType = KMT_INVALID;
            } dualMode;
            struct
            {
                KeyNode keyNode;
                QPointF pressPos = { 0.0, 0.0 };
                QPointF releasePos = { 0.0, 0.0 };
            } pressRelease;
            struct {
                KeyNode keyNode;
                QPointF centerPos = {0.0, 0.0};
                QPointF wheelPos = {0.2, 0.75};
                double speedRatio = 3.0;
                double skillOffset = 1.0;
            } mobaWheel;
            struct {
                KeyNode keyNode;
                double speedRatio = 3.0;
                bool stopMove = false;
                bool quickCast = false;
            } mobaSkill;
            struct {
                KeyNode keyNode;
                double rate = 10;
            } burstClick;
            DATA() {}
            ~DATA() {}
        } data;
        KeyMapNode() {}
        ~KeyMapNode() {}
    };

    KeyMap(QObject *parent = Q_NULLPTR);
    virtual ~KeyMap();

    void loadKeyMap(const QString &json);
    const KeyMap::KeyMapNode &getKeyMapNode(int key);
    KeyMapNode getKeyMapNodeKey(int key);
    const KeyMap::KeyMapNode &getKeyMapNodeMouse(int key);
    bool isSwitchOnKeyboard();
    int getSwitchKey();

    bool isValidMouseMoveMap();
    bool isValidSteerWheelMap();

    bool isValidMobaWheel();

    const KeyMap::KeyMapNode &getMouseMoveMap();
    bool getCustomMouseClick() const;

private:
    // set up the reverse map from key/event event to keyMapNode
    void makeReverseMap();

    // safe check for base
    bool checkItemString(const QJsonObject &node, const QString &name);
    bool checkItemDouble(const QJsonObject &node, const QString &name);
    bool checkItemBool(const QJsonObject &node, const QString &name);
    bool checkItemObject(const QJsonObject &node, const QString &name);
    bool checkItemPos(const QJsonObject &node, const QString &name);

    // safe check for KeyMapNode
    bool checkForClick(const QJsonObject &node);
    bool checkForClickMulti(const QJsonObject &node);
    bool checkForDelayClickNode(const QJsonObject &node);
    bool checkForClickTwice(const QJsonObject &node);
    bool checkForSteerWhell(const QJsonObject &node);
    bool checkForDrag(const QJsonObject &node);
    bool checkForAndroidKey(const QJsonObject &node);
    bool checkForRotaryTable(const QJsonObject &node);
    bool checkForDualMode(const QJsonObject &node);

    // get keymap from json object
    QString getItemString(const QJsonObject &node, const QString &name);
    double getItemDouble(const QJsonObject &node, const QString &name);
    bool getItemBool(const QJsonObject &node, const QString &name);
    QJsonObject getItemObject(const QJsonObject &node, const QString &name);
    QPointF getItemPos(const QJsonObject &node, const QString &name);
    QPair<ActionType, int> getItemKey(const QJsonObject &node, const QString &name);
    KeyMapType getItemKeyMapType(const QJsonObject &node, const QString &name);

    void setSteerWheelSwitchMode(const QJsonObject &node, KeyMap::KeyMapNode &keyMapNode);

    void setClickMapNode(KeyMapNode &keyMapNode, const QJsonObject &node, const KeyMapType &type, const QString &dualMode);

private:
    static QString s_keyMapPath;

    QVector<KeyMapNode> m_keyMapNodes;
    KeyNode m_switchKey = { AT_KEY, Qt::Key_QuoteLeft };
    bool m_customMouseClick = false;
    // just for return
    KeyMapNode m_invalidNode;

    // steer wheel index
    int m_idxSteerWheel = -1;

    // moba wheel index
    bool m_isValidMobaWheel = false;

    // mouse move index
    int m_idxMouseMove = -1;

    // mapping of key/mouse event name to index
    QMetaEnum m_metaEnumKey = QMetaEnum::fromType<Qt::Key>();
    QMetaEnum m_metaEnumMouseButtons = QMetaEnum::fromType<Qt::MouseButtons>();
    QMetaEnum m_metaEnumKeyMapType = QMetaEnum::fromType<KeyMap::KeyMapType>();
    // reverse map of key/mouse event
    QMultiHash<int, KeyMapNode *> m_rmapKey;
    QMultiHash<int, KeyMapNode *> m_rmapMouse;
    void setCommonProperties(const QJsonObject &node, KeyMap::KeyMapNode &keyMapNode);
    void setClickTwiceMapNode(KeyMapNode &keyMapNode, const QJsonObject &node, const KeyMapType &type, const QString &dualMode);
    void setClickMultiMapNode(KeyMapNode &keyMapNode, const QJsonObject &node, const KeyMapType &type, const QString &dualMode);
    void setSteerWheelMapNode(KeyMapNode &keyMapNode, const QJsonObject &node, const KeyMapType &type);
    void setDragMapNode(KeyMapNode &keyMapNode, const QJsonObject &node, const KeyMapType &type, QString dualMode);
    void setAndroidKeyMapNode(KeyMapNode &keyMapNode, const QJsonObject &node, const KeyMapType &type, QString string);
    void setRotaryTableMapNode(KeyMapNode &keyMapNode, const QJsonObject &node, const KeyMapType &type, QString dualMode);
    void setDualModeMapNode(KeyMap::KeyMapNode &keyMapNode, const QJsonObject &node, const KeyMap::KeyMapType &type);
    void setKeyMapNode(const QJsonObject &node, KeyMapNode &keyMapNode, const QString &dualMode);
    static void setDualMode(KeyMapNode &node, const QString &dualMode, KeyNode &keyNode, KeyMapType type);
    void setPressReleaseMapNode(KeyMapNode &node, const QJsonObject &object, KeyMapType type, const QString &dualMode);
    bool checkForPressRelease(const QJsonObject &node);

    bool checkForMobaWheel(const QJsonObject &node);

    void setMobaWheelMapNode(const QJsonObject &node, KeyMapNode mapNode);

    void setMobaWheelMapNode(KeyMapNode &keyMapNode, const QJsonObject &node, const KeyMapType &type);

    void setMobaSkillMapNode(KeyMapNode &keyMapNode, const QJsonObject &node, KeyMapType type);

    bool checkForMobaSkill(const QJsonObject &node);

    void setBurstClickMapNode(KeyMap::KeyMapNode &keyMapNode, const QJsonObject &node, KeyMap::KeyMapType type);

    bool checkForBurstClick(const QJsonObject &node);

    void setBoost(const QJsonObject &node, KeyMapNode &keyMapNode);


    void setModifierKey(const QJsonObject &node, KeyMapNode &keyMapNode, QPair<ActionType, int> key);
};

#endif // KEYMAP_H
