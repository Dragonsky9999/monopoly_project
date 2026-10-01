#ifndef CARD_HPP
#define CARD_HPP

#include <string>
#include <vector>

enum class CardId {
    NONE,
    P_BR1, P_BR2,
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

enum class GroupType {
    BROWN, SKY_BLUE, PINK, ORANGE, RED, YELLOW, GREEN, DARK_BLUE,
    RAILROAD_GROUP, UTILITY_GROUP
};

struct Card {
    CardId id;
    std::string cliName;    // 例: "P-BR1", "R-1"
    std::string guiDisplay; // 例: "1"
    GroupType group;
    int boardPosition;      // 0 ~ 39
    int price;              // 購入価格 (万円)
    int housePrice;         // 家建設費用 (万円)
    std::vector<int> rent;  // 地代・家建て時等の家賃リスト
};

#endif
