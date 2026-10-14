#ifndef GAME_STATE_HPP
#define GAME_STATE_HPP

#include "Card.hpp"
#include <array>
#include <string>
#include <vector>

enum class Phase { PURCHASE, NEGOTIATION, OPERATIONAL, FINISHED };

// ゲーム開始時に決める設定（ログに保存され、リプレイ時も同じ値が使われる）
struct Settings {
    int startMoney = 1500;
    int goBonus = 200;              // 0 で GO 手当なし
    bool taxEnabled = true;         // false で TAX マスは BLANK 扱い
    bool doubleRentOnSet = true;    // 家なしコンプリート（独占）時に地代2倍
    int unmortgageInterestPct = 10; // 抵当解除時の利息(%)
    bool doublesRollAgain = true;   // ぞろ目でもう一度振る（旧ログは false 扱い）
    int maxDoubles = 3;             // ぞろ目で振り直せるのは連続この回数まで（3回目のぞろ目は振り直しなし）。0 = 無制限（旧ログ）
    bool debtBlocksEndTurn = true;  // 所持金マイナスのままターン終了できない（旧ログは false 扱い）
    bool endOnBankrupt = true;      // 脱落者が出たらゲーム終了（CONTINUE で続行可）。旧ログは false
    bool utilityRentRoll = true;    // 公共施設は振り直した出目で支払う（旧ログは false = 移動の出目）
};

struct Player {
    int id = 0;
    std::string name;
    int money = 0;
    int position = 0;
    bool bankrupt = false;
    bool ready = false;   // 交渉フェーズの完了フラグ
};

// カードの可変状態（所有者・家・抵当）
struct CardState {
    int owner = -1;       // -1 = 銀行（未所有）
    int houses = 0;       // 0..5 (5 = ホテル)
    bool mortgaged = false;
};

struct GameState {
    Settings settings;
    Phase phase = Phase::PURCHASE;
    Phase resumePhase = Phase::FINISHED;  // 脱落で終了したとき、続行で戻るフェーズ
    int activePlayer = 0;
    int turn = 0;                 // 運用フェーズ開始時に 1。全員が1回ずつ振ると +1
    int consecutivePasses = 0;    // 購入フェーズ: 連続パス数（全員連続パスで終了）
    bool hasRolled = false;       // 運用フェーズ: 手番プレイヤーが振ったか
    int lastDice1 = 0;
    int lastDice2 = 0;
    int doublesStreak = 0;        // このターンの連続ぞろ目回数（>0 ならもう一度振れる状態）
    CardId pendingUtility = CardId::NONE;   // 他人の公共施設に止まった → 支払い用に振るのを待っている
    CardId pendingPurchase = CardId::NONE;  // 運用中に止まった未所有カード（買う/買わない/オークション待ち）
    int debtCreditor = -1;        // 直近の支払い先（破産時の既定債権者）。-1 = 銀行
    std::vector<Player> players;
    std::array<CardState, CARD_COUNT> cards{};

    const CardState& card(CardId id) const { return cards[cardIndex(id)]; }
    CardState& card(CardId id) { return cards[cardIndex(id)]; }

    std::vector<CardId> ownedBy(int playerId) const {
        std::vector<CardId> out;
        for (int i = 0; i < CARD_COUNT; ++i)
            if (cards[i].owner == playerId) out.push_back(cardFromIndex(i));
        return out;
    }

    int aliveCount() const {
        int n = 0;
        for (const auto& p : players) if (!p.bankrupt) ++n;
        return n;
    }
};

// 状態比較（リプレイ一致テスト用）
inline bool operator==(const Settings& a, const Settings& b) {
    return a.startMoney == b.startMoney && a.goBonus == b.goBonus && a.taxEnabled == b.taxEnabled &&
           a.doubleRentOnSet == b.doubleRentOnSet && a.unmortgageInterestPct == b.unmortgageInterestPct &&
           a.doublesRollAgain == b.doublesRollAgain && a.maxDoubles == b.maxDoubles && a.debtBlocksEndTurn == b.debtBlocksEndTurn &&
           a.utilityRentRoll == b.utilityRentRoll && a.endOnBankrupt == b.endOnBankrupt;
}
inline bool operator==(const Player& a, const Player& b) {
    return a.id == b.id && a.name == b.name && a.money == b.money && a.position == b.position &&
           a.bankrupt == b.bankrupt && a.ready == b.ready;
}
inline bool operator==(const CardState& a, const CardState& b) {
    return a.owner == b.owner && a.houses == b.houses && a.mortgaged == b.mortgaged;
}
inline bool operator==(const GameState& a, const GameState& b) {
    return a.settings == b.settings && a.phase == b.phase && a.resumePhase == b.resumePhase && a.activePlayer == b.activePlayer &&
           a.turn == b.turn && a.consecutivePasses == b.consecutivePasses && a.hasRolled == b.hasRolled &&
           a.lastDice1 == b.lastDice1 && a.lastDice2 == b.lastDice2 && a.doublesStreak == b.doublesStreak &&
           a.pendingPurchase == b.pendingPurchase && a.pendingUtility == b.pendingUtility && a.debtCreditor == b.debtCreditor &&
           a.players == b.players && a.cards == b.cards;
}
inline bool operator!=(const GameState& a, const GameState& b) { return !(a == b); }

#endif
