#ifndef SV_ASSETS_H
#define SV_ASSETS_H
/* Native effect ID stays stable; only its pre-gamestate wire binding moves.
 * Returns byte selector or -1. Valid only during full map initialization. */
int ReserveServerEntityEffect(unsigned effect);
#endif
