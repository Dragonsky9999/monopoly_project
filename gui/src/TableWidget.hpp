#pragma once
// 盤面中心の「テーブル」画面。
// 盤面の周りに各プレイヤーのエリア（所持金・紙幣・手札）を並べ、
// カード / 紙幣 / 駒をドラッグ&ドロップして記録する。
#include "MonopolyEngine.hpp"
#include <QString>
#include <QWidget>
#include <functional>
#include <vector>

class QTimer;

class TableWidget : public QWidget {
    Q_OBJECT
public:
    explicit TableWidget(const MonopolyEngine& engine, QWidget* parent = nullptr);

    void refresh();     // エンジンの状態が変わったら呼ぶ
    void clearTrade();  // 組み立て中の取引を破棄
    void showToast(const QString& msg, bool error);
    void setTextScale(double s);   // 文字サイズ（1.0 = 標準）。プレイヤーエリア・ホバー表示・通知に効く
    double textScale() const { return userScale; }
    void setSelectedCard(CardId id);
    CardId selectedCard() const { return selected; }

    // 画面確認用（--demo-tray）
    void demoTrade();
    void demoHover(CardId c);  // 画面確認用: そのマスにカーソルがある状態にする

signals:
    void actionRequested(const Action& a);
    void cardSelected(CardId id);
    void rollCorrectionRequested(int d1, int d2);  // 直前の ROLL を別の出目でやり直す

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    QSize sizeHint() const override { return QSize(1200, 900); }
    QSize minimumSizeHint() const override { return QSize(640, 480); }

private:
    enum class HitKind { Cell, HandCard, Bill, MoneyTotal, Pawn, PlayerArea, Button, Die, TradeItem };
    struct Hit {
        QRectF r;
        HitKind kind = HitKind::Cell;
        int player = -1;
        CardId card = CardId::NONE;
        int value = 0;   // Bill: 額面 / Die: 1 or 2 / TradeItem: side
        int count = 0;   // Bill: 枚数
        QString id;      // Button
        bool enabled = true;
    };
    struct Draft {
        int a = -1, b = -1;
        std::vector<CardId> aCards, bCards;
        int aMoney = 0, bMoney = 0;
        bool empty() const { return a < 0; }
    };

    // ---- 座標
    void computeLayout();
    QTransform viewTransform() const;
    QPointF toScene(const QPointF& widgetPt) const;
    int areaAt(const QPointF& sp) const;

    // ---- 描画
    void paintPlayerArea(QPainter& p, int pid);
    void paintPlayerAreaLocal(QPainter& p, int pid);
    void paintCenterUI(QPainter& p);
    void paintTradeZone(QPainter& p, const QRectF& zone);
    void paintPendingPopup(QPainter& p);
    void paintHoverInfo(QPainter& p);
    void paintCardPanel(QPainter& p);
    void updateCardPanel(const QPointF& sp);
    void scrollColumn(double dy);
    double areaScale() const;
    int ux(double v) const;  // ホバー等の文字サイズ（文字サイズ設定を反映）
    void paintDragGhost(QPainter& p);
    void paintToast(QPainter& p);
    void addButton(QPainter& p, const QRectF& r, const QString& text, const QString& id, bool enabled,
                   const QColor& color = QColor(0x35, 0x5C, 0x9A));
    const Hit* hitAt(const QPointF& sp, const std::function<bool(const Hit&)>& filter = {}) const;
    static bool draggable(const Hit& h);

    // ---- 操作
    void onClick(const Hit& h);
    void onButton(const QString& id);
    void handleDrop(const Hit& src, const QPointF& sp, Qt::KeyboardModifiers mods);
    void cardToPlayer(CardId c, int from, int to);
    void moneyToPlayer(int from, int to, int amount);
    void movePawn(int pid, int cell);
    void emitAction(const Action& a);
    bool emitAndCheck(const Action& a);  // 記録されたら true
    void fixDie(int which, int delta);   // 直前の出目を ±1 してその場で移動をやり直す
    bool canFixRoll() const;             // 直前の操作が手番プレイヤーの移動なら true
    int draftOffered(int pid) const;     // 組み立て中の取引でそのプレイヤーが出すお金

    bool draftEnsurePair(int from, int to);
    void draftAddCard(CardId c, int from, int to);
    void draftAddMoney(int from, int to, int amount);
    Action draftAction(ActionKind kind) const;

    const MonopolyEngine& engine;
    QRectF sceneR, boardR, tradeZoneR;
    QRectF centerBoardR;
    QRectF columnR;            // プレイヤー一覧の表示領域（盤面の右）
    double columnScroll = 0;   // プレイヤー一覧のスクロール量
    double columnMaxScroll = 0;
    double userScale = 1.0;
    struct CardPanel {         // マスやカードにカーソルを合わせたときの情報 + 操作
        bool visible = false;
        CardId card = CardId::NONE;
        bool fromHand = false;
        QRectF anchor;         // 対象（マス / 手札）の位置
        QRectF rect;           // パネルの位置（描画時に決まる）
    };
    CardPanel panel;  // 盤面中央UIを描くときの基準盤面（kBoard サイズ）
    std::vector<QRectF> areaR;
    mutable std::vector<Hit> hits;  // 描画のたびに作り直す（後に描いたものが上）
    CardId selected = CardId::NONE;
    int die1 = 1, die2 = 1;
    Draft draft;

    bool pressed = false;
    bool hasPressHit = false;
    Hit pressHit;
    QPointF pressWidget;
    bool dragging = false;
    QPointF dragScene;
    QPointF hoverScene;
    bool hoverValid = false;

    QString toastText;
    bool toastError = false;
    QTimer* toastTimer = nullptr;
};
