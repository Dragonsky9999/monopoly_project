#include "DataMaster.hpp"

std::unordered_map<CardId, Card> DataMaster::createCardMaster() {
    std::unordered_map<CardId, Card> m;

    auto addCard = [&](CardId id, const std::string& name, const std::string& cliName, GroupType g, int pos, int price, int housePrice, const std::vector<int>& rent) {
        Card c;
        c.id = id;
        c.name = name;
        c.cliName = cliName;
        c.group = g;
        c.boardPosition = pos;
        c.price = price;
        c.housePrice = housePrice;
        c.houseCount = 0;
        c.rent = rent;
        m[id] = c;
    };

    // Brown
    addCard(CardId::P_BR1, "Mediterranean Avenue", "P-BR1", GroupType::BROWN, 1, 60, 50, {2, 10, 30, 90, 160, 250});
    addCard(CardId::P_BR2, "Baltic Avenue", "P-BR2", GroupType::BROWN, 3, 60, 50, {4, 20, 60, 180, 320, 450});

    // Light Blue
    addCard(CardId::P_LB1, "Oriental Avenue", "P-LB1", GroupType::LIGHT_BLUE, 6, 100, 50, {6, 30, 90, 270, 400, 550});
    addCard(CardId::P_LB2, "Vermont Avenue", "P-LB2", GroupType::LIGHT_BLUE, 8, 100, 50, {6, 30, 90, 270, 400, 550});
    addCard(CardId::P_LB3, "Connecticut Avenue", "P-LB3", GroupType::LIGHT_BLUE, 9, 120, 50, {8, 40, 100, 300, 450, 600});

    // Pink
    addCard(CardId::P_PK1, "St. Charles Place", "P-PK1", GroupType::PINK, 11, 140, 100, {10, 50, 150, 450, 625, 750});
    addCard(CardId::P_PK2, "States Avenue", "P-PK2", GroupType::PINK, 13, 140, 100, {10, 50, 150, 450, 625, 750});
    addCard(CardId::P_PK3, "Virginia Avenue", "P-PK3", GroupType::PINK, 14, 160, 100, {12, 60, 180, 500, 700, 900});

    // Orange
    addCard(CardId::P_OR1, "St. James Place", "P-OR1", GroupType::ORANGE, 16, 180, 100, {14, 70, 200, 550, 750, 950});
    addCard(CardId::P_OR2, "Tennessee Avenue", "P-OR2", GroupType::ORANGE, 18, 180, 100, {14, 70, 200, 550, 750, 950});
    addCard(CardId::P_OR3, "New York Avenue", "P-OR3", GroupType::ORANGE, 19, 200, 100, {16, 80, 220, 600, 800, 1000});

    // Red
    addCard(CardId::P_RD1, "Kentucky Avenue", "P-RD1", GroupType::RED, 21, 220, 150, {18, 90, 250, 700, 875, 1050});
    addCard(CardId::P_RD2, "Indiana Avenue", "P-RD2", GroupType::RED, 23, 220, 150, {18, 90, 250, 700, 875, 1050});
    addCard(CardId::P_RD3, "Illinois Avenue", "P-RD3", GroupType::RED, 24, 240, 150, {20, 100, 300, 750, 925, 1100});

    // Yellow
    addCard(CardId::P_YL1, "Atlantic Avenue", "P-YL1", GroupType::YELLOW, 26, 260, 150, {22, 110, 330, 800, 975, 1150});
    addCard(CardId::P_YL2, "Ventnor Avenue", "P-YL2", GroupType::YELLOW, 27, 260, 150, {22, 110, 330, 800, 975, 1150});
    addCard(CardId::P_YL3, "Marvin Gardens", "P-YL3", GroupType::YELLOW, 29, 280, 150, {24, 120, 360, 850, 1025, 1200});

    // Green
    addCard(CardId::P_GR1, "Pacific Avenue", "P-GR1", GroupType::GREEN, 31, 300, 200, {26, 130, 390, 900, 1100, 1275});
    addCard(CardId::P_GR2, "North Carolina Avenue", "P-GR2", GroupType::GREEN, 32, 300, 200, {26, 130, 390, 900, 1100, 1275});
    addCard(CardId::P_GR3, "Pennsylvania Avenue", "P-GR3", GroupType::GREEN, 34, 320, 200, {28, 150, 450, 1000, 1200, 1400});

    // Dark Blue
    addCard(CardId::P_DB1, "Park Place", "P-DB1", GroupType::DARK_BLUE, 37, 350, 200, {35, 175, 500, 1100, 1300, 1500});
    addCard(CardId::P_DB2, "Boardwalk", "P-DB2", GroupType::DARK_BLUE, 39, 400, 200, {50, 200, 600, 1400, 1700, 2000});

    // Railroads
    addCard(CardId::R_READING, "Reading Railroad", "R-READING", GroupType::RAILROAD_GROUP, 5, 200, 0, {25, 50, 100, 200});
    addCard(CardId::R_PENNSYLVANIA, "Pennsylvania Railroad", "R-PENNSYLVANIA", GroupType::RAILROAD_GROUP, 15, 200, 0, {25, 50, 100, 200});
    addCard(CardId::R_B_AND_O, "B. & O. Railroad", "R-B-AND-O", GroupType::RAILROAD_GROUP, 25, 200, 0, {25, 50, 100, 200});
    addCard(CardId::R_SHORT_LINE, "Short Line", "R-SHORT-LINE", GroupType::RAILROAD_GROUP, 35, 200, 0, {25, 50, 100, 200});

    // Utilities
    addCard(CardId::U_ELECTRIC, "Electric Company", "U-ELECTRIC", GroupType::UTILITY_GROUP, 12, 150, 0, {4, 10});
    addCard(CardId::U_WATER, "Water Works", "U-WATER", GroupType::UTILITY_GROUP, 28, 150, 0, {4, 10});

    return m;
}
