#pragma once
// プレイヤー一覧（所持金・資産・所有カードを色チップで表示）
#include "MonopolyEngine.hpp"
#include <QWidget>
#include <utility>
#include <vector>

class PlayersWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlayersWidget(QWidget* parent = nullptr);
    void setEngine(const MonopolyEngine* e);
    void refresh();  // 状態が変わったら呼ぶ
    void setSelectedCard(CardId id);

    QSize sizeHint() const override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return layoutHeight(w); }

signals:
    void cardClicked(CardId id);
    void cardDropped(CardId id, int fromPlayer, int toPlayer);  // fromPlayer = -1 は銀行
    void moneyDropped(int fromPlayer, int toPlayer);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragMoveEvent(QDragMoveEvent* e) override;
    void dragLeaveEvent(QDragLeaveEvent* e) override;
    void dropEvent(QDropEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    bool event(QEvent* e) override;

private:
    int layoutHeight(int width) const;
    CardId chipAt(const QPointF& pt) const;
    int playerRowAt(const QPointF& pt) const;
    int moneyAt(const QPointF& pt) const;

    const MonopolyEngine* engine = nullptr;
    CardId selected = CardId::NONE;
    mutable std::vector<std::pair<QRectF, CardId>> chipRects;
    mutable std::vector<std::pair<QRectF, int>> rowRects;    // プレイヤー行
    mutable std::vector<std::pair<QRectF, int>> moneyRects;  // お金（ドラッグ元）
    QPointF pressPos;
    CardId pressCard = CardId::NONE;
    int pressMoneyPlayer = -1;
    int hoverRow = -1;  // ドロップ先の強調
};
