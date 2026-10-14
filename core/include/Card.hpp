#ifndef CARD_HPP
#define CARD_HPP

#include <string>
#include <vector>

// カードのグループ（購入制限・コンプリート判定の単位）
enum class GroupType {
    BROWN,
    SKY_BLUE,
    PINK,
    ORANGE,
    RED,
    YELLOW,
    GREEN,
    DARK_BLUE,
    RAILROAD,
    UTILITY
};

// 家賃計算の方式
enum class CardKind {
    PROPERTY,  // 物件: 家の数で家賃が決まる
    RAILROAD,  // 鉄道: 所有枚数で家賃が決まる
    UTILITY    // 水道・電気: 出目 x 倍率
};

// カードID（値はそのまま配列インデックスとして使う）
enum class CardId : int {
    NONE = -1,
    P_BR1 = 0, P_BR2,
    P_SB1, P_SB2, P_SB3,
    P_PI1, P_PI2, P_PI3,
    P_OR1, P_OR2, P_OR3,
    P_RE1, P_RE2, P_RE3,
    P_YE1, P_YE2, P_YE3,
    P_GR1, P_GR2, P_GR3,
    P_DB1, P_DB2,
    R_1, R_2, R_3, R_4,
    U_1, U_2
};

constexpr int CARD_COUNT = 28;

inline int cardIndex(CardId id) { return static_cast<int>(id); }
inline CardId cardFromIndex(int i) { return static_cast<CardId>(i); }

// カードの不変パラメータ（マスタ）。所有者・家の数などの可変情報は GameState 側に持つ
struct Card {
    CardId id = CardId::NONE;
    std::string code;          // 表示名 = ID文字列 (例: "P-BR1")
    GroupType group = GroupType::BROWN;
    CardKind kind = CardKind::PROPERTY;
    int position = 0;          // 盤面マス番号
    int price = 0;
    int housePrice = 0;
    int mortgage = 0;          // 抵当額
    std::vector<int> rent;     // PROPERTY: [地代,家1..家4,ホテル] / RAILROAD: [1枚..4枚] / UTILITY: [1枚倍率,2枚倍率]
};

#endif
