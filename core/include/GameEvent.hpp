#ifndef GAME_EVENT_HPP
#define GAME_EVENT_HPP

#include "Card.hpp"
#include <vector>

enum class ActionType {
    PURCHASE,
    PASS,
    ROLL_AND_MOVE,
    PAY_RENT,
    PAY_TAX,
    GO_BONUS,
    TRADE_OFFER,
    TRADE_EXECUTE,
    TRADE_CANCEL,
    BUILD_HOUSE,
    SELL_HOUSE,
    MORTGAGE,
    BANKRUPT,
    OVERRIDE
};

struct GameEvent {
    int eventId;
    int turn;
    int playerId;
    ActionType type;
    
    int targetPlayerId = -1;
    int moneyChange = 0;
    std::vector<CardId> cardsGained;
    std::vector<CardId> cardsLost;

    int playerMoneyAfter;
    int totalAssetAfter;
    double expectedValueDelta = 0.0;
};

#endif
