#pragma once
// 40マスの盤面を QPainter で描く。カード名は描かず「色帯 + 数字」のみ
#include "GameState.hpp"
#include <QWidget>

class BoardWidget : public QWidget {
    Q_OBJECT
public:
    explicit BoardWidget(QWidget* parent = nullptr);

    void setState(const GameState* s);
    void setSelectedCard(CardId id);
    CardId selectedCard() const { return selected; }

    QSize sizeHint() const override { return QSize(760, 760); }
    QSize minimumSizeHint() const override { return QSize(420, 420); }

signals:
    void cardClicked(CardId id);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    bool event(QEvent* e) override;

private:
    enum class Side { Bottom, Left, Top, Right, Corner };
    QRectF boardRect() const;
    QRectF cellRect(int pos) const;
    Side sideOf(int pos) const;
    int cellAt(const QPointF& pt) const;
    void paintCell(QPainter& p, int pos);
    void paintCenter(QPainter& p);
    void paintDie(QPainter& p, const QRectF& r, int value);

    const GameState* state = nullptr;
    CardId selected = CardId::NONE;
    QPointF pressPos;
    CardId pressCard = CardId::NONE;
};
