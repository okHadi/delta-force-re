# Helicopter home and height

Use `mission_object_helo_home` for4509F0. Bind current actor/person memory and required477770,477670, and450100 calls. The body captures home deltas, writes the target yaw, selects a speed and height band, and returns the source0/1 result.

Use `mission_object_helo_altitude` for450100. Keep a40h-byte caller frame mapped through all calls: locals00..1B, actor30, target34, band38, mode3C. Keep the argument cells current. The source can change the band argument.

Bind477820 to the real coarse sine table and47DF40 to current terrain. Keep486FD0 required. When current person+2C exceeds62, the source clears it, builds a three-word point, and calls486FD0 before the terrain samples.

The body preserves wrapping signed arithmetic, signed64-bit Q22 products, current position reads, currentD4 checks, and separate force stores. Use current memory for the final force read/modify/write.

The original tests compare full actor/person/local bytes and ordered reads, stores, calls, callback changes, errors, threshold edges, and force branches. The native test joins both bodies to the canonical actor/person state, real angle/length/coarse-sine helpers, and current terrain. It checks the required486FD0 error prefix.
