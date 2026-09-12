"""Generate the model header before PlatformIO scans and compiles sources."""
Import("env")

import hashlib
import pathlib
import struct
import subprocess

root = pathlib.Path(env.subst("$PROJECT_DIR"))
blob = root / "model/student_i8.bin"
generator = root / "scripts/blob_to_header.py"
out_dir = pathlib.Path(env.subst("$BUILD_DIR")) / "generated"
header = out_dir / "saan_model_blob.h"
stamp = out_dir / "model.sha256"

data = blob.read_bytes()
if len(data) < 64 or data[:4] != b"SAAN" or struct.unpack_from("<I", data, 4)[0] != 2:
    raise RuntimeError("Expected a SAAN v2 model: " + str(blob))

# Content fingerprints also detect replaced files whose timestamps are unchanged.
fingerprint = hashlib.sha256(data + generator.read_bytes() + (root / "scripts/platformio_model.py").read_bytes()).hexdigest()
if not header.exists() or not stamp.exists() or stamp.read_text(encoding="utf-8") != fingerprint:
    subprocess.run([
        env.subst("$PYTHONEXE"), "-X", "utf8", str(generator),
        "--blob", str(blob), "--out", str(header),
    ], check=True)
    stamp.write_text(fingerprint, encoding="utf-8")
env.Append(CPPPATH=[str(out_dir)])
