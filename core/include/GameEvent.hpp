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
    TRADE_EXECUTE,
    TRADE_CANCEL,
    BUILD_HOUSE,
    SELL_HOUSE,
    BANKRUPT,
    OVERRIDE
};

struct GameEvent {
    int eventId = 0;
    int turn = 0;
    int playerId = 0;
    ActionType type = ActionType::PASS;
    int targetPlayerId = -1;
    
    int dice1 = 0;
    int dice2 = 0;

    int moneyChange = 0;
    int offerMoney = 0;
    int requestMoney = 0;

    std::vector<CardId> cardsGained;
    std::vector<CardId> cardsLost;

    int playerMoneyAfter = 0;
};

#endif
