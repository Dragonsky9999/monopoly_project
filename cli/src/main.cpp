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

std::string phaseToString(Phase p) {
    switch (p) {
        case Phase::PURCHASE: return "PURCHASE";
        case Phase::NEGOTIATION: return "NEGOTIATION";
        case Phase::OPERATIONAL: return "OPERATIONAL";
    }
    return "UNKNOWN";
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

// AIの手番自動意思決定
void handleAITurn(MonopolyEngine& engine, const AIPlayer& ai, std::mt19937& gen) {
    const auto& state = engine.getState();
    int activeId = state.activePlayerIndex;
    if (activeId != ai.getPlayerId()) return;

    std::uniform_int_distribution<int> dis(1, 6);

    std::cout << "\n>>> [AI Turn] " << state.players[activeId].name << " is thinking... (" << phaseToString(state.currentPhase) << ") <<<" << std::endl;

    if (state.currentPhase == Phase::PURCHASE) {
        // 購入フェーズ：AIが止まったマスの権利書を購入可能か判定
        int pos = state.players[activeId].position;
        CardId landedId = engine.getCardIdAtPosition(pos);
        if (landedId != CardId::NONE && engine.getOwnerOfCard(landedId) == -1) {
            if (ai.shouldBuyProperty(engine, landedId)) {
                if (engine.buyCardInPurchasePhase(activeId, landedId)) {
                    std::cout << "   AI Action: BOUGHT property " << cardIdToString(landedId) << "!" << std::endl;
                    return;
                }
            }
        }
        // 購入しない場合はパス
        engine.passInPurchasePhase(activeId);
        std::cout << "   AI Action: PASSED in Purchase Phase." << std::endl;
    } 
    else if (state.currentPhase == Phase::NEGOTIATION) {
        // 交渉フェーズ：準備完了を宣言
        engine.setNegotiationReady(activeId, true);
        std::cout << "   AI Action: SET READY for Negotiation Phase." << std::endl;
    } 
    else if (state.currentPhase == Phase::OPERATIONAL) {
        // 運用フェーズ：サイコロを振って移動
        int d1 = dis(gen);
        int d2 = dis(gen);
        std::cout << "   AI Rolling dice: " << d1 << " + " << d2 << " = " << (d1 + d2) << std::endl;
        engine.rollAndMove(activeId, d1, d2);

        // 家建設チェック
        for (CardId cId : state.players[activeId].ownedCards) {
            if (ai.shouldBuildHouse(engine, cId)) {
                if (engine.buildHouse(activeId, cId)) {
                    std::cout << "   AI Action: BUILT 1 house on " << cardIdToString(cId) << "!" << std::endl;
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

    if (config.mode == SetupMode::LOAD_LOG) {
        if (engine.importAndReplayLog(config.importFilename)) {
            std::cout << "Successfully loaded and replayed Log File: " << config.importFilename << std::endl;
        } else {
            std::cout << "[Error] Failed to load log file. Starting default game." << std::endl;
            engine.initGame({"Player_1", "Player_2"}, config.enableTax);
        }
    } else {
        // ウィザードで指定された2〜10人のプレイヤー名で初期化
        engine.initGame(config.playerNames, config.enableTax);
        if (config.startingMoney != 1500) {
            for (int i = 0; i < config.playerCount; ++i) {
                engine.forceSetMoney(i, config.startingMoney);
            }
        }
    }

    // デモ・テスト用：プレイヤー2以降をAIとして自動参加させる設定（必要に応じて人間/AI混成可）
    std::unordered_map<int, std::unique_ptr<AIPlayer>> aiPlayers;
    if (config.playerCount > 1) {
        // Player 1以降をAIに設定（例として全自動シミュレーションの場合は全員AI）
        for (int i = 1; i < config.playerCount; ++i) {
            aiPlayers[i] = std::make_unique<AIPlayer>(i, AIDifficulty::AGGRESSIVE);
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

        // 現在の手番がAIの場合
        if (aiPlayers.find(activeId) != aiPlayers.end()) {
            handleAITurn(engine, *aiPlayers[activeId], gen);
            printStatus(engine);
            std::this_thread::sleep_for(std::chrono::milliseconds(400)); // 視認用ウェイト
            continue;
        }

        // 人間プレイヤーの手番入力
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
            } else if (state.currentPhase == Phase::NEGOTIATION) {
                engine.setNegotiationReady(activeId, true);
            } else if (state.currentPhase == Phase::OPERATIONAL) {
                engine.passTurn();
            }
            printStatus(engine);
        } else if (cmd == "buy") {
            std::string cardStr;
            ss >> cardStr;
            // カード文字列からID解決して購入
            // (簡易実装)
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
