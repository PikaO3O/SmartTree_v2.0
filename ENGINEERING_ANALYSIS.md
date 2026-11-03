# 原始功能盤點、差異與重構依據

## 已讀取的附件

完整讀取兩個 ZIP 內的 SmartTree.ino、PlantMonitor.h、DetectionSnapshot.h、ObjectClassList.h、原測試與說明。
比對結果：v1.2 的 PlantMonitor、快照、類別清單與主機測試均與 v1.1 相同；主程式主要新增 H264／MJPEG 啟動配置選擇及協商前後訊息，沒有解決阻塞初始化或連線狀態定義。

| v1.1 功能 | 原實作 | v2 對應／差異 |
|---|---|---|
| 模型／來源 | YOLOv7-tiny、576×320 RGB、10fps | 保留，configThreshold 明確設 0.50／NMS 0.30 |
| 盆栽篩選 | COCO 0-based 58、分數至少 50 | 保留，不使用 COCO 的非連續 category_id |
| 鳥類框 | 14 bird，只有顯示 | 預設僅盆栽；SHOW_BIRD=true 可恢復，不影響確認 |
| 確認 | 連續正結果 2 秒且至少 3 輪 | 原規格保留，新增可關閉的容忍；負結果期間不累積正證據時間 |
| 短漏判 | 立即 CHECKING，重新 2 秒 | 1 秒內保留已確認狀態／候選；超時仍 CHECKING，5 秒才提醒 |
| 未確認提醒 | 連續負結果 5 秒、一次事件 | 原門檻與去重規則保留 |
| 確認恢復事件 | 提醒後恢復才新增；斷線／逾時重新確認 | 保留；短暫漏判不新增事件 |
| LED | CHECKING 藍慢閃、CONFIRMED 綠、UNCONFIRMED 藍、逾時藍快閃、離線全暗 | 保留 HIGH 亮的原行為 |
| 事件歷史 | 最近 32 筆 RAM 環形紀錄 | 保留；HISTORY 分頁輸出，避免一次阻塞指令處理 |
| UART | PING／STATUS／HISTORY，32-byte 行 | 保留回覆與 STATUS 7 欄，新增診斷／開關；48-byte 行、每輪最多讀 64 bytes |
| OSD | 所有框與 6 行短文字，每 100 ms 重畫 | 單一影像 owner、單一原有畫布；僅新結果、狀態或模式改變才重畫 |
| 推論新鮮度 | 1500 ms；callback 包含零目標 | 保留；框線另有 600 ms 到期，無新回呼不累積負結果 |
| queue 中斷 | 丟失結果打斷確認 | 保留，另用序號／epoch 排除重連前資料 |
| USB 中斷 | 停 NN，再 start 顯示；恢復 start NN／顯示 | 依官方實作保留 one-shot 次序；不用瞬間佇列空當成斷線 |
| fatalError | setup／初始化錯誤後永久閃燈 | 改回報 ERROR、保留 UART；不進入高頻重試或重複配置 |

原 v1.1 沒有定時重複發送提醒：它每次「進入未確認」只計一次警告，持續未確認只更新狀態。本版沒有擅自加入週期性警告事件。

## 可從原始碼確定的問題

### 1. UART 被初始化等待擋住

4.0.9 `UVCD::nnbegin()` 在 `rtw_down_sema(&uvc_format_ptr->uvcd_change_sema)` 等待主機訊號，處理一輪後返回。
v1.1／v1.2 在 setup 呼叫，handleSerial 只在 loop 執行，故未開 OTG 串流時無法處理指令。這是可由程式路徑確認的阻塞關係。
`UVCD::begin()` 本身有永久 while 迴圈，服務多次協商，不能拿來直接取代 nnbegin。
F 將**整段原有影像初始化**移到一個 FreeRTOS 工作，不為等待而切換 API。

### 2. 4.0.9 狀態輪詢中的配置遺失

`cores/ambpro2/video_drv.c::cameraGetCtx()` 先執行 `rtw_malloc(sizeof(mm_context_t))`，隨即 `video_data=p`，配置地址被遺失且沒有 free。
v1.1／v1.2／官方範例每輪 `videostream_status()` 都走這條路徑；呼叫越多，該配置越可能持續累積。
F 使用 ModuleView 的合法 base-copy 存取已存在的 MMF context，再對相同 output_recycle 執行 `xQueuePeek(...,0)`；完全不配置新的 context。
沒有覆寫 SDK 符號、重定義其函式、修改套件或用強制轉型繞過 protected。ModuleView 只借用指標，不擁有資源。
**這證明一個洩漏風險，不證明 Windows 黑畫面的唯一根因；其他 SDK 配置仍須長時間量測。**

實際工具鏈產生的 cameraGetCtx object 反組譯仍保留 `rtw_malloc(72)` 呼叫，並非只在原始碼出現但被最佳化移除；證據另附 `validation/sdk_cameraGetCtx_disassembly.txt`。未包含 allocator 自己的額外管理成本。

### 3. 「已連線」也依賴瞬間佇列狀態

官方 `isUsbUvcConnected()` 需要 UVC state=1 且傳入的 recycle queue ready=1。
queue 暫空並不等同 OTG 實際拔除；若立刻呼叫 channelEnd／channelBegin，可能在影像活躍時反覆啟停。
F 同時輸出 host_stream 與 stream_ready，只在官方 struct 中的 **host state=0** 時走停 NN 流程；瞬間 ready=0 僅做診斷。
這項使用不同觀測值的修正有明確程式依據，改善程度與 USB state 在實機的變化仍待測。

### 4. USB preset 與一般顯示配置並非相同路徑

`VideoSetting(USB_UVCD_STREAM_PRESET)` 將 _preset 設 10；`Video::videoInit()` 因而使用 `cameraOpenUVCD()`，設定 USB 專用 queue length=1、gop／rc／static／meta 與 VOE heap。
v1.1 FHD/24/H264 與 v1.2 JPEG 配置屬一般建構式，沒有此 preset 分支。
官方 NN 範例在 4.0.9 本身也是一般 FHD/24/H264；官方 UVC_Device 才直接使用 USB preset。因此交付 A0、A1 兩個版本，僅改一行來對照，不把兩者混稱為完全相同設定。
最終版優先採使用者要求的 USB preset，並保留 DISPLAY_PROFILE=1 作已知 NN 啟動設定對照。實際 Windows 仍協商 MJPEG。

### 5. 韌體封裝器掃描 .ino

4.0.9 的 ino_validation／nn_json_modify 建置工具會掃描 .ino 的 modelSelect、configVideoChannel 與 NN include。
若把這些全部移入 .cpp，C++ 可以連結，封裝器卻可能沒有識別模型／VOE。
F 將真正執行的 modelSelect 與兩個 configVideoChannel 呼叫保留在 .ino 輔助函式，從影像工作呼叫，不使用假註解欺騙掃描器。
編譯紀錄另保留封裝器辨識資訊；請勿為了縮短 .ino 任意移走這些呼叫。

## 尚未被證明的高風險位置

| 位置 | 風險／證據界線 | 隔離方式 |
|---|---|---|
| 重複 channelBegin／channelEnd | channelBegin 真正啟動，官方 disconnectNN 註解卻寫 Stop；可能誤讀或反覆觸發 | 同一 owner，只在 host state 邊緣呼叫；記錄 USB_CONNECTED／DISCONNECTED |
| OSD 每 100 ms 完整重繪 | v1.1 多行狀態，可能增加 render 工作；沒有證據能直接定罪 | OSD OFF → BOXES → FULL，同一顯示、來源與門檻比較 |
| 相機格式改一般 JPEG | 不是 USB 描述元同步／協商的同義詞，還可能走一般 queue 配置 | A0/A1 對照，最終不以 v1.2 profile 為基底 |
| 推論 getResult 多次讀取 | 官方 loop 可能重讀上一輪；不能據此當新樣本 | 只使用每輪 callback 的完整 value snapshot，含零目標 |
| 高頻日誌 | 官方範例每輪印全部物件；與 UART 傳輸可能互相等待 | F 預設只印稀疏事件，逐輪記錄需明確開啟 |
| 記憶體餘裕 | 過去設備記錄剩約 2.5 MB，SDK 模型／影像用量不可憑空推算 | 不新增 raster buffer；MEM 每分鐘採樣，另測拔插後是否回到穩態 |

## 工作所有權與同步

| owner | 獨有操作 | 跨工作傳遞 |
|---|---|---|
| mediaTask | Video／UVCD／StreamIO、nnbegin、通道啟停、OSD | 發布 MediaStatus、讀 RenderView、讀一次性 retry 序號 |
| NN callback | 遍歷本輪 results、真實樣本統計 | 固定 16 框、8 深度 queue，timeout=0；失敗記 drops，下次標 gap |
| Arduino loop | PlantMonitor、BoxHold、LED、UART、32 筆事件、量測 | 讀 MediaStatus／CallbackStats、發布 RenderView |

結構複製時短暫關閉排程／中斷，沒有在 critical section 裡呼叫相機、OSD、Serial、等待或配置記憶體。SDK 自己的 vector 與影像工作仍由其函式庫管理，不能聲稱底層完全零配置。
session epoch 在主機串流啟停時改變；不同 epoch 或過期樣本不能累積確認。狀態機與 LED 不由多個工作同時修改。

新增影像工作固定 8192 個 StackType_t（ARM 32-bit 下 32 KiB）；queue 只存座標與整數。靜態快照、歷史與保留框也有固定上限，未建立新影像通道或影格副本。
0.6 秒 hold 的耗時與 1 秒 grace、1.5 秒 timeout、2 秒確認、5 秒未確認各自獨立。

## 恢復邊界

相同格式／影格率的關閉再開啟，預設沿官方啟停流程；不假設所有 Windows 驅動重啟都可自行恢復。
若主機重新協商不同格式／fps，nnbegin 原設計只處理第一次訊號，不會因為 loop 持續呼叫 isUsbUvcConnected 就自動重新協商。
因此提供一次性的 USB RETRY；它不做重複 malloc、不重建 USB descriptor。等待新協商時 UART 繼續可用，但影像不保證當下恢復。
USB RETRY／RESET 實際成功率、OSD 效果、硬體 watchdog、長時間無洩漏均列在待實機驗證，不做成功宣稱。
