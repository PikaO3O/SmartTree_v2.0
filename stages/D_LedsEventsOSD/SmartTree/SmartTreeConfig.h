#define SMARTTREE_STAGE 4
#ifndef SMARTTREE_CONFIG_H
#define SMARTTREE_CONFIG_H
#include <stdint.h>

// Staged builds: B=2, C=3, D=4, E=5, F=6. Final sketch defaults to F.
#ifndef SMARTTREE_STAGE
#define SMARTTREE_STAGE 6
#endif
// 0 = official USB preset (recommended); 1 = exact NN example FHD/24/H264 startup.
// Both are startup configurations: Windows negotiates the actual UVC format.
#ifndef SMARTTREE_DISPLAY_PROFILE
#define SMARTTREE_DISPLAY_PROFILE 0
#endif
#if SMARTTREE_STAGE < 2 || SMARTTREE_STAGE > 6
#error "SMARTTREE_STAGE must be 2 through 6"
#endif
#if SMARTTREE_DISPLAY_PROFILE < 0 || SMARTTREE_DISPLAY_PROFILE > 1
#error "SMARTTREE_DISPLAY_PROFILE must be 0 or 1"
#endif

namespace Config {
constexpr int STREAM_CHANNEL = 0;
constexpr int NN_CHANNEL = 3;
constexpr int NN_WIDTH = 576;
constexpr int NN_HEIGHT = 320;
constexpr int NN_SOURCE_FPS = 10;
constexpr int PLANT_CLASS = 58;
constexpr int BIRD_CLASS = 14;
constexpr int CONFIDENCE_PERCENT = 50; // model gate AND application gate
constexpr float NMS_THRESHOLD = 0.30f;
constexpr uint32_t CONFIRM_MS = 2000;  // v1.1, unchanged
constexpr uint32_t MISSING_MS = 5000;  // v1.1, unchanged
constexpr uint32_t NN_FRESH_MS = 1500; // v1.1, unchanged
constexpr uint32_t BOX_HOLD_MS = (SMARTTREE_STAGE >= 3) ? 600 : 0;
constexpr uint32_t MISS_GRACE_MS = (SMARTTREE_STAGE >= 3) ? 1000 : 0;
constexpr uint32_t MEDIA_POLL_MS = 100;
constexpr uint32_t APP_POLL_MS = 5;
constexpr unsigned QUEUE_LENGTH = 8;
constexpr unsigned MEDIA_STACK_WORDS = 8192; // 32 KiB on this ARM port
constexpr bool SHOW_BIRD = false; // v1.1 optional bird boxes; never affects plant logic
constexpr bool USE_MONITOR = SMARTTREE_STAGE >= 3;
constexpr bool USE_LEDS_AND_EVENTS = SMARTTREE_STAGE >= 4;
constexpr bool DEFAULT_EVENT_LOG = true; // transitions only, no per-inference logging
constexpr bool DEFAULT_AUTO_STATUS = false;
static_assert(CONFIDENCE_PERCENT >= 1 && CONFIDENCE_PERCENT <= 99, "invalid confidence");
static_assert(NN_FRESH_MS > MISS_GRACE_MS, "timeout must exceed miss grace");
}
#endif
