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
