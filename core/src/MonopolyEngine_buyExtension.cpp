#include "MonopolyEngine.hpp"
#include "TradeOffer.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>

// 運用フェーズ（OPERATIONAL）でも未所有の土地を購入可能にするメソッドを追加
bool MonopolyEngine::buyCard(int playerId, CardId cardId) {
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) return false;
    if (playerId != state.activePlayerIndex) return false;
    if (getOwnerOfCard(cardId) != -1) return false;

    Player& p = state.players[playerId];
    const Card& card = cardMaster.at(cardId);

    if (p.money < card.price) return false;

    historyStack.push_back(state);

    p.money -= card.price;
    p.ownedCards.push_back(cardId);

    GameEvent event;
    event.eventId = static_cast<int>(eventLogs.size()) + 1;
    event.turn = state.turnNumber;
    event.playerId = playerId;
    event.type = ActionType::PURCHASE;
    event.moneyChange = -card.price;
    event.cardsGained.push_back(cardId);
    event.playerMoneyAfter = p.money;
    eventLogs.push_back(event);

    return true;
}
