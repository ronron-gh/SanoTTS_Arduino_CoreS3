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
#include "saan_kanji.h"
#include "dictionary.h"
}
#include "text_input.h"

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
jdict_t dictionary;
constexpr int32_t kMaxIds = 350;
int32_t textIds[kMaxIds];
int32_t textIdCount = 0;
char lastText[1024] = "今日は良い天気ですね。";
static_assert(kArenaBytes >= SAAN_KANJI_WORKBYTES, "Japanese parser arena too small");

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

constexpr uint32_t kPrerollSamples = 4 * kPcmCapacity;
constexpr uint32_t kRingBuffers = 3;

class StreamingOutput {
    std::unique_ptr<int16_t, PcmDeleter> storage;
    uint32_t prerollCapacity = 0;
    uint32_t prerollFill = 0;
    uint32_t ringIndex = 0;
    uint32_t sent = 0;
    bool finished = false;

    bool queue(const int16_t *data, uint32_t count) {
        const int64_t before = esp_timer_get_time();
        // One producer owns channel 0. The consumer can only free slots while we wait.
        while (M5.Speaker.isPlaying(0) >= 2) {
            if (esp_timer_get_time() - before > 5000000) {
                Serial.println("FAIL: speaker queue timeout");
                return false;
            }
            vTaskDelay(1);
        }
        queueWaitUs += esp_timer_get_time() - before;
        if (!M5.Speaker.isRunning()) {
            Serial.println("FAIL: speaker task stopped");
            return false;
        }
        if (sent && !M5.Speaker.isPlaying(0)) ++queueEmptyEvents;
        const int64_t submitted = esp_timer_get_time();
        if (!firstRequestUs) firstRequestUs = submitted;
        if (!M5.Speaker.playRaw(data, count, SAAN_SR, false, 1, 0, false)) {
            Serial.println("FAIL: streaming playRaw");
            return false;
        }
        sent += count;
        return true;
    }

public:
    int64_t firstRequestUs = 0;
    int64_t queueWaitUs = 0;
    uint32_t queueEmptyEvents = 0;

    ~StreamingOutput() {
        // On every error, join the consumer before storage is released by unique_ptr.
        if (!finished && firstRequestUs && M5.Speaker.isRunning()) M5.Speaker.end();
    }

    bool allocate(uint32_t expected) {
        prerollCapacity = expected < kPrerollSamples ? expected : kPrerollSamples;
        const size_t bytes = (prerollCapacity + kRingBuffers * kPcmCapacity) * sizeof(int16_t);
        storage.reset(static_cast<int16_t *>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
        if (!storage) { Serial.println("FAIL: PSRAM streaming buffers"); return false; }
        Serial.printf("Streaming buffers: PSRAM %u bytes, preroll=%u samples, ring=3 x %u\n",
                      (unsigned)bytes, prerollCapacity, (unsigned)kPcmCapacity);
        return true;
    }

    int16_t *destination(uint32_t count) {
        if (!firstRequestUs) {
            if (count > prerollCapacity - prerollFill) return nullptr;
            return storage.get() + prerollFill;
        }
        if (count > kPcmCapacity) return nullptr;
        // After each successful enqueue only current+next can remain referenced.
        // With three ring buffers, this slot cannot be either of those two.
        return storage.get() + prerollCapacity + ringIndex * kPcmCapacity;
    }

    bool submit(uint32_t count) {
        if (!firstRequestUs) {
            prerollFill += count;
            if (prerollFill < prerollCapacity) return true;
            return queue(storage.get(), prerollFill);
        }
        const bool ok = queue(storage.get() + prerollCapacity + ringIndex * kPcmCapacity, count);
        if (ok) ringIndex = (ringIndex + 1) % kRingBuffers;
        return ok;
    }

    bool finish(uint32_t expected) {
        if (!firstRequestUs || sent != expected) {
            Serial.println("FAIL: incomplete streaming submission");
            return false;
        }
        const int64_t started = esp_timer_get_time();
        while (M5.Speaker.isPlaying(0)) {
            if (esp_timer_get_time() - started > 5000000) {
                Serial.println("FAIL: playback drain timeout");
                return false;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        const auto cfg = M5.Speaker.config();
        const uint32_t drainMs = (cfg.dma_buf_len * cfg.dma_buf_count * 1000 + SAAN_SR - 1) / SAAN_SR + 20;
        vTaskDelay(pdMS_TO_TICKS(drainMs));
        finished = true;
        Serial.printf("Playback complete: submitted=%u queue_empty_events=%u\n", sent, queueEmptyEvents);
        return true;
    }
};

bool infer() {
    saan_arena_init(&arena, arenaMemory, kArenaBytes);
    memset(&stream, 0, sizeof(stream));
    const int64_t started = esp_timer_get_time();
    saan_status status = saan_stream_init(&stream, &weights, &arena,
                                         textIds, textIdCount, SAAN_S_V);
    const int64_t initialized = esp_timer_get_time();
    if (status != SAAN_OK || arena.failed ||
        arena.used != saan_stream_arena_used(textIdCount)) {
        Serial.printf("FAIL init: %s arena=%u failed=%d\n", saan_strerror(status),
                      (unsigned)arena.used, arena.failed);
        return false;
    }
    if (stream.n_frames <= 0) {
        Serial.println("FAIL: no predicted frames");
        return false;
    }
    const uint64_t expectedWide = (uint64_t)stream.n_frames * SAAN_HOP;
    // Bound generated audio even for unexpected duration predictions.
    if (expectedWide > (uint64_t)SAAN_SR * 30 || expectedWide > SIZE_MAX / sizeof(int16_t)) {
        Serial.println("FAIL: PCM length exceeds streaming test limit");
        return false;
    }
    const uint32_t expected = (uint32_t)expectedWide;
    StreamingOutput output;
    if (!output.allocate(expected)) return false;
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
        int16_t *destination = output.destination(count);
        if (!destination) { Serial.println("FAIL: streaming buffer bounds"); return false; }
        for (int32_t j = 0; j < count; ++j) {
            const float value = pcm[j];
            if (!std::isfinite(value)) {
                ++nonfinite;
                destination[j] = 0;
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
                destination[j] = (int16_t)converted;
            }
            // FNV-1a over float32 bytes in little-endian order; not an int16 audio hash.
            uint32_t bits;
            memcpy(&bits, &value, sizeof(bits));
            for (unsigned b = 0; b < 4; ++b) {
                checksum ^= (bits >> (8 * b)) & 0xff;
                checksum *= UINT64_C(1099511628211);
            }
        }
        if (nonfinite) {
            Serial.println("FAIL: nonfinite PCM; stopping streaming playback");
            return false;
        }
        if (!output.submit((uint32_t)count)) return false;
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
    Serial.printf("PCM conversion clips=%u; wall includes conversion/queue waits, overlaps playback\n", clips);
    if (!ok) return false;
    if (!output.finish(samples)) return false;
    Serial.printf("Streaming timing: first_request=%.2f ms queue_wait=%.2f ms end_to_end=%.2f ms\n",
                  (output.firstRequestUs - started) / 1000.0, output.queueWaitUs / 1000.0,
                  (esp_timer_get_time() - started) / 1000.0);
    return true;
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
    Serial.printf("sanoTTS Arduino: %s / PIE=%d / Japanese text / max_ids=%d / sample_rate=%d\n",
                  SAAN_INT8_ACT ? "W8A8" : "W8A32", SAAN_PIE, kMaxIds, SAAN_SR);
#if SAAN_PIE
    if (!pieSelfTest()) {
        vTaskDelete(nullptr);
        return;
    }
#endif
    if (M5.Speaker.isRunning()) M5.Speaker.end();
    auto speakerConfig = M5.Speaker.config();
    speakerConfig.sample_rate = SAAN_SR;
    speakerConfig.stereo = false;
    M5.Speaker.config(speakerConfig);
    if (!M5.Speaker.begin() || !M5.Speaker.isEnabled()) {
        if (M5.Speaker.isRunning()) M5.Speaker.end();
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
    if (!text_dictionary_open(&dictionary) || !saan_kanji_init()) {
        Serial.println("FAIL: dictionary/parser initialization; reset to retry.");
        M5.Speaker.end();
        heap_caps_free(arenaMemory);
        arenaMemory = nullptr;
        vTaskDelete(nullptr);
        return;
    }
    Serial.printf("dictionary: entries=%u surfaces=%u matrix=%ux%u clusters=%ux%u char_runs=%u\n",
                  dictionary.n_entries, dictionary.n_surfaces, dictionary.lsize, dictionary.rsize,
                  dictionary.matrix_kr, dictionary.matrix_kc, dictionary.n_char_runs);
    Serial.printf("parser: workbytes=%u viterbi=%u arena=%u\n",
                  (unsigned)saan_kanji_workbytes(), (unsigned)saan_kanji_vitbytes(kArenaBytes),
                  (unsigned)kArenaBytes);
    TextLine line;
    for (;;) {
        Serial.printf("Text: %s\n", lastText);
        const int64_t parseStart = esp_timer_get_time();
        int tokens = 0;
        textIdCount = 0;
        const auto parsed = saan_kanji_to_ids(&dictionary, lastText, strlen(lastText),
                                            arenaMemory, kArenaBytes, textIds, kMaxIds,
                                            &textIdCount, &tokens);
        Serial.printf("Parse: %s status=%d tokens=%d ids=%ld time=%.2f ms\n",
                      saan_kanji_strerror(parsed), (int)parsed, tokens, (long)textIdCount,
                      (esp_timer_get_time() - parseStart) / 1000.0);
        if (parsed == SAAN_KANJI_OK && textIdCount > 0 && textIdCount <= kMaxIds) {
            if (!infer()) {
                Serial.println("Inference/playback stopped after failure; reset to retry.");
                if (M5.Speaker.isRunning()) M5.Speaker.end();
                heap_caps_free(arenaMemory);
                arenaMemory = nullptr;
                vTaskDelete(nullptr);
                return;
            }
        } else {
            Serial.println("Text rejected; try a shorter sentence. No playback.");
        }
        printHeap();
        Serial.printf("task stack minimum free=%u bytes\n", (unsigned)uxTaskGetStackHighWaterMark(nullptr));
        // Input received during synthesis is intentionally discarded, not queued as another utterance.
        while (Serial.available()) Serial.read();
        line.reset();
        Serial.println("Ready: UTF-8 Japanese + Enter (max 1023 bytes); /r repeats the last text.");
        for (;;) {
            if (!Serial.available()) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
            const auto result = line.push((uint8_t)Serial.read());
            if (result == TextLine::More) continue;
            if (result != TextLine::Complete) {
                Serial.println("Input rejected: empty, invalid UTF-8/control byte, or over 1023 bytes. Ready.");
                continue;
            }
            if (strcmp(line.text(), "/r") != 0) strcpy(lastText, line.text());
            break;
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
    M5.Display.println("sanoTTS Japanese");
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
