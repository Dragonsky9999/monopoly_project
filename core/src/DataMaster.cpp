#include "MonopolyEngine.hpp"

std::unordered_map<CardId, Card> DataMaster::createCardMaster() {
    std::unordered_map<CardId, Card> m;
    
    m[CardId::P_BR1] = {CardId::P_BR1, "P-BR1", "1", GroupType::BROWN, 1, 60, 50, {2, 10, 30, 90, 160, 250}};
    m[CardId::P_BR2] = {CardId::P_BR2, "P-BR2", "2", GroupType::BROWN, 3, 60, 50, {4, 20, 60, 180, 320, 450}};
    m[CardId::P_SB1] = {CardId::P_SB1, "P-SB1", "1", GroupType::SKY_BLUE, 6, 100, 50, {6, 30, 90, 270, 400, 550}};
    
    m[CardId::R_1] = {CardId::R_1, "R-1", "1", GroupType::RAILROAD_GROUP, 5, 200, 0, {25, 50, 100, 200}};
    m[CardId::U_1] = {CardId::U_1, "U-1", "1", GroupType::UTILITY_GROUP, 12, 150, 0, {4, 10}};

    return m;
}
