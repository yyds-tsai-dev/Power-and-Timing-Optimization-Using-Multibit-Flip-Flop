# Phase 3E Plan — Risk-Adaptive DIST_BONUS + Debank-Rebank Rescue

**Status**: Ready for implementation
**Created**: 2026-04-18
**Depends on**: Per-edge adaptive DIST_BONUS（已 shipped，Banking.cpp:770-791）
**Baseline**: Current shipped state = per-edge adaptive with `SLACK_THRESH=0`
**Design context**: `Project Knowledge/report_v21_decoupled.md`, `Project Knowledge/design_DecoupledBanking.md`

---

## Overview

兩個獨立改動，可以分開 ship：

| Part | 改動 | 預期效果 | 風險 |
|---|---|---|---|
| **Part A**: Per-edge risk-based DIST_BONUS | 用物理推導取代固定 `SLACK_THRESH` | hc02 更好（接近 THRESH=20 的 -4.83%），其他 case 不退化 | 低 |
| **Part B**: Post-banking debank-rebank rescue | 找 TNS 最差的 MBFF → debank → 2-bit re-matching | 修復 greedy 4/8-bit 的 TNS 退化 | 中 |

建議順序：**A → benchmark → B → benchmark**。

---

## Part A: Per-edge Risk-Based DIST_BONUS（parameter-free）

### 動機

現有 per-edge adaptive 用固定 `SLACK_THRESH`（default=0）判斷 "critical"：
```cpp
if(slA < slackThreshold || slB < slackThreshold)
    edgeBonus = 0;
```

THRESH=0 是 balanced default，但 THRESH=20 對 hc02 給 -4.83%。問題是不同 case 最佳 THRESH 不同。

### 核心 Insight

一個 FF 在某次 merge 後會變 critical 的物理條件：

```
post_slack ≈ slack - DisplacementDelay × (HPWL / 2) < 0
```

所以 "critical" 不是 FF 的固有屬性，而是 **per-edge** 的：同一個 FF 跟近的 partner merge 可能安全，跟遠的 partner merge 就會 critical。

### 改動

**檔案**: `src/Banking.cpp`，`doMatchingClustering()` 的 2-bit edge build loop

**位置**: Banking.cpp:770-791（per-edge adaptive 段落）

**現有 code**:
```cpp
if(adaptiveDistPerEdge){
    double slA = ffA->getTimingSlack("D");
    double slB = ffB->getTimingSlack("D");
    if(slA < slackThreshold || slB < slackThreshold)
        edgeBonus = 0;
}
```

**改為**:
```cpp
if(adaptiveDistPerEdge){
    double slA = ffA->getTimingSlack("D");
    double slB = ffB->getTimingSlack("D");
    // Per-edge risk: will this specific merge eat this FF's slack?
    // Each FF moves ~half the pair distance to the median.
    double dist = HPWL(coorA, coorB);
    double risk = mgr.DisplacementDelay * dist / 2.0;
    if(slA < risk || slB < risk)
        edgeBonus = 0;
}
```

**注意**: `dist` 已經在下面 line 784 被算過（`double dist = HPWL(coorA, coorB);`），可以提前到 adaptive check 前面避免重算。需要小心 scope — 把 `dist` 的宣告移到 `if(DIST_BONUS > 0)` block 的頂部。

重構後的完整段落：
```cpp
if(DIST_BONUS > 0){
    double edgeBonus = DIST_BONUS;
    double dist = HPWL(coorA, coorB);  // 提前計算
    if(adaptiveDistPerEdge){
        double slA = ffA->getTimingSlack("D");
        double slB = ffB->getTimingSlack("D");
        double risk = mgr.DisplacementDelay * dist / 2.0;
        if(slA < risk || slB < risk)
            edgeBonus = 0;
    }
    if(edgeBonus > 0){
        adjGain += gain * edgeBonus / (1.0 + dist * distScale);
    }
}
```

**移除 `SLACK_THRESH` env var**: 不再需要。如果想保留 A/B 測試能力，可以加一個 `RISK_ADAPTIVE` env toggle（default ON），`RISK_ADAPTIVE=0` 時退回固定 `SLACK_THRESH`。

### Higher-bit matching 的同步改動

如果 higher-bit matching code（Banking.cpp ~1000+）也有 adaptive DIST_BONUS，同樣改法。但目前 higher-bit matching default OFF 且結果 catastrophic，所以 **可以暫不改**，等 Part B rebank 做完再考慮。

### Benchmark 計劃

```bash
# A/B test: risk-adaptive vs current THRESH=0
# 跑所有 9 cases（6 main + 4 hidden，sampleCase 可跳）
BANKING_MODE=matching PRODUCTION=1 ./cadb_0015_final testcase/X.txt testcase/X.out

# 對照組（disable risk-adaptive，回到 THRESH=0）
RISK_ADAPTIVE=0 SLACK_THRESH=0 BANKING_MODE=matching PRODUCTION=1 ...
```

Gate: 所有 case score delta ≤ +0.1%，至少 2 個 case 改善。

### 預期

- 近的 pair（dist 小）→ risk 小 → 保留 DIST_BONUS → 跟 THRESH=0 一樣
- 遠的 pair（dist 大）→ risk 大 → 更多 pair 被判 critical → 接近 THRESH=20 的效果
- 自動適應不同 case 的 FF 密度和 DisplacementDelay
- **hc02 預期接近 -4.83%，t2_MBFF 預期不退化**

---

## Part B: Post-Banking Debank-Rebank Rescue

### 動機

Banking 之後（greedy 4/8-bit），某些 MBFF 的 TNS 嚴重惡化：constituent FF 被位移太遠，downstream slack 變負。現有 flow 沒有回頭修的機制。

Report 確認 4-bit matching 的 TNS 退化是結構性的（即使用 window-optimal 也 +60%）。與其修 matching，不如在 banking 之後加一個 **debank + rebank rescue pass**。

### 設計

在 `main.cpp` 的 `banking()` 之後、`postBankingOptimize()` 之前插入 rescue pass：

```
banking() → [rescue pass] → postBankingOptimize() → legalize() → DP
```

#### Step 1: 識別 TNS-offending MBFFs

掃描所有 multi-bit MBFF（bits ≥ 2），計算每個 MBFF 對全局 TNS 的貢獻：

```cpp
void Manager::identifyTNSOffenders(
    std::vector<std::pair<FF*, double>>& offenders,
    double threshold  // 只收集 per-MBFF TNS contribution > threshold 的
) {
    for (auto& [name, ff] : FF_Map) {
        if (ff->getCell()->getBits() <= 1) continue;
        double mbffTNS = 0;
        for (auto& cf : ff->getClusterFF()) {
            // D-pin slack of this constituent
            double slackD = cf->getSlack();
            mbffTNS += std::max(0.0, -slackD);
            // Q-pin: downstream FFs' D-pin slack
            for (auto& next : cf->getNextStage()) {
                mbffTNS += std::max(0.0, -next.ff->getSlack());
            }
        }
        if (mbffTNS > threshold)
            offenders.push_back({ff, mbffTNS});
    }
    // Sort by TNS contribution descending
    std::sort(offenders.begin(), offenders.end(),
        [](auto& a, auto& b) { return a.second > b.second; });
}
```

#### Step 2: Debank top-K% offenders

對 TNS contribution 最高的 MBFF 做 debank：

```cpp
void Manager::rescueDebank(
    std::vector<std::pair<FF*, double>>& offenders,
    int maxDebank  // 最多 debank 幾個
) {
    Cell* cell1bit = Bit_FF_Map[1][0];  // 1-bit cell for debanking
    int debanked = 0;
    for (auto& [mbff, tns] : offenders) {
        if (debanked >= maxDebank) break;
        // CostCompare check: 確認 debank 後確實更好
        // debankFF 會把 MBFF 拆回 1-bit FFs，每個 FF 放在原 MBFF 的 D-pin 位置
        debankFF(mbff, cell1bit);
        debanked++;
    }
}
```

`debankFF` 已經存在（Manager.cpp:387-416），它會：
- 把每個 constituent FF 放在 MBFF 的 D-pin 座標位置
- 建新的 1-bit FF instances
- 從 FF_Map 刪除原 MBFF

#### Step 3: Rebank debanked FFs（optional，Phase 2）

Debank 後的 1-bit FFs 可以嘗試用 2-bit matching 重新配對：

```cpp
void Manager::rescueRebank(std::vector<FF*>& debankedFFs) {
    // 只對剛 debank 出來的 FFs 做 2-bit matching
    // 用 per-edge risk-adaptive DIST_BONUS（Part A 的成果）
    // 限制：只 bank 成 2-bit（不做 4/8-bit，避免重蹈覆轍）
    // Legalizer 需要 re-init（因為 row state 變了）
    delete legalizer;
    legalizer = new Legalizer(*this);
    legalizer->initial();

    // Build graph only on debankedFFs, same-clock, same K-nearest
    // Run LEMON MaxWeightedMatching
    // Commit with FindPlace
}
```

**注意**: Step 3 可以在 Step 2 確認有效後再做。先只做 debank（不 rebank），如果純 debank 就能改善 TNS（因為 1-bit FF 回到離 driver 更近的位置），那就不需要冒 rebank 的風險。

### 在 main.cpp 的接入點

```cpp
STAGE("banking",           mgr.banking());

// ---- Rescue pass (Phase 3E Part B) ----
STAGE("rescue", {
    std::vector<std::pair<FF*, double>> offenders;
    mgr.identifyTNSOffenders(offenders, 0);  // threshold=0: 有任何 TNS contribution 的
    int maxDebank = offenders.size() * rescuePercent / 100;  // e.g., top 5%
    if (maxDebank > 0) {
        mgr.rescueDebank(offenders, maxDebank);
        // Optional Step 3: mgr.rescueRebank(debankedFFs);
    }
});

if(!production){ mgr.getOverallCost(cost_verbose, 0); mgr.dumpVisual("Banking.out"); }
STAGE("postBankingOpt",    mgr.postBankingOptimize());
```

### 需要新增的檔案/函數

| 檔案 | 新增 |
|---|---|
| `inc/Manager.h` | `void identifyTNSOffenders(...)` 和 `void rescueDebank(...)` 宣告 |
| `src/Manager.cpp` | 上述兩個函數的實作 |
| `main.cpp` | rescue pass 呼叫（在 banking 和 postBankingOpt 之間） |

不需要改 Banking.cpp。

### 調參

| 參數 | 建議 | Env var |
|---|---|---|
| `rescuePercent` | 5%（top 5% TNS offenders 被 debank） | `RESCUE_PCT=5` |
| threshold | 0（任何有 TNS contribution 的 MBFF 都是 candidate） | — |
| rebank | 先 OFF | `RESCUE_REBANK=0` |

### Benchmark 計劃

```bash
# Rescue pass ON (debank only, no rebank)
RESCUE_PCT=5 BANKING_MODE=matching PRODUCTION=1 ./cadb_0015_final testcase/X.txt testcase/X.out

# Rescue pass OFF (baseline)
RESCUE_PCT=0 BANKING_MODE=matching PRODUCTION=1 ./cadb_0015_final testcase/X.txt testcase/X.out

# Sweep rescue percentage
for pct in 1 2 5 10 20; do
    RESCUE_PCT=$pct BANKING_MODE=matching PRODUCTION=1 ./cadb_0015_final testcase/X.txt testcase/X.out
done
```

Gate: placement_checker pass + score delta ≤ +0.1% on all cases。

### 預期效果

- **TNS-heavy cases (hc02, t2_MBFF)**: Debank 掉 TNS 最差的幾個 MBFF → TNS 大幅降低。代價是 Power + Area 略增（1-bit cell 比 MBFF 大/耗電）。
- **Area-dominated cases (t1_0812, t3, hc04)**: 幾乎不動，因為這些 case 的 TNS contribution 很小，offender list 短。
- **整體**: 不會比 baseline 差（worst case = rescue 全部 debank 後 CostCompare 判 negative → 不 debank）。

### 風險

1. **Debank 改變 Legalizer row state**: debank 後 MBFF 消失、多個 1-bit FF 出現，row 的 occupied sites 分佈變了。PostBankingOptimizer 和 Legalize 需要看到正確的 state。
   **Mitigation**: debank 後 re-init legalizer（或讓 postBankingOptimize 自己 re-init，它本來就會重建 FF list）。

2. **Power/Area regression**: debank 會增加 Power + Area（1-bit cell 效率差）。如果 `α·ΔTNS` 的改善不夠大，總 score 可能反而變差。
   **Mitigation**: debank 前先估算 score delta：`ΔScore = -α·ΔTNS + β·ΔPower + γ·ΔArea`，只 debank 有 net gain 的。

3. **debank 太多導致 utilization 爆**: 大量 MBFF → 1-bit FF，cell 總面積增加，可能超出 die utilization limit。
   **Mitigation**: `rescuePercent` 預設保守（5%），加上 utilization check。

---

## 實作順序

```
Step 1: Part A — per-edge risk-based DIST_BONUS (改 ~10 行)
Step 2: Benchmark Part A (9 cases)
Step 3: Ship Part A if improved (commit)
Step 4: Part B — identifyTNSOffenders + rescueDebank (新增 ~80 行)
Step 5: Benchmark Part B alone + Part A+B combined
Step 6: Ship Part B if improved (commit)
Step 7: (Optional) Part B Step 3 — rescueRebank (新增 ~60 行)
Step 8: Benchmark Part B with rebank
```

每步都有 rollback point（git commit）。Part A 和 Part B 互相獨立，任一個 fail 不影響另一個。

---

## 成功標準

| Metric | Target |
|---|---|
| hc02 score delta | ≤ -3% (currently -2.18% with THRESH=0) |
| t2_MBFF score delta | ≤ -1% (currently -1.25% with THRESH=0) |
| 其他 case | ≤ +0.1% (不退化) |
| placement_checker | 9/9 pass |
| Banking stage runtime | ≤ +10% (rescue pass overhead) |
