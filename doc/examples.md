# Exampleの比較と確認方法

[READMEへ戻る](../README.md)

1. PlatformIOで対象の環境を選び、BuildとUploadを行います。同じCoreS3上のファームウェアは選択したexampleに置き換わります。
2. 115200bpsのモニターを開いてリセットし、起動からのログを確認します。
3. 01〜04は `r` を送信して再実行します。05は `Ready` 表示後に文章または `/r` を送り、Enterで確定します。

| example | 起動ログと実機での確認ポイント | 同一構成での参考チェックサム |
| --- | --- | --- |
| 01_inference | `W8A32 / PIE=0`、推論PASS、再実行でも一致。音は出ない | `3c5d15d4056974af` |
| 02_pie_inference | `W8A8 / PIE=1`、PIEセルフテストPASS、推論PASS。音は出ない | `7f28bdb2c151b52c` |
| 03_buffered_playback | PIEセルフテストと推論PASS、発声、Playback complete。rで再び発声 | `7f28bdb2c151b52c`（PIE版） |
| 04_streaming_playback | 先読み後の発声、推論PASS、Playback complete、発話開始要求時間とキュー枯渇回数 | `7f28bdb2c151b52c`（同じモデル・入力・PIE構成） |
| 05_text_input | 辞書の初期化、起動文のParse OK・7形態素・53 ids、発声とReady、文章入力と/r | 任意入力では値が変わる。起動文のidsは固定入力と一致 |

01〜03の各main.cppは当時のコードを変更せずコピーしています。01は `doc/codex/backups/20260912-w8a8-pie/src/main.cpp`、02は `doc/codex/backups/20260912-buffered-playback/src/main.cpp`、03は構成整理前の `src/main.cpp` が保存元です。蓄積再生版の既知のI2S終了ログも、参考コードをそのまま残すため変更していません。

[実装の仕組み](../doc/architecture.md)の蓄積再生の詳しい処理解説は [03_buffered_playback/main.cpp](../examples/03_buffered_playback/main.cpp) を対象とします。保持しているsrc/main.cppと内容は同一です。01・02には全音声用バッファと再生処理がなく、01にはPIEセルフテストもありません。

## 演算方式

W8A32は重みが8bit整数、活性化（計算途中の値）が32bit浮動小数点です。W8A8は活性化も8bit整数へ量子化して計算します。PIEはESP32-S3で複数の整数の積和をまとめて実行する命令です。演算方式が変わるため、両構成の波形やチェックサムが同一になるとは限りません。

## 再生方式

03は音声全体をPSRAMに蓄積した後に再生します。04と05は最初の4チャンクを先読みし、その後は推論と再生を並行させます。05だけが日本語文章を解析し、01〜04は固定の53 idsを使います。

## 04の実機確認

`cores3-streaming`をUploadし、モニターを開いたままリセットしてください。起動時と `r` による再実行で、PIEセルフテスト・推論のPASS、チェックサム `7f28bdb2c151b52c`、Playback completeを確認します。音切れ・ノイズ・末尾欠落、再生後のメモリの戻り、`first_request`と`queue_empty_events`も確認します。キュー枯渇や音切れがある場合は、ログと聴取結果に基づいて先読み量を調整します。


## 05の実機確認

起動時は `dictionary: entries=44000 surfaces=28128 matrix=1377x1377 clusters=256x256 char_runs=106` と、起動文の `Parse: OK`・`tokens=7 ids=53` を確認します。文章を送った後は、読み、数字の読み方、ノイズ、音切れ、末尾の欠落を聴取します。

`/r`を繰り返した際のヒープ・スタック残量・`queue_empty_events`の推移と、長い入力を拒否した後に短い文章を処理できることも確認してください。詳細な指標は[ログの読み方](validation.md)を参照してください。

## 03の既知ログ

03はスピーカー再設定の前に無条件で`M5.Speaker.end()`を呼びます。起動時の`I2S port 1 has not installed`は、未インストールのI2Sを終了しようとすることが原因と考えられます。確認した実機では、その後の初期化・再生は成功しています。03は参考コードとして保持し、呼び出し条件の修正は04・05に反映しています。
