# Live projectile scene records

Use `mission_scene_projectile_records` with the current 512-slot projectile owner. Pass its returned vector as `MissionSceneLiveInputs32::effects`. Keep the vector alive through the frame draw.

Each record has its physical source identity and a full current image. The collector uses image fields for the active and lifetime gates, model choice, pose, and draw data. A bound empty or inactive table is valid. Keep the full table size at 512.

The source loop at `48531B` checks signed `+FC > 0` and nonzero `+00`. Keep the source draw gates separate from the projectile update's Z check.

Evidence: `.local/reports/mission-transient-scene-source.json`, source `Df.exe` SHA-256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
