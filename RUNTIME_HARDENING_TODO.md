# Runtime hardening TODO

This list tracks runtime bugs found while playtesting the portable build.  Keep
the fixes separable from the platform port where possible, and record genuine
base JA2 1.13 fixes in `UPSTREAM_FIXES.md` once their root cause is confirmed.

## P0: sandstorm frame collapse and stale cursor playback

Status: implemented; awaiting live sandstorm playtest.

### Root causes

- The SDL host consumed exactly one event per main-loop iteration.  SDL keeps
  individual mouse-motion events, whereas the former Windows path effectively
  supplied the latest position.  Once rendering took longer than a scheduler
  period, every iteration rendered before consuming the next stale position.
- Sandstorms use eight times the resolution-scaled rain particle count.  Each
  update also clears an RGB565 screen-sized weather surface and color-key
  composites it over the back buffer.
- Portable `PixelSurface` fills and keyed blits performed several tiny helper
  operations per pixel.  ASan amplified that overhead enough to expose the
  event-consumption flaw particularly clearly.

### Implemented hardening

- Coalesce each bounded contiguous SDL mouse-motion run to its latest absolute
  position while preserving ordering around button, key, focus, and quit
  events.  A hard bound prevents live motion from monopolizing the host pump.
- Use row operations for RGB565/indexed fills and specialized aligned and
  unaligned keyed-blit loops.  Keep odd-pitch behavior covered by tests.
- Focused host tests verify coalescing and discrete-event ordering.
- A 1920x1080 weather-shaped composite benchmark improved from 5.17 to
  1.15 ms/frame optimized and from 27.72 to 2.43 ms/frame under ASan.

### Remaining validation

- Verify the current sandstorm no longer accumulates cursor latency under
  rapid motion and clicking.
- Check rain, thunderstorms, snow, dialogue overlays, overhead map, and the
  shopkeeper screen for unchanged weather clipping.
- Profile the complete weather frame in an optimized build before changing
  particle density or visual behavior further.

## P0: world-map route animation uses a freed strategic path

Status: fixed under ASan; awaiting reproduction playtest.

- Entering a battle sector clears the involved squad paths, but
  `TraceCharAnimatedRoute()` retained a static pointer to one of their nodes.
- The next map frame followed `pCurrentNode->pNext` before the function's old
  deletion check, producing a heap use-after-free.
- The animator now retains an offset and numeric route identity, resolving the
  node anew from the live route on every call.
- Recheck squad, vehicle, helicopter, and militia route editing as well as the
  original travel-order/pre-battle/enter-sector sequence.
- This is an inherited base-1.13 defect and is recorded in
  `UPSTREAM_FIXES.md`.

## P0: map-screen controls remain locked after tactical banter

### Reproduction

1. Trigger multi-line merc banter in tactical combat (observed after a lethal
   head shot).
2. Enter the map screen while the dialogue is still playing.
3. The map accepts some interactions, but tactical, load, and other bottom-bar
   exits remain disabled.  Movement orders are rejected because the squad is
   in combat.

### Investigation notes

- `AllowedToExitFromMapscreenTo()` disables exits while the dialogue queue is
  non-empty or the pause state is locked.
- Tactical dialogue normally pairs `LockPauseState(15)` with an unlock only
  after the current face stops talking and `fDoneTalking` is processed.
- The tactical-to-map handoff in `HandleDialogue()` removes/recreates speech
  UI state while the face is live.  Head-shot reactions can enqueue more than
  one quote, so both the active face and the queue must survive this handoff.
- Map-screen face progression (`HandleAutoFaces()` and
  `HandleTalkingAutoFaces()`) is skipped whenever
  `fDisableDueToBattleRoster` is true.  Pre-battle initialization sets exactly
  that flag.  Opening the pre-battle interface during a live quote can
  therefore stop the face before dialogue gets to its pause-lock cleanup.
- This is currently a lifecycle/state-machine bug hypothesis, not yet a proven
  single bad line.  It is not explained by SDL input translation.

### Work

- Add a bounded dialogue-transition trace containing the current screen,
  queue state, current face/talking state, `fDoneTalking`,
  `fWasPausedDuringDialogue`, `ENGAGED_IN_CONV`, and pause-lock state.
- Reproduce while switching to/from the map during each part of a two-quote
  head-shot exchange, with speech and subtitles independently enabled.
- Give one code path ownership of the tactical/map face handoff; do not let
  both map entry and `HandleDialogue()` partially recreate it.
- Ensure every dialogue pause lock has one matching unlock, including a face
  that ends while its UI is being moved between screens.
- Add an invariant/recovery check for an empty dialogue queue, no talking face,
  and a dialogue-owned pause lock that remains held.

## P0: invisible pre-battle/auto-resolve interface

### Reproduction

1. On the map screen, click the battle-sector locator.
2. The pre-battle panel and auto-resolve button do not appear.
3. Subsequent clicks may do nothing and map-screen exits may remain disabled.

### Investigation notes

- `InitPreBattleInterface()` hides all three buttons and marks the pre-battle
  interface active before the transition has rendered and exposed the panel.
- `DoTransitionFromMapscreenToPreBattleInterface()` can return early when the
  extra buffer is unavailable.  Rendering is also responsible for showing the
  buttons and clearing `gfIgnoreAllInput`.
- Once active, another `InitPreBattleInterface()` call immediately returns.
  `AllowedToExitFromMapscreenTo()` meanwhile blocks exits whenever the
  interface is active or battle-roster controls are disabled.
- Disabling the battle roster also suppresses map-screen face updates, so this
  interface can deadlock an in-progress dialogue and inherit its input/pause
  lock.  The two reported symptoms may consequently be one ordering bug even
  though each subsystem also has an independent incomplete-state hazard.
- An interrupted/failed first render therefore leaves a valid-looking active
  flag around an invisible and non-interactive interface.  This matches the
  observed symptom, but the exact failed transition still needs a trace from a
  reproduction.

### Work

- Trace init, transition, first render, action, and teardown with:
  `gfPreBattleInterfaceActive`, `gfRenderPBInterface`,
  `gfPBButtonsHidden`, `gfIgnoreAllInput`, `fDisableDueToBattleRoster`,
  `fDisableMapInterfaceDueToBattle`, `gfExtraBuffer`, current screen, and
  battle sector.
- Replace the loose booleans with an explicit lifecycle (inactive, preparing,
  presented, closing), or enforce equivalent invariants in one owner.
- Make initialization transactional: failure before presentation must destroy
  created resources, restore map controls/input, and return to inactive.
- Make a repeated click recover or complete a prepared-but-not-presented
  interface instead of silently returning.
- Provide a no-animation first-render fallback; presentation must not depend
  on the zoom transition or extra buffer succeeding.
- Test normal battles, persistent enemy-arrival prompts, militia-only battles,
  entering from tactical, and opening while dialogue is active.

## P1: popup and inventory-popup hardening

### Immediate memory-safety work

- Initialize `bestStack` in `Tactical/Interface Items.cpp` before the ammo
  popup searches for a candidate stack.
- Validate an item is ammunition before indexing `Magazine[...]` in the quick
  ammo callback.
- Stop treating popup labels as `swprintf` format strings and replace the
  fixed 128-character copies with bounded literal-string copies.
- Reconcile the 96-entry `MenuRegion` array with the allowed 96 options plus
  submenu regions; reject overflow before defining mouse regions.
- Check popup lookup results before region callbacks dereference them.
- Remove the heap allocation leaked by `registerPopupRegion()`.
- Do not retain raw pointers into `pInventoryPoolList` across vector mutation;
  store a stable key/index and revalidate it at callback time.
- Enforce the XML submenu-depth limit before writing the parser stack.

### Ownership and API cleanup

- Make XML popup definitions explicitly movable/non-copyable or give their
  child definitions real value ownership; current raw-pointer ownership is
  shallow-copyable.
- Replace callback-function-pointer erasure through `void*` with typed callback
  storage.
- Give shared popup graphics a balanced, idempotent load/unload lifecycle.
- Add focused tests for maximum option/submenu counts, missing popup IDs,
  malformed/deep XML, percent signs in labels, inventory-pool mutation, and
  repeated open/close cycles.

## Validation policy

- Reproduce each state bug under the native ASan build before changing it.
- Add a focused regression test or debug invariant where the subsystem can be
  exercised without a full playthrough.
- Playtest screen transitions, combat dialogue, battle entry/auto-resolve, and
  all affected popup types.
- Classify confirmed platform-independent fixes in `UPSTREAM_FIXES.md` for
  submission to the JA2 1.13 project.
