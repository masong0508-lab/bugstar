# BUGSTAR gameplay continuation — 2026-11-27

The user delegated a broad improvement pass instead of further save-only work. Maintain handheld/native parity and existing campaign saves.

1. Flight: one first-impact laser target, safe approach speed, explicit L+A combat near ports, cargo-cap parity and fuel bounds. Regressions: nearest object in a crowded pool, friendly exclusion, approach matrix.
2. Dungeons: retain speed-run budget in trip memory, ceil countdown display, gas/cave warnings, readable full-bag feedback. Regressions: wraparound/expiry, unchanged save schema and existing combat/spawn audits.
3. Android: keep existing save-transfer tests, inspect legacy picker status; follow up on mobile navigation / orientation and build delivery. Do not merge unverified code to main.

Decision: use existing C game mechanics and small HUD additions rather than a new renderer or save format. All modifications stay on the current development PR. No physical-phone FPS promises.
