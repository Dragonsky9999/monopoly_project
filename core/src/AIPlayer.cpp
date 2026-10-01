#include "../include/AIPlayer.hpp"
#include <algorithm>

AIPlayer::AIPlayer(int id, AIDifficulty diff)
    : playerId(id), difficulty(diff) {
    switch (difficulty) {
        case AIDifficulty::EASY:
            safetyCashMargin = 100;
            break;
        case AIDifficulty::BALANCED:
            safetyCashMargin = 200;
            break;
        case AIDifficulty::AGGRESSIVE:
            safetyCashMargin = 50;
            break;
    }
}

bool AIPlayer::shouldBuyProperty(const MonopolyEngine& engine, CardId cardId) const {
    const auto& state = engine.getState();
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) return false;
    
    const auto& player = state.players[playerId];
    const auto& card = engine.getCard(cardId);

    // 買収後に安全資金が残るかチェック
    if (player.money - card.price < safetyCashMargin) {
        return false;
    }

    return true;
}

bool AIPlayer::shouldBuildHouse(const MonopolyEngine& engine, CardId cardId) const {
    const auto& state = engine.getState();
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) return false;

    const auto& player = state.players[playerId];
    const auto& card = engine.getCard(cardId);

    // 建設後に十分な現金が残るかチェック
    if (player.money - card.housePrice < safetyCashMargin + 100) {
        return false;
    }

    return true;
}

std::optional<TradeOffer> AIPlayer::proposeTrade(const MonopolyEngine& engine) const {
    // トレード提案ロジック（現在試算用プレースホルダー）
    return std::nullopt;
}

bool AIPlayer::acceptTrade(const MonopolyEngine& engine, const TradeOffer& offer) const {
    if (offer.targetId != playerId) return false;

    // AIエンジンによるフェアネススコア評価
    double fairness = engine.evaluateTradeFairness(offer);

    // 自分視点でプラス（公平以上）であれば受諾
    return (fairness >= 0.0);
}
