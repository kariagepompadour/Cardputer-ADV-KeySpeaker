# 設計書：Cardputer ADV KeySpeaker

作成日: 2026年10月29日　状態: 設計案（合意前）。実機では何も確認していません。

## 1. 目的

M5Stack Cardputer ADVを、PCで使える小型の「外付けキーボード＆スピーカー」にする。

- USB接続時：有線のキーボード＆スピーカー（ケーブル1本、PC側ソフトなし）
- USB未接続時：ワイヤレスのキーボード＆スピーカー
- 対象OS：macOS、Windows、ChromeOS

優先順位は、手軽さ、音切れの少なさ、長時間の安定、聞き取りやすさ、音量とミュートの操作。デザインより基本機能と安定性を優先する。

RADIO DAYS SDRの追加機能ではなく、独立した新規プロジェクトとする。既存リポジトリは変更しない。

## 2. 実現可能性のまとめ

| 希望 | 判定 | 方法 |
|---|---|---|
| USB：スピーカー（ドライバー不要、全OS） | ◎ 実現できる見込み | USB Audio Class 1（UAC1） |
| USB：キーボードと同時に使う | ◎ | UAC1とHIDの複合デバイス |
| ワイヤレス：キーボード | ◎（Media Jogで実証済み） | BLE HID |
| ワイヤレス：スピーカー（PC側ソフトなし、全OS） | ✕ このハードウェアでは不可能 | Wi‑FiとPC側の送信アプリ |

## 3. 調査で分かったこと

### 3.1 USBスピーカーは普段の開発環境のまま作れる

- m5stack:esp32 3.3.10コア（RADIO DAYS SDRと同じ）に `USBAudioCard` クラスが入っている（手元のファイルで確認）。
  2026年4月に本家arduino-esp32へ追加された機能（[issue #12053](https://github.com/espressif/arduino-esp32/issues/12053)、[PR #12466](https://github.com/espressif/arduino-esp32/pull/12466)）。
- ESP32-S3ではUAC1、48kHz、16bit。UAC1はmacOS、Windows、Linux（ChromeOSを含む）の標準ドライバーで動く。
- PRの作者はWindows 11、Ubuntu、Macで動作確認したと報告している。
- 同じコアのUSB HID（キーボード、メディアキー）と組み合わせられる。
- ESP-IDF用のEspressif製UAC部品は「MacとWindowsのどちらか一方にしか対応できない」と公式資料にある（[公式資料](https://docs.espressif.com/projects/esp-iot-solution/en/latest/usb/usb_device/usb_device_uac.html)）。この点でもArduino側が目的に合う。
- 注意：`USBAudioCard` は新しく、PRのレビューでは複合デバイス周りの指摘もあった。

### 3.2 「定期的な音切れ」の原因

- `USBAudioCard` のスピーカーはadaptive方式で、フィードバックエンドポイントがない。
- PCとCardputerのクロック差（±100ppm程度）がバッファーに少しずつたまり、周期的に溢れるか枯れる。48kHzでは1時間あたり約1.7万サンプル（約0.36秒）ずれる計算になる。
- atomic14の「スピーカーだけ音が割れる」という報告（[記事](https://blog.atomic14.com/2025/09/26/esp32-s3-usb-uac.html)）も、この型の問題と一致する。
- ESP32-S3には出力クロックを微調整できるAPLLがない。そのため本体側で、バッファー残量を見て再生速度をごくわずかに伸縮させる（簡易的な非同期サンプルレート変換）。これを最重要の安定化策とする。

### 3.3 ワイヤレス音声の壁

- ESP32-S3はBLE専用で、Classic BluetoothもLE Audioも非対応。A2DPは使えない（[ESP-ADF公式](https://docs.espressif.com/projects/esp-adf/en/latest/solution-center/bluetooth-audio.html)）。
- Cardputer ADV（ESP32-S3FN8）にはPSRAMがない（[仕様](https://docs.m5stack.com/en/core/Cardputer-Adv)）。公開されているESP32のAirPlay受信実装は、どれもPSRAMが必須（[airplay-esp32](https://github.com/rbouteiller/airplay-esp32)、[squeezelite-esp32](https://github.com/sle118/squeezelite-esp32)）。
- WindowsとChromeOSには、ソフトなしで音声をWi‑Fiスピーカーへ送る標準の仕組みがない。

### 3.4 先人の実装

- 「Vibe Cardputer」は検索では見つからなかった（URLがあれば確認する）。
- Cardputer ADVのスピーカーとキーボードを同時に使う例、USBとワイヤレスの両方に対応した例も見つからなかった。この組み合わせは新規の挑戦になる。

## 4. システム構成

```
┌──────────────── Cardputer ADV（1つのファーム）────────────────┐
│ キーボード(TCA8418) → キーマップ/Fnレイヤー → HIDレポート生成    │
│                                   ├→ USB HID   (USBモード)      │
│                                   └→ BLE HID   (ワイヤレスモード)│
│ 音声入力 ┬ USB UAC1 48kHz ステレオ (USBモード)                  │
│          └ Wi‑Fi TCP PCM          (ワイヤレスモード)            │
│     → L+R合成 → 速度微調整 → リングバッファー → ES8311/スピーカー │
│ 接続監視: USB SOFカウンター → モード自動切替                     │
│ 画面: 接続/音量/ミュート/バッファー/電池/警告                     │
└────────────────────────────────────────────────────────────────┘
```

### 4.1 USBモード

- 複合デバイス：UAC1スピーカー、HID（キーボード、Consumer Control）、開発中だけ使うデバッグ用CDC。
- エンドポイント数はESP32-S3の上限内に収まる見込み。
- 音量：本体の `Fn+[` / `Fn+]` でHIDの音量キーをPCへ送る。PCが音量を変えると、UACのFeature Unit経由で本体に値が届き、画面に表示する。音量はPCの値を正としてひとつにそろえる。

### 4.2 ワイヤレスモード

- キーボード：BLE HID。Media JogのNimBLE実装（HIDサービス、ペアリング保存、切替時に押しっぱなしのキーを解放する処理）を移植する。
- 音声：Wi‑FiでPCMを受信する。受信側はRADIO DAYS SDRの `RadioDays_AudioProto.h` と `RadioDays_Playout.h` を流用する。PC側には小さな送信アプリが必要。

| OS | ワイヤレス音声の送信方法 | 難しさ |
|---|---|---|
| Windows | WASAPIループバックでPCの音を取り込む（ドライバー不要） | 低 |
| macOS | Core Audio taps（macOS 14.2以降）。初回だけ録音許可が必要 | 中 |
| ChromeOS | アプリを入れられないため対象外。ブラウザー送信を研究課題として残す | 高 |

- ブラウザー送信：Chromeの画面共有機能（getDisplayMedia）でPCの音を取り込む。WindowsとChromeOSではシステム音声を取り込める。macOSはChrome 141以降とmacOS 14.2以降で可能という報告があるが未確認（[Addpipe](https://blog.addpipe.com/getdisplaymedia-allows-capturing-the-screen-with-system-sounds-on-chrome-on-macos/)）。HTTPSが必須で、LAN内の機器へ直接つなぐ部分に課題が残るため、約束せず検証項目とする。
- 遅延：Wi‑Fiは0.3〜0.5秒程度の見込み。BGM、ラジオ、ポッドキャストには十分だが、動画では口の動きとずれる。USBは数十ミリ秒程度。

### 4.3 モードの切り替え

- 自動：USBのSOF（PCが1ミリ秒ごとに送る信号）があればUSBモード、なければワイヤレスモード。
  - Cardputer ADVはケーブルの抜き差しを電源電圧から検知できないため、RADIO DAYS SDRの `usbLinkService()` で実証済みのSOF監視を使う。
  - 充電器だけをつないだ場合もワイヤレスモードになる。
- 手動：`Fn+B` で切り替える（Media Jogと同じキー）。
- USBモードの間はBLEとWi‑Fiを止める。PCにキーボードが二重に見えるのを防ぎ、電力と電波干渉も減らす。
- USBの構成は起動時に決まり、動作中に変えられない。全機能を常に載せておく設計にする。

## 5. キーボード

TCA8418、4×14＝56キー。キーの押下と解放はイベントFIFOから読む。

### 5.1 そのまま送るキー

通常の文字、Shift、Ctrl、Tab、Enter、Del（Backspace）。

### 5.2 修飾キーのOSプロファイル（本体で選択し、保存する）

| 本体のキー | Macプロファイル | Windows / ChromeOSプロファイル |
|---|---|---|
| `opt` | Option | Win / 検索キー |
| `alt` | Command | Alt |

### 5.3 Fnレイヤー

| キー | 機能 |
|---|---|
| `Fn+; , . /` | ↑ ← ↓ →（キーの印字どおり） |
| `Fn+Shift+矢印` | PgUp / Home / PgDn / End |
| `` Fn+` `` | Esc |
| `Fn+1`…`Fn+=` | F1〜F12 |
| `Fn+Del` | 前方削除 |
| `Fn+[` / `Fn+]` / `Fn+\` | 音量− / 音量＋ / ミュート |
| `Fn+Space` | 再生 / 一時停止 |
| `Fn+N` / `Fn+M` | 前の曲 / 次の曲 |
| `Fn+B` | 接続画面を開く（本体だけで処理し、PCには送らない） |

### 5.4 日本語環境での注意

- キーの印字はUS配列。OS側でこのキーボードを「US（ANSI）配列」に設定してもらう必要がある。
- 英数／かなの切り替えキー（HIDのLANG1／LANG2）は `Fn` との組み合わせに割り当てる案（キーは未定）。

## 6. 音声処理

- PCからは48kHzステレオで受け取り、本体でL+Rを合成してモノラルにする（音割れしないよう余裕を持たせる）。S0ではモノラルとして名乗る方式とも比較する。
- 1Wの小型スピーカーを守り、声を聞き取りやすくするため、低音を軽くカットするフィルターと簡単なリミッターを入れる（選択式）。
- 出力はまずRADIO DAYS SDRで実績のある `M5.Speaker` で始め、足りなければI2Sを直接制御する方式に移る。
- 速度微調整：バッファー残量の目標値を決め、ずれに応じて再生速度を±数百ppmの範囲で連続的に変える（線形補間）。

## 7. 開発環境と既存コード

- Arduino、m5stack:esp32 3.3.10、arduino-cliのビルドスクリプト（RADIO DAYS SDR、Media Jogと同じ構成）。
- ボード設定：USB Mode = USB-OTG（TinyUSB）、Flash 8MB、Partition `default_8MB`。
- 予備案：`USBAudioCard` に不具合があっても、コアに組み込まれたTinyUSBで音声機能（`CONFIG_TINYUSB_AUDIO_ENABLED`）はすでに有効。ESP-IDFへ移行せずに、自前のUACクラスをスケッチ内に持てる。
- 流用するもの（コピーして使い、元のリポジトリは変更しない）：
  - RADIO DAYS SDR：`RadioDays_AudioProto.h`、`RadioDays_Playout.h`、SOF監視、画面描画とキーボード周りの知見
  - Cardputer ADV Media Jog：NimBLE HID、HIDレポートの構成、押しっぱなしのキーを解放する仕組み、NVS（設定の保存）

## 8. Launcherとの互換性

維持できる見込み。

- 本体アプリだけの `.ino.bin` をLauncherのWi‑Fiアップロードで入れる（既存の2プロジェクトと同じ）。
- 「Launcherから起動するとHardware CDCは認識されず、TinyUSBなら動く」というRADIO DAYS SDRの知見が当てはまる。UACとHIDのためにTinyUSBは必須なので問題ない。
- サイズ：RADIO DAYS SDRが1.38MB、Media Jogが0.90MB。両方の機能を合わせても2MB前後と見込む。アプリ領域に収まることはS0で確認する。

## 9. 開発と検証の進め方

| 段階 | 内容 | 合格基準 |
|---|---|---|
| **S0** | UAC1スピーカーだけの試作機。バッファー残量をCDCへ記録する | macOS、Windows 11、ChromeOSで認識される。1時間連続再生でバッファーの溢れ・枯れの推移を測る |
| S1 | 速度微調整を実装する | 8時間連続再生で、耳で分かる音切れがない |
| S2 | UACとHIDキーボード、メディアキーの複合デバイス | 3つのOSで、ケーブル1本で両方が使える |
| S3 | キーマップ全体、OSプロファイル、Fnレイヤー | 全キーをOSごとに確認する |
| S4 | 状態表示画面（実用性を優先） | 表示項目がすべて出る |
| S5 | BLE HIDの移植と、SOFによるモード自動切替 | 抜き差しで切り替わり、押しっぱなしのキーが残らない |
| S6 | Wi‑Fi音声の受信と送信アプリ（Windows → macOSの順） | BLEと同時に使って30分以上安定する。電池の持ちを測る |
| S7 | レトロ機器風の画面デザイン | — |

## 10. 主なリスク

1. `USBAudioCard` がCardputer ADV上で実際にどれだけ安定するか（S0で確認）。
2. PSRAMなしでBLEとWi‑Fi音声を同時に動かしたときのメモリーと電波共存（S6で確認）。ESP-ADFの資料には「PSRAMがないとBluetoothとWi‑Fiを共存させにくい」とある。NimBLEは軽量なので可能性は十分あると考えている。
3. ワイヤレス音声のmacOS送信アプリ（システム音声の取り込み許可）とChromeOSの扱い。
