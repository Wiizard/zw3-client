"""Check patch coverage against an archived renderer image, without opening a game."""
import argparse
import hashlib
import json
import re
from pathlib import Path

import capstone

parser = argparse.ArgumentParser()
parser.add_argument("capture", type=Path)
parser.add_argument("--report", type=Path)
args = parser.parse_args()
code = (args.capture / "0-0.text").read_bytes()
source = (Path(__file__).resolve().parent.parent / "src/Components/Modules/RendererBudgets.hpp").read_text()
metadata = json.loads((args.capture / "metadata.json").read_text())
base = metadata["base"]
decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
decoder.detail = True
decoder.skipdata = True
array_references = set()
append_calls = set()
patch_list_references = set()
patch_counter_references = set()
command_calls = set()
command_decoders = set()
recent = []
for instruction in decoder.disasm(code, base + 0x1000):
    if not instruction.id:
        continue
    recent.append(instruction)
    recent = recent[-10:]
    if instruction.mnemonic == "shl" and instruction.operands[-1].type == capstone.x86.X86_OP_IMM and instruction.operands[-1].imm == 2:
        register = instruction.operands[0].reg
        if any(item.mnemonic == "shr" and item.operands[0].type == capstone.x86.X86_OP_REG
               and item.operands[0].reg == register and item.operands[-1].type == capstone.x86.X86_OP_IMM
               and item.operands[-1].imm == 4 for item in recent[:-1]) and any(
                   item.mnemonic == "and" and item.operands[-1].type == capstone.x86.X86_OP_IMM
                   and item.operands[-1].imm == 15 for item in recent[:-1]):
            command_decoders.add(instruction.address - base)
    if instruction.mnemonic == "and" and instruction.operands[-1].type == capstone.x86.X86_OP_IMM and instruction.operands[-1].imm == 15:
        for candidate in recent[-5:-1]:
            if candidate.mnemonic != "shl" or candidate.operands[-1].type != capstone.x86.X86_OP_IMM or candidate.operands[-1].imm != 2:
                continue
            if any(item.address < candidate.address and item.mnemonic == "shr"
                   and item.operands[0].type == capstone.x86.X86_OP_REG and item.operands[0].reg == candidate.operands[0].reg
                   and item.operands[-1].type == capstone.x86.X86_OP_IMM and item.operands[-1].imm == 4 for item in recent):
                command_decoders.add(candidate.address - base)
    for operand in instruction.operands:
        if operand.type == capstone.x86.X86_OP_MEM:
            if operand.mem.base != capstone.x86.X86_REG_RIP:
                if operand.mem.disp == 0x1096A8:
                    patch_list_references.add(instruction.address - base)
                if operand.mem.disp == 0x41938:
                    patch_counter_references.add(instruction.address - base)
            target = None
            if operand.mem.base == capstone.x86.X86_REG_RIP:
                target = instruction.address + instruction.size + operand.mem.disp - base
            elif 0x9440B00 <= operand.mem.disp < 0x9450B00:
                target = operand.mem.disp
            if target is not None and 0x9440B00 <= target < 0x9450B00:
                array_references.add(instruction.address - base)
    if instruction.mnemonic in ("call", "jmp") and any(
        operand.type == capstone.x86.X86_OP_IMM and operand.imm - base == 0x5EFE0
        for operand in instruction.operands
    ):
        append_calls.add(instruction.address - base)
    if instruction.mnemonic in ("call", "jmp") and any(
        operand.type == capstone.x86.X86_OP_IMM and operand.imm - base == 0x5ED40
        for operand in instruction.operands
    ):
        command_calls.add(instruction.address - base)
    if instruction.mnemonic == "add" and any(
        operand.type == capstone.x86.X86_OP_IMM and operand.imm == 0x1096A8
        for operand in instruction.operands
    ):
        patch_list_references.add(instruction.address - base)

assert array_references == {0x1BBD8, 0x1BBEA, 0x21EDC, 0x3D466, 0x3D582, 0x3DD13}, array_references
assert append_calls == {0x5F1BD, 0x5F424, 0x5F6E5, 0x5FA59, 0x5FACE, 0x605E3, 0x60D51, 0x61312, 0x61A61}, append_calls
assert patch_list_references == {0x3D418, 0x3DC35, 0x4FC74}, patch_list_references
assert patch_counter_references == {0x1BC6F, 0x3D3F6, 0x3DC07, 0x4FC7B}, patch_counter_references
assert command_calls == {0x5F369, 0x5F4C0, 0x5F663, 0x5F7C1, 0x5F9AD, 0x5FB78}, command_calls
assert command_decoders == {0x7672E, 0x76803, 0x76B2F, 0x7B159, 0x7B33F, 0x7B4B2}, command_decoders
sites = {}
for match in re.finditer(r"(0x140[0-9A-Fa-f]+),\s*\{((?:0x[0-9A-Fa-f]+\s*,?\s*)+)\}", source):
    rva = int(match[1], 16) - 0x140000000
    expected = bytes(int(item, 16) for item in re.findall(r"0x[0-9A-Fa-f]+", match[2]))
    assert code[rva - 0x1000:rva - 0x1000 + len(expected)] == expected, hex(rva)
    assert rva not in sites, hex(rva)
    sites[rva] = expected.hex()
assert len(sites) == 43, len(sites)
assert array_references | append_calls | patch_list_references | command_calls | command_decoders <= sites.keys()
assert {0x3D401, 0x3DC12} <= sites.keys()
assert {0x3D4C1, 0x3D4CB, 0x3D5AE, 0x3D5D1, 0x3D5E9} <= sites.keys()
result = {
    "passed": True,
    "capture_sha256": hashlib.sha256(code).hexdigest(),
    "header_sha256": hashlib.sha256(source.encode()).hexdigest(),
    "policy_sha256": hashlib.sha256((Path(__file__).resolve().parent.parent / "src/Components/Modules/RenderBudgetPolicy.hpp").read_bytes()).hexdigest(),
    "guarded_sites": sites,
    "complete_direct_cache_array_references": len(array_references),
    "complete_material_append_calls": len(append_calls),
    "complete_lighting_patch_list_references": len(patch_list_references),
    "unchanged_lighting_patch_counter_references": sorted(patch_counter_references),
    "complete_model_command_producer_calls": sorted(command_calls),
    "complete_model_command_token_decoders": sorted(command_decoders),
    "cache_base_relative_accesses": 5,
    "warning_suppression": False,
    "game_started": False,
}
output = json.dumps(result, indent=2)
if args.report:
    args.report.write_text(output + "\n")
print(output)
