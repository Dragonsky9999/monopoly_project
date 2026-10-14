#pragma once
// カードの見た目（GUI表示は「色帯 + 数字」のみ。物件名は使わない）
#include "Card.hpp"
#include "GameState.hpp"
#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QString>

namespace CardVisual {

QColor groupColor(GroupType g);
QColor textColorOn(const QColor& bg);         // 背景色に対して読みやすい文字色
QString groupLabel(GroupType g);              // "茶", "水色", ... "鉄道", "公共"
QString cardNumber(CardId id);                // "1"〜"4"
QString cardShortLabel(CardId id);            // "茶1"
QColor playerColor(int playerId);

// 色地に数字のチップ。st があれば家（上部の小さな四角）・抵当（斜線）も描く
void paintChip(QPainter& p, const QRectF& r, CardId id, const CardState* st = nullptr);
QIcon cardIcon(CardId id, const CardState* st = nullptr, int size = 22);
QIcon playerIcon(int playerId, int size = 14);
QString cardTooltip(CardId id, const GameState& s);
QString playerName(const GameState& s, int id);  // -1 → "銀行"

}  // namespace CardVisual

// ドラッグ&ドロップで運ぶデータ（カード / お金）
class QMimeData;
namespace DragData {
QMimeData* makeCard(CardId id, int fromPlayer);  // fromPlayer = -1 は銀行（未所有）
QMimeData* makeMoney(int fromPlayer);
bool readCard(const QMimeData* m, CardId& id, int& fromPlayer);
bool readMoney(const QMimeData* m, int& fromPlayer);
bool hasAny(const QMimeData* m);
QPixmap moneyPixmap(int size = 28);
}  // namespace DragData
