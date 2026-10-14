#include "BoardPainter.hpp"
#include "CardVisual.hpp"
#include "DataMaster.hpp"
#include <vector>

namespace BoardPainter {

namespace {
constexpr double kCorner = 1.6;
constexpr double kUnits = 2 * kCorner + 9;

QRectF contentOf(const QRectF& r, Side side, double band, QRectF* bandOut) {
    QRectF bandR, content;
    switch (side) {
        case Side::Bottom: bandR = QRectF(r.left(), r.top(), r.width(), band); content = r.adjusted(0, band, 0, 0); break;
        case Side::Left: bandR = QRectF(r.right() - band, r.top(), band, r.height()); content = r.adjusted(0, 0, -band, 0); break;
        case Side::Top: bandR = QRectF(r.left(), r.bottom() - band, r.width(), band); content = r.adjusted(0, 0, 0, -band); break;
        case Side::Right: bandR = QRectF(r.left(), r.top(), band, r.height()); content = r.adjusted(band, 0, 0, 0); break;
        case Side::Corner: content = r; break;
    }
    if (bandOut) *bandOut = bandR;
    return content;
}

// そのマスにいる駒の配置（中心と半径）
void pawnLayout(const QRectF& board, int pos, int count, std::vector<QPointF>& centers, double& rad) {
    const double u = unit(board);
    const Side side = sideOf(pos);
    const QRectF content = contentOf(cellRect(board, pos), side, u * 0.42, nullptr);
    rad = qMin(qMin(content.width(), content.height()) * 0.17, u * 0.15);
    const int perRow = qMax(1, int(content.width() / (rad * 2.3)));
    centers.clear();
    for (int i = 0; i < count; ++i) {
        int row = i / perRow, col = i % perRow;
        const double baseY = (side == Side::Corner) ? content.bottom() - rad * 1.3 - ((count - 1) / perRow) * rad * 2.2
                                                    : content.center().y() - rad * 0.6;
        QPointF c(content.left() + rad * 1.2 + col * rad * 2.3, baseY + row * rad * 2.2);
        if (c.y() + rad > content.bottom()) c.setY(content.bottom() - rad);
        centers.push_back(c);
    }
}
}  // namespace

double unit(const QRectF& board) { return board.width() / kUnits; }

Side sideOf(int pos) {
    if (pos % 10 == 0) return Side::Corner;
    if (pos < 10) return Side::Bottom;
    if (pos < 20) return Side::Left;
    if (pos < 30) return Side::Top;
    return Side::Right;
}

QRectF cellRect(const QRectF& b, int pos) {
    double u = unit(b);
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

QRectF innerRect(const QRectF& b) {
    double c = kCorner * unit(b);
    return b.adjusted(c, c, -c, -c);
}

int cellAt(const QRectF& board, const QPointF& pt) {
    if (!board.contains(pt)) return -1;
    for (int i = 0; i < DataMaster::BOARD_SIZE; ++i)
        if (cellRect(board, i).contains(pt)) return i;
    return -1;
}

QPointF pawnCenter(const QRectF& board, const GameState& s, int playerId, double* radius) {
    const int pos = s.players[playerId].position;
    std::vector<int> here;
    for (const auto& pl : s.players)
        if (!pl.bankrupt && pl.position == pos) here.push_back(pl.id);
    std::vector<QPointF> centers;
    double rad = 0;
    pawnLayout(board, pos, int(here.size()), centers, rad);
    if (radius) *radius = rad;
    for (size_t i = 0; i < here.size(); ++i)
        if (here[i] == playerId) return centers[i];
    return cellRect(board, pos).center();
}

void paintDie(QPainter& p, const QRectF& r, int v) {
    p.save();
    p.setPen(QPen(Qt::black, 1.2));
    p.setBrush(Qt::white);
    p.drawRoundedRect(r, r.width() * 0.15, r.width() * 0.15);
    if (v >= 1 && v <= 6) {
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
    p.restore();
}

static void paintCell(QPainter& p, const QRectF& board, int pos, const GameState* state, const Options& opt) {
    const QRectF r = cellRect(board, pos);
    const auto& sq = DataMaster::board()[pos];
    const Side side = sideOf(pos);
    const double u = unit(board);
    const double band = u * 0.42;
    QRectF bandR;
    const QRectF content = contentOf(r, side, band, &bandR);

    p.save();
    p.setPen(QPen(QColor(60, 60, 60), 1));
    p.setBrush(QColor(0xF4, 0xF8, 0xF0));
    p.drawRect(r);
    QFont base = p.font();

    if (sq.type == DataMaster::SquareType::CARD) {
        const CardState* st = state ? &state->card(sq.card) : nullptr;
        if (st && st->owner >= 0) {
            QColor oc = CardVisual::playerColor(st->owner);
            oc.setAlpha(70);
            p.setPen(Qt::NoPen);
            p.setBrush(oc);
            p.drawRect(content);
        }
        QColor gc = CardVisual::groupColor(DataMaster::card(sq.card).group);
        p.setPen(QPen(QColor(60, 60, 60), 1));
        p.setBrush(gc);
        p.drawRect(bandR);
        QFont f = base;
        f.setBold(true);
        f.setPixelSize(qMax(9, int(band * 0.88)));
        p.setFont(f);
        p.setPen(CardVisual::textColorOn(gc));
        p.drawText(bandR, Qt::AlignCenter, CardVisual::cardNumber(sq.card));

        if (st && st->owner >= 0) {
            if (st->houses > 0) {
                const bool hotel = st->houses >= 5;
                const int n = hotel ? 1 : st->houses;
                const double hs = u * 0.17;
                const bool horiz = (side == Side::Bottom || side == Side::Top);
                double w = hotel ? hs * 2.6 : hs, h = hs;
                double total = n * (horiz ? w : h) + (n - 1) * 2;
                if (!horiz) std::swap(w, h);
                QPointF start;
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
            const double om = u * 0.2;
            QPointF oc;
            switch (side) {
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
                mf.setPixelSize(qMax(9, int(u * 0.26)));
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
            f.setPixelSize(qMax(12, int(u * 0.6)));
            p.setFont(f);
            // 上半分に GO、その下に手当（下の方は駒が並ぶので空ける）
            p.drawText(r.adjusted(0, r.height() * 0.06, 0, -r.height() * 0.5), Qt::AlignCenter, QStringLiteral("GO"));
            f.setPixelSize(qMax(9, int(u * 0.26)));
            f.setBold(false);
            p.setFont(f);
            QString bonus = state ? QStringLiteral("+%1万").arg(state->settings.goBonus) : QString();
            p.drawText(r.adjusted(0, r.height() * 0.5, 0, -r.height() * 0.28), Qt::AlignCenter, bonus);
        } else if (sq.type == DataMaster::SquareType::TAX) {
            const bool on = !state || state->settings.taxEnabled;
            f.setPixelSize(qMax(9, int(u * 0.28)));
            p.setFont(f);
            p.setPen(on ? QColor(120, 0, 0) : QColor(170, 170, 170));
            p.drawText(r, Qt::AlignCenter, on ? QStringLiteral("TAX\n%1").arg(sq.tax) : QStringLiteral("(TAX)"));
        }
    }

    QFont nf = base;
    nf.setPixelSize(qMax(8, int(u * 0.19)));
    p.setFont(nf);
    p.setPen(QColor(140, 140, 140));
    p.drawText(r.adjusted(2, 1, -2, -1), Qt::AlignLeft | Qt::AlignBottom, QString::number(pos));

    // 駒
    if (state) {
        std::vector<int> here;
        for (const auto& pl : state->players)
            if (!pl.bankrupt && pl.position == pos) here.push_back(pl.id);
        std::vector<QPointF> centers;
        double rad = 0;
        pawnLayout(board, pos, int(here.size()), centers, rad);
        for (size_t i = 0; i < here.size(); ++i) {
            if (here[i] == opt.hiddenPawn) continue;
            const bool active = (here[i] == state->activePlayer &&
                                 (state->phase == Phase::OPERATIONAL || state->phase == Phase::PURCHASE));
            p.setPen(QPen(active ? QColor(255, 215, 0) : Qt::black, active ? 2.5 : 1));
            p.setBrush(CardVisual::playerColor(here[i]));
            p.drawEllipse(centers[i], rad, rad);
            QFont tf = base;
            tf.setBold(true);
            tf.setPixelSize(qMax(7, int(rad * 1.2)));
            p.setFont(tf);
            p.setPen(Qt::white);
            p.drawText(QRectF(centers[i].x() - rad, centers[i].y() - rad, rad * 2, rad * 2), Qt::AlignCenter, QString::number(here[i] + 1));
        }
    }

    if (sq.type == DataMaster::SquareType::CARD) {
        if (state && state->pendingPurchase == sq.card) {
            p.setPen(QPen(QColor(255, 140, 0), 3, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRect(r.adjusted(2, 2, -2, -2));
        }
        if (sq.card == opt.selected) {
            p.setPen(QPen(QColor(255, 200, 0), 4));
            p.setBrush(Qt::NoBrush);
            p.drawRect(r.adjusted(1, 1, -1, -1));
        }
    }
    if (pos == opt.highlightCell) {
        p.setPen(QPen(QColor(0x42, 0xA5, 0xF5), 4));
        p.setBrush(QColor(0x42, 0xA5, 0xF5, 50));
        p.drawRect(r.adjusted(1, 1, -1, -1));
    }
    p.restore();
}

static void paintCenter(QPainter& p, const QRectF& board, const GameState* state) {
    const double u = unit(board);
    QRectF center = innerRect(board).adjusted(u * 0.3, u * 0.3, -u * 0.3, -u * 0.3);
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
        p.setPen(CardVisual::playerColor(ap.id).darker(130));
        p.drawText(line, Qt::AlignCenter, QStringLiteral("手番: %1").arg(QString::fromStdString(ap.name)));
    }
    if (s.phase == Phase::OPERATIONAL && s.hasRolled) {
        double ds = u * 0.9;
        paintDie(p, QRectF(center.center().x() - ds - u * 0.1, center.center().y() - ds / 2, ds, ds), s.lastDice1);
        paintDie(p, QRectF(center.center().x() + u * 0.1, center.center().y() - ds / 2, ds, ds), s.lastDice2);
    }
    p.restore();
}

void paintBoard(QPainter& p, const QRectF& board, const GameState* s, const Options& opt) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xDD, 0xEE, 0xDD));
    p.drawRect(board);
    for (int i = 0; i < DataMaster::BOARD_SIZE; ++i) paintCell(p, board, i, s, opt);
    if (opt.drawCenter) paintCenter(p, board, s);
    p.setPen(QPen(QColor(40, 40, 40), 2));
    p.setBrush(Qt::NoBrush);
    p.drawRect(board);
    p.restore();
}

}  // namespace BoardPainter
