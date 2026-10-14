#include "GameLog.hpp"
#include "DataMaster.hpp"
#include <fstream>
#include <map>
#include <sstream>

namespace GameLog {

namespace {

std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
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

// "P-BR1+P-SB2+50" → カードとお金。 "-" は空
bool parseItems(const std::string& s, std::vector<CardId>& cards, int& money, std::string& err) {
    cards.clear();
    money = 0;
    if (s.empty() || s == "-") return true;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, '+')) {
        if (tok.empty()) continue;
        int m;
        if (toInt(tok, m)) {
            money += m;
            continue;
        }
        CardId c = DataMaster::fromString(tok);
        if (c == CardId::NONE) {
            err = "unknown card '" + tok + "'";
            return false;
        }
        cards.push_back(c);
    }
    return true;
}

std::string itemsToString(const std::vector<CardId>& cards, int money) {
    std::string r;
    for (CardId c : cards) r += (r.empty() ? "" : "+") + DataMaster::toString(c);
    if (money != 0) r += (r.empty() ? "" : "+") + std::to_string(money);
    return r.empty() ? "-" : r;
}

std::string money(int v) { return std::to_string(v); }

}  // namespace

std::string playerName(const GameState& s, int id) {
    if (id >= 0 && id < static_cast<int>(s.players.size())) return s.players[id].name;
    return "Bank";
}

// ---------------------------------------------------------------- Action <-> 行
std::string actionToLine(const Action& a) {
    std::ostringstream o;
    o << actionKindToString(a.kind);
    if (a.player != -1) o << " p=" << a.player;
    if (a.target != -1) o << " to=" << a.target;
    if (a.card != CardId::NONE) o << " card=" << DataMaster::toString(a.card);
    if (a.d1 != 0 || a.d2 != 0) o << " d1=" << a.d1 << " d2=" << a.d2;
    if (a.amount != 0) o << " amount=" << a.amount;
    if (a.kind == ActionKind::TRADE || a.kind == ActionKind::TRADE_CANCEL) {
        o << " give=" << itemsToString(a.trade.offerCards, a.trade.offerMoney);
        o << " get=" << itemsToString(a.trade.requestCards, a.trade.requestMoney);
    }
    return o.str();
}

bool lineToAction(const std::string& line, Action& out, std::string& error) {
    std::stringstream ss(line);
    std::string kindStr;
    ss >> kindStr;
    Action a;
    if (!actionKindFromString(kindStr, a.kind)) {
        error = "unknown action '" + kindStr + "'";
        return false;
    }
    std::string tok;
    while (ss >> tok) {
        size_t eq = tok.find('=');
        if (eq == std::string::npos) {
            error = "expected key=value, got '" + tok + "'";
            return false;
        }
        std::string key = tok.substr(0, eq), val = tok.substr(eq + 1);
        int iv = 0;
        if (key == "card") {
            a.card = DataMaster::fromString(val);
            if (a.card == CardId::NONE) {
                error = "unknown card '" + val + "'";
                return false;
            }
        } else if (key == "give") {
            if (!parseItems(val, a.trade.offerCards, a.trade.offerMoney, error)) return false;
        } else if (key == "get") {
            if (!parseItems(val, a.trade.requestCards, a.trade.requestMoney, error)) return false;
        } else if (!toInt(val, iv)) {
            error = "bad number for '" + key + "': " + val;
            return false;
        } else if (key == "p") a.player = iv;
        else if (key == "to") a.target = iv;
        else if (key == "d1") a.d1 = iv;
        else if (key == "d2") a.d2 = iv;
        else if (key == "amount") a.amount = iv;
        else {
            error = "unknown key '" + key + "'";
            return false;
        }
    }
    a.trade.proposerId = a.player;
    a.trade.targetId = a.target;
    out = a;
    return true;
}

// ---------------------------------------------------------------- 保存
bool save(const MonopolyEngine& engine, const std::string& filename, bool includeRedo) {
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs.is_open()) return false;
    ofs << serialize(engine, includeRedo);
    return static_cast<bool>(ofs);
}

std::string serialize(const MonopolyEngine& engine, bool includeRedo) {
    std::ostringstream ofs;
    const GameState& init = engine.getInitialState();
    const Settings& st = init.settings;

    ofs << "# MONOPOLY LOG\n";
    ofs << "version=2\n\n";
    ofs << "[SETTINGS]\n";
    ofs << "startMoney=" << st.startMoney << "\n";
    ofs << "goBonus=" << st.goBonus << "\n";
    ofs << "tax=" << (st.taxEnabled ? 1 : 0) << "\n";
    ofs << "doubleRentOnSet=" << (st.doubleRentOnSet ? 1 : 0) << "\n";
    ofs << "doublesRollAgain=" << (st.doublesRollAgain ? 1 : 0) << "\n";
    ofs << "maxDoubles=" << st.maxDoubles << "\n";
    ofs << "debtBlocksEndTurn=" << (st.debtBlocksEndTurn ? 1 : 0) << "\n";
    ofs << "utilityRentRoll=" << (st.utilityRentRoll ? 1 : 0) << "\n";
    ofs << "endOnBankrupt=" << (st.endOnBankrupt ? 1 : 0) << "\n";
    ofs << "unmortgageInterestPct=" << st.unmortgageInterestPct << "\n\n";
    ofs << "[PLAYERS]\n";
    for (const auto& p : init.players) ofs << p.id << "=" << p.name << "\n";
    ofs << "\n[ACTIONS]\n";

    const auto& acts = engine.getActions();
    for (size_t i = 0; i < acts.size(); ++i) {
        ofs << actionToLine(acts[i]) << "\n";
        const GameState& after = (i + 1 < acts.size()) ? engine.getStateBeforeAction(i + 1) : engine.getState();
        for (const auto& e : engine.getEventsOfAction(i)) ofs << "#   > " << describeEvent(e, after) << "\n";
    }
    if (includeRedo) {
        const auto& r = engine.getRedoActions();
        for (auto it = r.rbegin(); it != r.rend(); ++it) ofs << actionToLine(*it) << "\n";
    }
    return ofs.str();
}

// ---------------------------------------------------------------- 読込
LoadResult load(MonopolyEngine& engine, const std::string& filename) {
    std::ifstream f(filename, std::ios::binary);
    if (!f.is_open()) {
        LoadResult res;
        res.error = "cannot open file: " + filename;
        return res;
    }
    std::ostringstream buf;
    buf << f.rdbuf();
    return deserialize(engine, buf.str());
}

LoadResult deserialize(MonopolyEngine& engine, const std::string& text) {
    LoadResult res;
    std::istringstream ifs(text);

    Settings settings;
    settings.doublesRollAgain = false;  // キーがない旧ログはぞろ目ルールなし
    settings.maxDoubles = 0;
    settings.debtBlocksEndTurn = false;
    settings.utilityRentRoll = false;
    settings.endOnBankrupt = false;
    settings.doubleRentOnSet = false;  // 旧ログは常にキーがあるが念のため旧既定に合わせる
    std::map<int, std::string> players;
    std::vector<std::pair<int, std::string>> actionLines;
    std::string section, raw;
    int lineNo = 0, version = 0;

    while (std::getline(ifs, raw)) {
        ++lineNo;
        std::string line = trim(raw);
        if (lineNo == 1 && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF) line = trim(line.substr(3));  // BOM
        if (line.empty() || line[0] == '#') continue;
        if (line[0] == '[') {
            section = line;
            continue;
        }
        if (section.empty()) {
            if (line.rfind("version=", 0) == 0) toInt(line.substr(8), version);
            continue;
        }
        if (section == "[SETTINGS]") {
            size_t eq = line.find('=');
            int v = 0;
            if (eq == std::string::npos || !toInt(trim(line.substr(eq + 1)), v)) {
                res.error = "bad setting line";
                res.errorLine = lineNo;
                return res;
            }
            std::string k = trim(line.substr(0, eq));
            if (k == "startMoney") settings.startMoney = v;
            else if (k == "goBonus") settings.goBonus = v;
            else if (k == "tax") settings.taxEnabled = (v != 0);
            else if (k == "doubleRentOnSet") settings.doubleRentOnSet = (v != 0);
            else if (k == "unmortgageInterestPct") settings.unmortgageInterestPct = v;
            else if (k == "doublesRollAgain") settings.doublesRollAgain = (v != 0);
            else if (k == "maxDoubles") settings.maxDoubles = v;
            else if (k == "debtBlocksEndTurn") settings.debtBlocksEndTurn = (v != 0);
            else if (k == "utilityRentRoll") settings.utilityRentRoll = (v != 0);
            else if (k == "endOnBankrupt") settings.endOnBankrupt = (v != 0);
        } else if (section == "[PLAYERS]") {
            size_t eq = line.find('=');
            int id = 0;
            if (eq == std::string::npos || !toInt(trim(line.substr(0, eq)), id)) {
                res.error = "bad player line";
                res.errorLine = lineNo;
                return res;
            }
            players[id] = trim(line.substr(eq + 1));
        } else if (section == "[ACTIONS]") {
            actionLines.emplace_back(lineNo, line);
        }
    }

    if (version != 2) {
        res.error = "unsupported log version (expected version=2). Old v1 logs cannot be loaded.";
        return res;
    }
    if (players.size() < 2) {
        res.error = "need at least 2 players in [PLAYERS]";
        return res;
    }
    std::vector<std::string> names;
    for (size_t i = 0; i < players.size(); ++i) {
        auto it = players.find(static_cast<int>(i));
        if (it == players.end()) {
            res.error = "player ids must be 0.." + std::to_string(players.size() - 1);
            return res;
        }
        names.push_back(it->second);
    }

    engine.newGame(names, settings);
    for (const auto& [ln, text] : actionLines) {
        Action a;
        std::string err;
        if (!lineToAction(text, a, err)) {
            res.error = err;
            res.errorLine = ln;
            return res;
        }
        // 旧バージョンで記録された「空の取引」（何も動かない記録）は読み飛ばす
        if ((a.kind == ActionKind::TRADE || a.kind == ActionKind::TRADE_CANCEL) && a.trade.offerCards.empty() &&
            a.trade.requestCards.empty() && a.trade.offerMoney == 0 && a.trade.requestMoney == 0) {
            res.skippedActions++;
            continue;
        }
        ApplyResult r = engine.apply(a);
        if (!r.ok) {
            res.error = "action rejected: " + r.error + "  [" + text + "]";
            res.errorLine = ln;
            return res;
        }
        res.appliedActions++;
    }
    res.ok = true;
    return res;
}

// ---------------------------------------------------------------- 表示
std::string describeAction(const Action& a, const GameState& s) {
    std::string who = playerName(s, a.player);
    std::string card = DataMaster::toString(a.card);
    switch (a.kind) {
        case ActionKind::BUY: return who + " buys " + (a.card == CardId::NONE ? std::string("landed card") : card);
        case ActionKind::PASS: return who + " passes";
        case ActionKind::READY: return who + " is ready";
        case ActionKind::ROLL: return who + " rolls " + std::to_string(a.d1) + "+" + std::to_string(a.d2);
        case ActionKind::ALL_READY: return "negotiation finished (all ready)";
        case ActionKind::CONTINUE: return "game continues";
        case ActionKind::UTILITY_ROLL: return who + " rolls for utility rent " + std::to_string(a.d1) + "+" + std::to_string(a.d2);
        case ActionKind::DECLINE: return who + " declines";
        case ActionKind::AUCTION:
            return a.target < 0 ? "auction: no winner" : "auction: " + playerName(s, a.target) + " wins for " + money(a.amount);
        case ActionKind::END_TURN: return who + " ends turn";
        case ActionKind::BUILD: return who + " builds on " + card;
        case ActionKind::SELL: return who + " sells a house on " + card;
        case ActionKind::MORTGAGE: return who + " mortgages " + card;
        case ActionKind::UNMORTGAGE: return who + " unmortgages " + card;
        case ActionKind::TRADE:
        case ActionKind::TRADE_CANCEL:
            return std::string(a.kind == ActionKind::TRADE ? "trade " : "trade cancelled ") + who + " gives [" +
                   itemsToString(a.trade.offerCards, a.trade.offerMoney) + "] / " + playerName(s, a.target) +
                   " gives [" + itemsToString(a.trade.requestCards, a.trade.requestMoney) + "]";
        case ActionKind::BANKRUPT: return who + " goes bankrupt (creditor: " + playerName(s, a.target) + ")";
        case ActionKind::SET_MONEY: return "[override] " + who + " money = " + money(a.amount);
        case ActionKind::SET_OWNER: return "[override] " + card + " owner = " + playerName(s, a.target);
        case ActionKind::SET_HOUSES: return "[override] " + card + " houses = " + std::to_string(a.amount);
        case ActionKind::SET_POS: return "[override] " + who + " position = " + std::to_string(a.amount);
    }
    return "?";
}

std::string describeEvent(const GameEvent& e, const GameState& s) {
    std::string who = playerName(s, e.playerId);
    auto cards = [](const std::vector<CardId>& v) {
        std::string r;
        for (CardId c : v) r += (r.empty() ? "" : ",") + DataMaster::toString(c);
        return r.empty() ? std::string("-") : r;
    };
    auto sq = [](int pos) { return (pos >= 0 && pos < 40) ? DataMaster::board()[pos].label : std::string("?"); };
    std::string after = " (" + who + ": " + money(e.playerMoneyAfter) + ")";
    switch (e.type) {
        case ActionType::PURCHASE: return who + " bought " + cards(e.cardsGained) + " for " + money(-e.moneyChange) + after;
        case ActionType::PASS: return who + " passed";
        case ActionType::READY: return who + " ready";
        case ActionType::PHASE_CHANGE: return "phase changed";
        case ActionType::ROLL_AND_MOVE:
            return who + " rolled " + std::to_string(e.dice1) + "+" + std::to_string(e.dice2) + " -> [" +
                   std::to_string(e.position) + "] " + sq(e.position) +
                   ((s.settings.doublesRollAgain && e.dice1 == e.dice2) ? " (doubles)" : "");
        case ActionType::GO_BONUS: return who + " GO bonus +" + money(e.moneyChange) + after;
        case ActionType::PAY_TAX: return who + " paid tax " + money(-e.moneyChange) + after;
        case ActionType::PAY_RENT:
            return who + " paid rent " + money(-e.moneyChange) + " to " + playerName(s, e.targetPlayerId) + " (" +
                   cards(e.cardsLost) + ")" + after;
        case ActionType::LAND_UNOWNED: return who + " landed on unowned " + cards(e.cardsGained) + " (buy / decline / auction)";
        case ActionType::DECLINE: return who + " declined " + cards(e.cardsLost);
        case ActionType::AUCTION:
            if (e.playerId < 0) return "auction of " + cards(e.cardsLost) + ": no winner";
            return who + " won auction " + cards(e.cardsGained) + " for " + money(-e.moneyChange) + after;
        case ActionType::END_TURN: return who + " ended turn";
        case ActionType::TRADE_EXECUTE:
            return "trade: " + who + " got [" + cards(e.cardsGained) + "] gave [" + cards(e.cardsLost) + "] money " +
                   (e.moneyChange >= 0 ? "+" : "") + money(e.moneyChange) + " with " + playerName(s, e.targetPlayerId);
        case ActionType::TRADE_CANCEL: return "trade cancelled: " + who + " <-> " + playerName(s, e.targetPlayerId);
        case ActionType::BUILD_HOUSE: return who + " built on " + cards(e.cardsGained) + after;
        case ActionType::SELL_HOUSE: return who + " sold a house on " + cards(e.cardsLost) + after;
        case ActionType::MORTGAGE: return who + " mortgaged " + cards(e.cardsLost) + " +" + money(e.moneyChange) + after;
        case ActionType::UNMORTGAGE: return who + " unmortgaged " + cards(e.cardsGained) + " " + money(e.moneyChange) + after;
        case ActionType::BANKRUPT: return who + " BANKRUPT -> assets to " + playerName(s, e.targetPlayerId);
        case ActionType::GAME_OVER: return "GAME OVER - winner: " + who;
        case ActionType::OVERRIDE: return "[override] " + who;
    }
    return "?";
}

}  // namespace GameLog
