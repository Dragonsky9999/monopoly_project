#include "DataMaster.hpp"
#include <algorithm>
#include <cctype>
#include <map>
#include <stdexcept>

namespace DataMaster {

namespace {

std::vector<Card> buildCards() {
    std::vector<Card> v(CARD_COUNT);
    auto add = [&](CardId id, const char* code, GroupType g, CardKind k, int pos, int price, int house,
                   int mortgage, std::vector<int> rent) {
        Card c;
        c.id = id;
        c.code = code;
        c.group = g;
        c.kind = k;
        c.position = pos;
        c.price = price;
        c.housePrice = house;
        c.mortgage = mortgage;
        c.rent = std::move(rent);
        v[cardIndex(id)] = c;
    };
    using G = GroupType;
    using K = CardKind;
    //   ID              code     group         kind        pos price house mortgage  地代,家1,家2,家3,家4,ホテル
    add(CardId::P_BR1, "P-BR1", G::BROWN,     K::PROPERTY,  1,  60,  50,  30, {2, 10, 30, 90, 160, 250});
    add(CardId::P_BR2, "P-BR2", G::BROWN,     K::PROPERTY,  3,  60,  50,  30, {4, 20, 60, 180, 320, 450});
    add(CardId::P_SB1, "P-SB1", G::SKY_BLUE,  K::PROPERTY,  6, 100,  50,  50, {6, 30, 90, 270, 400, 550});
    add(CardId::P_SB2, "P-SB2", G::SKY_BLUE,  K::PROPERTY,  8, 100,  50,  50, {6, 30, 90, 270, 400, 550});
    add(CardId::P_SB3, "P-SB3", G::SKY_BLUE,  K::PROPERTY,  9, 120,  50,  60, {8, 40, 100, 300, 450, 600});
    add(CardId::P_PI1, "P-PI1", G::PINK,      K::PROPERTY, 11, 140, 100,  70, {10, 50, 150, 450, 625, 750});
    add(CardId::P_PI2, "P-PI2", G::PINK,      K::PROPERTY, 13, 140, 100,  70, {10, 50, 150, 450, 625, 750});
    add(CardId::P_PI3, "P-PI3", G::PINK,      K::PROPERTY, 14, 160, 100,  80, {12, 60, 180, 500, 700, 900});
    add(CardId::P_OR1, "P-OR1", G::ORANGE,    K::PROPERTY, 16, 180, 100,  90, {14, 70, 200, 550, 750, 950});
    add(CardId::P_OR2, "P-OR2", G::ORANGE,    K::PROPERTY, 18, 180, 100,  90, {14, 70, 200, 550, 750, 950});
    add(CardId::P_OR3, "P-OR3", G::ORANGE,    K::PROPERTY, 19, 200, 100, 100, {16, 80, 220, 600, 800, 1000});
    add(CardId::P_RE1, "P-RE1", G::RED,       K::PROPERTY, 21, 220, 150, 110, {18, 90, 250, 700, 875, 1050});
    add(CardId::P_RE2, "P-RE2", G::RED,       K::PROPERTY, 23, 220, 150, 110, {18, 90, 250, 700, 875, 1050});
    add(CardId::P_RE3, "P-RE3", G::RED,       K::PROPERTY, 24, 240, 150, 120, {20, 100, 300, 750, 925, 1100});
    add(CardId::P_YE1, "P-YE1", G::YELLOW,    K::PROPERTY, 26, 260, 150, 130, {22, 110, 330, 800, 975, 1150});
    add(CardId::P_YE2, "P-YE2", G::YELLOW,    K::PROPERTY, 27, 260, 150, 130, {22, 110, 330, 800, 975, 1150});
    add(CardId::P_YE3, "P-YE3", G::YELLOW,    K::PROPERTY, 29, 280, 150, 140, {24, 120, 360, 850, 1025, 1200});
    add(CardId::P_GR1, "P-GR1", G::GREEN,     K::PROPERTY, 31, 300, 200, 150, {26, 130, 390, 900, 1100, 1275});
    add(CardId::P_GR2, "P-GR2", G::GREEN,     K::PROPERTY, 32, 300, 200, 150, {26, 130, 390, 900, 1100, 1275});
    add(CardId::P_GR3, "P-GR3", G::GREEN,     K::PROPERTY, 34, 320, 200, 160, {28, 150, 450, 1000, 1200, 1400});
    add(CardId::P_DB1, "P-DB1", G::DARK_BLUE, K::PROPERTY, 37, 350, 200, 175, {35, 175, 500, 1100, 1300, 1500});
    add(CardId::P_DB2, "P-DB2", G::DARK_BLUE, K::PROPERTY, 39, 400, 200, 200, {50, 200, 600, 1400, 1700, 2000});
    add(CardId::R_1,   "R-1",   G::RAILROAD,  K::RAILROAD,  5, 200,   0, 100, {25, 50, 100, 200});
    add(CardId::R_2,   "R-2",   G::RAILROAD,  K::RAILROAD, 15, 200,   0, 100, {25, 50, 100, 200});
    add(CardId::R_3,   "R-3",   G::RAILROAD,  K::RAILROAD, 25, 200,   0, 100, {25, 50, 100, 200});
    add(CardId::R_4,   "R-4",   G::RAILROAD,  K::RAILROAD, 35, 200,   0, 100, {25, 50, 100, 200});
    add(CardId::U_1,   "U-1",   G::UTILITY,   K::UTILITY,  12, 150,   0,  75, {4, 10});
    add(CardId::U_2,   "U-2",   G::UTILITY,   K::UTILITY,  28, 150,   0,  75, {4, 10});
    return v;
}

std::array<Square, BOARD_SIZE> buildBoard(const std::vector<Card>& cs) {
    std::array<Square, BOARD_SIZE> b;
    for (int i = 0; i < BOARD_SIZE; ++i) {
        b[i].type = SquareType::BLANK;
        b[i].label = "BLANK";
    }
    b[0].type = SquareType::GO;
    b[0].label = "GO";
    b[4].type = SquareType::TAX;
    b[4].tax = 200;
    b[4].label = "TAX-1";
    b[38].type = SquareType::TAX;
    b[38].tax = 100;
    b[38].label = "TAX-2";
    for (const auto& c : cs) {
        b[c.position].type = SquareType::CARD;
        b[c.position].card = c.id;
        b[c.position].label = c.code;
    }
    return b;
}

std::string normalize(const std::string& s) {
    std::string r;
    for (char ch : s) {
        char c = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        if (c == '_') c = '-';
        r += c;
    }
    return r;
}

}  // namespace

const std::vector<Card>& cards() {
    static const std::vector<Card> v = buildCards();
    return v;
}

const Card& card(CardId id) {
    int i = cardIndex(id);
    if (i < 0 || i >= CARD_COUNT) throw std::out_of_range("invalid CardId");
    return cards()[i];
}

const std::array<Square, BOARD_SIZE>& board() {
    static const std::array<Square, BOARD_SIZE> b = buildBoard(cards());
    return b;
}

const std::vector<CardId>& groupCards(GroupType g) {
    static const std::map<GroupType, std::vector<CardId>> m = [] {
        std::map<GroupType, std::vector<CardId>> r;
        for (const auto& c : cards()) r[c.group].push_back(c.id);
        return r;
    }();
    return m.at(g);
}

std::string toString(CardId id) {
    int i = cardIndex(id);
    if (i < 0 || i >= CARD_COUNT) return "-";
    return cards()[i].code;
}

CardId fromString(const std::string& s) {
    std::string n = normalize(s);
    for (const auto& c : cards())
        if (c.code == n) return c.id;
    return CardId::NONE;
}

std::string groupName(GroupType g) {
    switch (g) {
        case GroupType::BROWN: return "BROWN";
        case GroupType::SKY_BLUE: return "SKY_BLUE";
        case GroupType::PINK: return "PINK";
        case GroupType::ORANGE: return "ORANGE";
        case GroupType::RED: return "RED";
        case GroupType::YELLOW: return "YELLOW";
        case GroupType::GREEN: return "GREEN";
        case GroupType::DARK_BLUE: return "DARK_BLUE";
        case GroupType::RAILROAD: return "RAILROAD";
        case GroupType::UTILITY: return "UTILITY";
    }
    return "?";
}

}  // namespace DataMaster
