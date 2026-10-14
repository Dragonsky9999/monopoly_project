#include "TableWidget.hpp"
#include "BoardPainter.hpp"
#include "CardVisual.hpp"
#include "DataMaster.hpp"
#include "Dialogs.hpp"

#include <QApplication>
#include <QContextMenuEvent>
#include <QWheelEvent>
#include <QFontMetricsF>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QTextDocument>
#include <QTimer>
#include <algorithm>
#include <array>
#include <cmath>

namespace {
// ---- テーブル上のサイズ（シーン座標。画面には縮尺して表示）
constexpr double kBoard = 1100;   // 盤面
constexpr double kAreaW = 620;    // プレイヤーエリア
constexpr double kAreaH = 300;
constexpr double kGap = 30;       // 盤面とエリアの間
constexpr double kSlotGap = 20;   // エリア同士の間
constexpr double kChipW = 44, kChipH = 58, kChipStep = 50;
constexpr double kBillW = 72, kBillH = 40, kBillStep = 82;

const int kDenoms[] = {500, 100, 50, 20, 10, 5, 1};

// 金額を紙幣に分ける。できるだけ全種類が揃うように、小額紙幣を先に確保してから残りを大きい順に
// 例: 1500 → 500×2, 100×4, 50×1, 20×1, 10×2, 5×1, 1×5
std::array<int, 7> billBreakdown(int amount) {
    std::array<int, 7> cnt{};  // kDenoms と同じ並び
    int rest = std::max(0, amount);
    if (rest >= 200) {
        // 1×5, 5×1, 10×2, 20×1, 50×1, 100×1 (= 200)
        const int reserve[7] = {0, 1, 1, 1, 2, 1, 5};
        for (int i = 6; i >= 1; --i) {
            cnt[i] += reserve[i];
            rest -= reserve[i] * kDenoms[i];
        }
    }
    for (int i = 0; i < 7; ++i) {
        cnt[i] += rest / kDenoms[i];
        rest %= kDenoms[i];
    }
    return cnt;
}

QColor billColor(int v) {
    switch (v) {
        case 500: return QColor(0xF5, 0xA6, 0x23);
        case 100: return QColor(0xE8, 0xDC, 0xC2);
        case 50: return QColor(0x7E, 0xB6, 0xE8);
        case 20: return QColor(0x8B, 0xC9, 0x8B);
        case 10: return QColor(0xF7, 0xE2, 0x7A);
        case 5: return QColor(0xF2, 0xA2, 0xC0);
        default: return QColor(0xFA, 0xFA, 0xFA);
    }
}

void paintBill(QPainter& p, const QRectF& r, int value) {
    p.save();
    p.setPen(QPen(QColor(60, 60, 60), 1));
    p.setBrush(billColor(value));
    p.drawRoundedRect(r, 3, 3);
    p.setPen(QPen(QColor(0, 0, 0, 60), 1));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRectF(r.left() + 4, r.top() + 4, r.height() - 8, r.height() - 8));
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(int(r.height() * 0.5));
    p.setFont(f);
    p.setPen(QColor(30, 30, 30));
    p.drawText(r.adjusted(r.height() * 0.6, 0, -2, 0), Qt::AlignCenter, QString::number(value));
    p.restore();
}

QString pname(const GameState& s, int id) { return CardVisual::playerName(s, id); }

// 文字・部品を全体的に大きくするための倍率
constexpr double kText = 1.2;        // 盤面中央・取引欄・ホバー等の文字
constexpr double kAreaScale = 1.25;  // プレイヤーエリア全体（文字・紙幣・カード）
int px(double v) { return int(v * kText + 0.5); }
}  // namespace

TableWidget::TableWidget(const MonopolyEngine& e, QWidget* parent) : QWidget(parent), engine(e) {
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
    toastTimer = new QTimer(this);
    toastTimer->setSingleShot(true);
    connect(toastTimer, &QTimer::timeout, this, [this] {
        toastText.clear();
        update();
    });
}

void TableWidget::refresh() {
    computeLayout();
    update();
}

void TableWidget::clearTrade() {
    draft = Draft();
    update();
}

void TableWidget::showToast(const QString& msg, bool error) {
    toastText = msg;
    toastError = error;
    toastTimer->start(error ? 6000 : 4000);
    update();
}

void TableWidget::setTextScale(double s) {
    userScale = std::clamp(s, 0.6, 2.5);
    computeLayout();
    update();
}

double TableWidget::areaScale() const { return kAreaScale * userScale; }
int TableWidget::ux(double v) const { return int(v * kText * userScale + 0.5); }

void TableWidget::setSelectedCard(CardId id) {
    selected = id;
    update();
}

// ============================================================================ 座標
void TableWidget::computeLayout() {
    const GameState& s = engine.getState();
    const int n = static_cast<int>(s.players.size());
    areaR.assign(n, QRectF());

    // プレイヤーエリアは盤面の右側に1列で並べる。収まらない分はスクロール
    const double aw = kAreaW * areaScale(), ah = kAreaH * areaScale();
    const int shown = std::min(std::max(n, 1), 4);
    const double boardSize = std::max(kBoard, shown * ah + (shown - 1) * kSlotGap);
    boardR = QRectF(0, 0, boardSize, boardSize);
    columnR = QRectF(boardSize + kGap, 0, aw, boardSize);
    const double contentH = n * ah + std::max(0, n - 1) * kSlotGap;
    columnMaxScroll = std::max(0.0, contentH - boardSize);
    columnScroll = std::clamp(columnScroll, 0.0, columnMaxScroll);
    const double top = (contentH < boardSize) ? (boardSize - contentH) / 2 : -columnScroll;
    for (int i = 0; i < n; ++i) areaR[i] = QRectF(columnR.left(), top + i * (ah + kSlotGap), aw, ah);
    QRectF all = boardR.united(columnR.adjusted(0, 0, 22, 0));  // 22 = スクロールバー
    sceneR = all.adjusted(-20, -20, 20, 20);
}

QTransform TableWidget::viewTransform() const {
    if (sceneR.isEmpty()) return QTransform();
    const double sx = width() / sceneR.width(), sy = height() / sceneR.height();
    const double s = std::min(sx, sy);
    const double dx = (width() - sceneR.width() * s) / 2 - sceneR.left() * s;
    const double dy = (height() - sceneR.height() * s) / 2 - sceneR.top() * s;
    return QTransform(s, 0, 0, s, dx, dy);
}

QPointF TableWidget::toScene(const QPointF& w) const { return viewTransform().inverted().map(w); }

int TableWidget::areaAt(const QPointF& sp) const {
    if (!columnR.contains(sp)) return -1;
    for (size_t i = 0; i < areaR.size(); ++i)
        if (areaR[i].contains(sp)) return static_cast<int>(i);
    return -1;
}

const TableWidget::Hit* TableWidget::hitAt(const QPointF& sp, const std::function<bool(const Hit&)>& filter) const {
    for (auto it = hits.rbegin(); it != hits.rend(); ++it)
        if (it->r.contains(sp) && (!filter || filter(*it))) return &*it;
    return nullptr;
}

bool TableWidget::draggable(const Hit& h) {
    switch (h.kind) {
        case HitKind::Cell: return h.card != CardId::NONE;
        case HitKind::HandCard:
        case HitKind::Bill:
        case HitKind::MoneyTotal: return true;
        default: return false;
    }
}

// ============================================================================ 描画
void TableWidget::addButton(QPainter& p, const QRectF& r, const QString& text, const QString& id, bool enabled, const QColor& color) {
    const bool hover = hoverValid && enabled && !dragging &&
                       p.transform().mapRect(r).contains(viewTransform().map(hoverScene));
    p.save();
    p.setPen(QPen(enabled ? color.darker(140) : QColor(150, 150, 150), 1.5));
    QColor bg = enabled ? (hover ? color.lighter(125) : color) : QColor(200, 200, 200);
    p.setBrush(bg);
    p.drawRoundedRect(r, 8, 8);
    QFont f = p.font();
    f.setBold(true);
    int size = int(std::min(r.height() * 0.5, 30.0));
    f.setPixelSize(size);
    while (size > 10 && QFontMetricsF(f).horizontalAdvance(text) > r.width() - 14) {  // ボタン幅に収まるまで縮める
        f.setPixelSize(--size);
    }
    p.setFont(f);
    p.setPen(enabled ? Qt::white : QColor(120, 120, 120));
    p.drawText(r, Qt::AlignCenter, text);
    p.restore();
    Hit h;
    h.r = r;
    h.kind = HitKind::Button;
    h.id = id;
    h.enabled = enabled;
    hits.push_back(h);
}

void TableWidget::paintEvent(QPaintEvent*) {
    if (sceneR.isEmpty() || areaR.size() != engine.getState().players.size()) computeLayout();
    hits.clear();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.fillRect(rect(), QColor(0x1F, 0x3B, 0x2C));  // テーブル（フェルト）
    p.setTransform(viewTransform());

    const GameState& s = engine.getState();

    // ---- 盤面
    BoardPainter::Options opt;
    opt.selected = selected;
    opt.drawCenter = false;
    if (dragging && hasPressHit && pressHit.kind == HitKind::Pawn) {
        opt.hiddenPawn = pressHit.player;
        opt.highlightCell = BoardPainter::cellAt(boardR, dragScene);
    }
    BoardPainter::paintBoard(p, boardR, &s, opt);
    for (int pos = 0; pos < DataMaster::BOARD_SIZE; ++pos) {
        Hit h;
        h.r = BoardPainter::cellRect(boardR, pos);
        h.kind = HitKind::Cell;
        h.card = DataMaster::board()[pos].card;
        h.value = pos;
        h.player = (h.card != CardId::NONE) ? s.card(h.card).owner : -1;
        hits.push_back(h);
    }
    for (const auto& pl : s.players) {
        if (pl.bankrupt) continue;
        double rad = 10;
        QPointF c = BoardPainter::pawnCenter(boardR, s, pl.id, &rad);
        Hit h;
        h.r = QRectF(c.x() - rad, c.y() - rad, rad * 2, rad * 2);
        h.kind = HitKind::Pawn;
        h.player = pl.id;
        hits.push_back(h);
    }

    // ---- プレイヤーエリア（盤面の右、はみ出す分はスクロール）
    p.save();
    p.setClipRect(columnR.adjusted(-8, -8, 8, 8));
    for (size_t i = 0; i < s.players.size(); ++i)
        if (areaR[i].intersects(columnR)) paintPlayerArea(p, static_cast<int>(i));
    p.restore();
    if (columnMaxScroll > 0) {
        const QRectF track(columnR.right() + 8, columnR.top(), 10, columnR.height());
        const double ratio = columnR.height() / (columnR.height() + columnMaxScroll);
        const double thumbH = std::max(40.0, track.height() * ratio);
        const QRectF thumb(track.left(), track.top() + (track.height() - thumbH) * (columnScroll / columnMaxScroll), track.width(), thumbH);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 40));
        p.drawRoundedRect(track, 5, 5);
        p.setBrush(QColor(255, 255, 255, 150));
        p.drawRoundedRect(thumb, 5, 5);
    }

    // ---- 盤面中央の操作・取引・ポップアップ
    // 基準サイズ kBoard の盤面として描き、実際の盤面サイズに合わせて拡大する
    {
        const double c = boardR.width() / kBoard;
        centerBoardR = QRectF(0, 0, kBoard, kBoard);
        const size_t firstHit = hits.size();
        p.save();
        p.translate(boardR.topLeft());
        p.scale(c, c);
        paintCenterUI(p);
        paintPendingPopup(p);
        paintToast(p);
        p.restore();
        QTransform toScene;
        toScene.translate(boardR.left(), boardR.top());
        toScene.scale(c, c);
        for (size_t i = firstHit; i < hits.size(); ++i) hits[i].r = toScene.mapRect(hits[i].r);
        tradeZoneR = toScene.mapRect(tradeZoneR);
    }
    if (!dragging) {
        paintCardPanel(p);
        if (!panel.visible) paintHoverInfo(p);
    }
    paintDragGhost(p);
}

void TableWidget::paintPlayerArea(QPainter& p, int pid) {
    // エリアは基準サイズ (kAreaW x kAreaH) で描き、kAreaScale 倍して配置する
    const size_t firstHit = hits.size();
    p.save();
    p.translate(areaR[pid].topLeft());
    p.scale(areaScale(), areaScale());
    paintPlayerAreaLocal(p, pid);
    p.restore();
    QTransform toScene;
    toScene.translate(areaR[pid].left(), areaR[pid].top());
    toScene.scale(areaScale(), areaScale());
    for (size_t i = firstHit; i < hits.size(); ++i) hits[i].r = toScene.mapRect(hits[i].r).intersected(columnR);
}

void TableWidget::paintPlayerAreaLocal(QPainter& p, int pid) {
    const GameState& s = engine.getState();
    const Player& pl = s.players[pid];
    const QRectF a(0, 0, kAreaW, kAreaH);
    const bool active = (pid == s.activePlayer && (s.phase == Phase::PURCHASE || s.phase == Phase::OPERATIONAL));
    const bool dropTarget = dragging && areaAt(dragScene) == pid;
    QFont base = p.font();
    p.save();

    Hit area;
    area.r = a;
    area.kind = HitKind::PlayerArea;
    area.player = pid;
    hits.push_back(area);

    // 枠
    QColor pc = CardVisual::playerColor(pid);
    QColor bg = pl.bankrupt ? QColor(70, 70, 70) : QColor(0xF7, 0xF5, 0xEE);
    if (dropTarget) bg = QColor(0xE3, 0xF2, 0xFD);
    p.setPen(QPen(dropTarget ? QColor(0x1E, 0x88, 0xE5) : active ? QColor(255, 196, 0) : pc,
                  dropTarget ? 6 : active ? 6 : 3, dropTarget ? Qt::DashLine : Qt::SolidLine));
    p.setBrush(bg);
    p.drawRoundedRect(a, 14, 14);
    // 左の色帯
    p.setPen(Qt::NoPen);
    p.setBrush(pc);
    p.drawRoundedRect(QRectF(a.left() + 4, a.top() + 4, 10, a.height() - 8), 4, 4);

    // 見出し
    QPointF dot(a.left() + 42, a.top() + 34);
    p.setPen(QPen(Qt::black, 1.5));
    p.setBrush(pc);
    p.drawEllipse(dot, 21, 21);
    QFont f = base;
    f.setBold(true);
    f.setPixelSize(24);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(QRectF(dot.x() - 21, dot.y() - 21, 42, 42), Qt::AlignCenter, QString::number(pid + 1));

    f.setPixelSize(31);
    p.setFont(f);
    p.setPen(pl.bankrupt ? QColor(200, 200, 200) : QColor(30, 30, 30));
    QRectF nameR(a.left() + 72, a.top() + 8, 290, 52);
    p.drawText(nameR, Qt::AlignVCenter | Qt::AlignLeft,
               p.fontMetrics().elidedText(QString::fromStdString(pl.name), Qt::ElideRight, int(nameR.width())));

    if (pl.bankrupt) {
        f.setPixelSize(40);
        p.setFont(f);
        p.setPen(QColor(220, 80, 80));
        p.drawText(a, Qt::AlignCenter, QStringLiteral("破産"));
        p.restore();
        return;
    }

    // 所持金（合計）: ドラッグで金額指定して渡す
    f.setPixelSize(42);
    p.setFont(f);
    p.setPen(pl.money < 0 ? QColor(200, 0, 0) : QColor(20, 20, 20));
    QRectF moneyR(a.right() - 250, a.top() + 4, 234, 58);
    p.drawText(moneyR, Qt::AlignVCenter | Qt::AlignRight, QStringLiteral("%1万").arg(pl.money));
    const int offered = draftOffered(pid);
    {
        Hit h;
        h.r = moneyR;
        h.kind = HitKind::MoneyTotal;
        h.player = pid;
        hits.push_back(h);
    }
    QFont sf = base;
    sf.setPixelSize(19);
    p.setFont(sf);
    p.setPen(QColor(100, 100, 100));
    QString sub = QStringLiteral("換金可能 %1万   位置 %2").arg(MonopolyEngine::liquidValue(s, pid)).arg(pl.position);
    if (active) sub = QStringLiteral("▶ 手番   ") + sub;
    p.drawText(QRectF(a.left() + 72, a.top() + 62, 380, 28), Qt::AlignVCenter | Qt::AlignLeft, sub);
    if (offered > 0) {  // 取引欄に出している分
        sf.setBold(true);
        p.setFont(sf);
        p.setPen(QColor(0x1E, 0x88, 0xE5));
        p.drawText(QRectF(a.right() - 250, a.top() + 62, 234, 28), Qt::AlignVCenter | Qt::AlignRight,
                   QStringLiteral("取引中 -%1万").arg(offered));
    }

    // 紙幣（取引欄に出した分を除いた残り）
    int rest = std::max(0, pl.money - offered);
    double x = a.left() + 26;
    const double by = a.top() + 100;
    // 種類が少ないときは紙幣を大きく
    const auto bills = billBreakdown(rest);
    int kinds = 0;
    for (int c : bills)
        if (c > 0) ++kinds;
    const double billStep = std::min(104.0, (kAreaW - 44) / std::max(kinds, 1));
    const double billW = billStep - 12, billH = std::min(billW * 0.55, 50.0);
    for (int i = 0; i < 7; ++i) {
        const int d = kDenoms[i];
        const int count = bills[i];
        if (count <= 0) continue;
        const int shown = std::min(count, 4);
        for (int k = shown - 1; k >= 0; --k) paintBill(p, QRectF(x + k * 2.5, by - k * 2.5, billW, billH), d);
        QFont cf = base;
        cf.setPixelSize(18);
        cf.setBold(true);
        p.setFont(cf);
        p.setPen(QColor(60, 60, 60));
        p.drawText(QRectF(x, by + billH + 1, billW, 22), Qt::AlignCenter, QStringLiteral("×%1").arg(count));
        Hit h;
        h.r = QRectF(x, by - 8, billW + 8, billH + 30);
        h.kind = HitKind::Bill;
        h.player = pid;
        h.value = d;
        h.count = count;
        hits.push_back(h);
        x += billStep;
    }
    if (pl.money < 0) {
        QFont df = base;
        df.setPixelSize(22);
        df.setBold(true);
        p.setFont(df);
        p.setPen(QColor(200, 0, 0));
        p.drawText(QRectF(a.left() + 26, by, 560, 44), Qt::AlignVCenter, QStringLiteral("借金中（家を売る / 抵当 / 破産）"));
    }

    // 手札
    auto owned = s.ownedBy(pid);
    const double hy = a.top() + 176;
    const double rowW = kAreaW - 44;
    // 1列に収まるなら大きいカード、多いときは2段
    double chipW = 58, chipH = 78, chipStep = 66;
    if (owned.size() * chipStep > rowW) { chipW = kChipW; chipH = kChipH; chipStep = kChipStep; }
    const int perRow = std::max(1, int(rowW / chipStep));
    for (size_t i = 0; i < owned.size(); ++i) {
        const int row = int(i) / perRow, col = int(i) % perRow;
        QRectF chip(a.left() + 26 + col * chipStep, hy + row * (chipH + 6), chipW, chipH);
        CardVisual::paintChip(p, chip, owned[i], &s.card(owned[i]));
        if (owned[i] == selected) {
            p.setPen(QPen(QColor(255, 190, 0), 3));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(chip.adjusted(-2, -2, 2, 2), 4, 4);
        }
        Hit h;
        h.r = chip;
        h.kind = HitKind::HandCard;
        h.player = pid;
        h.card = owned[i];
        hits.push_back(h);
    }
    if (owned.empty()) {
        QFont ef = base;
        ef.setPixelSize(19);
        p.setFont(ef);
        p.setPen(QColor(160, 160, 160));
        p.drawText(QRectF(a.left() + 26, hy, 300, chipH), Qt::AlignVCenter, QStringLiteral("カードなし"));
    }

    p.restore();
}

void TableWidget::paintCenterUI(QPainter& p) {
    const GameState& s = engine.getState();
    const QRectF inner = BoardPainter::innerRect(centerBoardR).adjusted(20, 16, -20, -16);
    QFont base = p.font();
    p.save();

    QFont f = base;
    f.setBold(true);
    f.setPixelSize(px(40));
    p.setFont(f);
    p.setPen(QColor(0x2E, 0x5E, 0x2E));
    p.drawText(QRectF(inner.left(), inner.top(), inner.width(), 60), Qt::AlignCenter, QStringLiteral("BUSINESS MONOPOLY"));

    QString phase;
    switch (s.phase) {
        case Phase::PURCHASE: phase = QStringLiteral("① 購入フェーズ"); break;
        case Phase::NEGOTIATION: phase = QStringLiteral("② 交渉フェーズ"); break;
        case Phase::OPERATIONAL: phase = QStringLiteral("③ 運用フェーズ   ターン %1").arg(s.turn); break;
        case Phase::FINISHED: phase = QStringLiteral("ゲーム終了"); break;
    }
    f.setPixelSize(px(26));
    p.setFont(f);
    p.setPen(QColor(30, 30, 30));
    p.drawText(QRectF(inner.left(), inner.top() + 66, inner.width(), 42), Qt::AlignCenter, phase);

    auto hint = [&](double y, const QString& text, const QColor& c = QColor(90, 90, 90)) {
        QFont hf = base;
        hf.setPixelSize(px(17));
        p.setFont(hf);
        p.setPen(c);
        p.drawText(QRectF(inner.left(), y, inner.width(), 60), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, text);
    };
    const double cx = inner.center().x();
    double y = inner.top() + 118;

    if (s.phase == Phase::PURCHASE || s.phase == Phase::OPERATIONAL) {
        const Player& ap = s.players[s.activePlayer];
        f.setPixelSize(px(26));
        p.setFont(f);
        p.setPen(CardVisual::playerColor(ap.id).darker(130));
        p.drawText(QRectF(inner.left(), y, inner.width(), 42), Qt::AlignCenter, QStringLiteral("手番: %1").arg(QString::fromStdString(ap.name)));
        y += 50;
    }

    if (s.phase == Phase::PURCHASE) {
        hint(y, QStringLiteral("買うマスにカーソルを合わせて「購入」（または %1 のエリアへドラッグ）\n同じ色は1人1枚まで").arg(pname(s, s.activePlayer)));
        y += 70;
        addButton(p, QRectF(cx - 140, y, 280, 58), QStringLiteral("パス"), QStringLiteral("pass"), true, QColor(0x75, 0x75, 0x75));
        y += 76;
        hint(y, QStringLiteral("連続パス %1 / %2（全員が続けてパスで交渉フェーズへ）").arg(s.consecutivePasses).arg(s.aliveCount()));
    } else if (s.phase == Phase::NEGOTIATION) {
        hint(y, QStringLiteral("カードや紙幣を相手のエリアへドラッグ → 下の取引欄で「成立」"));
        y += 40;
        addButton(p, QRectF(cx - 210, y, 420, 60), QStringLiteral("交渉終了 → 運用フェーズへ"), QStringLiteral("allready"), true, QColor(0x2E, 0x7D, 0x32));
        y += 74;
    } else if (s.phase == Phase::OPERATIONAL) {
        if (s.pendingUtility != CardId::NONE) {
            // 他人の公共施設に止まった → もう一度振って支払い額を決める
            const int owner = s.card(s.pendingUtility).owner;
            hint(y, QStringLiteral("%1（%2 の公共施設）に止まりました。もう一度振って 出目×%3万 を支払い")
                        .arg(CardVisual::cardShortLabel(s.pendingUtility), pname(s, owner))
                        .arg(MonopolyEngine::rentOf(s, s.pendingUtility, 1)),  // 出目1あたりの額 = 倍率
                 QColor(0xE6, 0x51, 0x00));
            y += 40;
            const double ds = 90;
            QRectF d1(cx - ds - 14, y, ds, ds), d2(cx + 14, y, ds, ds);
            BoardPainter::paintDie(p, d1, die1);
            BoardPainter::paintDie(p, d2, die2);
            for (int k = 1; k <= 2; ++k) {
                Hit h;
                h.r = k == 1 ? d1 : d2;
                h.kind = HitKind::Die;
                h.value = k;
                hits.push_back(h);
            }
            y += ds + 14;
            const int rent = MonopolyEngine::rentOf(s, s.pendingUtility, die1 + die2);
            addButton(p, QRectF(cx - 290, y, 210, 60), QStringLiteral("🎲 ランダム"), QStringLiteral("urandom"), true, QColor(0x6A, 0x4C, 0x93));
            addButton(p, QRectF(cx - 60, y, 350, 60), QStringLiteral("この出目で支払い（%1万）").arg(rent), QStringLiteral("uroll"), true, QColor(0xE6, 0x51, 0x00));
            y += 74;
        } else if (!s.hasRolled) {
            if (s.doublesStreak > 0) {
                hint(y, QStringLiteral("ぞろ目（%1+%2）！ もう一度振ってください（%3 回連続%4）")
                            .arg(s.lastDice1).arg(s.lastDice2).arg(s.doublesStreak)
                            .arg(s.settings.maxDoubles > 0 ? QStringLiteral(" / %1 回まで").arg(s.settings.maxDoubles) : QString()),
                     QColor(0xE6, 0x51, 0x00));
                y += 40;
            }
            const double ds = 90;
            QRectF d1(cx - ds - 14, y, ds, ds), d2(cx + 14, y, ds, ds);
            BoardPainter::paintDie(p, d1, die1);
            BoardPainter::paintDie(p, d2, die2);
            for (int k = 1; k <= 2; ++k) {
                Hit h;
                h.r = k == 1 ? d1 : d2;
                h.kind = HitKind::Die;
                h.value = k;
                hits.push_back(h);
            }
            y += ds + 10;
            hint(y, QStringLiteral("サイコロをクリックで出目を合わせる（右クリックで戻す）"));
            y += 36;
            addButton(p, QRectF(cx - 290, y, 210, 60), QStringLiteral("🎲 ランダム"), QStringLiteral("random"), true, QColor(0x6A, 0x4C, 0x93));
            addButton(p, QRectF(cx - 60, y, 350, 60), QStringLiteral("この出目で移動（%1）").arg(die1 + die2), QStringLiteral("roll"), true);
            y += 74;
        } else {
            const double ds = 72;
            const QRectF d1(cx - ds - 10, y, ds, ds), d2(cx + 10, y, ds, ds);
            const auto& acts = engine.getActions();
            const bool afterUtility = !acts.empty() && acts.back().kind == ActionKind::UTILITY_ROLL;
            BoardPainter::paintDie(p, d1, afterUtility ? acts.back().d1 : s.lastDice1);
            BoardPainter::paintDie(p, d2, afterUtility ? acts.back().d2 : s.lastDice2);
            if (afterUtility) {
                QFont uf = base;
                uf.setPixelSize(px(14));
                p.setFont(uf);
                p.setPen(QColor(0xE6, 0x51, 0x00));
                p.drawText(QRectF(cx - ds - 230, y, 210, ds), Qt::AlignVCenter | Qt::AlignRight, QStringLiteral("公共施設の\n支払い用の出目"));
            }
            y += ds + 12;
            if (s.settings.maxDoubles > 0 && s.doublesStreak >= s.settings.maxDoubles) {
                hint(y, QStringLiteral("ぞろ目 %1 回目なので振り直しはここまで").arg(s.doublesStreak), QColor(0xE6, 0x51, 0x00));
                y += 34;
            }
            const Player& ap = s.players[s.activePlayer];
            if (s.pendingPurchase == CardId::NONE && ap.money < 0 && s.settings.debtBlocksEndTurn) {
                const bool hopeless = MonopolyEngine::liquidValue(s, ap.id) < 0;
                hint(y, hopeless
                            ? QStringLiteral("所持金 %1万。家を売って全部抵当に入れても足りません → 破産").arg(ap.money)
                            : QStringLiteral("所持金 %1万。このターン中に 家を売る / 抵当 / 取引 で 0 以上にしてください").arg(ap.money),
                     QColor(0xC6, 0x28, 0x28));
                y += 40;
                addButton(p, QRectF(cx - 170, y, 340, 60), QStringLiteral("破産する（ゲームオーバー）"), QStringLiteral("bankrupt"), true,
                          hopeless ? QColor(0xC6, 0x28, 0x28) : QColor(0x75, 0x75, 0x75));
                y += 74;
            } else if (s.pendingPurchase == CardId::NONE) {
                addButton(p, QRectF(cx - 170, y, 340, 60), QStringLiteral("ターン終了 ▶"), QStringLiteral("end"), true, QColor(0xC6, 0x28, 0x28));
                y += 74;
                hint(y, QStringLiteral("自由行動: マスやカードにカーソルを合わせて建設・抵当 / ドラッグで取引"));
            } else {
                hint(y, QStringLiteral("止まったマスの近くで 購入 / 見送り / オークション を選んでください"), QColor(0xE6, 0x51, 0x00));
            }
        }
    } else if (s.phase == Phase::FINISHED) {
        // 順位: 生き残りを換金可能額の多い順、脱落者は最後
        std::vector<int> order;
        for (const auto& pl : s.players) order.push_back(pl.id);
        std::stable_sort(order.begin(), order.end(), [&](int x, int y2) {
            const bool bx = s.players[x].bankrupt, by = s.players[y2].bankrupt;
            if (bx != by) return !bx;
            return MonopolyEngine::liquidValue(s, x) > MonopolyEngine::liquidValue(s, y2);
        });
        f.setPixelSize(px(30));
        p.setFont(f);
        p.setPen(QColor(0xB7, 0x1C, 0x1C));
        p.drawText(QRectF(inner.left(), y, inner.width(), 44), Qt::AlignCenter,
                   QStringLiteral("🏆 勝者: %1").arg(pname(s, order.front())));
        y += 50;
        QFont rf = base;
        rf.setPixelSize(px(18));
        p.setFont(rf);
        for (size_t i = 0; i < order.size(); ++i) {
            const Player& pl = s.players[order[i]];
            p.setPen(pl.bankrupt ? QColor(150, 150, 150) : QColor(30, 30, 30));
            const QString line = pl.bankrupt ? QStringLiteral("%1. %2  脱落").arg(i + 1).arg(pname(s, pl.id))
                                             : QStringLiteral("%1. %2  換金可能 %3万（現金 %4万）").arg(i + 1).arg(pname(s, pl.id))
                                                   .arg(MonopolyEngine::liquidValue(s, pl.id)).arg(pl.money);
            p.drawText(QRectF(inner.left(), y, inner.width(), 28), Qt::AlignCenter, line);
            y += 28;
        }
        y += 12;
        if (s.resumePhase != Phase::FINISHED && s.aliveCount() >= 2)
            addButton(p, QRectF(cx - 170, y, 340, 56), QStringLiteral("ゲームを続ける ▶"), QStringLiteral("continue"), true, QColor(0x35, 0x5C, 0x9A));
    }

    // 取引欄（下半分）
    const double zoneH = draft.empty() ? 90 : 340;
    tradeZoneR = QRectF(inner.left() + 10, inner.bottom() - zoneH - 10, inner.width() - 20, zoneH);
    if (s.phase == Phase::NEGOTIATION || s.phase == Phase::OPERATIONAL) paintTradeZone(p, tradeZoneR);
    p.restore();
}

void TableWidget::paintTradeZone(QPainter& p, const QRectF& z) {
    const GameState& s = engine.getState();
    QFont base = p.font();
    const bool dropHere = dragging && p.transform().mapRect(z).contains(viewTransform().map(dragScene));
    p.save();
    p.setPen(QPen(dropHere ? QColor(0x1E, 0x88, 0xE5) : QColor(120, 140, 120), 2, Qt::DashLine));
    p.setBrush(draft.empty() ? QColor(255, 255, 255, 60) : QColor(255, 255, 255, 200));
    p.drawRoundedRect(z, 12, 12);
    QFont f = base;
    f.setPixelSize(px(18));
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(60, 80, 60));
    if (draft.empty()) {
        QFont hf = base;
        hf.setPixelSize(px(16));
        p.setFont(hf);
        p.drawText(z, Qt::AlignCenter, QStringLiteral("取引欄: カード・紙幣を相手のエリアへドラッグすると、ここに取引が組み立てられます"));
        p.restore();
        return;
    }
    p.drawText(QRectF(z.left() + 14, z.top() + 8, z.width() - 28, 28), Qt::AlignVCenter | Qt::AlignLeft,
               QStringLiteral("取引: %1 ⇄ %2   （項目クリックで取り除く）").arg(pname(s, draft.a), pname(s, draft.b)));

    auto side = [&](int sideIdx, const QRectF& r) {
        const int from = sideIdx == 0 ? draft.a : draft.b, to = sideIdx == 0 ? draft.b : draft.a;
        const auto& cards = sideIdx == 0 ? draft.aCards : draft.bCards;
        const int money = sideIdx == 0 ? draft.aMoney : draft.bMoney;
        QFont lf = base;
        lf.setPixelSize(px(16));
        lf.setBold(true);
        p.setFont(lf);
        p.setPen(CardVisual::playerColor(from).darker(120));
        p.drawText(QRectF(r.left(), r.top(), r.width(), 24), Qt::AlignVCenter | Qt::AlignLeft,
                   QStringLiteral("%1 → %2").arg(pname(s, from), pname(s, to)));
        double x = r.left();
        const double cy = r.top() + 30;
        for (CardId c : cards) {
            QRectF chip(x, cy, kChipW, kChipH);
            CardVisual::paintChip(p, chip, c, &s.card(c));
            Hit h;
            h.r = chip;
            h.kind = HitKind::TradeItem;
            h.card = c;
            h.value = sideIdx;
            hits.push_back(h);
            x += kChipStep;
            if (x + kChipW > r.right()) {
                x = r.left();
                break;  // 1行に収まらない分は表示省略
            }
        }
        if (money > 0) {
            QRectF br(r.left(), cy + kChipH + 8, 150, 38);
            paintBill(p, QRectF(br.left(), br.top() + 2, kBillW, kBillH), money >= 500 ? 500 : money >= 100 ? 100 : 50);
            QFont mf = base;
            mf.setPixelSize(px(20));
            mf.setBold(true);
            p.setFont(mf);
            p.setPen(QColor(30, 30, 30));
            p.drawText(QRectF(br.left() + kBillW + 8, br.top(), 120, br.height()), Qt::AlignVCenter, QStringLiteral("%1万").arg(money));
            Hit h;
            h.r = br.adjusted(0, 0, 60, 0);
            h.kind = HitKind::TradeItem;
            h.value = sideIdx;
            hits.push_back(h);
        }
    };
    const double half = (z.width() - 40) / 2;
    side(0, QRectF(z.left() + 14, z.top() + 42, half, 140));
    side(1, QRectF(z.left() + 26 + half, z.top() + 42, half, 140));

    // 評価とボタン
    Action act = draftAction(ActionKind::TRADE);
    ApplyResult v = engine.validate(act);
    QFont ef = base;
    ef.setPixelSize(px(16));
    p.setFont(ef);
    QString evalText;
    if (!v.ok) {
        p.setPen(QColor(200, 40, 40));
        evalText = QStringLiteral("成立できません: ") + QString::fromStdString(v.error);
    } else {
        const double score = engine.evaluateTradeFairness(act.trade);
        p.setPen(QColor(40, 40, 40));
        if (std::abs(score) <= 30) evalText = QStringLiteral("簡易評価: 👍 妥当");
        else if (std::abs(score) <= 100) evalText = QStringLiteral("簡易評価: ⚠ やや %1 有利").arg(pname(s, score > 0 ? draft.a : draft.b));
        else evalText = QStringLiteral("簡易評価: 🔴 %1 が大きく有利").arg(pname(s, score > 0 ? draft.a : draft.b));
        evalText += QStringLiteral("（スコア %1）").arg(score, 0, 'f', 1);
    }
    p.drawText(QRectF(z.left() + 14, z.bottom() - 108, z.width() - 28, 40), Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap, evalText);
    const double bw = (z.width() - 60) / 3;
    addButton(p, QRectF(z.left() + 15, z.bottom() - 62, bw, 48), QStringLiteral("成立"), QStringLiteral("trade_ok"), v.ok, QColor(0x2E, 0x7D, 0x32));
    addButton(p, QRectF(z.left() + 30 + bw, z.bottom() - 62, bw, 48), QStringLiteral("破談として記録"), QStringLiteral("trade_cancel"), true, QColor(0x8D, 0x6E, 0x63));
    addButton(p, QRectF(z.left() + 45 + 2 * bw, z.bottom() - 62, bw, 48), QStringLiteral("クリア"), QStringLiteral("trade_clear"), true, QColor(0x75, 0x75, 0x75));
    p.restore();
}

void TableWidget::paintPendingPopup(QPainter& p) {
    const GameState& s = engine.getState();
    if (s.phase != Phase::OPERATIONAL || s.pendingPurchase == CardId::NONE) return;
    const Card& card = DataMaster::card(s.pendingPurchase);
    const QRectF cell = BoardPainter::cellRect(centerBoardR, card.position);
    const QRectF inner = BoardPainter::innerRect(centerBoardR).adjusted(8, 8, -8, -8);
    const QSizeF size(470, 175);
    QPointF tl;
    switch (BoardPainter::sideOf(card.position)) {
        case BoardPainter::Side::Bottom: tl = QPointF(cell.center().x() - size.width() / 2, cell.top() - size.height() - 12); break;
        case BoardPainter::Side::Top: tl = QPointF(cell.center().x() - size.width() / 2, cell.bottom() + 12); break;
        case BoardPainter::Side::Left: tl = QPointF(cell.right() + 12, cell.center().y() - size.height() / 2); break;
        default: tl = QPointF(cell.left() - size.width() - 12, cell.center().y() - size.height() / 2); break;
    }
    tl.setX(std::clamp(tl.x(), inner.left(), inner.right() - size.width()));
    tl.setY(std::clamp(tl.y(), inner.top(), inner.bottom() - size.height()));
    const QRectF r(tl, size);

    p.save();
    // 吹き出しの線
    p.setPen(QPen(QColor(0xE6, 0x51, 0x00), 3));
    p.drawLine(r.center(), cell.center());
    p.setBrush(QColor(0xFF, 0xF8, 0xE1));
    p.drawRoundedRect(r, 12, 12);
    CardVisual::paintChip(p, QRectF(r.left() + 14, r.top() + 12, kChipW, kChipH), card.id);
    QFont f = p.font();
    f.setPixelSize(px(20));
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(40, 40, 40));
    const Player& ap = s.players[s.activePlayer];
    p.drawText(QRectF(r.left() + 58, r.top() + 10, r.width() - 70, 48), Qt::AlignVCenter | Qt::AlignLeft,
               QStringLiteral("%1  価格 %2万\n%3 の所持金 %4万").arg(CardVisual::cardShortLabel(card.id)).arg(card.price)
                   .arg(QString::fromStdString(ap.name)).arg(ap.money));
    const double bw = (r.width() - 40) / 3;
    addButton(p, QRectF(r.left() + 10, r.bottom() - 62, bw, 50), QStringLiteral("購入"), QStringLiteral("pend_buy"), ap.money >= card.price, QColor(0x2E, 0x7D, 0x32));
    addButton(p, QRectF(r.left() + 20 + bw, r.bottom() - 62, bw, 50), QStringLiteral("見送り"), QStringLiteral("pend_decline"), true, QColor(0x75, 0x75, 0x75));
    addButton(p, QRectF(r.left() + 30 + 2 * bw, r.bottom() - 62, bw, 50), QStringLiteral("オークション"), QStringLiteral("pend_auction"), true, QColor(0xEF, 0x6C, 0x00));
    p.restore();
}

void TableWidget::paintToast(QPainter& p) {
    if (toastText.isEmpty()) return;
    p.save();
    QFont f = p.font();
    f.setPixelSize(ux(20));
    f.setBold(true);
    p.setFont(f);
    const QRectF inner = BoardPainter::innerRect(centerBoardR);
    QRectF r(inner.left() + 20, inner.top() - 6 + 8, inner.width() - 40, 0);
    QRectF br = p.fontMetrics().boundingRect(r.toRect(), Qt::AlignCenter | Qt::TextWordWrap, toastText);
    r.setHeight(br.height() + 20);
    p.setPen(Qt::NoPen);
    p.setBrush(toastError ? QColor(198, 40, 40, 235) : QColor(30, 30, 30, 215));
    p.drawRoundedRect(r, 10, 10);
    p.setPen(Qt::white);
    p.drawText(r, Qt::AlignCenter | Qt::TextWordWrap, toastText);
    p.restore();
}

void TableWidget::paintHoverInfo(QPainter& p) {
    if (!hoverValid) return;
    const GameState& s = engine.getState();
    const Hit* h = hitAt(hoverScene, [](const Hit& x) {
        return x.kind == HitKind::Cell || x.kind == HitKind::HandCard || x.kind == HitKind::Pawn || x.kind == HitKind::Bill ||
               x.kind == HitKind::Button || x.kind == HitKind::TradeItem || x.kind == HitKind::MoneyTotal || x.kind == HitKind::Die;
    });
    if (!h) return;
    QString html;
    if ((h->kind == HitKind::Cell || h->kind == HitKind::HandCard) && h->card != CardId::NONE) {
        return;  // カードはパネル（paintCardPanel）で表示
    } else if (h->kind == HitKind::Cell) {
        const auto& sq = DataMaster::board()[h->value];
        if (sq.type == DataMaster::SquareType::GO) html = QStringLiteral("<b>GO</b><br>通過で +%1万").arg(s.settings.goBonus);
        else if (sq.type == DataMaster::SquareType::TAX)
            html = s.settings.taxEnabled ? QStringLiteral("<b>税金</b> %1万").arg(sq.tax) : QStringLiteral("税金マス（オフ）");
        else return;
    } else if (h->kind == HitKind::Pawn) {
        const Player& pl = s.players[h->player];
        html = QStringLiteral("<b>%1</b><br>所持金 %2万")
                   .arg(QString::fromStdString(pl.name).toHtmlEscaped()).arg(pl.money);
    } else if (h->kind == HitKind::Bill) {
        html = QStringLiteral("<b>%1万札 ×%2</b><br><span style='color:gray'>相手のエリアへドラッグ: 1枚<br>Shift+ドラッグ: 束ごと</span>").arg(h->value).arg(h->count);
    } else if (h->kind == HitKind::MoneyTotal) {
        html = QStringLiteral("<span style='color:gray'>相手のエリアへドラッグ: 金額を指定して渡す</span>");
    } else {
        return;  // ボタン等は情報なし
    }

    QTextDocument doc;
    QFont df = p.font();
    df.setPixelSize(ux(24));
    doc.setDefaultFont(df);
    // 文字色を明示（ダーク配色だと既定が白になり、白い枠の中が空白に見える）
    doc.setHtml(QStringLiteral("<div style='color:#202020'>") + html + QStringLiteral("</div>"));
    doc.setTextWidth(ux(24) * 22);
    const QSizeF sz(doc.idealWidth() + 20, doc.size().height() + 16);
    QPointF tl = hoverScene + QPointF(24, 24);
    if (tl.x() + sz.width() > sceneR.right()) tl.setX(hoverScene.x() - sz.width() - 16);
    if (tl.y() + sz.height() > sceneR.bottom()) tl.setY(hoverScene.y() - sz.height() - 16);
    p.save();
    p.setPen(QPen(QColor(80, 80, 80), 1.5));
    p.setBrush(QColor(255, 255, 255, 245));
    p.drawRoundedRect(QRectF(tl, sz), 8, 8);
    p.translate(tl + QPointF(10, 8));
    doc.drawContents(&p);
    p.restore();
}

// ---- カードパネル（ホバーで物件の近くに出る情報 + 操作ボタン）
void TableWidget::updateCardPanel(const QPointF& sp) {
    // 対象の上かパネルの上にいる間だけ出す（パネルは対象にくっつけて置くので、そのまま移れる）
    if (panel.visible && (panel.anchor.contains(sp) || panel.rect.adjusted(-2, -2, 2, 2).contains(sp))) return;
    const Hit* h = hitAt(sp, [](const Hit& x) {
        return x.kind == HitKind::Cell || x.kind == HitKind::HandCard || x.kind == HitKind::Button || x.kind == HitKind::Pawn ||
               x.kind == HitKind::Bill || x.kind == HitKind::MoneyTotal || x.kind == HitKind::TradeItem || x.kind == HitKind::Die;
    });
    const GameState& s = engine.getState();
    if (h && (h->kind == HitKind::Cell || h->kind == HitKind::HandCard) && h->card != CardId::NONE &&
        !(h->kind == HitKind::Cell && s.phase == Phase::OPERATIONAL && s.pendingPurchase == h->card)) {
        if (!panel.visible || panel.card != h->card || panel.anchor != h->r) {
            panel.visible = true;
            panel.card = h->card;
            panel.fromHand = (h->kind == HitKind::HandCard);
            panel.anchor = h->r;
            panel.rect = QRectF();
        }
    } else {
        panel.visible = false;
    }
}

void TableWidget::paintCardPanel(QPainter& p) {
    if (!panel.visible || panel.card == CardId::NONE || !hoverValid) return;
    const GameState& s = engine.getState();
    const CardId c = panel.card;
    const Card& card = DataMaster::card(c);
    const CardState& st = s.card(c);

    // ---- 内容
    QStringList lines;
    lines << QStringLiteral("価格 %1万 / 抵当 %2万").arg(card.price).arg(card.mortgage);
    if (card.kind == CardKind::PROPERTY) {
        lines << QStringLiteral("家 %1万").arg(card.housePrice);
        lines << QStringLiteral("地代 %1 / 家1 %2 / 家2 %3").arg(card.rent[0]).arg(card.rent[1]).arg(card.rent[2]);
        lines << QStringLiteral("家3 %1 / 家4 %2 / ホテル %3").arg(card.rent[3]).arg(card.rent[4]).arg(card.rent[5]);
    } else if (card.kind == CardKind::RAILROAD) {
        lines << QStringLiteral("地代 1枚 25 / 2枚 50 / 3枚 100 / 4枚 200");
    } else {
        lines << QStringLiteral("地代 1枚 出目×4 / 2枚 出目×10");
    }
    QString ownerLine = QStringLiteral("所有: %1").arg(pname(s, st.owner));
    if (st.houses == 5) ownerLine += QStringLiteral("  ホテル");
    else if (st.houses > 0) ownerLine += QStringLiteral("  家%1軒").arg(st.houses);
    if (st.mortgaged) ownerLine += QStringLiteral("  （抵当中）");
    lines << ownerLine;
    if (st.owner >= 0) {  // いま止まったら払う額
        QString now;
        if (st.mortgaged) now = QStringLiteral("0万（抵当中）");
        else if (card.kind == CardKind::UTILITY)
            now = QStringLiteral("出目×%1万").arg(MonopolyEngine::rentOf(s, c, 1));
        else
            now = QStringLiteral("%1万").arg(MonopolyEngine::rentOf(s, c, 7));
        lines << QStringLiteral("いまの地代: ") + now;
    }

    struct Btn { QString text, id; bool enabled; QColor color; };
    std::vector<Btn> btns;
    auto valid = [&](ActionKind k, int player) {
        Action a;
        a.kind = k;
        a.card = c;
        a.player = player;
        return player >= 0 && engine.validate(a).ok;
    };
    const QString ci = QString::number(cardIndex(c));
    if (st.owner < 0 && s.phase == Phase::PURCHASE) {
        btns.push_back({QStringLiteral("%1 が購入 −%2万").arg(pname(s, s.activePlayer)).arg(card.price), QStringLiteral("pc:buy:") + ci,
                        valid(ActionKind::BUY, s.activePlayer), QColor(0x2E, 0x7D, 0x32)});
    }
    // 建設・抵当などは手番の人の物件だけ（交渉フェーズは全員が自分の物件を操作できる）
    const bool mine = st.owner >= 0 && ((st.owner == s.activePlayer && s.phase == Phase::OPERATIONAL) || s.phase == Phase::NEGOTIATION);
    if (mine) {
        if (card.kind == CardKind::PROPERTY) {
            btns.push_back({QStringLiteral("家を建てる −%1万").arg(card.housePrice), QStringLiteral("pc:build:") + ci, valid(ActionKind::BUILD, st.owner), QColor(0x2E, 0x7D, 0x32)});
            btns.push_back({QStringLiteral("家を売る +%1万").arg(card.housePrice / 2), QStringLiteral("pc:sell:") + ci, valid(ActionKind::SELL, st.owner), QColor(0x8D, 0x6E, 0x63)});
        }
        if (st.mortgaged)
            btns.push_back({QStringLiteral("抵当解除 −%1万").arg(MonopolyEngine::unmortgageCost(s, c)), QStringLiteral("pc:unmortgage:") + ci, valid(ActionKind::UNMORTGAGE, st.owner), QColor(0x35, 0x5C, 0x9A)});
        else
            btns.push_back({QStringLiteral("抵当に入れる +%1万").arg(card.mortgage), QStringLiteral("pc:mortgage:") + ci, valid(ActionKind::MORTGAGE, st.owner), QColor(0x35, 0x5C, 0x9A)});
    }
    // 使えないボタンは出さない（何もなければ情報だけ）
    btns.erase(std::remove_if(btns.begin(), btns.end(), [](const Btn& b) { return !b.enabled; }), btns.end());

    // ---- サイズ
    QFont base = p.font();
    QFont tf = base;
    tf.setBold(true);
    tf.setPixelSize(ux(30));
    QFont lf = base;
    lf.setPixelSize(ux(22));
    const double pad = ux(14);
    const double lineH = QFontMetricsF(lf).height() * 1.15;
    const double titleH = std::max(QFontMetricsF(tf).height(), double(ux(46)));
    double w = QFontMetricsF(tf).horizontalAdvance(CardVisual::cardShortLabel(c)) + ux(60);
    for (const QString& l : lines) w = std::max(w, QFontMetricsF(lf).horizontalAdvance(l));
    w += pad * 2;
    const double btnH = ux(48);
    const double h = pad + titleH + 8 + lines.size() * lineH + (btns.empty() ? 0 : 10 + btnH) + pad;
    w = std::max(w, btns.size() * double(ux(200)) + pad * 2);

    // ---- 位置（盤面のマスは盤面の内側へ、手札は一覧の左へ）
    const QRectF a = panel.anchor;
    QPointF tl;
    if (panel.fromHand) {
        tl = QPointF(a.left() - w + 2, a.center().y() - h / 2);  // 手札の左にくっつける
    } else {
        switch (BoardPainter::sideOf(card.position)) {
            case BoardPainter::Side::Bottom: tl = QPointF(a.center().x() - w / 2, a.top() - h + 2); break;
            case BoardPainter::Side::Top: tl = QPointF(a.center().x() - w / 2, a.bottom() - 2); break;
            case BoardPainter::Side::Left: tl = QPointF(a.right() - 2, a.center().y() - h / 2); break;
            default: tl = QPointF(a.left() - w + 2, a.center().y() - h / 2); break;
        }
    }
    tl.setX(std::clamp(tl.x(), sceneR.left() + 4, sceneR.right() - w - 4));
    tl.setY(std::clamp(tl.y(), sceneR.top() + 4, sceneR.bottom() - h - 4));
    const QRectF r(tl, QSizeF(w, h));
    panel.rect = r;

    // ---- 描画
    p.save();
    p.setPen(QPen(QColor(60, 60, 60), 2));
    p.setBrush(QColor(255, 255, 255, 248));
    p.drawRoundedRect(r, 12, 12);
    const QRectF chip(r.left() + pad, r.top() + pad, titleH * 0.78, titleH);
    CardVisual::paintChip(p, chip, c, &st);
    p.setFont(tf);
    p.setPen(QColor(25, 25, 25));
    p.drawText(QRectF(chip.right() + 12, r.top() + pad, w, titleH), Qt::AlignVCenter | Qt::AlignLeft, CardVisual::cardShortLabel(c));
    p.setFont(lf);
    double y = r.top() + pad + titleH + 8;
    for (const QString& l : lines) {
        p.setPen(l.startsWith(QStringLiteral("所有")) ? QColor(25, 25, 25) : QColor(70, 70, 70));
        p.drawText(QRectF(r.left() + pad, y, w - pad * 2, lineH), Qt::AlignVCenter | Qt::AlignLeft, l);
        y += lineH;
    }
    p.restore();
    if (!btns.empty()) {
        y += 10;
        const double bw = (w - pad * 2 - (btns.size() - 1) * 8) / btns.size();
        for (size_t i = 0; i < btns.size(); ++i)
            addButton(p, QRectF(r.left() + pad + i * (bw + 8), y, bw, btnH), btns[i].text, btns[i].id, btns[i].enabled, btns[i].color);
    }
}

void TableWidget::paintDragGhost(QPainter& p) {
    if (!dragging || !hasPressHit) return;
    const GameState& s = engine.getState();
    const Hit& h = pressHit;
    p.save();
    p.setOpacity(0.85);
    const QPointF c = dragScene;
    switch (h.kind) {
        case HitKind::Cell:
        case HitKind::HandCard:
            CardVisual::paintChip(p, QRectF(c.x() - 24, c.y() - 30, 48, 60), h.card, &s.card(h.card));
            break;
        case HitKind::Bill: {
            const bool stack = QApplication::keyboardModifiers() & Qt::ShiftModifier;
            const int shown = stack ? std::min(h.count, 4) : 1;
            for (int k = shown - 1; k >= 0; --k) paintBill(p, QRectF(c.x() - kBillW / 2 + k * 3, c.y() - kBillH / 2 - k * 3, kBillW, kBillH), h.value);
            QFont f = p.font();
            f.setPixelSize(px(16));
            f.setBold(true);
            p.setFont(f);
            p.setPen(Qt::white);
            p.drawText(QRectF(c.x() - 80, c.y() + kBillH / 2 + 2, 160, 22), Qt::AlignCenter,
                       QStringLiteral("%1万").arg(stack ? h.value * h.count : h.value));
            break;
        }
        case HitKind::MoneyTotal:
            p.drawPixmap(QRectF(c.x() - 20, c.y() - 20, 40, 40).toRect(), DragData::moneyPixmap(40));
            break;
        case HitKind::Pawn: {
            p.setPen(QPen(Qt::black, 1.5));
            p.setBrush(CardVisual::playerColor(h.player));
            p.drawEllipse(c, 18, 18);
            const int cell = BoardPainter::cellAt(boardR, c);
            if (cell >= 0 && h.player >= 0) {
                const int steps = (cell - s.players[h.player].position + 40) % 40;
                QFont f = p.font();
                f.setPixelSize(px(18));
                f.setBold(true);
                p.setFont(f);
                p.setPen(steps >= 2 && steps <= 12 ? QColor(0x1E, 0x88, 0xE5) : QColor(200, 0, 0));
                p.drawText(QRectF(c.x() - 60, c.y() + 20, 120, 24), Qt::AlignCenter, QStringLiteral("%1 マス").arg(steps));
            }
            break;
        }
        default: break;
    }
    p.restore();
}

// ============================================================================ マウス
void TableWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    const QPointF sp = toScene(e->position());
    pressed = true;
    pressWidget = e->position();
    const Hit* h = hitAt(sp);
    hasPressHit = (h != nullptr);
    if (h) pressHit = *h;
    dragging = false;
}

void TableWidget::mouseMoveEvent(QMouseEvent* e) {
    const QPointF sp = toScene(e->position());
    hoverScene = sp;
    hoverValid = true;
    if (pressed && hasPressHit && draggable(pressHit) && (e->buttons() & Qt::LeftButton)) {
        if (!dragging && (e->position() - pressWidget).manhattanLength() >= QApplication::startDragDistance()) dragging = true;
        if (dragging) {
            dragScene = sp;
            // 一覧の上端・下端付近ではスクロール（隠れているプレイヤーへ落とせるように）
            if (sp.x() >= columnR.left() && sp.x() <= columnR.right() + 30) {
                if (sp.y() < columnR.top() + 60) scrollColumn(-25);
                else if (sp.y() > columnR.bottom() - 60) scrollColumn(25);
            }
        }
    }
    if (!dragging) updateCardPanel(sp);
    update();
}

void TableWidget::wheelEvent(QWheelEvent* e) {
    const QPointF sp = toScene(e->position());
    if (columnMaxScroll > 0 && sp.x() >= columnR.left() && sp.x() <= columnR.right() + 30) {
        scrollColumn(-e->angleDelta().y() * 0.6);
        panel.visible = false;
        e->accept();
        return;
    }
    QWidget::wheelEvent(e);
}

void TableWidget::scrollColumn(double dy) {
    const double before = columnScroll;
    columnScroll = std::clamp(columnScroll + dy, 0.0, columnMaxScroll);
    if (columnScroll != before) {
        computeLayout();
        update();
    }
}

void TableWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton || !pressed) return;
    const QPointF sp = toScene(e->position());
    const bool wasDragging = dragging;
    const bool had = hasPressHit;
    const Hit src = pressHit;
    pressed = false;
    dragging = false;
    hasPressHit = false;
    update();
    if (wasDragging) {
        handleDrop(src, sp, e->modifiers());
        return;
    }
    if (!had) return;
    const Hit* now = hitAt(sp);
    if (now && now->kind == src.kind && now->r == src.r) onClick(src);
}

void TableWidget::leaveEvent(QEvent*) {
    hoverValid = false;
    panel.visible = false;
    update();
}

void TableWidget::onClick(const Hit& h) {
    switch (h.kind) {
        case HitKind::Button:
            if (h.enabled) onButton(h.id);
            break;
        case HitKind::Die:
            (h.value == 1 ? die1 : die2) = (h.value == 1 ? die1 : die2) % 6 + 1;
            update();
            break;
        case HitKind::Cell:
        case HitKind::HandCard:
            if (h.card != CardId::NONE) {
                selected = h.card;
                emit cardSelected(h.card);
                update();
            }
            break;
        case HitKind::TradeItem: {
            if (h.card != CardId::NONE) {
                auto& v = h.value == 0 ? draft.aCards : draft.bCards;
                v.erase(std::remove(v.begin(), v.end(), h.card), v.end());
            } else {
                (h.value == 0 ? draft.aMoney : draft.bMoney) = 0;
            }
            if (draft.aCards.empty() && draft.bCards.empty() && draft.aMoney == 0 && draft.bMoney == 0) draft = Draft();
            update();
            break;
        }
        default: break;
    }
}

void TableWidget::contextMenuEvent(QContextMenuEvent* e) {
    const QPointF sp = toScene(e->pos());
    const GameState& s = engine.getState();
    const Hit* h = hitAt(sp, [](const Hit& x) {
        return x.kind == HitKind::Cell || x.kind == HitKind::HandCard || x.kind == HitKind::PlayerArea || x.kind == HitKind::Die ||
               x.kind == HitKind::Bill || x.kind == HitKind::MoneyTotal;
    });
    if (!h) return;
    if (h->kind == HitKind::Die) {  // 右クリックで出目を戻す
        int& d = h->value == 1 ? die1 : die2;
        d = (d + 4) % 6 + 1;
        update();
        return;
    }
    QMenu menu(this);
    if ((h->kind == HitKind::Cell || h->kind == HitKind::HandCard) && h->card != CardId::NONE) {
        const CardId c = h->card;
        const int owner = s.card(c).owner;
        menu.addSection(CardVisual::cardShortLabel(c) + (owner >= 0 ? QStringLiteral("（%1）").arg(pname(s, owner)) : QStringLiteral("（未所有）")));
        auto add = [&](const QString& text, ActionKind k) {
            Action a;
            a.kind = k;
            a.card = c;
            a.player = owner;
            QAction* act = menu.addAction(text);
            act->setEnabled(owner >= 0 && engine.validate(a).ok);
            connect(act, &QAction::triggered, this, [this, a] { emitAction(a); });
        };
        add(QStringLiteral("家を建てる"), ActionKind::BUILD);
        add(QStringLiteral("家を売る（半額）"), ActionKind::SELL);
        add(QStringLiteral("抵当に入れる"), ActionKind::MORTGAGE);
        add(QStringLiteral("抵当を解除"), ActionKind::UNMORTGAGE);
        menu.addSeparator();
        QAction* own = menu.addAction(QStringLiteral("所有者を修正（管理者）..."));
        connect(own, &QAction::triggered, this, [this, c] {
            Action a;
            if (SmallDialogs::askOverride(this, engine.getState(), c, a)) emitAction(a);
        });
    } else if (h->player >= 0) {
        const int pid = h->player;
        const Player& pl = s.players[pid];
        menu.addSection(QString::fromStdString(pl.name));
        QAction* bank = menu.addAction(QStringLiteral("破産..."));
        bank->setEnabled(!pl.bankrupt && s.phase != Phase::FINISHED);
        connect(bank, &QAction::triggered, this, [this, pid] {
            const GameState& st = engine.getState();
            int player = pid, creditor = (pid == st.activePlayer) ? st.debtCreditor : -1;
            if (!SmallDialogs::askBankrupt(this, st, player, creditor)) return;
            Action a;
            a.kind = ActionKind::BANKRUPT;
            a.player = player;
            a.target = creditor;
            emitAction(a);
        });
        menu.addSeparator();
        QAction* money = menu.addAction(QStringLiteral("所持金を修正（管理者）..."));
        connect(money, &QAction::triggered, this, [this, pid] {
            bool ok = false;
            int v = QInputDialog::getInt(this, QStringLiteral("所持金を修正"), QStringLiteral("新しい所持金（万）"),
                                         engine.getState().players[pid].money, -1000000, 1000000, 10, &ok);
            if (!ok) return;
            Action a;
            a.kind = ActionKind::SET_MONEY;
            a.player = pid;
            a.amount = v;
            emitAction(a);
        });
        QAction* pos = menu.addAction(QStringLiteral("位置を修正（管理者）..."));
        connect(pos, &QAction::triggered, this, [this, pid] {
            bool ok = false;
            int v = QInputDialog::getInt(this, QStringLiteral("位置を修正"), QStringLiteral("マス番号（0〜39）"),
                                         engine.getState().players[pid].position, 0, 39, 1, &ok);
            if (!ok) return;
            Action a;
            a.kind = ActionKind::SET_POS;
            a.player = pid;
            a.amount = v;
            emitAction(a);
        });
    } else {
        return;
    }
    menu.exec(e->globalPos());
}

// ============================================================================ 操作
void TableWidget::emitAction(const Action& a) { emit actionRequested(a); }

bool TableWidget::emitAndCheck(const Action& a) {
    const size_t total = engine.getActions().size();
    emit actionRequested(a);  // 直接接続なのでここで記録まで終わる
    return engine.getActions().size() > total;
}

void TableWidget::fixDie(int which, int delta) {
    if (!canFixRoll()) return;
    const Action& last = engine.getActions().back();
    int d1 = last.d1, d2 = last.d2;
    int& d = which == 0 ? d1 : d2;
    d = (d - 1 + delta + 6) % 6 + 1;
    emit rollCorrectionRequested(d1, d2);
}

bool TableWidget::canFixRoll() const {
    const GameState& s = engine.getState();
    const auto& acts = engine.getActions();
    if (s.phase != Phase::OPERATIONAL || acts.empty() || engine.canRedo()) return false;
    const Action& last = acts.back();
    return (last.kind == ActionKind::ROLL || last.kind == ActionKind::UTILITY_ROLL) && last.player == s.activePlayer;
}

int TableWidget::draftOffered(int pid) const {
    if (draft.empty()) return 0;
    if (pid == draft.a) return draft.aMoney;
    if (pid == draft.b) return draft.bMoney;
    return 0;
}

void TableWidget::onButton(const QString& id) {
    const GameState& s = engine.getState();
    Action a;
    a.player = s.activePlayer;
    if (id == QLatin1String("pass")) {
        a.kind = ActionKind::PASS;
    } else if (id == QLatin1String("buySelected")) {
        a.kind = ActionKind::BUY;
        a.card = selected;
    } else if (id == QLatin1String("allready")) {
        a.kind = ActionKind::ALL_READY;
    } else if (id == QLatin1String("continue")) {
        a.kind = ActionKind::CONTINUE;
    } else if (id.startsWith(QLatin1String("ready:"))) {
        a.kind = ActionKind::READY;
        a.player = id.mid(6).toInt();
    } else if (id == QLatin1String("roll") || id == QLatin1String("random") || id == QLatin1String("uroll") ||
               id == QLatin1String("urandom")) {
        a.kind = id.startsWith(QLatin1Char('u')) ? ActionKind::UTILITY_ROLL : ActionKind::ROLL;
        if (id.endsWith(QLatin1String("random"))) {
            // ランダムは出目を作って表示するだけ（クリックで合わせたのと同じ）。移動は「この出目で移動」
            die1 = QRandomGenerator::global()->bounded(1, 7);
            die2 = QRandomGenerator::global()->bounded(1, 7);
            update();
            return;
        }
        a.d1 = die1;
        a.d2 = die2;
        if (emitAndCheck(a)) die1 = die2 = 1;
        return;
    } else if (id == QLatin1String("bankrupt")) {
        const Player& ap = s.players[s.activePlayer];
        const QString cred = s.debtCreditor >= 0 ? pname(s, s.debtCreditor) : QStringLiteral("銀行");
        auto ans = QMessageBox::question(this, QStringLiteral("破産"),
            QStringLiteral("%1 を破産（ゲームオーバー）にします。\n資産は %2 に渡ります。よろしいですか？").arg(pname(s, ap.id), cred));
        if (ans != QMessageBox::Yes) return;
        a.kind = ActionKind::BANKRUPT;
        a.player = ap.id;
        a.target = s.debtCreditor;
    } else if (id == QLatin1String("end")) {
        a.kind = ActionKind::END_TURN;
    } else if (id == QLatin1String("pend_buy")) {
        a.kind = ActionKind::BUY;
    } else if (id == QLatin1String("pend_decline")) {
        a.kind = ActionKind::DECLINE;
    } else if (id == QLatin1String("pend_auction")) {
        int winner = -1, price = 0;
        if (!SmallDialogs::askAuction(this, s, winner, price)) return;
        a.kind = ActionKind::AUCTION;
        a.target = winner;
        a.amount = price;
    } else if (id == QLatin1String("trade_ok") || id == QLatin1String("trade_cancel")) {
        if (emitAndCheck(draftAction(id == QLatin1String("trade_ok") ? ActionKind::TRADE : ActionKind::TRADE_CANCEL))) clearTrade();
        return;
    } else if (id.startsWith(QLatin1String("pc:"))) {  // カードパネルの操作 pc:<kind>:<card>
        const QStringList parts = id.split(QLatin1Char(':'));
        if (parts.size() != 3) return;
        const CardId c = cardFromIndex(parts[2].toInt());
        const QString k = parts[1];
        a.card = c;
        if (k == QLatin1String("buy")) {
            a.kind = ActionKind::BUY;
            a.player = s.activePlayer;
        } else {
            a.player = s.card(c).owner;
            a.kind = k == QLatin1String("build") ? ActionKind::BUILD
                   : k == QLatin1String("sell") ? ActionKind::SELL
                   : k == QLatin1String("mortgage") ? ActionKind::MORTGAGE : ActionKind::UNMORTGAGE;
        }
    } else if (id == QLatin1String("trade_clear")) {
        clearTrade();
        return;
    } else {
        return;
    }
    emitAction(a);
}

void TableWidget::handleDrop(const Hit& src, const QPointF& sp, Qt::KeyboardModifiers mods) {
    const GameState& s = engine.getState();
    const int targetPlayer = areaAt(sp);
    const bool inTrade = (s.phase == Phase::NEGOTIATION || s.phase == Phase::OPERATIONAL) && tradeZoneR.contains(sp) && !draft.empty();
    switch (src.kind) {
        case HitKind::Pawn: {
            const int cell = BoardPainter::cellAt(boardR, sp);
            if (cell >= 0) movePawn(src.player, cell);
            break;
        }
        case HitKind::Cell:
        case HitKind::HandCard: {
            const CardId c = src.card;
            const int owner = s.card(c).owner;
            if (targetPlayer >= 0) cardToPlayer(c, owner, targetPlayer);
            else if (inTrade && owner >= 0 && (owner == draft.a || owner == draft.b)) draftAddCard(c, owner, owner == draft.a ? draft.b : draft.a);
            break;
        }
        case HitKind::Bill: {
            const int amount = (mods & Qt::ShiftModifier) ? src.value * src.count : src.value;
            if (targetPlayer >= 0 && targetPlayer != src.player) moneyToPlayer(src.player, targetPlayer, amount);
            else if (inTrade && (src.player == draft.a || src.player == draft.b))
                moneyToPlayer(src.player, src.player == draft.a ? draft.b : draft.a, amount);
            break;
        }
        case HitKind::MoneyTotal: {
            int to = -1;
            if (targetPlayer >= 0 && targetPlayer != src.player) to = targetPlayer;
            else if (inTrade && (src.player == draft.a || src.player == draft.b)) to = src.player == draft.a ? draft.b : draft.a;
            if (to < 0) break;
            const int max = s.players[src.player].money - draftOffered(src.player);
            if (max <= 0) {
                showToast(QStringLiteral("%1 にはもう出せるお金がありません").arg(pname(s, src.player)), true);
                break;
            }
            bool ok = false;
            int amount = QInputDialog::getInt(this, QStringLiteral("お金を渡す"),
                                              QStringLiteral("%1 → %2 に渡す金額（万）").arg(pname(s, src.player), pname(s, to)),
                                              std::min(50, max), 1, max, 10, &ok);
            if (ok) moneyToPlayer(src.player, to, amount);
            break;
        }
        default: break;
    }
    update();
}

void TableWidget::cardToPlayer(CardId c, int from, int to) {
    const GameState& s = engine.getState();
    if (from == to) return;
    selected = c;
    if (from < 0) {  // 銀行のカード
        if (s.phase == Phase::PURCHASE) {
            Action a;
            a.kind = ActionKind::BUY;
            a.player = to;
            a.card = c;
            emitAction(a);
        } else if (s.phase == Phase::OPERATIONAL && s.pendingPurchase == c) {
            Action a;
            a.player = s.activePlayer;
            if (to == s.activePlayer) {
                a.kind = ActionKind::BUY;
            } else {
                bool ok = false;
                int price = QInputDialog::getInt(this, QStringLiteral("オークション"),
                    QStringLiteral("%1 が %2 を落札した金額（万）").arg(pname(s, to), CardVisual::cardShortLabel(c)),
                    DataMaster::card(c).price, 0, std::max(0, s.players[to].money), 10, &ok);
                if (!ok) return;
                a.kind = ActionKind::AUCTION;
                a.target = to;
                a.amount = price;
            }
            emitAction(a);
        } else {
            showToast(QStringLiteral("未所有のカードは、購入フェーズか、止まったマスでのみ渡せます"), true);
        }
        return;
    }
    if (s.phase != Phase::NEGOTIATION && s.phase != Phase::OPERATIONAL) {
        showToast(QStringLiteral("プレイヤー間の取引は交渉フェーズ以降でできます"), true);
        return;
    }
    draftAddCard(c, from, to);
}

void TableWidget::moneyToPlayer(int from, int to, int amount) {
    const GameState& s = engine.getState();
    if (s.phase != Phase::NEGOTIATION && s.phase != Phase::OPERATIONAL) {
        showToast(QStringLiteral("お金のやり取りは交渉フェーズ以降でできます"), true);
        return;
    }
    draftAddMoney(from, to, amount);
}

void TableWidget::movePawn(int pid, int cell) {
    const GameState& s = engine.getState();
    if (s.phase != Phase::OPERATIONAL) { showToast(QStringLiteral("駒の移動は運用フェーズでできます"), true); return; }
    if (pid != s.activePlayer) { showToast(QStringLiteral("いまの手番は %1 です").arg(pname(s, s.activePlayer)), true); return; }
    if (s.hasRolled) { showToast(QStringLiteral("このターンはもう移動しました（ターン終了で次の人へ）"), true); return; }
    const int steps = (cell - s.players[pid].position + 40) % 40;
    if (steps < 2 || steps > 12) { showToast(QStringLiteral("出目の合計は 2〜12 です（%1 マス先は不可）").arg(steps), true); return; }
    Action a;
    a.kind = ActionKind::ROLL;
    a.player = pid;
    a.d1 = std::max(1, steps - 6);
    a.d2 = steps - a.d1;
    emitAction(a);
}

// ============================================================================ 取引の下書き
bool TableWidget::draftEnsurePair(int from, int to) {
    if (draft.empty()) {
        draft.a = from;
        draft.b = to;
        return true;
    }
    if ((from == draft.a && to == draft.b) || (from == draft.b && to == draft.a)) return true;
    const GameState& s = engine.getState();
    auto ans = QMessageBox::question(this, QStringLiteral("取引の切り替え"),
        QStringLiteral("組み立て中の取引（%1 ⇄ %2）を破棄して、%3 ⇄ %4 の取引を始めますか？")
            .arg(pname(s, draft.a), pname(s, draft.b), pname(s, from), pname(s, to)));
    if (ans != QMessageBox::Yes) return false;
    draft = Draft();
    draft.a = from;
    draft.b = to;
    return true;
}

void TableWidget::draftAddCard(CardId c, int from, int to) {
    if (!draftEnsurePair(from, to)) return;
    auto& mine = from == draft.a ? draft.aCards : draft.bCards;
    auto& other = from == draft.a ? draft.bCards : draft.aCards;
    other.erase(std::remove(other.begin(), other.end(), c), other.end());
    if (std::find(mine.begin(), mine.end(), c) == mine.end()) mine.push_back(c);
    update();
}

void TableWidget::draftAddMoney(int from, int to, int amount) {
    if (amount <= 0) return;
    const GameState& s = engine.getState();
    if (from < 0 || from >= static_cast<int>(s.players.size())) return;
    // 自分の所持金以上は出せない（すでに取引欄に入れた分を差し引く）
    const bool samePair = !draft.empty() && ((from == draft.a && to == draft.b) || (from == draft.b && to == draft.a));
    const int already = samePair ? draftOffered(from) : 0;
    const int avail = std::max(0, s.players[from].money - already);
    if (avail <= 0) {
        showToast(QStringLiteral("%1 にはもう出せるお金がありません").arg(pname(s, from)), true);
        return;
    }
    if (amount > avail) {
        showToast(QStringLiteral("%1 の残り %2万 までにしました").arg(pname(s, from)).arg(avail), false);
        amount = avail;
    }
    if (!draftEnsurePair(from, to)) return;
    (from == draft.a ? draft.aMoney : draft.bMoney) += amount;
    update();
}

Action TableWidget::draftAction(ActionKind kind) const {
    Action a;
    a.kind = kind;
    a.player = draft.a;
    a.target = draft.b;
    a.trade.proposerId = draft.a;
    a.trade.targetId = draft.b;
    a.trade.offerCards = draft.aCards;
    a.trade.requestCards = draft.bCards;
    a.trade.offerMoney = draft.aMoney;
    a.trade.requestMoney = draft.bMoney;
    return a;
}

void TableWidget::demoTrade() {
    const GameState& s = engine.getState();
    if (s.players.size() < 2) return;
    auto owned = s.ownedBy(0);
    if (!owned.empty()) draftAddCard(owned.front(), 0, 1);
    draftAddMoney(1, 0, 120);
}

void TableWidget::demoHover(CardId c) {
    if (c == CardId::NONE) return;
    computeLayout();
    panel.visible = true;
    panel.card = c;
    panel.fromHand = false;
    panel.anchor = BoardPainter::cellRect(boardR, DataMaster::card(c).position);
    hoverValid = true;
    hoverScene = panel.anchor.center();
    update();
}
