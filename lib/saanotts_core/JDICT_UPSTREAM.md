# Dictionary reader provenance

`jdict.c` and `jdict.h` are copied without changes from the MIT-licensed
[ayutaz/sanoTTS-jp](https://github.com/ayutaz/sanoTTS-jp/tree/0a92f3b6a98f845017505956625315c338322f71/csrc),
commit `0a92f3b6a98f845017505956625315c338322f71` (retrieved 2026-09-13).
This update adds matrixc/charr support required by k1-dict-44000-2mb.bin,
including the unknown-word condition for compressed character tables.
The inference core and the vendored Open JTalk sources were not synchronized.

See [compatibility results](../../doc/codex/analysis/20260913-2mb-dictionary.md).
