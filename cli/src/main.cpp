#include "MonopolyEngine.hpp"
#include "TradeOffer.hpp"
#include "CliSetup.hpp"
#include "AIPlayer.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <random>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <vector>
#include <memory>
#include <thread>

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

void handleAITurn(MonopolyEngine& engine, const AIPlayer& ai, std::mt19937& gen) {
    const auto& state = engine.getState();
    int activeId = state.activePlayerIndex;
    if (activeId != ai.getPlayerId()) return;

    std::uniform_int_distribution<int> dis(1, 6);
    std::cout << "\n>>> [AI Turn] " << state.players[activeId].name << " is thinking... (" << phaseToString(state.currentPhase) << ") <<<" << std::endl;

    if (state.currentPhase == Phase::PURCHASE) {
        int pos = state.players[activeId].position;
        CardId landedId = engine.getCardIdAtPosition(pos);
        if (landedId != CardId::NONE && engine.getOwnerOfCard(landedId) == -1) {
            if (ai.shouldBuyProperty(engine, landedId)) {
                if (engine.buyCardInPurchasePhase(activeId, landedId)) {
                    std::cout << "   AI Action: BOUGHT " << cardIdToString(landedId) << "!" << std::endl;
                    return;
                }
            }
        }
        engine.passInPurchasePhase(activeId);
        std::cout << "   AI Action: PASSED in Purchase Phase." << std::endl;
    } 
    else if (state.currentPhase == Phase::NEGOTIATION) {
        engine.setNegotiationReady(activeId, true);
        std::cout << "   AI Action: SET READY for Negotiation." << std::endl;
    } 
    else if (state.currentPhase == Phase::OPERATIONAL) {
        int d1 = dis(gen);
        int d2 = dis(gen);
        std::cout << "   AI Rolling dice: " << d1 << " + " << d2 << " = " << (d1 + d2) << std::endl;
        engine.rollAndMove(activeId, d1, d2);

        int newPos = engine.getState().players[activeId].position;
        CardId landedCardId = engine.getCardIdAtPosition(newPos);
        if (landedCardId != CardId::NONE && engine.getOwnerOfCard(landedCardId) == -1) {
            if (ai.shouldBuyProperty(engine, landedCardId)) {
                if (engine.buyCardInPurchasePhase(activeId, landedCardId)) {
                    std::cout << "   AI Action: BOUGHT " << cardIdToString(landedCardId) << " after move!" << std::endl;
                }
            }
        }

        for (CardId cId : state.players[activeId].ownedCards) {
            if (ai.shouldBuildHouse(engine, cId)) {
                if (engine.buildHouse(activeId, cId)) {
                    std::cout << "   AI Action: BUILT house on " << cardIdToString(cId) << "!" << std::endl;
                }
            }
        }

        engine.passTurn();
        std::cout << "   AI Action: PASSED Turn." << std::endl;
    }
}

int main() {
    GameSettings config = CliSetup::runWizard();
    MonopolyEngine engine;

    engine.initGame(config.playerNames, config.enableTax);
    if (config.startingMoney != 1500) {
        for (int i = 0; i < config.playerCount; ++i) {
            engine.forceSetMoney(i, config.startingMoney);
        }
    }

    std::unordered_map<int, std::unique_ptr<AIPlayer>> aiPlayers;
    if (config.playerCount > 1) {
        for (int i = 1; i < config.playerCount; ++i) {
            aiPlayers[i] = std::make_unique<AIPlayer>(i, AIDifficulty::BALANCED);
            std::cout << "Registered AI Bot for Player [" << i << "] " << config.playerNames[i] << std::endl;
        }
    }

    std::random_device rd;
    std::mt19937 gen(rd());

    printStatus(engine);

    std::string line;
    while (true) {
        const auto& state = engine.getState();
        int activeId = state.activePlayerIndex;

        if (aiPlayers.find(activeId) != aiPlayers.end()) {
            handleAITurn(engine, *aiPlayers[activeId], gen);
            printStatus(engine);
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            continue;
        }

        std::cout << "\n[" << state.players[activeId].name << " (" << phaseToString(state.currentPhase) << ")]> ";
        if (!std::getline(std::cin, line)) {
            std::string autoSaveName = generateTimestampedFilename();
            engine.exportLog(autoSaveName);
            std::cout << "\n[Auto-Save] Log saved to: " << autoSaveName << std::endl;
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
        } else if (cmd == "status" || cmd == "show") {
            printStatus(engine);
        } else if (cmd == "pass") {
            if (state.currentPhase == Phase::PURCHASE) {
                engine.passInPurchasePhase(activeId);
                std::cout << "Passed purchase phase for " << state.players[activeId].name << std::endl;
            } else if (state.currentPhase == Phase::NEGOTIATION) {
                engine.setNegotiationReady(activeId, true);
            } else if (state.currentPhase == Phase::OPERATIONAL) {
                engine.passTurn();
            }
            printStatus(engine);
        } else if (cmd == "buy") {
            std::string cardStr;
            if (ss >> cardStr) {
                CardId cId = parseCardId(cardStr);
                if (cId != CardId::NONE) {
                    if (engine.buyCardInPurchasePhase(activeId, cId)) {
                        std::cout << "Successfully bought " << cardStr << "!" << std::endl;
                    } else {
                        std::cout << "[Error] Cannot buy " << cardStr << " (Not at this position, already owned, or insufficient funds)." << std::endl;
                    }
                } else {
                    std::cout << "[Error] Invalid card ID: " << cardStr << std::endl;
                }
            } else {
                std::cout << "[Error] Specify card ID to buy (e.g., buy P-BR1)." << std::endl;
            }
            printStatus(engine);
        } else if (cmd == "roll") {
            if (state.currentPhase == Phase::OPERATIONAL) {
                std::uniform_int_distribution<int> dis(1, 6);
                int d1 = dis(gen), d2 = dis(gen);
                std::cout << "Rolling dice: " << d1 << " + " << d2 << " = " << (d1 + d2) << std::endl;
                engine.rollAndMove(activeId, d1, d2);
                printStatus(engine);
            }
        } else if (cmd == "ready") {
            if (state.currentPhase == Phase::NEGOTIATION) {
                engine.setNegotiationReady(activeId, true);
                printStatus(engine);
            }
        } else if (cmd == "save" || cmd == "export") {
            std::string saveName = generateTimestampedFilename();
            if (engine.exportLog(saveName)) {
                std::cout << "Saved Log to: " << saveName << std::endl;
            }
        } else if (!cmd.empty()) {
            std::cout << "Unknown command." << std::endl;
        }
    }

    return 0;
}
