#include "MonopolyEngine.hpp"

MonopolyEngine::MonopolyEngine() {
    cardMaster = DataMaster::createCardMaster();
}

void MonopolyEngine::initGame(const std::vector<std::string>& playerNames, bool enableTax) {
    state.players.clear();
    state.enableTax = enableTax;
    state.currentPhase = Phase::PURCHASE;
    state.activePlayerIndex = 0;
    state.turnNumber = 1;

    for (int i = 0; i < static_cast<int>(playerNames.size()); ++i) {
        Player p;
        p.id = i;
        p.name = playerNames[i];
        p.money = 1500;
        state.players.push_back(p);
    }
    historyStack.clear();
    eventLogs.clear();
    historyStack.push_back(state);
}

bool MonopolyEngine::buyCardInPurchasePhase(int playerId, CardId cardId) {
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) return false;
    
    Player& p = state.players[playerId];
    const Card& card = cardMaster.at(cardId);

    if (p.money < card.price) return false;

    for (CardId ownedId : p.ownedCards) {
        if (cardMaster.at(ownedId).group == card.group) {
            return false;
        }
    }

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

bool MonopolyEngine::undo() {
    if (historyStack.size() <= 1) return false;
    historyStack.pop_back();
    state = historyStack.back();
    if (!eventLogs.empty()) {
        eventLogs.pop_back();
    }
    return true;
}
