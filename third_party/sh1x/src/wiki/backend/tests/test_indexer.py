"""Tests for the lexical source indexer.

The indexer is the part of the wiki that can be wrong in a way nobody notices —
a mis-parsed struct just shows fewer members — so these tests pin the shapes
that actually occur in this tree: engine headers with packing pragmas, macro
definitions butted against prototypes, multi-declarator members, and assembly
listings whose basic-block labels must not read as functions.
"""
from __future__ import annotations

from app.indexer.csource import extract, extract_asm, looks_like_asm
from app.indexer.repo import extract_builtins


def by_name(symbols, name, kind=None):
    for symbol in symbols:
        if symbol.name == name and (kind is None or symbol.kind == kind):
            return symbol
    raise AssertionError(f"{name!r} ({kind}) not found in {[s.name for s in symbols]}")


def test_typedef_struct_members_and_doc():
    source = """
/* Semantic state must not inherit engine-header packing. */
#pragma pack(push, 8)
typedef struct {
    uint32_t sequence;
    const codol_player_fields_t *player;   /* the scalar fields */
    int a, b[4];
    unsigned flag : 1;
} codol_snapshot_state_t;
#pragma pack(pop)
"""
    symbol = by_name(extract("x.h", source), "codol_snapshot_state_t", "struct")
    assert symbol.doc.startswith("Semantic state must not inherit")
    members = {m["name"]: m for m in symbol.detail["members"]}
    assert members["sequence"]["type"] == "uint32_t"
    assert members["player"]["type"] == "const codol_player_fields_t *"
    assert members["player"]["comment"] == "the scalar fields"
    # A multi-declarator line shares its base type, and the array stays on b.
    assert members["a"]["type"] == "int"
    assert members["b"]["type"] == "int [4]"
    assert members["flag"]["bits"] == "1"


def test_pragma_does_not_swallow_the_following_prototype():
    """`#pragma pack(pop)` reads as a call unless directives are blanked."""
    source = """
#pragma pack(pop)

/* Called only after the owner validates the handshake. */
int CODOL_AssociationStart(codol_association_t *, const uint8_t shared_x[28]);
"""
    symbols = extract("x.h", source)
    start = by_name(symbols, "CODOL_AssociationStart", "prototype")
    assert start.signature.startswith("int CODOL_AssociationStart(")
    assert start.doc.startswith("Called only after")
    assert not [s for s in symbols if s.name == "pack"]


def test_define_above_a_prototype_is_not_glued_to_its_return_type():
    source = """
#define CODOL_CLIENT_COMMAND_MAX 1024u
int CODOL_ReadClientCommand(codol_msg_t *message, uint32_t *sequence);
"""
    symbols = extract("x.h", source)
    command = by_name(symbols, "CODOL_ReadClientCommand", "prototype")
    assert command.signature.startswith("int CODOL_ReadClientCommand(")
    assert by_name(symbols, "CODOL_CLIENT_COMMAND_MAX", "macro").detail["value"] == "1024u"


def test_return_type_and_name_stay_separated():
    symbols = extract("x.h", "const char *CODOL_WriteSnapshotFailure(void);\n")
    assert by_name(symbols, "CODOL_WriteSnapshotFailure").signature == (
        "const char *CODOL_WriteSnapshotFailure(void)"
    )


def test_enum_values_and_comments():
    source = """
typedef enum {
    CODOL_STICK_NONE,          /* no attachment */
    CODOL_STICK_ALL = 1,
    CODOL_STICK_FLESH = 0x20,
} codol_missile_stickiness_mode_t;
"""
    symbol = by_name(extract("x.h", source), "codol_missile_stickiness_mode_t", "enum")
    values = {v["name"]: v for v in symbol.detail["values"]}
    assert values["CODOL_STICK_NONE"]["comment"] == "no attachment"
    assert values["CODOL_STICK_FLESH"]["value"] == "0x20"


def test_definition_versus_prototype_and_language():
    header = extract("a.h", "int Foo(int x);\n", "c")
    source = extract("a.cpp", "int Foo(int x) { return x; }\n", "cpp")
    assert by_name(header, "Foo").kind == "prototype"
    definition = by_name(source, "Foo")
    assert definition.kind == "function"
    assert definition.detail["implementation"] == "cpp"
    assert definition.detail["params"] == [{"type": "int x", "name": "x"}]


def test_control_flow_is_not_mistaken_for_a_function():
    source = """
static void run(void) {
    if (ready(1)) { step(); }
    while (more()) { step(); }
}
"""
    names = {s.name for s in extract("a.c", source) if s.kind == "function"}
    assert "run" in names
    assert not names & {"if", "while"}


def test_asm_labels_skip_basic_blocks():
    source = """
	extern AngleDelta
	extern BG_CheckProne
global PM_Friction

PM_Friction:
	push ebp
PM_Friction_50:
	mov eax, 1
PM_MeleeChargeUpdate:
	ret
"""
    symbols = extract_asm("src/asmsource/_bg_pmove.asm", source)
    names = [s.name for s in symbols]
    assert names == ["PM_Friction", "PM_MeleeChargeUpdate"]
    friction = symbols[0]
    assert friction.kind == "function"
    assert friction.detail["implementation"] == "x86-asm"
    assert friction.detail["retained"] is True
    assert friction.detail["exported"] is True
    assert "AngleDelta" in friction.detail["imports"]


def test_looks_like_asm_distinguishes_inc_files():
    assert looks_like_asm("; comment\n%define PLAYER_EFLAG_TURRET_MASK 0x0c00\n")
    assert looks_like_asm("; generated\ndd codol_anim_move_43, 0xffffffff\n")
    assert not looks_like_asm("/* Measured C1.exe table */\n    {0x0000, -97, 0},\n")


def test_builtin_registration_extraction():
    source = """
    Scr_AddFunction("createprintchannel", GScr_CreatePrintChannel, 1 );
    Scr_AddFunction("log", CODOL_Scr_Log, 0);
    Scr_AddMethod("playerlinkto", CODOL_PlayerCmd_PlayerLinkTo, qfalse);
    Scr_AddFunction(name, (xfunction_t)&stubs.s[i].dyncallstub, qfalse);
"""
    found = {b["name"]: b for b in extract_builtins("src/scr_vm_main.c", source)}
    assert found["log"]["kind"] == "function"
    assert found["log"]["developer_only"] is False
    assert found["createprintchannel"]["developer_only"] is True
    assert found["playerlinkto"]["kind"] == "method"
    assert found["playerlinkto"]["binding"] == "CODOL_PlayerCmd_PlayerLinkTo"
    # A computed binding has no single owner and is skipped rather than guessed.
    assert len(found) == 3


def test_typedef_alias_records_its_underlying_tag():
    symbols = extract("server.h", "typedef struct client_s client_t;\n")
    alias = by_name(symbols, "client_t", "typedef")
    assert alias.detail["underlying"] == "struct client_s"
