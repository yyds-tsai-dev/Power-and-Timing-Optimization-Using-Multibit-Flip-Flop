# HANDOFF:Fable → Opus(2026-07-07)

**讀我第一。** 這份文件 + auto-memory(`~/.claude/projects/.../memory/`)是完整交接。原則:**大判斷已做完,你只做小修與執行**。改動任何「已凍結」項目前先問用戶。

## 硬日期
- **7/10**:用戶親自送 abstract(`paper_aspdac2027/EASYCHAIR_submit.txt` 直接貼;Track 9)
- **7/15**:內容凍結目標;**7/17**:上傳 full paper(deadline 7/18 5PM AoE,留一天 buffer)
- 9/4 通知;被拒 → DATE'27(9 月中截稿,`venue_variants/main_date.tex` 已備)

## 已凍結(勿動,除非用戶要求)
- Paper 主張架構:四貢獻、cascade 敘事的「兩道保險」版本(§5.3 proxy 對照)、oracle 誠實句(§3.4 median 0.05%/max 1.1%)、front-end「揭露不主張」框架、dynasearch 已改名(不得回退)
- 主表數字(0.953 世代,全 evaluator 實測)——**除非** oracle-gap 修復後重跑官方 3×(見 T4)
- 頁面紀律:正文 ≤6 頁(refs 可佔第 7 頁)。**檢查法(aux-label 法有盲區,勿用)**:PyPDF2 抽 p7 文字、濾掉行號後,`References` 之前除頁首外必須為空——配方:
  `python3 -c "import PyPDF2,re; r=PyPDF2.PdfFileReader(open('main.pdf','rb')); t=r.getPage(6).extractText(); b='\n'.join(l for l in t.split('\n') if not re.fullmatch(r'\d{1,4}',l.strip())); i=b.find('References'); print(repr(b[b.find('\n'):i].strip() or 'CLEAN'))"`
  2026-07-07 凌晨曾為此打了一場 6 輪裁剪戰(cut 紀錄在 git 外;若 Opus 需要回覆任何字句,備份在 backups_20260706b tarball)。**任何新增文字都要重跑此檢查。**

## 任務佇列(優先序)

### T1|收割深夜 workflow(重跑版 task=wqkdkms20 / run=wf_ba69adc2-930)
**注意**:第一輪被 API 529 打掉三個 agent(thesis-sync/date-variant/zh-regen),已用 `resumeFromRunId` 重跑(cached: tcad-skeleton ✅)。若再遇 529:編輯 script(bump 對應 agent prompt 一個字元)→ 同 resumeFromRunId 再 resume。第一輪 verify 已知 flags:thesis 缺 EJECT 操作子章節與 0.05%/1.1% oracle 句、致謝有可見佔位符、2.5:62 與 1:76 有殘留的 hedged dynasearch first-use 句、中文版缺 counterfactual、main_date.tex 待產(main_ieee 8 頁是 ICCAD 版基底)。
四個交付:thesis 同步、DATE 6 頁版、中文版重生成、TCAD skeleton + verify 報告。照 verify 的 fix list 執行小修。若 thesis 的 EJ7 數字有 %TODO(hc03/hc04 當時未完),從 `/tmp/v1_*_ej7.evaluator` 補。

### T2|EJECT 收尾(EJ7 = bj4u5mi8j)
EJ7 七案若全勝(前五案已勝:tc1 734,319,834 / tc2 723,165 / tc3 726,041,482 / hc01 30,062,537 / hc02 10,089,886):
1. `improvements.md` 加條目(格式照舊,配方 = 統一配置 + `ORACLE_EJECT=1 EJECT_TIME=180`)
2. memory `project_refine_allff_discovery.md` 更新最終數字
3. V3 README 表更新(如果做了 oracle 修復+重跑,一起)
4. **paper 不加 EJECT**(已決策:留給 TCAD/thesis)——但若 re-review 或教授強烈要求,ICCAD 8 頁版有空間

### T3|Re-review 一輪 —— ✅ FABLE 已完成(2026-07-07)
驗證審結果:17/17 roadmap 全 addressed/partial、0 未處理、accept-quality。通讀抓到的 4 個數字錯誤 + 5 個語法斷點 + P0 殘留 + 新問題,共 17 項修正已全數套用並重編譯(頁面合規、字型全嵌入均複驗)。**paper 現為投稿級 v2,可直接轉教授。**唯一遺留:若做 T4 oracle 修復,§3.4 的「currently under investigation」句要更新為真實歸因。

### T4|Oracle-gap 探針(最高技術價值;修好可能全案再漲)
**狀態:診斷已由 Fable 於 2026-07-07 完成閉合——root cause 到手,剩實作。**
完整證據鏈:`reports/v1/2026-07-07_diagnosis_oracle_gap_ROOTCAUSE.md`(必讀)。一句話:**定價語義 evaluator-exact(1e-5 探針證明),腐蝕在 bitRepair commit 的有界深度 slack 更新;單 pass 高報 42% 改善(宣稱 −20,576 vs 實測 −14,513)。**

剩餘工作 = 實作 re-anchor 修復(7/13 gate):
1. 找到 bitRepair commit 路徑寫 stored slack 的地方(swap accept 後的 slack update;搜 Manager.cpp bitRepair 的 commit/apply 段)。
2. 先做便宜版驗證:每個 commit batch 後(或 ALT round 邊界)插入全域 re-anchor pass——從 parsed original per-sink slack + 當前位置/Qpd 全深度前向傳播無狀態重算所有 slack。驗收:oc_rep 實驗重跑(報告內有配方),內部宣稱 vs eval 實測的 6,063 缺口應歸零。
3. 過了再做增量版(只重算受影響 cone)省時間。
4. 修好:官方 unified 3× 重跑 → 更新兩篇 paper + thesis 數字(TCAD sec:gap-fix 的 todo、ASP-DAC 若數字變要全表重驗)。分數方向:margin 附近的假 accepts 會被正確拒絕,預期至少 tc2/hc02 受益;若有 case 反向且 >0.5%,回報告分析,別硬上。
5. 修不完:兩篇 paper 現版已寫成「root-caused、fix in progress」,自洽可投,凍結即可。
實作起點(已偵察):anchor 基礎設施已存在——`Manager.cpp:80-115`:`origDSlack_` 在 oracle build 時捕捉全部 logical FF 的原始 D-slack;`validateTNSOracle()` 已示範「setTimingSlack 還原 clean anchor → getSlack() 重算」的無狀態模式。re-anchor pass ≈ 把這個 restore+recompute 迴圈變成 commit 後的正式步驟(對受影響 cone 或全域),再讓 incr oracle 從 re-anchored 值重建(incrAccurateBuild)。
驗收注意:位移探針裡深度≥2 的都是零訊號(sinks 當時全正 slack),深層傳播只由 PROBE_CELL 間接驗證(pc_6065 delta=23 個 sink 當量穿 gate cone,1e-5 一致)——修復驗證時請加一支「深拓撲+確定負 slack sink」的位移探針(加大 dx 或挑當前負 slack 的 sink)。
PROBE_CELL harness 在 main.cpp 未 commit——先 `git add main.cpp && git commit`(訊息:probe: PROBE_CELL equal-footprint Qpd probe harness)。

### T5|投稿前 compliance(7/15–17)
1. 匿名掃描:`grep -riE "coherent17|nycu|yang|sweetcamper|thesis" sections/ main.tex`(baselineRepo 的 cite 是允許的第三方 artifact)
2. overfull:剩 2 處 ≤6pt(已確認 cosmetic,可不理)
3. PDF 檢查:✅ fonts 11/11 embedded(PyPDF2 已驗)、✅ 正文 ≤6 頁、✅ 雙盲——上傳前重驗一次即可
4. 上傳 EasyChair,確認收據
5. 文獻終掃:ICCAD'25 得主是否新發表(scite 搜一次)
6. **AI 揭露**:camera-ready 時把 `paper_aspdac2027/AI_DISCLOSURE_camera_ready.txt` 的聲明加入 Acknowledgments(投稿版 double-blind 不放);thesis 依 NYCU 規範揭露;確認用戶已通讀全文並能防守所有 claim(講稿 Q&A 是輔助)

### T6|其餘
- **出席人選(重要)**:用戶屆時(2027/1 會期)已畢業——9/4 錄取後立即與教授確定報告人(Plan A:用戶以作者身分出席,若無兵役/工作衝突,經費由教授計畫/公司/自費;Plan B:教授或在學共同作者代打)。**no-show = 論文從 Xplore 撤下**,這是錄取後唯一致命行政風險。
- V3 gates default-on 決策:paper 送出後再議
- 講稿/中文版給教授:`reports/v1/2026-07-05_講稿_給老師.md`(含 Q&A 防禦)

## 環境速查
- 統一配置:見 `v3_run_bundle/run.sh` 的 UNIENV(+EJECT 用 `ORACLE_EJECT=1 EJECT_TIME=180`)
- 穩定 binary 副本:scratchpad 的 `cadb_eject`(= V3 HEAD build);**跑長實驗一律用副本,勿用工作樹的 binary**(rebuild 會換 inode)
- 已知陷阱:背景 Bash 的 cwd 會重置到專案根(**每條背景命令都要顯式 cd**);`grep -E` 裡不要寫 `\|`;latexmk 要在正確目錄;evaluator/checkers 相對路徑
- Bundle 給用戶自跑:`v3_run_bundle/`(README 有預期分數)

## 檔案地圖
paper:`paper_aspdac2027/`(main.tex、sections/、venue_variants/、EASYCHAIR_submit.txt、paper_中文版.md)|thesis:`thesis/`|TCAD:`paper_tcad_extension/`|報告:`Project Knowledge/reports/v1/2026-07-0{4,5,6}_*`|備份:`backups_20260706b_*.tar.gz`


## 2026-07-08 追記:老師第二輪版面 feedback 已執行(Fable)
- caption 全面縮短(Fig 1/2/3/4 各砍到 1–2 句)
- 新增 Fig 4(operators 四格圖:re-pairing/rebanking/reloc+swap/eviction);§3.3 enumerate 改散文;§4 對應句壓縮
- 六圖二表、正文仍 ≤6 頁、0 overfull、全部 rasterize 目視過
- **數字凍結的兩處刻意例外**(非錯誤):intro 刪了「221 隊報名」趣聞句(fang2024overview 引用仍在 §2.1);§3.3 enumerate 的步驟編號隨改寫消失。其餘 token 不變。
- 若 T4 修好重跑,更新數字時注意以上兩處已不存在。


## GitHub 狀態(2026-07-08,Fable 記錄)
- `Coffeeturtle7/2024-ICCAD-Problem-B`:**private(已驗證,匿名 404)**;main = V3 tree(tree-exact merge a70cb18,無 force);README 已更新到 0.952 官方紀錄(刻意不提 ASP-DAC/論文字眼)
- **9/4 錄取通知後才轉 public**(雙盲期間公開 = 數字指紋破盲);共用一律走 Collaborators 邀請
- 本機 V1 worktree 的 main 落後 origin/main 一個 merge commit——正常,別「修」它
