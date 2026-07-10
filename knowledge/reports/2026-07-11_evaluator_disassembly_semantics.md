# Evaluator 完整語義反組譯(2026-07-11,server B)

**方法**:發現 `evaluator/preliminary-evaluator` **沒有 strip**(5,540 個符號)→ 8-agent 並行反組譯
全部計分函式,逐指令釘死型別與捨入點。本報告取代黑盒推斷成為 evaluator 語義的權威紀錄
(黑盒報告 `2026-07-11_evaluator_semantics_blackbox.md` 的結論全部相容,並補上它蓋不到的角落)。
**給 server A oracle 修復的可執行情報在 §1–§3;§7 是本機複測數據。**

原始材料(server B `~/scratch/`):`eval_disasm.txt`(objdump 全文)、
`eval_reverse_journal.jsonl`(8 個 agent 的完整逐指令分析)、`eval_reverse_full.json`。

---

## 1. 對 404/352 殘差最重要的三個發現

### 1a. Gate 內部弧只連到第一個 OUT pin —— 次要輸出是死的

`InstDB::createPins`(0x4240d0):gate 建弧時把**所有 IN pin 都接到 `outs[0]`**(庫檔 pin
宣告順序的第一個 OUT-type pin),**其餘 OUT pin 的 fanin 永遠是空**。後果鏈:

- `markNoDelay`(0x41f3d0,updateTiming 開頭必跑):可達性 BFS——一個 pin 除非其 fanin 錐
  能達到 input port 或 FF Q,否則標 `noDelay`。OUT2+ fanin 空 → noDelay;
  **其獨佔下游(經由它驅動的 gate IN、再下游、直到 FF D)整個錐都被連鎖標 noDelay**。
- `Pin::timing()`:noDelay pin 一律回 `(arrival=0, slack=0)` → **TNS 貢獻恰為 0,
  連負的 parse slack 都被歸零**(不是回 parse slack!)。

我們的 C++ oracle(computeAccurateTNS)把所有 OUT pin 一視同仁地傳播 arrival——結構性分歧。
tc2 的 gate 庫全是多輸出(OUT1/OUT2)。**修復方向:oracle 必須複製「僅 outs[0] 傳播 +
noDelay 錐歸零」語義**(以庫檔 pin 順序判定第一個 OUT)。

### 1b. 無驅動 D pin 也被歸零(N bucket 語義錯了)

fanin 為空的 FF D pin 同樣被 markNoDelay 標死 → evaluator 給 slack=0、TNS 貢獻 0。
我們的 C++ 對「無 prev」bit 保留 anchor slack(tc2 base 態:N bucket 35 個負 slack bit,
貢獻 0.6039)——**這 0.6 全是我們多算的**。修復:無 prev(且非 IO 驅動)的 bit 貢獻改 0。

### 1c. 全程 float32 算術(slack 的量化與結合順序)

- arrival 存 f32(`Pin::setArrival(float)`);**第一次** setArrival 快照 `arr0`(parse 錨)。
- **slack = f32( f32(parsedSlack + arr0) − arrCur )** —— 注意結合順序是 `(slack0+arr0)−arr`,
  不是 `slack0−(arr−arr0)`;兩次 f32 捨入。
- sink arrival = `f32( arr_src + f32( f32(int曼哈頓距離) × dd_f32 ) )`(距離全 int32 精確,
  再 cvtsi2ss;dd 是 strtof 的 f32)。
- FF Q arrival = **目前 mapping 到的 cell 的 Qpd(f32)**,launch-only,不含 CLK/D。
- gate OUT arrival = 對 IN pin 的 `timing().first` 做 **maxss 摺疊,種子 0.0f**。
- TimingSlack/QpinDelay/DisplacementDelay/Alpha/Beta/Gamma/Lambda/BinMaxUtil 全部 **strtof→f32**;
  GatePower 例外:**stof 成 f32 後升 double 存**(值仍是 f32 精度)。
- TNS = 對 `hasSlack` pin(即輸入檔有 TimingSlack 行者)以 **pin-id 順序** f64 累加
  `−f64(slack_f32)`,條件 `0.0f > slack` 嚴格比較。

單一 bit 的 f32 量化誤差可到 ~1e-4(arrival ~1e3–1e5 時);21k bit 聚合後屬雜訊級
(±數個 TNS 單位),**不足以解釋 352,主嫌是 1a/1b 的結構性分歧 + commit 路徑**。

## 2. 完整評分管線(main @0x407d60)

1. `readInput`:數值型別如 §1c;DieSize 四值以 float 讀入後 `cvttss2si` 截斷成 int32,
   存 (x0,y0,寬,高)。
2. 建 **parseDB**(輸入 netlist;port 是 type 2/3 偽 cell,pin 名含 "inputport"/"outputport")。
3. Net 建弧:**CLK pin(type 6)完全不建 timing 弧**;FF 內部建 D→Q 弧(D 在傳播時不外傳,
   僅供 markNoDelay/種子邏輯);SinkDelay 斷言每個 sink fanin ≤1(多 driver → abort)。
4. `updateTiming(parseDB, dd_f32, false)`:Kahn 拓撲(pin id 升冪種子,FIFO deque),
   **每個 pin 的 arr0 在此定錨**。
5. `readOutput` + 各 check;**mapping()(0x42cfb0)把 parseDB 的錨(initialTiming = arr0/slack0)
   移植到 contestDB 對應 pin**(contestDB = 輸入 netlist + .out 的新 FF 實例/座標/cell)。
6. `updateTiming(contestDB, dd_f32, false)` → arrCur。
7. `score()` → `cout << setprecision(15)`(**general 格式 %.15g,非 fixed**)。

`checkCycle` 是 dead code(整個 binary 無呼叫點)。

## 3. score 組合式(逐運算型別)

```
acc  = f64(alpha_f32)·TNS_f64 + f64(beta_f32)·TPWR_f64          # 兩個 mulsd,一個 addsd
acc += f64( f32(TAREA_i64) ×f32 gamma_f32 )                     # cvtsi2ss + mulss(f32 乘!)
acc += f64( f32(nviol_i32) ×f32 lambda_f32 )                    # 同上
```

- `TAREA(db,true)`:FF-only,**int64 累加 int32(w×h)**,精確。
- `TPWR`:FF-only,f64 累加(值為 f32 精度的 GatePower)。
- 面積 >2^24 時 `cvtsi2ss` 會捨入(tc2 FF 面積 6.97e11 → f32 有損,已含在公式內)。

## 4. Density 官方語義(首次驗證,終結「verify bin-grid rounding anyway」懸案)

`numBinViolation`(0x434a70)+`getOverlapBins`+`overlapArea`,**全 int 算術**:

- 網格:**numCols = trunc(x1 / binW)、numRows = trunc(y1 / binH)**(用 x1/y1 原值,
  不是 x1−x0;截斷不是 ceil)→ **右/上不足一格的殘條沒有 bin**;
  inst 落入殘條 = C++ 陣列越界 UB(合法輸入靠 placeViol 擋住,但「in-die 且在殘條」
  理論上仍可能——tc2 恰好整除,無此問題;**其他案要逐案檢查整除性**)。
- 分子 = `getAllInsts(false)` = **FF + Gate 都算**(只排除 port 偽 cell);
  overlap 全 int32(乘積 sext 到 int64 累加)。
- bin 邊界不裁剪到 die;半開語義:上邊恰在格線 → 不佔上一格。
- 門檻 = `f32( f32(i32(binW×binH)) × maxUtil_f32 / 100.0f )`,分母恆為整格面積
  (邊緣 bin 不縮),違規判定 `f32(area_i64) > thr` **嚴格大於**。
- 內部 mimic(`calculateBinDensityCost`)的 ceil 網格 + double 算術與官方**不同**
  (tc2 整除時結果一致;非整除案可能算出不同违规 bin 數,翻案/密度修復前要對表)。

## 5. Pin type 判定與建弧細節(重現必抄)

`getPinTypeFromName` 優先序:含"outputport"→1;含"inputport"→0;Q 開頭→2;D 開頭→3;
CLK 前綴→6;in/IN 前綴→5;out/OUT 前綴→4;否則 7。
FF D/Q 配對(`getDQPairs`):"D"/"Q"→pair0,"Dk"/"Qk"→pair k(尾碼 stoi)。
TNS 迭代順序 = pin id = 實例建立順序 × 庫 pin 宣告順序(f64 累加順序效應 < 列印精度,
重現不需要模擬 libstdc++ hashtable)。

## 6. 對 oracle 修復的優先序建議(server A)

1. **1a OUT2 死錐**:先量 tc2 有多少 bit 掛在次要輸出錐下(靜態圖分析即可),
   直接解釋殘差的候選首位。注意死錐 bit 的 slack 是「歸零」不是「凍結」——
   負 slack bit 被 evaluator 白送,**這對優化是免費的 TNS 減免,oracle 修對後
   這些 bit 應從計價中剔除**(有翻案價值:把 FF 搬進死錐=零時序代價?——
   不行,mapping 後 D pin 的驅動不變;但 bit repair 對死錐 bit 的讓步是浪費)。
2. **1b 無驅動 D 歸零**:一行修(N bucket 貢獻改 0)。
3. **1c f32 語義**:若要逐位重現(refsta 驗收),按 §1c 公式與捨入點照抄;
   只追殘差數量級的話可後做。

## 7. 本機(server B)tc2 base 態複測

- 配置:`BANKING_MODE=matching PRODUCTION=1 EVAL_ANCHOR=1 TNS_ORACLE_VALIDATE=1`,
  binary = HEAD(30eef42/a8e3f2b)重編(g++12,動態連結;`inc/Legalizer.h` 補 `#include <list>`)。
- `accurateTNS(EVAL_ANCHOR=1) = 11,308.034320`;evaluator Final score = 801,541.886212289
  → β·P=127,700.000、γ·A=557,235.648、λ·D=0(假設模型,tc2 整除下與官方同)
  → **implied TNS = 11,660.624,殘差 = +352.59**(server A 為 404;跨機軌跡分岔,量級同族)。
- Bucket 分解(EVAL_DIAG):BFS 17,524 bit / 10,954.8;直連 FF→FF 3,182 bit / **352.593**;
  IO 30 / 0;NULL 428 / 0.604。Q bucket ≈ 殘差是**巧合候選**,尚未定案:
  黑盒微實驗(見下)排除了直連 FF→FF 的簡單計價偏差。
- **微實驗**(直連 F1.Q→F2.D,黑盒 16 實驗未蓋到的結構):sink 移動/Qpd 交換/driver 移動
  的 evaluator 分數全部命中我們模型(30.02/130.02/130.02/0.02)→ 直連語義本身無偏差。
- per-bit 對照基礎設施:`computeAccurateTNS` 的 `EVAL_DIAG_DUMP=<path>` 鉤子
  (**未 commit**,patch 在 server B `~/scratch/refsta/eval_diag_dump_hook.patch`,
  72 行,含 21,164 bit 的 tc2 dump `~/scratch/run_tc2base/tc2_base_perbit.tsv`);
  refsta Python 骨架(parser/圖/引擎)在 `~/scratch/refsta/refsta.py`,
  數值策略已按本報告語義參數化,尚未跑通 byte-exact 驗證(工作因分工調整移交 server A)。

## 8. Server B 環境筆記

- boost 1.84:jfrog 死鏈 → `https://archives.boost.io/release/1.84.0/source/boost_1_84_0.tar.gz`
  (header-only 即可,不用 b2);g++ 12.1.1 需 `inc/Legalizer.h` 加 `#include <list>`(已改,
  待 commit);`-static` 連結因無 glibc-static 改動態。
- `/` (含 /tmp) 100% 滿(他人大檔,勿動)→ 一切暫存/輸出走 NAS `~/scratch/`;
  gcc 加 `-pipe` + `TMPDIR`。
- **無 GitHub 憑證(HTTPS 無帳密、無 SSH key)→ pull/push 全阻塞**,同步協議斷線,
  需使用者配置。
- 160 邏輯核(4 socket × 20 core × HT),共用機,load ~140/160——benchmark 需記錄負載。
