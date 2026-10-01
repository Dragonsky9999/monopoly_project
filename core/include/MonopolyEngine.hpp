#ifndef MONOPOLY_ENGINE_HPP
#define MONOPOLY_ENGINE_HPP

#include "GameState.hpp"
#include "GameEvent.hpp"
#include "Card.hpp"
#include <unordered_map>
#include <vector>

class DataMaster {
public:
    static std::unordered_map<CardId, Card> createCardMaster();
};

class MonopolyEngine {
private:
    GameState state;
    std::unordered_map<CardId, Card> cardMaster;
    std::vector<GameState> historyStack;
    std::vector<GameEvent> eventLogs;

public:
    MonopolyEngine();
    void initGame(const std::vector<std::string>& playerNames, bool enableTax = true);

    const GameState& getState() const { return state; }
    const Card& getCard(CardId id) const { return cardMaster.at(id); }
    const std::vector<GameEvent>& getLogs() const { return eventLogs; }

    bool buyCardInPurchasePhase(int playerId, CardId cardId);
    bool undo();
};

#endif
