"""Run with Linux/WSL Python, gcc/g++, and the documented local dictionary."""
from pathlib import Path
import importlib.util
import re
import subprocess
import sys

sys.dont_write_bytecode = True
import tempfile

source = Path(__file__).resolve().parent
root = source.parents[1]
core = root / "lib/saanotts_core"
example = root / "examples/05_text_input"
with tempfile.TemporaryDirectory(prefix="saan-text-test-") as temp:
    build = Path(temp)
    cmd = ["gcc", "-g", "-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
           "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections", "-DCHARSET_UTF_8",
           "-DLABEL_IDS_EXTERNAL_SCRATCH=1", "-I" + str(core), "-I" + str(root / "src"),
           str(source / "text_e2e.c"), str(core / "saan_kanji.c")]
    cmd += [str(core / n) for n in ["jdict.c", "accent.c", "njd_rules.c", "label_ids.c"]]
    cmd += [str(p) for p in sorted((core / "openjtalk").glob("*.c"))]
    cmd += ["-lm", "-o", str(build / "text_e2e")]
    subprocess.run(cmd, check=True)
    subprocess.run([str(build / "text_e2e"), str(root / "model/k1-dict-44000-2mb.bin")], check=True)
    subprocess.run(["g++", "-std=c++11", "-fsanitize=address,undefined", "-I" + str(example),
                    str(source / "line_test.cpp"), "-o", str(build / "line_test")], check=True)
    subprocess.run([str(build / "line_test")], check=True)
    print("UTF-8 / CR LF CRLF / overflow recovery PASS")

    spec = importlib.util.spec_from_file_location("dictionary_generator", root / "scripts/dictionary_to_header.py")
    generator = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(generator)
    blob = root / "model/k1-dict-44000-2mb.bin"
    data = blob.read_bytes()
    header = build / "dictionary.h"
    generator.generate(blob, header)
    body = header.read_text().split("= {", 1)[1].split("}", 1)[0]
    assert bytes(int(n) for n in re.findall(r"\d+", body)) == data
    for corrupted in [data[:-1], b"BAD!" + data[4:], data[:4] + b"\x03\x00" + data[6:],
                      data[:-1] + bytes([data[-1] ^ 1])]:
        try:
            generator.validate(corrupted)
        except ValueError:
            pass
        else:
            raise AssertionError("corrupted dictionary accepted")
    try:
        generator.generate(build / "missing.bin", header)
    except FileNotFoundError:
        pass
    else:
        raise AssertionError("missing dictionary accepted")
    print("Dictionary header byte equality / corrupt and missing input rejection PASS")
