#ifndef GAME_EVENT_HPP
#define GAME_EVENT_HPP

#include "Card.hpp"
#include <vector>

// アクション適用の「結果」。分析・表示用で、ログ再生では再計算される。
enum class ActionType {
    PURCHASE,      // 購入
    PASS,          // パス
    READY,         // 交渉完了
    PHASE_CHANGE,  // フェーズ移行
    ROLL_AND_MOVE, // 移動
    PAY_RENT,      // 家賃支払い
    PAY_TAX,       // 税金支払い
    GO_BONUS,      // GO手当
    LAND_UNOWNED,  // 未所有マスに止まった
    DECLINE,       // 買わない
    AUCTION,       // オークション
    END_TURN,      // 手番終了
    TRADE_EXECUTE, // 交渉成立
    TRADE_CANCEL,  // 交渉破談
    BUILD_HOUSE,   // 家建設
    SELL_HOUSE,    // 家売却
    MORTGAGE,      // 抵当
    UNMORTGAGE,    // 抵当解除
    BANKRUPT,      // 破産
    GAME_OVER,     // 終了
    OVERRIDE       // 管理者手動修正
};

struct GameEvent {
    int eventId = 0;
    int actionIndex = 0;      // 何番目のアクションから生じたか（0始まり）
    int turn = 0;
    int playerId = -1;
    ActionType type = ActionType::PASS;

    int targetPlayerId = -1;  // 取引相手・家賃の受取人など
    int moneyChange = 0;      // playerId から見た増減（正: 収入）
    std::vector<CardId> cardsGained;
    std::vector<CardId> cardsLost;
    int dice1 = 0;
    int dice2 = 0;
    int position = -1;

    // 分析用スナップショット
    int playerMoneyAfter = 0;
    int totalAssetAfter = 0;  // 所持金 + 家の売却額 + 抵当可能額
};

#endif
