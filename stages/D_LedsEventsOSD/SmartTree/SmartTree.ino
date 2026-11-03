/* SmartTree W2 v2.0 — AMB82-mini / AmebaPro2 4.0.9.
 * USB/NN lifecycle: official UVCDObjectDetectionLoop; see SOURCE_REFERENCES.md.
 * The model selection stays here because the board's build hook scans .ino files.
 */
#include <Arduino.h>
#include "NNObjectDetection.h"
#include "SmartTreeApp.h"

void selectSmartTreeModel(NNObjectDetection& detector) {
    detector.modelSelect(OBJECT_DETECTION, DEFAULT_YOLOV7TINY, NA_MODEL, NA_MODEL);
}

// The 4.0.9 firmware packer also scans .ino for configVideoChannel().
// Keep these real calls visible so the VOE firmware is included in flash_ntz.bin.
void configureSmartTreeCamera(Video& camera, VideoSetting& streamConfig, VideoSetting& nnConfig) {
    camera.configVideoChannel(0, streamConfig);
    camera.configVideoChannel(3, nnConfig);
}

void setup() { smartTreeSetup(); }
void loop() { smartTreeLoop(); }
