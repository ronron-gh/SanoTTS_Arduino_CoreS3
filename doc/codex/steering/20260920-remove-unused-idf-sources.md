# src の未使用ソース整理

## 目的・対象範囲
現在の Arduino の5環境で使用しない、src 内の保存用ESP-IDFソースとビルド定義、および重複する保存用Arduino main.cppを削除する。前作業の未コミット差分を維持し、それらを戻さない。

## 調査結果
platformio.ini の全環境で src のコンパイル対象は saan_model.c のみ。demo_ids.h は01〜04とホストテスト、saan_model.h はモデル接続で使用する。現行の日本語解析は lib/saanotts_core/saan_kanji.c・h を使用している。

## 削除対象（src 内、18ファイル）
- CMakeLists.txt、idf_component.yml
- main.c、main.cpp
- saan_console.c、saan_console.h
- saan_dict.c、saan_dict.h
- saan_kanji.c
- saan_speaker.cpp、saan_speaker.h、saan_speaker_i2s.c
- saan_ui.cpp、saan_ui.h、saan_ui_avatar.cpp、saan_ui_headless.c、saan_ui_impl.h、saan_ui_text.cpp

## 残すもの
- src/demo_ids.h、src/saan_model.c、src/saan_model.h：使用中。
- examples/03_buffered_playback/main.cpp：削除する src/main.cpp とSHA-256が一致する蓄積再生版。保存用コードはこのExampleに統一する。
- examples、lib、過去のステアリング・バックアップ、ライセンス原文は維持する。

## 主な変更ファイル・方針
上記18ファイルを個別に削除。doc/architecture.md、doc/examples.md、doc/porting.md、NOTICE.md の現行説明を更新する。解析の由来は保存ファイルへの参照から上流リポジトリの main/saan_kanji.c 由来という説明へ変更し、出所・ライセンスを保持する。src/main.cpp を保持しているという現行説明も修正する。過去の記録は変更しない。

## 副作用・戻し方
Arduinoの実行動作を変えない。保存用ESP-IDFソースを直接参照する用途には影響するため、上流参照とgit履歴で追跡できる説明を残す。戻す場合は今回の削除・文書変更のみ復元し、前作業の解析集約を取り消さない。

## 確認方法
- 現行ビルド、include、テスト、ドキュメントに削除対象への有効な依存がないことを再確認する。
- 全5環境のPlatformIOビルドと git diff --check を確認する。既存依存を使用し、新しい取得は行わない。
- コミット・pushは別途指示を受ける。

## 状態
2026-09-20 承認済み。指定の18ファイルを削除し、現行ドキュメントを更新。前作業の差分は保持。

## 確認結果
- 全5環境の PlatformIO ビルド成功（cores3-inference、cores3-pie、cores3-buffered、cores3-streaming、cores3-text-input）。
- src は demo_ids.h、saan_model.c、saan_model.h の3ファイルのみ。Example 03は変更なし。
- 現行文書の参照と git diff --check を確認。実機確認は未実施。
- 前作業の解析集約・移植手順の差分を保持。コミット・pushは未実施。
