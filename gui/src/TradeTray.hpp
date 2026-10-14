#pragma once
// ドラッグ&ドロップで組み立てる取引（トレイ）。「成立」で TRADE、「破談として記録」で TRADE_CANCEL を発行
#include "Action.hpp"
#include "MonopolyEngine.hpp"
#include <QGroupBox>
#include <vector>

class QLabel;
class QListWidget;
class QPushButton;

class TradeTray : public QGroupBox {
    Q_OBJECT
public:
    explicit TradeTray(const MonopolyEngine& engine, QWidget* parent = nullptr);

    bool isEmpty() const;
    void clear();
    // from → to にカード / お金を渡す項目を追加。別ペアの取引が組み立て中なら置き換えるか確認する
    bool addCard(CardId c, int from, int to);
    bool addMoney(int from, int to, int amount);
    void refresh();  // 盤面が変わったら呼ぶ（評価の更新）

signals:
    void submitRequested(const Action& a);
    void message(const QString& text, bool error);

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    bool ensurePair(int from, int to);
    Action buildAction(ActionKind kind) const;
    void rebuild();

    const MonopolyEngine& engine;
    int a = -1, b = -1;               // 取引する2人
    std::vector<CardId> aCards, bCards;  // a が渡すカード / b が渡すカード
    int aMoney = 0, bMoney = 0;

    QLabel* hint;
    QLabel* pairLabel;
    QListWidget* list;
    QLabel* evalLabel;
    QPushButton* okBtn;
    QPushButton* cancelRecordBtn;
    QPushButton* clearBtn;
};
