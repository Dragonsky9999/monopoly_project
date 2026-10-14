#ifndef ACTION_HPP
#define ACTION_HPP

#include "Card.hpp"
#include "TradeOffer.hpp"
#include <string>

// プレイヤー（または運営）の「入力」。ログにはこれだけを保存し、
// 家賃・GO手当・税などの結果はリプレイ時にエンジンが再計算する。
enum class ActionKind {
    BUY,          // 購入フェーズ: 任意の未所有カード / 運用フェーズ: 止まったカード
    PASS,         // 購入フェーズのパス
    READY,        // 交渉フェーズ完了
    ROLL,         // サイコロ (d1, d2)
    DECLINE,      // 止まったカードを買わない
    AUCTION,      // 止まったカードのオークション結果 (target=落札者 or -1, amount=落札額)
    END_TURN,     // 手番終了
    BUILD,        // 家を1軒建てる
    SELL,         // 家を1軒売る（半額）
    MORTGAGE,     // 抵当に入れる
    UNMORTGAGE,   // 抵当解除
    TRADE,        // 交渉成立
    TRADE_CANCEL, // 交渉破談（記録のみ）
    BANKRUPT,     // 破産 (target=債権者 or -1)
    SET_MONEY,    // 管理者修正: 所持金 (amount)
    SET_OWNER,    // 管理者修正: カード所有者 (card, target)
    SET_HOUSES,   // 管理者修正: 家の数 (card, amount)
    SET_POS,      // 管理者修正: 位置 (amount)
    UTILITY_ROLL, // 公共施設に止まったときの支払い用サイコロ (d1, d2)
    ALL_READY,    // 交渉フェーズを全員まとめて完了 → 運用フェーズへ
    CONTINUE      // 脱落者が出て終了したゲームを続ける
};

struct Action {
    ActionKind kind = ActionKind::PASS;
    int player = -1;
    int target = -1;
    CardId card = CardId::NONE;
    int d1 = 0;
    int d2 = 0;
    int amount = 0;
    TradeOffer trade;  // TRADE / TRADE_CANCEL のみ使用（proposerId=player, targetId=target）
};

std::string actionKindToString(ActionKind k);
bool actionKindFromString(const std::string& s, ActionKind& out);

#endif
