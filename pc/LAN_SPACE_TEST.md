# BUGSTAR experimental LAN space presence (developer test)

**Status:** first network vertical slice, **not yet full multiplayer**.
This mode is opt-in via command-line arguments. It is disabled in the
normal game. There is no host/join menu, synchronized damage, shared
enemy AI, inventory transfer, co-op saves, kart networking, or verified
Internet support. Do not advertise it as a completed multiplayer mode.

The initial implementation supports:
- Two independently running PC instances (Windows/Linux; Android shares
  the transport C code but has no host/join UI yet).
- A real nonblocking UDP socket on each instance, versioned HELLO/READY,
  CRC8 packet checking and world-system ID matching.
- A compact 16-byte state datagram at most every **three gameplay ticks**
  (~20 Hz at the GBA's normal 59.7275 Hz) with 16-unit-quantized world
  coordinates and 4-angle-unit yaw/pitch.
- A five-vertex 3D silhouette marked **P2** in the same camera and
  perspective as rocks, pirates, and stations.
- A clean loss of the remote silhouette after stale/lost packets; BYE on
  normal shutdown. Does not touch cartridge SRAM/save files or solo
  combat/hit/loot logic.

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
They will see a labeled P2 silhouette when reasonably close and inside
the other's view; each player can fly independently. There is no remote
damage/mission sync yet. Hit Return to menu or exit normally; the game
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
world rejection, timeout/disconnect and handshake rejoin. PC and
Android CI compile and execute it. Existing renderer tests and
`pc/tests/speed_guard.sh` must continue to pass with LAN disabled.

## What makes the next iteration truly playable

1. Gameplay-authoritative host assigns distinct safe starting poses and
   prevents a new peer from spawning in the same location.
2. Proper remote ship mesh/material with native-resolution overlay.
3. Versioned INPUT and EVENT packets with input sequencing, damage/hit
   validation and bounded retransmission; both instances display the
   same target HP, mission result and rewards.
4. UI with HOST/JOIN/READY/LEAVE, visible connection status and a LAN
   address on PC and Android. Same session can recover from suspend.
5. Independent runtime screenshots and gameplay footage, packet-loss/
   latency tests, plus Windows/Android hardware smoke testing.
6. Separate GBA and GBC cable transports, with their own link budgets;
   the current UDP implementation does **not** run on those handhelds.

See [../MULTIPLAYER_PLAN.md](../MULTIPLAYER_PLAN.md) for all-platform
milestones and cross-play limitations.
