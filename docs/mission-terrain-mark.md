# Terrain impact mark

Use `mission_terrain_mark` for 45DAB0. Keep the mission's shared behavior RNG current.

Supply reads from the current BD52AC color grid, BD5334 type map, selected item-index cell and C23810 item table. Type values 1 through 6 select one item index after the grid reads. Keep that selected index through the sound and alert calls. Read the current item table base after those calls.

Bind 43E7D0 to positional sound, 45FCF0 to the shared behavior-zone owner and 45D970 to actor insertion. Calls run synchronously. Missing reached bodies raise an error.

45D970 receives the complete mutable 276-byte image (0x114), with insertion argument 1. Its scalar argument 0 represents the image pointer and contains zero in the portable call. Keep the image valid until the callback returns. Use the current actor pool through its insertion owner.
