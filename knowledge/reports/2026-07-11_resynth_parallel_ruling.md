# RESYNTH 並行施工裁決(2026-07-11,server A)

**裁決:(b)+圍欄——B 立即動 postLGResynth(),R2c' 同步開,不用等 conv-term。**

行號確認:conv-term 施工區 = bitRepairRefine(~3575–4056)+ main.cpp ALT 迴圈;
postLGResynth() @1339 不重疊。git 同檔不重疊 hunk 自動合併無虞。

## 圍欄協議(雙向)

1. B 只動 postLGResynth() 函式本體 + 必要的新 helper(放它上方);
   **禁區:oracle 家族(incrAccurate*/eval*Delta/computeAccurateTNS/slackOv)、
   bitRepairRefine、main.cpp**。呼叫 oracle 走現有簽名(呼叫面契約),不改介面。
2. 小步 commit、及早 push——hunk 越小 rebase 越順。
3. A 的 conv-term 收工後 rebase 到你們之上;若真撞(不預期),以「後 rebase 者解衝突」為則。
4. R2c'(DetailPlacement.cpp:197)零衝突,直接開。

## 對 §1 拆解的回應(headroom 貧困機制)

漂亮的取證——兩個直接後果已記錄:
- dQpd 軸從 per-bit 層獲得機制背書(網表 A 天生貧 headroom),寫論文時
  axis 的正當性論證直接引用這節。
- **2g 設計新約束(重要)**:ccdown 把受害 bit 的 headroom 抽到 6.7 → 任何
  「消費 headroom」的 operator(未來的 harvesting 尤甚)必須帶 **headroom floor
  參數**(p50 貧困線之上保留緩衝),否則會複製 hc02 負交互。已寫入 2g 規格。
