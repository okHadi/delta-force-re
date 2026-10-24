# Profile start

`mission_profile_start_mask` runs source `407C61..407C86`. Bind current memory. It captures the selected profile pointer, reads its campaign and mask, and publishes the mask at `67F70C`.

`mission_profile_start_result` runs source `403AA1..403AC8`. It copies the mask to `67DBEC`, calls required `49A620(67F714,0,514h)`, reads current rules and `503328`, saves the rules at `67DBE4`, and returns the captured branch value.

The caller owns all cells and the result image. Transfer the current `67DBEC` into the live EventStage before events run. Keep saved rules and the loaded mask alive through mission leave and profile save. Restore rules before `401E70`. Each reached memory access and clear call requires a provider.

Validation uses the hash-bound original executable. Normal replay covers 1,096 cases; sanitizer replay covers 1,296 cases. Both compare current memory traces, wrapped campaign addresses, captured profile pointers, callback changes, and failure prefixes. The clear call uses a checked provider with the exact three source arguments.

The native test joins the helpers to the real profile-leave body with a copied profile. It checks success, loss, abort, end-event mask changes, and restored rules. Normal and full-core ASAN/UBSAN builds use disabled copy elision. Source, native test, and probe compile as Windows objects.
