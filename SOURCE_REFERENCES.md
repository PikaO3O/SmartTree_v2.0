# 來源、版本與 API 核對

查核日期：2026-11-03（Asia/Taipei）。原始碼以 **V4.0.9** 與官方發行 archive 為準，未採 dev／4.1.1 預發布版。

## 官方版本

- [官方儲存庫](https://github.com/Ameba-AIoT/ameba-arduino-pro2)
- [V4.0.9 release](https://github.com/Ameba-AIoT/ameba-arduino-pro2/releases/tag/V4.0.9)
- Tag commit：`520df242a17a34bb2dc4f2359c9a165605136e81`。
- [固定版本 NN 範例](https://github.com/Ameba-AIoT/ameba-arduino-pro2/blob/V4.0.9/Arduino_package/hardware/libraries/NeuralNetwork/examples/UVCDObjectDetectionLoop/UVCDObjectDetectionLoop.ino)
- [固定版本 UVC_Device](https://github.com/Ameba-AIoT/ameba-arduino-pro2/blob/V4.0.9/Arduino_package/hardware/libraries/USB/examples/UVC_Device/UVC_Device.ino)
- [4.0.9 當時的 Boards Manager index](https://raw.githubusercontent.com/Ameba-AIoT/ameba-arduino-pro2/V4.0.9/Arduino_package/package_realtek_amebapro2_index.json)
- [官方 4.0.9 archive](https://github.com/Ameba-AIoT/ameba-arduino-pro2/raw/main/Arduino_package/release/ameba_pro2-4.0.9-build20250805.tar.gz)，72,476,758 bytes，SHA256：`8bd1d0694e8d9d9ac729b9300eeef8fb6d2d681fd4905c808fe57c70006525b8`。

官方線上說明可能隨 latest 更新，API 行為以本次實際讀取的固定版本檔案為準：

- [UVCD 物件偵測說明](https://ameba-doc-arduino-sdk.readthedocs-hosted.com/en/latest/ameba_pro2/amb82-mini/Example_Guides/Neural%20Network/UVCD%20Object%20Detection.html)
- [ObjectDetection API](https://ameba-doc-arduino-sdk.readthedocs-hosted.com/en/latest/ameba_pro2/amb82-mini/API_Documents/NeuralNetwork/Class%20ObjectDetection.html)

## 直接檢查的宣告與實作

下表路徑都以官方 `Arduino_package/hardware/` 為根；檔案原貌與 SHA256 收在 `references/official_4.0.9/`。

| API／符號 | 宣告與實作 | 本版採用與限制 |
|---|---|---|
| VideoSetting、USB_UVCD_STREAM_PRESET | libraries/Multimedia/src/VideoStream.h、VideoStream.cpp | preset=10，FHD20 H2641Mbps；選擇 cameraOpenUVCD 分支，最終格式由主機協商 |
| configVideoChannel、videoInit、channelBegin／End | 同上；cores/ambpro2/video_drv.c | 依 NN 範例順序，由唯一影像工作操作；channelBegin 確實是啟動 |
| videostream_status／cameraGetCtx | VideoStream.cpp、video_drv.c | 原 cameraGetCtx 有 malloc 被覆寫；本版不呼叫，改相同 context 的非配置 queue peek |
| UVCD::configVideo／nnbegin／begin | libraries/USB/src/UVCD.h、UVCD.cpp | nnbegin 等一次訊號返回；begin 內有 forever loop；兩者不能隨意互換 |
| UVCD::isUsbUvcConnected | 同上 | 讀 UVC state 與 ready 引數，不是只判斷實體線纜 |
| uvc_format_ptr／uvc_format | UVCD.cpp、system/component/media/mmfv2/module_uvcd.h | 使用既有公開 SDK struct，只讀 host state／協商欄位，沒有自行寫 descriptor |
| StreamIO::begin／registerInput／Output、setStackSize／Priority | libraries/Multimedia/src/StreamIO.h、StreamIO.cpp | begin 回傳 0 表連線啟動成功；沿 NN 原配置，無新影像 queue |
| NNObjectDetection::configThreshold／setResultCallback／begin | libraries/NeuralNetwork/src/NNObjectDetection.h、NNObjectDetection.cpp | confidence 0.5、NMS 0.3；begin 為 void，不虛構成功回傳值 |
| ObjectDetectionResult::score／type／座標 | 同上 | score = int(float_score×100)，type 為模型連續索引，座標正規化；不直接共享正在改寫的結果 vector |
| NN callback | NNObjectDetection.cpp::ODResultCallback | 更新 vector 後呼叫使用者 callback，包含 res_cnt=0；p=NULL 則直接返回，故用 freshness 偵測缺少回呼 |
| 58 pottedplant／14 bird | NN 範例 ObjectClassList.h | 80 類 0-based 索引，與附件一致；不是 COCO JSON 的非連續 category_id |
| OSD.createBitmap／drawRect／drawText／update | libraries/Multimedia/src/VideoStreamOverlay.h、VideoStreamOverlay.cpp | 真實 API、單一畫布；GREEN／YELLOW 宏存在，不虛構虛線或畫圖函式 |
| xTaskCreate／vTaskDelay／vTaskDelete、critical sections | system/component/os/freertos/freertos_v202012.00/Source/include/task.h | 套件已有 FreeRTOS 排程；cores/ambpro2/main.cpp 的 Arduino main_task 本身也使用 xTaskCreate |
| xQueueCreate／Send／Receive／Peek | 同目錄 queue.h；mmf2_module.h 的 output_recycle | 使用 task API、timeout=0，不用 ISR API 或忙碌等待 |
| xPortGetFreeHeapSize／MinimumEverFreeHeapSize | 同目錄 portable.h | 僅診斷 FreeRTOS heap，不將 Flash 使用量誤認為相機 RAM 用量 |

MMFModule::_p_mmf_context 是 protected，不是 public。專案的 ModuleView 透過合法 base copy 提供本地唯讀 accessor；已以真實 4.0.9 C++ 編譯／連結檢查。
上述自訂 helper（ModuleView、outputReadyNoAllocation、readNegotiated 等）是本專案程式，不假裝它們是瑞昱 API。

## 編譯工具與模型

| 項目 | 版本／來源 |
|---|---|
| Arduino CLI | 1.2.2，commit c11b9dd5，官方 Linux x86_64 發行檔 |
| package | realtek:AmebaPro2 4.0.9，正式 Boards Manager archive |
| ameba_pro2_toolchain | 1.0.1-p1 |
| ameba_pro2_toolchain2 | 1.0.1-p2，由官方 prebuild 合併 |
| ameba_pro2_tools | 1.4.5 |
| ameba_pro2_nn_models | 1.0.2，內建 yolov7_tiny.nb；未訓練、未替換、未調整 shape |
| FQBN | realtek:AmebaPro2:Ameba_AMB82-MINI |
| 編譯選項 | Camera=f37、NN Source=flash、Show NN Logs=Disable；實機請沿用正確感光元件 |

`validation/download_manifest.json` 保存官方下載地址／核對碼；未把整個 SDK、模型或工具鏈加入交付 ZIP。
`validation/model_and_video_packer.txt` 記錄封裝器辨識結果。部分官方封裝器只掃 .ino，故本版保留實際的 modelSelect／configVideoChannel 呼叫於 .ino。

模型輸入 416×416 是使用者提供的既有設備條件；此版只選官方內建模型，不宣稱用靜態程式重新量測過 NPU tensor 形狀或推論時間。

## 附件與授權

原始兩個附件的 SHA256 與程式清單見 `validation/input_archives.json`。附件內容沒有覆寫，另建立 W2 v2.0。
保留原附件的 MIT LICENSE 與官方參考檔著作權資訊。自訂整合／測試／文件沿同一 MIT 授權提供。
官方核心檔各自原有的授權標頭仍保留，請勿將參考資料夾當作要覆蓋套件的補丁。

AI 協作範圍：附件比較、固定版本 API 查核、架構與程式整合、主機邏輯測試、工具鏈編譯及文件。實際硬體錄影、接線、模型效果與最終課堂報告驗收由使用者完成。
