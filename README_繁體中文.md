# SmartTree W2 v2.0

正式更新版：[PikaO3O/SmartTree_v2.0](https://github.com/PikaO3O/SmartTree_v2.0)。[8 頁成果報告](docs/Ameba影像辨識創意實作_更新報告_v2.0_20261104.pdf) 內含材料、接線、重現方式、原 40 筆實測與最新版驗證範圍。

AMB82-mini（RTL8735B）、Realtek Ameba Boards **4.0.9**、Windows 11 USB UVC、官方 YOLOv7-tiny。
專案依 4.0.9 的 `UVCDObjectDetectionLoop` 重新整合，沒有沿用 v1.2 的顯示格式實驗作為基底。

**交付範圍：完整原始碼、可回溯版本、主機邏輯測試及實際板套件編譯紀錄。Windows 相機、NPU、LED、USB 拔插與長時間記憶體表現均待實機驗證。**
請以 `VALIDATION.md` 與 `validation/` 的紀錄確認檢查範圍，編譯通過不代表實機通過。

## 先使用這個程式

1. 將整份 ZIP 解壓縮到簡短英文路徑，例如 `C:\IoT\SmartTree_W2_v2.0`。
2. 開啟根目錄 `SmartTree/SmartTree.ino`；同資料夾的 `.cpp`、`.h` 必須全部保留。
3. Arduino IDE 選擇 **Realtek Ameba Boards 4.0.9 → AMB82-MINI**。
4. Camera Options 沿用你先前官方範例成功使用的感光元件選項；此交付的 Linux 編譯使用預設 JXF37。
5. NN Model Load From 選 **Flash**、Show NN Logs 選 **Disable**。其他燒錄選項沿用已成功設定。
6. 使用燒錄／UART 接口驗證、上傳，再按 RESET。確認模型上傳／封裝包含 `yolov7_tiny`。
7. 接上 OTG，開啟 Windows「相機」。首次協商可能等待數秒；不要在這段時間反覆重新插拔。
8. 對準原先可辨識的同一張盆栽圖片，先觀察綠色 `LIVE` 框與黃色 `HOLD` 框，再依 `TEST_PLAN.md` 測試。

不必為本版修改 Arduino 套件。你已設定的 MJPEG 單一格式（YUY2=0、NV12=0、MJPG=1、H264=0、H265=0）直接保留，這不是本版新提出的修正。
官方發行檔中的參考 `UVCD_pram.h` 可能是多格式全開；`references/` 保留其原貌，**不要拿來覆蓋你目前的設定**。

## 只有一條 USB 線

| 接法 | 可以做什麼 | 狀態 |
|---|---|---|
| 只接 UART／燒錄接口 | 上傳、115200 序列指令、PING、STATUS、HISTORY | E／F 不等待 OTG 才接收指令；WAIT_HOST_STREAM 是正常待機 |
| 只接 OTG | 供電與 Windows 相機影像、OSD、板上 LED | 不會因此產生可用的 UART COM 埠 |
| UART 與 OTG 同時接 | 邊觀看邊收集逐次模型紀錄 | 是完整同步量測方式，並非日常使用的必要條件 |

只有一條線時，可先做 OTG 畫面／LED 測試，再換接 UART 測試指令。換線如果讓開發板斷電，RAM 歷史與計數會歸零；本版沒有跨重開機儲存。
若要量測同一段串流的逐次原始結果，需要能同時收集 UART 的接線條件，或另外記錄實機畫面；不能用框線保留推算原始命中率。

## 保留的原有規格與新增參數

設定集中在 `SmartTree/SmartTreeConfig.h`。一次只改一個參數、重新編譯與燒錄。

| 設定 | 預設 | 行為 |
|---|---:|---|
| CONFIDENCE_PERCENT | 50 | 模型信心門檻 0.50，應用接受分數 50；兩處同步設定 |
| NN_WIDTH / NN_HEIGHT | 576 / 320 | RGB 通道 3，維持原 AI 來源大小 |
| NN_SOURCE_FPS | 10 | 來源設定；4.0.9 通道 3 內部使用 CAM_NN_FPS=10；不是保證推論達 10 FPS |
| CONFIRM_MS | 2000 ms | v1.1 的 2 秒確認；仍至少 3 次真正的新推論 |
| MISSING_MS | 5000 ms | v1.1 的 5 秒連續負結果提醒，沒有覆寫原規格 |
| NN_FRESH_MS | 1500 ms | v1.1 的推論資料逾時；無回呼不算「沒有盆栽」 |
| BOX_HOLD_MS | 600 ms | 上次有效位置保留期限，不增加模型命中或確認事件 |
| MISS_GRACE_MS | 1000 ms | 首次負結果起的短暫漏判容忍；設 0 可回復 v1.1 的立即中斷規則 |
| SHOW_BIRD | false | 預設只畫盆栽；改 true 可恢復 v1.1 鳥類即時框，不影響盆栽判斷 |
| SMARTTREE_DISPLAY_PROFILE | 0 | 0：官方 USB preset；1：官方 NN 範例 FHD/24/H264 啟動設定 |

顯示設定採 `USB_UVCD_STREAM_PRESET`：4.0.9 的啟動參數是 FHD／20fps／H264／1Mbps、專用 `cameraOpenUVCD` 路徑。**這是協商前設定，不是強迫 Windows 使用 H264**；實際 UVC 格式由主機與你現有的 MJPEG 描述元協商。
STATUS 的 USB 行顯示協商結果。切勿把啟動設定改成 JPEG 就視為已完成 UVC 協商。

模型沿用官方內建 YOLOv7-tiny；依你提供的設備條件，其模型輸入為 416×416。程式沒有修改模型形狀，也沒有建立 1080p AI 輸入。
本版不新增影像副本、影像儲存或高解析度模式；現階段不提供未驗證的 720p／1080p 熱切換。

## 狀態、框線與燈號

「最近一輪原始結果」、「畫面框線」、「存在確認」是三個不同欄位，可同時成立，例如 **raw=0、Box=HOLD、PLANT_CONFIRMED**。

| 名稱 | 定義／原 v1.1 對照 |
|---|---|
| DETECTED／LIVE 綠框 | 最近一輪新回呼有合格盆栽，使用這輪位置；不代表重新訓練或準確率提升 |
| HOLD 黃框 | 最新回呼漏判，沿用上次有效位置及原信心分數；不餵回確認邏輯 |
| NONE | 無可顯示的有效框，或上次有效位置已滿 600 ms |
| CHECKING | v1.1 CHECKING；確認證據不足、或漏判已超過容忍但尚未達 5 秒 |
| CONFIRMED | 內部狀態；畫面 PLANT CONFIRMED、UART PLANT_CONFIRMED，綠燈恆亮 |
| NOT_CONFIRMED | 內部 UNCONFIRMED；畫面 NOT CONFIRMED、UART UNCONFIRMED_CHECK_VIEW，藍燈恆亮 |
| NN_TIMEOUT | 畫面 NN DATA TIMEOUT、UART INFERENCE_TIMEOUT，藍燈快閃（150 ms 半週期） |
| OFFLINE | UART USB_OFFLINE；相機未開啟／已關閉／OTG 中斷，兩燈暗 |

CHECKING 藍燈慢閃（500 ms 半週期），LED_B／LED_G 維持已測過的 HIGH 亮、LOW 暗。
LED 僅顯示存在確認，沒有缺水、健康或澆灌意義。
沿用原規格：確認整張影像內「任一合格盆栽」，不追蹤特定盆栽 ID；保留框以最近一整輪合格盆栽為一組，不是多目標追蹤器。

確認開始前若短暫漏判，保留既有候選，但**暫停累積正結果時間**；只有相鄰兩次皆為真實正結果的間隔才累積，仍需滿 2000 ms、至少 3 次真實正結果。
已確認後，1000 ms 內短暫負結果維持已確認；超時顯示 CHECKING；從第一個連續負結果計滿原有 5000 ms 才記一次提醒。
提醒之前恢復確認不新增確認事件；真正提醒後恢復才新增事件。斷線、推論逾時、結果遺失仍重新累積確認。

框線期限使用 millis()。邏輯於 600 ms 失效，畫面最晚於下一次約 100 ms 的 OSD 更新移除；底層排程與 Windows 顯示延遲待實測。
如果沒有新回呼，最後一輪結果的框也會到期；最近一輪正結果在到期前標為 LIVE，不能把它解讀成每個影格都有新推論。

## UART 指令（115200，行尾「新行」）

| 指令 | 用途 |
|---|---|
| PING | 相容回覆 `SMARTTREE_READY 1`；表示指令端可用，不表示 USB／模型已成功 |
| STATUS | 原 7 欄 STATUS + USB、NN、DETECTION、MEM 診斷行 |
| HISTORY | 最近 32 筆事件；RAM 環形紀錄、分批輸出、重開機清空 |
| LOG ON / LOG OFF | 開／關事件即時輸出與每秒狀態；歷史仍記錄。預設僅事件輸出、無每秒狀態 |
| SAMPLES ON / SAMPLES OFF | 開／關每輪真實結果 CSV；預設關閉 |
| STATS RESET | 重新設定量測基準；不清除確認／警告事件 |
| STATS | 真實回呼總數與命中、queue 遺失、分數分布、軟體預期框線可見時間 |
| OSD OFF / OSD BOXES / OSD FULL | 關閉疊圖／僅框線／完整短文字，供相同串流設定下對照 |
| USB RETRY | 完成初始啟動後，要求影像工作再呼叫一次官方 nnbegin；接著關閉並重新開啟 Windows 相機以產生協商訊號 |
| HELP | 指令清單 |

STATUS 保留 `STATUS,time_ms,state,score,plant_count,confirm_events,alert_events`。
USB 的 `host_stream` 是 SDK UVC state，`stream_ready` 是主機串流狀態加回收佇列的即時觀察，**不是 Windows 已收到影格的證明**。瞬間 stream_ready=0 不觸發重啟。
`pipeline_active` 是目前是否接受此串流 session 的真實樣本，`osd_ready` 表示 OSD.begin 已返回；等待協商時其他協商欄位可能是上次觀測值。
`format=2` 是 MJPEG；0=YUY2、1=NV12、3=H264、4=H265。
NN 的 context_ready 僅表示建立模組；callback_seen 表示真正收到過模型回呼，fresh 表示最近 1.5 秒有回呼；FPS 由真實回呼計算。

SAMPLE 格式：`SAMPLE,sequence,time_ms,plant_count,score,box,state,epoch`，只對新結果輸出。
分數是官方 `score()` 截斷後的 0～100 整數；0 代表本輪回傳結果沒有盆栽分數，不能推論模型的未過濾分數就是 0。
SCORE_BINS 為 0–9、10–19…80–89、90–100 十格；只包含通過模型門檻的該輪最佳盆栽分數。

## 架構與恢復限制

F／E：Arduino loop 負責 UART、狀態機、LED、歷史與統計；唯一影像工作負責攝影機、UVCD、通道啟停與 OSD。NN 回呼只整理固定大小座標、統計、非阻塞 queue 發送。
跨工作資料用短 critical section 複製；通道不由 UART／回呼操作；沒有持鎖呼叫 nnbegin 或 OSD。
USB 未開時 nnbegin 可等待訊號，Arduino loop 不跟著等待。失敗狀態保留指令端，不進入忙碌重試。

相機關閉／OTG 中斷時，沿官方 one-shot disconnectNN／reconnectNN 啟停順序處理；同時保留啟動前的獨立 UART。
預設不反覆重新建立 USB 或配置影像記憶體；USB RETRY 是人工觸發的一次協商等待，不是保證可從所有驅動狀態恢復。
第一次 nnbegin 尚未返回時，USB RETRY 會回覆不適用，請先開啟相機；若卡在底層驅動，依排錯表保存資訊後 RESET。

## 檔案與回溯版本

| 位置 | 用途 |
|---|---|
| SmartTree/ | 最終 F 整合版，日常使用請開這個 |
| stages/A0_OfficialYOLOv7/SmartTree/ | 原官方 NN 範例，僅換 YOLOv7-tiny；有原始高頻日誌與已知輪詢洩漏，僅短期對照 |
| stages/A1_OfficialUSBPreset/SmartTree/ | A0 僅改官方 USB preset，隔離顯示啟動配置差異 |
| stages/B_PlantOnly/SmartTree/ | 盆栽篩選、回呼 queue、無配置記憶體的連線檢查，無 HOLD／確認功能 |
| stages/C_HoldAndPresence/SmartTree/ | 加入 600 ms HOLD、1000 ms 容忍與原 2／5 秒存在狀態；只畫框線 |
| stages/D_LedsEventsOSD/SmartTree/ | 加入原 LED、事件輸出、完整疊字；初始等待仍在 setup |
| stages/E_IndependentUART/SmartTree/ | 將同一影像流程移至獨立工作，UART 不等待 OTG |
| stages/F_Integrated/SmartTree/ | 最終整合版回溯副本；與根目錄功能相同 |
| references/official_4.0.9/ | 實際檢查的官方原始碼快照與 SHA256，僅供閱讀 |
| tests/、tools/、validation/ | 可執行檢查、編譯與收集工具、結果 |

A0／A1／B／C／D 都是定位差異的診斷版本，**僅 UART 的驗收應使用 E 或 F**。
詳細原因、修改前後行為與未完成的硬體驗收，見 `ENGINEERING_ANALYSIS.md`、`CHANGELOG.md`、`TEST_PLAN.md`、`TROUBLESHOOTING.md`、`SOURCE_REFERENCES.md`。
