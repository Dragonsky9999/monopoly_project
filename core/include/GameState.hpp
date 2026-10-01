#ifndef GAME_STATE_HPP
#define GAME_STATE_HPP

#include "Card.hpp"
#include <string>
#include <vector>

enum class Phase { PURCHASE, NEGOTIATION, OPERATIONAL };

struct Player {
    int id;
    std::string name;
    int money = 1500;
    int position = 0;
    bool isBankrupt = false;
    std::vector<CardId> ownedCards;
};

struct GameState {
    Phase currentPhase = Phase::PURCHASE;
    int activePlayerIndex = 0;
    int turnNumber = 1;
    bool enableTax = true;
    std::vector<Player> players;
};

#endif
