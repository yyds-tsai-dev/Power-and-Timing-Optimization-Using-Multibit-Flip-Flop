# tc1 v2 record(734,266,279)出處調查(2026-07-16,server B)

**Type**: diagnosis
**動機**:tc1 是 v4 唯一未破的「紀錄」。gap 二分(`~/scratch/tc1gap/gap.log`)
把 v4 knobs 全數排除後,剩餘 +434k 指向紀錄本身的出處問題。

## 1. 證據鏈(全部 server B,今日實測)

| 實驗 | 結果 | 排除的假說 |
|---|---:|---|
| v4 逐 knob 拔除 | ADAPT_STACK=0 效應、R1 −302k、LNS −30k、conv **−476(幫忙)** | v4 knobs 不是 gap 來源 |
| 純 v2 env(README 配方)@ 今 binary | 734,700,499 | — |
| **六時點歷史 binary 掃**(968c2b3→d182ba1,含 1bis commit 本尊 e7df424) | 968c2b3: 734,696,560;**e7df424 起全部 734,700,499(零漂移)** | 「凍結前 commit 造成漂移」——**整個 bisect 範圍內無一 binary 能重現 record** |
| 純 v2 env **去 ORACLE_EJECT**(紅旗根因重演) | 737,190,067(反向大差)| 幽靈基線(A 漏 EJECT)假說 |
| e7df424 @ **T=8**(record 原配方執行緒) | **734,700,499(與 T=64 逐分同)** | 舊 binary 的執行緒敏感假說 |

## 2. 文獻出處

`2026-07-11_p1_sweep_results.md`:record 表 = **server A、repeat #1、
binary「1bis」**(當時 P1d 三重跑進行中)。1bis 的正式 commit = e7df424
(07-11 16:32)——**該 commit 的 binary 在 B 上給 734,700,499,非 record**。

## 3. 剩餘假說(B 側無法再判,需 A 一發)

1. **A 的「1bis」是 commit 前的工作樹**(repeat #1 跑在未定稿的 tree 上)
   → record 是不可重現的 WIP 工件。
2. **tc1 跨機分歧**(A 機給 734.27M、B 機給 734.70M)→ 對加冕流程是
   紅色警訊,必須先解再加冕。
**請 A 跑一發**:`v2 env(README 配方)+ 當前 HEAD binary + tc1`,
一個數字分辨兩者:734.70M → 假說 1(record 註記 WIP 工件,README 加星);
734.27M → 假說 2(跨機分歧,升級處理)。已寫入加冕包檢查清單。

## 4. 副產物:postBanking「1/50 翻位」追獵進度

helgrind 全管線 **races=0** → 非經典 data race。精煉嫌疑:**OMP 動態排程
下的 FP 累加序**(load-dependent、無 race、helgrind 不可見——與觀測
完全吻合:同機同 env 獨跑逐位同、共跑偶發同分不同位元)。
下一步(排隊):grep postBanking→RELOC 段的 `schedule(dynamic)` +
浮點累加組合,改定序 reduction(gated)。

## 5. 對帳面的影響

- v4 = **在可重現基線上的全案最佳**(B 側任何 binary/env 組合都無法重現
  舊 tc1 record)。README 的 v2 tc1 粗體**暫不動**,等 A 的一發定性後
  再決定加星或修正。
- v4 凍結不受影響(gap 不來自 v4 的任何 knob)。
