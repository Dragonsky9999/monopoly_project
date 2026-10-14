#include "MainWindow.hpp"
#include "CardVisual.hpp"
#include "DataMaster.hpp"
#include "Dialogs.hpp"
#include "GameLog.hpp"
#include "TableWidget.hpp"

#include <QAction>
#include <QActionGroup>
#include <QSettings>
#include <cmath>
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

namespace {
QString qs(const std::string& s) { return QString::fromStdString(s); }

QPushButton* makeButton(const QString& text, QWidget* parent = nullptr) {
    auto* b = new QPushButton(text, parent);
    b->setMinimumHeight(30);
    return b;
}
}  // namespace

// ============================================================================ 構築
MainWindow::MainWindow(QWidget* parent, bool restoreAutosave) : QMainWindow(parent) {
    resize(1500, 950);

    // ---- 中央: テーブル（盤面 + プレイヤーエリア）
    table = new TableWidget(engine);
    setCentralWidget(table);
    connect(table, &TableWidget::actionRequested, this, [this](const Action& a) { submit(a); });
    connect(table, &TableWidget::rollCorrectionRequested, this, [this](int d1, int d2) { correctRoll(d1, d2); });
    connect(table, &TableWidget::cardSelected, this, &MainWindow::selectCard);

    // ---- 右: 履歴
    logList = new QListWidget;
    logList->setIconSize(QSize(16, 16));
    logList->setWordWrap(true);
    logDock = new QDockWidget(QStringLiteral("履歴（クリックでその時点へ移動）"), this);
    logDock->setObjectName(QStringLiteral("logDock"));
    logDock->setWidget(logList);
    logDock->setMinimumWidth(300);
    addDockWidget(Qt::RightDockWidgetArea, logDock);
    connect(logList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (updating) return;
        const size_t idx = static_cast<size_t>(item->data(Qt::UserRole).toInt());
        QTimer::singleShot(0, this, [this, idx] { gotoIndex(idx); });  // リスト再構築はシグナル処理の後で
    });

    // ---- 右: 詳細パネル（予備。既定は非表示）
    auto* detailScroll = new QScrollArea;
    detailScroll->setWidget(buildDetailPanel());
    detailScroll->setWidgetResizable(true);
    detailScroll->setFrameShape(QFrame::NoFrame);
    detailDock = new QDockWidget(QStringLiteral("詳細パネル"), this);
    detailDock->setObjectName(QStringLiteral("detailDock"));
    detailDock->setWidget(detailScroll);
    addDockWidget(Qt::RightDockWidgetArea, detailDock);
    detailDock->hide();
    resizeDocks({logDock}, {330}, Qt::Horizontal);

    baseFont = QApplication::font();
    textScale = QSettings().value(QStringLiteral("textScale"), 1.0).toDouble();
    buildMenus();
    buildReplayBar();
    messageLabel = new QLabel;
    statusBar()->addWidget(messageLabel, 1);

    // 前回の自動保存があれば復元
    QFile f(autosavePath());
    if (restoreAutosave && f.exists() && f.open(QIODevice::ReadOnly)) {
        std::string text = f.readAll().toStdString();
        GameLog::LoadResult r = GameLog::deserialize(engine, text);
        if (r.ok) {
            currentFile = QSettings().value(QStringLiteral("currentFile")).toString();  // 続けて同じファイルに記録
            showInfo(QStringLiteral("前回の自動保存から復元しました（%1 手）").arg(r.appliedActions));
        } else if (r.appliedActions > 0) {
            QFile::remove(autosavePath() + QStringLiteral(".broken"));
            QFile::copy(autosavePath(), autosavePath() + QStringLiteral(".broken"));
            showError(QStringLiteral("自動保存の %1 行目が読めませんでした（%2 手目まで復元）。元のファイルは autosave.log.broken に保存しました")
                          .arg(r.errorLine).arg(r.appliedActions));
        } else {
            engine.newGame({"Player1", "Player2", "Player3", "Player4"});
        }
    } else {
        engine.newGame({"Player1", "Player2", "Player3", "Player4"});
        showInfo(QStringLiteral("「ファイル → 新しいゲーム」でプレイヤーを設定してください"));
    }
    dirty = false;
    rebuildReadyButtons();
    applyTextScale(textScale);
    refreshAll();
}

void MainWindow::applyTextScale(double s) {
    textScale = std::clamp(s, 0.6, 2.5);
    QSettings().setValue(QStringLiteral("textScale"), textScale);
    QFont f = baseFont;
    f.setPointSizeF(baseFont.pointSizeF() * textScale);
    QApplication::setFont(f);
    table->setTextScale(textScale);
    showInfo(QStringLiteral("文字サイズ: %1%（表示 → 文字サイズ / Ctrl+＋ / Ctrl+－）").arg(int(textScale * 100 + 0.5)));
}

void MainWindow::buildMenus() {
    QMenu* file = menuBar()->addMenu(QStringLiteral("ファイル(&F)"));
    QAction* a = file->addAction(QStringLiteral("新しいゲーム..."), this, &MainWindow::newGame);
    a->setShortcut(QKeySequence::New);
    a = file->addAction(QStringLiteral("ログを開く..."), this, &MainWindow::openLog);
    a->setShortcut(QKeySequence::Open);
    a = file->addAction(QStringLiteral("保存"), this, [this] { saveLog(); });
    a->setShortcut(QKeySequence::Save);
    a = file->addAction(QStringLiteral("名前を付けて保存..."), this, [this] { saveLogAs(); });
    a->setShortcut(QKeySequence::SaveAs);
    file->addSeparator();
    file->addAction(QStringLiteral("終了"), this, &QWidget::close);

    QMenu* edit = menuBar()->addMenu(QStringLiteral("編集(&E)"));
    // 「元に戻す / やり直し」と「1手戻る / 1手進む」は同じ操作なので1組にまとめる（Ctrl+Z / Ctrl+Y）
    undoAct = edit->addAction(QStringLiteral("◀ 1手戻る"), this, [this] {
        if (engine.undo()) { autosave(); refreshAll(); }
    });
    undoAct->setShortcut(QKeySequence::Undo);
    redoAct = edit->addAction(QStringLiteral("1手進む ▶"), this, [this] {
        if (engine.redo()) { autosave(); refreshAll(); }
    });
    redoAct->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_Y), QKeySequence::Redo});
    edit->addSeparator();
    edit->addAction(QStringLiteral("管理者修正..."), this, [this] {
        Action a;
        if (SmallDialogs::askOverride(this, engine.getState(), selected, a)) submit(a);
    });

    QMenu* view = menuBar()->addMenu(QStringLiteral("表示(&V)"));
    view->addAction(logDock->toggleViewAction());
    view->addAction(detailDock->toggleViewAction());
    view->addSeparator();
    QMenu* sizeMenu = view->addMenu(QStringLiteral("文字サイズ"));
    auto* sizeGroup = new QActionGroup(this);
    const std::pair<QString, double> sizes[] = {
        {QStringLiteral("小"), 0.85}, {QStringLiteral("標準"), 1.0}, {QStringLiteral("大"), 1.2},
        {QStringLiteral("特大"), 1.45}, {QStringLiteral("最大"), 1.75}};
    for (const auto& [label, value] : sizes) {
        QAction* act = sizeMenu->addAction(label);
        act->setCheckable(true);
        act->setData(value);
        sizeGroup->addAction(act);
        if (std::abs(value - textScale) < 0.01) act->setChecked(true);
        connect(act, &QAction::triggered, this, [this, value] { applyTextScale(value); });
    }
    sizeMenu->addSeparator();
    QAction* bigger = sizeMenu->addAction(QStringLiteral("大きく"), this, [this] { applyTextScale(textScale * 1.1); });
    bigger->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_Plus), QKeySequence(Qt::CTRL | Qt::Key_Equal), QKeySequence::ZoomIn});
    QAction* smaller = sizeMenu->addAction(QStringLiteral("小さく"), this, [this] { applyTextScale(textScale / 1.1); });
    smaller->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_Minus), QKeySequence::ZoomOut});
    QAction* reset = sizeMenu->addAction(QStringLiteral("標準に戻す"), this, [this] { applyTextScale(1.0); });
    reset->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));

    QMenu* help = menuBar()->addMenu(QStringLiteral("ヘルプ(&H)"));
    help->addAction(QStringLiteral("使い方"), this, [this] {
        QMessageBox::information(this, QStringLiteral("使い方"), QStringLiteral(
            "盤面の周りが各プレイヤーのエリア（所持金・紙幣・手札）です。\n\n"
            "・購入フェーズ: 盤面のカードを手番の人のエリアへドラッグで購入。中央の「パス」でパス。\n"
            "・交渉フェーズ: カードや紙幣を相手のエリアへドラッグ → 盤面中央の取引欄で「成立」。\n"
            "  紙幣はドラッグで1枚、Shift+ドラッグで束ごと。合計金額をドラッグすると金額を指定できます。\n"
            "  交渉が終わったら中央の「交渉終了 → 運用フェーズへ」。\n"
            "・運用フェーズ: 中央のサイコロをクリックで出目を合わせて「移動」（「ランダム」は出目を作るだけ）。\n"
            "  未所有マスに止まると、マスの近くに 購入 / 見送り / オークション が出ます。\n"
            "・カードを右クリック: 建設・売却・抵当。プレイヤーエリアを右クリック: 破産・修正。\n"
            "・マスやカードにカーソルを乗せると詳細が出ます。\n"
            "・上のバーで過去の任意の時点に移動できます。操作のたびに記録ファイルへ自動で書き込みます\n"
            "  （名前を指定しなければ ドキュメント\\MonopolyRecorder\\monopoly_日時.log）。次回起動時にも復元されます。"));
    });
}

void MainWindow::buildReplayBar() {
    QToolBar* bar = addToolBar(QStringLiteral("リプレイ"));
    bar->setMovable(false);
    firstAct = bar->addAction(QStringLiteral("⏮ 最初"), this, [this] { gotoIndex(0); });
    prevAct = undoAct;  // 編集メニューの「1手戻る」と同じもの
    bar->addAction(prevAct);
    nextAct = redoAct;
    bar->addAction(nextAct);
    lastAct = bar->addAction(QStringLiteral("最新 ⏭"), this, [this] { gotoIndex(timelineLength()); });

    timeline = new QSlider(Qt::Horizontal);
    timeline->setMinimumWidth(260);
    bar->addWidget(timeline);
    timelineLabel = new QLabel;
    timelineLabel->setMinimumWidth(110);
    bar->addWidget(timelineLabel);
    connect(timeline, &QSlider::valueChanged, this, [this](int v) {
        if (!updating) gotoIndex(static_cast<size_t>(v));
    });

    bar->addSeparator();
    bar->addWidget(new QLabel(QStringLiteral(" ターン ")));
    turnSpin = new QSpinBox;
    turnSpin->setRange(1, 99999);
    bar->addWidget(turnSpin);
    bar->addAction(QStringLiteral("へ移動"), this, [this] {
        const int t = turnSpin->value();
        size_t cur = engine.getActions().size();
        gotoIndex(timelineLength());
        for (size_t i = 0; i < engine.getActions().size(); ++i) {
            const GameState& b = engine.getStateBeforeAction(i);
            if (b.phase == Phase::OPERATIONAL && b.turn == t) {
                gotoIndex(i);
                return;
            }
        }
        gotoIndex(cur);
        showError(QStringLiteral("ターン %1 は見つかりません").arg(t));
    });
}

QWidget* MainWindow::buildDetailPanel() {
    auto* panel = new QWidget;
    auto* lay = new QVBoxLayout(panel);

    auto* cardBox = new QGroupBox(QStringLiteral("選択中のカード"));
    auto* cl = new QVBoxLayout(cardBox);
    selectedInfo = new QLabel;
    selectedInfo->setTextFormat(Qt::RichText);
    selectedInfo->setWordWrap(true);
    cl->addWidget(selectedInfo);
    auto* cardBtns = new QGridLayout;
    buildBtn = makeButton(QStringLiteral("家を建てる"));
    sellBtn = makeButton(QStringLiteral("家を売る"));
    mortgageBtn = makeButton(QStringLiteral("抵当に入れる"));
    unmortgageBtn = makeButton(QStringLiteral("抵当解除"));
    cardBtns->addWidget(buildBtn, 0, 0);
    cardBtns->addWidget(sellBtn, 0, 1);
    cardBtns->addWidget(mortgageBtn, 1, 0);
    cardBtns->addWidget(unmortgageBtn, 1, 1);
    cl->addLayout(cardBtns);
    connect(buildBtn, &QPushButton::clicked, this, [this] { cardAction(ActionKind::BUILD); });
    connect(sellBtn, &QPushButton::clicked, this, [this] { cardAction(ActionKind::SELL); });
    connect(mortgageBtn, &QPushButton::clicked, this, [this] { cardAction(ActionKind::MORTGAGE); });
    connect(unmortgageBtn, &QPushButton::clicked, this, [this] { cardAction(ActionKind::UNMORTGAGE); });
    lay->addWidget(cardBox);

    purchaseBox = new QGroupBox(QStringLiteral("① 購入フェーズ"));
    auto* pl = new QVBoxLayout(purchaseBox);
    purchaseTurnLabel = new QLabel;
    purchaseTurnLabel->setWordWrap(true);
    pl->addWidget(purchaseTurnLabel);
    auto* pRow = new QHBoxLayout;
    buySelectedBtn = makeButton(QStringLiteral("選択カードを購入"));
    auto* passBtn = makeButton(QStringLiteral("パス"));
    pRow->addWidget(buySelectedBtn);
    pRow->addWidget(passBtn);
    pl->addLayout(pRow);
    connect(buySelectedBtn, &QPushButton::clicked, this, [this] {
        Action a;
        a.kind = ActionKind::BUY;
        a.player = engine.getState().activePlayer;
        a.card = selected;
        submit(a);
    });
    connect(passBtn, &QPushButton::clicked, this, [this] {
        Action a;
        a.kind = ActionKind::PASS;
        a.player = engine.getState().activePlayer;
        submit(a);
    });
    lay->addWidget(purchaseBox);

    negotiationBox = new QGroupBox(QStringLiteral("② 交渉フェーズ"));
    auto* nl = new QVBoxLayout(negotiationBox);
    auto* tradeBtn = makeButton(QStringLiteral("取引をダイアログで記録..."));
    nl->addWidget(tradeBtn);
    connect(tradeBtn, &QPushButton::clicked, this, [this] {
        TradeDialog d(engine, this);
        if (d.exec() == QDialog::Accepted) submit(d.action());
    });
    nl->addWidget(new QLabel(QStringLiteral("準備完了:")));
    auto* readyHolder = new QWidget;
    readyGrid = new QGridLayout(readyHolder);
    readyGrid->setContentsMargins(0, 0, 0, 0);
    nl->addWidget(readyHolder);
    lay->addWidget(negotiationBox);

    operationBox = new QGroupBox(QStringLiteral("③ 運用フェーズ"));
    auto* ol = new QVBoxLayout(operationBox);
    operationTurnLabel = new QLabel;
    operationTurnLabel->setWordWrap(true);
    ol->addWidget(operationTurnLabel);
    auto* diceRow = new QHBoxLayout;
    die1 = new QSpinBox;
    die2 = new QSpinBox;
    for (auto* d : {die1, die2}) d->setRange(1, 6);
    rollBtn = makeButton(QStringLiteral("この出目で移動"));
    diceRow->addWidget(die1);
    diceRow->addWidget(new QLabel(QStringLiteral("+")));
    diceRow->addWidget(die2);
    diceRow->addWidget(rollBtn, 1);
    ol->addLayout(diceRow);
    connect(rollBtn, &QPushButton::clicked, this, [this] {
        Action a;
        a.kind = ActionKind::ROLL;
        a.player = engine.getState().activePlayer;
        a.d1 = die1->value();
        a.d2 = die2->value();
        submit(a);
    });
    pendingRow = new QWidget;
    auto* pr = new QHBoxLayout(pendingRow);
    pr->setContentsMargins(0, 0, 0, 0);
    pendingLabel = new QLabel;
    buyPendingBtn = makeButton(QStringLiteral("購入"));
    auto* declineBtn = makeButton(QStringLiteral("見送り"));
    auto* auctionBtn = makeButton(QStringLiteral("オークション..."));
    pr->addWidget(pendingLabel, 1);
    pr->addWidget(buyPendingBtn);
    pr->addWidget(declineBtn);
    pr->addWidget(auctionBtn);
    ol->addWidget(pendingRow);
    connect(buyPendingBtn, &QPushButton::clicked, this, [this] {
        Action a;
        a.kind = ActionKind::BUY;
        a.player = engine.getState().activePlayer;
        submit(a);
    });
    connect(declineBtn, &QPushButton::clicked, this, [this] {
        Action a;
        a.kind = ActionKind::DECLINE;
        a.player = engine.getState().activePlayer;
        submit(a);
    });
    connect(auctionBtn, &QPushButton::clicked, this, [this] {
        int winner = -1, price = 0;
        if (!SmallDialogs::askAuction(this, engine.getState(), winner, price)) return;
        Action a;
        a.kind = ActionKind::AUCTION;
        a.player = engine.getState().activePlayer;
        a.target = winner;
        a.amount = price;
        submit(a);
    });
    endTurnBtn = makeButton(QStringLiteral("ターン終了 ▶"));
    ol->addWidget(endTurnBtn);
    connect(endTurnBtn, &QPushButton::clicked, this, [this] {
        Action a;
        a.kind = ActionKind::END_TURN;
        a.player = engine.getState().activePlayer;
        submit(a);
    });
    auto* tradeBtn2 = makeButton(QStringLiteral("取引をダイアログで記録..."));
    ol->addWidget(tradeBtn2);
    connect(tradeBtn2, &QPushButton::clicked, this, [this] {
        TradeDialog d(engine, this);
        if (d.exec() == QDialog::Accepted) submit(d.action());
    });
    lay->addWidget(operationBox);
    lay->addStretch();
    return panel;
}

void MainWindow::rebuildReadyButtons() {
    for (auto* b : readyButtons) b->deleteLater();
    readyButtons.clear();
    const GameState& s = engine.getState();
    for (const auto& p : s.players) {
        auto* b = makeButton(QString());
        b->setIcon(CardVisual::playerIcon(p.id));
        const int id = p.id;
        connect(b, &QPushButton::clicked, this, [this, id] {
            Action a;
            a.kind = ActionKind::READY;
            a.player = id;
            submit(a);
        });
        readyGrid->addWidget(b, p.id / 3, p.id % 3);
        readyButtons.push_back(b);
    }
}

// ============================================================================ 更新
void MainWindow::refreshAll() {
    updating = true;
    table->setSelectedCard(selected);
    table->refresh();
    updateDetailPanel();
    updateLog();
    updateReplayBar();
    QString name = currentFile.isEmpty() ? QStringLiteral("無題") : QFileInfo(currentFile).fileName();
    setWindowTitle(QStringLiteral("%1%2 - Business Monopoly Recorder").arg(name, dirty ? QStringLiteral(" *") : QString()));
    updating = false;
}

void MainWindow::updateDetailPanel() {
    const GameState& s = engine.getState();
    purchaseBox->setVisible(s.phase == Phase::PURCHASE);
    negotiationBox->setVisible(s.phase == Phase::NEGOTIATION);
    operationBox->setVisible(s.phase == Phase::OPERATIONAL);
    if (s.phase == Phase::PURCHASE) {
        purchaseTurnLabel->setText(QStringLiteral("手番: <b>%1</b>（%2万） 連続パス %3/%4")
                                       .arg(qs(s.players[s.activePlayer].name)).arg(s.players[s.activePlayer].money)
                                       .arg(s.consecutivePasses).arg(s.aliveCount()));
        buySelectedBtn->setEnabled(selected != CardId::NONE && s.card(selected).owner == -1);
    }
    if (s.phase == Phase::NEGOTIATION) {
        for (size_t i = 0; i < readyButtons.size() && i < s.players.size(); ++i) {
            const Player& p = s.players[i];
            readyButtons[i]->setText(p.ready ? QStringLiteral("%1 ✔").arg(qs(p.name)) : qs(p.name));
            readyButtons[i]->setEnabled(!p.ready && !p.bankrupt);
        }
    }
    if (s.phase == Phase::OPERATIONAL) {
        const Player& ap = s.players[s.activePlayer];
        operationTurnLabel->setText(QStringLiteral("ターン %1 手番: <b>%2</b>（%3万）").arg(s.turn).arg(qs(ap.name)).arg(ap.money));
        rollBtn->setEnabled(!s.hasRolled);
        pendingRow->setVisible(s.pendingPurchase != CardId::NONE);
        if (s.pendingPurchase != CardId::NONE) {
            const Card& c = DataMaster::card(s.pendingPurchase);
            pendingLabel->setText(QStringLiteral("%1（%2万）").arg(CardVisual::cardShortLabel(c.id)).arg(c.price));
            buyPendingBtn->setEnabled(ap.money >= c.price);
        }
        endTurnBtn->setEnabled(s.hasRolled && s.pendingPurchase == CardId::NONE);
    }
    auto can = [&](ActionKind k) {
        if (selected == CardId::NONE) return false;
        Action a;
        a.kind = k;
        a.card = selected;
        a.player = s.card(selected).owner;
        return a.player >= 0 && engine.validate(a).ok;
    };
    buildBtn->setEnabled(can(ActionKind::BUILD));
    sellBtn->setEnabled(can(ActionKind::SELL));
    mortgageBtn->setEnabled(can(ActionKind::MORTGAGE));
    unmortgageBtn->setEnabled(can(ActionKind::UNMORTGAGE));
    selectedInfo->setText(selected == CardId::NONE ? QStringLiteral("<span style='color:gray'>盤面のカードをクリック</span>")
                                                   : CardVisual::cardTooltip(selected, s));
}

void MainWindow::updateLog() {
    logList->clear();
    const auto& acts = engine.getActions();
    const size_t applied = acts.size();
    QFont futureFont = logList->font();
    futureFont.setItalic(true);

    auto addRow = [&](size_t index, const Action& a, const GameState& before, const QString& events, bool future) {
        QString head = QStringLiteral("#%1 ").arg(index + 1);
        if (before.phase == Phase::OPERATIONAL) head += QStringLiteral("[T%1] ").arg(before.turn);
        else if (before.phase == Phase::PURCHASE) head += QStringLiteral("[購入] ");
        else if (before.phase == Phase::NEGOTIATION) head += QStringLiteral("[交渉] ");
        QString text = head + qs(GameLog::describeAction(a, before));
        if (!events.isEmpty()) text += QStringLiteral("\n    ") + events;
        auto* item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, static_cast<int>(index + 1));  // クリックでこの手の「直後」へ
        if (a.card != CardId::NONE) item->setIcon(CardVisual::cardIcon(a.card));
        else if (a.player >= 0) item->setIcon(CardVisual::playerIcon(a.player));
        if (future) {
            item->setForeground(QColor(150, 150, 150));
            item->setFont(futureFont);
        }
        logList->addItem(item);
    };

    for (size_t i = 0; i < applied; ++i) {
        const GameState& after = (i + 1 < applied) ? engine.getStateBeforeAction(i + 1) : engine.getState();
        QStringList ev;
        for (const auto& e : engine.getEventsOfAction(i)) {
            if (e.type == ActionType::END_TURN || e.type == ActionType::PASS || e.type == ActionType::READY) continue;
            if (e.type == ActionType::PHASE_CHANGE) { ev << QStringLiteral("== フェーズ移行 =="); continue; }
            ev << qs(GameLog::describeEvent(e, after));
        }
        addRow(i, acts[i], engine.getStateBeforeAction(i), ev.join(QStringLiteral("\n    ")), false);
    }
    const auto& redo = engine.getRedoActions();
    size_t idx = applied;
    for (auto it = redo.rbegin(); it != redo.rend(); ++it, ++idx) addRow(idx, *it, engine.getState(), QString(), true);

    if (applied > 0) {
        logList->setCurrentRow(static_cast<int>(applied - 1));
        logList->scrollToItem(logList->item(static_cast<int>(applied - 1)), QAbstractItemView::PositionAtCenter);
    }
}

void MainWindow::updateReplayBar() {
    const size_t cur = engine.getActions().size();
    const size_t total = timelineLength();
    timeline->setRange(0, static_cast<int>(total));
    timeline->setValue(static_cast<int>(cur));
    timelineLabel->setText(engine.canRedo() ? QStringLiteral(" %1 / %2 手（再生中）").arg(cur).arg(total)
                                            : QStringLiteral(" %1 / %2 手").arg(cur).arg(total));
    undoAct->setEnabled(engine.canUndo());
    redoAct->setEnabled(engine.canRedo());
    firstAct->setEnabled(cur > 0);
    prevAct->setEnabled(cur > 0);
    nextAct->setEnabled(engine.canRedo());
    lastAct->setEnabled(engine.canRedo());
}

void MainWindow::showError(const QString& msg) {
    messageLabel->setStyleSheet(QStringLiteral("color:#c62828; font-weight:bold;"));
    messageLabel->setText(QStringLiteral("⚠ ") + msg);
    if (table) table->showToast(QStringLiteral("⚠ ") + msg, true);
    QApplication::beep();
}

void MainWindow::showInfo(const QString& msg) {
    messageLabel->setStyleSheet(QString());
    messageLabel->setText(msg);
}

// ============================================================================ 操作
bool MainWindow::submit(const Action& a) {
    if (engine.canRedo()) {
        ApplyResult v = engine.validate(a);
        if (!v.ok) {
            showError(qs(v.error));
            return false;
        }
        auto ans = QMessageBox::question(this, QStringLiteral("履歴の分岐"),
            QStringLiteral("いまは過去の時点を表示しています。\nここで新しく入力すると、この先の %1 手は破棄されます。続けますか？")
                .arg(engine.getRedoActions().size()));
        if (ans != QMessageBox::Yes) return false;
    }
    ApplyResult r = engine.apply(a);
    if (!r.ok) {
        showError(qs(r.error));
        return false;
    }
    dirty = true;
    // 最新の出来事をステータスバーに
    QStringList ev;
    for (const auto& e : engine.getEventsOfAction(engine.getActions().size() - 1))
        if (e.type != ActionType::PHASE_CHANGE) ev << qs(GameLog::describeEvent(e, engine.getState()));
    showInfo(ev.join(QStringLiteral(" / ")));
    if (a.kind == ActionKind::ROLL || a.kind == ActionKind::END_TURN) {
        // 次の入力に備えて出目欄を戻す
        die1->setValue(1);
        die2->setValue(1);
    }
    autosave();
    refreshAll();
    return true;
}

void MainWindow::correctRoll(int d1, int d2) {
    const auto& acts = engine.getActions();
    if (acts.empty() || (acts.back().kind != ActionKind::ROLL && acts.back().kind != ActionKind::UTILITY_ROLL)) return;
    Action a = acts.back();
    if (a.d1 == d1 && a.d2 == d2) return;
    a.d1 = d1;
    a.d2 = d2;
    ApplyResult r = engine.replaceLast(a);
    if (!r.ok) {
        showError(qs(r.error));
        return;
    }
    dirty = true;
    QStringList ev;
    for (const auto& e : engine.getEventsOfAction(engine.getActions().size() - 1))
        if (e.type != ActionType::PHASE_CHANGE) ev << qs(GameLog::describeEvent(e, engine.getState()));
    showInfo(QStringLiteral("出目を修正: ") + ev.join(QStringLiteral(" / ")));
    autosave();
    refreshAll();
}

void MainWindow::gotoIndex(size_t target) {
    target = std::min(target, timelineLength());
    while (engine.getActions().size() > target && engine.undo()) {}
    while (engine.getActions().size() < target && engine.redo()) {}
    autosave();
    refreshAll();
}

void MainWindow::selectCard(CardId id) {
    selected = id;
    refreshAll();
}

void MainWindow::cardAction(ActionKind kind) {
    if (selected == CardId::NONE) return;
    Action a;
    a.kind = kind;
    a.card = selected;
    a.player = engine.getOwnerOfCard(selected);
    submit(a);
}

void MainWindow::demoHover(CardId c) { table->demoHover(c); }

void MainWindow::setTextScaleForRun(double s) {
    textScale = std::clamp(s, 0.6, 2.5);
    QFont f = baseFont;
    f.setPointSizeF(baseFont.pointSizeF() * textScale);
    QApplication::setFont(f);
    table->setTextScale(textScale);
}

void MainWindow::demoTray() {
    table->demoTrade();
}

// ============================================================================ ファイル
QString MainWindow::autosavePath() const {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/autosave.log");
}

QString MainWindow::recordDir() const {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/MonopolyRecorder");
    QDir().mkpath(dir);
    return dir;
}

QString MainWindow::datedRecordPath() const {
    return recordDir() + QStringLiteral("/monopoly_%1.log").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
}

void MainWindow::autosave() {
    if (!autosaveEnabled) return;
    const QByteArray text = QByteArray::fromStdString(GameLog::serialize(engine, true));
    QFile f(autosavePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(text);
    // 記録ファイルにも毎回書く（ファイル名の指定がなければ開始日時の名前）
    if (currentFile.isEmpty()) currentFile = datedRecordPath();
    QFile rec(currentFile);
    if (rec.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        rec.write(text);
        dirty = false;
    }
    QSettings().setValue(QStringLiteral("currentFile"), currentFile);
}

bool MainWindow::writeLog(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("保存エラー"), QStringLiteral("書き込めません:\n%1").arg(path));
        return false;
    }
    f.write(QByteArray::fromStdString(GameLog::serialize(engine, true)));
    currentFile = path;
    dirty = false;
    showInfo(QStringLiteral("保存しました: %1").arg(path));
    refreshAll();
    return true;
}

bool MainWindow::saveLog() {
    if (currentFile.isEmpty()) currentFile = datedRecordPath();  // 名前の指定がなければ日時
    return writeLog(currentFile);
}

bool MainWindow::saveLogAs() {
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("ログを保存"),
                                                currentFile.isEmpty() ? datedRecordPath() : currentFile,
                                                QStringLiteral("Monopoly ログ (*.log);;すべてのファイル (*)"));
    if (path.isEmpty()) return false;
    return writeLog(path);
}

bool MainWindow::maybeSave() {
    if (!dirty) return true;
    auto ans = QMessageBox::question(this, QStringLiteral("未保存の変更"),
                                     QStringLiteral("記録が保存されていません。保存しますか？\n（自動保存は常に行われています）"),
                                     QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (ans == QMessageBox::Cancel) return false;
    if (ans == QMessageBox::Save) return saveLog();
    return true;
}

void MainWindow::newGame() {
    if (!maybeSave()) return;
    NewGameDialog d(this);
    if (d.exec() != QDialog::Accepted) return;
    engine.newGame(d.playerNames(), d.settings());
    table->clearTrade();
    currentFile = datedRecordPath();  // 新しいゲームは開始日時の名前で記録
    dirty = false;
    selected = CardId::NONE;
    rebuildReadyButtons();
    autosave();
    showInfo(QStringLiteral("新しいゲームを開始しました。購入フェーズ: 手番の人はカードを選んで購入、またはパス。"));
    refreshAll();
}

void MainWindow::openLog() {
    if (!maybeSave()) return;
    QString dir = currentFile.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                                        : QFileInfo(currentFile).absolutePath();
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("ログを開く"), dir,
                                                QStringLiteral("Monopoly ログ (*.log);;すべてのファイル (*)"));
    if (path.isEmpty()) return;
    loadFile(path);
}

bool MainWindow::loadFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("読込エラー"), QStringLiteral("開けません:\n%1").arg(path));
        return false;
    }
    GameLog::LoadResult r = GameLog::deserialize(engine, f.readAll().toStdString());
    table->clearTrade();
    currentFile = path;
    dirty = false;
    selected = CardId::NONE;
    rebuildReadyButtons();
    if (r.ok) {
        showInfo(QStringLiteral("読み込みました（%1 手）。上のバーで再生できます。").arg(r.appliedActions));
    } else {
        QMessageBox::warning(this, QStringLiteral("読込エラー"),
                             QStringLiteral("%1 行目: %2\n\n%3 手目までは読み込めています。")
                                 .arg(r.errorLine).arg(qs(r.error)).arg(r.appliedActions));
    }
    autosave();
    refreshAll();
    return r.ok;
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (maybeSave()) e->accept();
    else e->ignore();
}
