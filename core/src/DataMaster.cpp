#include "MonopolyEngine.hpp"

std::unordered_map<CardId, Card> DataMaster::createCardMaster() {
    std::unordered_map<CardId, Card> m;
    
    // --- 物件カード (22枚) ---
    // BROWN (茶)
    m[CardId::P_BR1] = {CardId::P_BR1, "P-BR1", "1", GroupType::BROWN, 1, 60, 50, {2, 10, 30, 90, 160, 250}};
    m[CardId::P_BR2] = {CardId::P_BR2, "P-BR2", "2", GroupType::BROWN, 3, 60, 50, {4, 20, 60, 180, 320, 450}};

    // SKY_BLUE (水色)
    m[CardId::P_SB1] = {CardId::P_SB1, "P-SB1", "1", GroupType::SKY_BLUE, 6, 100, 50, {6, 30, 90, 270, 400, 550}};
    m[CardId::P_SB2] = {CardId::P_SB2, "P-SB2", "2", GroupType::SKY_BLUE, 8, 100, 50, {6, 30, 90, 270, 400, 550}};
    m[CardId::P_SB3] = {CardId::P_SB3, "P-SB3", "3", GroupType::SKY_BLUE, 9, 120, 50, {8, 40, 100, 300, 450, 600}};

    // PINK (桃)
    m[CardId::P_PI1] = {CardId::P_PI1, "P-PI1", "1", GroupType::PINK, 11, 140, 100, {10, 50, 150, 450, 625, 750}};
    m[CardId::P_PI2] = {CardId::P_PI2, "P-PI2", "2", GroupType::PINK, 13, 140, 100, {10, 50, 150, 450, 625, 750}};
    m[CardId::P_PI3] = {CardId::P_PI3, "P-PI3", "3", GroupType::PINK, 14, 160, 100, {12, 60, 180, 500, 700, 900}};

    // ORANGE (橙)
    m[CardId::P_OR1] = {CardId::P_OR1, "P-OR1", "1", GroupType::ORANGE, 16, 180, 100, {14, 70, 200, 550, 750, 950}};
    m[CardId::P_OR2] = {CardId::P_OR2, "P-OR2", "2", GroupType::ORANGE, 18, 180, 100, {14, 70, 200, 550, 750, 950}};
    m[CardId::P_OR3] = {CardId::P_OR3, "P-OR3", "3", GroupType::ORANGE, 19, 200, 100, {16, 80, 220, 600, 800, 1000}};

    // RED (赤)
    m[CardId::P_RE1] = {CardId::P_RE1, "P-RE1", "1", GroupType::RED, 21, 220, 150, {18, 90, 250, 700, 875, 1050}};
    m[CardId::P_RE2] = {CardId::P_RE2, "P-RE2", "2", GroupType::RED, 23, 220, 150, {18, 90, 250, 700, 875, 1050}};
    m[CardId::P_RE3] = {CardId::P_RE3, "P-RE3", "3", GroupType::RED, 24, 240, 150, {20, 100, 300, 750, 925, 1100}};

    // YELLOW (黄)
    m[CardId::P_YE1] = {CardId::P_YE1, "P-YE1", "1", GroupType::YELLOW, 26, 260, 150, {22, 110, 330, 800, 975, 1150}};
    m[CardId::P_YE2] = {CardId::P_YE2, "P-YE2", "2", GroupType::YELLOW, 27, 260, 150, {22, 110, 330, 800, 975, 1150}};
    m[CardId::P_YE3] = {CardId::P_YE3, "P-YE3", "3", GroupType::YELLOW, 29, 280, 150, {24, 120, 360, 850, 1025, 1200}};

    // GREEN (緑)
    m[CardId::P_GR1] = {CardId::P_GR1, "P-GR1", "1", GroupType::GREEN, 31, 300, 200, {26, 130, 390, 900, 1100, 1275}};
    m[CardId::P_GR2] = {CardId::P_GR2, "P-GR2", "2", GroupType::GREEN, 32, 300, 200, {26, 130, 390, 900, 1100, 1275}};
    m[CardId::P_GR3] = {CardId::P_GR3, "P-GR3", "3", GroupType::GREEN, 34, 320, 200, {28, 150, 1000, 1200, 1400, 1600}};

    // DARK_BLUE (紺)
    m[CardId::P_DB1] = {CardId::P_DB1, "P-DB1", "1", GroupType::DARK_BLUE, 37, 350, 200, {35, 175, 500, 1100, 1300, 1500}};
    m[CardId::P_DB2] = {CardId::P_DB2, "P-DB2", "2", GroupType::DARK_BLUE, 39, 400, 200, {50, 200, 600, 1400, 1700, 2000}};

    // --- 鉄道カード (4枚) ---
    m[CardId::R_1] = {CardId::R_1, "R-1", "1", GroupType::RAILROAD_GROUP, 5, 200, 0, {25, 50, 100, 200}};
    m[CardId::R_2] = {CardId::R_2, "R-2", "2", GroupType::RAILROAD_GROUP, 15, 200, 0, {25, 50, 100, 200}};
    m[CardId::R_3] = {CardId::R_3, "R-3", "3", GroupType::RAILROAD_GROUP, 25, 200, 0, {25, 50, 100, 200}};
    m[CardId::R_4] = {CardId::R_4, "R-4", "4", GroupType::RAILROAD_GROUP, 35, 200, 0, {25, 50, 100, 200}};

    // --- 水道・電気カード (2枚) ---
    m[CardId::U_1] = {CardId::U_1, "U-1", "1", GroupType::UTILITY_GROUP, 12, 150, 0, {4, 10}};
    m[CardId::U_2] = {CardId::U_2, "U-2", "2", GroupType::UTILITY_GROUP, 28, 150, 0, {4, 10}};

    return m;
}
