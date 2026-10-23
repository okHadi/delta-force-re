# Effect draw images

Supply each current projectile image in `SceneEffectRecord.image` and keep its source identity in `identity`. The full image supplies the active word, signed `+FC` counter, models, position, angles, flags, and callback fields. Supply normalized graphics handles at `+10` and `+14`.

Collection follows the `485290` effect path: require nonzero `+00`, positive signed `+FC`, and nonzero primary model `+10`; queue visible records in the near list. Collection ignores hidden bit 2. Draw selection uses current flags and the secondary model. The renderer keeps the full image with its queue identity for draw planning.

Pass the vector through `MissionSceneLiveInputs32.effects`. A bound empty vector clears `effect_table_not_supplied`. A reached draw requires a full image. Missing image data raises an explicit error.

`native_scene_effect_image` checks full-image collection and draw planning, current angle matrices, alternate model choice, source gates, and required image data. Original collection instructions are retained in `.local/reports/mission-transient-scene-source.json`; the existing model-visibility and model-instance checkers prove the projection and matrix bodies separately.
