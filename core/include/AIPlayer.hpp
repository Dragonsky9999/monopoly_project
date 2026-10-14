#ifndef AI_PLAYER_HPP
#define AI_PLAYER_HPP

#include "Action.hpp"
#include "MonopolyEngine.hpp"
#include <optional>
#include <random>

enum class AIDifficulty { EASY, BALANCED, AGGRESSIVE, MASTER };

// AI は「次に入力するアクション」を1つ返すだけ。適用は呼び出し側が engine.apply() で行う
// （人間の入力と同じ経路を通るので、AI の手もそのままログ・Undo の対象になる）
class AIPlayer {
public:
    AIPlayer(int playerId, AIDifficulty difficulty = AIDifficulty::BALANCED, unsigned seed = std::random_device{}());

    int getPlayerId() const { return playerId; }

    // 今この AI がやるべきことがなければ nullopt
    std::optional<Action> decide(const MonopolyEngine& engine);

    bool acceptTrade(const MonopolyEngine& engine, const TradeOffer& offer) const;

private:
    int playerId;
    AIDifficulty difficulty;
    int safetyCashMargin;
    std::mt19937 rng;
};

#endif
