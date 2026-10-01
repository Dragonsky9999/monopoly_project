#ifndef MONOPOLY_ENGINE_HPP
#define MONOPOLY_ENGINE_HPP

#include "GameState.hpp"
#include "GameEvent.hpp"
#include "TradeOffer.hpp"
#include "Card.hpp"
#include <unordered_map>
#include <vector>
#include <string>

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

    std::vector<bool> purchasePassFlags;
    std::vector<bool> negotiationReadyFlags;

    CardId getCardIdAtPosition(int position) const;
    int getOwnerOfCard(CardId cardId) const;
    void checkPurchasePhaseEnd();

public:
    MonopolyEngine();
    void initGame(const std::vector<std::string>& playerNames, bool enableTax = true);

    const GameState& getState() const { return state; }
    const Card& getCard(CardId id) const { return cardMaster.at(id); }
    const std::vector<GameEvent>& getLogs() const { return eventLogs; }

    bool buyCardInPurchasePhase(int playerId, CardId cardId);
    bool passInPurchasePhase(int playerId);

    bool setNegotiationReady(int playerId, bool ready);
    void forceAdvancePhase();

    bool rollAndMove(int playerId, int dice1, int dice2);
    void passTurn();

    bool isGroupCompleted(int playerId, GroupType group) const;
    bool buildHouse(int playerId, CardId cardId);
    bool sellHouse(int playerId, CardId cardId);

    bool validateTrade(const TradeOffer& offer) const;
    bool executeTrade(const TradeOffer& offer);
    bool cancelTrade(const TradeOffer& offer);

    int calculateTotalLiquidableAssets(int playerId) const;
    bool declareBankrupt(int playerId, int creditorId = -1);

    bool forceSetMoney(int playerId, int newAmount);
    bool forceSetCardOwner(CardId cardId, int newOwnerId);

    double calculateExpectedIncomePerTurn(int playerId) const;
    double evaluateTradeFairness(const TradeOffer& offer) const;

    bool undo();

    bool exportLog(const std::string& filename) const;
    bool importAndReplayLog(const std::string& filename);
};

#endif
