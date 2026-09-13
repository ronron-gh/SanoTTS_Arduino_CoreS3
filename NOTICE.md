# NOTICE

SanoTTS Arduino CoreS3は、以下の成果物を基にしたArduino / PlatformIO向けの移植・検証プロジェクトです。上流や音声素材の提供者による公式プロジェクト・推奨品であることを示すものではありません。

## コードと移植元

| 出所 | 使用箇所・本プロジェクトとの関係 | ライセンス |
|---|---|---|
| [sanoTTS-jp](https://github.com/ayutaz/sanoTTS-jp) — Copyright (c) 2026 yousan | 推論コア、辞書ローダー、日本語解析、モデル生成補助、保存済みのESP-IDFサンプルの基礎 | [MIT全文](LICENSES/sanoTTS-jp.LICENSE.txt) |
| [SanoTTS-jp-M5StackCoreS3](https://github.com/nnn112358/SanoTTS-jp-M5StackCoreS3) — Copyright (c) 2026 nnn112358 | このArduino移植の直接の参照元。CoreS3向け構成、スピーカー再生、arena再利用など | [MIT全文](LICENSES/SanoTTS-jp-M5StackCoreS3.LICENSE.txt) |
| 本プロジェクト — Copyright (c) 2026 ronron-gh | PlatformIO構成、各段階のArduino example、モデル・辞書埋め込み補助、シリアル入力、正規化・上限検査、ホストテストと文書 | [MIT全文](LICENSE) |

`lib/saanotts_core/`、`src/`、`scripts/blob_to_header.py`には上流由来のファイルを含みます。保存例やバックアップもそれぞれ元のライセンスに従います。

推論コア全体を最新上流へ同期したものではありません。`jdict.c/h`のみ、2M辞書のmatrixc/charr対応のため上流コミット `0a92f3b6a98f845017505956625315c338322f71` から取り込みました（[出所記録](lib/saanotts_core/JDICT_UPSTREAM.md)）。その他の初期コピー元について、単一の取得コミットはこのプロジェクトでは記録されていません。

`examples/05_text_input/japanese_parser.c`は保存済みの`src/saan_kanji.c`を基に、上限超過の拒否とOpen JTalk text2mecabによる正規化を追加したものです。

## モデル重みと生成音声

`model/student_i8.bin`はsanoTTS-jp Release **v0.3.0** の `saanotts-jp-v3-int8.bin`（blob v2、654,032 B）です。SHA-256は [model/README.md](model/README.md) に記載しています。

**モデル重みはMITではありません。** [sanoTTS-jp Model License 1.0全文](LICENSES/sanoTTS-jp.LICENSE-MODEL.md) が適用され、派生モデルと生成音声にも条件が及びます。本リポジトリは重みを同梱し、全exampleのファームウェアにも重みを埋め込みます。

### 必須帰属表示（上流指定の原文）

```
This model was distilled from a piper-plus teacher model.
sanoTTS-jp — https://github.com/ayutaz/sanoTTS-jp

つくよみちゃんコーパス
  本ソフトウェアの音声合成には、フリー素材キャラクター「つくよみちゃん」
  （© 夢前黎）が無料公開している音声データを使用しています。
  https://tyc.rei-yumesaki.net/material/corpus/

MOE-Speech (litagin) — https://huggingface.co/spaces/litagin/moe-speech-license
  著作権法 30 条の 4（情報解析のための利用）に基づき学習に使用。

蒸留に使用したテキストコーパス:
  - Common Voice ja (Mozilla) — CC0-1.0
      https://github.com/common-voice/common-voice
  - ROHAN4600 (森勢将雅) — CC0-1.0
      https://github.com/mmorise/rohan4600
  - ITA コーパス — CC0-1.0
      https://github.com/mmorise/ita-corpus
  - JSUT ver1.1 (高道慎之介) — CC-BY-SA-4.0 ほか（subset 別）
      https://sites.google.com/site/shinnosuketakamichi/publication/jsut

教師実装: piper-plus (MIT) — https://github.com/ayutaz/piper-plus
```

### 用途制限と再配布

上流モデルライセンスは、生成音声の「個人・団体への攻撃・批判」「政治・宗教上の主張」「アダルト用途」「音声素材としての再配布」を禁止用途として定めています。詳細と一次条件は [モデルライセンス](LICENSES/sanoTTS-jp.LICENSE-MODEL.md) および [つくよみちゃんコーパスの利用規約](https://tyc.rei-yumesaki.net/material/corpus/) を参照してください。

モデルやモデルを含むファームウェアを再配布する際は、上記帰属表示・用途制限・条件の伝播を受領者に伝えてください。コードのMIT表記をモデルへ拡張したり、モデルをMITとして再配布したりすることはできません。

## Open JTalk

`lib/saanotts_core/openjtalk/`はsanoTTS-jp経由で取り込んだpyopenjtalk-plus同梱のOpen JTalkコードです。Copyright (c) 2008-2016 Nagoya Institute of Technology / HTS Working Group。修正BSD条件の全文は [COPYING](lib/saanotts_core/openjtalk/COPYING)、上流の帰属表示は [NOTICE-openjtalk](LICENSES/sanoTTS-jp.NOTICE-openjtalk.txt) に同梱しています。

[PROVENANCE.md](lib/saanotts_core/openjtalk/PROVENANCE.md) は取得元が作成した記録です。その中の検証スクリプトやパスは取得元の構成を指します。本Arduino移植ではOpen JTalkのソース本文を変更せず、ビルド時のヘッダ指定で動的確保をPSRAM優先へ差し替えています。

## 辞書

`cores3-text-input`はsanoTTS-jpの [v0.3.1-rc1-smallflash Release](https://github.com/ayutaz/sanoTTS-jp/releases/tag/v0.3.1-rc1-smallflash) の `k1-dict-44000-2mb.bin`を使用します。NAIST Japanese DictionaryおよびUniDicを基にした派生辞書で、帰属・再配布条件は [辞書NOTICE全文](LICENSES/sanoTTS-jp.NOTICE-dictionary.txt) を参照してください。

- Copyright (c) 2009, Nara Institute of Science and Technology, Japan.
- Copyright (c) 2011-2017, The UniDic Consortium.

辞書バイナリはGit管理外ですが、05のビルド成果物には含まれます。辞書または辞書入りファームを配布する場合は、著作権表示・条件・免責を同梱してください。

## ビルド時の外部依存

| 依存 | この構成での取得・利用 | 条件・出所 |
|---|---|---|
| M5Unified 0.2.15 | PlatformIO lib_deps | [MIT全文](LICENSES/M5Unified.LICENSE.txt)、[上流](https://github.com/m5stack/M5Unified) |
| M5GFX 0.2.28（今回のビルドで解決された版） | M5Unified経由。platformio.iniでは直接固定していない | [MIT全文](LICENSES/M5GFX.LICENSE.txt)、[上流](https://github.com/m5stack/M5GFX) |
| Arduino-ESP32 2.0.9 | espressif32@6.3.2のArduino framework | [上流のライセンス](https://github.com/espressif/arduino-esp32/blob/2.0.9/LICENSE.md)。ArduinoコアにはLGPL-2.1-or-laterのコードを含む |
| ESP-IDFとSDK内の第三者コンポーネント | Arduino frameworkに同梱されたSDK | [ESP-IDF](https://github.com/espressif/esp-idf) のApache-2.0と各コンポーネント固有の条件 |

これらの依存全体は本リポジトリへ同梱していません。ファームウェアを配布する場合は、実際に使用した版のライセンス・NOTICEに加え、該当するソース提供・再リンク等の条件も確認してください。本NOTICEの同梱だけで、すべての依存の配布条件を満たしたとするものではありません。追加するフォントや別ライブラリにも個別の条件があります。

## 配布するものに応じた扱い

- **このGitリポジトリ**: `LICENSE`、`LICENSES/`、本NOTICE、Open JTalkのCOPYINGを保持してください。モデルが含まれるため、モデルの条件も適用されます。
- **ビルド済みファームウェア**: 上記の表示と適用ライセンスを同梱し、05では辞書NOTICEも含めてください。利用したframework・ライブラリの配布条件にも対応してください。
- **生成音声**: モデル由来の用途制限を守ってください。
