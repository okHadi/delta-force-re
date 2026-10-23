# Transient actor rendering

Build `mission_scene_transient_records` once per frame from the canonical `MissionTickState`. Keep the result alive through scene collection. The helper reads all 1024 physical `822E80` slots and uses each current actor image. It keeps active records in slot order. The renderer uses the same frame poses for their position, rotation, and animation fields.

The source count is fixed: `485290` pushes `400` hex and `822E80` at `4852EB..4852F5` before calling `485040`. Drawing does not use the insertion high-water count `8CFA14`. The source active gate reads actor `+00`; character selection uses the normalized definition pointer at `+04`. Queue identities use existing `actor_handle(4,slot)` tokens.

The snapshot provider and each active definition binding are required. Use the typed extra actor owner shared with insertion and expiry. Render snapshots are frame data. Current actor state stays in that owner.

Pass the vector through `MissionSceneLiveInputs32.runtime_records`. A bound empty vector sets `runtime_records_supplied` and clears the runtime-pool input gap. Keep other input gaps active until their providers are bound.

Retained source: `.local/reports/mission-transient-scene-source.json`, from the supplied executable with SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
