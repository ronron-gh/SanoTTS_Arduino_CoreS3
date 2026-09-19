"""Library-local Japanese parser sources and ESP32 Open JTalk allocation policy."""
Import("env", "pio_lib_builder")
from pathlib import Path

root = Path(pio_lib_builder.path)
defines = env.ParseFlags(env.GetProjectOption("build_flags")).get("CPPDEFINES", [])
values = {}
for define in defines:
    if isinstance(define, (tuple, list)):
        values[define[0]] = str(define[1])
    else:
        values[define] = "1"

env.Replace(SRC_FILTER=["-<*>", "+<saanotts.c>", "+<saanotts_stream.c>",
                        "+<saanotts_int8.c>", "+<fft.c>"])

if values.get("SAAN_KANJI", "0") == "1":
    # Public scratch sizing must agree between the application and this library.
    if values.get("LABEL_IDS_EXTERNAL_SCRATCH") != "1":
        raise RuntimeError("SAAN_KANJI=1 requires global LABEL_IDS_EXTERNAL_SCRATCH=1")
    env.Append(CPPDEFINES=["CHARSET_UTF_8"])
    env.Replace(SRC_FILTER=[
        "-<*>", "+<saanotts.c>", "+<saanotts_stream.c>",
        "+<saanotts_int8.c>", "+<fft.c>", "+<saan_kanji.c>",
        "+<jdict.c>", "+<accent.c>", "+<njd_rules.c>",
        "+<label_ids.c>", "+<oj_heap_psram.c>", "+<openjtalk/*.c>",
    ])

    def openjtalk_psram(build_env, node):
        if Path(node.srcnode().get_abspath()).parent != root / "openjtalk":
            return node
        custom = build_env.Clone()
        custom.Append(CCFLAGS=["-include", str(root / "oj_heap_psram.h")])
        return custom.Object(node)

    env.AddBuildMiddleware(openjtalk_psram, "*.c")
