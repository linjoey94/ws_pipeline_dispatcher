# log_parse

`log_parse` 是 stdin -> stdout 的 pipe-friendly 結構化日誌解析器。

它需要同時滿足兩個需求：

- 課程方向三「Embedded Data Pipeline」：支援 regex-based 解析、JSON/CSV 輸出、欄位選取與條件過濾。
- 本 repo integration pipeline：支援 `log_parse --filter type=clip`，保留 `stream_merge` 送出的 clip JSON Lines 給 `clip_store`。

## Responsibilities

- 從 stdin 逐行讀取 input line。
- 支援 regex-based parsing，將 raw log line 轉成 structured record。
- 支援輸出 `--format json` 與 `--format csv`。
- 支援 `--fields <f1,f2,...>` 欄位選取。
- 支援最小 filter expression：`--filter key=value`。
- integration mode 下，支援已是 JSON Lines 的 input，並用 `--filter type=clip` 過濾。
- parse failure、壞 JSON、filter syntax error 走 stderr warning，並跳過該行。

## Course Tool Mode

課程要求的主要模式是結構化日誌解析器。最小 CLI shape：

```text
log_parse --regex <pattern> --fields <f1,f2,...> --format json [--filter key=value]
log_parse --regex <pattern> --fields <f1,f2,...> --format csv  [--filter key=value]
```

`--regex` 使用 POSIX regex。欄位名稱由 `--fields` 提供，依 capture group 順序對應。

範例 input：

```text
1747065600 clip /tmp/clips/sess_001/1747065600.mp4
```

範例 command：

```text
log_parse --regex '^([0-9]+) ([a-z_]+) (.+)$' --fields ts,type,path --format json --filter type=clip
```

範例 output：

```json
{"ts":"1747065600","type":"clip","path":"/tmp/clips/sess_001/1747065600.mp4"}
```

## Integration Mode

`pipeline_dispatcher` 串接時的必要模式：

```text
log_parse --filter type=clip
```

在這個模式下，stdin 預期是 JSON Lines。`log_parse` 不改寫通過 filter 的 line，直接 pass-through 給 `clip_store`。

範例 input：

```json
{"type":"clip","session_id":"sess_001","ts":1747065600,"path":"/tmp/clips/sess_001/1747065600.mp4"}
{"type":"heartbeat","session_id":"sess_001","ts":1747065601}
```

範例 output：

```json
{"type":"clip","session_id":"sess_001","ts":1747065600,"path":"/tmp/clips/sess_001/1747065600.mp4"}
```

## JSON Handling Boundary

JSON parsing/filtering 屬於 `log_parse` 的責任，不屬於 `libpipeline`。

`libpipeline` 不引入 `cJSON`，也不提供 JSON compression helper。若 `log_parse` 需要解析 JSON Lines，應在 `log_parse` applet 內用最小 schema-aware parsing 或獨立 parser 實作。

## Stdout / Stderr Rule

- stdout：只輸出通過 parse/filter 的 structured records。
- stderr：parse error、JSON error、filter syntax error、diagnostic log。

這條規則很重要，否則 UNIX pipe 會被 warning/log 污染。

## Exit Codes

- `0`：stdin EOF，正常結束。
- `1`：參數錯誤或 filter/regex syntax error。
- `2`：stdin 讀取錯誤。

## Local Test Focus

- regex raw log input 可輸出 JSON。
- regex raw log input 可輸出 CSV。
- `--filter type=clip` 只保留 `type=clip` record。
- integration JSON Lines input 可 pass-through clip records。
- malformed input 不讓 process crash。
- stdout 不混入 warning 或 diagnostic log。
