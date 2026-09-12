#include <Arduino.h>
#include <M5Unified.h>
#include <cmath>
#include <cstring>
#include "esp_heap_caps.h"
#include "esp_timer.h"

extern "C" {
#include "saan_model.h"
#include "saanotts_stream.h"
#include "saanotts_int8.h"
}
#include "demo_ids.h"

#if SAAN_PIE && (!SAAN_INT8_ACT || !defined(CONFIG_IDF_TARGET_ESP32S3))
#error "PIE inference requires ESP32-S3 and SAAN_INT8_ACT=1"
#endif

namespace {
constexpr size_t kArenaBytes = 176 * 1024;
constexpr size_t kPcmCapacity = SAAN_CHUNK * SAAN_HOP;
float pcm[kPcmCapacity];
saan_weights weights;
saan_arena arena;
saan_stream stream;
void *arenaMemory = nullptr;

void printHeap() {
    const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    Serial.printf("heap: internal=%u largest=%u psram=%u bytes\n",
                  (unsigned)heap_caps_get_free_size(caps),
                  (unsigned)heap_caps_get_largest_free_block(caps),
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

bool infer() {
    saan_arena_init(&arena, arenaMemory, kArenaBytes);
    memset(&stream, 0, sizeof(stream));
    const int64_t started = esp_timer_get_time();
    saan_status status = saan_stream_init(&stream, &weights, &arena,
                                         kSaanDemoIds, SAAN_DEMO_N_IDS, SAAN_S_V);
    const int64_t initialized = esp_timer_get_time();
    if (status != SAAN_OK || arena.failed ||
        arena.used != saan_stream_arena_used(SAAN_DEMO_N_IDS)) {
        Serial.printf("FAIL init: %s arena=%u failed=%d\n", saan_strerror(status),
                      (unsigned)arena.used, arena.failed);
        return false;
    }
    if (stream.n_frames <= 0) {
        Serial.println("FAIL: no predicted frames");
        return false;
    }
    const uint32_t expected = (uint32_t)stream.n_frames * SAAN_HOP;
    uint32_t samples = 0;
    uint32_t chunks = 0;
    uint32_t nonfinite = 0;
    uint64_t checksum = UINT64_C(14695981039346656037);
    double sumSquares = 0;
    float peak = 0;
    int64_t pullUs = 0;
    int64_t maxPullUs = 0;
    bool ended = false;
    // Bound the loop so a broken termination cannot run indefinitely.
    for (uint32_t i = 0; i <= (expected + kPcmCapacity - 1) / kPcmCapacity; ++i) {
        int32_t count = 0;
        const int64_t before = esp_timer_get_time();
        status = saan_stream_pull(&stream, pcm, &count);
        const int64_t elapsed = esp_timer_get_time() - before;
        pullUs += elapsed;
        if (elapsed > maxPullUs) maxPullUs = elapsed;
        if (status != SAAN_OK || arena.failed || count < 0 || count > SAAN_CHUNK) {
            Serial.printf("FAIL pull: %s count=%d arena_failed=%d\n",
                          saan_strerror(status), (int)count, arena.failed);
            return false;
        }
        if (count == 0) { ended = true; break; }
        // The API returns frames, each containing SAAN_HOP PCM samples.
        count *= SAAN_HOP;
        for (int32_t j = 0; j < count; ++j) {
            const float value = pcm[j];
            if (!std::isfinite(value)) {
                ++nonfinite;
            } else {
                const float magnitude = fabsf(value);
                if (magnitude > peak) peak = magnitude;
                sumSquares += (double)value * value;
            }
            // FNV-1a over float32 bytes in little-endian order; not an int16 audio hash.
            uint32_t bits;
            memcpy(&bits, &value, sizeof(bits));
            for (unsigned b = 0; b < 4; ++b) {
                checksum ^= (bits >> (8 * b)) & 0xff;
                checksum *= UINT64_C(1099511628211);
            }
        }
        samples += count;
        ++chunks;
        vTaskDelay(1);
    }
    const int64_t wallUs = esp_timer_get_time() - started;
    const double audioSeconds = (double)samples / SAAN_SR;
    const double computeMs = (initialized - started + pullUs) / 1000.0;
    const bool ok = ended && samples == expected && nonfinite == 0 && peak > 0;
    Serial.printf("%s: samples=%u expected=%u chunks=%u nonfinite=%u\n",
                  ok ? "PASS" : "FAIL", samples, expected, chunks, nonfinite);
    Serial.printf("audio=%.3f s init=%.2f ms compute=%.2f ms wall=%.2f ms max_pull=%.2f ms\n",
                  audioSeconds, (initialized - started) / 1000.0, computeMs,
                  wallUs / 1000.0, maxPullUs / 1000.0);
    Serial.printf("compute_xRT=%.3f wall_xRT=%.3f peak=%.7f rms=%.7f\n",
                  audioSeconds > 0 ? computeMs / (audioSeconds * 1000) : 0,
                  audioSeconds > 0 ? wallUs / (audioSeconds * 1000000) : 0,
                  peak, samples ? sqrt(sumSquares / samples) : 0);
    Serial.printf("float32-le FNV1a64=%08lx%08lx arena_used=%u peak=%u capacity=%u\n",
                  (unsigned long)(checksum >> 32), (unsigned long)(checksum & 0xffffffff),
                  (unsigned)arena.used, (unsigned)arena.peak, (unsigned)kArenaBytes);
    return ok;
}

#if SAAN_PIE
bool pieSelfTest() {
    alignas(16) int8_t a[32];
    alignas(16) int8_t b[32];
    int32_t expected = 0;
    for (int i = 0; i < 32; ++i) {
        a[i] = (int8_t)(i * 8 - 128);
        b[i] = (int8_t)(127 - i * 7);
        expected += (int32_t)a[i] * b[i];
    }
    const int32_t actual = saan_dot_i8_pie(a, b, 32);
    Serial.printf("PIE dot self-test: %s actual=%ld expected=%ld\n",
                  actual == expected ? "PASS" : "FAIL", (long)actual, (long)expected);
    return actual == expected;
}
#endif

void inferenceTask(void *) {
    Serial.printf("sanoTTS Arduino: %s / PIE=%d / ids=%d / sample_rate=%d\n",
                  SAAN_INT8_ACT ? "W8A8" : "W8A32", SAAN_PIE, SAAN_DEMO_N_IDS, SAAN_SR);
#if SAAN_PIE
    if (!pieSelfTest()) {
        vTaskDelete(nullptr);
        return;
    }
#endif
    printHeap();
    if (!saan_model_open(&weights)) {
        Serial.println("FAIL: model load");
        vTaskDelete(nullptr);
        return;
    }
    Serial.printf("model: version=%u tensors=%u bytes=%u\n",
                  (unsigned)weights.version, (unsigned)weights.n_tensors, (unsigned)weights.size);
    arenaMemory = heap_caps_aligned_alloc(16, kArenaBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const char *location = "internal RAM";
    if (!arenaMemory) {
        arenaMemory = heap_caps_aligned_alloc(16, kArenaBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        location = "PSRAM (slower)";
    }
    if (!arenaMemory) {
        Serial.println("FAIL: cannot allocate aligned arena");
        printHeap();
        vTaskDelete(nullptr);
        return;
    }
    Serial.printf("arena: %s %u bytes at %p\n", location, (unsigned)kArenaBytes, arenaMemory);
    for (;;) {
        if (!infer()) {
            Serial.println("Inference stopped after failure; reset to retry.");
            heap_caps_free(arenaMemory);
            arenaMemory = nullptr;
            vTaskDelete(nullptr);
            return;
        }
        printHeap();
        Serial.printf("task stack minimum free=%u bytes\n", (unsigned)uxTaskGetStackHighWaterMark(nullptr));
        Serial.println("Send r to repeat the same inference. No audio playback.");
        while (Serial.available()) Serial.read();
        for (;;) {
            if (Serial.available() && Serial.read() == 'r') break;
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}
}  // namespace

void setup() {
    auto config = M5.config();
    config.internal_mic = false;
    config.internal_spk = false;
    M5.begin(config);
    Serial.begin(115200);
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(10, 30);
    M5.Display.println("sanoTTS inference");
    M5.Display.println("See serial monitor");
    delay(1500);
    if (xTaskCreatePinnedToCore(inferenceTask, "saan_infer", 16384, nullptr, 1, nullptr, 1) != pdPASS) {
        Serial.println("FAIL: cannot create inference task");
    }
}

void loop() {
    M5.update();
    delay(20);
}
