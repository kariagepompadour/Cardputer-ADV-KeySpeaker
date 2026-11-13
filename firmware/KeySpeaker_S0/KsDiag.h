/*
  KsDiag.h — KeySpeaker: Stage 0 diagnostics for the MP3 feasibility work (development branch only, KS_DIAG 1)

  Adds nothing to the audio, USB, BLE or screen code. It only reads and prints:
    [DIAG]   every 10 s: internal heap (free now, lowest ever, largest free block, number of free blocks), and each
             core's idle share.
    [DIAGT]  every 10 s: for each FreeRTOS task, its core, its share of the CPU over the last 10 s and its stack that has
             never been used (bytes), taken from the run-time statistics the core already enables.
    [PROBE]  on the serial character 'P': allocates and frees the memory an MP3 player would need (the sizes below), 50
             times, with small unrelated allocations mixed in, and reports the heap before, during and after.

  The probe's sizes (bytes): Helix decoder state 2032 + 56 + 328 + 284 + 4624 + 840 + 6944 + 8708 (= 23,816, from sizeof
  of its structs compiled for the ESP32-S3), input buffer 2048, SD read-ahead 8192, PCM output 4608, decoder task stack
  8192, file list 3072, FatFS / SPI 5120 (all but Helix's structs are estimates). Also the resampler's work buffer 2336 and filter table 4160 (firmware/mp3_spike/Sinc48.h). Total 61,544 bytes in 16 blocks.
*/
#pragma once
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace ksdiag {

static constexpr uint32_t CAPS = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;

static void heapLine(const char* tag) {
  multi_heap_info_t i;
  heap_caps_get_info(&i, CAPS);
  Serial.printf("[DIAG] %s heap free=%u min=%u largest=%u free_blocks=%u alloc_blocks=%u\n", tag, (unsigned)i.total_free_bytes,
                (unsigned)i.minimum_free_bytes, (unsigned)i.largest_free_block, (unsigned)i.free_blocks, (unsigned)i.allocated_blocks);
}

// Every 10 s from loop(). uxTaskGetSystemState() holds the scheduler for a few tens of microseconds.
static void report(uint32_t nowMs) {
  static constexpr int MAXT = 40;
  static TaskStatus_t ts[MAXT];
  static TaskHandle_t prevH[MAXT];
  static uint32_t prevRt[MAXT];
  static int prevN = 0;
  static uint32_t prevTotal = 0;
  uint32_t total = 0;
  int n = (int)uxTaskGetSystemState(ts, MAXT, &total);
  uint32_t dTot = total - prevTotal;
  char tag[24];
  snprintf(tag, sizeof tag, "t=%lu", (unsigned long)(nowMs / 1000));
  heapLine(tag);
  if (prevTotal && dTot) {
    static char line[640];
    int len = snprintf(line, sizeof line, "[DIAGT] t=%lu", (unsigned long)(nowMs / 1000));
    float idle[2] = {-1, -1};
    for (int i = 0; i < n && len < (int)sizeof line - 60; i++) {
      uint32_t before = 0;
      for (int j = 0; j < prevN; j++)
        if (prevH[j] == ts[i].xHandle) before = prevRt[j];
      float pct = 100.0f * (float)(ts[i].ulRunTimeCounter - before) / (float)dTot;
      int core = (int)ts[i].xCoreID;
      if (!strcmp(ts[i].pcTaskName, "IDLE0")) idle[0] = pct;
      if (!strcmp(ts[i].pcTaskName, "IDLE1")) idle[1] = pct;
      len += snprintf(line + len, sizeof line - len, " %s:c%s:%.1f%%:stk%u", ts[i].pcTaskName, core > 1 ? "x" : (core ? "1" : "0"), pct,
                      (unsigned)ts[i].usStackHighWaterMark);
    }
    Serial.println(line);
    Serial.printf("[DIAG] core0 busy=%.1f%% core1 busy=%.1f%% (100 - idle task share; interval %lu us)\n", idle[0] < 0 ? -1 : 100.0f - idle[0],
                  idle[1] < 0 ? -1 : 100.0f - idle[1], (unsigned long)dTot);
  }
  prevN = n < MAXT ? n : MAXT;
  for (int i = 0; i < prevN; i++) {
    prevH[i] = ts[i].xHandle;
    prevRt[i] = ts[i].ulRunTimeCounter;
  }
  prevTotal = total;
}

// What an MP3 player would allocate (see the header comment). The first eight are Helix's own structs.
static const uint16_t PROBE_SIZES[] = {2032, 56, 328, 284, 4624, 840, 6944, 8708, 2048, 8192, 4608, 8192, 3072, 5120, 2336, 4160};
static constexpr int PROBE_N = sizeof PROBE_SIZES / sizeof PROBE_SIZES[0];

// 'cycles' rounds of: allocate everything, hold ~20 ms, free it (in a different order every other round), with a few small
// blocks from "other code" kept until the end so that the heap is not freed back into one piece.
static void heapProbe(int cycles) {
  uint32_t total = 0;
  for (int k = 0; k < PROBE_N; k++) total += PROBE_SIZES[k];
  multi_heap_info_t b, a;
  heap_caps_get_info(&b, CAPS);
  Serial.printf("[PROBE] start: %d cycles of %u bytes in %d blocks; free=%u min=%u largest=%u free_blocks=%u\n", cycles, (unsigned)total, PROBE_N,
                (unsigned)b.total_free_bytes, (unsigned)b.minimum_free_bytes, (unsigned)b.largest_free_block, (unsigned)b.free_blocks);
  void* blk[PROBE_N];
  void* noise[64];
  int noiseN = 0;
  int fails = 0, worstFailAt = -1;
  uint32_t lowFree = 0xFFFFFFFF, lowLargest = 0xFFFFFFFF;
  for (int c = 0; c < cycles; c++) {
    int got = 0;
    for (int k = 0; k < PROBE_N; k++) {
      blk[k] = heap_caps_malloc(PROBE_SIZES[k], CAPS);
      if (blk[k]) got++;
    }
    if (got != PROBE_N) {
      fails++;
      if (worstFailAt < 0) worstFailAt = c;
    }
    multi_heap_info_t m;
    heap_caps_get_info(&m, CAPS);
    if (m.total_free_bytes < lowFree) lowFree = m.total_free_bytes;
    if (m.largest_free_block < lowLargest) lowLargest = m.largest_free_block;
    if (c < 2 || c == cycles - 1 || got != PROBE_N)
      Serial.printf("[PROBE] cycle %d: allocated %d/%d, free=%u largest=%u free_blocks=%u\n", c, got, PROBE_N, (unsigned)m.total_free_bytes,
                    (unsigned)m.largest_free_block, (unsigned)m.free_blocks);
    delay(20);
    if (c & 1)
      for (int k = PROBE_N - 1; k >= 0; k--) heap_caps_free(blk[k]);
    else
      for (int k = 0; k < PROBE_N; k++) heap_caps_free(blk[k]);
    if (noiseN < 64 && (c % 3) == 0) {  // a small block from other code, kept
      void* p = heap_caps_malloc(32 + (c * 37) % 480, CAPS);
      if (p) noise[noiseN++] = p;
    }
  }
  multi_heap_info_t m;
  heap_caps_get_info(&m, CAPS);
  Serial.printf("[PROBE] before free_blocks done: free=%u largest=%u (noise blocks held: %d)\n", (unsigned)m.total_free_bytes,
                (unsigned)m.largest_free_block, noiseN);
  for (int i = 0; i < noiseN; i++) heap_caps_free(noise[i]);
  heap_caps_get_info(&a, CAPS);
  Serial.printf("[PROBE] end: free=%u (was %u) largest=%u (was %u) free_blocks=%u (was %u) min=%u; lowest free during=%u lowest largest during=%u; "
                "failed cycles=%d (first at %d)\n",
                (unsigned)a.total_free_bytes, (unsigned)b.total_free_bytes, (unsigned)a.largest_free_block, (unsigned)b.largest_free_block,
                (unsigned)a.free_blocks, (unsigned)b.free_blocks, (unsigned)a.minimum_free_bytes, (unsigned)lowFree, (unsigned)lowLargest, fails,
                worstFailAt);
}

}  // namespace ksdiag
