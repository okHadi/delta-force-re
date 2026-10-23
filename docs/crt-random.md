# CRT random state

Keep one seed cell for each current CRT thread. Start a new thread's cell at `crt_random_initial_seed` (1), as written by 4C19FC. Keep the cell alive during each call.

Bind `current_seed` to the current thread's cell. The provider runs once on every next or seed call. A null cell makes next return zero and makes seed return without a store. An absent provider raises an error.

Use `crt_random_next` for 49AF6C and `crt_random_seed` for 49AF90. The next call uses wrapping 32-bit multiplication by 41C64E6D and addition of 3039, then returns bits 16 through 30. Keep the CRT seed and the behavior RNG in their separate owners.

Preserve all earlier CRT calls when carrying the seed into a mission. Run `check_crt_random_original.py GAME_FOLDER PROBE --report REPORT` to compare the real next and seed instructions, provider changes and null returns.
