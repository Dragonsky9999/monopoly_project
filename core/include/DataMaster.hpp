#ifndef DATA_MASTER_HPP
#define DATA_MASTER_HPP

#include "Card.hpp"
#include <array>
#include <string>
#include <vector>

// カード・盤面の静的マスタと、ID <-> 文字列の変換をここに集約する
namespace DataMaster {

enum class SquareType { GO, CARD, TAX, BLANK };

struct Square {
    SquareType type = SquareType::BLANK;
    CardId card = CardId::NONE;  // type == CARD のとき
    int tax = 0;                 // type == TAX のとき
    std::string label;           // 表示用 (例: "GO", "P-BR1", "TAX-1", "BLANK")
};

constexpr int BOARD_SIZE = 40;

const std::vector<Card>& cards();                    // cardIndex() で引ける
const Card& card(CardId id);
const std::array<Square, BOARD_SIZE>& board();
const std::vector<CardId>& groupCards(GroupType g);

std::string toString(CardId id);                     // "P-BR1" / NONE は "-"
CardId fromString(const std::string& s);             // 大文字小文字・'_'/'-' を区別しない。不正は NONE
std::string groupName(GroupType g);

}  // namespace DataMaster

#endif
