#include "../include/AIPlayer.hpp"
#include <algorithm>
#include <random>
#include <iostream>

AIPlayer::AIPlayer(int id, AIDifficulty diff)
    : playerId(id), difficulty(diff) {
    switch (difficulty) {
        case AIDifficulty::EASY: safetyCashMargin = 30; break;
        case AIDifficulty::BALANCED: safetyCashMargin = 50; break;
        case AIDifficulty::AGGRESSIVE: safetyCashMargin = 10; break;
        case AIDifficulty::MASTER: safetyCashMargin = 30; break;
    }
}

bool AIPlayer::shouldBuyProperty(const MonopolyEngine& engine, CardId cardId) const {
    const auto& state = engine.getState();
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) return false;
    
    const auto& player = state.players[playerId];
    const auto& card = engine.getCard(cardId);

    std::cout << "   [AI Eval] " << player.name << " checking property " << static_cast<int>(cardId) 
              << " | Price: " << card.price << " | Money: " << player.money << std::endl;

    // 購入後も安全マージンが残る、あるいは所持金が価格以上であれば購入する
    if (player.money - card.price >= safetyCashMargin) {
        return true;
    }

    return false;
}

bool AIPlayer::shouldBuildHouse(const MonopolyEngine& engine, CardId cardId) const {
    const auto& state = engine.getState();
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) return false;

    const auto& player = state.players[playerId];
    const auto& card = engine.getCard(cardId);

    if (player.money - card.housePrice < safetyCashMargin + 30) {
        return false;
    }

    if (card.houseCount >= 5) {
        return false;
    }

    return true;
}

std::optional<TradeOffer> AIPlayer::proposeTrade(const MonopolyEngine& engine) const {
    const auto& state = engine.getState();
    if (playerId < 0 || playerId >= static_cast<int>(state.players.size())) return std::nullopt;

    const auto& me = state.players[playerId];

    for (const auto& other : state.players) {
        if (other.id == playerId || other.isBankrupt) continue;

        for (CardId theirCard : other.ownedCards) {
            const auto& tCard = engine.getCard(theirCard);
            
            bool iHaveSameGroup = false;
            for (CardId myCard : me.ownedCards) {
                if (engine.getCard(myCard).group == tCard.group && myCard != theirCard) {
                    iHaveSameGroup = true;
                    break;
                }
            }

            if (iHaveSameGroup && !me.ownedCards.empty()) {
                TradeOffer offer;
                offer.proposerId = playerId;
                offer.targetId = other.id;
                offer.offerMoney = 50;
                offer.offerCards.push_back(me.ownedCards[0]);
                offer.requestMoney = 0;
                offer.requestCards.push_back(theirCard);

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
    double fairness = engine.evaluateTradeFairness(offer);
    return (fairness >= 0.0);
}
