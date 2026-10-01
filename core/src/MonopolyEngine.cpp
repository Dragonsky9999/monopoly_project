#include "MonopolyEngine.hpp"
#include "TradeOffer.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cctype>

namespace {
    std::string cardIdToString(CardId id) {
        switch (id) {
            case CardId::P_BR1: return "P-BR1";
            case CardId::P_BR2: return "P-BR2";
            case CardId::P_LB1: return "P-LB1";
            case CardId::P_LB2: return "P-LB2";
            case CardId::P_LB3: return "P-LB3";
            case CardId::P_PK1: return "P-PK1";
            case CardId::P_PK2: return "P-PK2";
            case CardId::P_PK3: return "P-PK3";
            case CardId::P_OR1: return "P-OR1";
            case CardId::P_OR2: return "P-OR2";
            case CardId::P_OR3: return "P-OR3";
            case CardId::P_RD1: return "P-RD1";
            case CardId::P_RD2: return "P-RD2";
            case CardId::P_RD3: return "P-RD3";
            case CardId::P_YL1: return "P-YL1";
            case CardId::P_YL2: return "P-YL2";
            case CardId::P_YL3: return "P-YL3";
            case CardId::P_GR1: return "P-GR1";
            case CardId::P_GR2: return "P-GR2";
            case CardId::P_GR3: return "P-GR3";
            case CardId::P_DB1: return "P-DB1";
            case CardId::P_DB2: return "P-DB2";
            case CardId::R_READING: return "R-READING";
            case CardId::R_PENNSYLVANIA: return "R-PENNSYLVANIA";
            case CardId::R_B_AND_O: return "R-B-AND-O";
            case CardId::R_SHORT_LINE: return "R-SHORT-LINE";
            case CardId::U_ELECTRIC: return "U-ELECTRIC";
            case CardId::U_WATER: return "U-WATER";
            default: return std::to_string(static_cast<int>(id));
        }
    }

    CardId stringToCardId(const std::string& str) {
        if (str == "P-BR1") return CardId::P_BR1;
        if (str == "P-BR2") return CardId::P_BR2;
        if (str == "P-LB1") return CardId::P_LB1;
        if (str == "P-LB2") return CardId::P_LB2;
        if (str == "P-LB3") return CardId::P_LB3;
        if (str == "P-PK1") return CardId::P_PK1;
        if (str == "P-PK2") return CardId::P_PK2;
        if (str == "P-PK3") return CardId::P_PK3;
        if (str == "P-OR1") return CardId::P_OR1;
        if (str == "P-OR2") return CardId::P_OR2;
        if (str == "P-OR3") return CardId::P_OR3;
        if (str == "P-RD1") return CardId::P_RD1;
        if (str == "P-RD2") return CardId::P_RD2;
        if (str == "P-RD3") return CardId::P_RD3;
        if (str == "P-YL1") return CardId::P_YL1;
        if (str == "P-YL2") return CardId::P_YL2;
        if (str == "P-YL3") return CardId::P_YL3;
        if (str == "P-GR1") return CardId::P_GR1;
        if (str == "P-GR2") return CardId::P_GR2;
        if (str == "P-GR3") return CardId::P_GR3;
        if (str == "P-DB1") return CardId::P_DB1;
        if (str == "P-DB2") return CardId::P_DB2;
        if (str == "R-READING") return CardId::R_READING;
        if (str == "R-PENNSYLVANIA") return CardId::R_PENNSYLVANIA;
        if (str == "R-B-AND-O") return CardId::R_B_AND_O;
        if (str == "R-SHORT-LINE") return CardId::R_SHORT_LINE;
        if (str == "U-ELECTRIC") return CardId::U_ELECTRIC;
        if (str == "U-WATER") return CardId::U_WATER;
        try {
            int val = std::stoi(str);
            return static_cast<CardId>(val);
        } catch (...) {
            return CardId::NONE;
        }
    }
}

MonopolyEngine::MonopolyEngine() {
    cardMaster = DataMaster::createCardMaster();
}

CardId MonopolyEngine::getCardIdAtPosition(int position) const {
    for (const auto& [id, card] : cardMaster) {
        if (card.boardPosition == position) {
            return id;
        }
    }
    return CardId::NONE;
}

int MonopolyEngine::getOwnerOfCard(CardId cardId) const {
    for (const auto& player : state.players) {
        if (std::find(player.ownedCards.begin(), player.ownedCards.end(), cardId) != player.ownedCards.end()) {
            return player.id;
        }
    }
    return -1;
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
        p.position = 0;
        p.isBankrupt = false;
        state.players.push_back(p);
    }

    for (auto& [id, card] : cardMaster) {
        card.houseCount = 0;
    }

    purchasePassFlags.assign(playerNames.size(), false);
    negotiationReadyFlags.assign(playerNames.size(), false);

    historyStack.clear();
    eventLogs.clear();
    historyStack.push_back(state);
}

void MonopolyEngine::checkPurchasePhaseEnd() {
    bool allPassed = true;
    for (bool p : purchasePassFlags) {
        if (!p) {
            allPassed = false;
            break;
        }
    }
    if (allPassed) {
        state.currentPhase = Phase::NEGOTIATION;
        state.activePlayerIndex = 0;
    }
}

bool MonopolyEngine::buyCardInPurchasePhase(int playerId, CardId cardId) {
    if (state.currentPhase != Phase::PURCHASE) return false;
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) return false;
    if (playerId != state.activePlayerIndex) return false;
    if (getOwnerOfCard(cardId) != -1) return false;

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

    state.activePlayerIndex = (state.activePlayerIndex + 1) % state.players.size();
    checkPurchasePhaseEnd();

    return true;
}

bool MonopolyEngine::passInPurchasePhase(int playerId) {
    if (state.currentPhase != Phase::PURCHASE) return false;
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) return false;
    if (playerId != state.activePlayerIndex) return false;

    historyStack.push_back(state);

    purchasePassFlags[playerId] = true;

    GameEvent event;
    event.eventId = static_cast<int>(eventLogs.size()) + 1;
    event.turn = state.turnNumber;
    event.playerId = playerId;
    event.type = ActionType::PASS;
    eventLogs.push_back(event);

    state.activePlayerIndex = (state.activePlayerIndex + 1) % state.players.size();
    checkPurchasePhaseEnd();

    return true;
}

bool MonopolyEngine::setNegotiationReady(int playerId, bool ready) {
    if (state.currentPhase != Phase::NEGOTIATION) return false;
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) return false;

    negotiationReadyFlags[playerId] = ready;

    bool allReady = true;
    for (bool r : negotiationReadyFlags) {
        if (!r) {
            allReady = false;
            break;
        }
    }
    if (allReady) {
        state.currentPhase = Phase::OPERATIONAL;
        state.activePlayerIndex = 0;
    } else {
        state.activePlayerIndex = (state.activePlayerIndex + 1) % state.players.size();
    }
    return true;
}

void MonopolyEngine::forceAdvancePhase() {
    historyStack.push_back(state);
    if (state.currentPhase == Phase::PURCHASE) {
        state.currentPhase = Phase::NEGOTIATION;
    } else if (state.currentPhase == Phase::NEGOTIATION) {
        state.currentPhase = Phase::OPERATIONAL;
    }
    state.activePlayerIndex = 0;
}

bool MonopolyEngine::isGroupCompleted(int playerId, GroupType group) const {
    if (group == GroupType::RAILROAD_GROUP || group == GroupType::UTILITY_GROUP) {
        return false;
    }

    int totalInGroup = 0;
    int ownedByPlayer = 0;

    for (const auto& [id, card] : cardMaster) {
        if (card.group == group) {
            totalInGroup++;
            if (getOwnerOfCard(id) == playerId) {
                ownedByPlayer++;
            }
        }
    }
    return (totalInGroup > 0 && totalInGroup == ownedByPlayer);
}

bool MonopolyEngine::buildHouse(int playerId, CardId cardId) {
    if (state.currentPhase != Phase::OPERATIONAL) return false;
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) return false;
    if (getOwnerOfCard(cardId) != playerId) return false;

    Card& targetCard = cardMaster.at(cardId);
    if (!isGroupCompleted(playerId, targetCard.group)) return false;
    if (targetCard.houseCount >= 5) return false;

    Player& p = state.players[playerId];
    if (p.money < targetCard.housePrice) return false;

    for (const auto& [id, card] : cardMaster) {
        if (card.group == targetCard.group) {
            if (card.houseCount < targetCard.houseCount) {
                return false;
            }
        }
    }

    historyStack.push_back(state);

    p.money -= targetCard.housePrice;
    targetCard.houseCount++;

    GameEvent event;
    event.eventId = static_cast<int>(eventLogs.size()) + 1;
    event.turn = state.turnNumber;
    event.playerId = playerId;
    event.type = ActionType::BUILD_HOUSE;
    event.moneyChange = -targetCard.housePrice;
    event.cardsGained.push_back(cardId);
    event.playerMoneyAfter = p.money;
    eventLogs.push_back(event);

    return true;
}

bool MonopolyEngine::sellHouse(int playerId, CardId cardId) {
    if (state.currentPhase != Phase::OPERATIONAL) return false;
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) return false;
    if (getOwnerOfCard(cardId) != playerId) return false;

    Card& targetCard = cardMaster.at(cardId);
    if (targetCard.houseCount <= 0) return false;

    for (const auto& [id, card] : cardMaster) {
        if (card.group == targetCard.group) {
            if (card.houseCount > targetCard.houseCount) {
                return false;
            }
        }
    }

    historyStack.push_back(state);

    int refundAmount = targetCard.housePrice / 2;
    Player& p = state.players[playerId];
    p.money += refundAmount;
    targetCard.houseCount--;

    GameEvent event;
    event.eventId = static_cast<int>(eventLogs.size()) + 1;
    event.turn = state.turnNumber;
    event.playerId = playerId;
    event.type = ActionType::SELL_HOUSE;
    event.moneyChange = refundAmount;
    event.cardsLost.push_back(cardId);
    event.playerMoneyAfter = p.money;
    eventLogs.push_back(event);

    return true;
}

bool MonopolyEngine::validateTrade(const TradeOffer& offer) const {
    if (state.currentPhase == Phase::PURCHASE) return false;

    int pId = offer.proposerId;
    int tId = offer.targetId;

    if (pId < 0 || pId >= static_cast<int>(state.players.size())) return false;
    if (tId < 0 || tId >= static_cast<int>(state.players.size())) return false;
    if (pId == tId) return false;

    const Player& proposer = state.players[pId];
    const Player& target = state.players[tId];

    if (proposer.isBankrupt || target.isBankrupt) return false;

    if (proposer.money < offer.offerMoney) return false;
    if (target.money < offer.requestMoney) return false;

    for (CardId cId : offer.offerCards) {
        if (getOwnerOfCard(cId) != pId) return false;
    }

    for (CardId cId : offer.requestCards) {
        if (getOwnerOfCard(cId) != tId) return false;
    }

    return true;
}

bool MonopolyEngine::executeTrade(const TradeOffer& offer) {
    if (!validateTrade(offer)) return false;

    historyStack.push_back(state);

    Player& proposer = state.players[offer.proposerId];
    Player& target = state.players[offer.targetId];

    proposer.money = proposer.money - offer.offerMoney + offer.requestMoney;
    target.money = target.money - offer.requestMoney + offer.offerMoney;

    for (CardId cId : offer.offerCards) {
        auto it = std::find(proposer.ownedCards.begin(), proposer.ownedCards.end(), cId);
        if (it != proposer.ownedCards.end()) {
            proposer.ownedCards.erase(it);
            target.ownedCards.push_back(cId);
        }
    }

    for (CardId cId : offer.requestCards) {
        auto it = std::find(target.ownedCards.begin(), target.ownedCards.end(), cId);
        if (it != target.ownedCards.end()) {
            target.ownedCards.erase(it);
            proposer.ownedCards.push_back(cId);
        }
    }

    GameEvent event;
    event.eventId = static_cast<int>(eventLogs.size()) + 1;
    event.turn = state.turnNumber;
    event.playerId = offer.proposerId;
    event.targetPlayerId = offer.targetId;
    event.type = ActionType::TRADE_EXECUTE;
    event.offerMoney = offer.offerMoney;
    event.requestMoney = offer.requestMoney;
    event.moneyChange = offer.requestMoney - offer.offerMoney;
    event.cardsGained = offer.requestCards;
    event.cardsLost = offer.offerCards;
    event.playerMoneyAfter = proposer.money;
    eventLogs.push_back(event);

    return true;
}

bool MonopolyEngine::cancelTrade(const TradeOffer& offer) {
    if (offer.proposerId < 0 || offer.proposerId >= static_cast<int>(state.players.size())) return false;
    if (offer.targetId < 0 || offer.targetId >= static_cast<int>(state.players.size())) return false;

    GameEvent event;
    event.eventId = static_cast<int>(eventLogs.size()) + 1;
    event.turn = state.turnNumber;
    event.playerId = offer.proposerId;
    event.targetPlayerId = offer.targetId;
    event.type = ActionType::TRADE_CANCEL;
    event.offerMoney = offer.offerMoney;
    event.requestMoney = offer.requestMoney;
    event.moneyChange = 0;
    event.cardsGained = offer.requestCards;
    event.cardsLost = offer.offerCards;
    event.playerMoneyAfter = state.players[offer.proposerId].money;
    eventLogs.push_back(event);

    return true;
}

int MonopolyEngine::calculateTotalLiquidableAssets(int playerId) const {
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) return 0;
    const Player& p = state.players[playerId];
    if (p.isBankrupt) return 0;

    int total = p.money;

    for (CardId cId : p.ownedCards) {
        const Card& card = cardMaster.at(cId);
        total += (card.houseCount * (card.housePrice / 2));
    }

    return total;
}

bool MonopolyEngine::declareBankrupt(int playerId, int creditorId) {
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) return false;
    Player& p = state.players[playerId];
    if (p.isBankrupt) return false;

    historyStack.push_back(state);

    p.isBankrupt = true;

    for (CardId cId : p.ownedCards) {
        cardMaster.at(cId).houseCount = 0;
    }

    std::vector<CardId> lostCards = p.ownedCards;
    if (creditorId >= 0 && creditorId < static_cast<int>(state.players.size()) && creditorId != playerId) {
        Player& creditor = state.players[creditorId];
        creditor.ownedCards.insert(creditor.ownedCards.end(), lostCards.begin(), lostCards.end());
    }
    p.ownedCards.clear();
    p.money = 0;

    GameEvent event;
    event.eventId = static_cast<int>(eventLogs.size()) + 1;
    event.turn = state.turnNumber;
    event.playerId = playerId;
    event.targetPlayerId = creditorId;
    event.type = ActionType::BANKRUPT;
    event.cardsLost = lostCards;
    event.playerMoneyAfter = 0;
    eventLogs.push_back(event);

    return true;
}

bool MonopolyEngine::forceSetMoney(int playerId, int newAmount) {
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) return false;

    historyStack.push_back(state);

    Player& p = state.players[playerId];
    int oldMoney = p.money;
    p.money = newAmount;

    GameEvent event;
    event.eventId = static_cast<int>(eventLogs.size()) + 1;
    event.turn = state.turnNumber;
    event.playerId = playerId;
    event.type = ActionType::OVERRIDE;
    event.moneyChange = newAmount - oldMoney;
    event.playerMoneyAfter = p.money;
    eventLogs.push_back(event);

    return true;
}

bool MonopolyEngine::forceSetCardOwner(CardId cardId, int newOwnerId) {
    if (newOwnerId < -1 || newOwnerId >= static_cast<int>(state.players.size())) return false;

    historyStack.push_back(state);

    int currentOwner = getOwnerOfCard(cardId);
    if (currentOwner != -1) {
        Player& oldP = state.players[currentOwner];
        auto it = std::find(oldP.ownedCards.begin(), oldP.ownedCards.end(), cardId);
        if (it != oldP.ownedCards.end()) {
            oldP.ownedCards.erase(it);
        }
    }

    if (newOwnerId != -1) {
        state.players[newOwnerId].ownedCards.push_back(cardId);
    }

    GameEvent event;
    event.eventId = static_cast<int>(eventLogs.size()) + 1;
    event.turn = state.turnNumber;
    event.playerId = newOwnerId;
    event.targetPlayerId = currentOwner;
    event.type = ActionType::OVERRIDE;
    if (newOwnerId != -1) event.cardsGained.push_back(cardId);
    if (currentOwner != -1) event.cardsLost.push_back(cardId);
    eventLogs.push_back(event);

    return true;
}

double MonopolyEngine::calculateExpectedIncomePerTurn(int playerId) const {
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) return 0.0;
    const Player& p = state.players[playerId];
    if (p.isBankrupt) return 0.0;

    int activeOpponents = 0;
    for (const auto& other : state.players) {
        if (other.id != playerId && !other.isBankrupt) {
            activeOpponents++;
        }
    }
    if (activeOpponents == 0) return 0.0;

    const double baseLandProb = 1.0 / 40.0;
    double expectedRentIncome = 0.0;

    for (CardId cId : p.ownedCards) {
        const Card& card = cardMaster.at(cId);
        int rentIndex = card.houseCount;
        int rentAmount = (rentIndex < static_cast<int>(card.rent.size())) ? card.rent[rentIndex] : card.rent.back();

        expectedRentIncome += (rentAmount * baseLandProb * activeOpponents);
    }

    double expectedGoBonus = (7.0 / 40.0) * 200.0;

    return expectedRentIncome + expectedGoBonus;
}

double MonopolyEngine::evaluateTradeFairness(const TradeOffer& offer) const {
    if (!validateTrade(offer)) return 0.0;

    double prevExpP = calculateExpectedIncomePerTurn(offer.proposerId);
    double prevExpT = calculateExpectedIncomePerTurn(offer.targetId);

    MonopolyEngine simEngine = *this;
    simEngine.executeTrade(offer);

    double nextExpP = simEngine.calculateExpectedIncomePerTurn(offer.proposerId);
    double nextExpT = simEngine.calculateExpectedIncomePerTurn(offer.targetId);

    double deltaExpP = nextExpP - prevExpP;
    double deltaExpT = nextExpT - prevExpT;

    double netMoneyGainP = offer.requestMoney - offer.offerMoney;

    double fairnessScore = (deltaExpP * 10.0 + netMoneyGainP) - (deltaExpT * 10.0 - netMoneyGainP);

    return fairnessScore;
}

bool MonopolyEngine::rollAndMove(int playerId, int dice1, int dice2) {
    if (state.currentPhase != Phase::OPERATIONAL) return false;
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) return false;
    if (playerId != state.activePlayerIndex) return false;
    if (state.players[playerId].isBankrupt) return false;
    
    if (dice1 < 1 || dice1 > 6 || dice2 < 1 || dice2 > 6) {
        return false;
    }

    historyStack.push_back(state);
    Player& p = state.players[playerId];
    int steps = dice1 + dice2;
    int oldPos = p.position;
    int newPos = (oldPos + steps) % 40;
    p.position = newPos;

    GameEvent rollEvent;
    rollEvent.eventId = static_cast<int>(eventLogs.size()) + 1;
    rollEvent.turn = state.turnNumber;
    rollEvent.playerId = playerId;
    rollEvent.type = ActionType::ROLL_AND_MOVE;
    rollEvent.dice1 = dice1;
    rollEvent.dice2 = dice2;
    rollEvent.playerMoneyAfter = p.money;
    eventLogs.push_back(rollEvent);

    if (newPos < oldPos) {
        p.money += 200;
        
        GameEvent goEvent;
        goEvent.eventId = static_cast<int>(eventLogs.size()) + 1;
        goEvent.turn = state.turnNumber;
        goEvent.playerId = playerId;
        goEvent.type = ActionType::GO_BONUS;
        goEvent.moneyChange = 200;
        goEvent.playerMoneyAfter = p.money;
        eventLogs.push_back(goEvent);
    }

    if (state.enableTax && newPos == 4) {
        p.money -= 200;
    } else if (state.enableTax && newPos == 38) {
        p.money -= 100;
    } else {
        CardId landedCardId = getCardIdAtPosition(newPos);
        if (landedCardId != CardId::NONE) {
            int ownerId = getOwnerOfCard(landedCardId);
            if (ownerId != -1 && ownerId != playerId) {
                const Card& card = cardMaster.at(landedCardId);
                int rentIndex = card.houseCount;
                int rentAmount = (rentIndex < static_cast<int>(card.rent.size())) ? card.rent[rentIndex] : card.rent.back();
                
                p.money -= rentAmount;
                state.players[ownerId].money += rentAmount;

                GameEvent rentEvent;
                rentEvent.eventId = static_cast<int>(eventLogs.size()) + 1;
                rentEvent.turn = state.turnNumber;
                rentEvent.playerId = playerId;
                rentEvent.targetPlayerId = ownerId;
                rentEvent.type = ActionType::PAY_RENT;
                rentEvent.moneyChange = -rentAmount;
                rentEvent.playerMoneyAfter = p.money;
                eventLogs.push_back(rentEvent);
            }
        }
    }
    return true;
}

void MonopolyEngine::passTurn() {
    if (state.currentPhase != Phase::OPERATIONAL) return;
    
    do {
        state.activePlayerIndex = (state.activePlayerIndex + 1) % state.players.size();
        if (state.activePlayerIndex == 0) {
            state.turnNumber++;
        }
    } while (state.players[state.activePlayerIndex].isBankrupt);
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

bool MonopolyEngine::exportLog(const std::string& filename) const {
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs.is_open()) return false;

    ofs << "# MONOPOLY FULL EVENT LOG\n";
    ofs << "[PLAYERS]\n";
    ofs << "playerCount=" << state.players.size() << "\n";
    for (const auto& p : state.players) {
        ofs << "p:" << p.id << "," << p.name << "\n";
    }

    ofs << "\n[EVENTS]\n";
    for (const auto& evt : eventLogs) {
        ofs << evt.eventId << "," << evt.turn << "," << evt.playerId << "," 
            << static_cast<int>(evt.type) << "," << evt.targetPlayerId << ","
            << evt.dice1 << "," << evt.dice2 << ","
            << evt.moneyChange << "," << evt.offerMoney << "," << evt.requestMoney << ",";
        
        if (evt.cardsGained.empty()) {
            ofs << "none";
        } else {
            for (size_t i = 0; i < evt.cardsGained.size(); ++i) {
                ofs << cardIdToString(evt.cardsGained[i]) << (i + 1 < evt.cardsGained.size() ? ";" : "");
            }
        }
        ofs << ",";
        if (evt.cardsLost.empty()) {
            ofs << "none";
        } else {
            for (size_t i = 0; i < evt.cardsLost.size(); ++i) {
                ofs << cardIdToString(evt.cardsLost[i]) << (i + 1 < evt.cardsLost.size() ? ";" : "");
            }
        }
        ofs << "\n";
    }

    ofs.close();
    return true;
}

bool MonopolyEngine::importAndReplayLog(const std::string& filename) {
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs.is_open()) return false;

    std::stringstream buffer;
    buffer << ifs.rdbuf();
    ifs.close();

    std::string content = buffer.str();
    if (content.empty()) return false;

    std::stringstream ssContent(content);
    std::string line;
    std::string currentSection = "";

    std::vector<std::string> loadedPlayerNames;

    auto safeStoi = [](const std::string& str, int defaultVal = 0) -> int {
        if (str.empty() || str == "none") return defaultVal;
        try {
            size_t idx = 0;
            return std::stoi(str, &idx);
        } catch (...) {
            return defaultVal;
        }
    };

    auto parseCardIdList = [](const std::string& str) -> std::vector<CardId> {
        std::vector<CardId> result;
        if (str.empty() || str == "none") return result;
        std::stringstream ss(str);
        std::string token;
        while (std::getline(ss, token, ';')) {
            if (!token.empty()) {
                CardId id = stringToCardId(token);
                if (id != CardId::NONE) {
                    result.push_back(id);
                }
            }
        }
        return result;
    };

    std::vector<std::string> lines;
    while (std::getline(ssContent, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || std::isspace(static_cast<unsigned char>(line.back())))) {
            line.pop_back();
        }
        if (!line.empty() && line[0] != '#') {
            lines.push_back(line);
        }
    }

    for (const auto& l : lines) {
        if (l == "[PLAYERS]") currentSection = "[PLAYERS]";
        else if (l == "[EVENTS]") currentSection = "[EVENTS]";
        else if (currentSection == "[PLAYERS]" && l.rfind("p:", 0) == 0) {
            std::stringstream pSs(l.substr(2));
            std::string idStr, name;
            std::getline(pSs, idStr, ',');
            std::getline(pSs, name, ',');
            if (!name.empty()) loadedPlayerNames.push_back(name);
        }
    }

    if (loadedPlayerNames.empty()) {
        loadedPlayerNames = {"Player_1", "Player_2"};
    }

    initGame(loadedPlayerNames, true);

    currentSection = "";
    for (const auto& l : lines) {
        if (l == "[EVENTS]") {
            currentSection = "[EVENTS]";
            continue;
        }
        if (currentSection == "[EVENTS]") {
            std::stringstream eSs(l);
            std::vector<std::string> tokens;
            std::string token;
            while (std::getline(eSs, token, ',')) {
                tokens.push_back(token);
            }

            if (tokens.size() < 12) continue;

            int pId = safeStoi(tokens[2], -1);
            ActionType type = static_cast<ActionType>(safeStoi(tokens[3], 0));

            if (pId < 0 || pId >= static_cast<int>(state.players.size())) continue;

            std::vector<CardId> gainedCards = parseCardIdList(tokens[10]);
            std::vector<CardId> lostCards = parseCardIdList(tokens[11]);

            if (type == ActionType::PURCHASE && !gainedCards.empty()) {
                buyCardInPurchasePhase(pId, gainedCards[0]);
            } else if (type == ActionType::PASS) {
                if (state.currentPhase == Phase::PURCHASE) {
                    passInPurchasePhase(pId);
                } else if (state.currentPhase == Phase::NEGOTIATION) {
                    setNegotiationReady(pId, true);
                } else if (state.currentPhase == Phase::OPERATIONAL) {
                    passTurn();
                }
            } else if (type == ActionType::BUILD_HOUSE && !gainedCards.empty()) {
                buildHouse(pId, gainedCards[0]);
            } else if (type == ActionType::SELL_HOUSE && !lostCards.empty()) {
                sellHouse(pId, lostCards[0]);
            } else if (type == ActionType::TRADE_EXECUTE) {
                int targetId = safeStoi(tokens[4], -1);
                if (targetId >= 0 && targetId < static_cast<int>(state.players.size())) {
                    TradeOffer offer;
                    offer.proposerId = pId;
                    offer.targetId = targetId;
                    offer.offerMoney = safeStoi(tokens[8], 0);
                    offer.requestMoney = safeStoi(tokens[9], 0);
                    offer.requestCards = gainedCards;
                    offer.offerCards = lostCards;

                    if (validateTrade(offer)) {
                        executeTrade(offer);
                    }
                }
            } else if (type == ActionType::ROLL_AND_MOVE) {
                int d1 = safeStoi(tokens[5], 1);
                int d2 = safeStoi(tokens[6], 1);
                rollAndMove(pId, d1, d2);
            }
        }
    }

    return true;
}
