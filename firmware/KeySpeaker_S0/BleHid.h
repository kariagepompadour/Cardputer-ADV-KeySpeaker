/*
  BleHid.h — KeySpeaker (USB Speaker & BLE Keyboard): the BLE keyboard and mouse (S5-1, design 5.2)

  Ported from Media Jog (Cardputer-ADV-Media-Jog, firmware MediaJog_2a_5: its BLE backend of BLE-1 r4 and its fixed
  GATT table of BLE-B1), with its fixes kept as they are and its diagnostic trace left out. Plan A of design 5.2: the
  same BLE identity (the chip's address) and the same GATT table, attribute for attribute, as Media Jog, so a computer
  paired in either app works in both. NimBLE keeps the pairings in one store the two apps share.

  - The GATT table is fixed (NimBLE's C API, not the library's objects): the library orders services by object
    address, so its handles could change between builds, which a paired computer takes for a different device.
  - Device Information, one HID service (keyboard input and output, report ID 1; mouse input, report ID 2) and a
    battery level fixed at 100. Each paired computer takes 3 of the 8 subscription records NimBLE keeps: at most 2
    paired computers (Media Jog's limit).
  - "Just Works" pairing with bonding (no passkey).
  - NimBLE can deliver the subscriptions (CCCD restore) of a bonded computer, and its encryption, before CONNECT; a
    live handle is then adopted from those events (adoptConnHandle). A connection that fails before CONNECT drops
    such an adoption (appConnectFailed).
  - (S5-9) macOS creates the HID device only if the link is already encrypted when it has read the HID service; it
    does not try again once pairing finishes ("Not creating HID device as the link is not encrypted", then
    "peerDidCompletePairing" 1.3 s later, in the Mac's log). So the Cardputer asks for encryption the moment a
    computer connects, and the HID service's characteristics and descriptors can be read only over an encrypted link
    (the computer pairs first, then reads). The table's layout and handles are unchanged; only permissions differ.
  - Ready = connected, encrypted and the keyboard subscribed. On becoming ready, an all-keys-up report and a released
    mouse are sent, and the all-up once more 100 ms later (there is no delivery confirmation).
  - The BLE stack starts the first time BLE is wanted and is never shut down; stop() only ends advertising and the
    link (pairings are kept).
*/
#pragma once
#include <atomic>
#ifndef KS_BLE
#define KS_BLE 1
#endif
#if !KS_BLE  // (S5-5) BLE left out of the build, to compare: the same calls, doing nothing
namespace BleHid {
static bool on = false;
static bool started = false;
static bool ready() { return false; }
static bool sendKeyboard(const uint8_t*) { return false; }
static bool sendMouse(uint8_t, int8_t, int8_t, int8_t = 0) { return false; }
static bool service(uint32_t) { return false; }
static uint32_t sentOk = 0, sentNg = 0;
static int lastRc = 0;
static bool kbSub() { return false; }
static bool mouseSub() { return false; }
static void start() {}
static void stop() {}
}  // namespace BleHid
#else
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLESecurity.h>
#include <host/ble_gatt.h>
#include <host/ble_hs_mbuf.h>

namespace BleHid {

static constexpr const char* DEVICE_NAME = "KeySpeaker";

// Report map = TinyUSB's keyboard (report ID 1) then mouse (report ID 2), byte for byte as Media Jog's.
static constexpr uint8_t REPORT_ID_KEYBOARD = 1, REPORT_ID_MOUSE = 2;
static const uint8_t HID_REPORT_MAP[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01,              // keyboard, report ID 1
    0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01,  //   modifiers
    0x95, 0x08, 0x75, 0x01, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01,                          //   reserved byte
    0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x95, 0x05, 0x75, 0x01,  //   LEDs (output)
    0x91, 0x02, 0x95, 0x01, 0x75, 0x03, 0x91, 0x01,
    0x05, 0x07, 0x19, 0x00, 0x2A, 0xFF, 0x00, 0x15, 0x00,        //   6 key codes
    0x26, 0xFF, 0x00, 0x95, 0x06, 0x75, 0x08, 0x81, 0x00,
    0xC0,
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x85, 0x02,              // mouse, report ID 2
    0x09, 0x01, 0xA1, 0x00,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x05, 0x15, 0x00, 0x25, 0x01,  //   5 buttons
    0x95, 0x05, 0x75, 0x01, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x03, 0x81, 0x01,                          //   padding
    0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x15, 0x81, 0x25, 0x7F,  //   X, Y
    0x75, 0x08, 0x95, 0x02, 0x81, 0x06,
    0x09, 0x38, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x01,  //   wheel
    0x81, 0x06,
    0x05, 0x0C, 0x0A, 0x38, 0x02, 0x15, 0x81, 0x25, 0x7F,        //   pan
    0x75, 0x08, 0x95, 0x01, 0x81, 0x06,
    0xC0, 0xC0,
};

// A characteristic value of the table: its value handle (written by ble_gatts_start(); 0xFFFF before) and its
// current value (read by the host, written by notify() or by the host). Guarded by valueMux.
struct GattValue {
  uint16_t handle;
  uint8_t len;
  uint8_t maxLen;
  uint8_t data[8];
};
static GattValue kbInput = {0xFFFF, 8, 8, {0}};          // keyboard input: all keys up
static GattValue kbOutput = {0xFFFF, 1, 1, {0}};         // keyboard LEDs, written by the host, ignored
static GattValue mouseInput = {0xFFFF, 5, 5, {0}};       // mouse input: released
static GattValue protocolMode = {0xFFFF, 1, 1, {0x01}};  // Report Protocol
static GattValue controlPoint = {0xFFFF, 1, 1, {0}};     // HID Control Point, ignored
static GattValue battery = {0xFFFF, 1, 1, {100}};        // fixed; the battery is not measured
static portMUX_TYPE valueMux = portMUX_INITIALIZER_UNLOCKED;

// Written by the NimBLE host task, read by loop().
static std::atomic<bool> connected{false};
static std::atomic<uint16_t> connHandle{0xFFFF};
static std::atomic<uint32_t> connGen{0};  // +1 on every connect and disconnect
static std::atomic<bool> kbSubscribed{false}, mouseSubscribed{false};

static BLEServer* server = nullptr;
static bool started = false;  // the stack is up (once)
static bool on = false;       // BLE is wanted: advertising or connected

// ---- Callbacks (NimBLE host task): flags only; loop() acts on them.
static bool connAlive(uint16_t h) {
  if (h == 0xFFFF) return false;
  ble_gap_conn_desc d;
  return ble_gap_conn_find(h, &d) == 0;
}

static bool adoptConnHandle(uint16_t h) {
  uint16_t cur = connHandle;
  if (cur == h) return false;
  if (cur != 0xFFFF && connAlive(cur)) return false;  // another live connection is current
  if (!connAlive(h)) return false;
  connHandle = h;
  kbSubscribed = false;  // nothing of an earlier connection is carried over
  mouseSubscribed = false;
  return true;
}

static void appConnect(uint16_t h) {
  if (connHandle != h) {  // keep the flags only if this handle was adopted from an early subscribe event
    kbSubscribed = false;
    mouseSubscribed = false;
  }
  connHandle = h;
  connected = true;
  connGen++;
}

static void appConnectFailed(uint16_t h) {
  if (connected || connHandle != h) return;
  connHandle = 0xFFFF;
  kbSubscribed = false;
  mouseSubscribed = false;
}

static void appDisconnect(uint16_t h) {
  if (h != connHandle) return;  // not the current connection
  connected = false;
  kbSubscribed = false;
  mouseSubscribed = false;
  connHandle = 0xFFFF;
  connGen++;  // advertising restarts by itself (advertiseOnDisconnect)
}

static void handleSubscribe(std::atomic<bool>* subscribed, uint16_t h, bool notify) {
  adoptConnHandle(h);
  if (h != connHandle) return;  // another connection's
  *subscribed = notify;
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*, ble_gap_conn_desc* desc) override {
    appConnect(desc->conn_handle);
    // (S5-9) encrypt at once (pairing the first time, the stored keys after), before the computer reads the HID part
    if (!desc->sec_state.encrypted) ble_gap_security_initiate(desc->conn_handle);
  }
  void onDisconnect(BLEServer*, ble_gap_conn_desc* desc) override { appDisconnect(desc->conn_handle); }
};

class SecurityCallbacks : public BLESecurityCallbacks {
  bool onSecurityRequest() override { return true; }
  void onAuthenticationComplete(ble_gap_conn_desc* desc) override {
    Serial.printf("[BLE] authentication %s\n", desc->sec_state.encrypted ? "complete" : "failed");
  }
  uint32_t onPassKeyRequest() override { return 0; }
  void onPassKeyNotify(uint32_t) override {}
  bool onConfirmPIN(uint32_t) override { return true; }
};

static ble_gap_event_listener subscribeListener, connectListener;

static int onSubscribeEvent(ble_gap_event* ev, void*) {
  if (ev->type != BLE_GAP_EVENT_SUBSCRIBE) return 0;
  std::atomic<bool>* flag = ev->subscribe.attr_handle == kbInput.handle      ? &kbSubscribed
                            : ev->subscribe.attr_handle == mouseInput.handle ? &mouseSubscribed
                                                                             : nullptr;
  if (!flag) return 0;  // battery level: ignored
  ble_gap_conn_desc desc;
  if (ble_gap_conn_find(ev->subscribe.conn_handle, &desc) != 0) return 0;
  handleSubscribe(flag, desc.conn_handle, ev->subscribe.cur_notify);
  return 0;
}

static int onConnectEvent(ble_gap_event* ev, void*) {  // a connection that failed before CONNECT
  if (ev->type == BLE_GAP_EVENT_CONNECT && ev->connect.status != 0) appConnectFailed(ev->connect.conn_handle);
  return 0;
}

// ---- The fixed GATT table (Media Jog's BLE-B1, attribute for attribute).
static const ble_uuid16_t UUID_DIS = BLE_UUID16_INIT(0x180A), UUID_HID = BLE_UUID16_INIT(0x1812),
                          UUID_BAS = BLE_UUID16_INIT(0x180F), UUID_MANUFACTURER = BLE_UUID16_INIT(0x2A29),
                          UUID_PNP_ID = BLE_UUID16_INIT(0x2A50), UUID_HID_INFO = BLE_UUID16_INIT(0x2A4A),
                          UUID_REPORT_MAP = BLE_UUID16_INIT(0x2A4B), UUID_HID_CONTROL = BLE_UUID16_INIT(0x2A4C),
                          UUID_REPORT = BLE_UUID16_INIT(0x2A4D), UUID_PROTOCOL_MODE = BLE_UUID16_INIT(0x2A4E),
                          UUID_BATTERY_LEVEL = BLE_UUID16_INIT(0x2A19), UUID_REPORT_REF = BLE_UUID16_INIT(0x2908),
                          UUID_PRESENTATION = BLE_UUID16_INIT(0x2904);

struct GattConst {
  const uint8_t* data;
  uint16_t len;
};
static const char MANUFACTURER_NAME[] = "M5Stack Cardputer";
static const uint8_t PNP_ID[] = {0x02, 0x30, 0x3A, 0x10, 0x01, 0x01, 0x00};
static const uint8_t HID_INFO[] = {0x11, 0x01, 0x00, 0x01};
static const uint8_t REF_KB_INPUT[] = {REPORT_ID_KEYBOARD, 0x01};
static const uint8_t REF_KB_OUTPUT[] = {REPORT_ID_KEYBOARD, 0x02};
static const uint8_t REF_MOUSE_INPUT[] = {REPORT_ID_MOUSE, 0x01};
static const uint8_t BATTERY_FORMAT[] = {0x04, 0x00, 0xAD, 0x27, 0x01, 0x00, 0x00};
static const GattConst C_MANUFACTURER = {(const uint8_t*)MANUFACTURER_NAME, sizeof(MANUFACTURER_NAME) - 1};
static const GattConst C_PNP_ID = {PNP_ID, sizeof PNP_ID}, C_HID_INFO = {HID_INFO, sizeof HID_INFO},
                       C_REPORT_MAP = {HID_REPORT_MAP, sizeof HID_REPORT_MAP},
                       C_REF_KB_INPUT = {REF_KB_INPUT, sizeof REF_KB_INPUT},
                       C_REF_KB_OUTPUT = {REF_KB_OUTPUT, sizeof REF_KB_OUTPUT},
                       C_REF_MOUSE_INPUT = {REF_MOUSE_INPUT, sizeof REF_MOUSE_INPUT},
                       C_BATTERY_FORMAT = {BATTERY_FORMAT, sizeof BATTERY_FORMAT};

static int constAccess(uint16_t, uint16_t, ble_gatt_access_ctxt* ctxt, void* arg) {
  const GattConst* v = (const GattConst*)arg;
  if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR && ctxt->op != BLE_GATT_ACCESS_OP_READ_DSC) return BLE_ATT_ERR_UNLIKELY;
  return os_mbuf_append(ctxt->om, v->data, v->len) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int valueAccess(uint16_t, uint16_t, ble_gatt_access_ctxt* ctxt, void* arg) {
  GattValue* v = (GattValue*)arg;
  uint8_t buf[sizeof(v->data)];
  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    portENTER_CRITICAL(&valueMux);
    uint8_t n = v->len;
    memcpy(buf, v->data, n);
    portEXIT_CRITICAL(&valueMux);
    return os_mbuf_append(ctxt->om, buf, n) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    uint16_t n = OS_MBUF_PKTLEN(ctxt->om);
    if (n == 0 || n > v->maxLen) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    if (ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &n) != 0) return BLE_ATT_ERR_UNLIKELY;
    portENTER_CRITICAL(&valueMux);
    memcpy(v->data, buf, n);
    v->len = (uint8_t)n;
    portEXIT_CRITICAL(&valueMux);
    return 0;
  }
  return BLE_ATT_ERR_UNLIKELY;
}

#define KS_DSC(uuid_, arg_) \
  {.uuid = &(uuid_).u, .att_flags = BLE_ATT_F_READ | BLE_ATT_F_READ_ENC, .min_key_size = 0, .access_cb = constAccess, \
   .arg = (void*)(arg_)}
#define KS_DSC_END {.uuid = nullptr, .att_flags = 0, .min_key_size = 0, .access_cb = nullptr, .arg = nullptr}
static ble_gatt_dsc_def kbInputDscs[] = {KS_DSC(UUID_REPORT_REF, &C_REF_KB_INPUT), KS_DSC_END};
static ble_gatt_dsc_def kbOutputDscs[] = {KS_DSC(UUID_REPORT_REF, &C_REF_KB_OUTPUT), KS_DSC_END};
static ble_gatt_dsc_def mouseInputDscs[] = {KS_DSC(UUID_REPORT_REF, &C_REF_MOUSE_INPUT), KS_DSC_END};
static ble_gatt_dsc_def batteryDscs[] = {KS_DSC(UUID_PRESENTATION, &C_BATTERY_FORMAT), KS_DSC_END};

#define KS_CHR(uuid_, cb_, arg_, dscs_, flags_, valHandle_)                                \
  {.uuid = &(uuid_).u, .access_cb = (cb_), .arg = (void*)(arg_), .descriptors = (dscs_), \
   .flags = (flags_), .min_key_size = 0, .val_handle = (valHandle_), .cpfd = nullptr}
#define KS_CHR_END {.uuid = nullptr, .access_cb = nullptr, .arg = nullptr, .descriptors = nullptr, \
                    .flags = 0, .min_key_size = 0, .val_handle = nullptr, .cpfd = nullptr}
static const ble_gatt_chr_def DIS_CHRS[] = {
    KS_CHR(UUID_MANUFACTURER, constAccess, &C_MANUFACTURER, nullptr, BLE_GATT_CHR_F_READ, nullptr),
    KS_CHR(UUID_PNP_ID, constAccess, &C_PNP_ID, nullptr, BLE_GATT_CHR_F_READ, nullptr),
    KS_CHR_END,
};
static constexpr ble_gatt_chr_flags R = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_READ_ENC;
static const ble_gatt_chr_def HID_CHRS[] = {
    // (S5-9) every HID attribute only over an encrypted link: R = read, W = write
    KS_CHR(UUID_HID_INFO, constAccess, &C_HID_INFO, nullptr, R, nullptr),
    KS_CHR(UUID_REPORT_MAP, constAccess, &C_REPORT_MAP, nullptr, R, nullptr),
    KS_CHR(UUID_HID_CONTROL, valueAccess, &controlPoint, nullptr, BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE_ENC,
           &controlPoint.handle),
    KS_CHR(UUID_PROTOCOL_MODE, valueAccess, &protocolMode, nullptr, R | BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE_ENC,
           &protocolMode.handle),
    KS_CHR(UUID_REPORT, valueAccess, &kbInput, kbInputDscs, R | BLE_GATT_CHR_F_NOTIFY, &kbInput.handle),
    KS_CHR(UUID_REPORT, valueAccess, &kbOutput, kbOutputDscs,
           R | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE_ENC, &kbOutput.handle),
    KS_CHR(UUID_REPORT, valueAccess, &mouseInput, mouseInputDscs, R | BLE_GATT_CHR_F_NOTIFY, &mouseInput.handle),
    KS_CHR_END,
};
static const ble_gatt_chr_def BAS_CHRS[] = {
    KS_CHR(UUID_BATTERY_LEVEL, valueAccess, &battery, batteryDscs, BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
           &battery.handle),
    KS_CHR_END,
};
static const ble_gatt_svc_def SERVICES[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &UUID_DIS.u, .includes = nullptr, .characteristics = DIS_CHRS},
    {.type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &UUID_HID.u, .includes = nullptr, .characteristics = HID_CHRS},
    {.type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &UUID_BAS.u, .includes = nullptr, .characteristics = BAS_CHRS},
    {.type = 0, .uuid = nullptr, .includes = nullptr, .characteristics = nullptr},
};

// ---- Sending: to the current connection, only while that report's notifications are on.
static uint32_t sentOk = 0, sentNg = 0;  // (S5-6) reports accepted / not, for the screen
static int lastRc = 0;                   // the last refusal: -1 no link, -2 not subscribed, else NimBLE's rc
static bool kbSub() { return kbSubscribed; }
static bool mouseSub() { return mouseSubscribed; }

static bool notify(GattValue* c, const void* data, size_t len) {
  portENTER_CRITICAL(&valueMux);
  memcpy(c->data, data, len);  // the value a host read returns
  c->len = (uint8_t)len;
  portEXIT_CRITICAL(&valueMux);
  bool sub = c == &mouseInput ? mouseSubscribed.load() : kbSubscribed.load();
  uint16_t h = connHandle;
  int rc = !connected || h == 0xFFFF ? -1 : !sub ? -2 : 0;
  if (rc == 0) {
    os_mbuf* om = ble_hs_mbuf_from_flat(data, (uint16_t)len);
    rc = om ? ble_gatts_notify_custom(h, c->handle, om) : BLE_HS_ENOMEM;  // consumes om
  }
  if (rc == 0) {
    sentOk++;
    return true;
  }
  sentNg++;
  lastRc = rc;
  Serial.printf("[BLE] notify not accepted: rc %d\n", rc);
  return false;
}

// ---- Link state (loop() only).
enum State : uint8_t { OFF, ADVERTISING, CONNECTED, READY };
static bool linkReady = false;
static uint32_t seenGen = 0, repeatAt = 0;
static bool repeatPending = false;

static bool ready() { return on && linkReady; }

static State state() {
  if (!on) return OFF;
  if (linkReady) return READY;
  return connected ? CONNECTED : ADVERTISING;
}

static bool sendKeyboard(const uint8_t report[8]) { return ready() && notify(&kbInput, report, 8); }

static bool sendMouse(uint8_t buttons, int8_t x, int8_t y, int8_t wheel = 0) {
  if (!ready() || !mouseSubscribed) return false;
  const uint8_t r[5] = {buttons, (uint8_t)x, (uint8_t)y, (uint8_t)wheel, 0};
  return notify(&mouseInput, r, sizeof r);
}

static void allUp() {
  static const uint8_t KB_UP[8] = {0}, MOUSE_UP[5] = {0};
  notify(&kbInput, KB_UP, sizeof KB_UP);
  if (mouseSubscribed) notify(&mouseInput, MOUSE_UP, sizeof MOUSE_UP);
}

// Called often from loop(): tracks ready (connected, encrypted, keyboard subscribed) and starts each ready period
// with everything released. Returns true when ready changed.
static bool service(uint32_t now) {
  if (!started) return false;
  bool encrypted = false;
  if (connected) {
    ble_gap_conn_desc d;
    if (ble_gap_conn_find(connHandle, &d) == 0) encrypted = d.sec_state.encrypted;
  }
  bool r = on && connected && encrypted && kbSubscribed;
  uint32_t gen = connGen;
  bool changed = false;
  if (gen != seenGen) {  // a disconnect and reconnect between two calls is still a loss
    seenGen = gen;
    if (linkReady) linkReady = false, changed = true;
  }
  if (linkReady != r) {
    linkReady = r;
    changed = true;
    Serial.printf("[BLE] link %s\n", r ? "ready" : "lost");
    if (r) {
      allUp();
      repeatPending = true;
      repeatAt = now + 100;
    } else {
      repeatPending = false;
    }
  }
  if (repeatPending && (int32_t)(now - repeatAt) >= 0) {
    repeatPending = false;
    if (linkReady) allUp();
  }
  return changed;
}

static void (*stepHook)(const char*) = nullptr;  // (diagnostics) called after each step of begin() with its name
#define KS_BLE_STEP(name) do { if (stepHook) stepHook(name); } while (0)

static void begin() {
  KS_BLE_STEP("ble before init");
  BLEDevice::init(DEVICE_NAME);
  KS_BLE_STEP("ble after BLEDevice::init");
  ble_gap_event_listener_register(&subscribeListener, onSubscribeEvent, nullptr);
  ble_gap_event_listener_register(&connectListener, onConnectEvent, nullptr);
  BLESecurity* security = new BLESecurity();
  security->setCapability(ESP_IO_CAP_NONE);
  security->setAuthenticationMode(true, false, true);  // bonding, no MITM, LE Secure Connections
  BLEDevice::setSecurityCallbacks(new SecurityCallbacks());
  server = BLEDevice::createServer();
  KS_BLE_STEP("ble after createServer");
  server->setCallbacks(new ServerCallbacks());
  server->advertiseOnDisconnect(true);
  // The table is queued after createServer() (which resets NimBLE's table) and registered by the one start().
  int rc = ble_gatts_count_cfg(SERVICES);
  if (rc == 0) rc = ble_gatts_add_svcs(SERVICES);
  server->start();
  KS_BLE_STEP("ble after server->start");
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->setAppearance(0x03C1);  // HID keyboard
  adv->addServiceUUID(BLEUUID((uint16_t)0x1812));
  adv->setScanResponse(true);
  adv->setMinPreferred(0x06);
  adv->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
  KS_BLE_STEP("ble after startAdvertising");
  Serial.printf("[BLE] advertising as %s; gatt rc %d, handles kb %u mouse %u\n", DEVICE_NAME, rc,
                (unsigned)kbInput.handle, (unsigned)mouseInput.handle);
}

static void start() {
  if (on) return;
  on = true;
  if (!started) {
    begin();  // also starts advertising
    started = true;
    return;
  }
  server->advertiseOnDisconnect(true);
  if (!connected) BLEDevice::startAdvertising();
  Serial.println("[BLE] on");
}

static void stop() {
  if (!on) return;
  on = false;
  linkReady = false;
  repeatPending = false;
  if (!started) return;
  server->advertiseOnDisconnect(false);
  BLEDevice::stopAdvertising();
  uint16_t h = connHandle;
  if (connected && h != 0xFFFF) ble_gap_terminate(h, BLE_ERR_REM_USER_CONN_TERM);
  Serial.println("[BLE] off");
}

}  // namespace BleHid
#endif  // KS_BLE
