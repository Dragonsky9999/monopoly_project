#include "../include/AIPlayer.hpp"
#include <algorithm>
#include <random>
#include <iostream>

AIPlayer::AIPlayer(int id, AIDifficulty diff)
    : playerId(id), difficulty(diff) {
    safetyCashMargin = 10; // ギリギリまで買う
}

bool AIPlayer::shouldBuyProperty(const MonopolyEngine& engine, CardId cardId) const {
    const auto& state = engine.getState();
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) return false;
    
    const auto& player = state.players[playerId];
    const auto& card = engine.getCard(cardId);

    std::cout << "   [AI Force Buy] " << player.name << " evaluating card " << static_cast<int>(cardId) 
              << " | Price: " << card.price << " | Money: " << player.money << std::endl;

    // 所持金が価格を上回っていれば問答無用で購入（絶対に買わせる）
    if (player.money >= card.price) {
        std::cout << "   >>> AI DECIDED TO BUY! <<<" << std::endl;
        return true;
    }

    std::cout << "   >>> AI PASSED (Not enough money) <<<" << std::endl;
    return false;
}

bool AIPlayer::shouldBuildHouse(const MonopolyEngine& engine, CardId cardId) const {
    const auto& state = engine.getState();
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) return false;
    const auto& player = state.players[playerId];
    const auto& card = engine.getCard(cardId);

    return (player.money >= card.housePrice + 100 && card.houseCount < 5);
}

std::optional<TradeOffer> AIPlayer::proposeTrade(const MonopolyEngine& engine) const {
    return std::nullopt;
}

bool AIPlayer::acceptTrade(const MonopolyEngine& engine, const TradeOffer& offer) const {
    return true;
}
