# BUGSTAR experimental LAN sparring + three-wave co-op mission (developer test)

**Status (21 November 2026):** the PC ↔ PC two-process SDL smoke test now **passes in both sparring and co-op**. In the first verified co-op smoke, the host and guest each logged three synchronized raider kills, and both pilots contributed real laser hits. Windows PC, Android APK and GBA ROM **builds** were green at protocol v2 commit `e4aca761a2`; this does not mean an Android device was tested for multiplayer, or that GBA has link networking. The newer ship-model, network hardening and tick-decoupling work is under CI re-verification.
This mode is opt-in via command-line arguments. It is disabled in the
normal game. There is no in-game host/join menu, shared campaign pirate/enemy AI, campaign hull
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
- An eight-vertex 3D interceptor silhouette labeled **P1/P2** in the same camera and
  perspective as rocks, pirates, and stations.
- A separate no-rewards **LAN sparring match**: pressing A to fire the
  real ship laser also submits a shot counter. The host checks aim against
  both 3D ship poses, grants 20 training damage per hit, and replicates
  P1/P2 HP (100 each), wins and a timed 2-second round reset. A disconnect
  or different star hides the session overlay. The real campaign hull, HP,
  cargo, save files, enemy AI and rewards are not shared or modified.
- The **three-wave co-op mission** runs only on the host. Wave 1 has the familiar moving five-hit RAIDER. Waves 2 and 3 add an independently moving, attack-capable flank DRONE (three and four hits respectively). Clear *both* active targets to unlock the next wave. Three victories show a synchronized MISSION COMPLETE result; a team defeat restarts the contract. Neither opponent can change campaign hull, XP, inventory or saves. This is a standalone trial mission, not shared campaign progression.
- A clean loss of the remote silhouette after stale/lost packets; BYE on
  normal shutdown. A recent enemy score cannot make an old 3D ship position fresh, and replayed guest shot counters are rejected. Game-clock networking continues between rendered flight frames. No cartridge SRAM/save modifications.

**Version compatibility:** The expanded co-op mission uses a fixed 24-byte RAIDER payload and LAN **protocol v3**. Both PCs must run the same updated build; v2 peers are rejected instead of misreading enemy data. The previous full-game verification was on v2; this three-wave revision still requires its own CI pass.

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

## Opt in to the shared-raider encounter

Start the same two PC processes shown above, but add `--lan-coop` **on both sides**. Use the same star and separate save directories. In wave 1, shoot the moving RAIDER five times between both pilots. For waves 2 and 3, rotate toward the separate flank **DRONE** and destroy both targets. The HUD shows RAIDER and DRONE health, wave progression, both pilots' encounter-only shields and hit contributions. Enemy shots announce the targeted pilot before firing; move sideways to dodge. Clear wave 3 to display **MISSION COMPLETE** on both machines. A sparring client and a co-op host deliberately cannot connect.

## Verification commands

```bash
cc -std=c99 -O1 -Wall -Wextra -Werror -fsanitize=undefined \
  -fno-sanitize-recover=all \
  tests/pc_multiplayer_net_test.c pc/platform/pc_net.c \
  pc/platform/pc_duel.c pc/platform/pc_coop.c net/session.c -lm \
  -o /tmp/bugstar-lan-test
/tmp/bugstar-lan-test
```

The test launches two **real localhost UDP sockets**, proves
bidirectional independent ship-state exchange, quantization, different
world rejection, timeout/disconnect, handshake rejoin, both players
hitting each other, HP updates and round victories. The independent
`tests/pc_multiplayer_duel_test.c` checks front/back/side aim, range,
match wins and respawn. `tests/pc_multiplayer_coop_test.c` and `tests/pc_multiplayer_coop_net_test.c` additionally test three-stage mission completion, two independently aimable targets, and the second drone's synchronized 3D pose, HP and wave status. PC and Android CI compile the tests. The PC test also boots two complete SDL game windows in sparring and co-op modes, captures screenshots and checks both entered 3D space. Existing renderer tests and
`pc/tests/speed_guard.sh` must continue to pass with LAN disabled. The separate `pc/tests/lan_live.sh` must also pass a **real combat victory on each running game**, not just establish a UDP connection.

## What makes the next iteration truly playable

1. Replace the current small guest-only spawn offset with an authoritative, collision-safe co-op assembly position; verify the view in real screenshots.
2. Improve native ship materials, occlusion and render-only smoothing.
3. Harden existing INPUT/EVENT/RAIDER exchanges with sequencing,
   reconnect/clock-skew handling, acknowledgement and rate controls;
   turn the one shared training raider into real AI with attacks and joint objectives, without trusting a client-supplied damage number.
4. UI with HOST/JOIN/READY/LEAVE, visible connection status and a LAN
   address on PC and Android. Same session can recover from suspend.
5. Independent runtime screenshots and gameplay footage, packet-loss/
   latency tests, plus Windows/Android hardware smoke testing.
6. Separate GBA and GBC cable transports, with their own link budgets;
   the current UDP implementation does **not** run on those handhelds.

See [../MULTIPLAYER_PLAN.md](../MULTIPLAYER_PLAN.md) for all-platform
milestones and cross-play limitations.
