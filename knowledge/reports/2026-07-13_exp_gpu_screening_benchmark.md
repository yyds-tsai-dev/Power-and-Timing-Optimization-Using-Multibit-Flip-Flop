# GPU 篩選加速實驗 — benchmark 結果(2026-07-13,server B)

**Type**: exp
**Plan**: `plans/plan_gpu_screening_experiment.md`(§0–§5 假說與判讀標準)
**環境**:NVIDIA L4 24GB(執行期間獨佔,始跑時 util 4%/4MiB)、CUDA 12.2、
sm_89、160 核 4-socket 主機(load ~12 起跑)。dump 快照:tc2 凍結快取,
`gates=132293 bits=21164 cands=566044`,CPU 真值 `evalBitSwapDelta`(hash 版,
64T,9.553s = 59,252 cand/s)。工件:`~/scratch/gpu_proto/`(tc2gpu.snap/.cand、
gpubench_result.txt、gpubench_v2_result.txt、tools/gpu_screen_bench{,_v2}.cu)。

## 1. 正確性關卡(手冊 §3 第一判準)— **PASS**

| 配置 | maxErr vs 真值 | 判定 |
|---|---:|---|
| CPU flat 1T,cap 2048/4096(v1) | 1.637e-11 | ✅ 平坦化忠實 |
| CPU flat 1T,cap 4096/8192(v2) | 3.274e-11 | ✅ |
| GPU FP64(v1 cap) | 1.273e-11 | ✅ GPU 逐行對映忠實 |
| GPU FP64(v2 cap) | 2.910e-11 | ✅ |

所有後續吞吐數字據此有效。

## 2. 吞吐量三方對照

| 配置 | 時間 | 吞吐 | spill(未定價) |
|---|---:|---:|---:|
| hash 64T(dump 內建,全候選) | 9.553s | 59,252 c/s | 0 |
| flat 160T,cap 2048/4096(v1) | 4.486s | 126,180 c/s | 50,485(8.9%) |
| flat 1T,cap 2048/4096(v1) | 238.2s | 2,376 c/s | 50,485 |
| flat 1T,cap 4096/8192(v2) | 644.9s | 878 c/s | 16,483(2.9%) |
| flat 64T,cap 4096/8192(v2) | 15.19s | 37,261 c/s | 16,483 |
| flat 160T,cap 4096/8192(v2) | 11.94s | 47,406 c/s | 16,483 |
| GPU FP64,cap 2048/4096(v1) | 173.4s | 3,264 c/s | 50,485 |
| GPU FP64,cap 4096/8192(v2) | 736.2s | 769 c/s | 16,483 |
| GPU FP32,cap 4096/8192(v2) | 729.6s | 776 c/s | 16,483 |

**GPU FP64 0.03×**:手冊預期內(L4 FP64 = 1/64 速率,Ada 消費級),非失敗。

## 3. 主發現:cone 大小決定架構,不是精度

cap 從 2048/4096 加倍到 4096/8192 後,spill 8.9%→2.9%,但吞吐**崩了**
(160T:126k→47k c/s;64T 甚至輸給 hash 版)。原因:原型的 cone 去重/查找是
線性掃描(O(cap) 每次插入 → O(cap²) 每候選),(2048,4096] 區間 ~3.4 萬個
大 cone 候選每個 ~12ms,吃掉全部收益。

**生產架構結論:按 cone 大小分流,不是調大 cap。**
- 小 cone(~91% 候選):flat/GPU 篩選,126k c/s(160T CPU)起跳;
- 大 cone 尾巴(~9%):直接掉回 CPU exact(`evalBitSwapDelta`)——apply 時
  本來就重報價,呼叫面契約天然支援 fallback,零語義風險;
- de-hashing 對小 cone 是實打實的收益(v1 160T = 2.1× hash 64T 全集吞吐,
  且只用了篩選段;A 的 v4 de-hashing 項有獨立佐證價值)。

## 4. FP32 篩選命中率(生產路線判準)— 精度成立,但槓桿不在精度

- top-1000 召回率:**99.90% @M=K,100.00% @M=2K**
- top-10000 召回率:**100.00% @M=K,100.00% @M=2K**
- sign-miss:2181/22990(9.487%)——集中在近零 delta 邊緣候選(不影響 top-K)
- FP32 maxErr = 1.723e-02(絕對 TNS 單位;對「排序篩選」用途無害,如上召回率所證)

**兩個結論:**
1. **精度背書成立**:FP32 篩 + CPU exact 重報價,top-K 贏家零漏(@M=2K 全召回)。
   未來任何 GPU kernel 可放心用 float。
2. **FP32 不是加速槓桿**:FP32 vs FP64 = **1.01×**(729.6s vs 736.2s)。
   手冊 §3「FP32 全速 ~30 TFLOPS 才是真槓桿」被實測推翻——kernel 是
   **線性掃描 bound**(O(cap²) 去重/查找,整數比較 + 分支發散),不是 FLOP bound。
   L4 的 FP64 1/64 罰則根本沒在瓶頸路徑上。

## 5. 執行記錄與異象

- v1 執行期間(03:42–03:56)monitor 曾撈到一組來源不明的數字
  (1T 391s / maxErr 8.9e-10 / spill=0 / 64T 8.15s),與落檔版不符;疑似
  背景任務重複啟動或 NFS 快取殘留。**以落檔的 gpubench_result.txt /
  gpubench_v2_result.txt 為權威。**
- v2 的 CPU 段(04:33–04:36)在機器僅 load~12 時獨佔跑完;GPU 段與 T128
  探測並行(GPU 段僅佔 1 核,對 128T 牆鐘影響可忽略,記錄在案)。
- 手冊風險 2(L4 共用)本輪未發生:全程獨佔。

## 6. 定案與下一步

**實驗判決**:語義對映 ✅(maxErr ~1e-11,spill 逐位同步);FP32 精度 ✅
(top-K 召回 100%@M=2K);**GPU 現形態吞吐 ✗**(769–776 c/s,輸 CPU flat
160T 60×,輸 v1-cap 的 160T 更多)。瓶頸是演算法形狀不是硬體:
one-thread-per-candidate + O(cap²) 串行掃描 = SIMT 最差情境。

1. **生產近路(不需 GPU,可先接)**:CPU flat(de-hash)篩選 + cone 大小
   分流(大 cone → `evalBitSwapDelta` exact fallback)。v1-cap 實測 126k c/s
   (160T)= hash 版 2.1×,且只算了篩選段;直接可接回 bitRepair。
   同時給 A 的 v4 de-hashing 項一個獨立量化佐證。
2. **GPU 要贏需 kernel 重設計**(V2/DAC follow-up,~數天):warp-per-candidate
   (32 lane 平行掃描)、排序合併去重取代線性掃描、cone 常駐 shared memory、
   cone 大小分桶(手冊風險 1 順帶解決)。需 ≥100× 現 GPU 吞吐才追平 160T CPU;
   價值主張改為「篩選卸載到 GPU,讓 160 核全留給 commit/其他階段」的
   pipeline 重疊,而非原始吞吐。
3. FP32 精度背書保留:未來 kernel 直接用 float(記憶體流量減半亦有利)。
4. Novelty 定位(手冊 §5)修正:「GPU-accelerated exact-oracle refinement」
   故事仍活,但 DAC 稿的 runtime 章先寫 CPU flat + 分流(有數字),GPU 留
   follow-up(等 kernel 重設計有數字再上)。
