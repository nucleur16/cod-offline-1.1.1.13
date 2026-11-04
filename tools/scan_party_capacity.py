"""Read-only candidate scan for exact C1 party capacity field accesses."""
import hashlib
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "analysis_deps"))
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_MEM

from audit_exact_c1_pe import image_section


raw = Path(sys.argv[1]).read_bytes()
print("sha256", hashlib.sha256(raw).hexdigest())
base = int(sys.argv[2], 0)
size = int(sys.argv[3], 0)
writes_only = "--writes" in sys.argv[4:]
offsets = {0x50, 0x54}
for arg in sys.argv[4:]:
    if arg.startswith("--offsets="):
        offsets = {int(value, 0) for value in arg.partition("=")[2].split(",")}
name, data = image_section(raw, base)
if name != ".text":
    raise ValueError("expected exact C1 code")
decoder = Cs(CS_ARCH_X86, CS_MODE_32)
decoder.detail = True
decoder.skipdata = True
for insn in decoder.disasm(data[:size], base):
    if insn.id == 0:
        continue
    if writes_only and insn.mnemonic not in ("mov", "add", "sub", "inc", "dec", "xor", "and", "or"):
        continue
    operands = insn.operands[:1] if writes_only else insn.operands
    if any(op.type == X86_OP_MEM and op.mem.disp in offsets for op in operands):
        print(f"{insn.address:08x}: {insn.mnemonic:<8} {insn.op_str}")
