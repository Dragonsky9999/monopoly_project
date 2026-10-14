#include "MonopolyEngine.hpp"
#include <algorithm>
#include <set>

namespace {

using DataMaster::BOARD_SIZE;
using DataMaster::SquareType;

bool validPlayer(const GameState& s, int p) { return p >= 0 && p < static_cast<int>(s.players.size()); }
bool alivePlayer(const GameState& s, int p) { return validPlayer(s, p) && !s.players[p].bankrupt; }
bool validCard(CardId c) { return cardIndex(c) >= 0 && cardIndex(c) < CARD_COUNT; }

// from の次の生存プレイヤー（from = -1 で先頭の生存者）
int nextAlive(const GameState& s, int from) {
    int n = static_cast<int>(s.players.size());
    for (int k = 1; k <= n; ++k) {
        int i = ((from + k) % n + n) % n;
        if (!s.players[i].bankrupt) return i;
    }
    return from;
}

std::string pname(const GameState& s, int p) { return validPlayer(s, p) ? s.players[p].name : "Bank"; }

}  // namespace

// ============================================================================
// 初期化・履歴
// ============================================================================

MonopolyEngine::MonopolyEngine() { newGame({"Player_1", "Player_2"}); }

void MonopolyEngine::newGame(const std::vector<std::string>& playerNames, const Settings& settings) {
    state = GameState();
    state.settings = settings;
    for (int i = 0; i < static_cast<int>(playerNames.size()); ++i) {
        Player p;
        p.id = i;
        p.name = playerNames[i];
        p.money = settings.startMoney;
        state.players.push_back(p);
    }
    initialState = state;
    snapshots.clear();
    actions.clear();
    eventsPerAction.clear();
    redoStack.clear();
}

ApplyResult MonopolyEngine::validate(const Action& action) const {
    GameState copy = state;
    std::vector<GameEvent> ev;
    std::string err = execute(action, copy, ev);
    return {err.empty(), err};
}

ApplyResult MonopolyEngine::apply(const Action& action) { return applyInternal(action, true); }

ApplyResult MonopolyEngine::applyInternal(const Action& action, bool clearRedo) {
    GameState next = state;
    std::vector<GameEvent> ev;
    std::string err = execute(action, next, ev);
    if (!err.empty()) return {false, err};

    // イベントの共通項目を埋める
    int baseId = 1;
    for (const auto& list : eventsPerAction) baseId += static_cast<int>(list.size());
    for (size_t i = 0; i < ev.size(); ++i) {
        GameEvent& e = ev[i];
        e.eventId = baseId + static_cast<int>(i);
        e.actionIndex = static_cast<int>(actions.size());
        if (e.turn == 0) e.turn = next.turn;
    }

    snapshots.push_back(state);
    actions.push_back(action);
    eventsPerAction.push_back(std::move(ev));
    state = std::move(next);
    if (clearRedo) redoStack.clear();
    return {true, ""};
}

ApplyResult MonopolyEngine::replaceLast(const Action& action) {
    if (actions.empty()) return {false, "nothing to replace"};
    if (!redoStack.empty()) return {false, "cannot replace while viewing the past"};
    undo();
    ApplyResult r = apply(action);
    if (!r.ok) redo();  // 元に戻す
    return r;
}

bool MonopolyEngine::undo() {
    if (snapshots.empty()) return false;
    state = snapshots.back();
    snapshots.pop_back();
    redoStack.push_back(actions.back());
    actions.pop_back();
    eventsPerAction.pop_back();
    return true;
}

bool MonopolyEngine::redo() {
    if (redoStack.empty()) return false;
    Action a = redoStack.back();
    ApplyResult r = applyInternal(a, false);
    if (!r.ok) return false;  // 通常は起こらない（記録時に成功した操作の再適用）
    redoStack.pop_back();
    return true;
}

std::vector<GameEvent> MonopolyEngine::getEvents() const {
    std::vector<GameEvent> all;
    for (const auto& list : eventsPerAction) all.insert(all.end(), list.begin(), list.end());
    return all;
}

const GameState& MonopolyEngine::getStateBeforeAction(size_t actionIndex) const {
    if (actionIndex < snapshots.size()) return snapshots[actionIndex];
    return state;
}

CardId MonopolyEngine::getCardIdAtPosition(int position) const {
    if (position < 0 || position >= BOARD_SIZE) return CardId::NONE;
    return DataMaster::board()[position].card;
}

// ============================================================================
// ルール計算
// ============================================================================

bool MonopolyEngine::groupCompleted(const GameState& s, int playerId, GroupType group) {
    if (group == GroupType::RAILROAD || group == GroupType::UTILITY) return false;
    for (CardId c : DataMaster::groupCards(group))
        if (s.card(c).owner != playerId) return false;
    return true;
}

int MonopolyEngine::rentOf(const GameState& s, CardId cardId, int diceSum) {
    const Card& card = DataMaster::card(cardId);
    const CardState& cs = s.card(cardId);
    if (cs.owner < 0 || cs.mortgaged) return 0;

    switch (card.kind) {
        case CardKind::PROPERTY: {
            if (cs.houses > 0) return card.rent[std::min(cs.houses, 5)];
            int base = card.rent[0];
            if (s.settings.doubleRentOnSet && groupCompleted(s, cs.owner, card.group)) base *= 2;
            return base;
        }
        case CardKind::RAILROAD: {
            int n = 0;
            for (CardId c : DataMaster::groupCards(GroupType::RAILROAD))
                if (s.card(c).owner == cs.owner) ++n;
            return card.rent[std::max(1, std::min(n, 4)) - 1];
        }
        case CardKind::UTILITY: {
            int n = 0;
            for (CardId c : DataMaster::groupCards(GroupType::UTILITY))
                if (s.card(c).owner == cs.owner) ++n;
            return diceSum * (n >= 2 ? card.rent[1] : card.rent[0]);
        }
    }
    return 0;
}

int MonopolyEngine::liquidValue(const GameState& s, int playerId) {
    if (!validPlayer(s, playerId) || s.players[playerId].bankrupt) return 0;
    int total = s.players[playerId].money;
    for (int i = 0; i < CARD_COUNT; ++i) {
        const CardState& cs = s.cards[i];
        if (cs.owner != playerId) continue;
        const Card& c = DataMaster::cards()[i];
        total += cs.houses * (c.housePrice / 2);
        if (!cs.mortgaged) total += c.mortgage;
    }
    return total;
}

int MonopolyEngine::unmortgageCost(const GameState& s, CardId cardId) {
    int m = DataMaster::card(cardId).mortgage;
    int pct = s.settings.unmortgageInterestPct;
    return (m * (100 + pct) + 99) / 100;  // 切り上げ
}

// ============================================================================
// アクション実行（ここがルールの本体）
// ============================================================================

std::string MonopolyEngine::execute(const Action& a, GameState& s, std::vector<GameEvent>& ev) {
    auto emit = [&](ActionType t, int player) -> GameEvent& {
        GameEvent e;
        e.type = t;
        e.playerId = player;
        e.turn = s.turn;
        // その時点の所持金（お金を動かしてから emit するので「このイベント直後」の値になる）
        if (validPlayer(s, player)) {
            e.playerMoneyAfter = s.players[player].money;
            e.totalAssetAfter = liquidValue(s, player);
        }
        ev.push_back(e);
        return ev.back();
    };
    auto groupHasHouses = [&](GroupType g) {
        if (g == GroupType::RAILROAD || g == GroupType::UTILITY) return false;
        for (CardId c : DataMaster::groupCards(g))
            if (s.card(c).houses > 0) return true;
        return false;
    };
    auto checkGameOver = [&]() {
        if (s.phase != Phase::FINISHED && s.aliveCount() <= 1) {
            s.phase = Phase::FINISHED;
            s.pendingPurchase = CardId::NONE;
            s.pendingUtility = CardId::NONE;
            GameEvent& e = emit(ActionType::GAME_OVER, nextAlive(s, -1));
            (void)e;
        }
    };
    auto startOperational = [&]() {
        s.phase = Phase::OPERATIONAL;
        s.activePlayer = nextAlive(s, -1);
        s.turn = 1;
        s.hasRolled = false;
        s.doublesStreak = 0;
        s.pendingPurchase = CardId::NONE;
        emit(ActionType::PHASE_CHANGE, -1).turn = 1;
    };

    const int p = a.player;

    switch (a.kind) {
    // ------------------------------------------------------------------ BUY
    case ActionKind::BUY: {
        if (!alivePlayer(s, p)) return "invalid player";
        if (s.phase == Phase::PURCHASE) {
            if (p != s.activePlayer) return "not " + pname(s, p) + "'s turn (active: " + pname(s, s.activePlayer) + ")";
            if (!validCard(a.card)) return "invalid card";
            const Card& c = DataMaster::card(a.card);
            if (s.card(a.card).owner != -1) return c.code + " is already owned by " + pname(s, s.card(a.card).owner);
            for (CardId owned : s.ownedBy(p))
                if (DataMaster::card(owned).group == c.group)
                    return "already owns a card in group " + DataMaster::groupName(c.group) + " (" + DataMaster::toString(owned) + ")";
            if (s.players[p].money < c.price) return "not enough money (" + std::to_string(c.price) + " needed)";
            s.players[p].money -= c.price;
            s.card(a.card).owner = p;
            s.consecutivePasses = 0;
            GameEvent& e = emit(ActionType::PURCHASE, p);
            e.moneyChange = -c.price;
            e.cardsGained.push_back(a.card);
            s.activePlayer = nextAlive(s, s.activePlayer);
            return "";
        }
        if (s.phase == Phase::OPERATIONAL) {
            if (p != s.activePlayer) return "not " + pname(s, p) + "'s turn";
            if (s.pendingPurchase == CardId::NONE) return "no unowned card to buy (must land on it)";
            if (a.card != CardId::NONE && a.card != s.pendingPurchase)
                return "can only buy the landed card " + DataMaster::toString(s.pendingPurchase);
            CardId cid = s.pendingPurchase;
            const Card& c = DataMaster::card(cid);
            if (s.players[p].money < c.price) return "not enough money (" + std::to_string(c.price) + " needed)";
            s.players[p].money -= c.price;
            s.card(cid).owner = p;
            s.pendingPurchase = CardId::NONE;
            GameEvent& e = emit(ActionType::PURCHASE, p);
            e.moneyChange = -c.price;
            e.cardsGained.push_back(cid);
            return "";
        }
        return "cannot buy in this phase";
    }
    // ----------------------------------------------------------------- PASS
    case ActionKind::PASS: {
        if (s.phase != Phase::PURCHASE) return "PASS is only for the purchase phase (use END to end a turn)";
        if (p != s.activePlayer) return "not " + pname(s, p) + "'s turn (active: " + pname(s, s.activePlayer) + ")";
        s.consecutivePasses++;
        emit(ActionType::PASS, p);
        if (s.consecutivePasses >= s.aliveCount()) {
            s.phase = Phase::NEGOTIATION;
            s.activePlayer = nextAlive(s, -1);
            for (auto& pl : s.players) pl.ready = false;
            emit(ActionType::PHASE_CHANGE, -1);
        } else {
            s.activePlayer = nextAlive(s, s.activePlayer);
        }
        return "";
    }
    // ---------------------------------------------------------------- READY
    case ActionKind::READY: {
        if (s.phase != Phase::NEGOTIATION) return "READY is only for the negotiation phase";
        if (!alivePlayer(s, p)) return "invalid player";
        if (s.players[p].ready) return pname(s, p) + " is already ready";
        s.players[p].ready = true;
        emit(ActionType::READY, p);
        bool all = true;
        for (const auto& pl : s.players)
            if (!pl.bankrupt && !pl.ready) all = false;
        if (all) startOperational();
        return "";
    }
    case ActionKind::ALL_READY: {
        if (s.phase != Phase::NEGOTIATION) return "only in the negotiation phase";
        for (auto& pl : s.players)
            if (!pl.bankrupt) pl.ready = true;
        emit(ActionType::READY, -1);
        startOperational();
        return "";
    }
    case ActionKind::CONTINUE: {
        if (s.phase != Phase::FINISHED || s.resumePhase == Phase::FINISHED) return "nothing to continue";
        if (s.aliveCount() < 2) return "only one player left";
        s.phase = s.resumePhase;
        s.resumePhase = Phase::FINISHED;
        emit(ActionType::PHASE_CHANGE, -1);
        return "";
    }
    // ----------------------------------------------------------------- ROLL
    case ActionKind::ROLL: {
        if (s.phase != Phase::OPERATIONAL) return "can only roll in the operational phase";
        if (p != s.activePlayer) return "not " + pname(s, p) + "'s turn (active: " + pname(s, s.activePlayer) + ")";
        if (s.hasRolled) return "already rolled this turn (use END)";
        if (s.pendingPurchase != CardId::NONE)
            return "decide on " + DataMaster::toString(s.pendingPurchase) + " first (buy / decline / auction)";
        if (s.pendingUtility != CardId::NONE) return "roll for the utility rent first";
        if (a.d1 < 1 || a.d1 > 6 || a.d2 < 1 || a.d2 > 6) return "dice must be 1-6";

        Player& pl = s.players[p];
        int sum = a.d1 + a.d2;
        int oldPos = pl.position;
        int newPos = (oldPos + sum) % BOARD_SIZE;
        pl.position = newPos;
        s.hasRolled = true;
        s.lastDice1 = a.d1;
        s.lastDice2 = a.d2;
        s.debtCreditor = -1;
        // ぞろ目: もう一度振れる（hasRolled を戻す）。連続 maxDoubles 回目のぞろ目は振り直しなし
        if (s.settings.doublesRollAgain && a.d1 == a.d2) {
            s.doublesStreak++;
            if (s.settings.maxDoubles <= 0 || s.doublesStreak < s.settings.maxDoubles) s.hasRolled = false;
        } else {
            s.doublesStreak = 0;
        }

        GameEvent& mv = emit(ActionType::ROLL_AND_MOVE, p);
        mv.dice1 = a.d1;
        mv.dice2 = a.d2;
        mv.position = newPos;

        if (newPos < oldPos && s.settings.goBonus > 0) {  // GO 通過 or 停止
            pl.money += s.settings.goBonus;
            emit(ActionType::GO_BONUS, p).moneyChange = s.settings.goBonus;
        }

        const auto& sq = DataMaster::board()[newPos];
        if (sq.type == SquareType::TAX && s.settings.taxEnabled) {
            pl.money -= sq.tax;
            GameEvent& e = emit(ActionType::PAY_TAX, p);
            e.moneyChange = -sq.tax;
            e.position = newPos;
        } else if (sq.type == SquareType::CARD) {
            CardState& cs = s.card(sq.card);
            if (cs.owner == -1) {
                s.pendingPurchase = sq.card;
                GameEvent& e = emit(ActionType::LAND_UNOWNED, p);
                e.cardsGained.push_back(sq.card);
                e.position = newPos;
            } else if (cs.owner != p && s.settings.utilityRentRoll && DataMaster::card(sq.card).kind == CardKind::UTILITY &&
                       !cs.mortgaged) {
                s.pendingUtility = sq.card;  // 支払い額はもう一度振った出目で決まる
            } else if (cs.owner != p) {
                int rent = rentOf(s, sq.card, sum);
                if (rent > 0) {
                    pl.money -= rent;
                    s.players[cs.owner].money += rent;
                    s.debtCreditor = cs.owner;
                    GameEvent& e = emit(ActionType::PAY_RENT, p);
                    e.targetPlayerId = cs.owner;
                    e.moneyChange = -rent;
                    e.cardsLost.push_back(sq.card);  // 対象カード（表示用）
                    e.position = newPos;
                }
            }
        }
        return "";
    }
    // --------------------------------------------------------- UTILITY_ROLL
    case ActionKind::UTILITY_ROLL: {
        if (s.phase != Phase::OPERATIONAL) return "not in operational phase";
        if (p != s.activePlayer) return "not " + pname(s, p) + "'s turn";
        if (s.pendingUtility == CardId::NONE) return "no utility rent to roll for";
        if (a.d1 < 1 || a.d1 > 6 || a.d2 < 1 || a.d2 > 6) return "dice must be 1-6";
        const CardId cid = s.pendingUtility;
        s.pendingUtility = CardId::NONE;
        const CardState& cs = s.card(cid);
        const int rent = (cs.owner >= 0 && cs.owner != p) ? rentOf(s, cid, a.d1 + a.d2) : 0;
        if (rent > 0) {
            s.players[p].money -= rent;
            s.players[cs.owner].money += rent;
            s.debtCreditor = cs.owner;
            GameEvent& e = emit(ActionType::PAY_RENT, p);
            e.targetPlayerId = cs.owner;
            e.moneyChange = -rent;
            e.cardsLost.push_back(cid);
            e.dice1 = a.d1;
            e.dice2 = a.d2;
            e.position = s.players[p].position;
        }
        return "";
    }
    // -------------------------------------------------------------- DECLINE
    case ActionKind::DECLINE: {
        if (s.phase != Phase::OPERATIONAL) return "not in operational phase";
        if (p != s.activePlayer) return "not " + pname(s, p) + "'s turn";
        if (s.pendingPurchase == CardId::NONE) return "nothing to decline";
        GameEvent& e = emit(ActionType::DECLINE, p);
        e.cardsLost.push_back(s.pendingPurchase);
        s.pendingPurchase = CardId::NONE;
        return "";
    }
    // -------------------------------------------------------------- AUCTION
    case ActionKind::AUCTION: {
        if (s.phase != Phase::OPERATIONAL) return "not in operational phase";
        if (p != s.activePlayer) return "not " + pname(s, p) + "'s turn";
        if (s.pendingPurchase == CardId::NONE) return "no card to auction";
        if (a.card != CardId::NONE && a.card != s.pendingPurchase)
            return "auction card must be " + DataMaster::toString(s.pendingPurchase);
        CardId cid = s.pendingPurchase;
        if (a.target == -1) {  // 落札者なし
            GameEvent& e = emit(ActionType::AUCTION, -1);
            e.cardsLost.push_back(cid);
            s.pendingPurchase = CardId::NONE;
            return "";
        }
        if (!alivePlayer(s, a.target)) return "invalid winner";
        if (a.amount < 0) return "price must be >= 0";
        if (s.players[a.target].money < a.amount) return pname(s, a.target) + " does not have " + std::to_string(a.amount);
        s.players[a.target].money -= a.amount;
        s.card(cid).owner = a.target;
        s.pendingPurchase = CardId::NONE;
        GameEvent& e = emit(ActionType::AUCTION, a.target);
        e.moneyChange = -a.amount;
        e.cardsGained.push_back(cid);
        return "";
    }
    // ------------------------------------------------------------- END_TURN
    case ActionKind::END_TURN: {
        if (s.phase != Phase::OPERATIONAL) return "not in operational phase";
        if (p != s.activePlayer) return "not " + pname(s, p) + "'s turn";
        if (!s.hasRolled) return s.doublesStreak > 0 ? "doubles: roll again" : "roll first";
        if (s.pendingPurchase != CardId::NONE)
            return "decide on " + DataMaster::toString(s.pendingPurchase) + " first (buy / decline / auction)";
        if (s.pendingUtility != CardId::NONE) return "roll for the utility rent first";
        if (s.settings.debtBlocksEndTurn && s.players[p].money < 0)
            return "money is negative: sell houses / mortgage / trade to pay, or declare bankruptcy";
        emit(ActionType::END_TURN, p);
        int nxt = nextAlive(s, s.activePlayer);
        if (nxt <= s.activePlayer) s.turn++;
        s.activePlayer = nxt;
        s.hasRolled = false;
        s.doublesStreak = 0;
        s.debtCreditor = -1;
        return "";
    }
    // ---------------------------------------------------------------- BUILD
    case ActionKind::BUILD: {
        if (s.phase != Phase::OPERATIONAL) return "can only build in the operational phase";
        if (!alivePlayer(s, p)) return "invalid player";
        if (!validCard(a.card)) return "invalid card";
        const Card& c = DataMaster::card(a.card);
        CardState& cs = s.card(a.card);
        if (c.kind != CardKind::PROPERTY) return "cannot build on " + c.code;
        if (cs.owner != p) return pname(s, p) + " does not own " + c.code;
        if (!groupCompleted(s, p, c.group)) return "group " + DataMaster::groupName(c.group) + " is not complete";
        int minH = 99;
        for (CardId g : DataMaster::groupCards(c.group)) {
            if (s.card(g).mortgaged) return DataMaster::toString(g) + " is mortgaged";
            minH = std::min(minH, s.card(g).houses);
        }
        if (cs.houses >= 5) return c.code + " already has a hotel";
        if (cs.houses > minH) return "must build evenly (other cards in group have fewer houses)";
        if (s.players[p].money < c.housePrice) return "not enough money (" + std::to_string(c.housePrice) + " needed)";
        s.players[p].money -= c.housePrice;
        cs.houses++;
        GameEvent& e = emit(ActionType::BUILD_HOUSE, p);
        e.moneyChange = -c.housePrice;
        e.cardsGained.push_back(a.card);
        return "";
    }
    // ----------------------------------------------------------------- SELL
    case ActionKind::SELL: {
        if (s.phase != Phase::OPERATIONAL) return "can only sell houses in the operational phase";
        if (!alivePlayer(s, p)) return "invalid player";
        if (!validCard(a.card)) return "invalid card";
        const Card& c = DataMaster::card(a.card);
        CardState& cs = s.card(a.card);
        if (cs.owner != p) return pname(s, p) + " does not own " + c.code;
        if (cs.houses <= 0) return c.code + " has no houses";
        for (CardId g : DataMaster::groupCards(c.group))
            if (s.card(g).houses > cs.houses) return "must sell evenly (sell from cards with more houses first)";
        int refund = c.housePrice / 2;
        s.players[p].money += refund;
        cs.houses--;
        GameEvent& e = emit(ActionType::SELL_HOUSE, p);
        e.moneyChange = refund;
        e.cardsLost.push_back(a.card);
        return "";
    }
    // ------------------------------------------------------------- MORTGAGE
    case ActionKind::MORTGAGE: {
        if (s.phase != Phase::OPERATIONAL && s.phase != Phase::NEGOTIATION) return "cannot mortgage in this phase";
        if (!alivePlayer(s, p)) return "invalid player";
        if (!validCard(a.card)) return "invalid card";
        const Card& c = DataMaster::card(a.card);
        CardState& cs = s.card(a.card);
        if (cs.owner != p) return pname(s, p) + " does not own " + c.code;
        if (cs.mortgaged) return c.code + " is already mortgaged";
        if (groupHasHouses(c.group)) return "sell all houses in group " + DataMaster::groupName(c.group) + " first";
        cs.mortgaged = true;
        s.players[p].money += c.mortgage;
        GameEvent& e = emit(ActionType::MORTGAGE, p);
        e.moneyChange = c.mortgage;
        e.cardsLost.push_back(a.card);
        return "";
    }
    case ActionKind::UNMORTGAGE: {
        if (s.phase != Phase::OPERATIONAL && s.phase != Phase::NEGOTIATION) return "cannot unmortgage in this phase";
        if (!alivePlayer(s, p)) return "invalid player";
        if (!validCard(a.card)) return "invalid card";
        const Card& c = DataMaster::card(a.card);
        CardState& cs = s.card(a.card);
        if (cs.owner != p) return pname(s, p) + " does not own " + c.code;
        if (!cs.mortgaged) return c.code + " is not mortgaged";
        int cost = unmortgageCost(s, a.card);
        if (s.players[p].money < cost) return "not enough money (" + std::to_string(cost) + " needed)";
        cs.mortgaged = false;
        s.players[p].money -= cost;
        GameEvent& e = emit(ActionType::UNMORTGAGE, p);
        e.moneyChange = -cost;
        e.cardsGained.push_back(a.card);
        return "";
    }
    // ---------------------------------------------------------------- TRADE
    case ActionKind::TRADE:
    case ActionKind::TRADE_CANCEL: {
        const TradeOffer& t = a.trade;
        if (s.phase != Phase::NEGOTIATION && s.phase != Phase::OPERATIONAL) return "cannot trade in this phase";
        if (t.proposerId != p || t.targetId != a.target) return "internal: trade players mismatch";
        if (!alivePlayer(s, p) || !alivePlayer(s, a.target)) return "invalid player";
        if (p == a.target) return "cannot trade with yourself";
        if (t.offerMoney < 0 || t.requestMoney < 0) return "money must be >= 0";
        if (t.offerCards.empty() && t.requestCards.empty() && t.offerMoney == 0 && t.requestMoney == 0)
            return "nothing to trade (select cards or money)";

        if (a.kind == ActionKind::TRADE_CANCEL) {  // 破談: 記録だけ（状態は変えない）
            GameEvent& e = emit(ActionType::TRADE_CANCEL, p);
            e.targetPlayerId = a.target;
            e.cardsGained = t.requestCards;
            e.cardsLost = t.offerCards;
            return "";
        }

        std::set<int> seen;
        for (CardId c : t.offerCards) {
            if (!validCard(c)) return "invalid card";
            if (!seen.insert(cardIndex(c)).second) return "duplicate card " + DataMaster::toString(c);
            if (s.card(c).owner != p) return pname(s, p) + " does not own " + DataMaster::toString(c);
        }
        for (CardId c : t.requestCards) {
            if (!validCard(c)) return "invalid card";
            if (!seen.insert(cardIndex(c)).second) return "duplicate card " + DataMaster::toString(c);
            if (s.card(c).owner != a.target) return pname(s, a.target) + " does not own " + DataMaster::toString(c);
        }
        for (int ci : seen)
            if (groupHasHouses(DataMaster::cards()[ci].group))
                return "sell houses in group " + DataMaster::groupName(DataMaster::cards()[ci].group) + " before trading";
        if (s.players[p].money < t.offerMoney) return pname(s, p) + " does not have " + std::to_string(t.offerMoney);
        if (s.players[a.target].money < t.requestMoney) return pname(s, a.target) + " does not have " + std::to_string(t.requestMoney);

        s.players[p].money += t.requestMoney - t.offerMoney;
        s.players[a.target].money += t.offerMoney - t.requestMoney;
        for (CardId c : t.offerCards) s.card(c).owner = a.target;      // 抵当状態はそのまま移る
        for (CardId c : t.requestCards) s.card(c).owner = p;

        GameEvent& e = emit(ActionType::TRADE_EXECUTE, p);
        e.targetPlayerId = a.target;
        e.moneyChange = t.requestMoney - t.offerMoney;
        e.cardsGained = t.requestCards;
        e.cardsLost = t.offerCards;
        return "";
    }
    // ------------------------------------------------------------- BANKRUPT
    case ActionKind::BANKRUPT: {
        if (s.phase == Phase::FINISHED) return "game is over";
        if (!alivePlayer(s, p)) return "invalid player";
        int cred = a.target;
        if (cred != -1 && (!alivePlayer(s, cred) || cred == p)) return "invalid creditor";

        Player& pl = s.players[p];
        // 家は銀行に半額で売却 → 現金化
        int cash = pl.money;
        std::vector<CardId> owned = s.ownedBy(p);
        for (CardId c : owned) {
            cash += s.card(c).houses * (DataMaster::card(c).housePrice / 2);
            s.card(c).houses = 0;
        }
        if (cred >= 0) {
            // 債権者は支払い時に満額を受け取っているので、払えなかった分（cash < 0）は返してもらう
            if (cash > 0 || s.settings.debtBlocksEndTurn) s.players[cred].money += cash;
            for (CardId c : owned) s.card(c).owner = cred;  // 抵当状態のまま移る
        } else {
            for (CardId c : owned) {
                s.card(c).owner = -1;
                s.card(c).mortgaged = false;
            }
        }
        pl.money = 0;
        pl.bankrupt = true;
        pl.ready = false;

        GameEvent& e = emit(ActionType::BANKRUPT, p);
        e.targetPlayerId = cred;
        e.cardsLost = owned;
        e.moneyChange = -std::max(0, cash);

        if (s.aliveCount() <= 1) {
            checkGameOver();
            return "";
        }
        if (s.phase == Phase::PURCHASE) {
            if (s.activePlayer == p) s.activePlayer = nextAlive(s, p);
            if (s.consecutivePasses >= s.aliveCount()) {
                s.phase = Phase::NEGOTIATION;
                s.activePlayer = nextAlive(s, -1);
                for (auto& x : s.players) x.ready = false;
                emit(ActionType::PHASE_CHANGE, -1);
            }
        } else if (s.phase == Phase::NEGOTIATION) {
            bool all = true;
            for (const auto& x : s.players)
                if (!x.bankrupt && !x.ready) all = false;
            if (all) startOperational();
        } else if (s.phase == Phase::OPERATIONAL && s.activePlayer == p) {
            int nxt = nextAlive(s, p);
            if (nxt <= p) s.turn++;
            s.activePlayer = nxt;
            s.hasRolled = false;
            s.doublesStreak = 0;
            s.pendingPurchase = CardId::NONE;
            s.pendingUtility = CardId::NONE;
            s.debtCreditor = -1;
        }
        if (s.settings.endOnBankrupt && s.phase != Phase::FINISHED) {
            // 脱落者が出たらゲーム終了（CONTINUE で続けられる）。勝者は換金可能額がいちばん多い人
            s.resumePhase = s.phase;
            s.phase = Phase::FINISHED;
            int best = -1;
            for (const auto& x : s.players)
                if (!x.bankrupt && (best < 0 || liquidValue(s, x.id) > liquidValue(s, best))) best = x.id;
            emit(ActionType::GAME_OVER, best);
        }
        return "";
    }
    // ------------------------------------------------------------ OVERRIDES
    case ActionKind::SET_MONEY: {
        if (!validPlayer(s, p)) return "invalid player";
        GameEvent& e = emit(ActionType::OVERRIDE, p);
        e.moneyChange = a.amount - s.players[p].money;
        s.players[p].money = a.amount;
        return "";
    }
    case ActionKind::SET_POS: {
        if (!validPlayer(s, p)) return "invalid player";
        if (a.amount < 0 || a.amount >= BOARD_SIZE) return "position must be 0-39";
        s.players[p].position = a.amount;
        emit(ActionType::OVERRIDE, p).position = a.amount;
        return "";
    }
    case ActionKind::SET_OWNER: {
        if (!validCard(a.card)) return "invalid card";
        if (a.target != -1 && !validPlayer(s, a.target)) return "invalid owner";
        CardState& cs = s.card(a.card);
        int old = cs.owner;
        cs.owner = a.target;
        if (a.target == -1) {
            cs.houses = 0;
            cs.mortgaged = false;
        }
        if (s.pendingPurchase == a.card && a.target != -1) s.pendingPurchase = CardId::NONE;
        GameEvent& e = emit(ActionType::OVERRIDE, a.target);
        e.targetPlayerId = old;
        e.cardsGained.push_back(a.card);
        return "";
    }
    case ActionKind::SET_HOUSES: {
        if (!validCard(a.card)) return "invalid card";
        if (DataMaster::card(a.card).kind != CardKind::PROPERTY) return "not a property";
        if (a.amount < 0 || a.amount > 5) return "houses must be 0-5";
        s.card(a.card).houses = a.amount;
        GameEvent& e = emit(ActionType::OVERRIDE, s.card(a.card).owner);
        e.cardsGained.push_back(a.card);
        return "";
    }
    }
    return "unknown action";
}

// ============================================================================
// 分析（簡易版: 着地率は一律 1/40）
// ============================================================================

double MonopolyEngine::calculateExpectedIncomePerTurn(int playerId) const {
    if (!alivePlayer(state, playerId)) return 0.0;
    int opponents = state.aliveCount() - 1;
    if (opponents <= 0) return 0.0;
    const double landProb = 1.0 / 40.0;
    double income = 0.0;
    for (CardId c : state.ownedBy(playerId)) income += rentOf(state, c, 7) * landProb * opponents;
    double goBonus = (7.0 / 40.0) * state.settings.goBonus;  // 平均出目7 → 約 7/40 の確率で GO 通過
    return income + goBonus;
}

double MonopolyEngine::evaluateTradeFairness(const TradeOffer& offer) const {
    Action a;
    a.kind = ActionKind::TRADE;
    a.player = offer.proposerId;
    a.target = offer.targetId;
    a.trade = offer;

    GameState sim = state;
    std::vector<GameEvent> ev;
    if (!execute(a, sim, ev).empty()) return 0.0;

    MonopolyEngine after;
    after.state = sim;
    double dP = after.calculateExpectedIncomePerTurn(offer.proposerId) - calculateExpectedIncomePerTurn(offer.proposerId);
    double dT = after.calculateExpectedIncomePerTurn(offer.targetId) - calculateExpectedIncomePerTurn(offer.targetId);
    // カードの帳簿価値（抵当中は 価格 − 解除費用）
    auto book = [&](CardId c) {
        const Card& cd = DataMaster::card(c);
        return state.card(c).mortgaged ? cd.price - unmortgageCost(state, c) : cd.price;
    };
    double cards = 0;
    for (CardId c : offer.requestCards) cards += book(c);
    for (CardId c : offer.offerCards) cards -= book(c);
    const double money = offer.requestMoney - offer.offerMoney;
    // 提案者視点のスコア（万円、正: 提案者有利）
    //   提案者の得 = お金 + カード価値 + 10ターン分の期待収入の変化。相手の得との差の半分
    constexpr double kHorizon = 10.0;
    return money + cards + kHorizon * (dP - dT) / 2.0;
}

// ============================================================================
// ActionKind <-> 文字列
// ============================================================================

namespace {
const std::pair<ActionKind, const char*> kKindNames[] = {
    {ActionKind::BUY, "BUY"},           {ActionKind::PASS, "PASS"},
    {ActionKind::READY, "READY"},       {ActionKind::ROLL, "ROLL"},
    {ActionKind::DECLINE, "DECLINE"},   {ActionKind::AUCTION, "AUCTION"},
    {ActionKind::END_TURN, "END"},      {ActionKind::BUILD, "BUILD"},
    {ActionKind::SELL, "SELL"},         {ActionKind::MORTGAGE, "MORTGAGE"},
    {ActionKind::UNMORTGAGE, "UNMORTGAGE"}, {ActionKind::TRADE, "TRADE"},
    {ActionKind::TRADE_CANCEL, "TRADE_CANCEL"}, {ActionKind::BANKRUPT, "BANKRUPT"},
    {ActionKind::SET_MONEY, "SET_MONEY"}, {ActionKind::SET_OWNER, "SET_OWNER"},
    {ActionKind::SET_HOUSES, "SET_HOUSES"}, {ActionKind::SET_POS, "SET_POS"},
    {ActionKind::UTILITY_ROLL, "UROLL"}, {ActionKind::ALL_READY, "ALLREADY"},
    {ActionKind::CONTINUE, "CONTINUE"},
};
}

std::string actionKindToString(ActionKind k) {
    for (const auto& kv : kKindNames)
        if (kv.first == k) return kv.second;
    return "?";
}

bool actionKindFromString(const std::string& s, ActionKind& out) {
    for (const auto& kv : kKindNames)
        if (s == kv.second) {
            out = kv.first;
            return true;
        }
    return false;
}
