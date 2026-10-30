# KeySpeaker

### USB Speaker & BLE Keyboard for M5Stack Cardputer ADV

**PCにも、スマホにも、ゲーム機にも。カバンに入れておける、小さなスピーカー＆キーボード。**

[English README](README.md)

> **状態: 開発の初期段階です。** 設計は [`docs/DESIGN_JA.md`](docs/DESIGN_JA.md) にあります。USBスピーカー（[`firmware/KeySpeaker_S0/`](firmware/KeySpeaker_S0/)、時計のずれの補正入り）は、
> Macで十分な音量で鳴り、3時間続けて音切れなし、一時停止と再開も問題ないことを確認しました。次はキーボードです。
> Windowsやそのほかの機器は、まだ確認していません。検証記録は [`docs/S0_TEST_JA.md`](docs/S0_TEST_JA.md) にあります。

## 発案のきっかけ

[RADIO DAYS SDR](https://github.com/kariagepompadour/RADIODAYS-SDR)の開発中、Cardputer ADVの内蔵スピーカーが想像より良い音で鳴ることに気づきました。
本格的な音楽鑑賞用ではありませんが、BGM、ニュース、インターネットラジオ、ポッドキャスト、動画の音声を聴くには十分です。
そしてCardputer ADVには物理キーボードがあります。スピーカーとして使いながら、キーボードも入力に使えたら面白いはずです。

## できるようにすること

- **USB-Cケーブルでつなぐと**：ケーブル1本で、スピーカーとキーボードになります。
- **ケーブルを抜くと**：Bluetooth LEキーボードになります。
- **相手の機器には何も入れません。** Cardputerに本体アプリだけの `.ino.bin` をLauncherから入れるだけです。USBとBluetooth LEは自動で切り替わるので、設定はいりません。

使い方の例:

- スピーカーのないPC（デスクトップPC、Mac mini、モニターなど）につなぐ。
- USB-Cから音声を出せる機器（スマホ、タブレット、デジタルオーディオプレーヤー、ゲーム機）につなぐ。
- Mac、iPhone、iPadなどで、Bluetooth LEキーボードとして使う（ゲーム機やテレビは、使えたらおまけ）。
- **USB-Cイヤホンアダプター**として使う：Cardputerのイヤホン端子にイヤホンを挿します（モノラルなので、両耳から同じ音が出ます）。

`Fn+M` で、画面を「データ表示」「ゲーム画面」「キーボード」と切り替えます。キーが相手の機器へ送られるのはキーボードの画面のときだけで、
Fnレイヤーで、Shift、Ctrl、Alt、Command／Windows、カーソル、Fキーを使えます。ほかの2つの画面では、いくつかのキーで音量、ミュート、再生／一時停止を操作します。
キーが小さいので、長文ではなく、ちょっとした入力向けです。

## 対象範囲

| 区分 | 機器 |
| --- | --- |
| **基本の対象** | macOS、WindowsとのUSB接続（スピーカー＋キーボード）、Bluetooth LEキーボード |
| **確認しながら広げる対象** | iPhone 15以降・USB-CのiPad、Android、USB音声出力のあるデジタルオーディオプレーヤー、ゲーム機、ほかの機器でのBluetooth LEキーボード |
| **対象外** | ChromeOS、ワイヤレス音声、インターネットラジオ、3.5mm端子からの音声入力 |

### ワイヤレス音声がない理由

相手の機器にソフトを入れずに、Cardputer ADVをワイヤレススピーカーにすることはできません。

- ESP32-S3にはClassic Bluetoothがないため、Bluetooth音声（A2DP）は使えません。
- Cardputer ADVにはPSRAMがありません。ESP32向けの既知のAirPlay受信実装は、どれもPSRAMを必要とします。
- Wi‑Fiで音声を送るには、相手の機器に送信アプリが必要になります。

### 1つのアプリに1つの仕事

Cardputer ADVには、すでに別の仕事があります。インターネットラジオは[RADIO DAYS](https://github.com/kariagepompadour/RADIODAYS)、
ジョグダイヤルは[Media Jog](https://github.com/kariagepompadour/Cardputer-ADV-Media-Jog)です。それぞれが別のアプリで、Launcherで切り替えます。
KeySpeakerには、設定がいりません。USBとBluetooth LEは自動で切り替わります。

## 給電

電源につながった機器（PC、Mac mini、ゲーム機）で使うのが一番です。Cardputerも同時に充電されます。
USB-C端子が1つのスマホやタブレットでは、その機器の電池からCardputerに給電されます。給電パススルー（USB PD）対応のUSB-Cハブを使えば、充電しながら使えます。
Cardputerの電源スイッチは**ON**にしてください。

## フォルダー構成

| フォルダー | 内容 |
| --- | --- |
| [`docs/`](docs/) | 設計書と検証記録 |
| [`firmware/`](firmware/) | Cardputer ADVのファームウェア（Arduino、`m5stack:esp32` 3.3.10） |

## ハードウェア

- M5Stack **Cardputer ADV**（ESP32-S3FN8、ES8311コーデック、1Wスピーカー、TCA8418キーボード）。初代Cardputerは対象外です。
- USB-C **データ**ケーブル

## ライセンス

[MIT](LICENSE)
