#pragma once
#include "MonopolyEngine.hpp"
#include <QFont>
#include <QMainWindow>
#include <QString>
#include <vector>

class QAction;
class QDockWidget;
class QGridLayout;
class QGroupBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSlider;
class QSpinBox;
class TableWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr, bool restoreAutosave = true);

    bool loadFile(const QString& path);
    void gotoAction(size_t index) { gotoIndex(index); }
    void selectCardPublic(CardId id) { selectCard(id); }
    void setAutosaveEnabled(bool on) { autosaveEnabled = on; }
    void demoTray();
    void demoHover(CardId c);
    void setTextScaleForRun(double s);  // 設定を保存せずに文字サイズを変える（画面確認用）  // 画面確認用: 取引欄にサンプルを入れる

protected:
    void closeEvent(QCloseEvent* e) override;

private:
    // ---- 構築
    void buildMenus();
    void buildReplayBar();
    QWidget* buildDetailPanel();  // 予備の操作パネル（表示 → 詳細パネル）
    void rebuildReadyButtons();

    // ---- 更新
    void refreshAll();
    void updateDetailPanel();
    void updateLog();
    void updateReplayBar();
    void showError(const QString& msg);
    void showInfo(const QString& msg);
    void applyTextScale(double s);

    // ---- 操作
    bool submit(const Action& a);
    void correctRoll(int d1, int d2);  // 直前の移動の出目を差し替え
    void gotoIndex(size_t target);
    size_t timelineLength() const { return engine.getActions().size() + engine.getRedoActions().size(); }
    void selectCard(CardId id);
    void cardAction(ActionKind kind);

    // ---- ファイル
    void newGame();
    void openLog();
    bool saveLog();
    bool saveLogAs();
    bool writeLog(const QString& path);
    bool maybeSave();
    void autosave();
    QString autosavePath() const;
    QString recordDir() const;
    QString datedRecordPath() const;

    MonopolyEngine engine;
    QString currentFile;
    bool dirty = false;
    bool updating = false;
    bool autosaveEnabled = true;
    double textScale = 1.0;
    QFont baseFont;
    CardId selected = CardId::NONE;

    TableWidget* table = nullptr;
    QListWidget* logList = nullptr;
    QLabel* messageLabel = nullptr;
    QDockWidget* logDock = nullptr;
    QDockWidget* detailDock = nullptr;

    // 詳細パネル
    QGroupBox* purchaseBox = nullptr;
    QGroupBox* negotiationBox = nullptr;
    QGroupBox* operationBox = nullptr;
    QLabel* selectedInfo = nullptr;
    QLabel* purchaseTurnLabel = nullptr;
    QPushButton* buySelectedBtn = nullptr;
    QGridLayout* readyGrid = nullptr;
    std::vector<QPushButton*> readyButtons;
    QLabel* operationTurnLabel = nullptr;
    QSpinBox* die1 = nullptr;
    QSpinBox* die2 = nullptr;
    QPushButton* rollBtn = nullptr;
    QWidget* pendingRow = nullptr;
    QLabel* pendingLabel = nullptr;
    QPushButton* buyPendingBtn = nullptr;
    QPushButton* endTurnBtn = nullptr;
    QPushButton* buildBtn = nullptr;
    QPushButton* sellBtn = nullptr;
    QPushButton* mortgageBtn = nullptr;
    QPushButton* unmortgageBtn = nullptr;

    // リプレイ
    QAction* undoAct = nullptr;
    QAction* redoAct = nullptr;
    QAction* firstAct = nullptr;
    QAction* prevAct = nullptr;
    QAction* nextAct = nullptr;
    QAction* lastAct = nullptr;
    QSlider* timeline = nullptr;
    QLabel* timelineLabel = nullptr;
    QSpinBox* turnSpin = nullptr;
};
