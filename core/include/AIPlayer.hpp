#ifndef AI_PLAYER_HPP
#define AI_PLAYER_HPP

#include "MonopolyEngine.hpp"
#include "TradeOffer.hpp"
#include <optional>

enum class AIDifficulty {
    EASY,
    BALANCED,
    AGGRESSIVE
};

class AIPlayer {
public:
    AIPlayer(int playerId, AIDifficulty difficulty = AIDifficulty::BALANCED);

    int getPlayerId() const { return playerId; }

    // 各フェーズでのAI意思決定関数
    bool shouldBuyProperty(const MonopolyEngine& engine, CardId cardId) const;
    bool shouldBuildHouse(const MonopolyEngine& engine, CardId cardId) const;
    std::optional<TradeOffer> proposeTrade(const MonopolyEngine& engine) const;
    bool acceptTrade(const MonopolyEngine& engine, const TradeOffer& offer) const;

private:
    int playerId;
    AIDifficulty difficulty;
    int safetyCashMargin; // 最低保持しておきたい安全資金量
};

#endif
