# v3 加冕包 — A 側執行套件(B 備,2026-07-13)

**B 側已完賽**:rep1 + rep2 **七案全部逐位相同**(比「3× 內部穩定」更強,
故 rep3 依裁定免跑;證據 `reports/2026-07-13_exp_v3_ritual_b_side.md`)。
執行緒不變性 8/16/32/64/128 五檔逐位(`2026-07-13_exp_t128_probe.md`)。
**只剩 A 側的 N× 跨機對帳。**

## A 側一行式

```bash
# 從 repo 根目錄;THREADS 自由(凍結清單:A 建議 8 或 40),REPS 照規格 3
# (B 先例:若 rep2 與 rep1 逐位相同,rep3 可裁定免跑)
THREADS=40 REPS=3 bash knowledge/coronation/a_ritual.sh
```

產物:`coronation_a/ritual_a.log`(每 run 分數 + sanity + placement_checker +
env dump + loadavg)與 `coronation_a/XMACHINE.tsv`(自動對 B 參考分數,
PASS/FAIL 每案)。

## Binary 提示

v3 之後的所有 commits 都是 default-off byte-exact gates(LNS_SPLIT 等)+
docs——**用當前 HEAD build 即可**,凍結 env 下行為與凍結期 binary 逐位同。
不放心就 checkout `d182ba1` build,結果應一致(不一致本身就是發現,回報)。

## B 側參考分數(rep1 ≡ rep2,T64)

| Case | Score(B) |
|---|---|
| testcase2_0812 | 705140.829646587 |
| testcase1_0812 | 734700023.359553 |
| hiddencase02 | 9678012.79050386 |
| hiddencase03 | 55771346.6806144 |
| testcase3 | 725803320.322809 |
| hiddencase01 | 29993040.5219705 |
| hiddencase04 | 725980967.603273 |

## 加冕判準(凍結清單原文)

七案 N× 內部穩定(byte 或 f32 噪音)+ **跨機分數一致(f32 噪音級)** +
全 checker 綠。跨機不一致 = conv-term 覆蓋不全的 bug,回報修復(這正是
DAC 稿 determinism 主張的實驗本體)。

## 加冕通過後

1. B 彙整跨機一致性表 → v3 report。
2. README v3 欄(已在 `v3_experimental` 就位,標「pending」)轉正,merge main。
3. v4 菜單開工:R1(`REBANK_MODES=15`,疊加證據已齊:6勝1平,
   `2026-07-13_exp_r1_x_v3_stacking.md`)+ LNS v1.5 + de-hashing(A 的)。
