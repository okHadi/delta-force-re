# Leave raster

The seven `mission_leave_raster` entries use current borrowed memory. `mission_leave_frame` (`404540`) builds raster rows over current `50130C` pixels. Mode `503138` selects 8 or 32 bits. The caller owns the 18h-byte descriptor, destination rows, pixel data, auxiliary data, and display cells. Keep scalar arguments and all memory spans stable through each synchronous call.

`mission_raster_rows` (`4719F0`) preserves current metadata reads and writes. Its cache key contains width, height, stride, pixel base, and auxiliary base. It stores current bits on every call. Signed division faults and missing memory providers throw after the reached source prefix. The source destination `67DBF8` has 1828h bytes for its metadata and up to 768 rows. Supply valid storage for each reached raw access.

`mission_leave_present` (`404610`) calls the current profiler with 5, presents through `496C50/496C70`, then restores the captured profiler index. Mode 2 returns before the profiler read. Each reached display hook requires its current provider. The wrapper reads hook targets again at their source call points. Errors retain the completed source prefix.

`496620` reads the current frame pointer. `496630` returns zero. The Game display owner supplies current mode, dimensions, pixels, and backend state. The existing leave body dims those same pixels in place.

Validation compares all seven original bodies and current memory/call traces. Native coverage joins real `mission_leave` qword dimming to the real FrameServices profiler with declared RDTSC and display providers. These fixtures establish backend boundaries; full display startup remains the caller's task.
