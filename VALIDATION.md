# 驗證紀錄

本次完成日：2026-11-03（Asia/Taipei）。**沒有實體 AMB82-mini 與 Windows 相機測試環境，所有硬體相容性與穩定性結果均待實機驗證。**

## 儲存庫與報告更新檢查

- 由儲存庫原始 commit `83b7c7408b55155fe293656b3194965ba0347d4e` 更新；舊 ZIP 保留，原 README、驗證說明與試次表另存 `archive/v1.1/`。
- `SmartTree/` 與既有已驗證 v2.0 原始碼逐檔 SHA256 相同；本次將完整程式展開到儲存庫，沒有再修改韌體行為。
- 最終 F 程式已從本儲存庫路徑重新編譯、連結與封裝，exit 0；額外紀錄為 `validation/compile_repository_F_result.json` 與 `compile_repository_F.log`。原七階段紀錄仍保留。
- 從本儲存庫重新執行 `python3 tools/run_checks.py`：10＋13 組 C++ 邏輯、3 項 Python 解析及五份階段一致性檢查通過；見 `validation/repository_checks.log`。
- 2026-11-04 依最終繳交規定收斂為 8 頁報告，保留原 40 筆紀錄與 40 張照片；第 5～8 頁的表格／照片區域與原報告逐像素相同，頁首、頁碼及交叉引用另更新。驗證與 SHA256 見 `validation/report_check.json`。
- 正式更新版改於 `PikaO3O/SmartTree_v2.0` 發布，PDF 與 README 的專案連結同步更新；原儲存庫保留原貌。新報告為 `docs/Ameba影像辨識創意實作_更新報告_v2.0_20261104.pdf`。
- 報告明確區分 2026/10/21 舊版實測、新版軟體驗證與待實機驗證；新增內容沒有將舊照片當成 v2.0 測試結果。

報告來源為 `docs/report_v2.0.html`。需要重新產生時先安裝 PyMuPDF 與 fontTools，再執行：

```sh
python3 tools/build_report.py --original /path/to/Ameba_original_20261021.pdf --output /path/to/updated_report.pdf
```

`--original` 為使用者原本的八頁報告；工具從中讀取嵌入字型與四頁原始證據，本儲存庫未重複收錄整份原報告。這是文件重建工具，與 Arduino 編譯無關。

## 已實際完成

| 檢查 | 結果 | 證據 |
|---|---|---|
| 兩份原附件完整讀取與差異比較 | 完成，原狀態／快照／類別與測試檔相同，v1.2 主要修改顯示啟動配置 | ENGINEERING_ANALYSIS.md、input_archives.json |
| 官方固定版本 API／實作 | V4.0.9 與 4.0.9 release archive，非只看 latest 文件 | SOURCE_REFERENCES.md、references/official_4.0.9/ |
| Arduino CLI／正式 SDK | CLI 1.2.2，realtek:AmebaPro2 4.0.9、AMB82-MINI | compile_results.json、官方下載 checksum |
| 模型／攝影機封裝 | 封裝器識別 yolov7_tiny、DEFAULT、Flash、video status ON | model_and_video_packer.txt |
| 原 v1.1 狀態邏輯相容性 | 10 組通過，grace=0 | host_legacy.log |
| v2 容忍／HOLD／時間／解析 | 13 組通過 | host_v2.log |
| 原始紀錄解析 | 3 項通過，HOLD 負結果不算命中 | host_python.log |
| ASan／UBSan | C++ 兩組程式通過，未出現這兩種 sanitizer 錯誤 | 下方執行方式；LeakSanitizer 未啟用 |
| 階段版本一致性 | B～F 五份自訂副本與根目錄相同，僅 stage define 不同 | checks.log、tools/run_checks.py |
| 參考檔案 checksum | 通過 | references/official_4.0.9/SHA256.json |
| 已知 SDK 輪詢洩漏 | object 仍有 rtw_malloc(72) 呼叫；F 不使用這條 API 路徑 | sdk_cameraGetCtx_disassembly.txt、原始碼 |

`validation/` 為上表證據的相對根目錄。主機邏輯測試沒有模擬完整 NPU、FreeRTOS 排程、USB 控制器、OSD 繪製或 Windows 解碼，不能替代實機。

## 逐一實際編譯

全部使用 Camera=f37、NN Source=flash、NN Logs=Disable。每次編譯依序執行，未讓官方共用封裝工具互相覆蓋。
本次測試環境鏡像使用者已經有的五個 UVC 設定：YUY2=0、NV12=0、MJPG=1、H264=0、H265=0；不是要求使用者增加套件修正。
原官方參考檔仍保留原貌；實際用於編譯的設定另見 compiled_uvcd_configuration.txt。

| 程式 | 編譯／連結／Flash 封裝 | 日誌 |
|---|---|---|
| A0 原 NN 範例僅換 YOLOv7 | 通過，exit 0 | compile_A0.log |
| A1 僅改官方 USB preset | 通過，exit 0 | compile_A1.log |
| B 盆栽篩選 | 通過，exit 0 | compile_B.log |
| C HOLD＋存在狀態 | 通過，exit 0 | compile_C.log |
| D LED／事件／完整 OSD | 通過，exit 0 | compile_D.log |
| E UART 獨立 | 通過，exit 0 | compile_E.log |
| F 最終整合（根目錄） | 通過，exit 0 | compile_F.log |
| F 的 DISPLAY_PROFILE=1 備用分支 | 通過，exit 0；僅此 define 改為 1，其餘與 F 相同 | compile_F_profile1.log、compile_profile1_result.json |

最終 log 沒有自訂程式編譯警告。所有封裝輸出均為 10,113,024 bytes，Arduino 回報約 60%／16,777,216 bytes。
封裝含固定分區、VOE／ISP／NN 資產，因此各階段包長相同；**不同階段的映像 SHA256 不同**，見 firmware_artifacts.json。
此數字是 Flash 映像大小，不能據此推論剩餘影像 RAM。ELF sections 另列 application_sections.log，也不包含執行後完整的動態資源用量。
未把預編譯 binary 當交付燒錄檔，避免使用者感光元件或工具選項不同；請從完整原始碼依自己的硬體編譯。

validated_source.json 保存最終根目錄程式的 SHA256。B～F 的 stage define、兩個官方基準與實際編譯命令均可追溯。

## 可重跑的主機檢查

在專案根目錄：

```sh
python3 tools/run_checks.py
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer tests/test_legacy_monitor.cpp -o /tmp/smarttree-legacy-test
ASAN_OPTIONS=detect_leaks=0 /tmp/smarttree-legacy-test
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer tests/test_v2.cpp -o /tmp/smarttree-v2-test
ASAN_OPTIONS=detect_leaks=0 /tmp/smarttree-v2-test
```

run_checks.py 會執行 10＋13 組 C++ 邏輯與 3 項 Python 解析測試，檢查階段內容、參考 checksum 與幾個架構限制。
它不是完整 C++ 靜態分析器；完整 API／連結檢查由上述真實板套件編譯完成。

## 可重跑的板套件編譯

已有 Arduino CLI 與正常安裝的 4.0.9，於專案根目錄（依自己感光元件調整 camera）：

```sh
python3 tools/compile_all.py --build-root /tmp/smarttree-builds --camera f37
```

Windows 範例：

```powershell
python tools/compile_all.py --cli arduino-cli.exe --build-root C:\IoT\SmartTreeBuilds --camera f37
```

只編譯最終版可加 `--targets F`。也可以直接在 Arduino IDE 按驗證；工具不負責上傳硬體。
如更動根目錄來源後需要重建各階段副本，再執行 `python tools/build_stages.py`，它會覆寫自動生成的階段檔；需要保留自行修改的階段時先另外備份。
Linux 自訂 data 目錄的官方工具固定預設路徑限制，見 TROUBLESHOOTING.md；本次未變更 HOME，未改官方建置工具程式。

## 尚未驗證，不能宣稱通過

| 實機項目 | 狀態 |
|---|---|
| Windows 11 相機實際顯示與 10 分鐘流暢性 | 待實機驗證 |
| YOLOv7-tiny 板端載入、實測 FPS、實際 tensor 形狀 | 待實機驗證；416×416／約 5 次每秒為使用者既有設備資訊 |
| 固定盆栽原始命中、空場景假陽性、門檻比較 | 待實機驗證 |
| 綠／黃框與字型、座標、600 ms 到期後的 Windows 實際呈現 | 待實機驗證 |
| LED 接腳／極性與真實燈號 | 待實機驗證；程式沿用使用者先前成功設定 |
| UART-only PING／STATUS、OTG 拔除後仍回覆 | 待實機驗證；排程與 API 已編譯核對 |
| 相機關閉重開、OTG 不斷電拔插、USB RETRY | 待實機驗證 |
| 10 分鐘及反覆重連後沒有持續 RAM 成長 | 待實機驗證；只移除一個已識別洩漏點，不能代替量測 |
| 720p／其他高解析度模式、替代感光元件 | 未新增模式／未獨立測試；本版先固定官方串流設定 |

實機驗收以 TEST_PLAN.md 與 test_record.csv 為準。任何由編譯或單元測試得出的結果，都不應寫成已確認 Windows 相機不凍結或 USB 自動恢復成功。
