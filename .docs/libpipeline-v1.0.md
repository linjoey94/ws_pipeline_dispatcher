# `libpipeline` — 細部開發文件 v1.0

> **對應檔案**：`lib/libpipeline.c`、`lib/libpipeline.h`
> **版本**：v1.0 / 2026-05-18
> **定位**：`libpipeline` 是所有 applet（`stream_merge`、`log_parse`、`clip_store`、`pipeline_dispatcher`）共用的基礎函式庫，封裝跨 applet 重複出現的低階作業（inotify 監聽、時間取得、JSON 壓縮、sentinel 判別）。

***

## 一、定位與設計原則

| 原則 | 說明 |
|---|---|
| **無業務邏輯** | `libpipeline` 不認識任何 JSON schema、不解析任何 chunk 內容，只負責「機械性」的低階作業 |
| **零隱性狀態** | 所有函式皆為純函式或顯式傳入 fd / buffer，不持有 module-level 變數 |
| **POSIX-only** | 僅依賴 POSIX 1003.1（inotify 例外，Linux-specific），不引入 glibc extensions |
| **可被 mock** | 所有函式皆可在單元測試中以 LD_PRELOAD 或 weak symbol 覆寫 |

***

## 二、函式清單（v1 最小介面）

```c
/* lib/libpipeline.h — v1 最小介面 */

#ifndef LIBPIPELINE_H
#define LIBPIPELINE_H

#include <stddef.h>
#include <stdint.h>

/**
 * 建立 inotify 監聽，監聽指定目錄的 IN_CREATE | IN_MOVED_TO 事件。
 *
 * v1 主要用途：監聽 sentinel `.pipeline_end` 的建立。目錄本身不會
 * 包含 chunk 檔（v1 改為單一 growing blob），因此不再訂 IN_CLOSE_WRITE。
 *
 * @param dir_path           欲監聽的目錄絕對路徑（必須已存在）
 * @param watch_descriptor   out 參數，回傳 inotify_add_watch() 的 wd
 *
 * @return inotify fd（>= 0），失敗時回傳 -1 並設定 errno
 *
 * 呼叫者責任：
 *   - 不再使用時呼叫 close(fd)
 *   - 自行管理 inotify_event 的讀取與解析
 */
int pipeline_watch_dir(const char *dir_path, int *watch_descriptor);

/**
 * 建立 inotify 監聽，監聽單一檔案的 IN_MODIFY 事件。
 *
 * v1 用途：stream_merge 監聽 Fastify 上層持續 append 的
 * `{session_id}.bin`，收到事件後從上次 offset 繼續 tail-read。
 *
 * @param file_path          欲監聽的檔案絕對路徑（必須已存在）
 * @param watch_descriptor   out 參數，回傳 inotify_add_watch() 的 wd
 *
 * @return inotify fd（>= 0），失敗時回傳 -1 並設定 errno
 */
int pipeline_watch_file(const char *file_path, int *watch_descriptor);

/**
 * 回傳當前 monotonic 時間（毫秒）
 *
 * 用於 idle timeout 計算、processing_lag_ms 統計。
 * 內部使用 clock_gettime(CLOCK_MONOTONIC)。
 *
 * @return 自 boot 以來的毫秒數，永不回傳負值
 */
int64_t pipeline_now_ms(void);

/**
 * 壓縮 JSON 字串（移除多餘空白），輸出到 dst buffer
 *
 * 用於 emit_json_line() 輸出前壓縮，確保 stdout 一行一個 JSON object。
 *
 * @param src       null-terminated 輸入字串
 * @param dst       輸出 buffer
 * @param dst_size  dst buffer 大小（含 null terminator）
 *
 * @return 壓縮後長度（不含 null terminator），dst_size 不足時回傳 -1
 *
 * v1 簡化策略：僅移除 ASCII space (0x20)、tab (0x09)、CR (0x0D)、LF (0x0A)，
 * 不處理字串字面值內的空白（呼叫端責任避免此情境）。
 */
int pipeline_compress_json(const char *src, char *dst, size_t dst_size);

/**
 * 判斷檔名是否為 sentinel 檔（session 結束訊號）
 *
 * sentinel 規格見第四節。
 *
 * @param filename  basename（不含目錄路徑）
 *
 * @return 1 = 是 sentinel，0 = 否
 */
int pipeline_is_sentinel(const char *filename);

#endif /* LIBPIPELINE_H */
```

***

## 三、各函式行為細節

### 3.1 `pipeline_watch_dir`

```
inotify_init1(IN_NONBLOCK | IN_CLOEXEC)
    │
    ├── 失敗 → return -1（errno 由 inotify_init1 設定）
    │
    └── inotify_add_watch(fd, dir_path, IN_CREATE | IN_MOVED_TO)
            │
            ├── 失敗 → close(fd) + return -1
            └── 成功 → *watch_descriptor = wd; return fd
```

| 設計選擇 | 理由 |
|---|---|
| `IN_NONBLOCK` | 與 `select`/`poll` 整合，避免 read 阻塞主迴圈 |
| `IN_CLOEXEC` | `fork()` 後子進程不繼承 inotify fd |
| `IN_CREATE \| IN_MOVED_TO` | 主要目的是偵測 sentinel 出現；覆蓋 `touch` 與 `rename-into-place` 兩種落地 |

### 3.1b `pipeline_watch_file`

```
inotify_init1(IN_NONBLOCK | IN_CLOEXEC)
    │
    └── inotify_add_watch(fd, file_path, IN_MODIFY)
            └── 成功 → *watch_descriptor = wd; return fd
```

依照 v1 架構，Fastify 上層會持續 append `{session_id}.bin`，每次 append 觸發 `IN_MODIFY`。`stream_merge` 在主迴圈中接收事件後以上次紀錄的 offset 繼續 `read()`。

**重要**：`IN_MODIFY` 可能被 batch（kernel 合併多次 write），因此事件數不等於 write 次數；`stream_merge` 應該**以事件為觸發、以 EOF 為邊界**不斷讀取。

### 3.2 `pipeline_now_ms`

```c
struct timespec ts;
clock_gettime(CLOCK_MONOTONIC, &ts);
return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
```

使用 `CLOCK_MONOTONIC` 而非 `CLOCK_REALTIME`，避免 NTP 校時造成倒退。

### 3.3 `pipeline_compress_json`

v1 採用「白名單字元」策略，逐 byte 拷貝：

```
for each c in src:
    if c is whitespace (0x20, 0x09, 0x0D, 0x0A):
        skip
    else:
        emit c
```

**限制**：若 JSON value 內含字串字面值且字串內有空白，將被誤刪。v1 限制呼叫端只能對「自己組裝的」JSON 使用此函式（不可對 inotify 來的 user payload 直接壓縮）。v2 將切換到 token-aware 壓縮器。

### 3.4 `pipeline_is_sentinel`

```c
return strcmp(filename, ".pipeline_end") == 0;
```

固定字串比對。v1 不支援自訂 sentinel 名稱，避免引入 config 相依。

***

## 四、Sentinel 檔案機制（與 Fastify 上層的契約）

sentinel **不是**「下層開始工作」的觸發點（dispatcher 在 `STRT` 階段已經啟動、`stream_merge` 已持續 tail-read）；它是「**上層已 close、下層讀完 EOF 後可以退出**」的信號。

| 項目 | 規格 |
|---|---|
| 檔名 | `.pipeline_end`（固定） |
| 放置位置 | `src_dir`（即 `/tmp/stream/{session_id}/`），與 `{session_id}.bin` 同目錄 |
| 建立者 | Fastify，於收到 `END_` opcode 並呼叫 `streamer.end()` 完成後 |
| 觸發時機 | `streamer.end()` 的 flush 回呼完成 → `fs.writeFileSync(sentinelPath, '')` |
| 內容 | 空檔案（不依賴內容，依賴存在性） |
| `stream_merge` 的處理 | 偵測到 → 繼續 drain `{session_id}.bin` 剩餘 byte → flush 最後一條 clip → exit 0 |
| 清理責任 | `pipeline_dispatcher` 在三個 applet 全部退出後，rm `.pipeline_end` 與整個 `src_dir`（若啟用 TTL 清理）|

### 4.1 上下層結束順序

```
t=0     Fastify STRT  → open {session_id}.bin (append) + spawn dispatcher
t=0→T   Fastify DATA × N → append byte                  // 與下層並行
        stream_merge IN_MODIFY × K → tail-read → emit clip // 同時進行
t=T     Fastify END_  → streamer.end()
                       → touch .pipeline_end       (上層先退)
t=T+ε   stream_merge 偵測 sentinel
                       → 繼續 read() 直到 EOF       (處理剩餘 byte)
                       → emit final clip
                       → exit 0
        log_parse / clip_store 依序收到 EOF、順勢退出
        pipeline_dispatcher 收集 waitpid → exit 0    (下層最後退)
```

### 4.2 為何用 sentinel 而非 SIGTERM / EOF 本身？

| 方案 | 優點 | 缺點 |
|---|---|---|
| Sentinel 檔（採用） | 與 inotify 事件迴圈同源、無需訊號處理；能區分「上層明確 END_」與「WS 意外斷線」| 多一次 filesystem write |
| 僅靠 EOF | 最少狀態 | `read()` 在 `{session_id}.bin` 上只會返 0，下層無法區分「上層暫停」與「已結束」|
| SIGTERM | 即時 | 須注入 PID、訊號處理難寫得正確 |
| WS close 事件 | 概念簡單 | Fastify 與 C 之間無 IPC，無法直接傳遞 |

***

## 五、錯誤處理與診斷

`libpipeline` 不直接呼叫 `LOG_*`（避免循環依賴 `stream_logger`），錯誤一律透過 errno + 回傳值傳達。呼叫端負責呼叫 `perror()` 或 `LOG_ERROR()`。

```c
int wd;
int fd = pipeline_watch_dir("/tmp/stream/sess_001", &wd);
if (fd < 0) {
    LOG_ERROR("pipeline_watch_dir failed: %s", strerror(errno));
    return -1;
}
```

***

## 六、檔案對應與相依

```
ws_pipeline_dispatcher/
├── lib/
│   ├── libpipeline.h        ← 本文件對應介面
│   └── libpipeline.c        ← 本文件對應實作
├── applets/
│   ├── pipeline_dispatcher.c    ← #include "libpipeline.h"
│   ├── stream_merge.c           ← #include "libpipeline.h"
│   ├── log_parse.c              ← #include "libpipeline.h"
│   └── clip_store.c             ← #include "libpipeline.h"
└── tests/
    └── test_libpipeline.c       ← 單元測試
```

**外部相依**：僅 libc + Linux 2.6.13+ 的 inotify7。不依賴 `stream_logger`、`cJSON` 等其他模組。

***

## 七、版本演進規劃

| 版本 | 變更 |
|---|---|
| v1.0 | 本文件描述的最小四函式 |
| v1.1 | 新增 `pipeline_atomic_write()`（write + fsync + rename 封裝） |
| v1.2 | 新增 `pipeline_safe_spawn()`（封裝 fork + exec + fd 重定向） |
| v2.0 | `pipeline_compress_json` 升級為 token-aware 壓縮器 |

***

*本文件對應 `ws_pipeline_dispatcher` repo，`libpipeline` 模組。*
*v1.0 — 2026-05-18*
