# Upstream Fix Candidates

Keep fixes in this file independently extractable from the platform-port commits.

## Animated strategic route retains a freed path node

Status: fixed locally on 2026-09-21; confirmed by AddressSanitizer when entering
a battle sector after issuing world-map travel orders. This is a pre-existing
base-1.13 bug in the route animator, present since the HAM 5.5 merge in commit
`c845f9f11`.

`TraceCharAnimatedRoute` retained a static `PathSt*` into a caller-owned linked
list. Entering the battle sector clears the involved squads' strategic paths,
but the animator survived into the next map frame. It advanced the dangling
node through `pCurrentNode->pNext` before reaching its later scan intended to
check whether the node had been deleted. The result was a deterministic
use-after-free in the map-screen route animation.

The local fix stores only the animation offset and a numeric path identity.
Each call resolves that offset from the current live list, resets safely for a
null, changed, shortened, or explicitly flagged edited route, and never retains
a node that path ownership can invalidate.

Upstream PR checklist:

- Plot a multi-sector squad route, trigger the pre-battle interface in the
  destination sector, and choose “Go to sector” under AddressSanitizer.
- Cancel, truncate, extend, and replace animated foot, vehicle, helicopter, and
  militia paths at different arrow-animation phases.
- Verify route arrows retain their pause/advance cadence and restart cleanly
  when switching the actively plotted route.
- Submit independently of SDL presentation/input work; the failing allocation,
  clear, and animation paths are entirely inherited strategic-map code.

## Strategic transport arrival continues after freeing its group

Status: fixed locally on 2026-09-21; confirmed by AddressSanitizer during
world-map time advancement. This is a pre-existing base-1.13 bug introduced by
the strategic transport-group feature in commit `fb03cba2f`.

When a transport group returns to the strategic-AI spawn sector,
`ProcessTransportGroupReachedDestination` transfers it to the reinforcement
pool and frees its `GROUP`. `StrategicAILookForAdjacentGroups` correctly reports
that the group was consumed, but `HandleNonCombatGroupArrival` discarded that
result because it returned `void`. `GroupArrivedAtSector` consequently continued
with the dangling pointer, first reading `usGroupTeam` in the militia-arrival
block. Disposable debug groups had a similar internal use-after-free after
`RemovePGroup`.

The local fix propagates a boolean “group consumed” result through the arrival
handler. The top-level caller clears its pointer and marks the group destroyed,
while live player, militia, and reassigned enemy groups retain their existing
movement behavior.

Upstream PR checklist:

- Advance time until an enemy transport reaches its target, returns home, and
  is transferred to the reinforcement pool.
- Run the arrival under AddressSanitizer and verify no access follows
  `TransferGroupToPool`/`RemovePGroup`.
- Exercise ordinary enemy patrols, player groups, militia groups, simultaneous
  arrivals, and debug groups to confirm only consumed groups report deletion.
- Submit independently of native-host work; the lifetime contract is entirely
  within base strategic movement code.

## Background drug users consume every eligible hour

Status: confirmed on 2026-09-21; pre-existing base-1.13 behavior, not caused by
the native port. Not changed locally pending a gameplay-compatibility decision.

`HourlyLarryUpdate` gives mercs with `BACKGROUND_DRUGUSE` no chance roll at all.
If a suitable consumable or bar is available, the merc consumes it on every
eligible hourly update. Commit `887170dda` removed the older temptation
accumulator while restructuring Larry's alternate personality, inadvertently
applying Larry's already-addicted behavior to all background drug users. The
same function also keeps `usTemptation`, `fBar`, and `pObj` outside the merc
loop, allowing one merc's discovered source to leak into later mercs.

Alcohol configured as both food and a drug produces two notifications for one
consumption: `ApplyFood` reports “drank alcohol” and `ApplyDrugs_New` reports
“drank some alcohol.” Maddog's repeated paired messages therefore represent one
guaranteed drink per processed hour, not a skewed native random-number stream.

Upstream PR checklist:

- Decide whether to restore the pre-`887170dda` temptation accumulator or add an
  explicit configurable hourly probability for non-Larry background users.
- Preserve Larry's intended immediate addicted-personality behavior separately.
- Reset all per-merc source and temptation state inside the merc loop.
- Emit one user-facing notification when an item is simultaneously food and a
  drug/alcohol source.
- Compare a fixed multi-hour scenario between 32-bit Windows and native builds;
  no platform-specific RNG adjustment is required for the current code path.

## Stacked corpse removal frees the next corpse's structure

Status: fixed locally on 2026-09-21; confirmed by AddressSanitizer while
quick-loading a tactical save. This regression was introduced in base 1.13 by
commit `78ed49c73` and is suitable for a focused upstream PR.

`RemoveCorpse` first calls `DeleteAniTile`, which removes the corpse's level node
and the exact structure owned by that node. A later cleanup added by
`78ed49c73` then called `FindLastStructure` on the corpse's grid and deleted that
result as well. When corpses are stacked, the result belongs to the next corpse.
Its animation node retains a pointer to the freed structure, producing a
use-after-free during later removal or world teardown.

The local fix remembers the current corpse's structure ID before deleting its
animation. It only performs fallback cleanup when that exact structure remains;
it never guesses ownership from another corpse found on the same tile.

Upstream PR checklist:

- Stack multiple corpses on both ground and roof tiles, then take them one at a
  time and confirm every corpse remains discoverable.
- Remove all stacked corpses through `TrashWorld` and saved-game loading under
  AddressSanitizer.
- Exercise corpse decapitation, vaporization, shifting, burial, and natural
  decay, all of which call `RemoveCorpse`.
- Keep the `FindLastStructure` selection from `5697acabb`, but do not use a
  second tile-wide lookup to decide which structure `RemoveCorpse` owns.

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

## Multiplayer synchronized RNG collapses on non-MSVC runtimes

Status: fixed locally on 2026-09-20; reproduced as several health and strength
increases while crossing roughly half a tactical map. Suitable for a focused
base-engine portability PR.

`MPPreRandom` scales full-width `UINT32` table entries with `RAND_MAX`. That
constant is only 32767 in MSVC but commonly 2147483647 in glibc. After the
32-bit multiplication wraps, the glibc calculation returns almost exclusively
zero or one for ordinary ranges. Low-probability checks consequently succeed
almost every time, affecting much more than stat progression. The local fix
uses the high half of a 32-by-32-bit product, producing the same deterministic,
well-distributed result on Windows and Linux without depending on the C runtime.

Upstream PR checklist:

- Add fixed-vector tests for zero, midpoint, and maximum `UINT32` inputs.
- Verify a synchronized table produces identical results in 32-bit Windows and
  64-bit Linux builds.
- Exercise combat hit, damage, item wear, and progression rolls in multiplayer;
  all use the synchronized stream.

## Explosion queue save field overruns its runtime counter

Status: fixed locally on 2026-09-20 in commit `f6d395a54`; confirmed by
AddressSanitizer while loading a tactical save. This is a pre-existing 1.13 bug
and the fix preserves the established Windows save format.

`gubElementsOnExplosionQueue` is a one-byte `UINT8`, but
`SaveExplosionTableToSaveGameFile` passed its address to `FileWrite` with a
four-byte `UINT32` size. Saving therefore read three bytes beyond the counter.
Loading performed the inverse four-byte write into the one-byte global and
overwrote the next three bytes of global storage. The local fix transfers the
on-disk value through a `UINT32` temporary, validates it against
`MAX_BOMB_QUEUE`, and converts it to `UINT8` only after validation.

Upstream PR checklist:

- Extract the `TileEngine/Explosion Control.cpp` hunk from `f6d395a54`; the
  other hunk in that commit fixes an unrelated opponent-list bug.
- Retain the four-byte field on disk so existing 32-bit Windows saves remain
  compatible.
- Round-trip saves with empty, partially populated, and maximum-sized explosion
  queues and compare the serialized field with the old format.
- Reject a corrupt queue count greater than `MAX_BOMB_QUEUE` before indexing
  `gExplosionQueue`.

## Public opponent memory decays beyond its lookup-table domain

Status: fixed locally on 2026-09-20 in commit `f6d395a54`; confirmed by
AddressSanitizer during a strategic update after loading a tactical save. This
is pre-existing gameplay logic, independent of the native port.

`DecayPublicOpplist` incremented `OLDEST_SEEN_VALUE` or decremented
`OLDEST_HEARD_VALUE` before asking `UpdatePublic` to forget the entry. That
temporarily creates an opponent-knowledge value outside the defined range.
`UpdatePublic` uses the value to index the 10-by-10 `gubKnowledgeValue` table,
causing an out-of-bounds read. The local fix forgets an entry while it still has
the oldest valid value; only younger entries are advanced.

Upstream PR checklist:

- Extract the `Tactical/opplist.cpp` hunk from `f6d395a54` separately from the
  explosion-save fix.
- Exercise both the oldest-seen and oldest-heard decay paths.
- Assert or exhaustively test that every value passed to `UpdatePublic` lies in
  `OLDEST_HEARD_VALUE..OLDEST_SEEN_VALUE` or equals `NOT_HEARD_OR_SEEN`.
- Advance strategic time after tactical combat and verify public knowledge is
  forgotten at the same turn as before.

## Surface-data lookup assigns instead of comparing

Status: fixed locally on 2026-09-20 as one line inside port commit `d016d012d`;
pre-existing source defect, but no dedicated legacy-runtime reproducer yet.
Submit only the minimal comparison fix, not that commit's portable-blitter work.

`SurfaceData::SetSurfaceData(HVSURFACE, BYTE*)` used
`if (sit->second = surface)`. For any non-null surface this mutates the first
registry entry and treats it as a match instead of searching for the requested
surface. The local code uses `==`. The overload appears uncommon, which likely
explains why this remained latent.

Upstream PR checklist:

- Apply only the `=` to `==` change in `sgp/vsurface.cpp`.
- Register at least two surfaces, lock the second by handle, and verify the
  returned buffer is associated with the second surface ID without modifying
  the first registry entry.
- Do not include `ClipRectangle.cpp` or portable raw-blitter changes from
  `d016d012d`.

## ClipRectangle width and height admit one pixel past the surface

Status: fixed locally on 2026-09-20 as part of `d016d012d`; pre-existing
inclusive-boundary inconsistency exposed while hardening the portable blitters.
Extract as a focused legacy bounds fix rather than cherry-picking the port
commit.

`ClipRectangle::Clip(x, y, width, height)` constructs an inclusive last pixel as
`x + width - 1`, but `SetRect(width, height, x, y)` formerly stored its inclusive
right and bottom boundaries as `x + width` and `y + height`. A rectangle ending
one pixel beyond the allocation could consequently be classified as in bounds.
The local implementation stores `x + width - 1` and `y + height - 1`, and treats
zero-sized rectangles as fully clipped.

Upstream PR checklist:

- Port the small `SetRect` and zero-size changes back to the location of
  `ClipRectangle` in current upstream; its move to `sgp/ClipRectangle.cpp` is
  organizational and not required.
- Test all four edges, one-pixel surfaces, and zero width/height.
- Audit callers that construct an `SGPRect` directly, since those retain their
  existing coordinate convention.

## Portable raw surface copies lost legacy bounds checks

Status: fixed locally on 2026-09-20 in `d016d012d`; port regression, explicitly
not a legacy 1.13 upstream candidate.

The portable replacement for `Blt16BPPTo16BPP` omitted the surface registry and
clipping performed by the old implementation. A shifted-overlay restoration
could therefore write before `BACKBUFFER`; glibc detected the damaged allocator
header only later while freeing the buffer during shutdown. This belongs with
the portable blitter rewrite. Do not include it in a base-1.13 bug-fix PR.

## Unfocused multiplayer clients stop pumping the transport

Status: fixed locally on 2026-09-20; portable-host integration issue, not a
legacy 1.13 upstream candidate in its current form.

The SDL application loop suppresses full game frames while its window is
inactive. Network polling was attached to the full game frame, so an unfocused
client sent no heartbeat and the server correctly timed it out after 120
seconds. The local loop now runs a narrow background callback which pumps only
the multiplayer client/server transports; gameplay, presentation, and clocks
remain subject to the existing inactive-window behavior.

## Delayed riposte taunt indexes the cooldown array with NOBODY

Status: fixed locally on 2026-09-21; confirmed by AddressSanitizer after an
enemy involved in tactical dialogue was removed before the quote finished.
This is a pre-existing 1.13 lifetime bug, independent of the native port.

When a merc finishes certain combat quotes, `HandleDialogueEnd` asks the merc's
previous attacker to deliver a riposte. Dialogue is asynchronous, so that
attacker may have died and had its soldier slot reset while the quote played.
The slot still has a stable address, but `SOLDIERTYPE::initialize()` changes its
embedded ID to `NOBODY`. `PossiblyStartEnemyTaunt` then used that sentinel as an
index into `uiTauntFinishTimes[TOTAL_SOLDIERS]`, reading exactly one element
past the global array. Its optional target lookup had the same stale-slot risk.

The local fix resolves both delayed participants through the active-soldier
boundary and verifies that the attacker is active, has an in-range ID, and is
still the soldier registered for that slot before any indexed access.

Upstream PR checklist:

- Extract the small changes in `Tactical/Dialogue Control.cpp` and
  `Tactical/Civ Quotes.cpp` independently of the portability work.
- Start a close-call/heavy-fire/taken-a-beating quote, remove the previous
  attacker before dialogue completion, and verify that no riposte is started.
- Repeat with a live attacker and confirm that valid ripostes still play.
- Exercise an attacker removal plus slot reuse to ensure a newly created
  soldier does not inherit the delayed riposte.

## Externalized reputation events are still treated as point values

Status: fixed locally on 2026-09-21; confirmed by AddressSanitizer when a
level-four merc died. This is a pre-existing 1.13 bug introduced by the 2014
snitch/reputation externalization commit `d5b4599e9`.

Before externalization, names such as `REPUTATION_SOLDIER_DIED` were signed
point constants, so multiplying one by the dead merc's experience level was
correct. The 2014 change turned those names into indices for
`gReputationSettings.bValues`, and changed `ModifyPlayerReputation` to perform
the lookup, but left the multiplication at the call site. A level-four death
therefore selected index `4 * 8 == 32` from a 32-byte array. Lower levels read
unrelated configured events and higher levels read farther out of bounds.
Merc-capture penalties had the same defect.

The same API mismatch made the snitch trait's direct configurable passive gain
act as a table index. Its default gain of 3 selected
`REPUTATION_POOR_MORALE` (normally -3), turning a promised daily gain into a
loss. The local API now distinguishes configured events from direct point
changes and applies experience level as a multiplier after event lookup. It
also validates every event index before accessing the settings table.

Upstream PR checklist:

- Extract the reputation API, death/capture calls, and passive-snitch call as a
  focused correction to `d5b4599e9`.
- Verify levels 1 through 10 apply the configured death and capture values
  multiplied by level without indexing another event.
- Verify a happy snitch applies `PASSIVE_REPUTATION_GAIN` as positive points,
  including zero and the configured maximum.
- Test reputation saturation at both 0 and 100.

## Bullet hit reactions invalidate the collision structure mid-flight

Status: fixed locally on 2026-09-21; confirmed by AddressSanitizer during a
brutal merc kill. This is a pre-existing 1.13 lifetime bug, independent of the
native port.

`MoveBullet` retains the hit merc's `STRUCTURE*` while calling `BulletHitMerc`.
A hit reaction can immediately change the soldier's animation surface, which
removes and recreates that world structure. If the bullet has enough impact to
continue, `MoveBullet` later inspects the freed pointer in its trailing riot
shield check. The local fix invalidates the collision pointer immediately after
the hit callback; the continuing bullet already excludes that structure from
the remaining collision candidates.

Upstream PR checklist:

- Extract the small `Tactical/LOS.cpp` change independently of portability work.
- Shoot a merc with a round that both triggers a death/hit animation change and
  retains enough impact to continue through the target.
- Repeat with front- and rear-facing riot shields to confirm the existing shield
  interception behavior is unchanged.
- Run the scenario under AddressSanitizer and verify that no old soldier
  structure is accessed after `BulletHitMerc` returns.

## Combat entry reads the old frame from a newly installed animation surface

Status: fixed locally on 2026-09-21; confirmed by AddressSanitizer when a sneak
attack transitioned into turn-based combat. This is a pre-existing 1.13
re-entrancy bug, independent of the native port.

`EVENT_InitNewSoldierAnim` installs the new animation surface and state before
it converts `usAniFrame` from the old surface. While ending an attack animation,
it calls `ReduceAttackBusyCount` in that transient state. That call can enter
combat and re-enter `EVENT_StopMerc`/`EVENT_SetSoldierDirection`, whose locator
update uses the stale frame number with the new surface. In the observed case,
frame 130 was read from a surface containing 128 subimages.

The dimension accessor already contained a partial range check, but returned
without initializing its output. The offset accessor had no range check at all.
The local fix gives both accessors safe locator defaults and makes the offset
lookup enforce the same subimage bound. The normal animation setup resumes and
recomputes the locator with the converted frame immediately after the nested
combat-entry call returns.

Upstream PR checklist:

- Extract the `Tactical/Soldier Control.cpp` accessor hardening independently
  of portability work.
- Trigger combat entry while a sneak/attack animation finishes and verify that
  the transitional locator update neither reads out of bounds nor changes the
  finished animation's final locator.
- Exercise invalid, unloaded, and zero-frame animation surfaces and verify that
  locator dimensions and offsets are always initialized.

## Logical-body rendering trusts the base animation's frame index

Status: fixed locally on 2026-09-21; confirmed by AddressSanitizer immediately
after an autofire animation. This is a pre-existing 1.13 rendering bug in the
logical-body-type path, independent of the native port.

`RenderTiles` uses the base soldier animation's `usAniFrame` for every logical
body layer. It dereferenced that index directly while calculating action-point
positions and dirty rectangles, without checking that the selected physical or
alpha overlay actually contained the same number of subimages. The blitters
cannot protect these earlier accesses. An incomplete overlay, or a transient
surface/frame mismatch during an animation transition, therefore reads beyond
the overlay's subimage table.

The local fix validates the physical and alpha subimage tables once before any
per-layer rendering or rectangle calculation and skips only the invalid layer.
It also clears the alpha-object handle at the start of each layer; previously a
layer without alpha data could accidentally reuse the preceding layer's alpha
surface.

Upstream PR checklist:

- Extract the small `TileEngine/renderworld.cpp` change independently of the
  portable blitters.
- Exercise autofire with logical body types enabled, including layered gun,
  gear, palette-swap, and alpha definitions.
- Test an intentionally incomplete overlay and verify that only that layer is
  skipped, without an out-of-bounds read or stale alpha mask.
- Verify dirty-rectangle rendering, action-point overlays, and normal full
  rendering use the same validated frame.

## Mouse map lookup reports the off-map sentinel as a valid grid

Status: fixed locally on 2026-09-21; confirmed by AddressSanitizer while
repeatedly quick-loading during incoming fire. This is a pre-existing 1.13
Big Maps integration bug, independent of the native port.

`MAPROWCOLTOPOS` returns `0xFFFFFFFF` (`-1` as `INT32`) when the converted mouse
coordinates fall outside the current world. `GetMouseMapPos` only treated zero
as failure, however, and cached the `-1` result as a successful grid lookup for
the remainder of the frame. Quick-load's post-load UI unlock forces a cursor
refresh while viewport and sector state are settling, making that window easy
to hit. `UIHandleMOnTerrain` then indexed `gpWorldLevelData[-1]`.

The local fix validates both freshly converted and cached positions against
`WORLD_MAX`, normalizes invalid results to zero, and returns failure to all
callers. Existing treatment of grid zero as invalid is preserved.

Upstream PR checklist:

- Extract the `TileEngine/Isometric Utils.cpp` validation independently of the
  native host and save-format work.
- Quick-load with the cursor on every viewport edge and while a tactical UI
  lock is active, then verify terrain handling never receives `-1`.
- Exercise non-default Big Maps dimensions and confirm the upper bound follows
  the current `WORLD_MAX`.
- Verify repeated same-frame calls return failure from the cached invalid
  position as well as from the initial conversion.

## Shattered windows leave a dangling bullet-collision pointer

Status: fixed locally on 2026-09-21; confirmed by AddressSanitizer during a
real-time burst. This is a pre-existing 1.13 lifetime bug, independent of the
native port.

`MoveBullet` correctly cleared the shattered window from its local collision
array after `BulletHitWindow`, because `WindowHit` swaps the window structure
for its partner and deletes the old `STRUCTURE`. It did not clear the separate
`pStructure` loop variable, however. A continuing round reached the trailing
riot-shield check in the same iteration and read the deleted window object.

The local fix invalidates the loop pointer after every directional window-hit
branch, matching the collision-array invalidation already present in the code.

Upstream PR checklist:

- Extract the small `Tactical/LOS.cpp` change independently of portability and
  the earlier merc hit-reaction lifetime fix.
- Fire penetrating single shots and real-time bursts through intact and cracked
  windows in all four travel directions.
- Confirm continuing rounds can still hit structures or soldiers beyond the
  window and that riot-shield interception remains unchanged.
- Repeat under AddressSanitizer and verify the pre-swap window structure is not
  accessed after `BulletHitWindow` returns.
