# SmartTree：第一週植栽影像監測原型 v1.1

本專案由使用者提供、已可執行的 SmartTree.ino 延伸。使用 AMB82-MINI 內建
YOLOv7-tiny，免自行訓練模型、免 Wi-Fi。辨識在開發板執行；Windows 11 相機
僅顯示／錄影。新增板端持續確認、未確認提醒、LED 提示與事件紀錄。

## 開啟與上傳

1. 將 ZIP 解壓縮到本機，例如 C:\IoT\SmartTree_W1。
2. 開啟內層 SmartTree 資料夾的 SmartTree.ino；四個 .h 檔須留在同一資料夾。
3. 沿用目前已成功的 Arduino IDE、AMB82-MINI、攝影機及 NN 模型載入設定。
4. 使用燒錄接口／COM 埠，先按「驗證」。成功後上傳，再按 RESET。
5. 保留板子供電，將 USB OTG 接到電腦，開啟 Windows「相機」。
6. 如需紀錄，另外保留 USB UART 連線；序列監控視窗設為 115200、新行。
   OTG 是影像接口，UART 是 COM／序列埠接口，不能互相替代。

使用者已回報 v1.0 能執行，並提供真盆栽辨識、PLANT_CONFIRMED 及事件計數畫面。
v1.1 修正畫面文字截斷，仍須重新編譯、上傳及確認顯示。交付端沒有 Ameba
Arduino 套件及開發板，主機 C++ 邏輯測試不能當作完整板端驗收。

## 狀態與燈號

| 畫面文字 | 中文意思 | 板上燈號 |
|---|---|---|
| CHECKING | 確認中：時間不足或短暫漏判 | 藍燈慢閃、綠燈暗 |
| PLANT CONFIRMED | 已連續確認到盆栽 | 綠燈亮、藍燈暗 |
| NOT CONFIRMED | 未確認，請檢查影像 | 藍燈亮、綠燈暗 |
| NN DATA TIMEOUT | 超過 1.5 秒無新推論結果 | 藍燈快閃、綠燈暗 |
| USB OFFLINE | OTG 觀看連線未啟用／中斷 | 兩燈暗；畫面無法更新 |

LED 使用先前已實測成功的 LED_B／LED_G，HIGH 亮、LOW 暗。
OSD 沿用官方英文文字字型。文字是監測狀態，白框與青色物件標籤沒有植物健康含義。

v1.1 將狀態、Plants（合格盆栽數）、Score（當輪盆栽最佳分數）、Confirm（確認事件數）、
Alert（提醒事件數）各自分行；最後一行顯示設定時間或 Check camera view。
避免長行被截斷，畫面狀態不用底線和豎線；序列埠仍使用原本完整名稱。

## 判斷規則

- 盆栽類別固定為 58，分數至少 50；模型預設閾值仍為 0.5。
- 新推論中至少有一個合格盆栽，連續 2 秒且至少 3 次新結果，才顯示已確認。
- 任何一次新結果沒有合格盆栽，都會打斷上述連續確認。
- 持續 5 秒的新結果都沒有合格盆栽，記錄一次「未確認」提醒。
- 若已確認後只是短暫漏判，顯示確認中並重新累積 2 秒；恢復不重複計為新事件。
- 真正進入未確認狀態後重新確認，才新增一次確認／恢復事件。
- 斷線、推論逾時或 queue 溢位造成的資料缺口，都會重新累積確認時間。
- 資料來源使用官方 callback，每輪新結果（含零目標）更新一次，避免重讀舊結果。
- 實際更新間隔若常超過 1.5 秒，會先顯示逾時；須實測後再評估 NN_FRESH_MS。
- 目前對整張畫面內任一盆栽確認，未追蹤特定盆栽身分，未限定澆灌區域。
- ObjectClassList.h 保留使用者的「鳥 14／盆栽 58」篩選設定。鳥可顯示框，
  但不會影響盆栽確認；尚未新增鳥類來訪紀錄。

重要：未辨識到可能是遮擋、外觀、光線或模型漏判，不等於盆栽被移走、枯死或缺水。
螢幕圖片也可能被辨識為盆栽，模型不會驗證目標是真實植物。不得將本原型直接
作為真實澆灌的唯一依據。這版不含水泵、感測器、遠端通知或 SD 儲存。

## 事件紀錄

程式將最近 32 筆事件保存在開發板 RAM；重開機會清空，滿時覆蓋最舊事件。
序列埠即時輸出事件，並每秒輸出一次 STATUS。只有狀態事件進入歷史，
每秒 STATUS 不算重複提醒。保留紀錄需要將序列監控內容複製到文字檔。

在序列監控視窗輸入以下任一指令，行尾設「新行」：

- PING：回覆 SMARTTREE_READY 1。
- STATUS：回覆當下狀態。
- HISTORY：印出最近 32 筆事件，可複製保存。

EVENT 格式：EVENT,開機後毫秒,事件名稱,盆栽最佳分數,合格盆栽數量。
STATUS 格式：STATUS,開機後毫秒,狀態,最佳分數,合格盆栽數量,確認次數,提醒次數。
時間是開機後經過時間，不是日期時間。重新連線保留計數；重新開機會重設。

## 30～60 秒基本驗證影片建議

1. 前 5～10 秒：展示開發板、USB 接線與 Windows 相機畫面。
2. 接著 10 秒：鏡頭對準可辨識的盆栽／圖片，呈現 CHECKING → PLANT_CONFIRMED 與綠燈。
3. 接著 10 秒：移開目標或切換到不含盆栽的畫面，呈現 CHECKING → UNCONFIRMED 與藍燈。
4. 接著 10 秒：恢復目標，呈現重新確認。
5. 最後 5～10 秒：展示 HISTORY 或狀態計數，說明圖片／實物及未完成項目。

用圖片驗證時，影片與提案均標示「螢幕圖片測試」，不能當成實體盆栽成功。
狀態計數不是成功率；成功率需由人工判定的試次紀錄計算。

## 自訂設定

在 SmartTree.ino 頂部調整 MIN_PLANT_SCORE、CONFIRM_MS、MISSING_MS、NN_FRESH_MS。
目前先保留 50、2000、5000、1500 的初始設定；時間常數須與實測結果一同報告。
改文字若加長請檢查緩衝區與板端顯示；初始設定下每行均為短文字。
畫面時間提示會讀取目前 CONFIRM_MS／MISSING_MS，以整秒顯示。

## 來源與 AI 協作

- 使用者提供的 SmartTree.ino：已改為 DEFAULT_YOLOV7TINY。
- 使用者提供的 ObjectClassList.h：保留鳥與盆栽兩類，未改類別編號。
- Ameba 官方 UVCDObjectDetectionLoop：
  https://github.com/Ameba-AIoT/ameba-arduino-pro2/blob/main/Arduino_package/hardware/libraries/NeuralNetwork/examples/UVCDObjectDetectionLoop/UVCDObjectDetectionLoop.ino
- 官方 NNObjectDetection API（callback、類別與分數）：
  https://ameba-doc-arduino-sdk.readthedocs-hosted.com/en/latest/ameba_pro2/amb82-mini/API_Documents/NeuralNetwork/Class%20ObjectDetection.html
- 官方儲存庫及授權：
  https://github.com/Ameba-AIoT/ameba-arduino-pro2

本次 AI 協助：閱讀現有程式與官方 API，加入板端狀態計時、LED 提示、去重事件、
資料新鮮度與 USB 重連處理、緩衝區及類別邊界檢查，產生邏輯測試與操作說明。
使用者仍需完成 Arduino 編譯、實機測試、錄影與結果判讀。

## 主機邏輯測試

有 g++ 的環境可在專案根目錄執行：

```sh
g++ -std=c++11 -Wall -Wextra -Werror tests/test_monitor.cpp -o /tmp/smarttree-monitor-test
/tmp/smarttree-monitor-test
```

此測試檢查計時、重複事件、短暫漏判、恢復、斷線重連、資料缺口與 millis 回繞；
未驗證相機、NPU、FreeRTOS queue、OSD 或 LED 的實際硬體行為。
