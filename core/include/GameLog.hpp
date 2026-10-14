#ifndef GAME_LOG_HPP
#define GAME_LOG_HPP

#include "Action.hpp"
#include "GameEvent.hpp"
#include "GameState.hpp"
#include "MonopolyEngine.hpp"
#include <string>
#include <vector>

// ログファイル（v2）の保存・読込と、表示用の文字列化
//
// 形式:
//   # MONOPOLY LOG
//   version=2
//   [SETTINGS]
//   startMoney=1500 ...
//   [PLAYERS]
//   0=Alice
//   [ACTIONS]
//   BUY p=0 card=P-BR1
//   ROLL p=0 d1=3 d2=4
//   #   > ...（イベントの説明。読込時は無視）
namespace GameLog {

struct LoadResult {
    bool ok = false;
    std::string error;
    int errorLine = 0;      // 失敗した行番号（1始まり）。0 = 行に依存しないエラー
    size_t appliedActions = 0;
    size_t skippedActions = 0;  // 読み飛ばした無意味な記録（空の取引など）
};

bool save(const MonopolyEngine& engine, const std::string& filename, bool includeRedo = false);
// ファイルを介さない版（GUI は自前の Unicode パス対応 I/O でこれを使う）
std::string serialize(const MonopolyEngine& engine, bool includeRedo = false);
LoadResult deserialize(MonopolyEngine& engine, const std::string& text);
// 読み込んで全アクションを適用する。途中で失敗した場合はそこまで適用した状態で止まる
LoadResult load(MonopolyEngine& engine, const std::string& filename);

std::string actionToLine(const Action& a);
bool lineToAction(const std::string& line, Action& out, std::string& error);

std::string describeAction(const Action& a, const GameState& s);
std::string describeEvent(const GameEvent& e, const GameState& s);
std::string playerName(const GameState& s, int id);  // -1 は "Bank"

}  // namespace GameLog

#endif
