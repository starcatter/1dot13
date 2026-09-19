# Multiplayer port plan

## Scope

Restore the arena/co-op feature set shipped by this 1.13 branch without
preserving RakNet as an implementation detail. The first compatibility target
is two copies of the same native build on a trusted LAN. Historical RakNet wire
compatibility and mixed 32/64-bit peers are not goals.

The legacy game is a synchronous central relay: `client_packet()` and
`server_packet()` poll on the game thread, and registered handlers immediately
mutate engine globals. The portable transport must not invoke gameplay code on
a worker thread.

## Transport contract

- SDL3_net TCP streams provide reliable, ordered delivery.
- Connection identities come from the accepted socket, never a payload.
- Preserve legacy routing while the wrapper remains intact:
  - broadcast plus unassigned address sends to all peers;
  - broadcast plus a peer address sends to everyone except that peer;
  - targeted plus a peer address sends only to that peer.
- Synthesize the connection events consumed by the existing screens and pumps.
- Keep teardown non-blocking and callback-safe.
- Bound frames, buffered input, dispatch work, and file-transfer allocations.
- Treat content synchronization as its own framed service; an empty set must
  complete explicitly.

## Implementation sequence

1. **Portable transport foundation (complete):** the native build uses the
   hardened SDL3_net adapter and real loopback tests. The Wine oracle retains
   its no-op fallback.
2. **Legacy wrapper hardening (active):** fix known frame-length, string, index,
   sender, disconnect, and ownership bugs before exposing the menu for
   playtesting.
3. **Two-instance smoke:** host and join on loopback, complete settings/file
   negotiation, enter one tactical sector, exchange chat and basic actions.
4. **Refinement passes:** test turns, interrupts, projectiles, explosions,
   inventory, combat end, disconnect/reconnect, 3-4 peers, and content sync.
5. **Stable wire format:** replace raw ABI-dependent structs with versioned,
   fixed-width little-endian messages. Do not claim cross-architecture support
   before this gate.
6. **Cleanup:** remove the RakNet-shaped naming facade after the legacy wrapper
   no longer depends on it.

## 1v13 lessons carried forward

Multiplayer was imported wholesale in upstream commit `3e22dce31` (SVN r2144,
2008-05-12), primarily from Hayden's independent dedicated-server work. The
original design is a central relay whose host also connects a normal client to
localhost; it is not an authoritative game server. File synchronization was a
later addition made alongside the RakNet 3.401 update, not part of the original
gameplay transport contract.

The reference implementation first landed in `a43bcd686`, but later fixes are
part of the minimum baseline: empty-set completion (`1f5cba35f`), nonblocking
close/fair reads/reentrant callbacks (`fa852b09e`), heartbeat timeouts
(`8134421b8`), bounded sender-bound file ingress (`c64dd133f`), lifecycle
hardening (`8cb03da8d`), and fragmented-stream handling (`cba20292d`).

RakNet priorities, named RPCs, plugin APIs, and the raw-struct wire layout are
historical choices rather than game requirements. They remain only as a narrow
migration seam for the initial playable implementation.
