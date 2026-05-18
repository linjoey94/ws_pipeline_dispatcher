# log_parse

`log_parse` 是 stdin -> stdout 的 UNIX filter。integration pipeline 目前只需要它保留 `type=clip` 的 JSON Lines。

## Responsibilities

- 從 stdin 逐行讀取 JSON Lines。
- 套用 filter。
- 將通過 filter 的行輸出到 stdout。
- 壞掉的 JSON line 走 stderr warning，並跳過。

## Required Integration Mode

```text
log_parse --filter type=clip
```

這是 `pipeline_dispatcher` 串接時使用的模式。

## Optional Repo-Local Modes

以下能力可以保留在 repo 實作與測試中，但不是 Linear integration contract：

- `--fields <f1,f2,...>`
- `--format json`
- `--format csv`
- `--format count`
- 多個 `--filter` 的 AND 語意

## Stdout / Stderr Rule

- stdout：只輸出通過 filter 的資料行。
- stderr：JSON parse error、filter syntax error、diagnostic log。

## Exit Codes

- `0`：stdin EOF，正常結束。
- `1`：參數錯誤。
- `2`：stdin 讀取錯誤。

## Local Test Focus

- `type=clip` 通過。
- 非 clip 被過濾。
- 壞 JSON 不讓 process crash。
- stdout 不混入 warning。
