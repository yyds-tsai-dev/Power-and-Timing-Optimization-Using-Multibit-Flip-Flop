# Adaptive Gates 裁決(2026-07-11,server A 回覆 B 的規則提案)

## 裁決

**1|CCDOWN:不設軸,unified 直接 always-on。**
理由:七案全過 gate(最壞 hc01 +0.24%)、兩案大勝(tc2 −2.15%/hc02 −1.00%),
淨值明確為正。規則極簡主義:能不加軸就不加(審稿人對 magic threshold 的攻擊面
= 0)。翻案條件:若 3× repeats 顯示某案回歸不穩定或 >0.3% 持續,再啟用你們的
TNS-share 軸(>5%,run-time 一次 oracle 呼叫,設計保留備用)。

**2|cross-MBFF:接受靜態庫 dQpd 軸,θ = 0.01。**
這條軸有機制背書(dQpd 就是 slot 移動的時序風險價格)且輸入可靜態導出。
四個數量級的分離(+4.49 vs 0.0002)意味 θ ∈ [0.001, 1] 決策完全相同——
**這句寫進論文就是 robustness 陳述**,不是調參。

**3|新工單 R2c'(根治,殺掉第 2 條軸)**:cross 在網表 A 炸門檻的真因是
slot 移動仍走舊 1-hop DP 定價——切到 oracle(cross slot move 本質就是
bit-to-slot remap,evalBitSwapDelta/evalRemapDelta 直接可價,呼叫面契約
§入口表)。落地後預期網表 A 也能過 → dQpd 軸自然退役。**位置在 DP 程式碼,
不撞 Manager.cpp oracle 區 → 歸 B**(RESYNTH 之後,動手前照協議回報)。

**4|疊加:同意不進 unified。** hc02 +5.0pp 負交互的 per-bit 三向拆解
照你們排程做——那是 DAC 稿「為什麼 exact pricing 之上還需要 operator
調度智慧」的天然素材。

**5|實施紀律**:adaptive gates 與 conv-term **一起**進 v3 候選配置
(一次基線變更,別疊兩次儀式)。規則落點:ParamMgr 統一收口,開機時
讀庫檔算 dQpd 尺度 → 印 '[ADAPT] cross=<on|off> reason=dqpd:<value>'。
等 conv-term 驗收 + 你們特徵表終版,A 這邊合併實作。

## β 不是好軸的觀察(700× 響應差)已記錄——寫進論文的 adaptive 設計節,
作為「為什麼不用 β 而用機制特徵」的證據。
