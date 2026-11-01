# KeySpeaker

### USB Speaker & BLE Keyboard for M5Stack Cardputer ADV

**PCにも、スマホにも、ゲーム機にも。カバンに入れておける、小さなスピーカー＆キーボード。**

[English README](README.md)

> **状態: 開発の初期段階です。** MacとWindows 11では、USB-Cケーブル1本で、スピーカーとキーボードの両方が使えるようになりました。十分な音量で鳴り（8時間続けて音切れなし、一時停止と再開も問題なし）、
> キーボードで文字が打て、音量や再生のキーも効きます。USBでつないだiPhone 15でも使え、傾きで動かすマウスも使えます。予備として、ケーブルを抜くとBluetooth LEのキーボードとマウスにもなります（MacとWindows 11で確認。USBに戻すには、電源スイッチを切ってリセットボタンを押します）。
> ファームウェアは [`firmware/KeySpeaker_S0/`](firmware/KeySpeaker_S0/)、設計は [`docs/DESIGN_JA.md`](docs/DESIGN_JA.md)、検証記録は [`docs/S0_TEST_JA.md`](docs/S0_TEST_JA.md) にあります。

## 発案のきっかけ

[RADIO DAYS SDR](https://github.com/kariagepompadour/RADIODAYS-SDR)の開発中、Cardputer ADVの内蔵スピーカーが想像より良い音で鳴ることに気づきました。
本格的な音楽鑑賞用ではありませんが、BGM、ニュース、インターネットラジオ、ポッドキャスト、動画の音声を聴くには十分です。
そしてCardputer ADVには物理キーボードがあります。スピーカーとして使いながら、キーボードも入力に使えたら面白いはずです。

## できるようにすること

- **USB-Cケーブルでつなぐと（基本の使い方）**：ケーブル1本で、スピーカーとキーボードになります。
- **ケーブルを抜くと（予備）**：数秒で自動的に、Bluetooth LEのキーボードとマウスになります。
- **相手の機器には何も入れません。** Cardputerに本体アプリだけの `.ino.bin` をLauncherから入れるだけです。

Bluetooth LE（予備）の制限:

- **USBに戻すとき**：一度抜くと、ケーブルを挿し直してもUSBとして認識されません。ケーブルを挿したまま、電源スイッチを切ってリセットボタンを押してください。キーボード画面に `Off+Reset to USB` と出ます。
- **アプリを切り替えたとき**：[Media Jog](https://github.com/kariagepompadour/Cardputer-ADV-Media-Jog)でペアリングしたパソコンは、KeySpeakerで使うときに一度削除してペアリングし直す必要があります。逆も同じです（Macで確認）。
- つながるのは一度に1台、ペアリングしておけるのは2台までです。

使い方の例:

- スピーカーのないPC（デスクトップPC、Mac mini、モニターなど）につなぐ。
- USB-Cから音声を出せる機器（スマホ、タブレット、デジタルオーディオプレーヤー、ゲーム機）につなぐ。
- ケーブルが届かないところで、予備のBluetooth LEキーボードとマウスとして使う（MacとWindows 11で確認）。
- **USB-Cイヤホンアダプター**として使う：Cardputerのイヤホン端子にイヤホンを挿します（モノラルなので、両耳から同じ音が出ます）。

キーが小さいので、長文ではなく、ちょっとした入力（検索欄、パスワード、ゲーム機の文字入力など）向けです。

## モードとキー

起動すると、スピーカーの網目に付いたかりポムのエンブレムと、「KeySpeaker」の名前が2秒出ます。そのあと `Fn+M` で3つのモードを順に切り替えます（USBのとき）。最後に使ったモードは覚えています。measure mode（計測）と flight mode（フライト）では、キー操作も音もない状態が1分続くと、エンブレムに戻り、下に `h HELP  Fn+M KEYBOARD` と出ます。キーを押すまでそのままです（そのキーは画面を戻すだけです）。音楽が鳴り出すと、エンブレムがスピーカーのコーンのように、大きな音に合わせて震えます。全部の画面の絵は [`docs/SCREENS_JA.md`](docs/SCREENS_JA.md) にあります。

| モード | 画面 | キー |
| --- | --- | --- |
| **measure mode**（計測） | バッファー、時計のずれ、音量とレベルメーター | スピーカーと再生のキー（下） |
| **flight mode**（フライト） | 音楽の上を飛ぶ画面（音量の地形、小さな戦闘機、昭和のグライコ） | スピーカーと再生のキー（下） |
| **keyboard mode**（キーボード） | ゆっくり回る地球と、キーの早見表 | すべてのキーをPCへ送る |

USBがないとき（Bluetooth LEのとき）は、keyboard modeだけになります。

どのモードでも:

| キー | 動作 |
| --- | --- |
| `Fn+M` | 次のモードへ（USBのとき） |
| `Fn+O` | Mac ↔ Windows の切り替え（覚えておく） |

スピーカーと再生のキー（measure mode と flight mode）:

| キー | 動作 |
| --- | --- |
| `[` / `]` / `\` | PCの音量− / 音量＋ / ミュート |
| `Space` | 再生 / 一時停止 |
| `,` / `/` | 前の曲 / 次の曲 |
| `-` / `=` | 持ち上げ量を下げる / 上げる |
| `b` | 低音カットのオン / オフ |
| `r` | カウンターをリセット |
| `v` | measure mode ↔ flight mode |
| `h` | このキーのヘルプ画面を開く / 閉じる（開いたままでもキーは効きます） |
| そのほかのキー | `h` と同じく、ヘルプ画面を開く / 閉じる |

keyboard mode（文字、Shift、Ctrl、Tab、Enter、Delはそのまま送る）:

| キー | 動作 |
| --- | --- |
| `Fn+;` `Fn+,` `Fn+.` `Fn+/` | ↑ ← ↓ → |
| `Fn+Shift+` 同じキー | PgUp / Home / PgDn / End |
| `` Fn+` `` | Esc |
| `Fn+1` … `Fn+=` | F1 … F12 |
| `Fn+Del` | 前方削除 |
| `Fn+[` / `Fn+]` | 英数 / かな。最後に切り替えたほうを、Macのメニューバーと同じ「A」「あ」で画面に出す |
| `alt` / `opt` | Mac: Command / Option、Windows: Alt / Win |

**傾きで動かすマウス**（keyboard mode）。マウスが手元にないときのためのものです。本体の横のボタン（G0）を押している間、画面にマウスのキーが出て、傾けるとカーソルが動きます。傾きは、G0を押した瞬間の持ち方を基準にします。

| G0を押したまま | 動作 |
| --- | --- |
| 傾ける | カーソルが動く（大きく傾けるほど速く） |
| `A` か `,` | 左クリック（キーを押している間は押したまま。傾ければドラッグ） |
| `D` か `/` | 右クリック |
| `E` か `;` / `Z` か `.` | ↑ / ↓ の矢印キー（メニューの移動、スクロール） |
| `S` | Enter |

**ジョグダイヤル**（なくても使えます）: [Media Jog](https://github.com/kariagepompadour/Cardputer-ADV-Media-Jog)と同じ、M5StackのUnit Encoder（U135）をGroveポートにつなぎます。measure modeとflight modeでは、回すとPCの音量、押すと再生 / 一時停止。keyboard modeでは ← → とEnterを送ります。つなぐのは、Cardputerの電源を切った状態で。

## 対象範囲

| 区分 | 機器 |
| --- | --- |
| **基本の対象** | macOS、WindowsとのUSB接続（スピーカー＋キーボード） |
| **予備** | Bluetooth LEのキーボードとマウス（macOS、Windows） |
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
KeySpeakerには、設定がいりません。つなげば、スピーカーとキーボードになります。

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
