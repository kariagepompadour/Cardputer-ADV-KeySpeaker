/*
  mp3_spike — KeySpeaker MP3 feasibility spike, stage 1 (a separate sketch: it does not touch KeySpeaker_S0)

  No audio is played. It measures, on a Cardputer ADV with a microSD card holding .mp3 files (in / or /music):
    1. Heap: free / lowest / largest block before and after the decoder, the SD file, the buffers; then 100 rounds of
       allocate-everything / free-everything (Helix's own mallocs, the way an MP3 input would do it on entry and exit),
       with the heap compared before and after. A ballast block first brings the free heap down to KeySpeaker's measured
       87,500 bytes (BALLAST_FREE_TARGET), so the numbers are those of the real firmware's room, not of an empty sketch.
    2. CPU: for each file, a full decode (Helix) + mono mix + 44.1 -> 48 kHz sinc resample (Sinc48.h), as fast as
       possible, on core 0 and on core 1, in microseconds per frame (average, maximum, 99th percentile of 50 us bins)
       and as the share of one core at real-time speed (processing time / audio time).
    3. SD: a paced sequential read of each file in 4 KB blocks at the file's own bit rate: latency histogram and the
       slowest read, to see whether the card ever stalls longer than the buffers (ring ~150-250 ms) can hide.
  Serial (115200, USB CDC): 'g' runs everything again, 'h' only the heap test. Results are printed as [SPIKE] lines.

  The decoder is the Helix MP3 decoder (chmorgan/esp-libhelix-mp3, RealNetworks RPSL / RCSL) that the m5stack:esp32 3.3.10
  SDK already ships prebuilt (libchmorgan__esp-libhelix-mp3.a): no source is added to this repository and nothing extra
  is installed; <mp3dec.h> is found on the SDK's include path. Its license still needs a decision before release.

  Board: M5Cardputer (m5stack:esp32 3.3.10), USB CDC On Boot: Enabled, PartitionScheme default_8MB.
*/
#include <M5Cardputer.h>
#include <SPI.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mp3dec.h>
#include "Sinc48.h"

struct Stats;  // forward declarations: the IDE's generated function prototypes come before the definitions
struct Job;

static constexpr uint32_t BALLAST_FREE_TARGET = 87500;  // KeySpeaker S15-2 on the device (AUX input, flight view), bytes
static constexpr uint32_t CAPS = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
static constexpr int SD_SCK = 40, SD_MISO = 39, SD_MOSI = 14, SD_CS = 12;
static constexpr size_t INBUF = 2048;   // Helix wants at least MAINBUF_SIZE (1940) plus the next frame's header
static constexpr size_t READ_BLOCK = 4096;
static constexpr int MAXFILES = 40;

static char files[MAXFILES][200];
static int nFiles = 0;
static void* ballast = nullptr;

static void heapLine(const char* what) {
  multi_heap_info_t i;
  heap_caps_get_info(&i, CAPS);
  Serial.printf("[SPIKE] heap %-28s free=%u min=%u largest=%u free_blocks=%u\n", what, (unsigned)i.total_free_bytes, (unsigned)i.minimum_free_bytes,
                (unsigned)i.largest_free_block, (unsigned)i.free_blocks);
}

static void scan(const char* dir) {
  File d = SD.open(dir);
  if (!d || !d.isDirectory()) return;
  for (File f = d.openNextFile(); f && nFiles < MAXFILES; f = d.openNextFile()) {
    String n = f.name();
    String low = n;
    low.toLowerCase();
    if (!f.isDirectory() && low.endsWith(".mp3") && !n.startsWith(".")) {
      snprintf(files[nFiles++], sizeof files[0], "%s%s%s", dir, dir[1] ? "/" : "", n.c_str());
    }
  }
}

// ---- 1. heap
// Allocates what an MP3 input would on entry (Helix's own structs through MP3InitDecoder, plus the buffers) and frees it again.
struct Kit {
  HMP3Decoder dec = nullptr;
  uint8_t* in = nullptr;
  uint8_t* rd = nullptr;
  int16_t* pcm = nullptr;  // stereo out, then reused as the 48 kHz mono out: 1152 x 2 x 2 = 4608 bytes
  sinc48::Resampler* rs = nullptr;
  void* list = nullptr;
  void* stack = nullptr;
  bool ok() const { return dec && in && rd && pcm && rs && list && stack; }
  bool get() {
    dec = MP3InitDecoder();
    in = (uint8_t*)heap_caps_malloc(INBUF, CAPS);
    rd = (uint8_t*)heap_caps_malloc(8192, CAPS);  // SD read-ahead
    pcm = (int16_t*)heap_caps_malloc(1152 * 2 * sizeof(int16_t), CAPS);
    rs = (sinc48::Resampler*)heap_caps_malloc(sizeof(sinc48::Resampler), CAPS);
    list = heap_caps_malloc(3072, CAPS);   // file names
    stack = heap_caps_malloc(8192, CAPS);  // stands for the decoder task's stack
    return ok();
  }
  void put() {
    if (dec) MP3FreeDecoder(dec);
    heap_caps_free(in); heap_caps_free(rd); heap_caps_free(pcm); heap_caps_free(rs); heap_caps_free(list); heap_caps_free(stack);
    *this = Kit();
  }
};

static void heapTest() {
  multi_heap_info_t a, b;
  heap_caps_get_info(&a, CAPS);
  heapLine("start");
  int fails = 0;
  uint32_t lowFree = 0xFFFFFFFF, lowLargest = 0xFFFFFFFF;
  for (int c = 0; c < 100; c++) {
    Kit k;
    if (!k.get()) fails++;
    multi_heap_info_t m;
    heap_caps_get_info(&m, CAPS);
    if (m.total_free_bytes < lowFree) lowFree = m.total_free_bytes;
    if (m.largest_free_block < lowLargest) lowLargest = m.largest_free_block;
    if (c == 0) heapLine("with the kit (first round)");
    if (c & 1) { /* also open and close a file and the SD stack's own allocations */
      if (nFiles) { File f = SD.open(files[0]); if (f) { f.read(k.rd, 512); f.close(); } }
    }
    k.put();
    delay(5);
  }
  heap_caps_get_info(&b, CAPS);
  heapLine("after 100 rounds");
  Serial.printf("[SPIKE] heap test: failed rounds=%d, lowest free=%u, lowest largest block=%u, free %u -> %u, largest %u -> %u\n", fails,
                (unsigned)lowFree, (unsigned)lowLargest, (unsigned)a.total_free_bytes, (unsigned)b.total_free_bytes, (unsigned)a.largest_free_block,
                (unsigned)b.largest_free_block);
}

// ---- 2. decode + resample CPU
struct Stats {
  uint32_t frames = 0, errors = 0, maxUs = 0, bins[40] = {};
  uint64_t sumUs = 0, audioSamples = 0;
  int sr = 0, br = 0, ch = 0, ver = 0;
  uint32_t sdMaxUs = 0;
};

static void decodeFile(const char* path, Stats& st) {
  Kit k;
  if (!k.get()) { Serial.println("[SPIKE] decode: no memory for the kit"); k.put(); return; }
  File f = SD.open(path);
  if (!f) { Serial.printf("[SPIKE] cannot open %s\n", path); k.put(); return; }
  // ID3v2 header: skip it
  uint8_t h[10];
  if (f.read(h, 10) == 10 && !memcmp(h, "ID3", 3)) f.seek(10 + (((h[6] & 0x7f) << 21) | ((h[7] & 0x7f) << 14) | ((h[8] & 0x7f) << 7) | (h[9] & 0x7f)));
  else f.seek(0);
  int have = 0;
  bool rsReady = false;
  bool eof = false;
  uint32_t t0 = millis();
  while (!eof || have > 0) {
    if (have < (int)INBUF - 512 && !eof) {  // refill
      int64_t a = esp_timer_get_time();
      int n = f.read(k.in + have, INBUF - have);
      uint32_t us = (uint32_t)(esp_timer_get_time() - a);
      if (us > st.sdMaxUs) st.sdMaxUs = us;
      if (n <= 0) eof = true; else have += n;
    }
    int off = MP3FindSyncWord(k.in, have);
    if (off < 0) { have = 0; if (eof) break; continue; }
    unsigned char* p = k.in + off;
    int left = have - off;
    int64_t a = esp_timer_get_time();
    int err = MP3Decode(k.dec, &p, &left, k.pcm, 0);
    int64_t b = esp_timer_get_time();
    memmove(k.in, p, left);
    have = left;
    if (err == ERR_MP3_INDATA_UNDERFLOW && eof) break;
    if (err) { st.errors++; if (err == ERR_MP3_INDATA_UNDERFLOW) continue; continue; }
    MP3FrameInfo fi;
    MP3GetLastFrameInfo(k.dec, &fi);
    if (!rsReady) {
      if (fi.samprate < 16000 || fi.samprate > 48000) { Serial.printf("[SPIKE] unsupported rate %d\n", fi.samprate); break; }
      k.rs->begin(fi.samprate);
      st.sr = fi.samprate; st.br = fi.bitrate; st.ch = fi.nChans; st.ver = fi.version;
      rsReady = true;
    }
    // mono mix into the resampler's input, then resample; the stereo buffer is reused for the 48 kHz output
    int n = fi.outputSamps / fi.nChans;
    int16_t* in = k.rs->in();
    if (fi.nChans == 2) for (int i = 0; i < n; i++) in[i] = (int16_t)(((int32_t)k.pcm[2 * i] + k.pcm[2 * i + 1]) >> 1);
    else memcpy(in, k.pcm, n * sizeof(int16_t));
    size_t m = k.rs->process(n, k.pcm, 1152 * 2);
    (void)m;
    int64_t c = esp_timer_get_time();
    uint32_t dec = (uint32_t)(b - a), total = (uint32_t)(c - a);
    (void)dec;
    st.frames++;
    st.sumUs += total;
    st.audioSamples += n;
    if (total > st.maxUs) st.maxUs = total;
    st.bins[total / 500 < 39 ? total / 500 : 39]++;
    if ((millis() - t0) > 120000) break;  // two minutes of a file is enough
  }
  f.close();
  k.put();
}

struct Job { const char* path; Stats* st; volatile bool done; };
static void decodeTask(void* a) {
  Job* j = (Job*)a;
  decodeFile(j->path, *j->st);
  Serial.printf("[SPIKE] decode task stack never used: %u of 8192 bytes\n", (unsigned)uxTaskGetStackHighWaterMark(nullptr));
  j->done = true;
  vTaskDelete(nullptr);
}

static void cpuTest(const char* path, int core) {
  static Stats st;
  st = Stats();
  Job j{path, &st, false};
  xTaskCreatePinnedToCore(decodeTask, "dec", 8192, &j, 3, nullptr, core);
  while (!j.done) delay(20);
  if (!st.frames) { Serial.printf("[SPIKE] cpu %s core%d: no frames decoded (errors=%u)\n", path, core, (unsigned)st.errors); return; }
  uint32_t acc = 0, p99 = 0;
  for (int i = 0; i < 40; i++) { acc += st.bins[i]; if (acc >= st.frames * 99 / 100) { p99 = (i + 1) * 500; break; } }
  double audioUs = (double)st.audioSamples * 1e6 / st.sr;
  Serial.printf("[SPIKE] cpu core%d %s: MPEG%d %d Hz %d kbps %dch frames=%u errors=%u avg=%.0f us max=%u us p99<%u us  => %.1f %% of one core at real time "
                "(decode+mix+resample; frame = %.1f ms of audio); worst frame / frame time = %.0f %%\n",
                core, path, st.ver == 0 ? 1 : st.ver == 1 ? 2 : 25, st.sr, st.br / 1000, st.ch, (unsigned)st.frames, (unsigned)st.errors,
                (double)st.sumUs / st.frames, (unsigned)st.maxUs, (unsigned)p99, 100.0 * st.sumUs / audioUs, audioUs / st.frames / 1000.0,
                100.0 * st.maxUs / (audioUs / st.frames));
}

// ---- 3. SD latency at the file's own pace
static void sdTest(const char* path) {
  File f = SD.open(path);
  if (!f) return;
  size_t size = f.size();
  uint8_t* buf = (uint8_t*)heap_caps_malloc(READ_BLOCK, CAPS);
  if (!buf) { f.close(); return; }
  uint32_t bins[8] = {};  // <2, <5, <10, <20, <50, <100, <200, >=200 ms
  static const uint32_t lim[7] = {2, 5, 10, 20, 50, 100, 200};
  uint32_t maxMs10 = 0, reads = 0;
  size_t done = 0;
  uint32_t start = millis();
  double bytesPerSec = 16000;  // 128 kbps; real rate is not known without parsing, so this tests the card, not the file
  while (millis() - start < 60000 && done < size) {
    int64_t a = esp_timer_get_time();
    int n = f.read(buf, READ_BLOCK);
    uint32_t us = (uint32_t)(esp_timer_get_time() - a);
    if (n <= 0) break;
    done += n; reads++;
    uint32_t ms10 = us / 100;  // in 0.1 ms
    if (ms10 > maxMs10) maxMs10 = ms10;
    int b = 0;
    while (b < 7 && us / 1000 >= lim[b]) b++;
    bins[b]++;
    uint32_t due = start + (uint32_t)(done * 1000.0 / bytesPerSec);
    while ((int32_t)(due - millis()) > 0) delay(1);
  }
  f.close();
  heap_caps_free(buf);
  Serial.printf("[SPIKE] sd %s: %u reads of %u bytes in %lu ms, slowest %.1f ms; <2:%u <5:%u <10:%u <20:%u <50:%u <100:%u <200:%u >=200:%u\n", path,
                (unsigned)reads, (unsigned)READ_BLOCK, (unsigned long)(millis() - start), maxMs10 / 10.0, (unsigned)bins[0], (unsigned)bins[1],
                (unsigned)bins[2], (unsigned)bins[3], (unsigned)bins[4], (unsigned)bins[5], (unsigned)bins[6], (unsigned)bins[7]);
}

static void runAll(bool heapOnly) {
  heapTest();
  if (heapOnly) return;
  for (int i = 0; i < nFiles; i++) {
    cpuTest(files[i], 0);
    cpuTest(files[i], 1);
    sdTest(files[i]);
    heapLine("after file");
  }
  Serial.println("[SPIKE] done");
}

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.drawString("mp3_spike: see serial", 4, 4);
  delay(1500);
  Serial.println("[SPIKE] mp3_spike start");
  heapLine("after M5Cardputer.begin");
  int64_t a = esp_timer_get_time();
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  bool sd = SD.begin(SD_CS, SPI, 25000000);
  Serial.printf("[SPIKE] SD.begin: %s in %lld ms, card %llu MB\n", sd ? "ok" : "FAILED", (long long)((esp_timer_get_time() - a) / 1000),
                sd ? (unsigned long long)(SD.cardSize() / 1048576) : 0ULL);
  heapLine("after SD.begin");
  if (sd) {
    scan("/");
    scan("/music");
  }
  Serial.printf("[SPIKE] %d mp3 file(s)\n", nFiles);
  for (int i = 0; i < nFiles; i++) Serial.printf("[SPIKE]   %s\n", files[i]);
  // Ballast: bring the free heap down to what KeySpeaker has (one block; the real heap is more fragmented, which stage 3 checks).
  uint32_t freeNow = heap_caps_get_free_size(CAPS);
  if (freeNow > BALLAST_FREE_TARGET) ballast = heap_caps_malloc(freeNow - BALLAST_FREE_TARGET, CAPS);
  heapLine("with ballast");
  runAll(false);
}

void loop() {
  while (Serial.available()) {
    int c = Serial.read();
    if (c == 'g') runAll(false);
    if (c == 'h') runAll(true);
  }
  delay(20);
}
