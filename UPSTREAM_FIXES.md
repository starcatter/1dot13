# Upstream Fix Candidates

Keep fixes in this file independently extractable from the platform-port commits.

## Sector inventory sorting corrupts world items

Status: fixed locally and playtested on 2026-09-16; prepare a focused upstream PR.

`SortSectorInventory` in `Strategic/Map Screen Interface Map Inventory.cpp` used C
`qsort` whenever `_ITERATOR_DEBUG_LEVEL <= 1`. `WORLDITEM` is not trivially
relocatable: its `OBJECTTYPE` owns `std::list` storage for stacked objects and
attachments. Bytewise relocation by `qsort` corrupts the lists' ownership state.
The visible failure is delayed and misleading; observed symptoms included invalid
item temperatures during strategic time advancement and crashes in unrelated Lua,
rendering, save/load, and heap-management code after using sector inventory.

The local fix always uses `std::sort` with a typed comparator and preserves the
existing `CompareItemsForSorting` ordering. It also bounds the requested prefix to
the vector size. The old configuration conditional must not return: the local
clang-cl Debug configuration did not define `_DEBUG`, so MSVC's STL selected
`_ITERATOR_DEBUG_LEVEL == 0` and exercised the unsafe branch too.

Upstream PR checklist:

- Submit the inventory-sort change independently from the portability work.
- Explain that this is pre-existing undefined behavior, not a new file-I/O format
  or UTF conversion issue.
- Verify opening, manipulating, sorting, saving, and reloading sector inventory,
  followed by strategic time advancement.
- Retain the typed comparator so future changes cannot silently restore bytewise
  sorting of `WORLDITEM`.

## Item cooldown indexes attachments with the parent stack index

Status: fixed locally and playtested on 2026-09-16; include in a focused item
cooldown correctness PR or submit separately from the sorting fix.

`HandleItemCooldownFunctions` iterates each object in a parent stack with index
`i`, then visits that object's attachments. Each attachment `OBJECTTYPE` is its
own stack, but the old code read and wrote attachment element `i`. For parent
elements after index zero this can address beyond the attachment's actual stack
and corrupt adjacent state. The local fix uses attachment element zero for both
the temperature read and write while leaving the parent-stack iteration intact.

Upstream PR checklist:

- Exercise cooling on a multi-object gun stack where a nonzero parent element
  owns a gun or launcher attachment.
- Verify temperature changes on the attachment and no neighboring object state
  changes.
- Consider asserting the attachment stack count before indexing in a separate
  hardening change.

## Save loading can finish an attack against an already discarded world

Status: fixed locally on 2026-09-16; needs focused playtesting before an
upstream PR.

`LoadSavedGame` removes the old soldiers before `TrashWorld` deletes animation
tiles. A leftover bullet animation can therefore call `RemoveBullet`, then
`InternalReduceAttackBusyCount`, after the old soldier state is gone. The latter
reconstructs an attacker from global tactical state and may run suppression,
LOS, and attack-notification callbacks against the discarded world. The observed
crash reached `LightTrueLevel` through `NoticeUnseenAttacker` while loading from
`DoneFadeOutForSaveLoadScreen`.

The local fix treats attack-busy decrements received while
`LOADING_SAVED_GAME` is set as teardown notifications: it resets the stale
counter and returns without running gameplay callbacks.

Upstream PR checklist:

- Repeatedly quick-load during bullets, explosions, and enemy attack animations.
- Verify tactical saves still load normally and no attack-busy state survives
  into the loaded world.
- Submit independently from the application-host refactor; the crash occurs
  before application shutdown begins.

## Key binding parser can overrun its packed result

Status: fixed locally on 2026-09-17; suitable for a small hardening PR.

`ParseKeyString` packs parsed key bytes into one `int`, but previously kept
writing when a configuration value named more keys than fit in that integer.
The local fix stops packing at `sizeof(int)` while preserving the established
four-byte binding representation and all accepted key names.

Upstream PR checklist:

- Verify one- through four-key bindings retain their existing packed values.
- Feed a binding with five or more valid names and verify it truncates safely.
- Keep this separate from the SDK-header extraction; the bounds bug exists in
  the old implementation too.

## Multiplayer placement transmits an encoded direction as a live facing

Status: fixed locally on 2026-09-20; confirmed by a native client core dump and
the captured RPC payload. Verify whether current upstream still contains the
sender bug before preparing a focused PR.

`PutDownMercPiece` sets the soldier's real facing, adds `100` to that value for
the legacy `ubInsertionDirection` encoding, and then passed the encoded value to
`send_gui_dir`. The receiving client consequently called
`EVENT_SetSoldierDirection` with values such as `102`, although world directions
are limited to `0..NUM_WORLD_DIRECTIONS-1`. The observed failure corrupted
tactical structure state and crashed in `InternalAddStructureToWorld` while a
client placed its mercs.

The local fix preserves the real facing before applying the insertion encoding
and transmits that value. This is independent of SDL3_net; the old RakNet path
sent the same malformed payload.

Upstream PR checklist:

- Host a match with at least one remote client and place/reposition every merc.
- Capture or assert that every `gui_dir` payload contains a direction in `0..7`.
- Preserve the `+100` insertion-direction encoding locally; only the network
  facing must remain unencoded.

## Multiplayer placement RPCs trust stale soldiers and malformed payloads

Status: fixed locally on 2026-09-20; overlaps later upstream hardening changes,
so compare against current trunk before submitting.

`recieveguiPOS` and `recieveguiDIR` previously cast the incoming buffer without
checking its size, converted a wire soldier ID directly to a pointer, and
mutated that soldier without checking whether it was active and in-sector. The
direction handler also accepted arbitrary signed values. Placement and sector
loading create a normal race window in which those assumptions are false.

The local fix validates frame sizes, copies packed wire data into aligned local
objects, bounds the soldier ID and direction, rejects non-finite/off-map
positions, and requires an active in-sector soldier before applying either
event.

## Multiplayer lobby column widths include their screen offset

Status: fixed locally on 2026-09-20; suitable for a small UI correctness PR.

`InitializeMPCoordinates` correctly adds `UI_CHARLIST.Region.x` to each column's
X coordinate, but also added it to `MP_PLAYER_W`, `MP_TEAM_W`, `MP_COMPASS_W`,
and `MP_GAMEINFO_W`. At resolutions where the panel origin is nonzero, centered
player/team/edge text drifts into later columns and dirty rectangles become much
wider than the panel. The local fix keeps the widths at their intended constants
while offsetting only positions.

## Multiplayer M.E.R.C. pages advertise locked entries

Status: fixed locally on 2026-09-20; needs multiplayer laptop playtesting before
an upstream PR.

Multiplayer assigned `LaptopSaveInfo.gubLastMercIndex = LAST_MERC_ID`, exposing
the full M.E.R.C. page count, but did not set the corresponding
`StartMercsAvailable` flags. `GetAvailableMercIndex` then searched beyond the
small unlocked subset, producing repeated starting mercs and potentially
walking beyond meaningful availability data.

The local fix explicitly unlocks every otherwise-valid M.E.R.C. profile for the
non-progressing multiplayer hiring phase, updates both persistent and temporary
availability tables, and rebuilds the displayed array/count together. If
upstream does not intend all mercs to be selectable in multiplayer, the
alternative valid fix is to advertise only the unlocked count; the count and
availability set must not disagree.

## Stock magazine rows are mistaken for item IDs

Status: fixed locally on 2026-09-20; reproduced in multiplayer with a revolver
loaded with a 5.56 belt and a shotgun loaded with tank shells. This affects
stock-data single player too and is suitable for a focused compatibility PR.

`MagazineClassIndexToItemType` assumed that `Magazine[].uiIndex` always stores
an item ID. Newer `Magazines.xml` files may do that, but stock 1.13 data stores
the magazine-table row there. Returning the row as an item ID selects arbitrary
items. Guns can consequently spawn with impossible ammunition, and unloading
one reinterprets the remaining-round count as that item's condition.

The local fix accepts the direct mapping only when it names an `IC_AMMO` item
whose `ubClassIndex` points back to the requested magazine. Otherwise it resolves
the old format through the item table. It also prevents reload-all from merging
an ejected magazine into a stack of a different magazine type.

Upstream PR checklist:

- Test both stock and new-style `Magazines.xml` data.
- Create the default equipment for multiple revolvers, shotguns, rifles, and
  belt-fed weapons and validate item type, calibre, capacity, and ammo type.
- Reload a weapon from a mixed magazine stack and verify the ejected magazine
  is autoplaced or dropped rather than merged into an incompatible stack.

## Multiplayer movement passively trains physical stats

Status: fixed locally on 2026-09-20; reproduced by gaining health and strength
after ordinary tactical movement. Suitable for a focused multiplayer rules PR.

The normal exertion trainer in `DeductPoints` awards health and strength chances
for breath spent while moving. Multiplayer maps make that repeatable inside a
short match, and other passive/strategic stat sources can run while networked as
well. The local fix disables exertion training in multiplayer and centrally
rejects non-combat passive stat changes while retaining deliberate in-combat
skill gains such as shooting, first aid, and lock work.

## Unfocused multiplayer clients stop pumping the transport

Status: fixed locally on 2026-09-20; portable-host integration issue, not a
legacy 1.13 upstream candidate in its current form.

The SDL application loop suppresses full game frames while its window is
inactive. Network polling was attached to the full game frame, so an unfocused
client sent no heartbeat and the server correctly timed it out after 120
seconds. The local loop now runs a narrow background callback which pumps only
the multiplayer client/server transports; gameplay, presentation, and clocks
remain subject to the existing inactive-window behavior.
