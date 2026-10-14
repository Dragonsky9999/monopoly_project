#include "CardVisual.hpp"
#include "DataMaster.hpp"
#include <QMimeData>
#include <QPixmap>

namespace CardVisual {

QColor groupColor(GroupType g) {
    switch (g) {
        case GroupType::BROWN: return QColor(0x8B, 0x4A, 0x2B);
        case GroupType::SKY_BLUE: return QColor(0x8F, 0xD3, 0xF5);
        case GroupType::PINK: return QColor(0xD9, 0x3A, 0x96);
        case GroupType::ORANGE: return QColor(0xF7, 0x94, 0x1D);
        case GroupType::RED: return QColor(0xE1, 0x1B, 0x22);
        case GroupType::YELLOW: return QColor(0xFE, 0xE1, 0x00);
        case GroupType::GREEN: return QColor(0x1F, 0xA2, 0x55);
        case GroupType::DARK_BLUE: return QColor(0x00, 0x48, 0x9C);
        case GroupType::RAILROAD: return QColor(0x10, 0x10, 0x10);
        case GroupType::UTILITY: return QColor(0x32, 0xCD, 0x32);
    }
    return Qt::gray;
}

QColor textColorOn(const QColor& bg) {
    double l = 0.299 * bg.red() + 0.587 * bg.green() + 0.114 * bg.blue();
    return l > 150 ? QColor(20, 20, 20) : QColor(255, 255, 255);
}

QString groupLabel(GroupType g) {
    switch (g) {
        case GroupType::BROWN: return QStringLiteral("茶");
        case GroupType::SKY_BLUE: return QStringLiteral("水色");
        case GroupType::PINK: return QStringLiteral("桃");
        case GroupType::ORANGE: return QStringLiteral("橙");
        case GroupType::RED: return QStringLiteral("赤");
        case GroupType::YELLOW: return QStringLiteral("黄");
        case GroupType::GREEN: return QStringLiteral("緑");
        case GroupType::DARK_BLUE: return QStringLiteral("紺");
        case GroupType::RAILROAD: return QStringLiteral("鉄道");
        case GroupType::UTILITY: return QStringLiteral("公共");
    }
    return QStringLiteral("?");
}

QString cardNumber(CardId id) {
    std::string code = DataMaster::toString(id);
    return code.empty() ? QString() : QString(QChar(code.back()));
}

QString cardShortLabel(CardId id) {
    if (id == CardId::NONE) return QStringLiteral("-");
    return groupLabel(DataMaster::card(id).group) + cardNumber(id);
}

QColor playerColor(int id) {
    static const QColor colors[] = {
        QColor(0x1E, 0x88, 0xE5), QColor(0xE5, 0x39, 0x35), QColor(0x43, 0xA0, 0x47), QColor(0xFB, 0x8C, 0x00),
        QColor(0x8E, 0x24, 0xAA), QColor(0x00, 0xAC, 0xC1), QColor(0x6D, 0x4C, 0x41), QColor(0xD8, 0x1B, 0x60),
        QColor(0x39, 0x49, 0xAB), QColor(0x7C, 0xB3, 0x42)};
    if (id < 0) return QColor(120, 120, 120);
    return colors[id % 10];
}

void paintChip(QPainter& p, const QRectF& r, CardId id, const CardState* st) {
    if (id == CardId::NONE) return;
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    QColor bg = groupColor(DataMaster::card(id).group);
    p.setPen(QPen(QColor(0, 0, 0, 120), 1));
    p.setBrush(bg);
    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);

    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(qMax(8, int(qMin(r.width(), r.height()) * 0.55)));
    p.setFont(f);
    p.setPen(textColorOn(bg));
    p.drawText(r, Qt::AlignCenter, cardNumber(id));

    if (st) {
        if (st->houses > 0) {
            if (st->houses >= 5) {
                QRectF h(r.left() + r.width() * 0.2, r.top() + 2, r.width() * 0.6, r.height() * 0.18);
                p.setPen(QPen(Qt::black, 0.8));
                p.setBrush(QColor(0xD3, 0x2F, 0x2F));
                p.drawRect(h);
            } else {
                double s = qMax(3.0, r.width() / 6.0);
                double gap = 1.0;
                double total = st->houses * s + (st->houses - 1) * gap;
                double x = r.center().x() - total / 2;
                p.setPen(QPen(Qt::black, 0.8));
                p.setBrush(QColor(0x2E, 0x7D, 0x32));
                for (int i = 0; i < st->houses; ++i) p.drawRect(QRectF(x + i * (s + gap), r.top() + 2, s, s));
            }
        }
        if (st->mortgaged) {
            p.setPen(Qt::NoPen);
            p.setBrush(QBrush(QColor(255, 255, 255, 170), Qt::BDiagPattern));
            p.drawRoundedRect(r, 3, 3);
            p.setBrush(QColor(80, 80, 80, 110));
            p.drawRoundedRect(r, 3, 3);
        }
    }
    p.restore();
}

QIcon cardIcon(CardId id, const CardState* st, int size) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    paintChip(p, QRectF(0, 0, size, size), id, st);
    return QIcon(pm);
}

QIcon playerIcon(int playerId, int size) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(Qt::black, 1));
    p.setBrush(playerColor(playerId));
    p.drawEllipse(QRectF(1, 1, size - 2, size - 2));
    return QIcon(pm);
}

QString playerName(const GameState& s, int id) {
    if (id >= 0 && id < static_cast<int>(s.players.size())) return QString::fromStdString(s.players[id].name);
    return QStringLiteral("銀行");
}

QString cardTooltip(CardId id, const GameState& s) {
    const Card& c = DataMaster::card(id);
    const CardState& st = s.card(id);
    QString t = QStringLiteral("<b>%1</b> &nbsp;<span style='color:gray'>(%2)</span><br>")
                    .arg(cardShortLabel(id), QString::fromStdString(c.code));
    t += QStringLiteral("価格 %1万 / 抵当 %2万<br>").arg(c.price).arg(c.mortgage);
    if (c.kind == CardKind::PROPERTY) {
        t += QStringLiteral("家 %1万<br>地代 %2 / 家1 %3 / 家2 %4 / 家3 %5 / 家4 %6 / ホテル %7<br>")
                 .arg(c.housePrice).arg(c.rent[0]).arg(c.rent[1]).arg(c.rent[2]).arg(c.rent[3]).arg(c.rent[4]).arg(c.rent[5]);
    } else if (c.kind == CardKind::RAILROAD) {
        t += QStringLiteral("地代 1枚 25 / 2枚 50 / 3枚 100 / 4枚 200<br>");
    } else {
        t += QStringLiteral("地代 1枚 出目×4 / 2枚 出目×10<br>");
    }
    t += QStringLiteral("所有: <b>%1</b>").arg(playerName(s, st.owner));
    if (st.houses == 5) t += QStringLiteral(" / ホテル");
    else if (st.houses > 0) t += QStringLiteral(" / 家%1軒").arg(st.houses);
    if (st.mortgaged) t += QStringLiteral(" / <span style='color:#c00'>抵当中</span>");
    return t;
}

}  // namespace CardVisual

namespace DragData {
namespace {
const QString kCardMime = QStringLiteral("application/x-monopoly-card");
const QString kMoneyMime = QStringLiteral("application/x-monopoly-money");
}  // namespace

QMimeData* makeCard(CardId id, int fromPlayer) {
    auto* m = new QMimeData;
    m->setData(kCardMime, QByteArray::number(cardIndex(id)) + ";" + QByteArray::number(fromPlayer));
    return m;
}

QMimeData* makeMoney(int fromPlayer) {
    auto* m = new QMimeData;
    m->setData(kMoneyMime, QByteArray::number(fromPlayer));
    return m;
}

bool readCard(const QMimeData* m, CardId& id, int& fromPlayer) {
    if (!m || !m->hasFormat(kCardMime)) return false;
    const QList<QByteArray> parts = m->data(kCardMime).split(';');
    if (parts.size() != 2) return false;
    id = cardFromIndex(parts[0].toInt());
    fromPlayer = parts[1].toInt();
    return cardIndex(id) >= 0 && cardIndex(id) < CARD_COUNT;
}

bool readMoney(const QMimeData* m, int& fromPlayer) {
    if (!m || !m->hasFormat(kMoneyMime)) return false;
    fromPlayer = m->data(kMoneyMime).toInt();
    return true;
}

bool hasAny(const QMimeData* m) { return m && (m->hasFormat(kCardMime) || m->hasFormat(kMoneyMime)); }

QPixmap moneyPixmap(int size) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(0x8D, 0x6E, 0x00), 1.5));
    p.setBrush(QColor(0xFF, 0xC1, 0x07));
    p.drawEllipse(QRectF(1.5, 1.5, size - 3, size - 3));
    QFont f;
    f.setBold(true);
    f.setPixelSize(int(size * 0.5));
    p.setFont(f);
    p.setPen(QColor(0x5D, 0x40, 0x00));
    p.drawText(QRectF(0, 0, size, size), Qt::AlignCenter, QStringLiteral("¥"));
    return pm;
}
}  // namespace DragData
