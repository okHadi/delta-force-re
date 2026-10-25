# Character identity

Keep one current cache, controller owner, source pool and arena alive. Bind current
memory reads, resource/controller lookups and exact live arena allocation stamps.
Serialize owner changes with identity calls.

Each allocation stamp contains current source generation, size and a distinct
nonzero64-bit arena-owner incarnation. Assign a new incarnation at every actual
allocation. Keep it distinct through source generation wrap and payload reuse.
The identity module stores metadata only.

Call `publish_resource(token,row)` after42CBB0 completes. Use its actual6BA440
registry row and the matching current cache token. An exact current repeat keeps
one binding. Call `publish_instance(token,row,resource_token)` after the full
42CCE0 constructor, pose seed and action setup, before publishing actor+18.
Use the actual42FC60 pool row and the current typed controller token.

Publication checks current source pointers, counts, scalars, bytes and pose/root
data against a fresh borrowed controller view. Keep all initial state equal.
Make an initial source-to-controller state transfer explicitly before publication
when current shared motion or retained pool fields require it. Keep one animation
clock. Unregistered source rows remain with their raw pool owner.

The four lookup methods convert explicit current identities in either direction.
Null0 returns0. Unknown nonnull identities report an error. Current owner and
allocation stamps are checked on each lookup. Physical addresses retain source
slot reuse behavior.

Call `unpublish_instance` after real source instance release. Remove its matching
typed controller at the caller's required boundary. Call `unpublish_resource`
after its source release/registry clear and after removing its instance bindings.
A row that already has a new allocation can keep its new data while the old
metadata is removed. Retired tokens stay retired for this registry lifetime.
Create a fresh registry for a new owner lifetime.

Raw memory encoding stays with the caller. Internal renderer, matrix and muzzle
users keep their typed handles. Raw actor18 and definition74 need the same explicit
identity mapping when their memory codec is installed.

Run `native_mission_character_identity GAME_FOLDER` for the actual cache/file,
arena, constructor, seed/action, release and slot reuse test. It also checks
incomplete publication, explicit initial state transfer, repeated source stamps,
retired tokens and current metadata removal. Tests open no audio device.
