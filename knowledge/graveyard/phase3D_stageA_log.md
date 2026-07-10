# Phase 3D — Method D Stage A (Slack Redistribution) Implementation Log

> Date: 2026-04-16
> Scope: Steps 1+2 of thesis Method D 的 Stage A — 把 sink-pin D-slack 沿 path 重新分配到每個 FF 的 redistribution budget，並導入 Banking.cpp 的 CostCompare。
> 相關檔案：`inc/Util.h`, `src/Util.cpp`, `inc/FF.h`, `src/FF.cpp`, `inc/Manager.h`, `src/Manager.cpp`, `src/Banking.cpp`, `main.cpp`

---

## Design

### Formulation
對每條時序 path `(fi → gates → fj)`：
- `S_P = fj.slack("D")` （sink D-pin slack）
- 按 weight 分到兩端：`share_fi = S_P * w_fi / (w_fi+w_fj)`, `share_fj = S_P * w_fj / (w_fi+w_fj)`
- 每個 FF 的 budget = **min across incident paths**（conservative bound），clamp 到 raw D-slack
- `ff->redistributedSlackD` 儲存此值

### 三個 mode（`SLACK_REDIST_MODE` env var）
| mode | weight rule | 目的 |
|---|---|---|
| 0 (off) | `redistributedSlackD = getTimingSlack("D")` | identity baseline（完全 zero-behavior-change） |
| 1 (uniform) | `w_i = 1` → 兩端均分 | naive baseline |
| 2 (freedom-prop) | `w_i = max(slack_i, eps)` → slack 大的 FF 分多 | displacement-freedom-proportional |

### CostCompare 接線（Step 2）
```cpp
// src/Banking.cpp
if(slackW > 0){
    const bool useRedist = (mgr.param.SLACK_REDIST_MODE > 0)
                           && (ff->getClusterFF().size() <= 1);
    double slackD = useRedist ? ff->getRedistributedSlackD()
                              : ff->getTimingSlack("D");
    double overshoot = predictedDelay - slackD;
    if(overshoot > 0) slackOvershoot += overshoot * affectNum;
}
```
Multi-bit 情境下 fallback 到 min-pin slack（避免需要 aggregate redistributedSlackD 跨 bits）。

---

## Full Sweep Benchmark（6 testcases × 3 modes, W=1.0）

### Score table（lower = better；粗體為最佳）

| Testcase | mode=0 (off) | mode=1 (uniform) | mode=2 (freedom-prop) | Best | Best Δ vs m=0 |
|---|---:|---:|---:|:---:|---:|
| sampleCase | 593.36 | 593.36 | 593.36 | tie | 0 |
| testcase1_0812 | 738,695,251.68 | **738,393,950.64** | 738,802,277.14 | m=1 | -0.041% |
| testcase2_0812 | **865,258.78** | 875,581.09 | 870,899.88 | m=0 | 0 |
| testcase3 | **729,063,614.40** | 729,179,389.11 | 729,179,389.11 | m=0 | 0 |
| testcase1_MBFF | 743,531,170.41 | **742,325,595.42** | 743,139,011.06 | m=1 | -0.162% |
| testcase2_MBFF | **910,102.96** | 915,651.23 | 915,144.66 | m=0 | 0 |

### Wins count
- **mode=0**: t2_0812, t3, t2_MBFF（3 cases；single-clk benchmark）
- **mode=1**: t1_0812, t1_MBFF（2 cases；multi-clk benchmark）
- **mode=2**: 沒有獨勝 case

### Gate check（±0.1%）
| Case | mode=1 Δ | mode=2 Δ |
|---|---:|---:|
| t1_0812 | -0.041% ✅ | +0.014% ✅ |
| t2_0812 | **+1.193% ❌** | **+0.652% ❌** |
| t3 | +0.016% ✅ | +0.016% ✅ |
| t1_MBFF | -0.162% ✅ | -0.053% ✅ |
| t2_MBFF | +0.610% ❌ | +0.554% ❌ |

mode=1 有 2 個超 gate、mode=2 有 2 個超 gate。single-clk 小 case（t2_0812 / t2_MBFF）對 redistribution 敏感。

### Banking wall
三個 mode 對 banking 時間幾乎無影響（±3%，全在 OpenMP nondeterminism 範圍內）；`[STAGE] slackRedist` 本身 55-65 ms（t1_0812），約占 preLegalize 的 2.5%。

### Placement checker
24/24 全 pass。

---

## 結論 / 決策

1. **Stage A 單獨不足以穩定改善 score。** mode=1 在 multi-clk case 有小幅 gain（-0.04 ~ -0.16%），但在 single-clk case 超 gate（+0.6 ~ +1.2%）。mode=2 freedom-prop 理論上較優但實測沒贏。
2. **解讀**：parallel banking 有 ~±0.7% nondeterminism，單次測量無法判斷 Stage A 真實貢獻。必須搭配 Stage B（max-weight matching）一起 ship，才能讓「 redistributed slack 被正確使用」— 目前 CostCompare 只是把它當 soft penalty，配對決策還是 nearest-neighbor heuristic。
3. **不 ship default change**：`SLACK_REDIST_MODE=0` 保持 default；mode=1/2 繼續當 env knob 給 thesis 用。不把任何 mode 寫進 `improvements.md` highlight reel。
4. **Thesis 框架**：Stage A 變成「infrastructure + ablation control」；主要 novelty 押在 Stage B matching + (1-1/e) bound proof。這反而讓 thesis 故事更緊—「單獨 redistribution 不夠，必須結合 combinatorial matching」。

## 下一步（短期）

- **Step 3 = Stage B 原型**：link LEMON，寫 max-weight matching over 2-bit merge candidates
- Stage A 不動，繼續留 3 個 mode 供 ablation
- 讀 P1 (Revisit MBFF ASP-DAC25) + P13 (Capacitated K-means DAC24)，抽 thesis 比較表數字
- 讀 P11 (Dynamic Net-Weighting ISPD15) + P12 (Lagrangian Gate Sizing ISPD19) 找第 3 個 weighting rule

## Files touched (committed as phase3D-stageA experimental snapshot)

- `inc/Util.h`：新增 `SLACK_REDIST_MODE` param
- `src/Util.cpp`：初始化
- `inc/FF.h` + `src/FF.cpp`：`redistributedSlackD` field + setter/getter
- `inc/Manager.h` + `src/Manager.cpp`：`computeSlackRedistribution()` 實作（~100 行，含 stats log）
- `src/Banking.cpp`：CostCompare 讀 `redistributedSlackD` when `SLACK_REDIST_MODE>0`
- `main.cpp`：env var 讀取 + `STAGE("slackRedist", ...)` hook
