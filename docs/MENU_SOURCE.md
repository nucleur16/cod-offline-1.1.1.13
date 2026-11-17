# Menu sources (`.menu`)

Every menu asset the zones carry is kept as `.menu` source text in the IW3/IW4 menu language (`menuDef` /
`itemDef`, `#include` / `#define`, event handlers, `visible when`, `exp ...`) and compiled by the zone build.
`zonekit/menu_source.py` is the compiler and decompiler (its docstring is the full keyword reference);
`zonekit/menus.py` holds the 1.1.1.13 struct layouts, the blob writer and reader and the 2.6.9.7 converter.

```
.menu text  --menu_source.compile_*-->  struct tree  --menus.encode_*-->  zone blob (what build_zones.py writes)
            <--menu_source.decompile_*--             <--menus.decode_*--
```

A decompile compiles its own text again and refuses to return text that does not give back the same tree
(`menus.normalize`: the tree as the zone stores it), so every `.menu` written by the tools is an exact source.

## Example

`zones/field_orders_postgfx_mp/menu/field_order_win.menu` (abridged):

```
{
    menuDef
    {
        name                 "field_order_win"
        rect                 0 0 640 1 1 0
        decoration
        menuExtra            1
        fadeCycle            1
        fadeClamp            1
        fadeAmount           0.1
        focusColor           0 0 0 0
        exp rect Y ( [ ( staticdvarbool "camera_thirdPerson" ) ] * ( 0 - 40 ) * [ ( ! [ ( staticdvarbool "splitscreen"
                ) ] ];
        onOpen
        {
            lerp alpha from 0 to 1 over 0.15;
            lerp x from - 640 to 0 over 0.15;
            close splash;
            close intel_found close perk_gained close killstreak;
        }
        itemDef
        {
            name                 "fo_header_complete"
            rect                 230 32 0 0 1 1
            decoration
            visible              1
            forecolor            1 1 1 1
            textfont             4
            textscale            0.47
            glowColor            1 0.8 0.6 1
            text                 "@FO_COMPLETE"
            editField
            floatExpressionsEmpty
        }
    }
}
```

and from a kiosk (`zones/survival_menus/menufile/kiosk_locker.menu`):

```
        onESC
        {
            if ( ( ( milliseconds ) - localvarint( "ui_quit_timer" ) ) >= 100 )
            {
                close self;
            }
        }
        execKeyInt 168
        {
            setLocalVarInt ui_quit_timer milliseconds;
            ...
```

## The language, in short

- **Window / menu / item keywords** are IW4's: `name rect style border borderSize ownerdraw forecolor backcolor
  bordercolor outlinecolor disablecolor background visible decoration popup outOfBoundsClick legacySplitScreenScale
  hiddenDuringFlash hiddenDuringScope hiddenDuringUI textOnlyFocus autowrapped font fullScreen fadeCycle fadeClamp
  fadeAmount fadeInAmount blurWorld focusColor allowedBinding soundLoop type align textfont textalign textalignx
  textaligny textscale textstyle text dvar dvarTest localvar focusSound feeder glowColor ...`, list boxes
  (`elementWidth elementHeight elementType columns notselectable noScrollBars usePaging selectBorder selectIcon
  doubleClick`), multi (`dvarStrList dvarFloatList`), `dvarEnumList`, edit fields (`maxChars maxCharsGotoNext
  maxPaintChars`), `enableDvar disableDvar showDvar hideDvar focusDvar { values }`.
- **Event handlers**: `onOpen onClose onRequestClose onESC` (menus), `mouseEnterText mouseExitText mouseEnter
  mouseExit action accept onFocus leaveFocus` (items), `execKey "c" { }` / `execKeyInt N { }` (both). Inside:
  script commands (`setdvar`, `open`, `close`, `exec`, `uiScript`, `scriptMenuResponse`, `play`, `lerp`, ...),
  `if EXPR { } else { }`, `setLocalVarBool|Int|Float|String name EXPR;`.
- **Expressions** end at `;` (after `if`, at `{`) and are written one token per stored entry: `name( args )` is
  IW4's call form, `( name args )` the form 1.1.1.13's own compiler stored, `[ EXPR ]` a nested statement operand,
  `1` an int and `1.0` a float operand, `-1` (together) a negative number.

## 1.1.1.13 extensions (no IW4 keyword exists for these)

| keyword | stores |
|---|---|
| `menuExtra N` | menuDef_t +W+8, an int IW4's menuDef_t does not have (1.1.1.13's HUD menus: 1; converted popups: 0) |
| `layoutRect x y w h horz vert` | an item's stored windowDef_t.rect when it is not its rect (rectClient) moved by the menu's origin |
| `rectClient ...` (menus), `textRect ...` (items) | those rects when the zone holds something other than 0 |
| `editField` / `listBox` | an edit-field / list-box data block whose fields are all 0 |
| `floatExpressionsEmpty` | an item whose floatExpressions pointer is set though it has no entries (2.6.9.7 conversions) |
| `staticFlags N` | static flag bits without a keyword; `dynamicFlags N`: the whole dynamic word when it is not what `visible` / `forecolor` imply |
| `staticdvarint` `staticdvarbool` `staticdvarfloat` `staticdvarstring` | the operator table's `dvarint(static)` .. `dvarstring(static)` |
| `[ EXPR ]` | an operand of type 3 (a whole statement evaluated on its own) |
| `dataType itemFlags dvarFlags cursorPos fontIndex cursorItem nextTime decayActive fx* lastSoundPlayedTime mousePos startPos endPos minValue maxValue defValue range paintOffset` | struct fields the game sets or derives, kept as the zone has them |

Not supported (the decompiler refuses them with a message): inline (non-reference) materials and sounds, menu
transitions, news ticker / text scroll item data, null event handlers, two script handlers in a row, script strings
not in IW's token form. None of the menus in this repository has any of them.

## What is a source and what is reference data

| file | role |
|---|---|
| `zones/field_orders_postgfx_mp/menu/*.menu` | source (manifest `menu,<name>,src=`), decompiled from the former `0001..0003_*.bin` blobs |
| `zones/survival_menus/menufile/*.menu` | source (manifest `menufile,..,src=`), 2.6.9.7's kiosks converted by `python -m zonekit.menus convert` (formerly `.json` + `.txt`); also the input of `tools/build_survival_kiosks.py` (the identical copies in `patch_common_gametype_survival/sources/` are gone) |
| `zones/hud_styles_postgfx_mp/menufile/hud_styles.menu` | written by `tools/build_hud_styles.py`, compiled by the zone build (formerly `.json` + `.txt`) |
| `zones/survival_hud_postgfx_mp/menufile/survival_hud_visibility.menu` | written by `tools/build_hud_styles.py` / `build_survival_hud.py` (formerly `.bin` + `.json` + `.txt`) |
| `zones/survival_hud_postgfx_mp/sources/survival_hud_visibility.original.menu` | 1.1.1.13's own killstreaks_hd / scorebar_hd, the generators' input (formerly `menufile/...original.json`) |
| `zones/survival_hud_postgfx_mp/components/survival_status.menu` | written by `tools/build_survival_status.py`, read by `build_survival_hud.py` (formerly `.bin` + `.json` + `.txt`) |
| `zones/patch_common_gametype_survival/menufile/kiosk_*.menu` | written by `tools/build_survival_kiosks.py`, compiled by the zone build (formerly `.bin` + `.json` + `.txt`) |
| `zones/hud_styles_postgfx_mp/sources/sm2.menu` | the HUD-style dialect `tools/build_hud_styles.py` reads (`menu_source.parse`), unchanged |
| `zones/hud_styles_postgfx_mp/sources/menus_{stock,v26,v33}.json`, `zones/survival_hud_postgfx_mp/sources/survival_scorebar_2697.json` | **kept as reference data**: CODOL-AssetExplorer's `zone-menus` dumps of the installs (written by `tools/hud_styles_extract.py`), the input the converters read. v26 / v33 / 2.6.9.7 are other builds' layouts and operator tables, which `.menu` (1.1.1.13's language) does not describe; `menus_stock.json`'s `minimap_fullscreen` names its zone's own inline materials, which have no source form; keeping them as dumped lets a re-extraction be diffed against them |

## Commands

```
python -m zonekit.menu_source decompile <tree.json | blob.bin> <out.menu> [--menu]
python -m zonekit.menu_source compile <in.menu> <blob.bin> [--name ui_mp/scriptmenus/x.menu | --menu]
python -m zonekit.menus convert --dump <2.6.9.7 zone-menus.json> --out <dir> <names>     # -> <dir>/<name>.menu
python -m zonekit.menus list <tree.json | menufile.bin>                                  # a debugging listing
```

## Verification

`tests/test_menu_source.py`: every `.menu` in the tree round-trips (compile, decompile to the same text, compile to
the same tree, encode and read back); a menu using every keyword round-trips; malformed input is refused with
`file:line`; the five zones that carry menus assemble to the pinned zone-image md5s of the build from the former
`.bin` / `.json` sources, and (with the game installed) to the pinned `.ff` md5s.

When the switch was made (branch `feat/menu-source`), every `.ff` of `python scripts/build_zones.py`, the staging
zones and `--parts` (43 files) was built on main and again from the `.menu` sources: all 43 are byte-identical.
