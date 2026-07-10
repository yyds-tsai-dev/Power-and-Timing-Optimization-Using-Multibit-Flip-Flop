---
name: oracle-gap-rootcause
description: Oracle-vs-evaluator gap — delta semantics exact (1e-5); anchors baked with 1-hop error in Preprocess (EVAL_ANCHOR=1 fixes ~25%, unreleased); 450-TNS residual under decomposition (model vs commit-accumulation)
metadata: 
  node_type: memory
  type: project
  originSessionId: 9381340c-d956-45d8-adf9-8fe6a9322fba
---

Oracle-evaluator gap(舊 [[project-evaluator-vs-internal]] 的 open issue)已於 2026-07-07 根因閉合:

- **定價語義精確**:PROBE_MOVE/PROBE_CELL 單發探針,基態+final 態,穿 gate/重收斂,d1hop=dmultihop=dEval 到 1e-5。tc2 無任何 FF→FF 直連。
- **腐蝕源 = bitRepair commit**:單 pass 宣稱 −20,576 vs eval −14,513(高報 42%,0.81% score 單向低估);rebank/eject 裸態無動作。
- **判別**:post-repair 態,被換過 bit 的鄰域探針失準達 15%(FF_4_985),未觸碰鄰域仍精確;FF_4_996 上 1hop/multihop/eval 三方互不相等。
- **機制**:commit 用有界深度更新 stored slack,淺於 delta 評估的全深度 walk;rectifier 邊界被 stale slack 挪動;incr==full 檢查結構性抓不到(同讀腐蝕狀態)。
- **修法**:commit 後從 original slack 錨點無狀態 re-anchor(全深度前向傳播);驗收=checkpoint 殘差歸零;修好重跑官方 3×。
- 完整報告:`Project Knowledge/reports/v1/2026-07-07_diagnosis_oracle_gap_ROOTCAUSE.md`;PROBE_CELL harness 在 main.cpp(2026-07-07,記得 commit)。

**2026-07-11 P1a 更新**:EVAL_ANCHOR=1 已實作(parse 真錨,oracle 家族 38 讀取點切換;byte-exact off 驗證 PASS;未 commit)。oc_rep 缺口 6,063→4,503:**anchor-bake-in 只解釋 ~25%,殘差 450 TNS 待分解**(下一步:base 態 + TNS_ORACLE_VALIDATE 判「模型語義 vs commit 累積」)。副產品:flag on 讓 tc2 rep-config 分數好 1,104。原「commit 淺更新」假說被讀碼推翻——anchor 三元組 preprocess 後無人寫;真結構是 Preprocess::updateSlack(1-hop)把誤差烤進錨點 + arrCorrection_ 每 ALT round 才刷新。詳見 plan_dac2027_optimization.md P1a 節。
