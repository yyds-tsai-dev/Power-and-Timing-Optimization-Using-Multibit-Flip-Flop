# BOOTSTRAP — DAC 2027 極限優化(給接手的 Claude session)

**你是誰、在做什麼**:接續 DAC 2027 優化戰役(score + runtime 雙軸)。主計畫:`knowledge/plans/plan_dac2027_optimization.md`(先讀它,再讀本檔的「當前狀態」)。使用者授權「照計畫走」。

## 讀取順序

1. `knowledge/plans/plan_dac2027_optimization.md` — 作戰計畫(Phase 0 已完成,Phase 1 進行中)
2. `knowledge/reports/2026-07-11_evaluator_semantics_blackbox.md` — **evaluator 語義已被完整解碼**(16 實驗零誤差),這是全案地基
3. `knowledge/reports/2026-07-11_dac_phase0_analysis.md` — Phase 0 分析(時間解剖/預算探測/profiling)
4. `knowledge/memory/MEMORY.md` — 專案長期記憶索引(42 個記憶檔在同目錄,按需讀)

## 當前狀態(2026-07-11,server A)

- **P1a 已 commit**(30eef42 + 後續診斷 commit):`EVAL_ANCHOR=1` 讓 oracle 家族錨定 parse 真值(Preprocess 的 1-hop 重錨誤差被繞開)。byte-exact off 已驗證。**tc2 全配置 723,165 → 720,451(−0.375%)**
- **殘差 404 TNS(base 態、靜態)**:已排除 fallback 覆蓋(n=0)與 per-path 語義(爆到 107k);黑盒解碼證實 evaluator = max-max STA = 我們的模型形狀 → 殘差是**實作偏差**,server A 上有 refsta workflow(離線 Python 參考 STA,逐位重現 evaluator 再 per-FF diff)在跑——**結果報告若不在 knowledge/reports/,表示還沒完成,先重建 refsta**(spec 在計畫與本檔末尾)
- **6 案 EVAL_ANCHOR sweep** 在 server A 跑動中——結果同上處理
- 下一步佇列:refsta 修完殘差 → 7 案 gate → Phase 2f 墳場翻案(計畫裡有清單)→ Phase 3 runtime(de-hashing/early-exit,profiling 證據在 phase0 報告)

## 環境開機

```bash
git clone <this repo> && cd 2024-ICCAD-Problem-B && git checkout v3_experimental
make boost && make release        # g++≥9;LEMON vendored;OR-Tools 不需要
# 標準跑法(unified config)見 README.md;EVAL_ANCHOR=1 疊加即為 P1a 配置
```

## 環境陷阱(server A 的血淚,新環境自行對應)

- **長跑一律把 binary cp 到暫存目錄再跑**(rebuild 換 inode 會弄死跑到一半的 run)
- 背景 shell cwd 會重置——腳本第一行永遠明確 `cd`
- 測資在 repo `testcase/`(53 檔含全部 7 contest 案);server A 的專案根另有分居目錄,repo 內不受影響
- gprof+OpenMP 出不了 gmon.out——profiling 用 gdb 取樣法(`gdb -p PID -batch -ex "thread apply all bt 4"` 每 3s)
- 評分永遠跑 `evaluator/preliminary-evaluator` 本尊 + 兩個 checker;`BANKING_MODE=matching PRODUCTION=1` 必掛
- 數字凍結紀律:任何文字改動用 numeric-token 多重集前後比對自驗

## refsta 規格(若需重建)

離線參考 STA(Python):讀 input+.out,照黑盒解碼語義(max-max、兩點 HPWL、gate 零延遲、Qpd 進 max、parse 錨)算完整分數,**必須逐位重現 evaluator Final score**;然後 per-logical-bit slack 與 C++ `computeAccurateTNS`(EVAL_ANCHOR=1)對比,分類發散 bit 的結構 → 找出實作偏差。C++ 側已有 EVAL_DIAG 鉤子(src/Manager.cpp,computeAccurateTNS 內)。

## Phase 2f 墳場翻案的上下文(計畫裡的 R1–R3)

翻案所需的歷史全在包內:
- **postmortem 摘要**:`knowledge/memory/` 裡的 project_match_higher_bit、project_dp_slot_assign、project_tc2_exhaustive_analysis、project_unbank_rebank、project_approach_c_deadend、project_stage3_lp_deadend、project_egr_implemented(每份含當年數據與死因)
- **詳細報告**:`knowledge/graveyard/`——tc2 十四連敗的完整研究(2026-04-26)、tc2 驗證診斷(2026-06-14)、深度機會研究(2026-05-03)、Approach C 原計畫、phase 逐日誌
- **env gates 全在程式碼**:MATCH_HIGHER_BIT、DP_SLOT_ASSIGN(+INTRA_ONLY)、ALG2_LP_BANK、POST_LG_DECLUSTER、EGR——翻案=舊 gate + `EVAL_ANCHOR=1` 重測,不用重寫
- **鐵律**:翻案前先確認 refsta 殘差已修(滿血定價)+ 逐案跑 7 案 gate;Steiner/net-HPWL 永久死亡(語義解碼釘棺),別碰

## 與 server A 的同步協議

server A 的新結果會以「更新 knowledge/ + push」的形式送上來;開工前先 `git pull`。你產出的報告也寫進 `knowledge/reports/` 並 commit push(私有 repo,可放心)。
