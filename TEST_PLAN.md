# 實機測試計畫（所有硬體項目：待實機驗證）

先記錄：日期、板子／感光元件、IDE 版本、套件 4.0.9、程式版本／階段、Camera Options、模型來源 Flash、UVCD 描述元設定、Windows 版本、USB 線／接口、相機程式設定。
每次只改一個參數；每次重新燒錄後 RESET。先用之前相同的圖片、距離、光線與 Windows 相機選項。

## 建議燒錄順序

| 順序 | 程式 | 本輪判斷點 | 失敗時保留資料 |
|---|---|---|---|
| 1 | A0_OfficialYOLOv7 | 重現使用者已確認順暢的原官方 NN 範例＋YOLOv7 | 起始／協商日誌、相機設定；僅短期測試，不做長時間洩漏驗收 |
| 2 | A1_OfficialUSBPreset | 只改 USB preset，確認這一行是否影響板子上的相容性 | 若 A0 好而 A1 差，先固定 DISPLAY_PROFILE=1 再回測後續，別同時改模型／來源 |
| 3 | B_PlantOnly | 只顯示類別 58、分數≥50；不再用有配置洩漏的狀態輪詢 | STATUS、原始 SAMPLE，確認其他物件不顯示 |
| 4 | C_HoldAndPresence | 加入短漏判容忍與 600 ms HOLD，還沒有 LED／完整疊字 | 同一段畫面中原始命中、HOLD 與存在狀態的差異 |
| 5 | D_LedsEventsOSD | LED、事件、完整短文字；OSD OFF／BOXES／FULL 相同設定對照 | 是否只有 FULL 發生卡頓，保存 MEM 與重連事件 |
| 6 | E_IndependentUART | 初始化移到單一影像工作；只接 UART 仍可回應 | WAIT_HOST_STREAM 時的 PING／STATUS |
| 7 | 根目錄 SmartTree（F） | 依下列完整驗收，與 E 功能相同 | 完整測試表與異常前後日誌 |

A0/A1 有官方每輪 printf 與原始 cameraGetCtx 洩漏，僅作短時間基準比較；長時間測試請用 F。
B/C/D 的初始 nnbegin 在 setup，沒有 OTG 時阻塞是刻意保留的階段差異；UART 獨立只驗收 E/F。
若只想先燒一個日常版本，可以直接使用根目錄 F，出現問題再回退這個順序。

## F 的驗收項目

### 1. 只接 UART（先做，最容易確認）

1. 使用 UART／燒錄接口供電，OTG 不接，RESET。
2. 115200、行尾新行，輸入 PING，預期 `SMARTTREE_READY 1`。
3. 輸入 STATUS，預期原 STATUS 行為 USB_OFFLINE；USB.phase 依進度為 CONFIGURING 或 WAIT_HOST_STREAM，host_stream=0。
4. 以 1 秒間隔連續輸入 10 次 PING／STATUS，確認等待 nnbegin 不影響指令。
5. HISTORY 應能結束，輸入超過 47 字元後換行，應 ERR,LINE_TOO_LONG，之後 PING 仍正常。
6. 不可用 READY 字串判斷模型成功；檢查 context_ready、callback_seen，若未載入模型保存 SDK 錯誤訊息。

### 2. 開啟 Windows 相機並連續串流 10 分鐘

1. 使用 OTG 開啟 Windows「相機」，保持其他相機應用關閉。
2. 首次協商允許數秒等待，開始後不應長時間黑畫面或凍結。記錄是否超過 1 秒不更新，以及次數、時間點。
3. 若有 UART 同步紀錄，STATUS 預期 phase=RUNNING、host_stream=1、format=2（你原有 MJPEG-only 配置），callback_seen=1，FPS 是實測值，不要求等於來源 10。
4. stream_ready 瞬間 0 可以出現，但不應因此反覆產生 USB_DISCONNECTED／CONNECTED 事件或重啟 NN。
5. 第 1、2、5、10 分鐘各記錄 MEM.free_heap、min_free_heap、queue_drops、FPS 與主機影像觀察。
6. 初始暖機的記憶體降低不是洩漏證據；暖機後若 free_heap 隨時間持續單向下降，判為未通過，保存數據。min_free_heap 是歷史最低值，不會回升。

### 3. 固定盆栽圖片 2 分鐘

1. 固定相機／螢幕距離，例如自行量測並記錄 40 cm；不要求所有設備都用相同距離，之後每輪維持同一值。
2. 螢幕亮度、顯示縮放、圖片大小、相機角度與室內照明固定，畫面包含完整盆器與主要葉片。
3. 如果有逐輪量測條件，輸入 LOG OFF、STATS RESET、SAMPLES ON，連續記錄 120 秒，結束 SAMPLES OFF、STATS。
4. 預期第一次確認至少需要 2 秒真實正結果時間、至少 3 輪。中間有漏判時可以較久，不能承諾上畫面後恰好 2 秒。
5. 有短漏判時：SAMPLE.plant_count=0；畫面黃框 HOLD；HOLD 的 score 是上次有效框分數；當前狀態列 Score 仍顯示本輪結果。
6. 最後有效框時間滿 600 ms 後邏輯移除；OSD 下一輪更新清除。若負結果超過 1 秒，已確認轉 CHECKING，除非前面已進入 5 秒未確認狀態。
7. 事件確認不因每輪推論重複增加；短暫漏判恢復不能憑空增加確認事件。

### 4. 移除目標後觀察 30 秒

1. 從已確認狀態移走盆栽／切換為無盆栽圖片；記錄「第一輪負結果」時間。
2. 最後有效框約 600 ms 後消失，不應保留整個 5 秒，更不能永久保留。
3. 從第一輪連續負結果起，約 1 秒後 CHECKING、藍燈慢閃；滿原有 5 秒仍是負結果時 NOT CONFIRMED、藍燈常亮，Alert 增加 1。
4. 繼續 30 秒不含盆栽，Alert 不應每秒增加。若期間有真實誤判正結果，連續負結果計時重啟，保存 SAMPLE 以區分邏輯與模型問題。
5. 蓋住鏡頭也可能產生影像中斷，必須區分正常負結果和 NN DATA TIMEOUT；逾時不能增加「無盆栽」提醒。

### 5. 反覆移入／移出

至少做 5 輪：目標進入至確認 → 移出至少 6 秒 → 再移入至確認。每輪確認／提醒各增加一次，依最初狀態決定總數。
另做 5 次小於 1 秒的遮擋，檢查已確認燈號容忍；再做 1～5 秒的移出，應 CHECKING，恢復確認時不新增提醒或確認事件。
資料 queue 遺失或 NN 逾時會打斷確認，不能把這類試次當成一般短漏判容忍測試。

### 6. 關閉／重新開啟 Windows 相機

做 5 次：關閉相機 3 秒 → 開啟 → 等待回復。預期 UART 不受影響，USB 狀態轉離線再恢復；計數保留，但重新累積確認時間。
此為同格式／fps 測試；先不要改 Windows 解析度。若 host_stream=1 但畫面不回來，保存 STATUS 再按排錯表測一次 USB RETRY。
USB RETRY 後需重新開啟相機來產生新協商；等待期間 PING 必須仍可回應。

### 7. OTG 拔除／恢復

若 UART 或獨立電源仍供電，拔除 OTG 5 秒，期間 PING／STATUS 仍可用；插回 OTG，重新開啟 Windows 相機，觀察是否自動回復。
連續做 5 次並觀察 MEM；確認／提醒計數不可因單純重連歸零，新的確認依原規則新增。
若只有一條供電線，拔除將造成開發板重啟，這是「重新開機」測試，不能用來驗收「不斷電 USB 恢復」或保留 RAM 計數。

### 8. 疊字負載對照

F 在同一主機格式、模型與光線下：OSD OFF 2 分鐘 → OSD BOXES 2 分鐘 → OSD FULL 2 分鐘。
每組重設量測基準，不改解析度／來源 FPS／模型門檻，記錄卡頓、FPS、記憶體、queue_drops。
OSD OFF 只是清除既有畫布，OSD 工作仍已初始化；若要隔離 OSD 初始化本身，回退 A0/A1 或另行實測，不把這個開關當成完整停用 OSD driver。

## 準確度與框線可見度分開

| 指標 | 計算／限制 |
|---|---|
| 有盆栽時原始命中率 | STATS.raw_hits ÷ raw_samples；標籤必須是固定有盆栽 |
| 無盆栽時假陽性率 | 同一比值，但標籤固定無盆栽；不能稱為命中率 |
| 原始逐輪紀錄 | SAMPLE 每輪一筆；sequence 不連續或 queue_drops>0 表示逐輪日誌不完整 |
| 框線預期可見比例 | visible_ms ÷ display_ms，包含 HOLD，只是軟體排程估計；不能替代 Windows 錄影 |
| HOLD 占比 | hold_ms ÷ display_ms，反映呈現補償，不是模型真實成功 |
| 確認／提醒事件 | 行為事件，不是準確率分母 |

STATS 的 raw_samples／raw_hits 在 callback 直接統計，即使 queue 丟失也不會把 HOLD 加成命中；有遺失時仍可用這兩個總數看該段比率，但逐輪時間與確認行為需標記資料不完整。
STATS 僅計 host_stream active 的回呼；關閉相機的時間不計原始分母。display_ms 僅計啟用 OSD 且主機啟動的時間，其他畫面 freeze 仍須錄影觀察。

建議測三個門檻：**先 50，再 60，必要時 40**。每次只改 CONFIDENCE_PERCENT，重新燒錄，同一盆栽圖片 120 秒 + 空白／花瓶／杯子／葉片背景等負控制 120 秒，各至少三輪。
模型端與應用端門檻同步；不動顯示設定、AI 來源、NMS 或模型。低於 0.50 的候選在預設模型閘門已被濾除，不能從原來的 50 門檻日誌推算 40 的表現。
分數常靠近 50、原始命中增加但空場景誤判也增加，都必須如實記錄；沒有實測結果前不建議降低門檻。

反光、曝光過度、螢幕摩爾紋、縮放插值、盆器被切掉、植物太小均可列為待測因素。每次只調整一項，模型可能辨識圖片而非真實植物，請在報告標明「螢幕圖片測試」。

## 可選 PC 收集工具

需要同時有 UART 紀錄條件，且 Windows 相機已顯示、暖機完成，先關閉占用 COM 的 Arduino 序列監控：

```powershell
python -m pip install pyserial
python tools/capture_serial.py --port COM5 --seconds 120 --label present --output measurements/present_50.csv
python tools/capture_serial.py --port COM5 --seconds 120 --label absent --output measurements/absent_50.csv
```

輸出 CSV、完整 .log、摘要 .json。工具不控制 USB 相機、不修改模型、不把空日誌算成功；檢查 .log 的 STATS queue_drops 與樣本總數。
切換標籤時要結束上一輪並更換檔名；不要把移動目標的過渡時間混入固定場景率。

## 記錄

填寫 `test_record.csv`。任何未測項寫「待實機驗證」，不要填 PASS。若失敗，先保存原始日誌、STATUS、MEM 與 Windows 畫面，再更改一個變因。
