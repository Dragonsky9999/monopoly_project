#include "TradeTray.hpp"
#include "CardVisual.hpp"
#include "DataMaster.hpp"

#include <QDragEnterEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace {
QString pname(const GameState& s, int id) { return CardVisual::playerName(s, id); }
}  // namespace

TradeTray::TradeTray(const MonopolyEngine& e, QWidget* parent) : QGroupBox(QStringLiteral("取引（ドラッグ&ドロップ）"), parent), engine(e) {
    setAcceptDrops(true);
    auto* lay = new QVBoxLayout(this);
    hint = new QLabel(QStringLiteral("プレイヤー一覧で、カードやコイン(¥)を別のプレイヤーの行へドラッグすると、ここに取引が組み立てられます。"));
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: gray;"));
    lay->addWidget(hint);
    pairLabel = new QLabel;
    pairLabel->setTextFormat(Qt::RichText);
    lay->addWidget(pairLabel);
    list = new QListWidget;
    list->setIconSize(QSize(20, 20));
    list->setMaximumHeight(120);
    list->setToolTip(QStringLiteral("ダブルクリックで項目を取り除く"));
    lay->addWidget(list);
    evalLabel = new QLabel;
    evalLabel->setWordWrap(true);
    evalLabel->setTextFormat(Qt::RichText);
    lay->addWidget(evalLabel);
    auto* row = new QHBoxLayout;
    okBtn = new QPushButton(QStringLiteral("成立"));
    cancelRecordBtn = new QPushButton(QStringLiteral("破談として記録"));
    clearBtn = new QPushButton(QStringLiteral("クリア"));
    okBtn->setMinimumHeight(30);
    row->addWidget(okBtn, 2);
    row->addWidget(cancelRecordBtn, 1);
    row->addWidget(clearBtn, 1);
    lay->addLayout(row);

    connect(okBtn, &QPushButton::clicked, this, [this] { emit submitRequested(buildAction(ActionKind::TRADE)); });
    connect(cancelRecordBtn, &QPushButton::clicked, this, [this] {
        emit submitRequested(buildAction(ActionKind::TRADE_CANCEL));
    });
    connect(clearBtn, &QPushButton::clicked, this, &TradeTray::clear);
    connect(list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        const int kind = item->data(Qt::UserRole).toInt();      // 0 = card, 1 = money
        const int side = item->data(Qt::UserRole + 1).toInt();  // 0 = a gives, 1 = b gives
        const int value = item->data(Qt::UserRole + 2).toInt();
        if (kind == 0) {
            auto& v = side == 0 ? aCards : bCards;
            v.erase(std::remove(v.begin(), v.end(), cardFromIndex(value)), v.end());
        } else {
            (side == 0 ? aMoney : bMoney) = 0;
        }
        if (aCards.empty() && bCards.empty() && aMoney == 0 && bMoney == 0) clear();
        else rebuild();
    });
    rebuild();
}

bool TradeTray::isEmpty() const { return a < 0; }

void TradeTray::clear() {
    a = b = -1;
    aCards.clear();
    bCards.clear();
    aMoney = bMoney = 0;
    rebuild();
}

bool TradeTray::ensurePair(int from, int to) {
    if (a < 0) {
        a = from;
        b = to;
        return true;
    }
    if ((from == a && to == b) || (from == b && to == a)) return true;
    const GameState& s = engine.getState();
    auto ans = QMessageBox::question(this, QStringLiteral("取引の切り替え"),
        QStringLiteral("組み立て中の取引（%1 ⇄ %2）を破棄して、%3 ⇄ %4 の取引を始めますか？")
            .arg(pname(s, a), pname(s, b), pname(s, from), pname(s, to)));
    if (ans != QMessageBox::Yes) return false;
    clear();
    a = from;
    b = to;
    return true;
}

bool TradeTray::addCard(CardId c, int from, int to) {
    if (!ensurePair(from, to)) return false;
    // すでに反対側に入っていれば取り除く（ドラッグで戻した扱い）
    auto& mine = (from == a) ? aCards : bCards;
    auto& other = (from == a) ? bCards : aCards;
    other.erase(std::remove(other.begin(), other.end(), c), other.end());
    if (std::find(mine.begin(), mine.end(), c) == mine.end()) mine.push_back(c);
    rebuild();
    return true;
}

bool TradeTray::addMoney(int from, int to, int amount) {
    if (!ensurePair(from, to)) return false;
    (from == a ? aMoney : bMoney) = amount;
    rebuild();
    return true;
}

Action TradeTray::buildAction(ActionKind kind) const {
    Action act;
    act.kind = kind;
    act.player = a;
    act.target = b;
    act.trade.proposerId = a;
    act.trade.targetId = b;
    act.trade.offerCards = aCards;
    act.trade.requestCards = bCards;
    act.trade.offerMoney = aMoney;
    act.trade.requestMoney = bMoney;
    return act;
}

void TradeTray::refresh() { rebuild(); }

void TradeTray::rebuild() {
    const GameState& s = engine.getState();
    const bool empty = (a < 0);
    hint->setVisible(empty);
    pairLabel->setVisible(!empty);
    list->setVisible(!empty);
    evalLabel->setVisible(!empty);
    okBtn->setEnabled(false);
    cancelRecordBtn->setEnabled(!empty);
    clearBtn->setEnabled(!empty);
    list->clear();
    if (empty) {
        evalLabel->clear();
        return;
    }
    pairLabel->setText(QStringLiteral("<b>%1</b> ⇄ <b>%2</b>").arg(pname(s, a), pname(s, b)));
    auto addItem = [&](int side, CardId c, int money) {
        const int from = side == 0 ? a : b, to = side == 0 ? b : a;
        QString text;
        QIcon icon;
        if (c != CardId::NONE) {
            text = QStringLiteral("%1   %2 → %3").arg(CardVisual::cardShortLabel(c), pname(s, from), pname(s, to));
            icon = CardVisual::cardIcon(c, &s.card(c));
        } else {
            text = QStringLiteral("%1万   %2 → %3").arg(money).arg(pname(s, from), pname(s, to));
            icon = QIcon(DragData::moneyPixmap(20));
        }
        auto* item = new QListWidgetItem(icon, text);
        item->setData(Qt::UserRole, c != CardId::NONE ? 0 : 1);
        item->setData(Qt::UserRole + 1, side);
        item->setData(Qt::UserRole + 2, c != CardId::NONE ? cardIndex(c) : money);
        list->addItem(item);
    };
    for (CardId c : aCards) addItem(0, c, 0);
    if (aMoney > 0) addItem(0, CardId::NONE, aMoney);
    for (CardId c : bCards) addItem(1, c, 0);
    if (bMoney > 0) addItem(1, CardId::NONE, bMoney);

    ApplyResult v = engine.validate(buildAction(ActionKind::TRADE));
    if (!v.ok) {
        evalLabel->setText(QStringLiteral("<span style='color:#e57373'>このままでは成立できません: %1</span>")
                               .arg(QString::fromStdString(v.error).toHtmlEscaped()));
        return;
    }
    okBtn->setEnabled(true);
    TradeOffer t = buildAction(ActionKind::TRADE).trade;
    double score = engine.evaluateTradeFairness(t);
    QString verdict;
    if (std::abs(score) <= 50) verdict = QStringLiteral("👍 妥当");
    else if (std::abs(score) <= 200) verdict = QStringLiteral("⚠ やや %1 有利").arg(pname(s, score > 0 ? a : b));
    else verdict = QStringLiteral("🔴 %1 が大きく有利").arg(pname(s, score > 0 ? a : b));
    evalLabel->setText(QStringLiteral("簡易評価: <b>%1</b> <span style='color:gray'>(スコア %2)</span>").arg(verdict).arg(score, 0, 'f', 1));
}

void TradeTray::dragEnterEvent(QDragEnterEvent* e) {
    if (DragData::hasAny(e->mimeData()) && a >= 0) e->acceptProposedAction();
}

// トレイに直接落とした場合: 持ち主側の「渡す」に入れる
void TradeTray::dropEvent(QDropEvent* e) {
    CardId c;
    int from = -1;
    if (DragData::readCard(e->mimeData(), c, from)) {
        if (from != a && from != b) {
            emit message(QStringLiteral("この取引（%1 ⇄ %2）の当事者のカードではありません")
                             .arg(pname(engine.getState(), a), pname(engine.getState(), b)), true);
            return;
        }
        e->acceptProposedAction();
        addCard(c, from, from == a ? b : a);
    } else if (DragData::readMoney(e->mimeData(), from)) {
        if (from != a && from != b) return;
        e->acceptProposedAction();
        const int to = from == a ? b : a;
        bool ok = false;
        const int max = std::max(1, engine.getState().players[from].money);
        int amount = QInputDialog::getInt(this, QStringLiteral("お金を渡す"),
                                          QStringLiteral("%1 → %2 に渡す金額（万）").arg(pname(engine.getState(), from), pname(engine.getState(), to)),
                                          std::min(50, max), 0, max, 10, &ok);
        if (ok) addMoney(from, to, amount);
    }
}
