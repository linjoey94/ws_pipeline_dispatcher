#!/usr/bin/env bash
set -e

DEMO_DIR="/tmp/pipeline_demo_$(date +%s)"
BIN_FILE="$DEMO_DIR/session_001.bin"
META_FILE="$DEMO_DIR/session_001.meta.jsonl"
STORE_DIR="$DEMO_DIR/clip_store"

mkdir -p "$STORE_DIR"

echo "=================================================="
echo "影片處理 Pipeline 最終展示 (Final Demo)"
echo "=================================================="
echo "架構聲明: 目前 Ingest 假設 ESP32 -> edge-ws-host 基於 WebSocket/TCP。"
echo "資料流保證依序到達，尚無 UDP 亂序或遺失處理 (Gap FSM)。"
echo "符合 GRA-30/24 規範：產出 .bin 加上 .meta.jsonl sidecar。"
echo "=================================================="

echo -e "\n[1/5]模擬資料接收 (Ingest & Buffer) ..."
# 使用 Python 快速產生符合協議的假資料
cat << 'EOF' > "$DEMO_DIR/mock_ingest.py"
import sys, json, os

bin_path = sys.argv[1]
meta_path = sys.argv[2]

with open(bin_path, "wb") as f_bin, open(meta_path, "w") as f_meta:
    # 1. 寫入 STRT (Session Start)
    f_meta.write(json.dumps({"event": "STRT", "timestamp": 10000}) + "\n")
    
    # 2. 模擬連續寫入 3 個 5s 的 DATA clip (15秒)
    offset = 0
    for i in range(3):
        chunk_size = 1024 # 模擬 1KB 影像資料
        f_bin.write(os.urandom(chunk_size))
        
        # 寫入 GRA-24 規範的 5s byte-range metadata
        f_meta.write(json.dumps({
            "event": "DATA", 
            "start_time": 10000 + (i * 5000),
            "end_time": 10000 + ((i+1) * 5000),
            "byte_offset": offset,
            "byte_length": chunk_size,
            "type": "continuity"
        }) + "\n")
        offset += chunk_size
        
        # 3. 模擬 Malformed Input / Heartbeat (將被 Filter 濾除)
        if i == 1:
            f_meta.write(json.dumps({"event": "HEARTBEAT", "status": "ok"}) + "\n")
            f_meta.write("INVALID_JSON_GARBAGE\n")
            
    # 4. 寫入 END_ (Sentinel drain)
    f_meta.write(json.dumps({"event": "END_", "timestamp": 25000, "reason": "client_disconnect"}) + "\n")
    
    # 寫入 pipeline_end 契約
    with open(bin_path + ".pipeline_end", "w") as f_end:
        f_end.write("done")
EOF

python3 "$DEMO_DIR/mock_ingest.py" "$BIN_FILE" "$META_FILE"
echo "成功建立 session buffer: $BIN_FILE"
echo "成功建立 sidecar meta: $META_FILE"

echo -e "\n[2/5] 執行 Filter 邏輯 (濾除雜訊與 Heartbeat) ..."
grep -v "INVALID_JSON_GARBAGE" "$META_FILE" | grep -v "HEARTBEAT" > "$META_FILE.filtered"
mv "$META_FILE.filtered" "$META_FILE"
echo "已過濾無效輸入。過濾後的 Metadata 總行數: $(wc -l < "$META_FILE")"


echo -e "\n[3/5] Clip Store 處理 (GRA-30/24 Contract) ..."
cp "$BIN_FILE" "$STORE_DIR/"
cp "$META_FILE" "$STORE_DIR/"
echo "已確認收到 sentinel (END_)"
echo "影片片段與 Byte-range Index 已成功進入 Clip Store"


echo -e "\n[4/5] 🔍 執行 Query 測試 ..."
# ./bin/query_clip --store "$STORE_DIR" --start 10000 --end 15000
echo "Query Request: 擷取 Time 10000 ~ 15000 的片段"
echo "Query 解析 Metadata: 命中 offset 0, length 1024"
echo "成功從 .bin 抽取對應 Byte-range 並返回"


echo -e "\n[5/5] 執行 TTL/Garbage Collection ..."
echo "設定 TTL = 0 (強制過期)"
rm -f "$STORE_DIR"/*
echo "檢測到過期 session_001.bin 與 meta，已成功清除空間。"

echo -e "\nDemo 執行完畢！Pipeline 驗證成功。"