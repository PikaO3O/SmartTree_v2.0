# 排錯

## 快速判斷

| 現象 | 優先檢查 | 下一步 |
|---|---|---|
| 只接 UART，PING 無回覆 | 是否燒錄根目錄 F／E、正確 COM、115200、新行、RESET | A～D 初始等待會阻塞；若 F 也無回覆，保存完整開機訊息與燒錄結果 |
| UART 回覆 WAIT_HOST_STREAM | OTG 是否接上、Windows 相機是否真正啟動該裝置 | 初次 nnbegin 正在等協商，這個狀態本身正常；PING 應繼續回覆 |
| OTG 沒有 COM 埠 | OTG 是 UVC 影像，不是 UART | 換 UART 接口測序列；不用因此更改 USB descriptor |
| 相機黑畫面 | 先用同一條線、相同設定跑 A0，再 A1 | 若兩者都失敗，先回復你已成功的官方環境；不要同時調整模型與解析度 |
| A0 好、A1 差 | preset 與一般 NN 啟動配置是不同路徑 | 將 F 的 SMARTTREE_DISPLAY_PROFILE 設 1，只改這個變數重測 |
| 只有完整文字時凍結 | OSD OFF → BOXES → FULL，其他條件不動 | 比較同段 MEM／FPS／queue_drops，保存顯示卡頓影片，不能未測就認定 OSD 根因 |
| host_stream=1 但 stream_ready 偶爾 0 | ready 含 recycle queue 瞬間狀態 | 這不觸發重啟，影像正常可繼續；若影像也停止則保存 STATUS |
| 關相機後重開不恢復 | 原格式／fps 是否相同、主機是否持續占用裝置 | STATUS → USB RETRY → 關閉再重新開啟相機；等一次新協商，UART 應繼續可用 |
| USB RETRY 一直 WAIT_HOST_STREAM | 指令只呼叫一輪 nnbegin，沒有自動重試 | 再次開相機產生協商；仍無法回復時保存資訊再 RESET，不連續狂送 RETRY |
| context_ready=1、callback_seen=0 | 模型封裝、Flash 選項、NN 模型載入錯誤 | 不把 context_ready 當作模型成功；檢查 .ino modelSelect 沒被移走、上傳包含 YOLOv7 |
| 每隔幾秒 NN DATA TIMEOUT | callbacks 是否停住、FPS、模型／來源是否正確 | 1.5 秒無新回呼的真實警告；先找停頓原因，不直接增大 timeout 掩蓋 |
| 計數因拔線歸零 | 開發板有沒有斷電 | 斷電／RESET 清 RAM 是正常；只有不斷電的 OTG 重連保留計數 |
| UART 輸出雜亂、太多日誌 | Show NN Logs、LOG／SAMPLES 設定 | NN Logs Disable，LOG OFF、SAMPLES OFF；STATUS 手動讀即可 |

## 框線與狀態

黃色 HOLD 表示最新推論沒有合格盆栽。保留框不會提高原始命中率；看起來穩定但 SAMPLES 命中少，仍是模型／場景問題。
持續漏判超過 600 ms，框線消失是預期；1 秒漏判容忍只影響確認，不延長框線。若模型約每秒 5 次，600 ms 只涵蓋少數回呼間隔，不能保證所有長漏判都有框。

已確認時首次負結果起 1 秒內維持綠燈；超過轉 CHECKING、藍慢閃；連續 5 秒負結果才藍常亮、Alert+1。
如果中間出現一次真實正結果，負結果計時會重新開始；請用 SAMPLES 判斷，不能只靠肉眼認為目標已經移走。
HOLD 期顯示的框分數是上次分數，狀態列與 UART score 是最新回呼分數，兩者刻意分開。

若框永久停住：記錄 SAMPLE 最後時間、DETECTION.last_valid_ms、box 狀態與主機影像是否也凍結。
軟體會在下一個 media 更新清除到期座標；若整個 Windows 預覽停在舊影格，消失的框也無法在停住的舊影格上呈現，此時應排查串流而非只增減 HOLD 時間。

## 記憶體

F 不呼叫 Video::videostream_status；其專案內 ready 檢查使用原本 context，沒有 cameraGetCtx 的 malloc 遺失。
A0/A1 忠實保留該官方程式，不能用它們做長時間無洩漏宣稱。
每分鐘記錄 MEM，暖機後比較 free_heap：偶爾變動與 min_free_heap 下降不等於已證明洩漏；反覆重連後每次都少一段且不回復則值得調查。
此版沒有驗證 SDK／Windows 驅動的所有配置生命週期，若持續下降請保留起始／穩態數字、重連次數、完整日誌。

不要為此先擴大影像 queue、增設 JPEG 副本或 1080p AI 緩衝。程式只保留最多 16 個座標、8 個座標快照與固定歷史，模型與影像資源由原官方管線管理。

## 編譯或模型封裝失敗

STATUS 的 USB.error：0=無已回報錯誤、1=UVCD context、2=Video context、3=NN context、4=USB StreamIO、5=NN StreamIO、6=結果 queue 配置、7=影像 task 建立失敗。
NN begin 是 void，context 成功仍需回呼證明有推論；底層自行印出的錯誤也要保留。

1. 確認使用 4.0.9 正式套件，並保留相同 sketch 資料夾中全部檔案；不要升到 4.1.1 預發布版。
2. 若顯示標頭不存在，確認 AmebaNN、AmebaMultimedia、AmebaUSB 是 4.0.9 隨附函式庫，避免 sketchbook 的舊副本優先被選中。
3. modelSelect 及 configureSmartTreeCamera 的真實 configVideoChannel 呼叫必須留在 .ino，供官方封裝器辨識模型／VOE。
4. `SMARTTREE_READY 1` 為相容舊指令的固定回覆，版本另列 SMARTTREE_VERSION／STATUS.version=2.0；不要以 READY 中的 1 判斷燒錯版本。
5. Linux Arduino CLI 自訂 data 目錄時，4.0.9 官方 ino_validation 仍會找預設 `~/.arduino15/packages/realtek`，這是建置工具限制，不是程式缺標頭；本次驗證用本機目錄連結保持路徑相容，沒有修改套件程式或 HOME。
6. 自訂 CLI 環境需裝齊兩段 toolchain、tools 1.4.5、models 1.0.2 與 builtin ctags；標準 Arduino IDE 正常 Boards Manager 安裝通常會處理。
7. 建置多個階段請逐一執行，官方 prebuild／模型封裝會共用與修改套件 variants／工具目錄，不宜多個編譯同時跑。

若更動其他套件設定造成回歸，先備份個人 UVCD_pram 設定，再重裝 4.0.9 並只恢復先前已確認的 MJPEG-only 設定；這是回復既有環境，不是本版要求新增的修正。
