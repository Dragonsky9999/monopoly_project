#ifndef MONOPOLY_ENGINE_HPP
#define MONOPOLY_ENGINE_HPP

#include "Action.hpp"
#include "Card.hpp"
#include "DataMaster.hpp"
#include "GameEvent.hpp"
#include "GameState.hpp"
#include "TradeOffer.hpp"
#include <string>
#include <vector>

struct ApplyResult {
    bool ok = false;
    std::string error;  // ok == false のときの理由
};

// ゲーム状態・ルール・履歴を持つコア。
// 状態を変える手段は apply(Action) だけ（Undo/Redo/リプレイの整合性を保つため）。
class MonopolyEngine {
public:
    MonopolyEngine();

    void newGame(const std::vector<std::string>& playerNames, const Settings& settings = Settings());

    // ---- 操作 ----
    ApplyResult apply(const Action& action);            // 成功時のみ状態が変わる。Redo 履歴は破棄
    ApplyResult validate(const Action& action) const;   // 状態を変えずに可否だけ判定

    ApplyResult replaceLast(const Action& action);      // 直前の操作を差し替え（出目の修正など）。失敗時は元のまま
    bool undo();
    bool redo();
    bool canUndo() const { return !snapshots.empty(); }
    bool canRedo() const { return !redoStack.empty(); }

    // ---- 参照 ----
    const GameState& getState() const { return state; }
    const GameState& getInitialState() const { return initialState; }
    const Card& getCard(CardId id) const { return DataMaster::card(id); }
    const std::vector<Action>& getActions() const { return actions; }          // 適用済み（古い順）
    const std::vector<Action>& getRedoActions() const { return redoStack; }    // 末尾が次に redo される
    std::vector<GameEvent> getEvents() const;                                  // 全イベント（古い順）
    const std::vector<GameEvent>& getEventsOfAction(size_t actionIndex) const { return eventsPerAction.at(actionIndex); }
    const GameState& getStateBeforeAction(size_t actionIndex) const;           // actionIndex 番目の直前の状態

    CardId getCardIdAtPosition(int position) const;
    int getOwnerOfCard(CardId cardId) const { return state.card(cardId).owner; }
    bool isGroupCompleted(int playerId, GroupType group) const { return groupCompleted(state, playerId, group); }
    int calculateRent(CardId cardId, int diceSum) const { return rentOf(state, cardId, diceSum); }
    int calculateTotalLiquidableAssets(int playerId) const { return liquidValue(state, playerId); }

    // ---- 分析 ----
    double calculateExpectedIncomePerTurn(int playerId) const;
    double evaluateTradeFairness(const TradeOffer& offer) const;

    // ---- 静的ルール計算（任意の GameState に対して使える） ----
    static bool groupCompleted(const GameState& s, int playerId, GroupType group);
    static int rentOf(const GameState& s, CardId cardId, int diceSum);
    static int liquidValue(const GameState& s, int playerId);
    static int unmortgageCost(const GameState& s, CardId cardId);

private:
    // s を書き換えて ev にイベントを積む。失敗時はエラー文字列を返す（s は破棄される前提）
    static std::string execute(const Action& a, GameState& s, std::vector<GameEvent>& ev);
    ApplyResult applyInternal(const Action& action, bool clearRedo);

    GameState state;
    GameState initialState;
    std::vector<GameState> snapshots;                 // snapshots[i] = actions[i] 適用前の状態
    std::vector<Action> actions;
    std::vector<std::vector<GameEvent>> eventsPerAction;
    std::vector<Action> redoStack;
};

#endif
