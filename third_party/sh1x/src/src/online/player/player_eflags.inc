; playerState eFlags are native C1 state in a CoDOL build.  C1 reserves
; 0x100/0x200 for Pmove's input/firing markers and uses 0x400/0x800 for the
; two turret modes.  Retained IW3 code must consume the C1 turret pair too.
%ifdef OL_SERVER
%define PLAYER_EFLAG_TURRET_MASK 0x0c00
%define PLAYER_EFLAG_TURRET_CLEAR_MASK 0xfffff3ff
%else
%define PLAYER_EFLAG_TURRET_MASK 0x0300
%define PLAYER_EFLAG_TURRET_CLEAR_MASK 0xfffffcff
%endif
