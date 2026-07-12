# v3 儀式凍結清單(2026-07-13,server A 發佈;生效待 tc2 ADAPT_STACK 驗證章)

## 正式 env set(v3 候選配置)

```
OMP_NUM_THREADS=<per-machine, 見執行緒裁決>
BANKING_MODE=matching PRODUCTION=1
INCR_RELOC=1 RELOC=1 CRIT_SWAP=1
BIT_REPAIR=1 BIT_REPAIR_DYNA=2 BIT_REPAIR_RESCAN=4 BIT_REPAIR_BATCH=1
REFINE_ALLFF=1 BIT_REPAIR_INTRA=1 ALT_ROUNDS=2
BIT_REPAIR_TIME=1200            # conv-term 下自動降級為 3x 保險絲
ORACLE_REBANK=1 REBANK_TIME=600
DENSITY_REPAIR=1 ORACLE_EJECT=1 EJECT_TIME=180
EVAL_ANCHOR=1
CONV_TERM=1 CONV_RATE=1e-6
ADAPT_STACK=1                   # 單軸疊加:tns_share>=0.05 -> re-exec 帶
                                # CCDOWN+DP_SLOT_INTRA_ONLY=0+DP_SLOT_ORACLE
# ADAPT_STACK_TH=0.05 為 default,env 可覆寫(此即 four_rulings §2 的收口——
# 以 env 完成;ParamMgr 形式化登記列 cosmetic follow-up,不擋儀式)
```

**不進 v3**:R1 REBANK_MODES(等 B 的 1b 豐富案 gate)、LNS_KICK(v1 剛綠、參數掃未跑)、
de-hashing(Phase 3 後續)。它們是 v4 的菜。

## 執行緒裁決

**T = 各機自由(A 建議 8 或 40、B 用 64),per-run 記錄。** conv-term 的意義就是讓
執行緒變純速度參數——**儀式的驗收判準 = 跨機分數一致(f32 噪音級)**,這本身就是
DAC 稿 determinism 主張的實驗。若跨機不一致,即為 conv-term 覆蓋不全的 bug,回報修復。

## 儀式規格

- 兩機各 3×、七案全跑、evaluator 本尊 + 雙 checker、每 run 附 env dump 與
  loadavg(教訓:基線宣稱必附 env diff)
- 產物:per-case 中位數 + 跨機一致性表 + wall(標 T)
- 加冕條件:七案 3× 內部穩定(byte 或 f32 噪音)+ 跨機一致 + 全 checker 綠
- 預估 composite ~0.941(B 疊加矩陣 + conv-term 定點紅利)

## 時程

tc2 ADAPT_STACK 驗證(A,跑動中)→ 蓋章生效 → 兩機開跑。若 tc2 驗證爆
(axis 未觸發/重啟失敗/分數偏離 B 矩陣),回報後修,清單不變只延章。
