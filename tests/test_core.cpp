// コアの最小テスト（フレームワーク不要）。失敗があれば exit code 1
#include "AIPlayer.hpp"
#include "GameLog.hpp"
#include "MonopolyEngine.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

static int g_fail = 0, g_pass = 0;
#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (cond) ++g_pass;                                                          \
        else { ++g_fail; std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; } \
    } while (0)

static Action act(ActionKind k, int p, CardId c = CardId::NONE) {
    Action a; a.kind = k; a.player = p; a.card = c; return a;
}
static Action roll(int p, int d1, int d2) {
    Action a = act(ActionKind::ROLL, p); a.d1 = d1; a.d2 = d2; return a;
}
static bool ok(MonopolyEngine& e, const Action& a) {
    ApplyResult r = e.apply(a);
    if (!r.ok) std::cerr << "   rejected: " << GameLog::actionToLine(a) << " : " << r.error << "\n";
    return r.ok;
}
static bool ng(MonopolyEngine& e, const Action& a) { return !e.apply(a).ok; }
static int money(const MonopolyEngine& e, int p) { return e.getState().players[p].money; }

enum { A = 0, B = 1, C = 2 };

static void scriptedGame(MonopolyEngine& e) {
    Settings noDoubles;
    noDoubles.doublesRollAgain = false;  // このシナリオは旧ルール（ぞろ目なし・公共施設は移動の出目）で数値を確認
    noDoubles.utilityRentRoll = false;
    noDoubles.doubleRentOnSet = false;
    noDoubles.endOnBankrupt = false;
    e.newGame({"A", "B", "C"}, noDoubles);
    using K = ActionKind;

    // ---- 購入フェーズ
    CHECK(ng(e, act(K::BUY, B, CardId::P_BR2)));           // 手番違い
    CHECK(ok(e, act(K::BUY, A, CardId::P_BR1)));
    CHECK(ok(e, act(K::BUY, B, CardId::P_BR2)));
    CHECK(ok(e, act(K::BUY, C, CardId::R_1)));
    CHECK(ng(e, act(K::BUY, A, CardId::P_BR2)));           // 所有済み
    CHECK(ok(e, act(K::BUY, A, CardId::R_2)));
    CHECK(ok(e, act(K::BUY, B, CardId::R_3)));
    CHECK(ng(e, act(K::BUY, C, CardId::R_4)));             // 同一グループ制限
    CHECK(ok(e, act(K::BUY, C, CardId::U_1)));
    CHECK(ok(e, act(K::PASS, A)));
    CHECK(ok(e, act(K::PASS, B)));
    CHECK(e.getState().phase == Phase::PURCHASE);
    CHECK(ok(e, act(K::PASS, C)));
    CHECK(e.getState().phase == Phase::NEGOTIATION);
    CHECK(money(e, A) == 1240 && money(e, B) == 1240 && money(e, C) == 1150);

    // ---- 交渉フェーズ
    Action t = act(K::TRADE, A);
    t.target = B;
    t.trade.proposerId = A; t.trade.targetId = B;
    t.trade.offerCards = {CardId::R_2}; t.trade.offerMoney = 50;
    t.trade.requestCards = {CardId::P_BR2};
    Action empty = act(K::TRADE, A);
    empty.target = B;
    empty.trade.proposerId = A; empty.trade.targetId = B;
    CHECK(ng(e, empty));                                     // 空の取引は不可
    Action tc = t; tc.kind = K::TRADE_CANCEL;
    CHECK(ok(e, tc));
    CHECK(e.getOwnerOfCard(CardId::P_BR2) == B);            // 破談は状態を変えない
    CHECK(ok(e, t));
    CHECK(money(e, A) == 1190 && money(e, B) == 1290);
    CHECK(e.getOwnerOfCard(CardId::P_BR2) == A && e.getOwnerOfCard(CardId::R_2) == B);
    CHECK(ng(e, roll(A, 1, 1)));                             // まだ運用フェーズでない
    CHECK(ok(e, act(K::READY, A)));
    CHECK(ok(e, act(K::READY, B)));
    CHECK(ok(e, act(K::READY, C)));
    CHECK(e.getState().phase == Phase::OPERATIONAL && e.getState().turn == 1);

    // ---- 運用フェーズ Turn 1
    CHECK(ok(e, roll(A, 2, 3)));                             // pos5 R-1(C, 1枚) → 25
    CHECK(money(e, A) == 1165 && money(e, C) == 1175);
    CHECK(ok(e, act(K::BUILD, A, CardId::P_BR1)));
    CHECK(ng(e, act(K::BUILD, A, CardId::P_BR1)));          // 均等建設違反
    CHECK(ok(e, act(K::BUILD, A, CardId::P_BR2)));
    CHECK(money(e, A) == 1065);
    CHECK(ng(e, roll(A, 1, 1)));                             // 2回振れない
    CHECK(ok(e, act(K::END_TURN, A)));
    CHECK(ok(e, roll(B, 1, 3)));                             // pos4 TAX-1
    CHECK(money(e, B) == 1090);
    CHECK(ok(e, act(K::END_TURN, B)));
    CHECK(ok(e, roll(C, 1, 2)));                             // pos3 P-BR2 家1 → 20
    CHECK(money(e, C) == 1155 && money(e, A) == 1085);
    CHECK(ok(e, act(K::END_TURN, C)));
    CHECK(e.getState().turn == 2 && e.getState().activePlayer == A);

    // ---- Turn 2
    CHECK(ok(e, roll(A, 6, 4)));                             // pos15 R-2(B は鉄道2枚) → 50
    CHECK(money(e, A) == 1035 && money(e, B) == 1140);
    CHECK(ok(e, act(K::END_TURN, A)));
    CHECK(ok(e, roll(B, 4, 4)));                             // pos12 U-1(C 1枚) → 8x4=32
    CHECK(money(e, B) == 1108 && money(e, C) == 1187);
    CHECK(ok(e, act(K::END_TURN, B)));
    CHECK(ok(e, roll(C, 3, 3)));                             // pos9 P-SB3 未所有
    CHECK(e.getState().pendingPurchase == CardId::P_SB3);
    CHECK(ng(e, act(K::END_TURN, C)));                       // 未決定のまま終われない
    Action au = act(K::AUCTION, C); au.target = A; au.amount = 90;
    CHECK(ok(e, au));
    CHECK(e.getOwnerOfCard(CardId::P_SB3) == A && money(e, A) == 945);
    CHECK(ok(e, act(K::END_TURN, C)));

    // ---- Turn 3
    CHECK(ok(e, roll(A, 6, 6)));                             // pos27 P-YE2 未所有
    CHECK(ok(e, act(K::BUY, A)));
    CHECK(money(e, A) == 685);
    CHECK(ok(e, act(K::END_TURN, A)));
    CHECK(ok(e, roll(B, 5, 5)));  CHECK(ok(e, act(K::END_TURN, B)));   // pos22
    CHECK(ok(e, roll(C, 6, 5)));  CHECK(ok(e, act(K::END_TURN, C)));   // pos20

    // ---- Turn 4
    CHECK(ok(e, roll(A, 6, 6)));                             // pos39 P-DB2
    CHECK(ok(e, act(K::DECLINE, A)));
    CHECK(e.getOwnerOfCard(CardId::P_DB2) == -1);
    CHECK(ok(e, act(K::END_TURN, A)));
    CHECK(ok(e, roll(B, 1, 2)));  CHECK(ok(e, act(K::END_TURN, B)));   // pos25 R-3 自分
    CHECK(money(e, B) == 1108);
    CHECK(ok(e, roll(C, 1, 1)));  CHECK(ok(e, act(K::END_TURN, C)));   // pos22

    // ---- Turn 5
    CHECK(ok(e, roll(A, 1, 1)));                             // 39→1 GO通過 +200
    CHECK(money(e, A) == 885 && e.getState().players[A].position == 1);
    CHECK(ng(e, act(K::MORTGAGE, A, CardId::P_BR1)));       // 家があると抵当不可
    CHECK(ok(e, act(K::SELL, A, CardId::P_BR1)));
    CHECK(ok(e, act(K::SELL, A, CardId::P_BR2)));
    CHECK(money(e, A) == 935);
    CHECK(ok(e, act(K::MORTGAGE, A, CardId::P_BR1)));
    CHECK(money(e, A) == 965);
    CHECK(e.calculateRent(CardId::P_BR1, 7) == 0);          // 抵当中は家賃0
    CHECK(ok(e, act(K::UNMORTGAGE, A, CardId::P_BR1)));     // 30 * 1.1 = 33
    CHECK(money(e, A) == 932);
    CHECK(ok(e, act(K::END_TURN, A)));

    Action sm = act(K::SET_MONEY, B); sm.amount = -100;
    CHECK(ok(e, sm));
    Action br = act(K::BANKRUPT, B); br.target = A;
    CHECK(ok(e, br));
    CHECK(e.getState().players[B].bankrupt);
    CHECK(e.getOwnerOfCard(CardId::R_2) == A && e.getOwnerOfCard(CardId::R_3) == A);
    CHECK(e.getState().activePlayer == C);
    CHECK(e.calculateRent(CardId::R_2, 7) == 50);           // A は鉄道2枚 (R-2,R-3)
    Action br2 = act(K::BANKRUPT, C);
    CHECK(ok(e, br2));
    CHECK(e.getState().phase == Phase::FINISHED);
}

static bool roundTrip(MonopolyEngine& e, const std::string& file) {
    if (!GameLog::save(e, file)) return false;
    MonopolyEngine e2;
    GameLog::LoadResult r = GameLog::load(e2, file);
    if (!r.ok) std::cerr << "   load error line " << r.errorLine << ": " << r.error << "\n";
    return r.ok && e2.getState() == e.getState() && e2.getActions().size() == e.getActions().size() &&
           e2.getEvents().size() == e.getEvents().size();
}

static bool undoRedoAll(MonopolyEngine& e) {
    GameState fin = e.getState();
    size_t n = e.getActions().size();
    size_t nev = e.getEvents().size();
    while (e.undo()) {}
    if (e.getState() != e.getInitialState() || !e.getActions().empty()) return false;
    for (size_t i = 0; i < n; ++i)
        if (!e.redo()) return false;
    return e.getState() == fin && e.getEvents().size() == nev && !e.canRedo();
}

// ぞろ目でもう一度 / 出目の差し替え
static void doublesTest() {
    MonopolyEngine e;
    e.newGame({"A", "B"});
    using K = ActionKind;
    CHECK(ok(e, act(K::PASS, A)));
    CHECK(ok(e, act(K::PASS, B)));
    CHECK(ok(e, act(K::READY, A)));
    CHECK(ok(e, act(K::READY, B)));
    CHECK(e.getState().phase == Phase::OPERATIONAL);
    CHECK(ok(e, roll(A, 1, 1)));                       // [2] BLANK
    CHECK(e.getState().doublesStreak == 1 && !e.getState().hasRolled);
    CHECK(ng(e, act(K::END_TURN, A)));                 // ぞろ目なので終了できない
    CHECK(ok(e, roll(A, 2, 2)));                       // [6] P-SB1 未所有
    CHECK(e.getState().pendingPurchase == CardId::P_SB1);
    CHECK(ng(e, roll(A, 1, 2)));                       // 購入判断が先
    CHECK(ok(e, act(K::DECLINE, A)));
    CHECK(ok(e, roll(A, 1, 3)));                       // [10]
    CHECK(e.getState().doublesStreak == 0 && e.getState().hasRolled);
    CHECK(e.getState().players[A].position == 10);
    {   // ぞろ目は3回まで: 3回目のぞろ目では振り直しなし
        MonopolyEngine d;
        d.newGame({"A", "B"});
        CHECK(ok(d, act(K::PASS, A)));
        CHECK(ok(d, act(K::PASS, B)));
        CHECK(ok(d, act(K::READY, A)));
        CHECK(ok(d, act(K::READY, B)));
        CHECK(ok(d, roll(A, 1, 1)));                   // [2]
        CHECK(ok(d, roll(A, 5, 5)));                   // [12] U-1 未所有
        CHECK(ok(d, act(K::DECLINE, A)));
        CHECK(ok(d, roll(A, 3, 3)));                   // [18] 3回目
        CHECK(d.getState().hasRolled && d.getState().doublesStreak == 3);
        CHECK(ok(d, act(K::DECLINE, A)));
        CHECK(ng(d, roll(A, 1, 2)));                   // もう振れない
        CHECK(ok(d, act(K::END_TURN, A)));
    }
    // 出目の差し替え
    CHECK(e.replaceLast(roll(A, 5, 6)).ok);                // [17] BLANK
    CHECK(e.getState().players[A].position == 17 && e.getState().lastDice2 == 6);
    CHECK(!e.replaceLast(roll(A, 7, 1)).ok);          // 不正 → 元のまま
    CHECK(e.getState().players[A].position == 17);
    CHECK(ok(e, act(K::END_TURN, A)));
    CHECK(e.getState().activePlayer == B);
    // 保存/読込で設定が残る
    std::string text = GameLog::serialize(e);
    MonopolyEngine g;
    auto r = GameLog::deserialize(g, text);
    CHECK(r.ok && g.getState() == e.getState() && g.getState().settings.doublesRollAgain);

    // 所持金マイナスではターン終了できない
    Action sm = act(K::SET_MONEY, B);
    sm.amount = 5;
    CHECK(ok(e, sm));
    CHECK(ok(e, roll(B, 2, 2)));                       // [4] TAX-1 200 → マイナス（ぞろ目なのでもう一度）
    CHECK(money(e, B) < 0);
    CHECK(ok(e, roll(B, 1, 2)));                       // [7] BLANK
    CHECK(ng(e, act(K::END_TURN, B)));
    Action br = act(K::BANKRUPT, B);
    br.target = -1;
    CHECK(ok(e, br));
    CHECK(e.getState().phase == Phase::FINISHED);
}

// 公共施設: 止まったらもう一度振って 出目×4（1枚）/ ×10（2枚）
static void utilityTest() {
    MonopolyEngine e;
    e.newGame({"A", "B"});
    using K = ActionKind;
    CHECK(ok(e, act(K::BUY, A, CardId::U_1)));
    CHECK(ok(e, act(K::PASS, B)));
    CHECK(ok(e, act(K::PASS, A)));
    CHECK(ok(e, act(K::READY, A)));
    CHECK(ok(e, act(K::READY, B)));
    CHECK(ok(e, roll(A, 1, 2)));                       // A: [3] P-BR2
    CHECK(ok(e, act(K::DECLINE, A)));
    CHECK(ok(e, act(K::END_TURN, A)));
    Action pos = act(K::SET_POS, B);                   // B を [9] へ（管理者修正）
    pos.amount = 9;
    CHECK(ok(e, pos));
    const int before = money(e, B);
    CHECK(ok(e, roll(B, 1, 2)));                       // B: [12] U-1（A の所有）
    CHECK(e.getState().pendingUtility == CardId::U_1);
    CHECK(money(e, B) == before);                      // まだ払っていない
    CHECK(ng(e, act(K::END_TURN, B)));
    Action ur = roll(B, 4, 5);
    ur.kind = K::UTILITY_ROLL;
    CHECK(ok(e, ur));
    CHECK(money(e, B) == before - 9 * 4);
    CHECK(e.getState().pendingUtility == CardId::NONE);
    CHECK(ok(e, act(K::END_TURN, B)));
    std::string text = GameLog::serialize(e);
    MonopolyEngine g;
    CHECK(GameLog::deserialize(g, text).ok && g.getState() == e.getState());

    // 取引の簡易評価: 価格100のカードを120で売る → 売り手が少し得（妥当の範囲）
    Action sb = act(K::SET_OWNER, A);
    sb.card = CardId::P_SB1;
    sb.target = A;
    CHECK(ok(e, sb));
    TradeOffer t;
    t.proposerId = A; t.targetId = B;
    t.offerCards = {CardId::P_SB1};
    t.requestMoney = 120;
    const double sc = e.evaluateTradeFairness(t);
    CHECK(sc > 0 && sc < 30);

    // 破産: 払えなかった分は債権者から差し引く（ない袖は振れない）
    MonopolyEngine h;
    h.newGame({"A", "B"});
    CHECK(ok(h, act(K::BUY, A, CardId::P_BR1)));
    CHECK(ok(h, act(K::PASS, B)));
    CHECK(ok(h, act(K::PASS, A)));
    CHECK(ok(h, act(K::READY, A)));
    CHECK(ok(h, act(K::READY, B)));
    Action hs = act(K::SET_HOUSES, A);
    hs.card = CardId::P_DB2;
    hs.amount = 0;
    Action own = act(K::SET_OWNER, A);
    own.card = CardId::P_DB2;
    own.target = A;
    CHECK(ok(h, own));
    hs.amount = 5;
    CHECK(ok(h, hs));                                  // A の DB2 にホテル（地代 2000）
    CHECK(ok(h, roll(A, 1, 2)));
    CHECK(ok(h, act(K::DECLINE, A)));
    CHECK(ok(h, act(K::END_TURN, A)));
    Action bp = act(K::SET_POS, B);
    bp.amount = 36;
    CHECK(ok(h, bp));
    Action bm = act(K::SET_MONEY, B);
    bm.amount = 500;
    CHECK(ok(h, bm));
    const int aBefore = money(h, A);
    CHECK(ok(h, roll(B, 1, 2)));                       // [39] P-DB2 → 2000 払って -1500
    CHECK(money(h, B) == -1500 && money(h, A) == aBefore + 2000);
    Action bk = act(K::BANKRUPT, B);
    bk.target = h.getState().debtCreditor;
    CHECK(bk.target == A);
    CHECK(ok(h, bk));
    CHECK(money(h, A) == aBefore + 500);               // 実際に受け取れるのは B の持ち金 500 だけ
}

// 交渉の一括完了 / 脱落で終了 → 続行 / 独占で地代2倍（既定）
static void endTest() {
    MonopolyEngine e;
    e.newGame({"A", "B", "C"});
    using K = ActionKind;
    CHECK(e.getState().settings.doubleRentOnSet && e.getState().settings.endOnBankrupt);
    CHECK(ok(e, act(K::PASS, A)));
    CHECK(ok(e, act(K::PASS, B)));
    CHECK(ok(e, act(K::PASS, 2)));
    CHECK(e.getState().phase == Phase::NEGOTIATION);
    CHECK(ok(e, act(K::ALL_READY, -1)));
    CHECK(e.getState().phase == Phase::OPERATIONAL);
    // 独占: A に茶を2枚 → 地代 2→4
    for (CardId c : {CardId::P_BR1, CardId::P_BR2}) {
        Action o = act(K::SET_OWNER, A);
        o.card = c;
        o.target = A;
        CHECK(ok(e, o));
    }
    CHECK(e.calculateRent(CardId::P_BR1, 7) == 4);
    // B が破産 → 終了
    Action br = act(K::BANKRUPT, B);
    br.target = -1;
    CHECK(ok(e, br));
    CHECK(e.getState().phase == Phase::FINISHED);
    CHECK(ok(e, act(K::CONTINUE, -1)));
    CHECK(e.getState().phase == Phase::OPERATIONAL);
    CHECK(ng(e, act(K::CONTINUE, -1)));
    std::string text = GameLog::serialize(e);
    MonopolyEngine g;
    CHECK(GameLog::deserialize(g, text).ok && g.getState() == e.getState());
}

int main() {
    endTest();
    doublesTest();
    utilityTest();
    // 1. 手書きシナリオ（ルールの数値確認）
    MonopolyEngine e;
    scriptedGame(e);
    CHECK(roundTrip(e, "test_scripted.log"));
    CHECK(undoRedoAll(e));

    // 2. 途中まで戻して新しい操作 → redo 履歴は破棄される
    for (int i = 0; i < 5; ++i) e.undo();
    CHECK(e.canRedo());
    Action sm = Action(); sm.kind = ActionKind::SET_MONEY; sm.player = 0; sm.amount = 1;
    CHECK(e.apply(sm).ok);
    CHECK(!e.canRedo());

    // 3. AI 同士の長時間対戦 → 保存/読込一致 + undo/redo 一致
    for (unsigned seed = 1; seed <= 10; ++seed) {
        MonopolyEngine g;
        Settings st;
        st.doubleRentOnSet = (seed % 2 == 0);
        st.taxEnabled = (seed % 3 != 0);
        st.endOnBankrupt = (seed % 4 == 0);
        int n = 2 + static_cast<int>(seed % 9);  // 2〜10人
        std::vector<std::string> names;
        for (int i = 0; i < n; ++i) names.push_back("AI_" + std::to_string(i + 1));
        g.newGame(names, st);
        std::vector<AIPlayer> ais;
        for (int i = 0; i < n; ++i) ais.emplace_back(i, static_cast<AIDifficulty>((i + seed) % 4), seed * 100 + i);
        int steps = 0;
        bool stuck = false;
        while (g.getState().phase != Phase::FINISHED && steps < 3000) {
            bool acted = false;
            for (auto& ai : ais) {
                auto a = ai.decide(g);
                if (!a) continue;
                long before = 0;
                for (const auto& pl : g.getState().players) before += pl.money;
                ApplyResult r = g.apply(*a);
                if (r.ok) {
                    // お金の保存: 全員の所持金の変化 = 銀行とのやり取りの合計（家賃・取引はプレイヤー間で相殺）
                    long after = 0, bank = 0;
                    bool skip = false;
                    for (const auto& pl : g.getState().players) after += pl.money;
                    for (const auto& ev : g.getEventsOfAction(g.getActions().size() - 1)) {
                        switch (ev.type) {
                            case ActionType::PURCHASE: case ActionType::GO_BONUS: case ActionType::PAY_TAX:
                            case ActionType::AUCTION: case ActionType::BUILD_HOUSE: case ActionType::SELL_HOUSE:
                            case ActionType::MORTGAGE: case ActionType::UNMORTGAGE:
                                bank += ev.moneyChange; break;
                            case ActionType::BANKRUPT: case ActionType::OVERRIDE: skip = true; break;
                            default: break;
                        }
                    }
                    if (!skip && after - before != bank) {
                        std::cerr << "   money not conserved (seed " << seed << "): " << GameLog::actionToLine(*a)
                                  << " delta=" << (after - before) << " bank=" << bank << "\n";
                        ++g_fail;
                    }
                }
                if (!r.ok) {
                    std::cerr << "   AI action rejected (seed " << seed << "): " << GameLog::actionToLine(*a) << " : " << r.error << "\n";
                    stuck = true;
                    break;
                }
                acted = true;
                ++steps;
                break;
            }
            if (stuck || !acted) break;
        }
        CHECK(!stuck);
        CHECK(roundTrip(g, "test_ai.log"));
        CHECK(undoRedoAll(g));
        if (seed <= 3)
            std::cout << "  AI game seed " << seed << ": players=" << n << " actions=" << g.getActions().size()
                      << " turn=" << g.getState().turn
                      << (g.getState().phase == Phase::FINISHED ? " FINISHED" : " (cap)") << "\n";
    }

    // 4. 不正ログ
    {
        std::ofstream("test_v1.log") << "# MONOPOLY FULL EVENT LOG\n[PLAYERS]\nplayerCount=2\np:0,A\np:1,B\n[EVENTS]\n1,1,0,0,-1,0,0,-60,0,0,P-BR1,none\n";
        MonopolyEngine x;
        CHECK(!GameLog::load(x, "test_v1.log").ok);
        std::ofstream("test_bad.log") << "version=2\n[PLAYERS]\n0=A\n1=B\n[ACTIONS]\nBUY p=0 card=P-BR1\nBUY p=0 card=P-BR2\n";
        GameLog::LoadResult r = GameLog::load(x, "test_bad.log");
        CHECK(!r.ok && r.errorLine == 7 && r.appliedActions == 1);
        CHECK(x.getOwnerOfCard(CardId::P_BR1) == 0);          // 失敗行の直前まで適用されている
    }
    {   // 旧版で記録された空の取引は読み飛ばされる
        std::ofstream("test_empty_trade.log") << "version=2\n[PLAYERS]\n0=A\n1=B\n[ACTIONS]\nBUY p=0 card=P-BR1\nPASS p=1\nPASS p=0\nTRADE p=0 to=1 give=- get=-\nREADY p=0\n";
        MonopolyEngine x;
        GameLog::LoadResult r = GameLog::load(x, "test_empty_trade.log");
        CHECK(r.ok && r.appliedActions == 4 && r.skippedActions == 1);
        std::remove("test_empty_trade.log");
    }
    std::remove("test_scripted.log");
    std::remove("test_ai.log");
    std::remove("test_v1.log");
    std::remove("test_bad.log");

    std::cout << "passed: " << g_pass << "  failed: " << g_fail << "\n";
    return g_fail == 0 ? 0 : 1;
}
