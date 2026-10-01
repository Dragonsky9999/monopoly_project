#ifndef AI_PLAYER_HPP
#define AI_PLAYER_HPP

#include "MonopolyEngine.hpp"
#include "TradeOffer.hpp"
#include <optional>
#include <vector>

enum class AIDifficulty {
    EASY,
    BALANCED,
    AGGRESSIVE,
    MASTER // ガチ思考プロファイル
};

class AIPlayer {
public:
    AIPlayer(int playerId, AIDifficulty difficulty = AIDifficulty::MASTER);

    int getPlayerId() const { return playerId; }

    // 高度な意思決定関数
    bool shouldBuyProperty(const MonopolyEngine& engine, CardId cardId) const;
    bool shouldBuildHouse(const MonopolyEngine& engine, CardId cardId) const;
    std::optional<TradeOffer> proposeTrade(const MonopolyEngine& engine) const;
    bool acceptTrade(const MonopolyEngine& engine, const TradeOffer& offer) const;

private:
    int playerId;
    AIDifficulty difficulty;
    int safetyCashMargin;

    // ヘルパー：特定グループの所有状況を調査
    bool isGroupTargetNeeded(const MonopolyEngine& engine, CardId cardId) const;
};

#endif
