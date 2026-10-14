#pragma once
// 新規ゲーム・取引・オークション・破産・管理者修正のダイアログ
#include "Action.hpp"
#include "GameState.hpp"
#include "MonopolyEngine.hpp"
#include <QDialog>
#include <QVector>
#include <string>
#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QStackedWidget;

// ---------------------------------------------------------------- 新規ゲーム
class NewGameDialog : public QDialog {
    Q_OBJECT
public:
    explicit NewGameDialog(QWidget* parent = nullptr);
    std::vector<std::string> playerNames() const;
    Settings settings() const;

private:
    void updateNameRows();
    void renumber();
    QSpinBox* countSpin;
    QListWidget* nameList;   // ドラッグで並べ替え、ダブルクリックで名前を編集
    QSpinBox* moneySpin;
    QSpinBox* goSpin;
    QCheckBox* taxCheck;
    QCheckBox* doubleRentCheck;
    QCheckBox* doublesCheck;
    QCheckBox* endOnBankruptCheck;
    QSpinBox* interestSpin;
};

// ---------------------------------------------------------------- 取引
class TradeDialog : public QDialog {
    Q_OBJECT
public:
    TradeDialog(const MonopolyEngine& engine, QWidget* parent = nullptr);
    Action action() const;           // TRADE or TRADE_CANCEL
    bool isCancelRecord() const { return cancelRecord; }

private:
    void rebuildLists();
    void updateEvaluation();
    TradeOffer currentOffer() const;

    const MonopolyEngine& engine;
    QComboBox* proposerCombo;
    QComboBox* targetCombo;
    QListWidget* proposerCards;
    QListWidget* targetCards;
    QSpinBox* proposerMoney;
    QSpinBox* targetMoney;
    QLabel* evalLabel;
    QPushButton* tradeButton = nullptr;
    bool cancelRecord = false;
};

// ---------------------------------------------------------------- 小さな入力ダイアログ
namespace SmallDialogs {
// オークション結果。winner = -1 で落札者なし
bool askAuction(QWidget* parent, const GameState& s, int& winner, int& price);
// 破産。creditor = -1 で銀行
bool askBankrupt(QWidget* parent, const GameState& s, int& player, int& creditor);
// 管理者修正 → SET_MONEY / SET_POS / SET_OWNER / SET_HOUSES のどれか
bool askOverride(QWidget* parent, const GameState& s, CardId selected, Action& out);
// 出目の修正（直前の移動をやり直す）
bool askDice(QWidget* parent, int& d1, int& d2);
}  // namespace SmallDialogs
