#include "PlayersWidget.hpp"
#include "CardVisual.hpp"
#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QHelpEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>

namespace {
constexpr int kHeader = 26;
constexpr int kChipW = 22, kChipH = 26, kGap = 3, kPad = 8;
}  // namespace

PlayersWidget::PlayersWidget(QWidget* parent) : QWidget(parent) {
    QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
    setMouseTracking(true);
    setAcceptDrops(true);
}

void PlayersWidget::setEngine(const MonopolyEngine* e) {
    engine = e;
    refresh();
}

void PlayersWidget::setSelectedCard(CardId id) {
    selected = id;
    update();
}

void PlayersWidget::refresh() {
    setMinimumHeight(layoutHeight(width()));
    updateGeometry();
    update();
}

void PlayersWidget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    setMinimumHeight(layoutHeight(width()));
}

QSize PlayersWidget::sizeHint() const { return QSize(380, layoutHeight(380)); }

int PlayersWidget::layoutHeight(int width) const {
    if (!engine) return 40;
    const GameState& s = engine->getState();
    int perLine = qMax(1, (width - 2 * kPad - 20) / (kChipW + kGap));
    int h = 0;
    for (const auto& p : s.players) {
        int n = static_cast<int>(s.ownedBy(p.id).size());
        int lines = p.bankrupt ? 0 : qMax(1, (n + perLine - 1) / perLine);
        h += kHeader + lines * (kChipH + kGap) + 8;
    }
    return h + 4;
}

void PlayersWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    chipRects.clear();
    rowRects.clear();
    moneyRects.clear();
    if (!engine) return;
    const GameState& s = engine->getState();
    const int w = width();
    const int perLine = qMax(1, (w - 2 * kPad - 20) / (kChipW + kGap));
    int y = 2;
    QFont base = font();

    for (const auto& pl : s.players) {
        auto owned = s.ownedBy(pl.id);
        int lines = pl.bankrupt ? 0 : qMax(1, (int(owned.size()) + perLine - 1) / perLine);
        int rowH = kHeader + lines * (kChipH + kGap) + 4;
        QRectF row(2, y, w - 4, rowH);

        const bool active = (pl.id == s.activePlayer && (s.phase == Phase::PURCHASE || s.phase == Phase::OPERATIONAL));
        const bool hover = (pl.id == hoverRow);
        p.setPen(QPen(hover ? QColor(0x42, 0xA5, 0xF5) : active ? QColor(255, 190, 0) : QColor(200, 200, 200),
                      hover ? 3.5 : active ? 2.5 : 1, hover ? Qt::DashLine : Qt::SolidLine));
        QColor bg = pl.bankrupt ? QColor(230, 230, 230) : palette().base().color();
        if (hover) bg = bg.lighter(130);
        p.setBrush(bg);
        p.drawRoundedRect(row, 6, 6);
        if (!pl.bankrupt) rowRects.emplace_back(row, pl.id);

        // 見出し: 色丸 + 名前 + 所持金
        QPointF dot(row.left() + 14, row.top() + kHeader / 2.0 + 1);
        p.setPen(QPen(Qt::black, 1));
        p.setBrush(CardVisual::playerColor(pl.id));
        p.drawEllipse(dot, 8, 8);
        QFont nf = base;
        nf.setBold(true);
        nf.setPixelSize(10);
        p.setFont(nf);
        p.setPen(Qt::white);
        p.drawText(QRectF(dot.x() - 8, dot.y() - 8, 16, 16), Qt::AlignCenter, QString::number(pl.id));

        nf.setPixelSize(13);
        p.setFont(nf);
        p.setPen(pl.bankrupt ? QColor(150, 150, 150) : palette().text().color());
        QRectF nameR(row.left() + 28, row.top() + 2, row.width() * 0.42, kHeader);
        p.drawText(nameR, Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(QString::fromStdString(pl.name), Qt::ElideRight, int(nameR.width())));

        QString info;
        if (pl.bankrupt) {
            info = QStringLiteral("破産");
        } else {
            info = QStringLiteral("%1万").arg(pl.money);
            QFont mf = base;
            mf.setPixelSize(13);
            mf.setBold(true);
            p.setFont(mf);
            p.setPen(pl.money < 0 ? QColor(200, 0, 0) : palette().text().color());
        }
        QRectF moneyR(row.left() + row.width() * 0.45, row.top() + 2, row.width() * 0.22, kHeader);
        p.drawText(moneyR, Qt::AlignVCenter | Qt::AlignRight, info);
        if (!pl.bankrupt) {
            // お金のドラッグ元（コイン）
            const double cs = 18;
            QRectF coin(moneyR.left() - cs - 2, row.top() + (kHeader - cs) / 2 + 2, cs, cs);
            p.drawPixmap(coin.toRect(), DragData::moneyPixmap(int(cs)));
            moneyRects.emplace_back(coin.united(moneyR), pl.id);
        }

        if (!pl.bankrupt) {
            QFont sf = base;
            sf.setPixelSize(11);
            p.setFont(sf);
            p.setPen(QColor(110, 110, 110));
            QString sub = QStringLiteral("資産 %1  位置 %2").arg(MonopolyEngine::liquidValue(s, pl.id)).arg(pl.position);
            if (s.phase == Phase::NEGOTIATION) sub = (pl.ready ? QStringLiteral("✔完了  ") : QStringLiteral("交渉中  ")) + sub;
            const QRectF subR(moneyR.right() + 6, row.top() + 2, row.right() - moneyR.right() - 10, kHeader);
            sub = p.fontMetrics().elidedText(sub, Qt::ElideRight, int(subR.width()));
            p.drawText(QRectF(moneyR.right() + 6, row.top() + 2, row.right() - moneyR.right() - 10, kHeader),
                       Qt::AlignVCenter | Qt::AlignRight, sub);
        }

        // 所有カード
        if (!pl.bankrupt) {
            if (owned.empty()) {
                QFont ef = base;
                ef.setPixelSize(11);
                p.setFont(ef);
                p.setPen(QColor(160, 160, 160));
                p.drawText(QRectF(row.left() + 28, row.top() + kHeader, 200, kChipH), Qt::AlignVCenter, QStringLiteral("カードなし"));
            }
            for (size_t i = 0; i < owned.size(); ++i) {
                int line = int(i) / perLine, col = int(i) % perLine;
                QRectF chip(row.left() + 28 + col * (kChipW + kGap), row.top() + kHeader + line * (kChipH + kGap), kChipW, kChipH);
                CardVisual::paintChip(p, chip, owned[i], &s.card(owned[i]));
                if (owned[i] == selected) {
                    p.setPen(QPen(QColor(255, 190, 0), 2.5));
                    p.setBrush(Qt::NoBrush);
                    p.drawRoundedRect(chip.adjusted(-1.5, -1.5, 1.5, 1.5), 4, 4);
                }
                chipRects.emplace_back(chip, owned[i]);
            }
        }
        y += rowH + 4;
    }
}

CardId PlayersWidget::chipAt(const QPointF& pt) const {
    for (const auto& [r, id] : chipRects)
        if (r.contains(pt)) return id;
    return CardId::NONE;
}

int PlayersWidget::playerRowAt(const QPointF& pt) const {
    for (const auto& [r, id] : rowRects)
        if (r.contains(pt)) return id;
    return -1;
}

int PlayersWidget::moneyAt(const QPointF& pt) const {
    for (const auto& [r, id] : moneyRects)
        if (r.contains(pt)) return id;
    return -1;
}

void PlayersWidget::mousePressEvent(QMouseEvent* e) {
    pressPos = e->position();
    pressCard = chipAt(e->position());
    pressMoneyPlayer = (pressCard == CardId::NONE) ? moneyAt(e->position()) : -1;
    if (pressCard != CardId::NONE) emit cardClicked(pressCard);
}

void PlayersWidget::mouseMoveEvent(QMouseEvent* e) {
    if (!(e->buttons() & Qt::LeftButton) || !engine) return;
    if ((e->position() - pressPos).manhattanLength() < QApplication::startDragDistance()) return;
    QDrag* drag = nullptr;
    if (pressCard != CardId::NONE) {
        drag = new QDrag(this);
        const GameState& s = engine->getState();
        drag->setMimeData(DragData::makeCard(pressCard, s.card(pressCard).owner));
        drag->setPixmap(CardVisual::cardIcon(pressCard, &s.card(pressCard), 32).pixmap(32, 32));
        drag->setHotSpot(QPoint(16, 16));
    } else if (pressMoneyPlayer >= 0) {
        drag = new QDrag(this);
        drag->setMimeData(DragData::makeMoney(pressMoneyPlayer));
        drag->setPixmap(DragData::moneyPixmap(30));
        drag->setHotSpot(QPoint(15, 15));
    }
    pressCard = CardId::NONE;
    pressMoneyPlayer = -1;
    if (drag) drag->exec(Qt::MoveAction);
}

void PlayersWidget::dragEnterEvent(QDragEnterEvent* e) {
    if (DragData::hasAny(e->mimeData())) e->acceptProposedAction();
}

void PlayersWidget::dragMoveEvent(QDragMoveEvent* e) {
    int row = playerRowAt(e->position());
    if (row != hoverRow) {
        hoverRow = row;
        update();
    }
    if (row >= 0 && DragData::hasAny(e->mimeData())) e->acceptProposedAction();
    else e->ignore();
}

void PlayersWidget::dragLeaveEvent(QDragLeaveEvent*) {
    hoverRow = -1;
    update();
}

void PlayersWidget::dropEvent(QDropEvent* e) {
    const int to = playerRowAt(e->position());
    hoverRow = -1;
    update();
    if (to < 0) return;
    CardId id;
    int from = -1;
    if (DragData::readCard(e->mimeData(), id, from)) {
        e->acceptProposedAction();
        emit cardDropped(id, from, to);
    } else if (DragData::readMoney(e->mimeData(), from)) {
        e->acceptProposedAction();
        emit moneyDropped(from, to);
    }
}

bool PlayersWidget::event(QEvent* e) {
    if (e->type() == QEvent::ToolTip && engine) {
        auto* he = static_cast<QHelpEvent*>(e);
        CardId id = chipAt(he->pos());
        if (id != CardId::NONE) {
            QToolTip::showText(he->globalPos(), CardVisual::cardTooltip(id, engine->getState()) +
                               QStringLiteral("<br><i>ドラッグして別のプレイヤーに渡す</i>"), this);
        } else if (moneyAt(he->pos()) >= 0) {
            QToolTip::showText(he->globalPos(), QStringLiteral("コインをドラッグして別のプレイヤーにお金を渡す"), this);
        } else {
            QToolTip::hideText();
        }
        return true;
    }
    return QWidget::event(e);
}
