# BUGSTAR multiplayer: cross-port implementation plan
_Status: EXPERIMENTAL LAN SPARRING + SINGLE SHARED-RAIDER ENCOUNTER, two-game verification pending. Updated 20 November 2026._

This is the actionable multiplayer milestone plan for `masong0508-lab/bugstar`
(GBA core, Windows/Linux PC, Android) and `masong0508-lab/bugstar-gbc`
(Game Boy Color). The first LAN networking pieces now exist, but this is not complete multiplayer on every port.
The intent is for each supported port to have *real, usable multiplayer*,
rather than a menu entry that cannot connect.

## What ships on each port

| Target | First actually achievable multiplayer | Connection | First capacity | Cross-play |
| --- | --- | --- | --- | --- |
| PC (Windows/Linux) | Cooperative space flight + duel, then karts | Host/join LAN using UDP; Internet direct/relay in phase 2 | 2, later 4 | PC ↔ Android |
| Android | Same shared-session rules and gameplay as PC | Wi-Fi / mobile via UDP host or join, with Android lifecycle handling | 2, later 4 | Android ↔ PC |
| GBA hardware | Two-player space duel / co-op encounters; later link karts | Native GBA multi-serial link cable | 2 first; 4 only after bandwidth/CPU tests | GBA ↔ GBA; emulator bridge optional later |
| GBC hardware | Two-player space encounter / race ghost, then link race | Game Boy Color serial link cable | 2 | GBC ↔ GBC; emulator bridge optional later |
| Other targets | No verified build/transport found in these repositories | **Do not promise** until a port and transport actually exist | TBD | TBD |

**Important:** GBA and GBC do not have ordinary built-in Internet networking.
A PC/Android ↔ real GBA/GBC online session needs a **separate bridge device**
and a compatibility translation layer; merely sharing a C game engine does
not make it possible. The GBA four-player link cable must not be mistaken
for the two-player GBC cable. Cross-play of every mode on every console is
a stretch target, not a first release requirement.

## The first playable user journey

1. Choose **MULTIPLAYER** on the title or ship menu. Until a transport is
   implemented and tested, do **not** display a nonfunctional selection.
2. Choose **HOST** or **JOIN** (PC/Android: local room code or direct IP;
   handhelds: cable waiting/connecting). Show connected players and latency.
3. Select an encounter and team rules. Both sides confirm **READY**.
4. Host advances simulation at the agreed *game ticks*, not monitor FPS.
   Both ships move, aim, fire, take damage, collect shared drops and dock.
5. On leaving, either player may return to single-player safely. No session
   can permanently overwrite the original save, world seed, inventory or
   story flags.

## Shared session rules (engine independent of transport)

- **Shared semantic protocol, version 1.** `HELLO` (protocol/build/content
  hash, port capability mask, world seed), `READY`, `INPUT` (tick/sequence/
  held keys), `STATE` (tick/positions/orientation/hull/shield/HP), `EVENT`
  (hit/kill/pickup/dock/result), `ACK`, `PING`, `BYE`. Avoid sending
  framebuffer data or entire save files.
- **World-state authority:** PC/Android host-authoritative; link-cable
  handhelds start with a fixed two-player encounter where player 1 owns
  spawn and rewards, with deterministic local movement and confirmed hits.
  Never trust a remote message that directly increments gold, XP or items.
- **Fixed-tick simulation:** do not use SDL render-frame intervals to run
  ships, bullets or cooldowns. PC/Android existing simulation guard is
  approximately 59.7275Hz; send compact inputs per tick or grouped ticks.
  GBC uses its own hardware cadence, but exchanges *logical ticks* and
  quantized states, not wall-clock microseconds.
- **Network loss:** bounded packets, checksum/CRC, protocol version,
  sequence/ACK, timeout and a clean disconnect. On PC/Android: buffer a few
  input ticks, interpolate **render-only** remote actors, reconcile against
  the host. A temporary hitch must not speed up gameplay. For cable:
  pause or hold a safe input until both sides agree rather than silently
  running divergent boss logic.
- **Deterministic environment:** transmit only the world/encounter seed and
  compact changes. Separate cosmetic RNG and particle effects from
  gameplay RNG. Avoid running each platform's full procedural generator
  independently without a verified cross-port parity test.
- **Saves:** separate a multiplayer session profile from normal SRAM and
  PC files. Rewards are committed after explicit results validation only;
  never overwrite the active single-player save mid-session.
- **Abuse/robustness:** validate length/type/ranges and peer versions before
  indexing buffers. Apply rate limits on online ports and support disconnect
  without crashing, freezing, corrupting save RAM or blocking menus.
- **Usable controls:** Host/Join/Ready/Leave must all work with d-pad and
  gamepad; Android touch has equivalent reachable buttons. Show why a
  connection failed. Do not lock the player into the lobby.

## Implementation milestones and tests

### M0 — contract, budgets, instrumentation (NOW; not multiplayer yet)
- Land this document and the GBC companion `MULTIPLAYER_PLAN.md`.
- **Started (codec, not transport):** `net/session.h` and `net/session.c`
  implement the allocation-free v1 packet layout with nine message types, including RAIDER, 
  explicit lengths, a CRC8 and version checks. `tests/multiplayer_protocol_test.c`
  exercises 90,000 round trips and malformed data (PC/Android CI).
  This does not connect players or make a multiplayer mode playable.
- **Still to do:** add a deterministic replay fixture, bounded ACK/retransmit
  session state and per-platform transport adapters before connecting game AI.
- Record existing single-player state transitions: `space_frame()`,
  station docking, `ship_hit()`, firing, pickups, kart results and
  save/version ownership. Identify multiplayer-safe event hooks.
- Track bytes/tick, queue high water mark, input delay, RTT, stalls and
  FPS separately. No per-frame malloc and no added full-screen buffers
  on GBA/GBC. Re-run `pc/tests/speed_guard.sh`.
- Gate M0 on an isolated host test that encodes/decodes packets, rejects
  malformed input and reproduces 10,000 logical ticks byte-identically.

### M1 — PC ↔ Android, LAN, *two real ships*

**Experimental LAN sparring and shared-raider co-op (full two-game smoke verification pending):**
`pc/platform/pc_net.c` exchanges real UDP HELLO/READY, positions,
cumulative laser-input counters and host-written duel HP/wins; its
`pc/platform/pc_duel.c` validates the 3D firing direction and range.
The two players can independently move and fire A-lasers that cause
20 **sparring-only** damage; a five-hit knockout awards a round and
respawns duel HP after 120 game ticks. `pc/pc_multiplayer.inc` draws
the remote ship with camera-correct native-resolution wireframes,
movement smoothing, a short hit flash, score and correct P1/P2 labels.
The game uses its ordinary firing cooldown, but no shared real hull,
enemies or loot. The host approves all sparring hits (no client-provided
damage values). Match scores and HP are never written to single-player
saves. `pc/build.sh` includes this only for PC/Android builds, **not**
GBA; desktop opt in via `--lan-host` or `--lan-join IPv4`.
`tests/pc_multiplayer_net_test.c` tests localhost host/guest shots,
both victories, world checks and reconnect. The unit test covers misses
and respawn. The new `--lan-coop` flag on **both** PC peers enables one shared moving, armed 3D raider, separate from sparring: `pc/platform/pc_coop.c` owns its short-range 3D patrol and HP, the host validates both ships' A-laser shots, then broadcasts a `RAIDER` packet with target coordinates, health, hit counts and shared victories. `pc/pc_multiplayer.inc` draws the raider in its 3D world and displays a joint HP/win HUD. After five combined hits, it is defeated and respawns after 150 ticks. The raider can shoot back at the two pilots with predictable, dodgeable beams; each pilot has separate encounter-only shields, and the host also synchronizes a shared loss if both reach zero HP. The campaign hull, loot and saves are unchanged. The dedicated unit and UDP tests cover both pilots hitting, mode mismatches and a shared round result. Android's host/join UI is **not** built, and LAN has no Internet-facing authentication. Campaign enemy AI, cooperative mission scripting, docking, progression and cross-port cable support remain **unimplemented**.
All current code still awaits full game build/runtime verification.
[Developer instructions](pc/LAN_SPACE_TEST.md).
- First vertical slice: connect, leave, move in independent 3D space,
  see the other ship, shoot a moving target, damage, death, respawn,
  station docking and victory/defeat. Two local gamepads on one PC are
  useful for a smoke test but **not** a substitute for network testing.
- Provide a native UDP transport for Windows/Linux, reuse on Android
  with focus/suspend and Wi-Fi reconnect handling. Use one shared
  `net_session_step()` and keep rendering platform-specific.
- Test PC ↔ PC and PC ↔ Android; a scripted packet-loss/jitter test,
  one-minute soak, disconnect/rejoin, and a *real* screenshot from each
  player's perspective. Validate normal-time and high-refresh parity.
- Phase 2: direct-Internet/relay and input validation hardening.

### M2 — GBA link cable, two-player space combat
- Add an isolated SIO transport with VBlank-safe bounded exchange,
  connect/ready UI, timeout, clear disconnect and no save writes during
  battle. Use memory already budgeted by the single-player renderer.
- Transmit only quantized movement/aim/fire plus occasional authoritative
  event checksums. Cap per-tick bytes based on measured cable timing.
- Verify with a real link-capable emulator **and** two actual GBAs when
  possible. Test plug-out/reconnect, audio timing, line clipping, station
  entry, collision and HUD. Four players must be separately benchmarked
  before appearing in the UI.

### M3 — GBC link cable, two-player encounter
- Separate implementation in `bugstar-gbc`, sharing the *semantic*
  session contract but not falsely reusing GBA's 32-bit packet layout.
- Work within current 40-sprite limit and 10 sprites/scanline. The
  remote ship takes priority over cosmetic dust, not over the world or
  enemy telegraphs. Preserve bank 18 3D flight and SRAM layout.
- Start with low-rate inputs and sparse state correction. Test in a
  link-capable GBC emulator, then on two real GBCs; compare fps, musical
  timing and view alignment across a full docking/combat sequence.

### M4 — karts, co-op progression and cross-port modes
- PC/Android: 2–4 kart races, deterministic items, race countdown, laps,
  finish ordering, then co-op boss encounters and optional shared towns.
- GBA/GBC: small link-kart modes only after the space transport and
  screen budgets pass; same *rules*, but platform-appropriate visuals.
- Shared progression is opt-in and verified at result screen; no remote
  writes to story/world flags. Avoid live-syncing gigantic PC-only universe
  and colony states to tiny handheld RAM.
- A PC emulator bridge for retro multiplayer can be considered *after*
  authentic cable multiplayer exists; requires a measured, documented
  emulator protocol and compatibility matrix.

## Pass/fail bar for every port

- **Not done** until two independently running game instances connect,
  both can control their own ship, both see hits/damage, and both can
  disconnect/re-enter without losing progress.
- Stable input/gameplay speed independent of output resolution and refresh.
- No regressions to solo game, procedural seeds, GBA ROM/stack budget,
  GBC bank/WRAM usage, Android launch, PC native rendering or save format.
- Record emulator/device, build commit, screenshot, elapsed test time,
  measured fps/latency, packet loss and memory use for every milestone.
- Ship functionality behind a compile-time flag until the complete vertical
  slice passes on **that port**.

## Parallel 3D visual roadmap

- **Already built:** both the GBA path and PC/Android native path clip
  3D station/asteroid wireframe edges at the camera near plane.
- **Current pass:** native station/asteroid animations now use the same
  gameplay tick as their original GBA-scale geometry; render frames can vary
  independently without apparent object rotation drift. Station port ring
  projects 160 world units forward from
  its hull; at close distance four rotated struts connect that ring to
  its face. No meshes, texture loads, additional framebuffers, new
  procedural arrays or extra work for far-away stations.
- **Next:** PC/Android native-resolution ship silhouettes and consistent
  near/far occlusion, better readable docking alignment, asteroid volume
  shading and improved planet/moon limb contrast. Preserve CLASSIC fallback.
- **Then:** GBA selective depth cues and billboard LOD validated against
  the game-frame time; GBC pixel-accurate station/planet movement and
  remote-ship sprite priority. No unaudited fullscreen 3D work on GBC.
- For each feature run GBA camera projection audit,
  PC native projection/UBSan audit, GBC `tools/check_space3d.py`,
  render screenshots at front/back/edge/close flyby, and compare FPS.
