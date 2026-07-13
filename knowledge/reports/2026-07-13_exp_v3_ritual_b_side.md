# v3 加冕儀式 — B 側結論(2026-07-13)

**Type**: exp
**判決:B 側 determinism 主張成立。rep2 七案全部與 rep1 逐位相同 → 按使用者
裁定免跑 rep3。** B 側儀式收官:14 runs(7×2),凍結 env
(`2026-07-13_v3_freeze_list.md`;`v3_ritual/env_dump.txt` 存證),T64。

## B 側 14-run 表

| Case | 分數(rep1 = rep2,逐位)| rep1 wall | rep2 wall† | sanity | 軸/機制 |
|---|---:|---:|---:|---|---|
| tc2 | 705,140.829646587 | 337s | 350s | ✓ | stack ON(tns_share=0.1455)|
| tc1 | 734,700,023.359553 | 100s | 102s | ✓ | stack off;conv −476 |
| hc02 | 9,678,012.79050386 | 567s | 764s† | ✓ | stack ON,−4.28% |
| hc03 | 55,771,346.6806144 | 389s | (並行)† | ✓ | off,= v2 base |
| tc3 | 725,803,320.322809 | 1131s | 1567s† | ✓ | conv 深收斂 −123k |
| hc01 | 29,993,040.5219705 | 98s | 124s† | ✓ | off,= v2 base |
| hc04 | 725,980,967.603273 | 1266s | 1735s† | ✓ | conv 深收斂 −103k |

† rep2 五案(hc02/hc03/tc3/hc01/hc04)以**兩條 64T 車道並行**補跑(使用者
指示並行化;正當性 = 執行緒不變性,見下),wall 含共跑干擾,**不作 runtime
參考**;分數與 .out 逐位不受影響(實證:7/7 BYTE-IDENTICAL,cmp 存證
`v3_ritual/ritual.log`)。

## Determinism 證據鏈(B 側,四個維度)

1. **重跑不變**:rep2 ≡ rep1 逐位,7/7(含 stack-ON 案 tc2/hc02 與撞牆深收斂案
   tc3/hc04——原預期這幾案有窗口噪音,實測**零噪音**,比預期更強)。
2. **執行緒不變**:tc2 8T/64T(rep1 期)+ tc3/hc02 128T(T128 探測,
   `2026-07-13_exp_t128_probe.md`)輸出皆逐位同 T64。
3. **負載不變**:rep2 兩案在 128 核共跑高負載下仍逐位同(load 46–62 起跑)。
4. **build 不變**(2026-07-13 下午補):全新 HEAD 編譯(b78d5473,新增 gates
   全關;canonical build 指令)七案凍結 env 輸出**全部逐位同 rep1**——
   紀錄可從原始碼 + env 完整重現,不依賴特定 binary 工件。
   (使用者裁定:不等 A 側跨機,以 B 側四軸自證為準。)

**含義**:凍結清單「執行緒各機自由」安全;跨機對帳只剩「不同機器」單一變因。

## 環境限制(照 handoff 註明)

`checker/placement_checker` 在 B 機無法執行(需 GLIBCXX_3.4.29,本機 glibc
2.28);合法性以 sanity(7/7 pass)+ evaluator 內建 placeViol(反組譯證據:
score() 前強制 die/site/overlap 檢查,非零分即合法)。A 側原生雙保險。

## 下一步(儀式加冕流程)

1. A 補完 3×(等他機器安靜窗口)→ B 彙整**跨機一致性表 = 加冕判準**。
2. 加冕通過 → v3 report + README 加 v3 欄(v2 欄保留;README @ `3469d46`)。
3. composite 預估 ~0.941(v2 = 0.9500)。
