#include "MonopolyEngine.hpp"
#include <iostream>

int main() {
    MonopolyEngine engine;
    
    std::vector<std::string> players = {"Player_A", "Player_B", "Player_C"};
    engine.initGame(players, true);

    std::cout << "==========================================" << std::endl;
    std::cout << " Business Monopoly CLI Prototype (Core v1.0)" << std::endl;
    std::cout << "==========================================" << std::endl;

    std::cout << "Player 0 Money: " << engine.getState().players[0].money << "万円" << std::endl;

    std::cout << "\n[Action] Player 0 buys P-BR1..." << std::endl;
    if (engine.buyCardInPurchasePhase(0, CardId::P_BR1)) {
        std::cout << "Success! Money left: " << engine.getState().players[0].money << "万円" << std::endl;
    }

    std::cout << "\n[Action] Player 0 tries P-BR2 (Same Group)..." << std::endl;
    if (!engine.buyCardInPurchasePhase(0, CardId::P_BR2)) {
        std::cout << "Rejected correctly by Rule Engine!" << std::endl;
    }

    std::cout << "\n[Action] Undo..." << std::endl;
    if (engine.undo()) {
        std::cout << "Undo Success! Money restored: " << engine.getState().players[0].money << "万円" << std::endl;
    }

    return 0;
}
