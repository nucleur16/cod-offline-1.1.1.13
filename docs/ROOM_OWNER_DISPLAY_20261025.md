# Display the confirmed room owner's public name

The RoomInfo label read the raw native room title, which can contain a private
launch identity such as `p69c7b...`. The existing native roster callback supplies
member display names and an owner role bound to each member's UID. When exactly
one current roster member has that role and a nonempty public name, render that
name in the label. When this client is the native host, use the unique current
`isMe` member instead; `LobbyController.refreshRoomStatus` sets `forces.isHost`
from native `IsRoomHost`. This also covers a local host whose per-member host
flags are all false. Otherwise retain the existing title.

The change writes only the text field. It does not mutate the room title, roster,
owner, UID, Ready state or any host-selection policy. In particular, a ranked
qualification contour exists with every native host flag false; changing Quick
host authority to require that flag would need stronger evidence. That experiment
was preserved separately and rejected. Both Quick controller sources and their
compiled tags remain unchanged in this candidate.

Validation:

- Execute the actual display AS body against 51 inputs including English/Chinese
  names, literal markup text, duplicate names/owners, stale UIDs, missing role/name
  data, absent rosters and local-host/all-false-role inputs. Input objects remain unchanged. The original raw-title
  body fails the private-name case.
- Existing shared UI baseline, Quick host-name and current Ready-cycle tests pass:
  eleven unittest cases including the new display cases. These use explicit native
  callback inputs and are not graphical C1 acceptance.
- FFDec 25.1.3 compiles the stored RoomInfo tag. Only that class changes in the
  reconstructed movie; all 6864 other tags remain byte-identical.
- Build the shared `voice_ui_mp.ff` candidate and unwrap/inspect its three rawfiles.
  The Codo movie matches the compiled candidate; the login movie and zone marker
  match their existing sources. The retained offline native archive-reader fixture
  replays all three assets successfully. That fixture is not a C1 renderer.

Movie SHA-256:
`bc5b6e29b7e6d49a383d0fe2c3e34d3da69ff1785baf8ce07add301010f66455`.
Candidate `voice_ui_mp.ff` SHA-256:
`9aa50c0d791f5da0e3d9189575e274b23578b9929a8cf7f39e7f8b8cbcb74ec1`.

Incident receipts retain source/tag hashes, original stock identity, compiler
attempts, the rejected controller experiment, final movie comparison and package
readback. On Windows this checkout's longest tag path exceeds the normal path
limit; a short task-local junction allows the existing compiler to read it.
The tag exists in Git; no missing-tag or compiler-policy source repair is included.

This is a built, offline-checked client asset candidate. Unknown owners and
all-false guest rosters retain the old label and remain a verification boundary. Graphical C1
acceptance, selected-file publication and the production release gate are pending.
