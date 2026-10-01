#ifndef CARD_HPP
#define CARD_HPP

#include <string>
#include <vector>

enum class GroupType {
    BROWN,
    LIGHT_BLUE,
    PINK,
    ORANGE,
    RED,
    YELLOW,
    GREEN,
    DARK_BLUE,
    RAILROAD_GROUP,
    UTILITY_GROUP
};

enum class CardId {
    NONE = -1,
    P_BR1, P_BR2,
    P_LB1, P_LB2, P_LB3,
    P_PK1, P_PK2, P_PK3,
    P_OR1, P_OR2, P_OR3,
    P_RD1, P_RD2, P_RD3,
    P_YL1, P_YL2, P_YL3,
    P_GR1, P_GR2, P_GR3,
    P_DB1, P_DB2,
    R_READING, R_PENNSYLVANIA, R_B_AND_O, R_SHORT_LINE,
    U_ELECTRIC, U_WATER
};

struct Card {
    CardId id;
    std::string name;
    std::string cliName;
    GroupType group;
    int boardPosition;
    int price;
    int housePrice;
    int houseCount;
    std::vector<int> rent;
};

#endif
