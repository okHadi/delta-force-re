# Ground surface publication

Use the four-argument `MissionGeometry::ground_entity` overload to publish each accepted face height as the face is built. The observer runs immediately after the world height is calculated. Apply the current height and depth gates in the observer and publish the current actor handle there.

Keep actor images, model data, matrix data, and query cells stable during each call. Let the observer write only the caller's accepted-height and ground-handle cells. Process target actors in source order. An error in a later face or target preserves earlier observer writes.

Keep each returned `MissionGroundEntity` alive while its `GroundEntity` view is in use. The owned definition remains stable when the result moves. The three-argument overload uses the same calculation with an empty observer.

Run `native_mission_geometry_incremental` for publication order, later geometry and matrix errors, observer errors, skipped materials, and moved-view lifetime. Use `mission_geometry_incremental_probe` with `check_mission_geometry_original.py` for the original executable comparison and current height/depth gates.
