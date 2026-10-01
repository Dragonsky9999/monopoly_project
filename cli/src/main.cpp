#include "MonopolyEngine.hpp"
#include "TradeOffer.hpp"
#include "CliSetup.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <random>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iomanip>

std::string generateTimestampedFilename() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm buf;
#if defined(_WIN32)
    localtime_s(&buf, &in_time_t);
#else
    localtime_r(&in_time_t, &buf);
#endif
    std::ostringstream ss;
    ss << "monopoly_log_" << std::put_time(&buf, "%Y%m%d_%H%M%S") << ".log";
    return ss.str();
}

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
    if (s == "P-BR1" || s == "P_BR1") return CardId::P_BR1;
    if (s == "P-BR2" || s == "P_BR2") return CardId::P_BR2;
    if (s == "P-LB1" || s == "P_LB1") return CardId::P_LB1;
    if (s == "P-LB2" || s == "P_LB2") return CardId::P_LB2;
    if (s == "P-LB3" || s == "P_LB3") return CardId::P_LB3;
    if (s == "P-PK1" || s == "P_PK1") return CardId::P_PK1;
    if (s == "P-PK2" || s == "P_PK2") return CardId::P_PK2;
    if (s == "P-PK3" || s == "P_PK3") return CardId::P_PK3;
    if (s == "P-OR1" || s == "P_OR1") return CardId::P_OR1;
    if (s == "P-OR2" || s == "P_OR2") return CardId::P_OR2;
    if (s == "P-OR3" || s == "P_OR3") return CardId::P_OR3;
    if (s == "P-RD1" || s == "P_RD1") return CardId::P_RD1;
    if (s == "P-RD2" || s == "P_RD2") return CardId::P_RD2;
    if (s == "P-RD3" || s == "P_RD3") return CardId::P_RD3;
    if (s == "P-YL1" || s == "P_YL1") return CardId::P_YL1;
    if (s == "P-YL2" || s == "P_YL2") return CardId::P_YL2;
    if (s == "P-YL3" || s == "P_YL3") return CardId::P_YL3;
    if (s == "P-GR1" || s == "P_GR1") return CardId::P_GR1;
    if (s == "P-GR2" || s == "P_GR2") return CardId::P_GR2;
    if (s == "P-GR3" || s == "P_GR3") return CardId::P_GR3;
    if (s == "P-DB1" || s == "P_DB1") return CardId::P_DB1;
    if (s == "P-DB2" || s == "P_DB2") return CardId::P_DB2;
    if (s == "R-READING" || s == "R_READING") return CardId::R_READING;
    if (s == "R-PENNSYLVANIA" || s == "R_PENNSYLVANIA") return CardId::R_PENNSYLVANIA;
    if (s == "R-B-AND-O" || s == "R_B_AND_O") return CardId::R_B_AND_O;
    if (s == "R-SHORT-LINE" || s == "R_SHORT_LINE") return CardId::R_SHORT_LINE;
    if (s == "U-ELECTRIC" || s == "U_ELECTRIC") return CardId::U_ELECTRIC;
    if (s == "U-WATER" || s == "U_WATER") return CardId::U_WATER;
    return CardId::NONE;
}

std::string cardIdToString(CardId id) {
    switch(id) {
        case CardId::P_BR1: return "P-BR1"; case CardId::P_BR2: return "P-BR2";
        case CardId::P_LB1: return "P-LB1"; case CardId::P_LB2: return "P-LB2"; case CardId::P_LB3: return "P-LB3";
        case CardId::P_PK1: return "P-PK1"; case CardId::P_PK2: return "P-PK2"; case CardId::P_PK3: return "P-PK3";
        case CardId::P_OR1: return "P-OR1"; case CardId::P_OR2: return "P-OR2"; case CardId::P_OR3: return "P-OR3";
        case CardId::P_RD1: return "P-RD1"; case CardId::P_RD2: return "P-RD2"; case CardId::P_RD3: return "P-RD3";
        case CardId::P_YL1: return "P-YL1"; case CardId::P_YL2: return "P-YL2"; case CardId::P_YL3: return "P-YL3";
        case CardId::P_GR1: return "P-GR1"; case CardId::P_GR2: return "P-GR2"; case CardId::P_GR3: return "P-GR3";
        case CardId::P_DB1: return "P-DB1"; case CardId::P_DB2: return "P-DB2";
        case CardId::R_READING: return "R-READING"; case CardId::R_PENNSYLVANIA: return "R-PENNSYLVANIA";
        case CardId::R_B_AND_O: return "R-B-AND-O"; case CardId::R_SHORT_LINE: return "R-SHORT-LINE";
        case CardId::U_ELECTRIC: return "U-ELECTRIC"; case CardId::U_WATER: return "U-WATER";
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
        CardId::P_BR1, CardId::P_BR2, CardId::P_LB1, CardId::P_LB2, CardId::P_LB3,
        CardId::P_PK1, CardId::P_PK2, CardId::P_PK3, CardId::P_OR1, CardId::P_OR2, CardId::P_OR3,
        CardId::P_RD1, CardId::P_RD2, CardId::P_RD3, CardId::P_YL1, CardId::P_YL2, CardId::P_YL3,
        CardId::P_GR1, CardId::P_GR2, CardId::P_GR3, CardId::P_DB1, CardId::P_DB2,
        CardId::R_READING, CardId::R_PENNSYLVANIA, CardId::R_B_AND_O, CardId::R_SHORT_LINE,
        CardId::U_ELECTRIC, CardId::U_WATER
    };

    printStatus(engine);

    std::string line;
    while (true) {
        const auto& state = engine.getState();
        int activeId = state.activePlayerIndex;
        std::cout << "\n[" << state.players[activeId].name << " (" << phaseToString(state.currentPhase) << ")]> ";
        if (!std::getline(std::cin, line)) {
            // EOF (Ctrl+Z / Ctrl+D) の場合も自動保存
            std::string autoSaveName = generateTimestampedFilename();
            engine.exportLog(autoSaveName);
            std::cout << "\n[Auto-Save] Deterministic Log saved to: " << autoSaveName << std::endl;
            break;
        }

        std::stringstream ss(line);
        std::string rawCmd;
        ss >> rawCmd;
        std::string cmd = toLower(rawCmd);

        if (cmd == "exit") {
            std::string autoSaveName = generateTimestampedFilename();
            if (engine.exportLog(autoSaveName)) {
                std::cout << "[Auto-Save] Log saved on exit to: " << autoSaveName << std::endl;
            }
            std::cout << "Exiting game..." << std::endl;
            break;
        } else if (cmd == "save" || cmd == "export") {
            std::string saveName;
            if (!(ss >> saveName)) {
                saveName = generateTimestampedFilename();
            }
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
                std::cout << "\n--- 全28枚カード売り出し・所有ステータス ---" << std::endl;
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
            } else if (subCmd == "money") {
                std::string targetStr;
                int newMoney = 0;
                ss >> targetStr >> newMoney;
                int targetId = resolvePlayerId(targetStr, state.players);
                if (targetId != -1 && engine.forceSetMoney(targetId, newMoney)) {
                    std::cout << "Money set to " << newMoney << " for " << state.players[targetId].name << std::endl;
                    printStatus(engine);
                } else {
                    std::cout << "[Error] Admin set money failed." << std::endl;
                }
            } else if (subCmd == "owner") {
                std::string cardStr, targetStr;
                ss >> cardStr >> targetStr;
                CardId cId = parseCardId(cardStr);
                int targetId = resolvePlayerId(targetStr, state.players);
                if (engine.forceSetCardOwner(cId, targetId)) {
                    std::cout << "Card owner changed successfully." << std::endl;
                    printStatus(engine);
                } else {
                    std::cout << "[Error] Admin set owner failed." << std::endl;
                }
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
