A good time is now, at late Phase 2 / start of Phase 3.

Best strategy by checklist phase:

Start in Phase 3–6 (recommended)
Add unit tests for pure logic first: Vec2 math, clamp/angle helpers (Phase 4), input state transitions (Phase 5), map queries/collision helpers (Phase 6).
These are stable, deterministic, and don’t require SDL windows.
Expand in Phase 7–12
Test player movement/collision rules, ray-step calculations, sprite sorting, entity interactions.
Keep renderer output mostly for smoke/integration tests, not strict pixel-perfect unit tests yet.
Before/within Phase 18
Make tests part of stabilization (“Native Game Loop Stabilization”) and CI quality gate.
By then, enough systems exist to justify broader integration tests.
So: introduce formal unit test infrastructure immediately, and grow coverage as each new logic module lands.