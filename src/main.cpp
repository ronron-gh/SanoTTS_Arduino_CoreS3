#include <Arduino.h>
#include <M5Unified.h>
#include <cmath>
#include <cstring>
#include <memory>
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

struct PcmDeleter {
    void operator()(int16_t *p) const { heap_caps_free(p); }
};

bool playBuffered(const int16_t *audio, uint32_t samples) {
    const int64_t start = esp_timer_get_time();
    const int64_t durationUs = (int64_t)samples * 1000000 / SAAN_SR;
    Serial.printf("Playback start: %u samples, %d Hz, mono, volume=128\n", samples, SAAN_SR);
    if (!M5.Speaker.playRaw(audio, samples, SAAN_SR, false, 1, 0, false)) {
        M5.Speaker.end();
        Serial.println("FAIL: playRaw rejected buffer");
        return false;
    }
    while (M5.Speaker.isPlaying(0)) {
        if (esp_timer_get_time() - start > durationUs + 5000000) {
            // end joins the consumer task before the caller releases its PCM buffer.
            M5.Speaker.end();
            Serial.println("FAIL: playback timeout");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    // isPlaying tracks source consumption; allow queued DMA audio to drain too.
    const auto cfg = M5.Speaker.config();
    const uint32_t drainMs = (cfg.dma_buf_len * cfg.dma_buf_count * 1000 + SAAN_SR - 1) / SAAN_SR + 20;
    vTaskDelay(pdMS_TO_TICKS(drainMs));
    Serial.printf("Playback complete: %.2f ms (including DMA drain wait)\n",
                  (esp_timer_get_time() - start) / 1000.0);
    return true;
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
    const uint64_t expectedWide = (uint64_t)stream.n_frames * SAAN_HOP;
    // This fixed-input test accepts at most 30 seconds; reject corrupt sizes before allocation.
    if (expectedWide > (uint64_t)SAAN_SR * 30 || expectedWide > SIZE_MAX / sizeof(int16_t)) {
        Serial.println("FAIL: PCM length exceeds buffered test limit");
        return false;
    }
    const uint32_t expected = (uint32_t)expectedWide;
    std::unique_ptr<int16_t, PcmDeleter> audio(static_cast<int16_t *>(
        heap_caps_malloc(expected * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
    if (!audio) {
        Serial.println("FAIL: PSRAM PCM allocation");
        return false;
    }
    Serial.printf("PCM buffer: PSRAM %u bytes\n", (unsigned)(expected * sizeof(int16_t)));
    uint32_t clips = 0;
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
        if (samples > expected || (uint32_t)count > expected - samples) {
            Serial.println("FAIL: PCM buffer bounds");
            return false;
        }
        for (int32_t j = 0; j < count; ++j) {
            const float value = pcm[j];
            if (!std::isfinite(value)) {
                ++nonfinite;
                audio.get()[samples + j] = 0;
            } else {
                const float magnitude = fabsf(value);
                if (magnitude > peak) peak = magnitude;
                sumSquares += (double)value * value;
                // Match the sample's lrintf(x * 32767) conversion, guarding huge values first.
                const float scaled = value * 32767.0f;
                long converted;
                if (scaled > 32767.0f) { converted = 32767; ++clips; }
                else if (scaled < -32768.0f) { converted = -32768; ++clips; }
                else { converted = lrintf(scaled); }
                audio.get()[samples + j] = (int16_t)converted;
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
    Serial.printf("PCM conversion clips=%u; wall includes conversion/buffering, excludes playback\n", clips);
    if (!ok) return false;
    return playBuffered(audio.get(), samples);
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
    M5.Speaker.end();
    auto speakerConfig = M5.Speaker.config();
    speakerConfig.sample_rate = SAAN_SR;
    M5.Speaker.config(speakerConfig);
    if (!M5.Speaker.begin()) {
        M5.Speaker.end();
        Serial.println("FAIL: speaker initialization");
        vTaskDelete(nullptr);
        return;
    }
    M5.Speaker.setVolume(128);
    Serial.println("Speaker initialized; heap below includes audio resources.");
    printHeap();
    if (!saan_model_open(&weights)) {
        M5.Speaker.end();
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
        M5.Speaker.end();
        Serial.println("FAIL: cannot allocate aligned arena");
        printHeap();
        vTaskDelete(nullptr);
        return;
    }
    Serial.printf("arena: %s %u bytes at %p\n", location, (unsigned)kArenaBytes, arenaMemory);
    for (;;) {
        if (!infer()) {
            Serial.println("Inference/playback stopped after failure; reset to retry.");
            M5.Speaker.end();
            heap_caps_free(arenaMemory);
            arenaMemory = nullptr;
            vTaskDelete(nullptr);
            return;
        }
        printHeap();
        Serial.printf("task stack minimum free=%u bytes\n", (unsigned)uxTaskGetStackHighWaterMark(nullptr));
        Serial.println("Send r to synthesize and play again.");
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
    config.internal_spk = true;
    M5.begin(config);
    Serial.begin(115200);
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(10, 30);
    M5.Display.println("sanoTTS buffered");
    M5.Display.println("See serial monitor");
    delay(1500);
    if (xTaskCreatePinnedToCore(inferenceTask, "saan_infer", 16384, nullptr, 1, nullptr, 1) != pdPASS) {
        M5.Speaker.end();
        Serial.println("FAIL: cannot create inference task");
    }
}

void loop() {
    M5.update();
    delay(20);
}
