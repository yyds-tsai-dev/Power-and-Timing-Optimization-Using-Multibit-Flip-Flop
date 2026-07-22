#!/bin/bash
# v3 coronation ritual, side A. Run from repo root:  THREADS=40 REPS=3 bash knowledge/coronation/a_ritual.sh
# Frozen env per knowledge/reports/2026-07-13_v3_freeze_list.md; threads free per machine.
# Auto-reconciles every run against B's reference scores -> coronation_a/XMACHINE.tsv.
set -u
REPO=$(pwd)
T=${THREADS:-40}
N=${REPS:-3}
OUT=$REPO/coronation_a
BIN=${BIN:-$REPO/cadb_0015_final}
EVBIN=$REPO/evaluator/preliminary-evaluator
mkdir -p $OUT
FROZEN="OMP_NUM_THREADS=$T BANKING_MODE=matching PRODUCTION=1 INCR_RELOC=1 RELOC=1 CRIT_SWAP=1 BIT_REPAIR=1 BIT_REPAIR_DYNA=2 BIT_REPAIR_RESCAN=4 BIT_REPAIR_BATCH=1 REFINE_ALLFF=1 BIT_REPAIR_INTRA=1 BIT_REPAIR_TIME=1200 ORACLE_REBANK=1 REBANK_TIME=600 DENSITY_REPAIR=1 ORACLE_EJECT=1 EJECT_TIME=180 EVAL_ANCHOR=1 CONV_TERM=1 CONV_RATE=1e-6 ADAPT_STACK=1 REBANK_MODES=15 LNS_KICK=1 LNS_SPLIT=1 LNS_REGION_K=4 LNS_TIME=120 ALT_ROUNDS=8 STAGE_CONV=1"
echo "$FROZEN" > $OUT/env_dump.txt
declare -A BREF=(
  [tc2]=702879.888776541   [tc1]=734252912.190722  [hc02]=9615766.92728436
  [hc03]=55769896.912118   [tc3]=725793475.831126  [hc01]=29933155.7870576
  [hc04]=725923744.439963 )
printf "case\trep\tscore_A\tscore_B\tmatch\n" > $OUT/XMACHINE.tsv
for rep in $(seq 1 $N); do
for C in testcase2_0812:tc2 testcase1_0812:tc1 hiddencase02:hc02 hiddencase03:hc03 testcase3:tc3 hiddencase01:hc01 hiddencase04:hc04; do
  f=${C%%:*}; t=${C##*:}; tag=${t}_a${rep}
  echo "[$(date +%H:%M:%S)] $tag start T=$T load=$(cut -d' ' -f1-3 /proc/loadavg)" >> $OUT/ritual_a.log
  /usr/bin/time -o $OUT/${tag}.time -f "%e s %P" env $FROZEN $BIN $REPO/testcase/$f.txt $OUT/${tag}.out > $OUT/${tag}.log 2>&1
  s=$($EVBIN $REPO/testcase/$f.txt $OUT/${tag}.out 2>&1 | tail -1)
  c1=$(cd $REPO && ./checker/sanity testcase/$f.txt $OUT/${tag}.out 2>&1 | tail -1)
  c2=$(cd $REPO && ./checker/placement_checker testcase/$f.txt $OUT/${tag}.out 2>&1 | tail -1)
  echo "$tag: $s | sanity:$c1 | placement:$c2 | $(cat $OUT/${tag}.time)" >> $OUT/ritual_a.log
  sa=$(echo "$s" | grep -oE '[0-9]+\.?[0-9]*' | tail -1)
  [ "$sa" = "${BREF[$t]}" ] && m=PASS || m=CHECK
  printf "%s\t%s\t%s\t%s\t%s\n" "$t" "$rep" "$sa" "${BREF[$t]}" "$m" >> $OUT/XMACHINE.tsv
  # B precedent: if rep2 output is byte-identical to rep1, rep3 may be waived
  if [ $rep -ge 2 ]; then cmp -s $OUT/${t}_a1.out $OUT/${tag}.out && echo "$tag: BYTE-IDENTICAL to a1" >> $OUT/ritual_a.log; fi
done
done
echo A_RITUAL_DONE >> $OUT/ritual_a.log
