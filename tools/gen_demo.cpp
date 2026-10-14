// デモ用の対局ログを作る（tools/demo_full_game.log の元）。
// ビルド例: g++ -std=c++17 -Icore/include tools/gen_demo.cpp <monopoly_core のライブラリ> -o gen_demo
// 実行: gen_demo 15 demo_full_game.log   （15 = 乱数シード）
#include "AIPlayer.hpp"
#include "GameLog.hpp"
#include "MonopolyEngine.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <random>

static bool apply(MonopolyEngine& e, const Action& a) {
    auto r = e.apply(a);
    return r.ok;
}

int main(int argc, char** argv) {
    unsigned seed = argc > 1 ? std::stoul(argv[1]) : 1;
    bool write = argc > 2;
    MonopolyEngine e;
    std::vector<std::string> names = {"riku", "Aoi", "Ken", "Mina"};
    e.newGame(names, Settings());
    std::vector<AIPlayer> ais;
    for (int i = 0; i < 4; ++i) ais.emplace_back(i, AIDifficulty(i % 4), seed * 31 + i);
    // ① 購入フェーズ
    int guard = 0;
    // 買い方の性格: riku=高いものから / Aoi=安いものから / Ken=現金900万は残す / Mina=気まぐれ・600万で打ち止め
    std::mt19937 prng(seed * 7 + 3);
    while (e.getState().phase == Phase::PURCHASE && guard++ < 500) {
        const GameState& st = e.getState();
        const int p = st.activePlayer;
        const int money = st.players[p].money;
        const int keep[4] = {150, 200, 900, 600};
        std::vector<CardId> cand;
        for (const Card& c : DataMaster::cards()) {
            if (money - c.price < keep[p]) continue;
            Action t; t.kind = ActionKind::BUY; t.player = p; t.card = c.id;
            if (e.validate(t).ok) cand.push_back(c.id);
        }
        Action a; a.player = p;
        if (cand.empty()) {
            a.kind = ActionKind::PASS;
        } else {
            auto price = [](CardId c) { return DataMaster::card(c).price; };
            CardId pick = cand.front();
            if (p == 0) { for (CardId c : cand) if (price(c) > price(pick)) pick = c; }
            else if (p == 1) { for (CardId c : cand) if (price(c) < price(pick)) pick = c; }
            else if (p == 2) { pick = cand[cand.size() / 2]; }
            else { pick = cand[prng() % cand.size()]; }
            a.kind = ActionKind::BUY;
            a.card = pick;
        }
        if (!apply(e, a)) return 1;
    }
    if (!write) {
        for (const auto& pl : e.getState().players) std::cerr << "  " << pl.name << " cash=" << pl.money << " cards=" << e.getState().ownedBy(pl.id).size() << "\n";
    }
    // ② 交渉: 物件グループをそろえる取引を試す
    const GameState* s = &e.getState();
    std::set<int> done;
    int trades = 0;
    bool cancelled = false;
    for (int round = 0; round < 3; ++round)
        for (int p = 0; p < 4; ++p) {
            if (done.count(p)) continue;
            for (GroupType g : {GroupType::ORANGE, GroupType::SKY_BLUE, GroupType::RED, GroupType::PINK, GroupType::YELLOW, GroupType::BROWN}) {
                auto cards = DataMaster::groupCards(g);
                bool mine = false;
                for (CardId c : cards) if (e.getState().card(c).owner == p) mine = true;
                if (!mine) continue;
                // 他人のカードを1枚ずつ、お金 or 別グループのカードと交換
                bool all = true;
                for (CardId c : cards) {
                    int o = e.getState().card(c).owner;
                    if (o == p) continue;
                    if (o < 0) { all = false; break; }
                    Action t; t.kind = ActionKind::TRADE; t.player = p; t.target = o;
                    t.trade.proposerId = p; t.trade.targetId = o;
                    t.trade.requestCards = {c};
                    // 相手が欲しがりそうな自分のカード（相手がその色を持っている）
                    CardId give = CardId::NONE;
                    for (CardId m : e.getState().ownedBy(p)) {
                        if (DataMaster::card(m).group == g) continue;
                        for (CardId x : DataMaster::groupCards(DataMaster::card(m).group))
                            if (e.getState().card(x).owner == o) give = m;
                    }
                    if (give != CardId::NONE) t.trade.offerCards = {give};
                    t.trade.offerMoney = std::max(0, DataMaster::card(c).price * 13 / 10 -
                                                         (give != CardId::NONE ? DataMaster::card(give).price : 0));
                    if (!cancelled && trades == 1) {  // 1回は破談として記録
                        Action cx = t; cx.kind = ActionKind::TRADE_CANCEL;
                        cx.trade.offerMoney = DataMaster::card(c).price / 2;
                        apply(e, cx);
                        cancelled = true;
                    }
                    if (!apply(e, t)) { all = false; break; }
                    ++trades;
                }
                if (all) { done.insert(p); break; }
            }
        }
    Action ar; ar.kind = ActionKind::ALL_READY; ar.player = -1;
    if (!apply(e, ar)) return 2;
    // ③ 運用
    guard = 0;
    while (e.getState().phase == Phase::OPERATIONAL && guard++ < 3000) {
        auto a = ais[e.getState().activePlayer].decide(e);
        if (a && a->kind == ActionKind::DECLINE && prng() % 2 == 0) {
            // 見送ったカードを半分くらいはオークションに（いちばんお金のある人が 8 割の値段で落札）
            const GameState& st = e.getState();
            const int price = DataMaster::card(st.pendingPurchase).price * 8 / 10;
            int win = -1;
            for (const auto& pl : st.players)
                if (!pl.bankrupt && pl.id != st.activePlayer && pl.money - price >= 150 && (win < 0 || pl.money > st.players[win].money)) win = pl.id;
            if (win >= 0) { a->kind = ActionKind::AUCTION; a->target = win; a->amount = price; }
        }
        if (!a || !apply(e, *a)) { std::cerr << "stuck\n"; return 3; }
    }
    // 統計
    std::map<std::string, int> cnt;
    for (const auto& ev : e.getEvents()) cnt[std::to_string(int(ev.type))]++;
    int doubles = 0, urolls = 0, builds = 0, morts = 0, taxes = 0;
    for (const auto& a : e.getActions()) {
        if (a.kind == ActionKind::ROLL && a.d1 == a.d2) ++doubles;
        if (a.kind == ActionKind::UTILITY_ROLL) ++urolls;
        if (a.kind == ActionKind::BUILD) ++builds;
        if (a.kind == ActionKind::MORTGAGE) ++morts;
    }
    int auctions = 0;
    for (const auto& a : e.getActions()) if (a.kind == ActionKind::AUCTION) ++auctions;
    for (const auto& ev : e.getEvents()) if (ev.type == ActionType::PAY_TAX) ++taxes;
    std::cout << seed << " actions=" << e.getActions().size() << " turn=" << e.getState().turn << " trades=" << trades
              << " doubles=" << doubles << " uroll=" << urolls << " build=" << builds << " mort=" << morts
              << " tax=" << taxes << " auction=" << auctions << " finished=" << (e.getState().phase == Phase::FINISHED) << "\n";
    if (write) {
        std::ofstream f(argv[2], std::ios::binary);
        f << GameLog::serialize(e);
    }
}
