# Cardputer ADV KeySpeaker

**Cardputer ADVを、PCで使える小型の「外付けキーボード＆スピーカー」にします。**

[English README](README.md)

> **状態: 設計段階です。** ファームウェアはまだ書いていません。設計は
> [`docs/DESIGN_JA.md`](docs/DESIGN_JA.md) にあります。このリポジトリの内容は、まだ実機で確認していません。

## 発案のきっかけ

RADIO DAYS SDRの開発中、Cardputer ADVの内蔵スピーカーが想像より良い音で鳴ることに気づきました。
本格的な音楽鑑賞用ではありませんが、BGM、ニュース、インターネットラジオ、ポッドキャスト、動画の音声を聴くには十分です。
そしてCardputer ADVには物理キーボードがあります。スピーカーとして使いながら、キーボードもPCへの入力に使えたら面白いはずです。

## 目標

| 接続 | スピーカー | キーボード | PC側 |
| --- | --- | --- | --- |
| **USBケーブル** | USB Audio Class 1スピーカー（48kHzステレオで受信し、モノラルで再生） | USB HIDキーボード＋メディアキー | インストール不要（macOS、Windows、ChromeOS） |
| **ワイヤレス** | Wi‑Fi音声ストリーム | Bluetooth LE HIDキーボード＋メディアキー | キーボードはインストール不要。音声は小さな送信アプリが必要（下記） |

- Fnレイヤーで、Shift、Ctrl、Alt、Command／Windows、カーソル、Fキー、音量、ミュート、再生／一時停止を使えるようにします。
- 画面には、接続状態、音量、ミュート、バッファーの状態、電池残量、警告を表示します。
- 作者の他のCardputer ADVプロジェクトと同じく、本体アプリだけの `.ino.bin` をLauncherから入れる方式にします。

### わかっている制約：ワイヤレス音声

PC側にソフトを入れずにワイヤレスで音声を送ることは、**このハードウェアではできません**。

- ESP32-S3にはClassic Bluetoothがないため、Bluetooth音声（A2DP）は使えません。
- Cardputer ADVにはPSRAMがありません。ESP32向けの既知のAirPlay受信実装は、どれもPSRAMを必要とします。
- WindowsとChromeOSには、PCの音を一般的なWi‑Fiスピーカーへ送る標準の仕組みがありません。

そのため、ワイヤレス音声にはPC側の小さな送信アプリを使います（Windowsを先に、次にmacOS）。
USB音声と、USB・ワイヤレス両方のキーボードは、インストール不要です。

## フォルダー構成

| フォルダー | 内容 |
| --- | --- |
| [`docs/`](docs/) | 設計書。のちに検証記録も置きます |
| [`firmware/`](firmware/) | Cardputer ADVのファームウェア（Arduino、`m5stack:esp32` 3.3.10） |
| [`sender/`](sender/) | ワイヤレス音声用のPC側送信アプリ |

## ハードウェア

- M5Stack **Cardputer ADV**（ESP32-S3FN8、ES8311コーデック、1Wスピーカー、TCA8418キーボード）。初代Cardputerは対象外です。
- USB-C **データ**ケーブル

## ライセンス

[MIT](LICENSE)
