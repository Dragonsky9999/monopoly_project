#include "AIPlayer.hpp"

AIPlayer::AIPlayer(int id, AIDifficulty diff, unsigned seed) : playerId(id), difficulty(diff), rng(seed) {
    switch (difficulty) {
        case AIDifficulty::EASY: safetyCashMargin = 300; break;
        case AIDifficulty::BALANCED: safetyCashMargin = 150; break;
        case AIDifficulty::AGGRESSIVE: safetyCashMargin = 50; break;
        case AIDifficulty::MASTER: safetyCashMargin = 100; break;
    }
}

std::optional<Action> AIPlayer::decide(const MonopolyEngine& engine) {
    const GameState& s = engine.getState();
    if (playerId < 0 || playerId >= static_cast<int>(s.players.size())) return std::nullopt;
    const Player& me = s.players[playerId];
    if (me.bankrupt || s.phase == Phase::FINISHED) return std::nullopt;

    Action a;
    a.player = playerId;

    auto ok = [&](const Action& x) { return engine.validate(x).ok; };

    switch (s.phase) {
    case Phase::PURCHASE: {
        if (s.activePlayer != playerId) return std::nullopt;
        // 買えるカードのうち一番高いもの（安全マージンを残せる範囲）
        CardId best = CardId::NONE;
        int bestPrice = -1;
        for (const Card& c : DataMaster::cards()) {
            if (me.money - c.price < safetyCashMargin) continue;
            Action t = a;
            t.kind = ActionKind::BUY;
            t.card = c.id;
            if (c.price > bestPrice && ok(t)) {
                best = c.id;
                bestPrice = c.price;
            }
        }
        if (best != CardId::NONE) {
            a.kind = ActionKind::BUY;
            a.card = best;
        } else {
            a.kind = ActionKind::PASS;
        }
        return a;
    }
    case Phase::NEGOTIATION:
        if (me.ready) return std::nullopt;
        a.kind = ActionKind::READY;
        return a;

    case Phase::OPERATIONAL: {
        if (s.activePlayer != playerId) return std::nullopt;

        // 借金状態: 家を売る → 抵当 → それでも足りなければ破産
        if (me.money < 0) {
            for (CardId c : s.ownedBy(playerId)) {
                Action t = a;
                t.kind = ActionKind::SELL;
                t.card = c;
                if (ok(t)) return t;
            }
            for (CardId c : s.ownedBy(playerId)) {
                Action t = a;
                t.kind = ActionKind::MORTGAGE;
                t.card = c;
                if (ok(t)) return t;
            }
            a.kind = ActionKind::BANKRUPT;
            a.target = s.debtCreditor;
            return a;
        }

        if (s.pendingUtility != CardId::NONE) {
            std::uniform_int_distribution<int> die(1, 6);
            a.kind = ActionKind::UTILITY_ROLL;
            a.d1 = die(rng);
            a.d2 = die(rng);
            return a;
        }
        if (s.pendingPurchase != CardId::NONE) {
            const Card& c = DataMaster::card(s.pendingPurchase);
            a.kind = (me.money - c.price >= safetyCashMargin) ? ActionKind::BUY : ActionKind::DECLINE;
            return a;
        }
        if (!s.hasRolled) {
            std::uniform_int_distribution<int> die(1, 6);
            a.kind = ActionKind::ROLL;
            a.d1 = die(rng);
            a.d2 = die(rng);
            return a;
        }
        // 抵当解除 → 家建設（安全マージンを残せる範囲）
        for (CardId c : s.ownedBy(playerId)) {
            if (!s.card(c).mortgaged) continue;
            if (me.money - MonopolyEngine::unmortgageCost(s, c) < safetyCashMargin * 2) continue;
            Action t = a;
            t.kind = ActionKind::UNMORTGAGE;
            t.card = c;
            if (ok(t)) return t;
        }
        for (CardId c : s.ownedBy(playerId)) {
            if (me.money - DataMaster::card(c).housePrice < safetyCashMargin) continue;
            Action t = a;
            t.kind = ActionKind::BUILD;
            t.card = c;
            if (ok(t)) return t;
        }
        a.kind = ActionKind::END_TURN;
        return a;
    }
    case Phase::FINISHED:
        break;
    }
    return std::nullopt;
}

bool AIPlayer::acceptTrade(const MonopolyEngine& engine, const TradeOffer& offer) const {
    if (offer.targetId != playerId) return false;
    return engine.evaluateTradeFairness(offer) <= 0.0;  // 提案者視点スコアが 0 以下 = 自分が損していない
}
