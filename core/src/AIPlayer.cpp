#include "../include/AIPlayer.hpp"
#include <algorithm>
#include <random>

AIPlayer::AIPlayer(int id, AIDifficulty diff)
    : playerId(id), difficulty(diff) {
    switch (difficulty) {
        case AIDifficulty::EASY:
            safetyCashMargin = 150;
            break;
        case AIDifficulty::BALANCED:
            safetyCashMargin = 250;
            break;
        case AIDifficulty::AGGRESSIVE:
            safetyCashMargin = 100;
            break;
        case AIDifficulty::MASTER:
            safetyCashMargin = 300; // 破産を極力避け、家建設資金を十分に保つガチ設定
            break;
    }
}

bool AIPlayer::shouldBuyProperty(const MonopolyEngine& engine, CardId cardId) const {
    const auto& state = engine.getState();
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) return false;
    
    const auto& player = state.players[playerId];
    const auto& card = engine.getCard(cardId);

    // 資金が安全マージンを下回る場合は購入見送り
    if (player.money - card.price < safetyCashMargin) {
        // ただし、マスターAIでカラーグループが揃う直前の最後の1枚ならリスクをとる
        if (difficulty == AIDifficulty::MASTER && player.money - card.price >= 50) {
            return true;
        }
        return false;
    }

    return true;
}

bool AIPlayer::shouldBuildHouse(const MonopolyEngine& engine, CardId cardId) const {
    const auto& state = engine.getState();
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) return false;

    const auto& player = state.players[playerId];
    const auto& card = engine.getCard(cardId);

    // マスタールール：家を建てても安全資金が確保できる場合のみ建設
    if (player.money - card.housePrice < safetyCashMargin + 100) {
        return false;
    }

    // すでに5軒（ホテル）に達している場合は建てられない
    if (card.houseCount >= 5) {
        return false;
    }

    return true;
}

std::optional<TradeOffer> AIPlayer::proposeTrade(const MonopolyEngine& engine) const {
    const auto& state = engine.getState();
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) return std::nullopt;

    const auto& me = state.players[playerId];

    // ガチAIのトレード提案ロジック：
    // 他のプレイヤーが持っているカードをスキャンし、自分がすでに同グループのカードを持っていればトレードを申し込む
    for (const auto& other : state.players) {
        if (other.id == playerId || other.isBankrupt) continue;

        for (CardId theirCard : other.ownedCards) {
            const auto& tCard = engine.getCard(theirCard);
            
            // 自分がこのグループのカードを少なくとも1枚持っており、相手のカードを欲しているか判定
            bool iHaveSameGroup = false;
            for (CardId myCard : me.ownedCards) {
                if (engine.getCard(myCard).group == tCard.group && myCard != theirCard) {
                    iHaveSameGroup = true;
                    break;
                }
            }

            if (iHaveSameGroup && !me.ownedCards.empty()) {
                // 提案の組み立て：自分の所有カードから適当な1枚と現金を提供し、相手のカードを要求
                TradeOffer offer;
                offer.proposerId = playerId;
                offer.targetId = other.id;
                offer.offerMoney = 50; // 50万のプレミアム付加
                offer.offerCards.push_back(me.ownedCards[0]);
                offer.requestMoney = 0;
                offer.requestCards.push_back(theirCard);

                // 公平性チェック（AIエンジンで検証）
                if (engine.evaluateTradeFairness(offer) >= -10.0) {
                    return offer;
                }
            }
        }
    }

    return std::nullopt;
}

bool AIPlayer::acceptTrade(const MonopolyEngine& engine, const TradeOffer& offer) const {
    if (offer.targetId != playerId) return false;

    // フェアネススコアがマイナス大きすぎなければ受諾（マスターAIは利益が出るトレードのみ厳選）
    double fairness = engine.evaluateTradeFairness(offer);
    
    if (difficulty == AIDifficulty::MASTER) {
        return (fairness >= 5.0); // 自分に少しでも有利な場合のみ受諾
    }

    return (fairness >= 0.0);
}
