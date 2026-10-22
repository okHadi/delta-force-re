# Mission ground geometry

Create `MissionGeometry(state, cache, assets)` after mission resources load.
Keep the cache alive while geometry is in use. Pass each current actor image
and the query position to `ground_entity()`. Keep its result alive while using
the `GroundEntity` view. `ground_entities()` builds owned views in list order.
Use `bounds(handle)` for the model header fields `+64` and `+60`.

The provider reads actual first-LOD mesh bounds, 72-byte faces, face planes,
normals, vertices, and material flags. It applies the live actor transform,
the mesh and face XY gates, material bit `01000000`, normal-Z threshold `40`,
and the original XY triangle test. Pass the views to `ground_entity_height()`
for terrain, depth, and highest-surface selection.

The `gnrc` matrix callback uses the live actor transform. Supply
`MissionGeometryMatrixSource` for other tags. Return each first-LOD matrix and
its instance `+3C` flag byte. Set `present=false` for a null callback result.
Missing special-tag state raises `std::invalid_argument` after the radius gate.

Use `mission_geometry_callback_matrices()` to run known first-LOD callbacks.
Supply current CHR and pose for non-null `org0` instances. Supply prior scratch
flags for `org0` and `tire`; these callbacks retain flag bytes. Supply the copied
stack flag for `palm` selectors `10/11/20`.
Ground tests consume flag bits 0/1; world hits consume bit 0.
The original `org0` callback returns bone-local matrices. Its draw callback applies actor
placement in a separate step. The geometry provider follows the query callback.

`world_hit_records()` preserves every input slot and returns mesh views backed
by `MissionGeometry`. Each record owns its actor snapshot. Matrix callbacks run
after the query passes its bounds and sphere gates. Keep geometry alive while
using the records. Use current group 1
and group 0 images for the two lists passed to `world_segment_query()`. World
hits use primary model `+10`, radius `+60`, and inverse first-LOD matrices.

`mission_geometry_probe MODEL.3DI` reads one query per line:
`translation[3] angles[3] query[3] terrain depth flags primary alternate`.
Handles 1 and 2 select the supplied model. Output is
`radius_64 height_60 ground_height entity_hit mesh_count`.

Original instruction replay matches 2,580 queries on seven stock models,
including 1,278 accepted surfaces. Matrix generation, inverse/forward transforms,
plane solving, and point-in-face code execute without geometry helper hooks.
Only terrain height is supplied by the test. Native tests pass with address and
undefined-behavior checks. Tests produce no game audio.
World-list replay also matches 2,580 rays (1,340 hits). Callback checks match
212 cases across all 16 table rows and 1,032 used slots, including 20 current-pose
PLAYER01 character cases. Scratch and copied stack flags use explicit patterns.

Run the original checker with local assets:
`python tools/check_mission_geometry_original.py Df.exe mission_geometry_probe --models MODELS --chr PLAYER01.CHR --report REPORT.json`.
The native test also passes with `-fno-elide-constructors` to check returned-view
lifetime when copy elision is off.

Source: `486FD0`, matrix helpers `45D170`, `45CE60`, `45CDA0`, face test `42D2A0`,
and loader `486370` (copies model header `+64` from the file).
`Df.exe` SHA-256:
`abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
