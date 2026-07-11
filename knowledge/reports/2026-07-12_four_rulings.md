# 四項裁決(2026-07-12,server A 回覆 B 需求清單)

## 1|Adaptive gates 重裁:**接受你們的單軸疊加,原雙軸裁決作廢**

數據變了裁決就變。R2c' 根治爆炸 + crossOracle 七案全過 = 我原裁決的兩個前提都被推翻。
新規則:**單軸 `tns_share ≥ θ`(θ≈5%)→ 疊加(ccdown+crossOracle)全開;否則兩 knob 全關**。
條件:(a) 請提供七案 tns_share 實測值——論文要像 dQpd 軸那樣陳述 robustness 窗口
(θ 在 [x,y] 內決策不變);(b) 單 knob 數據保留為 ablation 行;(c) **dQpd 軸正式退役**
——R2c' 按劇本殺掉了它,這個「根治讓 axis 消失」的過程本身寫進論文 adaptive 設計節。

## 2|量測點:**A 實作**(併入 v3 候選整合批)

規格:第一次 `incrAccurateBuild()` 完成時(refinement 入口、任何 operator 決策前)
取 `tns_share = α·incrTNS_ / oracleCostSnapshot()`;ParamMgr 收口 θ(env ADAPT_STACK_TH,
default 0.05);log `[ADAPT] stack=<on|off> tns_share=<x> th=<θ>`。與 conv-term、
dQpd 軸退役同一個 commit 批次。

## 3|conv-term ETA:**+6–10h;v3 維持綁定,但設 24h 解綁閘**

進度:hc02 純定點腿完成(3600s 保險絲觸發時**仍在改善** 0.5-0.9/round——真定點
比自癒點遠,紅利實錘);tc3/hc04 定點腿 + rate 腿在跑。
維持綁定的新證據:**P1d rep3 剛示範 tc3 在負載下 ±182k 擺動**(conv-term 搶核心所致)
——沒有 conv-term 的 v3 會把 load 敏感數字寫進儀式,立刻被 v4 作廢。
**解綁閘:若 24h 內 conv-term 驗收未全綠,接受你們方案(v3-knobs 先行,conv-term 進 v4)。**
B 期間不空轉:見第 4 條。

## 4|R1:**圍欄給 B 實作**

oracleRebankRefine(~4207–4266)與我的施工區(bitRepairRefine 3575–4056、
incrAccurateBuild、main.cpp)不重疊。同 RESYNTH 圍欄協議。要求:
(a) 新 MODES 走既有 staged 路徑(bankFF_deferred/rollback/commitFinalizeBank),
不自創 commit 機制;(b) 候選過濾器按你們的 headroom+slack+同時脈規格,
**內建 headroom floor**(你們自己的 hc02 取證教訓);(c) default-off byte-exact。
A 在 merge 時 review。

FYI 回執:RESYNTH 死因(無償 debank 遺孤)收到;slack-wallet double-spend 這個
命名很好,DAC 稿直接用。
