# clip_store

`clip_store` 是 pipeline 的終端持久化層。它從 stdin 讀取 clip JSON Lines，寫入純文字 index。

## Responsibilities

- 從 stdin 讀取 filtered clip JSON Lines。
- 用 `session_id:ts` 建立 key。
- 用 `path` 作為 value。
- 寫入 `--db` 指定的純文字 index。
- 支援 TTL。
- 提供 repo-local 查詢 / GC CLI。

## Required Integration Mode

```text
clip_store --db <db_path> --ttl <ttl_seconds>
```

stdin input example：

```json
{"type":"clip","session_id":"sess_001","ts":1747065600,"path":"/tmp/clips/sess_001/1747065600.mp4"}
```

## DB Format

`/tmp/clips.db` 是純文字檔，不是 SQLite。

```text
key<TAB>value<TAB>expire_at<NEWLINE>
```

範例：

```text
sess_001:1747065600	/tmp/clips/sess_001/1747065600.mp4	1747069200
```

## Write Semantics

- key 格式：`session_id:ts`。
- 相同 key 再寫入採 upsert 語意。
- 可用 append-only + GC 去重實作。
- rewrite / GC 應使用 tmp file + fsync + rename，避免 crash 造成半寫入。

## Optional Repo-Local Modes

以下 CLI 是 repo 內部管理用途，不是跨 repo contract：

- `--get <key>`
- `--list [--filter <expr>]`
- `--delete <key>`
- `--gc`

## Stdout / Stderr Rule

- stdout：操作結果 JSON，或查詢結果。
- stderr：diagnostic log。

## Local Test Focus

- pipe 寫入單筆 clip。
- 相同 key upsert 後查詢最新值。
- TTL 過期後查詢不回傳有效結果。
- GC 去重與清理過期資料。
- concurrent writer 不破壞 db。
