# Object update messages

Use `mission_object_update` with the captured source callback pointer and the original 32-bit message. It dispatches 429BC0 vehicle AI, 4519A0 searchlight AI, 4534A0 cactus, 45C340/45C3A0/45C400 sound bodies, 453810 small wood, 4533F0 generic objects, and 453930 large wood. An unowned callback raises `MissionTickError`.

The caller reads actor +110 at its source call point. The dispatcher uses that captured value. Keep the current mission state and required `MissionObjectLiveBindings` owners alive through the call.

For 45F4C0, bind its `update` provider to this dispatcher. Forward the current actor reference, captured callback, and message 1. The event action owns the health and A158BC stores before that callback read.

Vehicle and searchlight four-argument overloads accept the message word. Their original bodies ignore that word. Cactus, both wood bodies, generic, and sound bodies apply their source message gates. The three-argument vehicle and searchlight entry points, and registered frame binders, pass message 0.

The native integration test runs the actual event action and selected body. It changes +110 at the health store and after pointer capture, checks message 1 sound gating, and checks the registered frame message 0 sound path. The original comparison runs all nine bodies with messages 0, 1, 4, 80000000, and FFFFFFFF, including synchronous state changes. Source-byte stack analysis checks the vehicle and searchlight argument accesses.

Run `check_mission_object_update_original.py GAME_FOLDER PROBE --report FILE`. Source executable SHA256: `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`.
