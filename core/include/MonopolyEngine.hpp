#ifndef MONOPOLY_ENGINE_HPP
#define MONOPOLY_ENGINE_HPP

#include "Card.hpp"
#include "DataMaster.hpp"
#include "TradeOffer.hpp"
#include <vector>
#include <string>
#include <unordered_map>

enum class Phase {
    PURCHASE,
    NEGOTIATION,
    OPERATIONAL
};

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

struct Player {
    int id;
    std::string name;
    int money;
    int position;
    bool isBankrupt;
    std::vector<CardId> ownedCards;
};

struct GameState {
    int turnNumber;
    Phase currentPhase;
    int activePlayerIndex;
    bool enableTax;
    std::vector<Player> players;
};

struct GameEvent {
    int eventId;
    int turn;
    int playerId;
    ActionType type;
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

class MonopolyEngine {
public:
    MonopolyEngine();

    void initGame(const std::vector<std::string>& playerNames, bool enableTax = true);

    const GameState& getState() const { return state; }
    const Card& getCard(CardId id) const { return cardMaster.at(id); }
    const std::vector<GameEvent>& getLogs() const { return eventLogs; }

    // パブリックアクセス用ゲッターメソッド
    CardId getCardIdAtPosition(int position) const;
    int getOwnerOfCard(CardId cardId) const;

    // ゲームアクション関数
    bool buyCardInPurchasePhase(int playerId, CardId cardId);
    bool passInPurchasePhase(int playerId);
    bool setNegotiationReady(int playerId, bool ready);
    void forceAdvancePhase();

    bool rollAndMove(int playerId, int dice1, int dice2);
    void passTurn();

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

private:
    void checkPurchasePhaseEnd();
    bool isGroupCompleted(int playerId, GroupType group) const;

    GameState state;
    std::unordered_map<CardId, Card> cardMaster;
    std::vector<bool> purchasePassFlags;
    std::vector<bool> negotiationReadyFlags;

    std::vector<GameState> historyStack;
    std::vector<GameEvent> eventLogs;
};

#endif
