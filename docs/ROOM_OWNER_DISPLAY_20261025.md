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
was preserved separately and rejected. The paired Quick correction is documented
in `QUICK_PRIVATE_HOST_IDENTITY_20261025.md`; it uses native self UID for a local
host and retains the signed-title path for guests, preserving all-false roles.

Validation:

- Execute the actual display AS body against 51 inputs including English/Chinese
  names, literal markup text, duplicate names/owners, stale UIDs, missing role/name
  data, absent rosters and local-host/all-false-role inputs. Input objects remain unchanged. The original raw-title
  body fails the private-name case.
- Existing shared UI baseline, Quick host-name and current Ready-cycle tests pass:
  twelve unittest cases including the coupled identity and display cases. These use explicit native
  callback inputs and are not graphical C1 acceptance.
- FFDec 25.1.3 compiles the stored RoomInfo tag. That individual change preserves
  all 6864 other tags. The subsequent coupled Quick correction compiles three
  method bodies in two additional tags, preserving this display tag.
- Build the shared `voice_ui_mp.ff` candidate and unwrap/inspect its three rawfiles.
  The Codo movie matches the compiled candidate; the login movie and zone marker
  match their existing sources. The retained offline native archive-reader fixture
  replays all three assets successfully. That fixture is not a C1 renderer.

Movie SHA-256:
`c0ac41b419cfe9f273fcb4d27bc74e0e246fa121896d5d0239de74cd57ff600e`.
Candidate `voice_ui_mp.ff` SHA-256:
`30061852c0d7a6484a00999ef5f88924356cee7fb7b068bbd1ad0166304740c8`.

Incident receipts retain source/tag hashes, original stock identity, compiler
attempts, the rejected controller experiment, final movie comparison and package
readback. On Windows this checkout's longest tag path exceeds the normal path
limit; a short task-local junction allows the existing compiler to read it.
The tag exists in Git; no missing-tag or compiler-policy source repair is included.

This is a built, offline-checked client asset candidate. Unknown owners and
all-false guest rosters retain the old label and remain a verification boundary. Graphical C1
acceptance, selected-file publication and the production release gate are pending.
