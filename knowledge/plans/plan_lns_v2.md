# LNS v2 施工圖(2026-07-13,server B;承 v1.5 `ac71ff3` + 報告
`2026-07-13_exp_lns_v15_bit_split.md`)

**Type**: plan(v1.5 已綠:hc02 −0.252% 合法、row-discipline 修復落庫)
**定位**:v4 菜單項;與 R1(`REBANK_MODES=15`,疊加證據 6勝1平)正交疊加。

## 0. v1.5 遺留的實測事實(v2 的出發點)

1. 篩選樂觀偏差:整區 evalRemapDelta 不計 FindPlace 位移 → tc3 放行 28
   全空手;hc02 也是 121 過篩僅 5 accept(4% 命中)。
2. reject 有固有成本:debank/rebank 改名 → 下游名序漂移(tc3 +0.0003%)。
   命中率上去,這項自動下來。
3. K 單調訊號:K=6 > K=10 > K=16 —— 小區域細粒度贏。
4. FreeRect 契約教訓:EJECT revertAll 的 failure path 有同款潛伏雷,順手補。

## 1. Phase A —— 純 env 實驗(零代碼,先跑)

- **A1 margin 掃**:`LNS_MARGIN ∈ {0, 50, 200, 1000}` × hc02 K=6。
  預期:命中率↑、攪動↓;找 accept 數不掉但 rej 大減的甜點。
- **A2 K 小值掃**:`K ∈ {3,4,6}` × hc02(margin 用 A1 甜點)。
- **A3 R1+LNS 疊加**(v4 驗證輪):`REBANK_MODES=15 LNS_KICK=1 LNS_SPLIT=1`
  七案 vs rep1;過 0.5% gate 即進 v4 凍結候選。
- **A4 修復版七案 gate**(v1.5 驗收 §5.3 未竟項):K/margin 用甜點值。

## 2. Phase B —— 篩選保真(小代碼)

- 篩選階段對每個計劃群組先 `FindPlace` 試探(不 UpdateRows、即 free?
  FindPlace 本身 read-only,拿到真實落點再定價)→ nD/nQ 用真實落點,
  消除樂觀偏差主源。注意:試探順序不佔位,群組間可能搶同一空位 →
  仍是估計,但偏差大減。
- 或者便宜版:位移罰項 = dd × |wish 質心 − 區域現有空隙估計|。

## 3. Phase C —— 新鏡頭(原施工圖 §3 預留)

- **C1 headroom 貧困區 kick**:seed 改取 `rbBitHeadroom < floor` 密度最高
  的鄰域;目標 = 修復性重整(可與 REBANK_HR_FLOOR 聯動放寬)。
- **C2 power-harvest kick**:目標函數偏 β 項;與 2g 規格 floor 同源。
  兩者皆重用 v1.5 交易骨架,只換 seed 選擇與 accept 權重。

## 4. Phase D —— 分組最優化(代碼較大,排最後)

貪婪鄰近 → matching:同 clk 池內用 merge_economics 表估價、CSRTree 枚舉
(rebank 樣板)、disjoint 取組。預期把 4% 命中率再拉一級。

## 5. 順手項

- EJECT `revertAll` failure path 的 FreeRect-unrowed 雷:比照 v1.5 修法
  (顯式 rowed 追蹤)。獨立小 commit,byte-exact off 不變。

## 6. 驗收(house 標準,同 v1.5)

byte-exact off cmp / determinism ×2 / 七案 gate 0.5% 規則 / INCR_VALIDATE
對照(chain4 的無 LNS 基線出爐後即有參照)。報告一份收全部 phase。
