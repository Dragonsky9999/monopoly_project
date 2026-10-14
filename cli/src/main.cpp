// Monopoly CLI: 記録（入力）・再生（リプレイ移動）・Undo/Redo
#include "AIPlayer.hpp"
#include "CliSetup.hpp"
#include "DataMaster.hpp"
#include "GameLog.hpp"
#include "MonopolyEngine.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {

const char* AUTOSAVE_FILE = "monopoly_autosave.log";

std::string timestampedFilename() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm buf{};
#if defined(_WIN32)
    localtime_s(&buf, &t);
#else
    localtime_r(&t, &buf);
#endif
    std::ostringstream ss;
    ss << "monopoly_log_" << std::put_time(&buf, "%Y%m%d_%H%M%S") << ".log";
    return ss.str();
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string phaseName(Phase p) {
    switch (p) {
        case Phase::PURCHASE: return "PURCHASE";
        case Phase::NEGOTIATION: return "NEGOTIATION";
        case Phase::OPERATIONAL: return "OPERATIONAL";
        case Phase::FINISHED: return "FINISHED";
    }
    return "?";
}

bool toInt(const std::string& s, int& out) {
    try {
        size_t idx = 0;
        out = std::stoi(s, &idx);
        return idx == s.size();
    } catch (...) {
        return false;
    }
}

// ============================================================================
class Cli {
public:
    Cli() : rng(std::random_device{}()) {}
    int run();

private:
    MonopolyEngine engine;
    std::map<int, std::unique_ptr<AIPlayer>> ai;
    std::mt19937 rng;

    // ---- 表示
    void printHelp() const;
    void printStatus() const;
    void printCards() const;
    void printLog(int n) const;
    void printLastActionEvents(const std::string& prefix = "  ") const;
    std::string prompt() const;
    std::string cardLabel(CardId c) const;

    // ---- 入力補助
    int resolvePlayer(const std::string& tok) const;   // id or 名前。見つからなければ -2
    bool parseTradeItems(std::istringstream& ss, TradeOffer& t, std::string& err) const;

    // ---- 実行
    bool submit(const Action& a);  // 適用して結果を表示、オートセーブ
    void runAI();
    void gotoIndex(size_t target);
    size_t timelineLength() const { return engine.getActions().size() + engine.getRedoActions().size(); }
    void autosave() const { GameLog::save(engine, AUTOSAVE_FILE, true); }

    bool handle(const std::string& line);  // false = 終了
};

// ---------------------------------------------------------------------------- 表示
void Cli::printHelp() const {
    std::cout <<
        "------------------------------------------------------------------------------\n"
        " <player> = id or name   <card> = P-BR1, P-SB2, R-1, U-2 ...\n"
        " [Record]\n"
        "  buy <card>             purchase phase: buy any unowned card\n"
        "  buy                    operational: buy the card you landed on\n"
        "  pass                   purchase phase: pass  (operational: same as end)\n"
        "  ready [<player>|all]   negotiation finished (no arg = all)\n"
        "  continue               keep playing after a player went bankrupt\n"
        "  roll [d1 d2]           roll dice (no arg = random)\n"
        "  redice <d1> <d2>       fix the dice of the last roll\n"
        "  uroll [d1 d2]          roll for utility rent (after landing on someone's utility)\n"
        "  decline                do not buy the landed card\n"
        "  auction <winner|none> <price>   auction result for the landed card\n"
        "  end                    end turn\n"
        "  build <card> | sell <card> | mortgage <card> | unmortgage <card>\n"
        "  trade <p1> <p2> give <items> get <items>     items: cards and/or money\n"
        "        e.g. trade 0 1 give P-BR1 50 get P-SB1   (use - for nothing)\n"
        "  tradecancel <same as trade>                  record a rejected offer\n"
        "  bankrupt <player> [<creditor>|bank]\n"
        "  set money <player> <n> | set owner <card> <player|bank>\n"
        "  set houses <card> <0-5> | set pos <player> <0-39>\n"
        " [Replay / History]\n"
        "  undo | redo | prev [n] | next [n] | goto <n> | turn <t> | first | last\n"
        "  log [n]                show last n actions (default 10)\n"
        " [Other]\n"
        "  status (s) | cards | save [file] | help | exit\n"
        "------------------------------------------------------------------------------\n";
}

std::string Cli::cardLabel(CardId c) const {
    const CardState& cs = engine.getState().card(c);
    std::string s = DataMaster::toString(c);
    if (cs.houses == 5) s += "(Hotel)";
    else if (cs.houses > 0) s += "(H" + std::to_string(cs.houses) + ")";
    if (cs.mortgaged) s += "(M)";
    return s;
}

void Cli::printStatus() const {
    const GameState& s = engine.getState();
    std::cout << "\n================================================================================\n";
    std::cout << " Phase: " << phaseName(s.phase);
    if (s.phase == Phase::OPERATIONAL || s.phase == Phase::FINISHED) std::cout << " | Turn " << s.turn;
    if (s.phase != Phase::NEGOTIATION && s.phase != Phase::FINISHED)
        std::cout << " | Active: " << s.players[s.activePlayer].name;
    std::cout << " | Action " << engine.getActions().size() << "/" << timelineLength();
    if (engine.canRedo()) std::cout << "  (REPLAY: " << engine.getRedoActions().size() << " ahead)";
    std::cout << "\n";
    if (s.phase == Phase::OPERATIONAL) {
        std::cout << " " << (s.hasRolled ? "rolled " + std::to_string(s.lastDice1) + "+" + std::to_string(s.lastDice2)
                                         : std::string("not rolled yet"));
        if (s.pendingPurchase != CardId::NONE)
            std::cout << " | landed on unowned " << DataMaster::toString(s.pendingPurchase) << " (price "
                      << DataMaster::card(s.pendingPurchase).price << "): buy / decline / auction";
        if (s.pendingUtility != CardId::NONE)
            std::cout << " | landed on " << DataMaster::toString(s.pendingUtility) << ": uroll to decide the rent";
        std::cout << "\n";
    }
    std::cout << "--------------------------------------------------------------------------------\n";
    for (const auto& p : s.players) {
        bool active = (p.id == s.activePlayer && (s.phase == Phase::PURCHASE || s.phase == Phase::OPERATIONAL));
        std::cout << (active ? " * " : "   ") << "[" << (p.id + 1) << "] " << p.name;
        if (ai.count(p.id)) std::cout << " (AI)";
        if (p.bankrupt) {
            std::cout << " | BANKRUPT\n";
            continue;
        }
        std::cout << " | Money: " << p.money << "万";
        if (p.money < 0) std::cout << " (DEBT!)";
        std::cout << " | Pos: " << p.position << " " << DataMaster::board()[p.position].label
                  << " | Assets: " << engine.calculateTotalLiquidableAssets(p.id) << "万"
                  << " | EV: +" << std::fixed << std::setprecision(1) << engine.calculateExpectedIncomePerTurn(p.id)
                  << "万/T" << std::defaultfloat;
        if (s.phase == Phase::NEGOTIATION) std::cout << (p.ready ? " | READY" : " | negotiating");
        std::cout << "\n       Cards: ";
        auto owned = s.ownedBy(p.id);
        if (owned.empty()) std::cout << "-";
        for (CardId c : owned) std::cout << cardLabel(c) << " ";
        std::cout << "\n";
    }
    std::cout << "================================================================================\n";
}

void Cli::printCards() const {
    const GameState& s = engine.getState();
    std::cout << " Card    Group      Price House Mortg  Owner        State\n";
    for (const Card& c : DataMaster::cards()) {
        const CardState& cs = s.card(c.id);
        std::cout << " " << std::left << std::setw(7) << c.code << " " << std::setw(10) << DataMaster::groupName(c.group)
                  << std::right << std::setw(5) << c.price << std::setw(6) << c.housePrice << std::setw(6) << c.mortgage
                  << "  " << std::left << std::setw(12) << GameLog::playerName(s, cs.owner) << " ";
        if (cs.owner >= 0) {
            if (cs.houses == 5) std::cout << "Hotel ";
            else if (cs.houses > 0) std::cout << "H" << cs.houses << " ";
            if (cs.mortgaged) std::cout << "Mortgaged ";
            if (c.kind == CardKind::PROPERTY && engine.isGroupCompleted(cs.owner, c.group)) std::cout << "[set] ";
        }
        std::cout << std::right << "\n";
    }
}

void Cli::printLog(int n) const {
    const auto& acts = engine.getActions();
    size_t total = acts.size();
    size_t from = (n <= 0 || static_cast<size_t>(n) >= total) ? 0 : total - n;
    for (size_t i = from; i < total; ++i) {
        const GameState& before = engine.getStateBeforeAction(i);
        std::cout << " #" << std::setw(4) << (i + 1) << " [T" << before.turn << " " << phaseName(before.phase).substr(0, 4)
                  << "] " << GameLog::describeAction(acts[i], before) << "\n";
        const GameState& after = (i + 1 < total) ? engine.getStateBeforeAction(i + 1) : engine.getState();
        for (const auto& e : engine.getEventsOfAction(i))
            if (e.type != ActionType::END_TURN && e.type != ActionType::PASS && e.type != ActionType::READY)
                std::cout << "          > " << GameLog::describeEvent(e, after) << "\n";
    }
    if (engine.canRedo()) std::cout << " ... " << engine.getRedoActions().size() << " more action(s) ahead (next / last)\n";
}

void Cli::printLastActionEvents(const std::string& prefix) const {
    if (engine.getActions().empty()) return;
    size_t i = engine.getActions().size() - 1;
    for (const auto& e : engine.getEventsOfAction(i)) {
        if (e.type == ActionType::PHASE_CHANGE) {
            std::cout << prefix << "== Phase -> " << phaseName(engine.getState().phase) << " ==\n";
            continue;
        }
        std::cout << prefix << GameLog::describeEvent(e, engine.getState()) << "\n";
    }
}

std::string Cli::prompt() const {
    const GameState& s = engine.getState();
    std::ostringstream o;
    o << "\n[";
    if (engine.canRedo()) o << "REPLAY " << engine.getActions().size() << "/" << timelineLength() << " | ";
    o << phaseName(s.phase);
    if (s.phase == Phase::OPERATIONAL) o << " T" << s.turn;
    if (s.phase == Phase::PURCHASE || s.phase == Phase::OPERATIONAL) {
        const Player& p = s.players[s.activePlayer];
        o << " | " << p.name << " " << p.money << "万";
    }
    if (s.phase == Phase::PURCHASE) o << " | buy <card> / pass";
    if (s.phase == Phase::NEGOTIATION) o << " | trade ... / ready";
    if (s.phase == Phase::OPERATIONAL) {
        if (s.pendingPurchase != CardId::NONE) o << " | " << DataMaster::toString(s.pendingPurchase) << ": buy / decline / auction";
        if (s.pendingUtility != CardId::NONE) o << " | " << DataMaster::toString(s.pendingUtility) << ": uroll";
        else if (!s.hasRolled) o << " | roll";
        else o << " | end";
    }
    o << "]> ";
    return o.str();
}

// ---------------------------------------------------------------------------- 入力補助
int Cli::resolvePlayer(const std::string& tok) const {
    const GameState& s = engine.getState();
    int id;
    if (toInt(tok, id)) return (id >= 1 && id <= static_cast<int>(s.players.size())) ? id - 1 : -2;  // 表示・入力は 1 始まり
    std::string t = lower(tok);
    if (t == "bank" || t == "none") return -1;
    for (const auto& p : s.players)
        if (lower(p.name) == t) return p.id;
    return -2;
}

// "give P-BR1 50 get P-SB1" を解析
bool Cli::parseTradeItems(std::istringstream& ss, TradeOffer& t, std::string& err) const {
    std::string tok;
    int side = 0;  // 1 = give, 2 = get
    while (ss >> tok) {
        std::string l = lower(tok);
        if (l == "give") { side = 1; continue; }
        if (l == "get") { side = 2; continue; }
        if (l == "-") continue;
        if (side == 0) { err = "expected 'give' or 'get' before '" + tok + "'"; return false; }
        int m;
        if (toInt(tok, m)) {
            (side == 1 ? t.offerMoney : t.requestMoney) += m;
            continue;
        }
        CardId c = DataMaster::fromString(tok);
        if (c == CardId::NONE) { err = "unknown card '" + tok + "'"; return false; }
        (side == 1 ? t.offerCards : t.requestCards).push_back(c);
    }
    return true;
}

// ---------------------------------------------------------------------------- 実行
bool Cli::submit(const Action& a) {
    if (engine.canRedo()) {
        ApplyResult v = engine.validate(a);
        if (!v.ok) {
            std::cout << "[Error] " << v.error << "\n";
            return false;
        }
        std::cout << "This discards " << engine.getRedoActions().size()
                  << " later action(s) of the replay timeline. Continue? (y/N): ";
        std::string ans;
        if (!std::getline(std::cin, ans) || lower(CliSetup::trim(ans)) != "y") {
            std::cout << "Cancelled.\n";
            return false;
        }
    }
    const int prevActive = engine.getState().activePlayer;
    const Phase prevPhase = engine.getState().phase;
    ApplyResult r = engine.apply(a);
    if (!r.ok) {
        std::cout << "[Error] " << r.error << "\n";
        return false;
    }
    printLastActionEvents();
    autosave();
    const GameState& s = engine.getState();
    if (s.phase != prevPhase || s.activePlayer != prevActive) printStatus();
    return true;
}

void Cli::runAI() {
    int guard = 0;
    while (!engine.canRedo() && engine.getState().phase != Phase::FINISHED) {
        bool acted = false;
        for (auto& [pid, bot] : ai) {
            auto a = bot->decide(engine);
            if (!a) continue;
            const int prevActive = engine.getState().activePlayer;
            const Phase prevPhase = engine.getState().phase;
            ApplyResult r = engine.apply(*a);
            if (!r.ok) {
                std::cout << "  [AI error] " << GameLog::actionToLine(*a) << " : " << r.error << "\n";
                continue;
            }
            std::cout << " AI> " << GameLog::describeAction(*a, engine.getStateBeforeAction(engine.getActions().size() - 1)) << "\n";
            printLastActionEvents("      ");
            if (engine.getState().phase != prevPhase || engine.getState().activePlayer != prevActive) {
                if (ai.size() < engine.getState().players.size()) printStatus();
            }
            acted = true;
            break;
        }
        if (!acted) break;
        if (++guard >= 3000) {
            std::cout << "[AI] paused after " << guard << " actions. Press Enter to continue, or type a command.\n";
            break;
        }
    }
    autosave();
}

void Cli::gotoIndex(size_t target) {
    target = std::min(target, timelineLength());
    while (engine.getActions().size() > target && engine.undo()) {}
    while (engine.getActions().size() < target && engine.redo()) {}
}

// ---------------------------------------------------------------------------- コマンド
bool Cli::handle(const std::string& line) {
    std::istringstream ss(line);
    std::string cmd;
    if (!(ss >> cmd)) {
        runAI();
        return true;
    }
    cmd = lower(cmd);
    const GameState& s = engine.getState();

    Action a;
    a.player = s.activePlayer;

    auto needPlayer = [&](const std::string& tok, int& out, bool allowBank = false) {
        out = resolvePlayer(tok);
        if (out == -2 || (out == -1 && !allowBank)) {
            std::cout << "[Error] unknown player '" << tok << "'\n";
            return false;
        }
        return true;
    };
    auto needCard = [&](CardId& out) {
        std::string tok;
        if (!(ss >> tok)) {
            std::cout << "[Error] specify a card (e.g. P-BR1)\n";
            return false;
        }
        out = DataMaster::fromString(tok);
        if (out == CardId::NONE) {
            std::cout << "[Error] unknown card '" << tok << "'\n";
            return false;
        }
        return true;
    };
    // カード操作は「カードの所有者」が行う（手番でなくてもよい）
    auto ownerOr = [&](CardId c) {
        int o = engine.getOwnerOfCard(c);
        return o >= 0 ? o : s.activePlayer;
    };

    if (cmd == "exit" || cmd == "quit") {
        std::string name = timestampedFilename();
        if (GameLog::save(engine, name, true)) std::cout << "[Saved] " << name << "\n";
        return false;
    } else if (cmd == "help" || cmd == "h" || cmd == "?") {
        printHelp();
    } else if (cmd == "status" || cmd == "s" || cmd == "show") {
        printStatus();
    } else if (cmd == "cards" || cmd == "board") {
        printCards();
    } else if (cmd == "log" || cmd == "history") {
        int n = 10;
        std::string t;
        if (ss >> t) {
            if (lower(t) == "all") n = 0;
            else toInt(t, n);
        }
        printLog(n);
    } else if (cmd == "save" || cmd == "export") {
        std::string name;
        if (!(ss >> name)) name = timestampedFilename();
        std::cout << (GameLog::save(engine, name, true) ? "[Saved] " : "[Error] could not write ") << name << "\n";

    // ---- 記録
    } else if (cmd == "buy") {
        a.kind = ActionKind::BUY;
        std::string tok;
        if (ss >> tok) {
            a.card = DataMaster::fromString(tok);
            if (a.card == CardId::NONE) {
                std::cout << "[Error] unknown card '" << tok << "'\n";
                return true;
            }
        } else if (s.phase == Phase::PURCHASE) {
            std::cout << "[Error] specify a card (e.g. buy P-BR1)\n";
            return true;
        }
        submit(a);
    } else if (cmd == "pass") {
        a.kind = (s.phase == Phase::OPERATIONAL) ? ActionKind::END_TURN : ActionKind::PASS;
        submit(a);
    } else if (cmd == "end") {
        a.kind = ActionKind::END_TURN;
        submit(a);
    } else if (cmd == "continue") {
        a.kind = ActionKind::CONTINUE;
        submit(a);
    } else if (cmd == "ready") {
        std::string tok;
        if (!(ss >> tok) || lower(tok) == "all") {
            a.kind = ActionKind::ALL_READY;  // 交渉フェーズを全員まとめて完了
            submit(a);
        } else {
            a.kind = ActionKind::READY;
            if (!needPlayer(tok, a.player)) return true;
            submit(a);
        }
    } else if (cmd == "roll" || cmd == "uroll") {
        a.kind = cmd == "uroll" ? ActionKind::UTILITY_ROLL : ActionKind::ROLL;
        std::string t1, t2;
        if (ss >> t1) {
            if (!(ss >> t2) || !toInt(t1, a.d1) || !toInt(t2, a.d2)) {
                std::cout << "[Error] usage: roll <d1> <d2>  (e.g. roll 3 4)\n";
                return true;
            }
        } else {
            std::uniform_int_distribution<int> die(1, 6);
            a.d1 = die(rng);
            a.d2 = die(rng);
        }
        submit(a);
    } else if (cmd == "redice") {  // 直前の移動の出目を修正
        const auto& acts = engine.getActions();
        int d1 = 0, d2 = 0;
        std::string t1, t2;
        if (!(ss >> t1 >> t2) || !toInt(t1, d1) || !toInt(t2, d2)) {
            std::cout << "[Error] usage: redice <d1> <d2>\n";
            return true;
        }
        if (acts.empty() || (acts.back().kind != ActionKind::ROLL && acts.back().kind != ActionKind::UTILITY_ROLL)) {
            std::cout << "[Error] the last action is not a roll\n";
            return true;
        }
        Action r = acts.back();
        r.d1 = d1;
        r.d2 = d2;
        ApplyResult res = engine.replaceLast(r);
        if (!res.ok) {
            std::cout << "[Error] " << res.error << "\n";
            return true;
        }
        printLastActionEvents();
        autosave();
    } else if (cmd == "decline") {
        a.kind = ActionKind::DECLINE;
        submit(a);
    } else if (cmd == "auction") {
        a.kind = ActionKind::AUCTION;
        std::string w, pr;
        if (!(ss >> w)) {
            std::cout << "[Error] usage: auction <winner|none> <price>\n";
            return true;
        }
        if (!needPlayer(w, a.target, true)) return true;
        if (a.target >= 0 && (!(ss >> pr) || !toInt(pr, a.amount))) {
            std::cout << "[Error] usage: auction <winner> <price>\n";
            return true;
        }
        submit(a);
    } else if (cmd == "build" || cmd == "sell" || cmd == "mortgage" || cmd == "unmortgage") {
        a.kind = cmd == "build" ? ActionKind::BUILD
               : cmd == "sell" ? ActionKind::SELL
               : cmd == "mortgage" ? ActionKind::MORTGAGE : ActionKind::UNMORTGAGE;
        if (!needCard(a.card)) return true;
        a.player = ownerOr(a.card);
        submit(a);
    } else if (cmd == "trade" || cmd == "tradecancel") {
        a.kind = (cmd == "trade") ? ActionKind::TRADE : ActionKind::TRADE_CANCEL;
        std::string p1, p2, err;
        if (!(ss >> p1 >> p2)) {
            std::cout << "[Error] usage: trade <p1> <p2> give <items> get <items>\n";
            return true;
        }
        if (!needPlayer(p1, a.player) || !needPlayer(p2, a.target)) return true;
        a.trade.proposerId = a.player;
        a.trade.targetId = a.target;
        if (!parseTradeItems(ss, a.trade, err)) {
            std::cout << "[Error] " << err << "\n";
            return true;
        }
        if (a.kind == ActionKind::TRADE) {
            double score = engine.evaluateTradeFairness(a.trade);
            std::cout << "  fairness score (for " << s.players[a.player].name << "): " << std::fixed
                      << std::setprecision(1) << score << std::defaultfloat
                      << (score > 30 ? "  -> favors proposer" : score < -30 ? "  -> favors target" : "  -> roughly fair")
                      << "\n";
        }
        submit(a);
    } else if (cmd == "bankrupt") {
        a.kind = ActionKind::BANKRUPT;
        std::string p, c;
        if (!(ss >> p)) {
            std::cout << "[Error] usage: bankrupt <player> [<creditor>|bank]\n";
            return true;
        }
        if (!needPlayer(p, a.player)) return true;
        if (ss >> c) {
            if (!needPlayer(c, a.target, true)) return true;
        } else {
            a.target = (a.player == s.activePlayer) ? s.debtCreditor : -1;
        }
        std::cout << "  creditor: " << GameLog::playerName(s, a.target) << "\n";
        submit(a);
    } else if (cmd == "set") {
        std::string what, x, y;
        ss >> what >> x >> y;
        what = lower(what);
        if (what == "money" || what == "pos") {
            a.kind = (what == "money") ? ActionKind::SET_MONEY : ActionKind::SET_POS;
            if (!needPlayer(x, a.player) || !toInt(y, a.amount)) {
                std::cout << "[Error] usage: set " << what << " <player> <number>\n";
                return true;
            }
        } else if (what == "owner" || what == "houses") {
            a.kind = (what == "owner") ? ActionKind::SET_OWNER : ActionKind::SET_HOUSES;
            a.card = DataMaster::fromString(x);
            if (a.card == CardId::NONE) {
                std::cout << "[Error] unknown card '" << x << "'\n";
                return true;
            }
            if (what == "owner") {
                if (!needPlayer(y, a.target, true)) return true;
            } else if (!toInt(y, a.amount)) {
                std::cout << "[Error] usage: set houses <card> <0-5>\n";
                return true;
            }
            a.player = engine.getOwnerOfCard(a.card);
        } else {
            std::cout << "[Error] usage: set money|pos|owner|houses ...\n";
            return true;
        }
        submit(a);

    // ---- 履歴・リプレイ
    } else if (cmd == "undo" || cmd == "prev" || cmd == "redo" || cmd == "next") {
        int n = 1;
        std::string t;
        if (ss >> t && !toInt(t, n)) n = 1;
        bool back = (cmd == "undo" || cmd == "prev");
        int done = 0;
        for (int i = 0; i < n; ++i) {
            if (back ? !engine.undo() : !engine.redo()) break;
            ++done;
            if (!back) printLastActionEvents();
        }
        if (done == 0) std::cout << (back ? "Nothing to undo.\n" : "Nothing to redo.\n");
        else std::cout << (back ? "Undid " : "Redid ") << done << " action(s). Now at " << engine.getActions().size()
                       << "/" << timelineLength() << "\n";
        autosave();
    } else if (cmd == "goto" || cmd == "first" || cmd == "last") {
        int n = 0;
        if (cmd == "last") n = static_cast<int>(timelineLength());
        else if (cmd == "goto") {
            std::string t;
            if (!(ss >> t) || !toInt(t, n) || n < 0) {
                std::cout << "[Error] usage: goto <action number 0-" << timelineLength() << ">\n";
                return true;
            }
        }
        gotoIndex(static_cast<size_t>(n));
        printStatus();
    } else if (cmd == "turn") {
        int t = 0;
        std::string tok;
        if (!(ss >> tok) || !toInt(tok, t) || t < 1) {
            std::cout << "[Error] usage: turn <number>\n";
            return true;
        }
        size_t cur = engine.getActions().size();
        gotoIndex(timelineLength());
        // そのターン最初のアクションの直前（= ターン開始時点）を探す
        size_t found = SIZE_MAX;
        for (size_t i = 0; i < engine.getActions().size(); ++i) {
            const GameState& b = engine.getStateBeforeAction(i);
            if (b.phase == Phase::OPERATIONAL && b.turn == t) {
                found = i;
                break;
            }
        }
        if (found == SIZE_MAX) {
            std::cout << "[Error] turn " << t << " not found\n";
            gotoIndex(cur);
            return true;
        }
        gotoIndex(found);
        printStatus();
    } else {
        std::cout << "Unknown command. Type 'help'.\n";
    }
    return true;
}

int Cli::run() {
    GameSettings config = CliSetup::runWizard();

    if (config.mode == SetupMode::LOAD_LOG) {
        GameLog::LoadResult r = GameLog::load(engine, config.importFilename);
        if (r.ok) {
            std::cout << "[Loaded] " << config.importFilename << " : " << r.appliedActions << " actions\n";
            std::cout << "Replay: first / prev / next / goto <n> / turn <t> / last / log\n";
            std::cout << "(new input at the end continues recording)\n";
        } else if (r.appliedActions > 0 || r.errorLine > 0) {
            std::cout << "[Load error] line " << r.errorLine << ": " << r.error << "\n";
            std::cout << "Stopped after " << r.appliedActions << " valid actions. You can inspect / fix from here.\n";
        } else {
            std::cout << "[Load error] " << r.error << "\n";
            return 1;
        }
        // 読み込んだゲームは全員人間扱い（AI は自動で動かさない）
    } else {
        engine.newGame(config.playerNames, config.rules);
        for (int i = 0; i < static_cast<int>(config.playerNames.size()); ++i) {
            if (config.playerNames[i].rfind("AI_", 0) == 0) {
                ai[i] = std::make_unique<AIPlayer>(i, AIDifficulty::BALANCED);
                std::cout << "Registered AI for Player [" << (i + 1) << "] " << config.playerNames[i] << "\n";
            }
        }
        autosave();
    }

    printStatus();
    runAI();

    std::string line;
    while (true) {
        std::cout << prompt() << std::flush;
        if (!std::getline(std::cin, line)) {
            std::string name = timestampedFilename();
            if (GameLog::save(engine, name, true)) std::cout << "\n[Saved] " << name << "\n";
            break;
        }
        if (!handle(line)) break;
        runAI();
    }
    return 0;
}

}  // namespace

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    Cli cli;
    return cli.run();
}
