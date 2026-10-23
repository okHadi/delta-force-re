# Mine update

Bind `451D40` to current actor memory and an eight-byte caller frame. Route
`477820` to the shared sine table and `45DA30` to actor removal. Keep the
record writable after removal for the source delay and flag stores.

Velocity uses the low 32 bits after the signed product shifts right by 22,
then shifts that signed value right by six. Reads after callbacks use current
actor fields and frame bytes. Each reached provider is required.
