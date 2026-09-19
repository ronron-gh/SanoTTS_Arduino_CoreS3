# Example 05 を他のプロジェクトへ移す

[READMEへ戻る](../README.md)

まず同じ CoreS3 / PlatformIO / Arduino 構成で確認します。他チップや別バージョンのArduinoでの動作は未確認です。

## コピーするもの

| コピー元 | 移植先 | 役割 |
|---|---|---|
| lib/saanotts_core/ 全体 | lib/saanotts_core/ | 推論・解析・ビルド設定・出所資料 |
| examples/05_text_input/ の main.cpp、text_input.h、dictionary.c、dictionary.h | src/ | 入力・再生サンプル・辞書接続 |
| src/saan_model.c、src/saan_model.h | src/ | モデル接続 |
| scripts/ の platformio_model.py、blob_to_header.py、platformio_dictionary.py、dictionary_to_header.py | scripts/ | データ検査と埋め込みヘッダ生成 |
| model/student_i8.bin | model/ | モデル |
| model/k1-dict-44000-2mb.bin | model/ | 別途取得する指定2M辞書 |
| my_cores3_16MB.csv | プロジェクト直下 | 下記単体構成用パーティション表 |
| LICENSE、NOTICE.md、LICENSES/、model/README.md | 出所を保持できる場所 | ライセンス・取得元・再配布条件 |

既存の main.cpp や LICENSE を上書きせず、一時プロジェクトで先に確認してください。このリポジトリの src/ は共通ファイルのみです。Example 05に必要なファイルは上記の表に従って選びます。辞書は[取得手順](../model/README.md)に従って用意し、生成ヘッダや .pio/ はコピーしません。

## 単体確認用 PlatformIO 設定

通常の src/main.cpp 配置を使います。src_dir や build_src_filter の指定は不要です。

~~~ini
[platformio]
default_envs = cores3-text-input

[env:cores3-text-input]
platform = espressif32@6.3.2
board = esp32s3box
framework = arduino
board_build.arduino.memory_type = qio_qspi
board_build.arduino.partitions = my_cores3_16MB.csv
board_build.f_flash = 80000000L
monitor_speed = 115200
upload_speed = 1500000
lib_deps =
    m5stack/M5Unified @ 0.2.15
build_flags =
    -DBOARD_HAS_PSRAM
    -DARDUINO_M5STACK_CORES3
    -DSAAN_INT8_ACT=1
    -DSAAN_PIE=1
    -DSAAN_KANJI=1
    -DLABEL_IDS_EXTERNAL_SCRATCH=1
extra_scripts =
    pre:scripts/platformio_model.py
    pre:scripts/platformio_dictionary.py
~~~

~~~sh
pio run
pio run -t upload
pio device monitor -b 115200
~~~

SAAN_INT8_ACT / SAAN_PIE は利用側とコアに共通の演算設定です。PIE はESP32-S3用です。SAAN_KANJI=1 にすると、library.json 経由の platformio_build.py が解析ソースを追加し、Open JTalkだけにPSRAM優先の割り当て設定を適用します。CHARSET_UTF_8 はライブラリ側で設定します。ライブラリの最適化設定 -O2 は推論・解析に共通です。

LABEL_IDS_EXTERNAL_SCRATCH=1 は公開ヘッダの作業領域サイズにも影響するため、共通 build_flags に必要です。欠けるとビルド設定がエラーにします。解析を使わない場合は SAAN_KANJI を未指定または0とし、辞書接続コードと辞書生成スクリプトも外します。

モデルと辞書はアプリに埋め込み、専用辞書パーティションは不要です。既存アプリではパーティション表を単純に置き換えず、アプリ容量・OTA・ファイルシステム配置を確認してください。生成スクリプトは指定ファイル名と model/、scripts/ の配置を前提とします。

## 既存アプリへの接続順序

1. PSRAMと音声出力を初期化し、saan_model_open()、text_dictionary_open()、saan_kanji_init() で準備します。saan_kanji_init() 自体は現在メモリを確保しません。
2. 16バイト整列した作業領域と独立したids配列を用意します。Example 05はarena 180,224バイト、最大350 ids、タスクスタック16,384バイトです。
3. UTF-8文章を検査し、saan_kanji_to_ids() に辞書、文章のバイト数、作業領域、ids格納先を渡します。
4. 解析成功とids数を確認後、同じ領域を saan_arena_init() で推論用に初期化し、saan_stream_init() を呼びます。ids はarena外に保持し、推論終了まで有効にします。
5. saan_stream_pull() を繰り返し、float PCMをint16 PCMに変換します。戻される個数はフレーム数で、サンプル数は SAAN_HOP を掛けます。具体的な引数・上限・エラー処理はExample 05を参照してください。
6. 先読み後、22,050Hz・モノラルで再生します。再生側が参照中のPCMを上書き・解放せず、末尾の再生完了を待ちます。

初回はサンプルの動作を維持し、その後Serial受付をアプリの文章受付へ置き換えます。M5初期化やタスクを重複させないよう統合してください。ライブラリはSerial、M5.Speaker、再生キューを管理しません。

## メモリと排他

- 解析内部にstaticの作業ポインタがあるため、別arenaでも解析の並行呼び出しは禁止です。単一タスクまたは解析全体の排他を使います。
- 解析と推論でarenaを順番に再利用します。推論中に同じarenaで次の文章を解析しないでください。
- arenaは内部RAM優先、確保失敗時はPSRAMです。性能は配置と既存アプリの負荷に依存します。
- Open JTalkの一時メモリとPCMも必要です。先読み4チャンク＋リング3チャンクのPCMは通常28,672バイトです。
- マイク・スピーカー・I2Sを使う既存機能との排他と、再生失敗時の停止・解放を維持してください。
- UTF-8検査は入力側の責務です。正規化による文字列伸長を含め上限エラーを処理し、解析失敗時は発声しません。

## 動作確認と配布

起動文、「123個あります。」「OpenAIで開発します。」、反復入力、長すぎる入力後の復帰を確認します。Parse: OK / PASSだけでなく、ノイズ・音切れ・末尾欠落と反復時の空きメモリを確認してください。

モデル・辞書・Open JTalk等の条件を移植先にも引き継ぎます。独自部分のMITだけでは全体を説明できません。ファームウェアにもモデル・辞書が含まれるため、[NOTICE](../NOTICE.md)と原ライセンスを確認してください。
