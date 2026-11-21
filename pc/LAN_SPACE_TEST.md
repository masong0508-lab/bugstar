# BUGSTAR experimental LAN sparring + three-wave co-op mission (developer test)

## Graphical three-wave and Ready/reconnect milestone (experimental, pending CI)

The earlier real SDL smoke reached a shared first-raider victory, but was insufficient evidence of all three waves on both rendered game clients. This branch extends `pc/tests/lan_live.sh ./bugstar_pc coop` to drive the ordinary A-laser cadence in two actual SDL game processes, with an opt-in test-only aim-following camera; it requires log evidence of waves 1, 2 and 3, all three wins, both players' contribution and both final graphical victory PNGs. The successful captures will appear in the `bugstar-lan-graphical-smoke` CI artifact under `pc/out/lan_live`. The test does not pretend to simulate human flight or an Android touchscreen.

This branch upgrades the shared LAN packet version from **v3 to v4** for the new READY flag; both peers must use v4 binaries. Earlier v3 test results below are historical, not evidence for this change.

The **MULTIPLAYER LAN** options now include **CONFIRM READY / CANCEL READY**. HOST/JOIN establish the UDP socket, but each client must independently confirm Ready and stay in the same star 3D view for combat to begin. Leaving, switching world or losing current ship snapshots clears readiness. A rejoined client must confirm again; no network reward writes to a campaign save. The command-line `--lan-host`/`--lan-join` test path intentionally auto-confirms to preserve existing automated sparring tests.

These are **source changes awaiting build/runtime verification**, not a verified Android, Windows, GBA or GBC multiplayer release. Run `tests/pc_multiplayer_ready_test.c` and `pc/tests/lan_menu.sh` as well as the full SDL smoke. Recheck any regressions to frame pacing and ordinary offline play.


**Status (21 November 2026):** the PC ↔ PC two-process SDL game smoke now **passes in both sparring and co-op**, including the expanded protocol-v3 three-wave encounter at `49495e1024`. The two-real-UDP-socket mission test confirms host and guest receive the same final result. Builds for Windows, Android APK and GBA ROM were successful on that commit. **The new in-game LAN menu is committed but is still undergoing its own SDL event and cross-platform build verification.** Android hardware-to-PC networking has not been tested, and neither GBA nor GBC supports LAN UDP.
LAN mode is opt-in, available from the platform options overlay and from
command-line arguments. Normal play stays offline. This is not shared
campaign multiplayer: no campaign hull damage, inventory transfers,
persistent co-op rewards, co-op saves, kart networking, or Internet
matchmaking. Do not expose the UDP port to the public Internet.

The initial implementation supports:
- Two independently running PC instances (Windows/Linux). Android shares
  the SDL options UI and the UDP transport C code but still needs
  actual-device Wi-Fi / suspend / lifecycle testing.
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

**Version compatibility:** The expanded co-op mission uses a fixed 24-byte RAIDER payload and LAN **protocol v3**. Both PCs must run the same updated build; v2 peers are rejected. Both real PC instances completed their co-op combat smoke and the full 3-wave socket test at commit `49495e1024`. Future UI changes are gated by a separate SDL menu smoke test.

## Start from the in-game options menu

On PC, press **Esc** (gamepad: Back+Start); on Android, open the touchscreen
**MENU/MODE** options overlay. Select **MULTIPLAYER LAN**. Set **MATCH TYPE**
to **3-WAVE CO-OP** or **SPARRING** and pick UDP port 33777, 33778 or 33779.
Choose **HOST MATCH** on the first player. On the second machine, choose
**JOIN ADDRESS**, enter the host's numeric local IPv4 address using the
keyboard or Android text input, confirm with Enter, then choose **JOIN MATCH**.
**127.0.0.1** works only when both running games share a computer. Both
players must then enter 3D flight in the same star system. The overlay
shows HOST/JOIN, WAITING/CONNECTED and match type. Select **LEAVE MATCH**
from the same menu to disconnect and return to normal solo play without
writing any multiplayer state to the campaign save. Menu controls use SDL
mouse, keyboard, gamepad, and the Android touch overlay; menu behavior
is currently checked by a scripted SDL event test, with hardware testing
still required.

The port picker offers three known local ports for quick play. The command
line below remains available for arbitrary UDP ports.

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
`pc/tests/speed_guard.sh` must continue to pass with LAN disabled. The separate `pc/tests/lan_live.sh` must also pass a **real combat victory on each running game**, not just establish a UDP connection. `pc/tests/lan_menu.sh` exercises the actual SDL options panel through keyboard and text events: HOST, LEAVE, numeric IPv4 edit, JOIN, LEAVE.

## What makes the next iteration truly playable

1. Replace the current small guest-only spawn offset with an authoritative, collision-safe co-op assembly position; verify the view in real screenshots.
2. Improve native ship materials, occlusion and render-only smoothing.
3. Harden existing INPUT/EVENT/RAIDER exchanges with sequencing,
   reconnect/clock-skew handling, acknowledgement and rate controls;
   turn the one shared training raider into real AI with attacks and joint objectives, without trusting a client-supplied damage number.
4. Evolve the new options HOST/JOIN/LEAVE interface into an in-world
   preflight lobby with READY confirmations, latency indication and more
   descriptive timeout/error messages. Test Android suspend/reconnect.
5. Independent runtime screenshots and gameplay footage, packet-loss/
   latency tests, plus Windows/Android hardware smoke testing.
6. Separate GBA and GBC cable transports, with their own link budgets;
   the current UDP implementation does **not** run on those handhelds.

See [../MULTIPLAYER_PLAN.md](../MULTIPLAYER_PLAN.md) for all-platform
milestones and cross-play limitations.
