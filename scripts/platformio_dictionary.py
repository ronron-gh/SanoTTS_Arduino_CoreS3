"""Only cores3-text-input embeds a dictionary."""
Import("env")
import hashlib
from pathlib import Path
import subprocess

root = Path(env.subst("$PROJECT_DIR"))
blob = root / "model/k1-dict-44000-2mb.bin"
generator = root / "scripts/dictionary_to_header.py"
out = Path(env.subst("$BUILD_DIR")) / "generated"
header = out / "saan_dict_blob.h"
stamp = out / "dictionary.sha256"
fingerprint = hashlib.sha256(blob.read_bytes() + generator.read_bytes() +
                             (root / "scripts/platformio_dictionary.py").read_bytes()).hexdigest()
if not header.exists() or not stamp.exists() or stamp.read_text() != fingerprint:
    subprocess.run([env.subst("$PYTHONEXE"), "-X", "utf8", str(generator),
                    "--blob", str(blob), "--out", str(header)], check=True)
    stamp.write_text(fingerprint)
env.Append(CPPPATH=[str(out)])
