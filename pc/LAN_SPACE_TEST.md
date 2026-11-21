# BUGSTAR experimental LAN space presence (developer test)

**Status:** experimental two-player LAN **space sparring**, not campaign co-op. The duel logic is newly implemented and must pass the two-socket and game-build tests before being considered validated.
This mode is opt-in via command-line arguments. It is disabled in the
normal game. There is no in-game host/join menu, shared pirate/enemy AI, campaign hull
damage, inventory transfer, co-op saves, kart networking, or verified
Internet support. Do not advertise this as completed campaign multiplayer.

The initial implementation supports:
- Two independently running PC instances (Windows/Linux; Android shares
  the transport C code but has no host/join UI yet).
- A real nonblocking UDP socket on each instance, versioned HELLO/READY,
  CRC8 packet checking and world-system ID matching.
- A compact 16-byte state datagram about every **three gameplay ticks**
  (~20 Hz at the GBA's normal 59.7275 Hz) with 16-unit-quantized world
  coordinates and 4-angle-unit yaw/pitch.
- A five-vertex 3D silhouette marked **P2** in the same camera and
  perspective as rocks, pirates, and stations.
- A separate no-rewards **LAN sparring match**: pressing A to fire the
  real ship laser also submits a shot counter. The host checks aim against
  both 3D ship poses, grants 20 training damage per hit, and replicates
  P1/P2 HP (100 each), wins and a timed 2-second round reset. A disconnect
  or different star hides the session overlay. The real campaign hull, HP,
  cargo, save files, enemy AI and rewards are not shared or modified.
- A clean loss of the remote silhouette after stale/lost packets; BYE on
  normal shutdown. No cartridge SRAM/save modifications.

## Test on one computer first

Build as usual, and use **two different save directories** to avoid
colliding writes. Open two terminals:

Host:

```bash
cd pc
./build.sh
./bugstar_pc --lan-host --lan-port 33777 --data /tmp/bugstar-lan-host
```

Joiner:

```bash
cd pc
./bugstar_pc --lan-join 127.0.0.1 --lan-port 33777 --data /tmp/bugstar-lan-join
```

Both players must enter the *same star system* and **3D space flight**.
They will see a labeled **P1/P2** silhouette when reasonably close and
inside the other's view; both can fly independently, aim and shoot at
each other. The host synchronizes a separate LAN SPAR scoreboard and hit
points, but normal mission damage, enemy combat and rewards are still
single-player. Hit Return to menu or exit normally; the game
should remain playable offline. Two processes on different LAN computers
can instead use the host's local IPv4 address in `--lan-join`.
No names, DNS lookup, NAT traversal, public matchmaking or encryption.

Do **not** forward UDP port 33777 to the public Internet: this is a
trusted-LAN test with accidental-corruption detection (CRC8), not an
authenticated or encrypted network game. If host/join fails, check
Windows firewall/local UDP port, numeric address, and that only one
instance hosts each port. When moving to a different star system, the
peer silhouette intentionally disappears until both instances match.

## Verification commands

```bash
cc -std=c99 -O1 -Wall -Wextra -Werror -fsanitize=undefined \
  -fno-sanitize-recover=all \
  tests/pc_multiplayer_net_test.c pc/platform/pc_net.c net/session.c \
  -o /tmp/bugstar-lan-test
/tmp/bugstar-lan-test
```

The test launches two **real localhost UDP sockets**, proves
bidirectional independent ship-state exchange, quantization, different
world rejection, timeout/disconnect, handshake rejoin, both players
hitting each other, HP updates and round victories. The independent
`tests/pc_multiplayer_duel_test.c` checks front/back/side aim, range,
match wins and respawn. PC and Android CI compile and execute both tests. Existing renderer tests and
`pc/tests/speed_guard.sh` must continue to pass with LAN disabled.

## What makes the next iteration truly playable

1. Gameplay-authoritative host assigns distinct safe starting poses and
   prevents a new peer from spawning in the same location.
2. Improve native ship materials, occlusion and render-only smoothing.
3. Harden the existing INPUT/EVENT sparring exchange with sequencing,
   reconnect/clock-skew handling, acknowledgement and rate controls;
   expand from duel-only HP to validated shared enemy/hit/mission events,
   without trusting a client-supplied damage number.
4. UI with HOST/JOIN/READY/LEAVE, visible connection status and a LAN
   address on PC and Android. Same session can recover from suspend.
5. Independent runtime screenshots and gameplay footage, packet-loss/
   latency tests, plus Windows/Android hardware smoke testing.
6. Separate GBA and GBC cable transports, with their own link budgets;
   the current UDP implementation does **not** run on those handhelds.

See [../MULTIPLAYER_PLAN.md](../MULTIPLAYER_PLAN.md) for all-platform
milestones and cross-play limitations.
