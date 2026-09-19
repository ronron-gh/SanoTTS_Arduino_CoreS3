# SanoTTS Arduino CoreS3

M5Stack CoreS3で日本語TTS [sanoTTS-jp](https://github.com/ayutaz/sanoTTS-jp)の推論コアをPlatformIO/Arduino環境から動かす検証用プロジェクトです。sanoTTS-jpをESP-IDF環境でM5Stack各種で動かすプロジェクト[SanoTTS-jp-M5StackCoreS3](https://github.com/nnn112358/SanoTTS-jp-M5StackCoreS3)をベースとし、推論のみの検証から日本語文章の読み上げまで、段階的にExampleを作成して確認しました。

## Example一覧

推論のみの検証から日本語文章の読み上げまで、目的に合わせて5つのExampleを選べます。各Exampleは [platformio.ini](platformio.ini) の環境名（env）に対応しています。まず日本語を読み上げて試す場合は、既定の `cores3-text-input` を使用してください。

| 環境名(env) | example | 演算方式・動作 |
| --- | --- | --- |
| `cores3-inference` | [01_inference](examples/01_inference/main.cpp) | W8A32の推論のみ。音声は出ない |
| `cores3-pie` | [02_pie_inference](examples/02_pie_inference/main.cpp) | W8A8＋PIEの推論のみ。音声は出ない |
| `cores3-buffered` | [03_buffered_playback](examples/03_buffered_playback/main.cpp) | W8A8＋PIEで全PCMを蓄積後に再生 |
| `cores3-streaming` | [04_streaming_playback](examples/04_streaming_playback/main.cpp) | W8A8＋PIEで先読み後、計算と並行して再生 |
| `cores3-text-input`（既定） | [05_text_input](examples/05_text_input/main.cpp) | 2M辞書で日本語文章を解析し、W8A8＋PIEでストリーミング再生 |

## 必要なもの

- M5Stack CoreS3と、PCへ接続するUSBデータケーブル。
- PlatformIOを利用できる開発環境。以下のコマンドは、このリポジトリのルートで実行します。
- 日本語入力版で使用する2M辞書。モデルは同梱されていますが、辞書は別途取得します。

ビルド構成はEspressif 32 6.3.2、Arduino Core 2.0.9、M5Unified 0.2.15です。`board = esp32s3box` にCoreS3用の設定を加えています。[platformio.ini](platformio.ini)で確認できます。

## まず動かす

1. リポジトリをcloneします。

   ```sh
   git clone https://github.com/ronron-gh/SanoTTS_Arduino_CoreS3.git
   cd SanoTTS_Arduino_CoreS3
   ```

2. [辞書の取得手順](model/README.md)に従い、`k1-dict-44000-2mb.bin`を`model/`に置きます。ビルド時に形式とSHA-256を検査します。辞書の自動ダウンロードは行いません。
3. CoreS3を接続し、ビルド・書き込みを行います。既定は日本語入力版の`cores3-text-input`です。

   ```sh
   pio run
   pio run -t upload
   pio device monitor -b 115200
   ```

4. モニターを開いた状態でCoreS3をリセットします。「今日は良い天気ですね。」が再生され、`Ready`が表示されたら文章を送れます。

   ```text
   今日は良い天気ですね。
   ```

端末はUTF-8、改行付き送信に設定し、Enterで確定してください。起動ログの`Parse: OK`、推論の`PASS`、`Playback complete`が正常動作の目安です。実際の音声も確認してください。

初回ビルドではPlatformIOが依存パッケージを取得します。モデルと辞書はファームウェアに埋め込まれるため、別の書き込み操作は不要です。

## 日本語入力の使い方

`Ready`が表示されてから、文章を1行ずつ送信します。CR・LF・CRLFに対応しています。

```text
東京都に行きます。
123個あります。
スーパーでパンを買います。
OpenAIで開発します。
/r
```

`/r`＋Enterで、直前に受け付けた文章を再解析・再生します。解析に失敗した文章も直前の文章として保持します。合成中の入力は予約せず破棄するため、次の`Ready`を待ってください。

### 読み方と入力の制約

- 半角英数字は解析前に正規化します。`123`は「ヒャクニジュウサン」、`OpenAI`は「オーピーイーエヌエーアイ」という文字名読みになります。英単語の自然な読みを推定する機能ではありません。
- 小さい辞書を使うため、固有名詞や未知語の読み・アクセントを誤る場合があります。
- 入力と正規化後の文章は、それぞれ最大1,023バイトです。全角化でバイト数が増えるため、短い入力でも正規化後に上限へ達する場合があります。
- 解析・モデルにも長さの上限があります。長文は短く区切ってください。空行、空白だけの行、不正UTF-8、制御文字、上限超過は拒否します。
- 解析失敗時は発声せず次の入力を待ちます。推論や再生の失敗で停止した場合は、リセットしてやり直してください。

内部上限はモデル入力350 ids、形態素96、ラベル512、予測音声30秒です。辞書の鍵長や作業領域によって、これらより前に上限へ達する場合もあります。

## 他のExampleを試す

冒頭の一覧から環境名を選び、BuildとUploadの両方に同じ`-e`を指定します。例えば固定入力のストリーミング再生は次の手順です。

```sh
pio run -e cores3-streaming
pio run -e cores3-streaming -t upload
pio device monitor -e cores3-streaming -b 115200
```

01〜04は辞書不要で、起動時に固定の53 idsを使って推論します。01・02は音声を再生しません。再実行は半角`r`です。05は文章または`/r`を改行付きで送信します。

各段階の確認ポイントと演算方式の説明は、[Exampleの比較と確認方法](doc/examples.md)を参照してください。

## 困ったとき

| 状況 | 確認すること |
|---|---|
| 辞書が見つからない・SHA-256不一致でビルドできない | [指定の2M辞書](model/README.md)を`model/k1-dict-44000-2mb.bin`として配置してください。他サイズの辞書は現在の設定では使用できません |
| モニターにログが出ない | 115200bpsで接続し、ポートとUSBデータケーブルを確認してリセットしてください |
| 文章を送っても反応しない | `Ready`を待ち、UTF-8・改行付きで送信してください。01〜04を選んでいないかも確認してください |
| `Text rejected`や内部バッファ超過が出る | 短い文章に分けてください。正規化後の長さにも上限があります |
| 発声しない、音が途切れる | 01・02は無音が正常です。03〜05では起動からの`FAIL`、`Parse`、再生ログを確認してください。推論の`PASS`だけでは発声成功を表しません |
| 03で`I2S port 1 has not installed`が出る | 保存サンプルの初期化時に出る既知のログです。確認済みの実機では、その後の初期化と再生は成功しています。04・05は初期化済みか確認してから終了処理を呼びます |

問題を報告する際は、使用した環境名、入力文章、起動からのログ、実際に聞こえた音の状態を添えると確認しやすくなります。[ログの読み方](doc/validation.md)も参照してください。

## 仕組みと検証資料

05は「文章の正規化 → 辞書による単語分割 → 読み・アクセントの処理 → モデル用ids → 音声合成 → 先読み付き再生」の順に動きます。解析と推論は作業領域を順番に再利用し、モデルと辞書はflash上のデータを参照します。

| 資料 | 内容 |
|---|---|
| [他プロジェクトへの移植](doc/porting.md) | 必要ファイル、設定、呼び出し順、メモリと排他 |
| [実装の仕組み](doc/architecture.md) | ファイル構成、03のmain.cpp解説、04のバッファ設計、05の解析処理 |
| [Exampleの比較と確認方法](doc/examples.md) | 各段階の違い、チェックサム、実行時の確認ポイント |
| [ログと検証結果](doc/validation.md) | 時間・メモリの指標、実機測定例、ホストテストの実行方法 |
| [モデルと辞書](model/README.md) | 取得元、形式、SHA-256、ビルド時の埋め込み |

01〜05はCoreS3で動作確認済みです。05は半角英数字の正規化後の発声も確認しています。詳しい性能測定の有無や対象構成は、検証資料に分けて記載しています。

## ライセンス・謝辞

独自のArduino移植・追加部分は[MIT](LICENSE)、上流由来のコードは各原ライセンスを保持しています。**同梱モデルと生成音声はMITではなく、sanoTTS-jp Model License 1.0の条件が適用されます。** ファームウェアにもモデルが含まれ、05には辞書も含まれます。

出所、変更範囲、必須クレジット、用途制限、再配布条件は[NOTICE.md](NOTICE.md)、ライセンス原文は[LICENSES](LICENSES/README.md)を参照してください。

本プロジェクトは、[sanoTTS-jp](https://github.com/ayutaz/sanoTTS-jp)、そのESP-IDF移植版[SanoTTS-jp-M5StackCoreS3](https://github.com/nnn112358/SanoTTS-jp-M5StackCoreS3)、Open JTalk、M5Unified/M5GFXなどの成果を利用しています。各提供者による公式プロジェクト・推奨品であることを示すものではありません。
