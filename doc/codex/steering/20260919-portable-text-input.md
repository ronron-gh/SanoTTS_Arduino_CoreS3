# Example 05 の移植性改善と移植手順

## 目的
日本語文章からモデル入力 ids を生成する機能を lib/saanotts_core 内に集約し、他の PlatformIO / Arduino プロジェクトへ移す際の必要ファイルと設定を明確にする。

## 現状と対象範囲
- examples/05_text_input/japanese_parser.c と src/saan_kanji.h がライブラリ外にある。
- platformio.ini が jdict / accent / njd_rules / label_ids / oj_heap_psram / Open JTalk をプロジェクトソースとして列挙している。
- scripts/platformio_dictionary.py が辞書生成に加え Open JTalk のメモリ割り当て置換も担当している。
- モデル埋め込みは src/saan_model.c・h と scripts、辞書埋め込みは Example 05 の dictionary.c・h と scripts に依存する。

## 変更方針（2026-09-20 承認済み）
1. 現行の正規化・上限チェックを含む japanese_parser.c を lib/saanotts_core/saan_kanji.c へ移し、公開ヘッダ saan_kanji.h も同所へ移す。C++ から利用するための extern "C" ガードと、現行実装に合う API コメントを確認する。
2. 日本語解析ソースの選択と Open JTalk 限定の PSRAM 割り当て置換をライブラリ側のビルド設定へ移す。SAAN_KANJI による有効化を明示し、01〜04では解析コード・辞書を要求しない。PlatformIO のライブラリビルド環境へのフラグ伝播は実ビルドで確認する。
3. モデル・辞書の選択と埋め込み、Serial 入力、M5Unified 初期化、PCM 再生・タスク管理は利用側に残す。今回は汎用プレーヤー API の新設や移植先プロジェクトへの組み込みは行わない。
4. doc/porting.md を作成する。コピーするライブラリ・モデル接続コード・辞書接続コード・生成スクリプト・データ・ライセンス資料、最小限の PlatformIO 設定、解析→推論→再生の呼び出し順と移植先の責務を記載する。既存プロジェクトの src_dir を変更せず導入できる構成を示す。
5. tests/host/run_tests.py、README、doc/architecture.md、NOTICE などの現行パス参照を更新する。過去のステアリングやバックアップは履歴として残す。

## 設計上の注意点
- 解析結果の ids は再利用する arena の外に保持する。解析と推論を同時に実行しない。解析内部の static 状態も確認し、並行呼び出しの制約を明記する。
- 16 バイト整列、内部 RAM / PSRAM、タスクスタック、PCM バッファ寿命、既存音声機能との排他を移植手順に記載する。
- PIE は ESP32-S3 向けの設定であり、他チップへの無条件な移植可能性は約束しない。
- モデル・辞書のライセンスとファームウェア再配布時の条件も移植対象に含める。
- src/saan_kanji.c は保存された ESP-IDF 参考実装であるため、現行解析実装と取り違えず、ビルド対象から除外したままにする。

## 主な変更ファイル
lib/saanotts_core/{saan_kanji.c,saan_kanji.h,library.json,ビルド用スクリプト}、platformio.ini、scripts/platformio_dictionary.py、tests/host/run_tests.py、README.md、doc/porting.md、doc/architecture.md、必要な出所・参照ドキュメント。

## 確認方法
- ホストの既存解析・正規化・入力境界テストを実行する。
- 01〜05 の PlatformIO ビルドを確認する。05 以外に解析機能や辞書が混入しないこと、Open JTalk の割り当て置換が維持されることをシンボル等で確認する。
- 一時的な最小移植先プロジェクトで、通常の src/main.cpp 配置と文書化した手順によるビルドを確認する。
- 実機での発声・反復・メモリ確認はユーザーに依頼する。今回の確認で未実施のものは明記する。

## 副作用と戻し方
ビルド対象の重複、設定の伝播漏れ、01〜04への依存混入が主なリスク。変更前は git status が clean。問題があれば本作業の差分だけを戻す。コミット・push は別途指示を受けて実施する。

## 実装経過
- 2026-09-20: 解析実装・ヘッダをライブラリへ移動。解析本体の処理は変更していない。
- ライブラリの extraScript で SAAN_KANJI と共通 scratch 設定を確認し、解析ソースと Open JTalk 限定の割り当て置換を設定。フラグは GetProjectOption("build_flags") を ParseFlags で展開して参照する。固定 srcFilter が動的指定より優先されるため、通常の推論ソース一覧もスクリプト側に移した。Open JTalk の対象判定には node.srcnode() の実ソースパスを使う。
- doc/porting.md と通常の src 配置による単体設定を追加。
- ホストの ASan/UBSan 付き解析・正規化・入力境界・辞書生成テストは成功。

- 最小移植先は doc/porting.md の設定をそのまま使用し、通常の src/ 配置でビルド成功。既存依存をコピーし、新規ダウンロードは行っていない。
- 最小移植先の Open JTalk オブジェクトは saan_oj_calloc/free/strdup を参照し、ラッパー自身は heap_caps_* を参照することを確認。
- 公開ヘッダ単独のC++構文確認、scratchフラグ不足時のビルド拒否、移動前後の解析本体一致を確認。
- ライブラリの既存 -O2 が解析コードにも適用される。最小構成の静的RAM 39,552バイト、flash 2,213,285バイト。以前の解析はプロジェクト側の最適化設定だったため、実機の時間・音声は再確認が必要。

## 最終確認結果
- 最終状態で cores3-inference / cores3-pie / cores3-buffered / cores3-streaming / cores3-text-input の全5環境ビルド成功。
- 01〜04のELFに解析・辞書シンボルがないこと、05には解析・辞書とPSRAM割り当て関数があることを確認。
- git diff --check 成功。今回の変更後の実機確認は未実施。コミット・pushは行っていない。
