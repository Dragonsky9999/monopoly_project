#include "MonopolyEngine.hpp"
#include "TradeOffer.hpp"
#include "CliSetup.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <random>
#include <algorithm>
#include <cctype>

std::string toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](char c){
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    });
    return str;
}

int resolvePlayerId(const std::string& input, const std::vector<Player>& players) {
    if (input.empty()) return -1;
    try {
        size_t idx;
        int id = std::stoi(input, &idx);
        if (idx == input.length() && id >= 0 && id < static_cast<int>(players.size())) {
            return id;
        }
    } catch (...) {}

    std::string lowerInput = toLower(input);
    for (const auto& p : players) {
        if (toLower(p.name) == lowerInput) {
            return p.id;
        }
    }
    return -1;
}

CardId parseCardId(const std::string& name) {
    std::string s = name;
    std::transform(s.begin(), s.end(), s.begin(), [](char c){
        return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    });
    if (s == "P-BR1") return CardId::P_BR1;
    if (s == "P-BR2") return CardId::P_BR2;
    if (s == "P-DB1") return CardId::P_DB1;
    if (s == "P-DB2") return CardId::P_DB2;
    return CardId::NONE;
}

std::string cardIdToString(CardId id) {
    switch(id) {
        case CardId::P_BR1: return "P-BR1";
        case CardId::P_BR2: return "P-BR2";
        case CardId::P_DB1: return "P-DB1";
        case CardId::P_DB2: return "P-DB2";
        default: return "NONE";
    }
}

std::string phaseToString(Phase p) {
    switch (p) {
        case Phase::PURCHASE: return "PURCHASE";
        case Phase::NEGOTIATION: return "NEGOTIATION";
        case Phase::OPERATIONAL: return "OPERATIONAL";
    }
    return "UNKNOWN";
}

std::string actionTypeToString(ActionType type) {
    switch (type) {
        case ActionType::PURCHASE: return "PURCHASE";
        case ActionType::PASS: return "PASS";
        case ActionType::ROLL_AND_MOVE: return "MOVE";
        case ActionType::PAY_RENT: return "RENT";
        case ActionType::PAY_TAX: return "TAX";
        case ActionType::GO_BONUS: return "GO_BONUS";
        case ActionType::TRADE_EXECUTE: return "TRADE_EXEC";
        case ActionType::TRADE_CANCEL: return "TRADE_CANCEL";
        case ActionType::BUILD_HOUSE: return "BUILD";
        case ActionType::SELL_HOUSE: return "SELL";
        case ActionType::BANKRUPT: return "BANKRUPT";
        case ActionType::OVERRIDE: return "OVERRIDE";
        default: return "OTHER";
    }
}

void printCardDetail(const MonopolyEngine& engine, CardId cId) {
    if (cId == CardId::NONE) {
        std::cout << "[Error] Invalid card ID or this position is not a purchasable property." << std::endl;
        return;
    }
    const auto& card = engine.getCard(cId);
    std::cout << "\n--- Card Info: " << cardIdToString(cId) << " (" << card.cliName << ") ---" << std::endl;
    std::cout << " Price: " << card.price << "万 | House Price: " << card.housePrice << "万" << std::endl;
    std::cout << " Rent: [0H:" << card.rent[0] << "万";
    for (size_t i = 1; i < card.rent.size(); ++i) {
        std::cout << ", " << i << "H:" << card.rent[i] << "万";
    }
    std::cout << "]" << std::endl;
}

void printPlayerCards(const MonopolyEngine& engine, int playerId) {
    const auto& state = engine.getState();
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) {
        std::cout << "[Error] Invalid player ID/Name." << std::endl;
        return;
    }
    const auto& p = state.players[playerId];
    std::cout << "\n--- [" << p.name << "] の所有カード一覧 ---" << std::endl;
    if (p.ownedCards.empty()) {
        std::cout << "  (所有カードなし)" << std::endl;
        return;
    }
    for (CardId cId : p.ownedCards) {
        const auto& card = engine.getCard(cId);
        std::cout << "  - " << cardIdToString(cId) << " (" << card.cliName << ") | Price: " 
                  << card.price << "万 | House: " << card.houseCount << std::endl;
    }
}

void printStatus(const MonopolyEngine& engine) {
    const auto& state = engine.getState();
    std::cout << "\n================================================================================" << std::endl;
    std::cout << " [Turn " << state.turnNumber << "] Phase: " << phaseToString(state.currentPhase)
              << " | Active: " << state.players[state.activePlayerIndex].name << std::endl;
    std::cout << "--------------------------------------------------------------------------------" << std::endl;
    for (const auto& p : state.players) {
        if (p.isBankrupt) {
            std::cout << "   [" << p.id << "] " << p.name << " | [BANKRUPT]" << std::endl;
            continue;
        }
        double ev = engine.calculateExpectedIncomePerTurn(p.id);
        int assets = engine.calculateTotalLiquidableAssets(p.id);
        
        std::string cardListStr = "";
        for (CardId cId : p.ownedCards) {
            const auto& card = engine.getCard(cId);
            cardListStr += cardIdToString(cId);
            if (card.houseCount > 0) cardListStr += "(H:" + std::to_string(card.houseCount) + ")";
            cardListStr += " ";
        }
        if (cardListStr.empty()) cardListStr = "なし";

        std::cout << " " << (p.id == state.activePlayerIndex ? "* " : "  ")
                  << "[" << p.id << "] " << p.name 
                  << " | Money: " << p.money << "万 | Pos: " << p.position 
                  << " | Assets: " << assets << "万"
                  << " | EV: +" << ev << "万/T" << std::endl;
        std::cout << "       Owned Cards: " << cardListStr << std::endl;
    }
    std::cout << "================================================================================" << std::endl;
}

int main() {
    GameSettings config = CliSetup::runWizard();
    MonopolyEngine engine;

    if (config.mode == SetupMode::LOAD_LOG) {
        if (engine.importAndReplayLog(config.importFilename)) {
            std::cout << "Successfully loaded and replayed Log File: " << config.importFilename 
                      << " (" << engine.getLogs().size() << " events)" << std::endl;
        } else {
            std::cout << "[Error] Failed to load log file: " << config.importFilename 
                      << ". Starting default new game." << std::endl;
            engine.initGame({"Player_1", "Player_2"}, true);
        }
    } else {
        engine.initGame(config.playerNames, config.enableTax);
        if (config.startingMoney != 1500) {
            for (int i = 0; i < config.playerCount; ++i) {
                engine.forceSetMoney(i, config.startingMoney);
            }
        }
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(1, 6);

    std::vector<CardId> allCards = {
        CardId::P_BR1, CardId::P_BR2, CardId::P_DB1, CardId::P_DB2
    };

    printStatus(engine);

    std::string line;
    while (true) {
        const auto& state = engine.getState();
        int activeId = state.activePlayerIndex;
        std::cout << "\n[" << state.players[activeId].name << " (" << phaseToString(state.currentPhase) << ")]> ";
        if (!std::getline(std::cin, line)) break;

        std::stringstream ss(line);
        std::string rawCmd;
        ss >> rawCmd;
        std::string cmd = toLower(rawCmd);

        if (cmd == "exit") {
            std::cout << "Exiting game..." << std::endl;
            break;
        } else if (cmd == "save" || cmd == "export") {
            std::string saveName;
            if (!(ss >> saveName)) saveName = "monopoly_saved.log";
            if (engine.exportLog(saveName)) {
                std::cout << "Successfully saved Deterministic Log File to: " << saveName << std::endl;
            } else {
                std::cout << "[Error] Failed to save Log File." << std::endl;
            }
        } else if (cmd == "status" || cmd == "show") {
            printStatus(engine);
        } else if (cmd == "mycards") {
            printPlayerCards(engine, activeId);
        } else if (cmd == "cards") {
            std::string targetArg;
            if (ss >> targetArg) {
                int targetId = resolvePlayerId(targetArg, state.players);
                if (targetId != -1) {
                    printPlayerCards(engine, targetId);
                } else {
                    std::cout << "[Error] Unknown player: " << targetArg << std::endl;
                }
            } else {
                std::cout << "\n--- 全カード売り出し・所有ステータス ---" << std::endl;
                for (CardId cId : allCards) {
                    const auto& card = engine.getCard(cId);
                    int owner = -1;
                    for (const auto& p : state.players) {
                        if (std::find(p.ownedCards.begin(), p.ownedCards.end(), cId) != p.ownedCards.end()) {
                            owner = p.id; break;
                        }
                    }
                    std::cout << " " << cardIdToString(cId) << " (" << card.cliName << ") | Price: " << card.price << "万";
                    if (owner != -1) {
                        std::cout << " | Owner: " << state.players[owner].name << " (House: " << card.houseCount << ")";
                    } else {
                        std::cout << " | Owner: UNOWNED";
                    }
                    std::cout << std::endl;
                }
            }
        } else if (cmd == "info") {
            std::string arg;
            if (ss >> arg) {
                if (toLower(arg) == "pos") {
                    int pos = state.players[activeId].position;
                    CardId landedId = CardId::NONE;
                    for (CardId cId : allCards) {
                        if (engine.getCard(cId).boardPosition == pos) {
                            landedId = cId; break;
                        }
                    }
                    if (landedId != CardId::NONE) {
                        printCardDetail(engine, landedId);
                    } else {
                        std::cout << "\n[Info] Current position (" << pos << ") is Special Square (GO, Tax, Chance, etc)." << std::endl;
                    }
                } else {
                    CardId cId = parseCardId(arg);
                    printCardDetail(engine, cId);
                }
            } else {
                int pos = state.players[activeId].position;
                CardId landedId = CardId::NONE;
                for (CardId cId : allCards) {
                    if (engine.getCard(cId).boardPosition == pos) {
                        landedId = cId; break;
                    }
                }
                if (landedId != CardId::NONE) {
                    printCardDetail(engine, landedId);
                } else {
                    std::cout << "\n[Info] Current position (" << pos << ") is Special Square (GO, Tax, Chance, etc)." << std::endl;
                }
            }
        } else if (cmd == "log" || cmd == "logs") {
            std::cout << "\n--- イベントログ (" << engine.getLogs().size() << "件) ---" << std::endl;
            for (const auto& evt : engine.getLogs()) {
                std::cout << " Event #" << evt.eventId << " [Turn " << evt.turn << "] "
                          << state.players[evt.playerId].name << " -> " << actionTypeToString(evt.type);
                if (evt.targetPlayerId != -1) {
                    std::cout << " (Target: " << state.players[evt.targetPlayerId].name << ")";
                }
                if (evt.moneyChange != 0) {
                    std::cout << " | Money: " << (evt.moneyChange > 0 ? "+" : "") << evt.moneyChange << "万";
                }
                std::cout << std::endl;
            }
        } else if (cmd == "roll") {
            if (state.currentPhase != Phase::OPERATIONAL) {
                std::cout << "[Error] Cannot roll dice in " << phaseToString(state.currentPhase) << " phase!" << std::endl;
            } else {
                int d1 = 0, d2 = 0;
                if (!(ss >> d1 >> d2)) {
                    d1 = dis(gen);
                    d2 = dis(gen);
                }
                std::cout << "Rolling dice: " << d1 << " + " << d2 << " = " << (d1 + d2) << std::endl;
                if (engine.rollAndMove(activeId, d1, d2)) {
                    std::cout << "Moved to position " << engine.getState().players[activeId].position << std::endl;
                } else {
                    std::cout << "[Error] Cannot roll dice." << std::endl;
                }
            }
        } else if (cmd == "buy") {
            std::string cardStr;
            ss >> cardStr;
            CardId cId = parseCardId(cardStr);
            if (cId == CardId::NONE) {
                std::cout << "[Error] Unknown card ID: " << cardStr << std::endl;
            } else {
                if (engine.buyCardInPurchasePhase(activeId, cId)) {
                    std::cout << "Successfully bought " << cardStr << "!" << std::endl;
                } else {
                    std::cout << "[Error] Failed to buy card." << std::endl;
                }
            }
        } else if (cmd == "pass") {
            if (state.currentPhase == Phase::PURCHASE) {
                engine.passInPurchasePhase(activeId);
                std::cout << "Pass registered in PURCHASE phase." << std::endl;
            } else if (state.currentPhase == Phase::OPERATIONAL) {
                engine.passTurn();
                std::cout << "Turn passed." << std::endl;
            }
            printStatus(engine);
        } else if (cmd == "build") {
            std::string cardStr;
            ss >> cardStr;
            CardId cId = parseCardId(cardStr);
            if (engine.buildHouse(activeId, cId)) {
                std::cout << "Built 1 house on " << cardStr << "!" << std::endl;
            } else {
                std::cout << "[Error] Cannot build house." << std::endl;
            }
        } else if (cmd == "sell") {
            std::string cardStr;
            ss >> cardStr;
            CardId cId = parseCardId(cardStr);
            if (engine.sellHouse(activeId, cId)) {
                std::cout << "Sold 1 house on " << cardStr << "!" << std::endl;
            } else {
                std::cout << "[Error] Cannot sell house." << std::endl;
            }
        } else if (cmd == "ready") {
            if (engine.setNegotiationReady(activeId, true)) {
                std::cout << state.players[activeId].name << " set Negotiation Ready!" << std::endl;
                printStatus(engine);
            }
        } else if (cmd == "trade") {
            std::string subCmd;
            ss >> subCmd;
            subCmd = toLower(subCmd);
            if (subCmd == "exec" || subCmd == "cancel") {
                std::string targetStr, offerCardStr, reqCardStr;
                int offerM = 0, reqM = 0;
                ss >> targetStr >> offerM >> offerCardStr >> reqM >> reqCardStr;
                
                int targetId = resolvePlayerId(targetStr, state.players);
                if (targetId == -1) {
                    std::cout << "[Error] Unknown target player: " << targetStr << std::endl;
                    continue;
                }

                TradeOffer offer;
                offer.proposerId = activeId;
                offer.targetId = targetId;
                offer.offerMoney = offerM;
                if (toLower(offerCardStr) != "none") offer.offerCards.push_back(parseCardId(offerCardStr));
                offer.requestMoney = reqM;
                if (toLower(reqCardStr) != "none") offer.requestCards.push_back(parseCardId(reqCardStr));

                if (subCmd == "exec") {
                    if (engine.executeTrade(offer)) {
                        std::cout << "Trade EXECUTED successfully!" << std::endl;
                        printStatus(engine);
                    } else {
                        std::cout << "[Error] Trade failed." << std::endl;
                    }
                } else {
                    engine.cancelTrade(offer);
                    std::cout << "Trade CANCELLED logged." << std::endl;
                }
            }
        } else if (cmd == "admin") {
            std::string subCmd;
            ss >> subCmd;
            subCmd = toLower(subCmd);
            if (subCmd == "advance") {
                engine.forceAdvancePhase();
                std::cout << "Phase forced to advance." << std::endl;
                printStatus(engine);
            }
        } else if (cmd == "undo") {
            if (engine.undo()) {
                std::cout << "Undo successful!" << std::endl;
                printStatus(engine);
            } else {
                std::cout << "[Error] Cannot undo further." << std::endl;
            }
        } else if (!cmd.empty()) {
            std::cout << "Unknown command: " << cmd << std::endl;
        }
    }

    return 0;
}
