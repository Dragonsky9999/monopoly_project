#pragma once
// 盤面（40マス）の描画と座標計算。BoardWidget / TableWidget の両方から使う
#include "GameState.hpp"
#include <QPainter>
#include <QRectF>

namespace BoardPainter {

enum class Side { Bottom, Left, Top, Right, Corner };

struct Options {
    CardId selected = CardId::NONE;   // 黄色枠
    int highlightCell = -1;           // ドロップ先などの強調（水色枠）
    int hiddenPawn = -1;              // ドラッグ中の駒は描かない
    bool drawCenter = true;           // 中央のタイトル・フェーズ・サイコロを描くか
};

double unit(const QRectF& board);                   // 辺マスの幅
QRectF cellRect(const QRectF& board, int pos);
QRectF innerRect(const QRectF& board);              // 中央の空き領域
Side sideOf(int pos);
int cellAt(const QRectF& board, const QPointF& pt);  // -1 = なし
QPointF pawnCenter(const QRectF& board, const GameState& s, int playerId, double* radius = nullptr);

void paintBoard(QPainter& p, const QRectF& board, const GameState* s, const Options& opt);
void paintDie(QPainter& p, const QRectF& r, int value);

}  // namespace BoardPainter
