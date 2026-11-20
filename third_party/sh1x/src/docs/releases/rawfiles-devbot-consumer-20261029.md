# Reviewed development bot-target consumer

Ported the actual `maintainBots` override from native historical commit
282614295896644322761e8e9ccd6d5241e4093b into the active rawfiles submodule's
`maps/mp/gametypes/_codol_bots.gsc`. Historical embedded native GSC is not staged.

Protocol confirmed with the native controls owner: an empty
`scr_codol_devbot_target` resumes normal playlist fill, while string `0` requests
removal of all training bots. The authenticated backend/operator sets the dvar
through existing RCON; native player/clients/cheats commands do not set it.

The consumer respects existing matchLimit/availableBots (including six-client
modes and an empty server's reserved human seat), caps development population
at sixteen including humans, and clamps nonnegative even when humans exceed
that cap. Negative synthetic test-human counts cannot raise the hard bot cap.
Survival's forcedTeam exclusion remains before the loop. Normal fill logic is
unchanged when the override is empty.

`node tests/rawfiles_devbot_target_test.js` executes an iteration of the actual
merged GSC body with native creation/removal boundaries. Empty/zero semantics,
exact spawn/removal, negative and overfull clamps, playlist capacity, reserved
human seat, normal-fill behavior and Survival exclusion pass. Full MP war and
Survival Favela source plus built-binary resolver gates pass. Script-only source
change; no deployment or real-client behavior claim.
