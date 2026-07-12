# 翻案戰役收官 + v3 素材終表(2026-07-12,server B)

**Type**: milestone
**一句話**: 八個「死於定價」的墳場實驗全數結案;oracle 疊加(單軸規則)成為
v3 頭牌配置,tc2 −4.23% / hc02 −4.29% 雙紀錄;R2c'/R1/PLR/LNS 四個實作落庫。

## 1. 墳場終審(8/8)

| 實驗 | 終審 | 依據 |
|---|---|---|
| COSTCOMPARE_DOWNSTREAM | ✅ 翻身(tc2 −2.15%,七案過 gate) | 定價死,P1b 平反 |
| DP cross-MBFF | ✅ 翻身 ×2(1-hop −1.32%;**R2c' oracle 版七案全過**) | 定價死;R2c' 根治網表 A |
| ACCURATE_BANKING | ✅ 小勝(−0.15%) | P1b 直接修根因 |
| DP_ROUNDS | ✅ 邊際(−0.06%) | 同上 |
| TNS_SCALE 0.95/1.05 | ✅ 斷崖消失(±2.5%→±0.03%) | 超敏感 = 定價 artifact |
| ITER_BANKING | 持平(+0.006%) | 舊定價殘留,無翻案價值 |
| BFS_PRE_DP | 微負(+0.06%) | oracle 時代冗餘,退役 |
| POST_LG_RESYNTH | ❌ **死於設計,蓋棺**(exact 定價 +1.07%;還原版 restored=3/176 仍 +1.06%) | 不可逆無償 destroy;與 bitRepair 冗餘;屍檢=LNS 設計規格 |

## 2. crossOracle(R2c')七案 gate:7/7 PASS

tc2 −2.05%|hc02 −3.40%|hc03 −0.035%|tc3 +0.034%(窗口噪音)|hc04 −0.023%|
tc1 −0.039%(從 +0.69% 翻贏)|hc01 +0.31%(過門檻)。
**dQpd 軸依 four-rulings 劇本退役**(義務 1c)。

## 3. oracle 疊加(ccdown + crossOracle)七案矩陣 = v3 頭牌

| Case | tns_share | 疊加結果 | 軸判定 |
|---|---|---|---|
| hc02 | 22.68% | **−4.29%(史上最佳)** | ON ✓ |
| tc2 | 14.55% | **−4.23%(新紀錄)** | ON ✓ |
| hc01 | 4.13% | +0.555%(爆門檻) | OFF ✓ |
| tc1 | 1.19% | +0.064% | OFF ✓ |
| tc3 | 0.38% | −0.0175%(窗口噪音級) | OFF(棄零頭) |
| hc04 | 0.38% | −0.0062%(窗口噪音級) | OFF(棄零頭) |
| hc03 | 0.28% | −0.007% | OFF(棄零頭) |

單軸規則 `tns_share ≥ θ(0.05)`:θ 窗口 (4.13%, 14.55%),3.5× 無差別跨度
(`2026-07-12_tns_share_axis.md`)。**composite 預估:0.9500 → ~0.941**
(兩大案 −4.2% 攤七案 ≈ −0.9pp;以 server A 軌跡的 v3 儀式為準)。

## 4. 實作落庫(全部 default-off byte-exact,cmp 驗證)

| 實作 | commit | 驗收 |
|---|---|---|
| R2c' `DP_SLOT_ORACLE`(slot 移動 oracle 定價) | dc423ef | 7/7 gate ✓(§2) |
| R1 `REBANK_MODES` 4/8 + `REBANK_HR_FLOOR` | c2e2490 | byte-exact ✓;hc02 冒煙 −0.05%;1b 豐富案 gate 待排 |
| PLR `PLR_ORACLE` + 遺孤還原 | c2e2490 | byte-exact ✓;判決:設計死,存檔為 LNS 負對照 |
| LNS v1 `LNS_KICK`(區域 rebundle 交易) | (本 commit) | byte-exact + hc02 冒煙進行中,結果補記 |

## 5. hc02 負交互機制(DAC 素材,已具 per-bit 取證)

slack-wallet double-spend(命名已被 A 採用):ccdown 花 headroom 買 power
(重組 90.5% 分組),cross 花 power 買 TNS;受害 bit 的鄰域 headroom p50
15.2 → 6.7(未受害 47.8→44.9),1-hop 錯價移動在貧困區被接受 → 爆。
**R2c' 的 oracle gate 根治之(疊加從 +2.68% 變 −4.29%)**——
「exact pricing 之上仍需 operator 調度智慧」的完整敘事閉環。

## 6. 待辦交接(若 session 中斷,按此接力)

1. LNS 冒煙結果補記本檔 §4;正向則按 `plan_lns_destroy_repair.md` §5/§8
   走參數掃 + 七案 gate(v1.5 bit 拆分規格在計畫 §2)。
2. R1 modes 4/8 在 1b 豐富案(tc3/hc04 家族)的 gate。
3. conv-term(A,ETA 內)→ ParamMgr 單軸收口(A)→ **v3 儀式雙機 3×**
  (B 側配方:unified + EVAL_ANCHOR + 單軸疊加;24h 解綁閘 2026-07-13)。
4. tc1 乾淨 wall 階梯(機器安靜時;3b 報告附錄)。
5. 2g 實作(A 的規格含 floor;候選過濾器與 R1 共用 rbBitHeadroom)。

工件總索引:server B `~/scratch/`(probes_r2b / probes_r2c / probes_stack /
probes_r1 / probes_lns / verify_1bis / tns_share / run_3b / refsta)。
