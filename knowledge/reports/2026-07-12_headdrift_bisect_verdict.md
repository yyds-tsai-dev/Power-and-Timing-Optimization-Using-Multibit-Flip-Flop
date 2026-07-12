# 紅旗判決:HEAD 漂移 bisect(server B,2026-07-12)

**Type**: diagnosis(回覆 A 的紅旗程序,HANDOFF_A_SIDE §0)
**判決:e7df424→HEAD 在 v2-config 路徑零漂移,代碼無洩漏;+434k 為 A 機本地現象,
頭號嫌疑 = 負載壓進時間牆。**

## Bisect 結果(六點,tc1 v2-config,server B,各自 worktree 獨立建置)

| 時點 | tc1 分數 | 判讀 |
|---|---|---|
| 968c2b3(P1b,1bis 前) | 734,696,559.983051 | 基準前點 |
| e7df424(1bis) | 734,700,499.246088 | 1bis 照設計移動 **+3,939(+0.0005%)** |
| c2e2490(R1/PLR) | 734,700,499.246088 | 逐位同 ✓ |
| dc423ef(R2c') | 734,700,499.246088 | 逐位同 ✓ |
| cf7675f(LNS v1) | 734,700,499.246088 | 逐位同 ✓ |
| d182ba1(HEAD,含 conv-term) | 734,700,499.246088 | 逐位同 ✓ |

另:此值與本機稍早 knob 矩陣 base(不同 session、不同 build)逐位吻合——
**七次獨立建置/執行完全再現,server B 的決定性無可挑剔**。

## 對 A 的 +434,343 的解讀

1. **不是 e7df424..HEAD 的代碼**(上表)。
2. **不是 1bis**(量級差 110×;方向雖同但 +3.9k ≠ +434k)。
3. **A 機本地因素**,按可能性排序:
   - **負載壓進時間牆**(頭號):tc1 在輕載下 ~400s 收斂、不撞牆;重載下
     bitRepair 被拖進 1200s cap → 少做工 → 分數壞數十萬級。
     Server B 前例:hc04 T8 在 load 61 同 binary 兩窗口差 +200k
     (`2026-07-11_phase3b_thread_scaling.md` 撞牆案節);A 自己觀察到
     tc3 ±182k 負載擺動——同一機制。
     **驗證法**:比對兩次 run 的 [STAGE] bitRepair 秒數與 [REBANK]/round 數;
     若 no-CONV 基線 run 的 bitRepair ≈1200s 而 interim run 遠低於此,結案。
   - interim binary 出處(次嫌):flags/march/-static、或 P1b 前時點。
4. **建議**:A 安靜窗口(或 T64)重跑 no-CONV 基線一次;若 +434k 消失/大幅縮小
   → 紅旗解除、v3 解凍(7/13 閘照 four_rulings);順帶採 B 的成對同窗口紀律
   於一切撞牆案 A/B。

## 附帶確認

- conv-term(cef3cc1)在 B 機 v2 路徑 byte-exact ✓(HEAD 含之,分數逐位同)。
- 1bis 的 +3.9k(tc1)為真實定價修正代價,遠低於 0.5% gate(0.0005%),
  且換來 tc1 錯價 −23,875 TNS 的修復——淨值明確為正。

工件:server B `~/scratch/bisect/`(六 worktree、六 binary、bisect.log、全部 .out)。
