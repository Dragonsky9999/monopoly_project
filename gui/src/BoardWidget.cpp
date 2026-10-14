#include "BoardWidget.hpp"
#include "CardVisual.hpp"
#include "DataMaster.hpp"
#include <QApplication>
#include <QDrag>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <cmath>

namespace {
constexpr double kCorner = 1.6;            // 角マスの大きさ（辺マス幅 = 1）
constexpr double kUnits = 2 * kCorner + 9;  // 盤面一辺のユニット数
}  // namespace

BoardWidget::BoardWidget(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void BoardWidget::setState(const GameState* s) {
    state = s;
    update();
}

void BoardWidget::setSelectedCard(CardId id) {
    selected = id;
    update();
}

QRectF BoardWidget::boardRect() const {
    double s = qMin(width(), height()) - 8.0;
    return QRectF((width() - s) / 2.0, (height() - s) / 2.0, s, s);
}

BoardWidget::Side BoardWidget::sideOf(int pos) const {
    if (pos % 10 == 0) return Side::Corner;
    if (pos < 10) return Side::Bottom;
    if (pos < 20) return Side::Left;
    if (pos < 30) return Side::Top;
    return Side::Right;
}

QRectF BoardWidget::cellRect(int pos) const {
    QRectF b = boardRect();
    double u = b.width() / kUnits;
    double c = kCorner * u;
    double L = b.left(), T = b.top(), S = b.width();
    if (pos == 0) return QRectF(L + S - c, T + S - c, c, c);
    if (pos < 10) return QRectF(L + S - c - pos * u, T + S - c, u, c);
    if (pos == 10) return QRectF(L, T + S - c, c, c);
    if (pos < 20) return QRectF(L, T + S - c - (pos - 10) * u, c, u);
    if (pos == 20) return QRectF(L, T, c, c);
    if (pos < 30) return QRectF(L + c + (pos - 21) * u, T, u, c);
    if (pos == 30) return QRectF(L + S - c, T, c, c);
    return QRectF(L + S - c, T + c + (pos - 31) * u, c, u);
}

int BoardWidget::cellAt(const QPointF& pt) const {
    for (int i = 0; i < DataMaster::BOARD_SIZE; ++i)
        if (cellRect(i).contains(pt)) return i;
    return -1;
}

void BoardWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), palette().window());

    QRectF b = boardRect();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xDD, 0xEE, 0xDD));
    p.drawRect(b);

    for (int i = 0; i < DataMaster::BOARD_SIZE; ++i) paintCell(p, i);
    paintCenter(p);

    p.setPen(QPen(QColor(40, 40, 40), 2));
    p.setBrush(Qt::NoBrush);
    p.drawRect(b);
}

void BoardWidget::paintCell(QPainter& p, int pos) {
    const QRectF r = cellRect(pos);
    const auto& sq = DataMaster::board()[pos];
    const Side side = sideOf(pos);
    const double u = boardRect().width() / kUnits;

    p.save();
    p.setPen(QPen(QColor(60, 60, 60), 1));
    p.setBrush(QColor(0xF4, 0xF8, 0xF0));
    p.drawRect(r);

    // 色帯は盤面の内側の辺に付ける
    const double band = u * 0.42;
    QRectF bandR, content;
    switch (side) {
        case Side::Bottom: bandR = QRectF(r.left(), r.top(), r.width(), band); content = r.adjusted(0, band, 0, 0); break;
        case Side::Left: bandR = QRectF(r.right() - band, r.top(), band, r.height()); content = r.adjusted(0, 0, -band, 0); break;
        case Side::Top: bandR = QRectF(r.left(), r.bottom() - band, r.width(), band); content = r.adjusted(0, 0, 0, -band); break;
        case Side::Right: bandR = QRectF(r.left(), r.top(), band, r.height()); content = r.adjusted(band, 0, 0, 0); break;
        case Side::Corner: content = r; break;
    }

    QFont base = p.font();
    const CardState* st = nullptr;

    if (sq.type == DataMaster::SquareType::CARD) {
        st = state ? &state->card(sq.card) : nullptr;
        // 所有者の色で薄く塗る
        if (st && st->owner >= 0) {
            QColor oc = CardVisual::playerColor(st->owner);
            oc.setAlpha(70);
            p.setPen(Qt::NoPen);
            p.setBrush(oc);
            p.drawRect(content);
        }
        // 色帯 + 数字
        QColor gc = CardVisual::groupColor(DataMaster::card(sq.card).group);
        p.setPen(QPen(QColor(60, 60, 60), 1));
        p.setBrush(gc);
        p.drawRect(bandR);
        QFont f = base;
        f.setBold(true);
        f.setPixelSize(qMax(8, int(band * 0.8)));
        p.setFont(f);
        p.setPen(CardVisual::textColorOn(gc));
        p.drawText(bandR, Qt::AlignCenter, CardVisual::cardNumber(sq.card));

        if (st && st->owner >= 0) {
            // 家・ホテル（色帯の近くに並べる）
            if (st->houses > 0) {
                const bool hotel = st->houses >= 5;
                const int n = hotel ? 1 : st->houses;
                const double hs = u * 0.17;
                const bool horiz = (side == Side::Bottom || side == Side::Top);
                QPointF start;
                double w = hotel ? hs * 2.6 : hs, h = hs;
                double total = n * (horiz ? w : h) + (n - 1) * 2;
                if (!horiz) std::swap(w, h);
                if (side == Side::Bottom) start = QPointF(content.center().x() - total / 2, content.top() + 3);
                else if (side == Side::Top) start = QPointF(content.center().x() - total / 2, content.bottom() - 3 - h);
                else if (side == Side::Left) start = QPointF(content.right() - 3 - w, content.center().y() - total / 2);
                else start = QPointF(content.left() + 3, content.center().y() - total / 2);
                p.setPen(QPen(Qt::black, 0.8));
                p.setBrush(hotel ? QColor(0xD3, 0x2F, 0x2F) : QColor(0x2E, 0x7D, 0x32));
                for (int i = 0; i < n; ++i) {
                    QPointF o = horiz ? QPointF(start.x() + i * (w + 2), start.y()) : QPointF(start.x(), start.y() + i * (h + 2));
                    p.drawRect(QRectF(o, QSizeF(w, h)));
                }
            }
            // 所有者マーク（外側の隅に小さな丸）
            const double om = u * 0.2;
            QPointF oc;
            switch (side) {
                case Side::Bottom: oc = QPointF(content.right() - om * 0.8, content.bottom() - om * 0.8); break;
                case Side::Top: oc = QPointF(content.right() - om * 0.8, content.top() + om * 0.8); break;
                case Side::Left: oc = QPointF(content.left() + om * 0.8, content.bottom() - om * 0.8); break;
                default: oc = QPointF(content.right() - om * 0.8, content.bottom() - om * 0.8); break;
            }
            p.setPen(QPen(Qt::black, 0.8));
            p.setBrush(CardVisual::playerColor(st->owner));
            p.drawEllipse(oc, om * 0.55, om * 0.55);

            if (st->mortgaged) {
                p.setPen(Qt::NoPen);
                p.setBrush(QBrush(QColor(90, 90, 90, 150), Qt::BDiagPattern));
                p.drawRect(r);
                QFont mf = base;
                mf.setPixelSize(qMax(7, int(u * 0.2)));
                mf.setBold(true);
                p.setFont(mf);
                p.setPen(QColor(150, 0, 0));
                p.drawText(content, Qt::AlignCenter, QStringLiteral("抵当"));
            }
        }
    } else {
        QFont f = base;
        f.setBold(true);
        p.setPen(QColor(40, 40, 40));
        if (sq.type == DataMaster::SquareType::GO) {
            f.setPixelSize(qMax(10, int(u * 0.5)));
            p.setFont(f);
            p.drawText(r.adjusted(0, 0, 0, -r.height() * 0.25), Qt::AlignCenter, QStringLiteral("GO"));
            f.setPixelSize(qMax(7, int(u * 0.2)));
            f.setBold(false);
            p.setFont(f);
            QString bonus = state ? QStringLiteral("+%1万").arg(state->settings.goBonus) : QString();
            p.drawText(r.adjusted(0, r.height() * 0.35, 0, 0), Qt::AlignCenter, bonus + QStringLiteral("\n← 進行方向"));
        } else if (sq.type == DataMaster::SquareType::TAX) {
            const bool on = !state || state->settings.taxEnabled;
            f.setPixelSize(qMax(7, int(u * 0.22)));
            p.setFont(f);
            p.setPen(on ? QColor(120, 0, 0) : QColor(170, 170, 170));
            p.drawText(r, Qt::AlignCenter, on ? QStringLiteral("TAX\n%1").arg(sq.tax) : QStringLiteral("(TAX)"));
        }
    }

    // マス番号（小さく）
    QFont nf = base;
    nf.setPixelSize(qMax(6, int(u * 0.14)));
    p.setFont(nf);
    p.setPen(QColor(140, 140, 140));
    p.drawText(r.adjusted(2, 1, -2, -1), Qt::AlignLeft | Qt::AlignBottom, QString::number(pos));

    // 駒
    if (state) {
        std::vector<int> here;
        for (const auto& pl : state->players)
            if (!pl.bankrupt && pl.position == pos) here.push_back(pl.id);
        if (!here.empty()) {
            const double rad = qMin(qMin(content.width(), content.height()) * 0.17, u * 0.15);
            const int perRow = qMax(1, int(content.width() / (rad * 2.3)));
            for (size_t i = 0; i < here.size(); ++i) {
                int row = int(i) / perRow, col = int(i) % perRow;
                const double baseY = (side == Side::Corner) ? content.bottom() - rad * 1.3 - (int(here.size() - 1) / perRow) * rad * 2.2
                                                            : content.center().y() - rad * 0.6;
                QPointF c(content.left() + rad * 1.2 + col * rad * 2.3, baseY + row * rad * 2.2);
                if (c.y() + rad > content.bottom()) c.setY(content.bottom() - rad);
                const bool active = (here[i] == state->activePlayer &&
                                     (state->phase == Phase::OPERATIONAL || state->phase == Phase::PURCHASE));
                p.setPen(QPen(active ? QColor(255, 215, 0) : Qt::black, active ? 2.5 : 1));
                p.setBrush(CardVisual::playerColor(here[i]));
                p.drawEllipse(c, rad, rad);
                QFont tf = base;
                tf.setBold(true);
                tf.setPixelSize(qMax(7, int(rad * 1.2)));
                p.setFont(tf);
                p.setPen(Qt::white);
                p.drawText(QRectF(c.x() - rad, c.y() - rad, rad * 2, rad * 2), Qt::AlignCenter, QString::number(here[i]));
            }
        }
    }

    // 選択・未決定の強調
    if (sq.type == DataMaster::SquareType::CARD) {
        if (state && state->pendingPurchase == sq.card) {
            p.setPen(QPen(QColor(255, 140, 0), 3, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRect(r.adjusted(2, 2, -2, -2));
        }
        if (sq.card == selected) {
            p.setPen(QPen(QColor(255, 200, 0), 4));
            p.setBrush(Qt::NoBrush);
            p.drawRect(r.adjusted(1, 1, -1, -1));
        }
    }
    p.restore();
}

void BoardWidget::paintDie(QPainter& p, const QRectF& r, int v) {
    p.setPen(QPen(Qt::black, 1.2));
    p.setBrush(Qt::white);
    p.drawRoundedRect(r, r.width() * 0.15, r.width() * 0.15);
    if (v < 1 || v > 6) return;
    auto dot = [&](double fx, double fy) {
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::black);
        p.drawEllipse(QPointF(r.left() + r.width() * fx, r.top() + r.height() * fy), r.width() * 0.09, r.width() * 0.09);
    };
    const double a = 0.25, m = 0.5, z = 0.75;
    if (v % 2 == 1) dot(m, m);
    if (v >= 2) { dot(a, a); dot(z, z); }
    if (v >= 4) { dot(z, a); dot(a, z); }
    if (v == 6) { dot(a, m); dot(z, m); }
}

void BoardWidget::paintCenter(QPainter& p) {
    QRectF b = boardRect();
    double u = b.width() / kUnits;
    double c = kCorner * u;
    QRectF center = b.adjusted(c + u * 0.3, c + u * 0.3, -c - u * 0.3, -c - u * 0.3);

    p.save();
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(qMax(14, int(u * 0.75)));
    p.setFont(f);
    p.setPen(QColor(0x2E, 0x5E, 0x2E));
    QRectF title(center.left(), center.top(), center.width(), u * 1.2);
    p.drawText(title, Qt::AlignCenter, QStringLiteral("BUSINESS MONOPOLY"));

    if (!state) {
        p.restore();
        return;
    }
    const GameState& s = *state;
    QString phase;
    switch (s.phase) {
        case Phase::PURCHASE: phase = QStringLiteral("① 購入フェーズ"); break;
        case Phase::NEGOTIATION: phase = QStringLiteral("② 交渉フェーズ"); break;
        case Phase::OPERATIONAL: phase = QStringLiteral("③ 運用フェーズ  ターン %1").arg(s.turn); break;
        case Phase::FINISHED: phase = QStringLiteral("ゲーム終了"); break;
    }
    f.setPixelSize(qMax(11, int(u * 0.36)));
    p.setFont(f);
    p.setPen(QColor(30, 30, 30));
    QRectF line(center.left(), title.bottom() + u * 0.2, center.width(), u * 0.6);
    p.drawText(line, Qt::AlignCenter, phase);

    if (s.phase == Phase::PURCHASE || s.phase == Phase::OPERATIONAL) {
        line.translate(0, u * 0.7);
        const Player& ap = s.players[s.activePlayer];
        QString who = QStringLiteral("手番: %1").arg(QString::fromStdString(ap.name));
        p.setPen(CardVisual::playerColor(ap.id).darker(130));
        p.drawText(line, Qt::AlignCenter, who);
    } else if (s.phase == Phase::FINISHED) {
        for (const auto& pl : s.players)
            if (!pl.bankrupt) {
                line.translate(0, u * 0.7);
                p.drawText(line, Qt::AlignCenter, QStringLiteral("勝者: %1").arg(QString::fromStdString(pl.name)));
            }
    }

    if (s.phase == Phase::OPERATIONAL && s.hasRolled) {
        double ds = u * 0.9;
        QRectF d1(center.center().x() - ds - u * 0.1, center.center().y() - ds / 2, ds, ds);
        QRectF d2(center.center().x() + u * 0.1, center.center().y() - ds / 2, ds, ds);
        paintDie(p, d1, s.lastDice1);
        paintDie(p, d2, s.lastDice2);
        f.setPixelSize(qMax(10, int(u * 0.3)));
        p.setFont(f);
        p.setPen(QColor(30, 30, 30));
        p.drawText(QRectF(center.left(), d1.bottom() + 4, center.width(), u * 0.5), Qt::AlignCenter,
                   QStringLiteral("出目 %1").arg(s.lastDice1 + s.lastDice2));
    }
    if (s.pendingPurchase != CardId::NONE) {
        f.setPixelSize(qMax(10, int(u * 0.3)));
        p.setFont(f);
        double cs = u * 0.7;
        double y = center.center().y() + u * 1.3;
        QString msg = QStringLiteral("未所有マス: 買う / 見送る / オークション");
        double tw = p.fontMetrics().horizontalAdvance(msg);
        QRectF chip(center.center().x() - (tw + cs + 8) / 2, y, cs, cs);
        CardVisual::paintChip(p, chip, s.pendingPurchase);
        p.setPen(QColor(200, 100, 0));
        p.drawText(QRectF(chip.right() + 8, y, tw + 4, cs), Qt::AlignVCenter | Qt::AlignLeft, msg);
    }
    p.restore();
}

void BoardWidget::mousePressEvent(QMouseEvent* e) {
    pressCard = CardId::NONE;
    int pos = cellAt(e->position());
    if (pos < 0) return;
    CardId id = DataMaster::board()[pos].card;
    if (id == CardId::NONE) return;
    pressPos = e->position();
    pressCard = id;
    selected = id;
    update();
    emit cardClicked(id);
}

// マスからカードをドラッグ → プレイヤー一覧にドロップ（未所有なら購入、所有済みなら取引）
void BoardWidget::mouseMoveEvent(QMouseEvent* e) {
    if (!(e->buttons() & Qt::LeftButton) || pressCard == CardId::NONE || !state) return;
    if ((e->position() - pressPos).manhattanLength() < QApplication::startDragDistance()) return;
    auto* drag = new QDrag(this);
    drag->setMimeData(DragData::makeCard(pressCard, state->card(pressCard).owner));
    drag->setPixmap(CardVisual::cardIcon(pressCard, &state->card(pressCard), 32).pixmap(32, 32));
    drag->setHotSpot(QPoint(16, 16));
    pressCard = CardId::NONE;
    drag->exec(Qt::MoveAction);
}

bool BoardWidget::event(QEvent* e) {
    if (e->type() == QEvent::ToolTip && state) {
        auto* he = static_cast<QHelpEvent*>(e);
        int pos = cellAt(he->pos());
        if (pos >= 0) {
            const auto& sq = DataMaster::board()[pos];
            QString tip;
            if (sq.type == DataMaster::SquareType::CARD)
                tip = CardVisual::cardTooltip(sq.card, *state) + QStringLiteral("<br><i>プレイヤーへドラッグ: 未所有なら購入 / 所有済みなら取引</i>");
            else if (sq.type == DataMaster::SquareType::GO) tip = QStringLiteral("GO（通過で +%1万）").arg(state->settings.goBonus);
            else if (sq.type == DataMaster::SquareType::TAX)
                tip = state->settings.taxEnabled ? QStringLiteral("税金 %1万").arg(sq.tax) : QStringLiteral("税金マス（オフ）");
            QStringList who;
            for (const auto& pl : state->players)
                if (!pl.bankrupt && pl.position == pos) who << QString::fromStdString(pl.name);
            if (!who.isEmpty()) tip += (tip.isEmpty() ? QString() : QStringLiteral("<br>")) + QStringLiteral("ここにいる: ") + who.join(QStringLiteral(", "));
            if (!tip.isEmpty()) {
                QToolTip::showText(he->globalPos(), tip, this);
                return true;
            }
        }
        QToolTip::hideText();
        e->ignore();
        return true;
    }
    return QWidget::event(e);
}
