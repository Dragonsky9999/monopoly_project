#include "Dialogs.hpp"
#include "CardVisual.hpp"
#include "DataMaster.hpp"

#include <QCheckBox>
#include <cmath>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
void fillPlayers(QComboBox* box, const GameState& s, bool aliveOnly, bool withBank, const QString& bankLabel) {
    box->clear();
    if (withBank) box->addItem(bankLabel, -1);
    for (const auto& p : s.players) {
        if (aliveOnly && p.bankrupt) continue;
        box->addItem(CardVisual::playerIcon(p.id), QStringLiteral("[%1] %2").arg(p.id + 1).arg(QString::fromStdString(p.name)), p.id);
    }
}
}  // namespace

// ============================================================================ NewGameDialog
NewGameDialog::NewGameDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("新しいゲーム"));
    auto* lay = new QVBoxLayout(this);

    auto* top = new QFormLayout;
    countSpin = new QSpinBox;
    countSpin->setRange(2, 10);
    countSpin->setValue(4);
    top->addRow(QStringLiteral("プレイヤー数"), countSpin);
    lay->addLayout(top);

    auto* namesBox = new QGroupBox(QStringLiteral("プレイヤー（上から手番順）"));
    auto* nl = new QVBoxLayout(namesBox);
    nameList = new QListWidget;
    nameList->setDragDropMode(QAbstractItemView::InternalMove);
    nameList->setDefaultDropAction(Qt::MoveAction);
    nameList->setSelectionMode(QAbstractItemView::SingleSelection);
    nameList->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                              QAbstractItemView::SelectedClicked);
    nameList->setIconSize(QSize(24, 24));
    nameList->setMinimumHeight(10 * 30);
    nl->addWidget(nameList);
    nl->addWidget(new QLabel(QStringLiteral("ドラッグで順番を入れ替え / ダブルクリックで名前を変更")));
    connect(nameList->model(), &QAbstractItemModel::rowsMoved, this, [this] { renumber(); });
    lay->addWidget(namesBox);

    auto* rules = new QGroupBox(QStringLiteral("ルール設定"));
    auto* form = new QFormLayout(rules);
    moneySpin = new QSpinBox;
    moneySpin->setRange(100, 100000);
    moneySpin->setSingleStep(100);
    moneySpin->setValue(1500);
    moneySpin->setSuffix(QStringLiteral(" 万"));
    goSpin = new QSpinBox;
    goSpin->setRange(0, 10000);
    goSpin->setSingleStep(50);
    goSpin->setValue(200);
    goSpin->setSuffix(QStringLiteral(" 万"));
    goSpin->setSpecialValueText(QStringLiteral("なし"));
    taxCheck = new QCheckBox(QStringLiteral("税金マスを有効にする (TAX-1: 200万, TAX-2: 100万)"));
    taxCheck->setChecked(true);
    doubleRentCheck = new QCheckBox(QStringLiteral("独占（家なしコンプリート）で地代2倍"));
    doubleRentCheck->setChecked(true);
    endOnBankruptCheck = new QCheckBox(QStringLiteral("脱落者が出たらゲーム終了（終了後に「続ける」も可）"));
    endOnBankruptCheck->setChecked(true);
    doublesCheck = new QCheckBox(QStringLiteral("ぞろ目でもう一度振る（連続3回まで）"));
    doublesCheck->setChecked(true);
    interestSpin = new QSpinBox;
    interestSpin->setRange(0, 100);
    interestSpin->setValue(10);
    interestSpin->setSuffix(QStringLiteral(" %"));
    form->addRow(QStringLiteral("初期資金"), moneySpin);
    form->addRow(QStringLiteral("GO 通過手当"), goSpin);
    form->addRow(QString(), taxCheck);
    form->addRow(QString(), doubleRentCheck);
    form->addRow(QString(), doublesCheck);
    form->addRow(QString(), endOnBankruptCheck);
    form->addRow(QStringLiteral("抵当解除の利息"), interestSpin);
    lay->addWidget(rules);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("開始"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("キャンセル"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(buttons);

    connect(countSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { updateNameRows(); });
    updateNameRows();
}

void NewGameDialog::updateNameRows() {
    const int n = countSpin->value();
    while (nameList->count() > n) delete nameList->takeItem(nameList->count() - 1);
    while (nameList->count() < n) {
        // まだ使っていない PlayerN を付ける
        int k = 1;
        auto used = [&](const QString& name) {
            for (int i = 0; i < nameList->count(); ++i)
                if (nameList->item(i)->text() == name) return true;
            return false;
        };
        while (used(QStringLiteral("Player%1").arg(k))) ++k;
        auto* it = new QListWidgetItem(QStringLiteral("Player%1").arg(k));
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled);
        nameList->addItem(it);
    }
    renumber();
}

// 並び順に合わせて番号付きの色アイコンを付け直す（色はプレイヤー番号で決まる）
void NewGameDialog::renumber() {
    for (int i = 0; i < nameList->count(); ++i) {
        QPixmap pm(24, 24);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(Qt::black, 1));
        p.setBrush(CardVisual::playerColor(i));
        p.drawEllipse(QRectF(1, 1, 22, 22));
        QFont f = p.font();
        f.setBold(true);
        f.setPixelSize(13);
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(QRectF(0, 0, 24, 24), Qt::AlignCenter, QString::number(i + 1));
        p.end();
        nameList->item(i)->setIcon(QIcon(pm));
    }
}

std::vector<std::string> NewGameDialog::playerNames() const {
    std::vector<std::string> v;
    for (int i = 0; i < nameList->count(); ++i) {
        QString n = nameList->item(i)->text().trimmed().left(24);
        n.replace(QLatin1Char('='), QLatin1Char('_'));
        n.replace(QLatin1Char(' '), QLatin1Char('_'));
        if (n.isEmpty()) n = QStringLiteral("Player%1").arg(i + 1);
        v.push_back(n.toStdString());
    }
    return v;
}

Settings NewGameDialog::settings() const {
    Settings s;
    s.startMoney = moneySpin->value();
    s.goBonus = goSpin->value();
    s.taxEnabled = taxCheck->isChecked();
    s.doubleRentOnSet = doubleRentCheck->isChecked();
    s.doublesRollAgain = doublesCheck->isChecked();
    s.endOnBankrupt = endOnBankruptCheck->isChecked();
    s.unmortgageInterestPct = interestSpin->value();
    return s;
}

// ============================================================================ TradeDialog
TradeDialog::TradeDialog(const MonopolyEngine& e, QWidget* parent) : QDialog(parent), engine(e) {
    setWindowTitle(QStringLiteral("取引（交渉）"));
    resize(560, 460);
    const GameState& s = engine.getState();
    auto* lay = new QVBoxLayout(this);

    auto* sides = new QHBoxLayout;
    auto makeSide = [&](const QString& title, QComboBox*& combo, QListWidget*& list, QSpinBox*& money) {
        auto* box = new QGroupBox(title);
        auto* v = new QVBoxLayout(box);
        combo = new QComboBox;
        fillPlayers(combo, s, true, false, QString());
        list = new QListWidget;
        list->setIconSize(QSize(22, 22));
        money = new QSpinBox;
        money->setRange(0, 1000000);
        money->setSingleStep(10);
        money->setSuffix(QStringLiteral(" 万"));
        v->addWidget(combo);
        v->addWidget(new QLabel(QStringLiteral("渡すカード（チェック）")));
        v->addWidget(list, 1);
        auto* h = new QHBoxLayout;
        h->addWidget(new QLabel(QStringLiteral("渡すお金")));
        h->addWidget(money, 1);
        v->addLayout(h);
        sides->addWidget(box);
    };
    makeSide(QStringLiteral("提案する人"), proposerCombo, proposerCards, proposerMoney);
    makeSide(QStringLiteral("相手"), targetCombo, targetCards, targetMoney);
    if (targetCombo->count() > 1) targetCombo->setCurrentIndex(1);
    lay->addLayout(sides, 1);

    evalLabel = new QLabel;
    evalLabel->setWordWrap(true);
    evalLabel->setTextFormat(Qt::RichText);
    lay->addWidget(evalLabel);

    auto* buttons = new QHBoxLayout;
    auto* doTrade = new QPushButton(QStringLiteral("成立として記録"));
    auto* doCancel = new QPushButton(QStringLiteral("破談として記録"));
    auto* close = new QPushButton(QStringLiteral("閉じる"));
    doTrade->setDefault(true);
    tradeButton = doTrade;
    buttons->addStretch();
    buttons->addWidget(doTrade);
    buttons->addWidget(doCancel);
    buttons->addWidget(close);
    lay->addLayout(buttons);

    connect(doTrade, &QPushButton::clicked, this, [this] { cancelRecord = false; accept(); });
    connect(doCancel, &QPushButton::clicked, this, [this] { cancelRecord = true; accept(); });
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    connect(proposerCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { rebuildLists(); });
    connect(targetCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { rebuildLists(); });
    connect(proposerCards, &QListWidget::itemChanged, this, [this](QListWidgetItem*) { updateEvaluation(); });
    // 行のどこをクリックしてもチェックが切り替わるように
    auto toggle = [](QListWidgetItem* item) {
        item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
    };
    connect(proposerCards, &QListWidget::itemClicked, this, toggle);
    connect(targetCards, &QListWidget::itemClicked, this, toggle);
    connect(targetCards, &QListWidget::itemChanged, this, [this](QListWidgetItem*) { updateEvaluation(); });
    connect(proposerMoney, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { updateEvaluation(); });
    connect(targetMoney, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { updateEvaluation(); });
    rebuildLists();
}

void TradeDialog::rebuildLists() {
    const GameState& s = engine.getState();
    auto fill = [&](QListWidget* list, int pid) {
        QSignalBlocker block(list);
        list->clear();
        if (pid < 0) return;
        for (CardId c : s.ownedBy(pid)) {
            const CardState& st = s.card(c);
            QString text = CardVisual::cardShortLabel(c);
            if (st.houses > 0) text += QStringLiteral("  家%1").arg(st.houses);
            if (st.mortgaged) text += QStringLiteral("  (抵当中)");
            auto* item = new QListWidgetItem(CardVisual::cardIcon(c, &st), text);
            item->setFlags(Qt::ItemIsEnabled);  // チェックはクリックで切り替え（itemClicked）
            item->setCheckState(Qt::Unchecked);
            item->setData(Qt::UserRole, cardIndex(c));
            item->setToolTip(CardVisual::cardTooltip(c, s));
            list->addItem(item);
        }
    };
    fill(proposerCards, proposerCombo->currentData().toInt());
    fill(targetCards, targetCombo->currentData().toInt());
    int pId = proposerCombo->currentData().toInt(), tId = targetCombo->currentData().toInt();
    if (pId >= 0) proposerMoney->setMaximum(qMax(0, s.players[pId].money));
    if (tId >= 0) targetMoney->setMaximum(qMax(0, s.players[tId].money));
    updateEvaluation();
}

TradeOffer TradeDialog::currentOffer() const {
    TradeOffer t;
    t.proposerId = proposerCombo->currentData().toInt();
    t.targetId = targetCombo->currentData().toInt();
    t.offerMoney = proposerMoney->value();
    t.requestMoney = targetMoney->value();
    for (int i = 0; i < proposerCards->count(); ++i)
        if (proposerCards->item(i)->checkState() == Qt::Checked)
            t.offerCards.push_back(cardFromIndex(proposerCards->item(i)->data(Qt::UserRole).toInt()));
    for (int i = 0; i < targetCards->count(); ++i)
        if (targetCards->item(i)->checkState() == Qt::Checked)
            t.requestCards.push_back(cardFromIndex(targetCards->item(i)->data(Qt::UserRole).toInt()));
    return t;
}

void TradeDialog::updateEvaluation() {
    TradeOffer t = currentOffer();
    Action a = action();
    a.kind = ActionKind::TRADE;
    ApplyResult v = engine.validate(a);
    if (tradeButton) tradeButton->setEnabled(v.ok && t.proposerId != t.targetId);
    if (t.proposerId == t.targetId) {
        evalLabel->setText(QStringLiteral("<span style='color:#c00'>同じプレイヤー同士では取引できません</span>"));
        return;
    }
    if (!v.ok) {
        evalLabel->setText(QStringLiteral("<span style='color:#c00'>このままでは成立できません: %1</span>")
                               .arg(QString::fromStdString(v.error).toHtmlEscaped()));
        return;
    }
    double score = engine.evaluateTradeFairness(t);
    const GameState& s = engine.getState();
    QString pn = QString::fromStdString(s.players[t.proposerId].name);
    QString tn = QString::fromStdString(s.players[t.targetId].name);
    QString verdict;
    if (std::abs(score) <= 30) verdict = QStringLiteral("👍 妥当");
    else if (std::abs(score) <= 100) verdict = QStringLiteral("⚠ やや %1 有利").arg(score > 0 ? pn : tn);
    else verdict = QStringLiteral("🔴 %1 が大きく有利").arg(score > 0 ? pn : tn);
    evalLabel->setText(QStringLiteral("簡易評価: <b>%1</b> &nbsp;<span style='color:gray'>(スコア %2、正なら提案者有利)</span>")
                           .arg(verdict).arg(score, 0, 'f', 1));
}

Action TradeDialog::action() const {
    Action a;
    a.kind = cancelRecord ? ActionKind::TRADE_CANCEL : ActionKind::TRADE;
    a.trade = currentOffer();
    a.player = a.trade.proposerId;
    a.target = a.trade.targetId;
    return a;
}

// ============================================================================ SmallDialogs
namespace SmallDialogs {

bool askAuction(QWidget* parent, const GameState& s, int& winner, int& price) {
    QDialog d(parent);
    d.setWindowTitle(QStringLiteral("オークション結果"));
    auto* form = new QFormLayout(&d);
    auto* chip = new QLabel;
    chip->setPixmap(CardVisual::cardIcon(s.pendingPurchase, nullptr, 28).pixmap(28, 28));
    form->addRow(QStringLiteral("対象カード"), chip);
    auto* who = new QComboBox;
    fillPlayers(who, s, true, true, QStringLiteral("落札者なし"));
    if (who->count() > 1) who->setCurrentIndex(1);
    auto* amount = new QSpinBox;
    amount->setRange(0, 1000000);
    amount->setSingleStep(10);
    amount->setSuffix(QStringLiteral(" 万"));
    form->addRow(QStringLiteral("落札者"), who);
    form->addRow(QStringLiteral("落札額"), amount);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    QObject::connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    QObject::connect(who, qOverload<int>(&QComboBox::currentIndexChanged), &d,
                     [who, amount](int) { amount->setEnabled(who->currentData().toInt() >= 0); });
    form->addRow(bb);
    if (d.exec() != QDialog::Accepted) return false;
    winner = who->currentData().toInt();
    price = winner >= 0 ? amount->value() : 0;
    return true;
}

bool askDice(QWidget* parent, int& d1, int& d2) {
    QDialog d(parent);
    d.setWindowTitle(QStringLiteral("出目を修正"));
    auto* form = new QFormLayout(&d);
    auto* s1 = new QSpinBox;
    auto* s2 = new QSpinBox;
    for (auto* sp : {s1, s2}) {
        sp->setRange(1, 6);
        sp->setMinimumWidth(90);
    }
    s1->setValue(d1);
    s2->setValue(d2);
    form->addRow(new QLabel(QStringLiteral("直前の移動を、この出目でやり直します。")));
    form->addRow(QStringLiteral("サイコロ1"), s1);
    form->addRow(QStringLiteral("サイコロ2"), s2);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    QObject::connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    form->addRow(bb);
    s1->setFocus();
    s1->selectAll();
    if (d.exec() != QDialog::Accepted) return false;
    d1 = s1->value();
    d2 = s2->value();
    return true;
}

bool askBankrupt(QWidget* parent, const GameState& s, int& player, int& creditor) {
    QDialog d(parent);
    d.setWindowTitle(QStringLiteral("破産"));
    auto* form = new QFormLayout(&d);
    auto* who = new QComboBox;
    fillPlayers(who, s, true, false, QString());
    int idx = who->findData(s.activePlayer);
    if (idx >= 0) who->setCurrentIndex(idx);
    auto* cred = new QComboBox;
    fillPlayers(cred, s, true, true, QStringLiteral("銀行（カードは未所有に戻る）"));
    int cidx = cred->findData(s.debtCreditor);
    if (cidx >= 0) cred->setCurrentIndex(cidx);
    form->addRow(QStringLiteral("破産するプレイヤー"), who);
    form->addRow(QStringLiteral("債権者（資産の受取人）"), cred);
    form->addRow(new QLabel(QStringLiteral("家は半額で現金化され、お金とカードが債権者に移ります。")));
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    QObject::connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    form->addRow(bb);
    if (d.exec() != QDialog::Accepted) return false;
    player = who->currentData().toInt();
    creditor = cred->currentData().toInt();
    return true;
}

bool askOverride(QWidget* parent, const GameState& s, CardId selected, Action& out) {
    QDialog d(parent);
    d.setWindowTitle(QStringLiteral("管理者修正（ルールを無視して直接書き換え）"));
    auto* form = new QFormLayout(&d);
    auto* kind = new QComboBox;
    kind->addItem(QStringLiteral("所持金を設定"), int(ActionKind::SET_MONEY));
    kind->addItem(QStringLiteral("位置を設定"), int(ActionKind::SET_POS));
    kind->addItem(QStringLiteral("カードの所有者を設定"), int(ActionKind::SET_OWNER));
    kind->addItem(QStringLiteral("家の数を設定"), int(ActionKind::SET_HOUSES));
    auto* player = new QComboBox;
    fillPlayers(player, s, false, false, QString());
    auto* owner = new QComboBox;
    fillPlayers(owner, s, false, true, QStringLiteral("銀行（未所有）"));
    auto* card = new QComboBox;
    card->setIconSize(QSize(20, 20));
    for (const Card& c : DataMaster::cards())
        card->addItem(CardVisual::cardIcon(c.id, &s.card(c.id), 20), CardVisual::cardShortLabel(c.id), cardIndex(c.id));
    if (selected != CardId::NONE) card->setCurrentIndex(cardIndex(selected));
    auto* value = new QSpinBox;
    value->setRange(-1000000, 1000000);

    form->addRow(QStringLiteral("内容"), kind);
    form->addRow(QStringLiteral("プレイヤー"), player);
    form->addRow(QStringLiteral("カード"), card);
    form->addRow(QStringLiteral("新しい所有者"), owner);
    form->addRow(QStringLiteral("値"), value);

    auto refresh = [=]() {
        auto k = ActionKind(kind->currentData().toInt());
        const bool usesPlayer = (k == ActionKind::SET_MONEY || k == ActionKind::SET_POS);
        form->setRowVisible(player, usesPlayer);
        form->setRowVisible(card, !usesPlayer);
        form->setRowVisible(owner, k == ActionKind::SET_OWNER);
        form->setRowVisible(value, k != ActionKind::SET_OWNER);
        int pid = player->currentData().toInt();
        CardId cid = cardFromIndex(card->currentData().toInt());
        if (k == ActionKind::SET_MONEY) { value->setRange(-1000000, 1000000); value->setValue(s.players[pid].money); }
        if (k == ActionKind::SET_POS) { value->setRange(0, 39); value->setValue(s.players[pid].position); }
        if (k == ActionKind::SET_HOUSES) { value->setRange(0, 5); value->setValue(s.card(cid).houses); }
        if (k == ActionKind::SET_OWNER) {
            int i = owner->findData(s.card(cid).owner);
            if (i >= 0) owner->setCurrentIndex(i);
        }
    };
    QObject::connect(kind, qOverload<int>(&QComboBox::currentIndexChanged), &d, [=](int) { refresh(); });
    QObject::connect(player, qOverload<int>(&QComboBox::currentIndexChanged), &d, [=](int) { refresh(); });
    QObject::connect(card, qOverload<int>(&QComboBox::currentIndexChanged), &d, [=](int) { refresh(); });
    refresh();

    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    QObject::connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    form->addRow(bb);
    if (d.exec() != QDialog::Accepted) return false;

    Action a;
    a.kind = ActionKind(kind->currentData().toInt());
    if (a.kind == ActionKind::SET_MONEY || a.kind == ActionKind::SET_POS) {
        a.player = player->currentData().toInt();
        a.amount = value->value();
    } else {
        a.card = cardFromIndex(card->currentData().toInt());
        a.player = s.card(a.card).owner;
        if (a.kind == ActionKind::SET_OWNER) a.target = owner->currentData().toInt();
        else a.amount = value->value();
    }
    out = a;
    return true;
}

}  // namespace SmallDialogs
