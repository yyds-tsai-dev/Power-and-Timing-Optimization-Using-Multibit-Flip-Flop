# 碩論大綱(B 側,2026-07-17 草)— 章節 ↔ 現有素材映射

**工作題目**:Exact-Priced Refinement for Multi-bit Flip-Flop Banking:
a Deterministic Destroy-and-Repair Framework(暫)
**貢獻切分聲明(必寫)**:時序 oracle 主幹(EVAL_ANCHOR 家族)為合作者 A 所有;
本論文署名主體 = LNS 系列、R1 gate、PROBE、實驗程序全套、GPU 研究、
determinism 證據體系(分工:`dac2027-division-of-labor` / repo BOOTSTRAP)。

## Ch.1 Introduction
- 動機引言:Synopsys US 11,328,109 自述 de-bank/re-bank「expensive and
  unpredictable」(`plan_lns_destroy_repair.md` §0)
- 賽題與基線:ICCAD 2024 Problem B;top-3 / DATE'26 0.979 / DAC'25 0.991

## Ch.2 Background & Related Work
- lit recon:`2026-07-08_dac2027_lit_recon.md`(exact-priced destroy-repair
  查無先例)+ `2026-07-11_dehashing_design_survey.md`

## Ch.3 框架:evaluator-exact 定價上的算子家族(A 主幹,概述層)
- oracle 語義:`2026-07-11_evaluator_disassembly_semantics.md`、
  `2026-07-11_oracle_call_surface.md`
- 記錄鏈:0.952→0.9500(v2)→0.9437(v3)→0.9428(v4)→**0.9424(v4.1)**

## Ch.4 【頭牌】Exact-priced Destroy-and-Repair LNS
- v1 整顆(−0.0096%)→ v1.5 bit 拆分(−0.385%,26×)→ 內建 ablation
- 交易協議、row-discipline postmortem(FreeRect 契約):
  `2026-07-13_exp_lns_v15_bit_split.md`
- 調參幾何:K=4 甜點、margin 單調、`2026-07-13_exp_lns_phaseA_probe.md`
- PROBE(real-site 定價):工程綠/分數混合 + row-state 非中立
- 陰性結果(照樣入章,機理清楚):C2 收割碎片化擋路、RESYNTH 負對照
- R1 疊加:6勝1平 `2026-07-13_exp_r1_x_v3_stacking.md`;AR3 二階紅利:
  `2026-07-17_exp_v41_alt_rounds3.md`
- Phase D(matching 分組):【結果待補,驗證中】

## Ch.5 Determinism as a First-Class Property(差異化章)
- 四軸逐位不變(重跑/執行緒 8–128/負載/重編譯):
  `2026-07-13_exp_v3_ritual_b_side.md` + T128 報告
- byte-exact gate 紀律、pointer-hash 陷阱、env-diff 教訓、
  1/50 翻位追獵(helgrind=0 → FP 累加序):rowfix 報告 §3b
- 基線可重現性審計:`2026-07-16_diagnosis_tc1_v2_record_provenance.md`

## Ch.6 GPU 可行性研究(短章或附錄)
- 兩案 bench:正確性 1e-11、FP32 召回 100%@M=2K、掃描 bound 診斷、
  cone 分流生產路線:`2026-07-13_exp_gpu_screening_benchmark.md`

## Ch.7 實驗總表
- 七案 × {top-3, v2, v3, v4, v4.1};runtime 表(T64/T128);
  獨立重算通道(chain7 矩陣驗算)

## Ch.8 結論與未來工作
- v5 菜單:PROBE per-case、DP 壓實收割、de-hashing、FP 定序、AR4

## 缺口(寫作前必補)
1. 跨機加冕(A 側 v4.1 env 一輪 + tc1 出處一發)
2. NTU baseline 對照表併入 Ch.7(CLAUDE.md 要求 per-case vs NTU)
3. 口試防禦點:AI 輔助開發的方法論聲明(依系所規範)
