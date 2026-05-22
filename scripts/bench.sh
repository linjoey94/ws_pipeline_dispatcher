#!/bin/bash
set -e

echo "========================================="
echo " System Environment"
echo "========================================="
uname -a
echo "CPU Cores: $(nproc)"
echo ""

echo "========================================="
echo " Phase 1: Generating Datasets"
echo "========================================="
rm -rf test_env && mkdir -p test_env
python3 scripts/gen_data.py small
python3 scripts/gen_data.py medium
python3 scripts/gen_data.py malformed

echo ""
echo "========================================="
echo " Phase 2: Running Benchmark (Medium)"
echo "========================================="
FILE_SIZE=$(wc -c < test_env/bench_medium.bin)
MB_SIZE=$(awk "BEGIN {printf \"%.2f\", $FILE_SIZE/1048576}")

echo "Processing $MB_SIZE MB of binary stream..."

# 改用 date 指令來精準計算毫秒時間差，避開 /usr/bin/time 找不到的問題
START=$(date +%s.%N)
/usr/bin/time -v ./build/stream_merge bench_medium test_env > test_env/output.jsonl 2>test_env/bench.log
END=$(date +%s.%N)
LATENCY=$(awk "BEGIN {printf \"%.3f\", $END - $START}")

THROUGHPUT=$(awk "BEGIN {printf \"%.2f\", $MB_SIZE / $LATENCY}")
CLIPS=$(wc -l < test_env/output.jsonl)
MAX_RSS=$(grep "Maximum resident" test_env/bench.log | awk '{print $NF}')

echo "Latency (Real Time): $LATENCY seconds"
echo "Throughput: $THROUGHPUT MB/s"
echo "Clips Emitted: $CLIPS"
echo "Max RSS: ${MAX_RSS} kbytes"
echo "See test_env/bench.log for stream_logger output."
echo "========================================="
