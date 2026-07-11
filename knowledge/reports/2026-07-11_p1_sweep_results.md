# Phase 1 Exactness 戰役 — 七案官方結果(2026-07-11,server A)

Unified config v2 = 原 unified + `EVAL_ANCHOR=1`(P1a+P1b+1bis 全套語義)。
全案 evaluator 本尊 + sanity + placement checker 通過。repeat #1;P1d 三重跑進行中。

| Case | 舊基線(0.9518)| v2 repeat#1 | Δ | binary |
|---|---:|---:|---|---|
| tc1 | 734,319,834 | 734,266,279 | −0.0073% | 1bis |
| tc2 | 723,165 | 717,378 | **−0.80%** | P1b(=1bis,零 partial)|
| tc3 | 726,041,482 | 725,877,524 | −0.0226% | P1b |
| hc01 | 30,062,537 | 30,065,106 | +0.0085% | 1bis |
| hc02 | 10,089,886 | 10,028,140 | **−0.612%** | P1b |
| hc03 | 55,781,010 | 55,772,291 | −0.0156% | P1b |
| hc04 | 726,015,652 | 725,906,134 | −0.0151% | P1b |

**Composite vs top-3:0.9518 → 0.9500**(per-case ratio 平均;6 勝 1 微負,0.5% gate 通過)。
Ablation 資料點:anchor-only sweep(EVAL_ANCHOR 無語義修復)另存 dac0/ea_sweep.csv——
tc2 720,451、5 勝 2 微負,證明「錨」與「語義」兩層各自貢獻。
hc01 +0.0085% 為軌跡噪音級;2f 翻案(cross-MBFF −1.32% hc02 已翻正)預期蓋過。
