%include "src/asmsource/item_index.inc"
;Imports of bg_misctables:

;Exports of bg_misctables:
	global bg_itemlist


SECTION .text


;Initialized global or static variables of bg_misctables:
SECTION .data


;Initialized constant data of bg_misctables:
SECTION .rdata


;Zero initialized global or static variables of bg_misctables:
SECTION .bss
bg_itemlist: resb ITEM_COUNT*4


;All cstrings:
SECTION .rdata



;All constant floats and doubles:
SECTION .rdata

