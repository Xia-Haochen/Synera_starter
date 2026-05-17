#include "gui/unititem.h"
#include "entity/unit.h"
#include <QCoreApplication>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QFileInfo>

UnitItem::UnitItem(Unit* unit, QGraphicsItem* parent)
    : QGraphicsObject(parent)
    , m_unit(unit)
    , m_gridPos(-1, -1)
    , m_dragging(false)
    , m_spriteTried(false)
{
    // 当前仅接受左键拖拽。
    setAcceptedMouseButtons(Qt::LeftButton);
}

QRectF UnitItem::boundingRect() const
{
    return QRectF(-42, -42, 84, 84);
}

void UnitItem::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    painter->setRenderHint(QPainter::Antialiasing);

    // 优先绘制角色贴图；贴图不存在时回退为几何占位图。
    ensureSpriteLoaded();

    if (!m_sprite.isNull()) {
        const QRectF targetRect(-40, -40, 80, 80);
        painter->drawPixmap(targetRect, m_sprite, m_sprite.rect());
    } else {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(20, 20, 20, 110));
        painter->drawEllipse(QRectF(-14, 8, 28, 10));

        QPolygonF badge;
        badge << QPointF(0, -15)
              << QPointF(13, -7)
              << QPointF(13, 7)
              << QPointF(0, 15)
              << QPointF(-13, 7)
              << QPointF(-13, -7);

        painter->setPen(QPen(QColor(18, 18, 18), 1.5));
        painter->setBrush(QColor(100, 150, 200));
        painter->drawPolygon(badge);

        if (m_unit) {
            painter->setPen(Qt::white);
            QFont font = painter->font();
            font.setPointSize(12);
            font.setBold(true);
            painter->setFont(font);

            QString jobChar;
            switch (m_unit->get_job()) {
            case JobType::Warrior: jobChar = QStringLiteral("战"); break;
            case JobType::Mage:    jobChar = QStringLiteral("法"); break;
            case JobType::Archer:  jobChar = QStringLiteral("弓"); break;
            default:               jobChar = QStringLiteral("?"); break;
            }
            painter->drawText(QRectF(-13, -9, 26, 26), Qt::AlignCenter, jobChar);
        }
    }

    // 绘制血条、蓝条和属性面板
    if (m_unit) {
        // 血条背景（稍微加高）
        QRectF hpBgRect(-20, -25, 40, 6);
        painter->setPen(Qt::NoPen);
        painter->setBrush(Qt::black);
        painter->drawRect(hpBgRect);

        // 血条前端
        if (m_unit->get_maxHp() > 0) {
            qreal hpRatio = qBound(0.0, static_cast<qreal>(m_unit->get_hp()) / m_unit->get_maxHp(), 1.0);
            QRectF hpRect(-20, -25, 40 * hpRatio, 6);
            painter->setBrush(Qt::green);
            painter->drawRect(hpRect);
        }

        // 蓝条背景
        QRectF manaBgRect(-20, -18, 40, 6);
        painter->setBrush(Qt::black);
        painter->drawRect(manaBgRect);

        // 蓝条前端
        if (m_unit->get_maxMana() > 0) {
            qreal manaRatio = qBound(0.0, static_cast<qreal>(m_unit->get_mana()) / m_unit->get_maxMana(), 1.0);
            QRectF manaRect(-20, -18, 40 * manaRatio, 6);
            painter->setBrush(Qt::blue);
            painter->drawRect(manaRect);
        }

        // 血条与蓝条的数值标注
        QFont valueFont = painter->font();
        valueFont.setPointSize(7);
        valueFont.setBold(false);
        painter->setFont(valueFont);
        painter->setPen(QColor(200, 200, 200));
        QString hpVal = QString("%1/%2").arg(m_unit->get_hp()).arg(m_unit->get_maxHp());
        painter->drawText(QRectF(22, -25, 60, 6), Qt::AlignVCenter | Qt::AlignLeft, hpVal);
        QString manaVal = QString("%1/%2").arg(m_unit->get_mana()).arg(m_unit->get_maxMana());
        painter->drawText(QRectF(22, -18, 60, 6), Qt::AlignVCenter | Qt::AlignLeft, manaVal);

        // 属性面板：攻击力显示在上方
        painter->setPen(Qt::white);
        QFont statsFont = painter->font();
        statsFont.setPointSize(9);
        statsFont.setBold(false);
        painter->setFont(statsFont);
        QString atkStr = QString("A:%1").arg(m_unit->get_atk());
        painter->drawText(QRectF(-30, -36, 60, 15), Qt::AlignCenter, atkStr);

        // 星级显示在单位底部
        QFont starFont = painter->font();
        starFont.setPointSize(9);
        starFont.setBold(true);
        painter->setFont(starFont);
        painter->setPen(QColor(255, 215, 0)); // 金色
        QString starStr;
        int starLevel = m_unit->get_starLevel();
        for (int i = 0; i < starLevel; ++i) {
            starStr += QStringLiteral("★");
        }
        painter->drawText(QRectF(-30, 26, 60, 14), Qt::AlignCenter, starStr);

        // 行动高亮：正在行动的单位显示金色边框
        if (m_highlighted) {
            painter->setPen(QPen(QColor(255, 215, 0), 3));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(QRectF(-41, -41, 82, 82), 6, 6);
        }
    }
}

void UnitItem::ensureSpriteLoaded() const
{
    if (m_spriteTried) {
        return;
    }

    m_spriteTried = true;
    const QString relativePath = spriteRelativePathForUnit();
    if (relativePath.isEmpty()) {
        return;
    }

    // 兼容常见运行目录：可执行文件同级上溯 1~2 层寻找 assets。
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString roots[] = {
        QFileInfo(appDir + "/..").canonicalFilePath(),
        QFileInfo(appDir + "/../..").canonicalFilePath()
    };

    QPixmap pix;
    for (const QString& root : roots) {
        if (root.isEmpty()) {
            continue;
        }
        pix.load(root + "/" + relativePath);
        if (!pix.isNull()) {
            break;
        }
    }

    if (pix.isNull()) {
        return;
    }

    m_sprite = pix.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QString UnitItem::spriteRelativePathForUnit() const
{
    if (!m_unit) {
        return QString();
    }

    const QString name = m_unit->name();

    return QString();
}

int UnitItem::unitId() const
{
    return m_unit ? m_unit->id() : -1;
}

void UnitItem::setGridPos(const QPoint& gridPos)
{
    m_gridPos = gridPos;
}

void UnitItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QGraphicsObject::mousePressEvent(event);
        return;
    }

    // 进入拖拽态并上报拖拽起点。
    m_dragging = true;
    emit dragStarted(unitId(), m_gridPos, event->scenePos());
    event->accept();
}

void UnitItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (!m_dragging) {
        QGraphicsObject::mouseMoveEvent(event);
        return;
    }

    // 拖拽过程中持续上报鼠标场景坐标。
    emit dragMoved(unitId(), m_gridPos, event->scenePos());
    event->accept();
}

void UnitItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (!m_dragging || event->button() != Qt::LeftButton) {
        QGraphicsObject::mouseReleaseEvent(event);
        return;
    }

    // 结束拖拽并交由 Game 决定是否落子成功。
    m_dragging = false;
    emit dragDropped(unitId(), m_gridPos, event->scenePos());
    event->accept();
}
