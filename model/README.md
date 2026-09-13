# モデルと辞書

## 推論モデル（Gitに同梱）

| 項目 | 内容 |
|---|---|
| ファイル | `student_i8.bin` |
| 取得元 | [sanoTTS-jp Release v0.3.0](https://github.com/ayutaz/sanoTTS-jp/releases/tag/v0.3.0) の `saanotts-jp-v3-int8.bin` |
| 形式・サイズ | v3モデル、SAAN blob v2形式、int8、654,032 B |
| SHA-256 | `2d2b8543c06b6a749f19c9918de68244409e2bb6ad1d921a90b5c358f96d4d79` |
| ライセンス | [sanoTTS-jp Model License 1.0](../LICENSES/sanoTTS-jp.LICENSE-MODEL.md)（MITではありません） |

piper-plus（つくよみちゃん）教師から蒸留された559,008パラメータのモデルです。詳細は [取得版のMODEL_CARD](https://github.com/ayutaz/sanoTTS-jp/blob/v0.3.0/MODEL_CARD.md) を参照してください。必須帰属表示・生成音声の用途制限・再配布先への条件伝播は [NOTICE.md](../NOTICE.md) に記載しています。

PlatformIOの [platformio_model.py](../scripts/platformio_model.py) が [blob_to_header.py](../scripts/blob_to_header.py) を呼び、`.pio/build/<環境>/generated/saan_model_blob.h` を生成します。モデルは16バイト整列したconst配列としてファームウェアのflashに含まれます。モデル専用パーティションへの書き込みは不要です。

モデルを入手し直す場合（GitHub CLIを使う例）:

```sh
gh release download v0.3.0 --repo ayutaz/sanoTTS-jp --pattern 'saanotts-jp-v3-int8.bin' --dir model/download
```

ダウンロードしたファイルのSHA-256が上記と一致することを確認してから `student_i8.bin` として配置してください。`sanoTTS-jp-v0.3.0.SHA256SUMS.txt` は取得元Releaseの照合資料です。最新モデルへの自動追従はしません。

## 2M辞書（Git管理外）

| 項目 | 内容 |
|---|---|
| ファイル | `k1-dict-44000-2mb.bin` |
| 取得元 | [sanoTTS-jp Release v0.3.1-rc1-smallflash](https://github.com/ayutaz/sanoTTS-jp/releases/tag/v0.3.1-rc1-smallflash) |
| 形式・サイズ | K1D1 v2、44,000エントリ、977,456 B、matrixc/charr |
| SHA-256 | `cd1ed65241600b29ced9fde627b1543d93f5f5c0cd5297a4dba42d3f01d8806a` |
| 出所・条件 | NAIST-jdic / UniDic由来。[辞書NOTICE全文](../LICENSES/sanoTTS-jp.NOTICE-dictionary.txt) |

次の例で取得するか、取得元Releaseからファイルをダウンロードしてmodel/へ置いてください。

```sh
gh release download v0.3.1-rc1-smallflash --repo ayutaz/sanoTTS-jp --pattern 'k1-dict-44000-2mb.bin' --dir model
```

移植元の `scripts/get_dict.sh 44000` を使って取得した同一ファイルも利用できます。このプロジェクトにget_dict.shは同梱していません。ビルドによる自動ダウンロードもありません。

`cores3-text-input` だけが [dictionary_to_header.py](../scripts/dictionary_to_header.py) で形式・SHA-256を検査し、`.pio/build/cores3-text-input/generated/saan_dict_blob.h` を生成します。辞書はモデルと同様にflashへ埋め込み、RAMへの全体コピーや専用dictパーティションは使いません。他サイズの辞書は現在の環境では選択対象外です。

辞書をGit管理外にしていても、05のファームウェアには辞書が含まれます。バイナリ配布には [NOTICE.md](../NOTICE.md) と辞書のライセンス表示を含めてください。
