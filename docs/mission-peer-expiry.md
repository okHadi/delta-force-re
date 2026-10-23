# Peer expiry

`mission_peer_expiry` ports `4470C0`. Bind the current host gate `67F528`, x87 control, shared `442630` outbox, and `460950` effect handler. Keep the input position record writable and stable through both calls.

The request has 18 bytes. It stores the source actor row, X, Y, converted Z, upper angle word, mode 2, and life 200. Every packet byte is set. Copy the packet during the outbox call. The outbox runs before the effect handler. Changes to the input record made by the outbox reach the effect handler.

`mission_peer_expiry_effect` ports `460950`. Its required call handler dispatches `477850`, `477860`, `45DC70`, `4657D0`, `43E7D0`, and `45FCF0`. Its required spawn handler dispatches `463200` with the mutable six-word local record. Raise an error for any reached call without a handler.

Provide the current input record. Three words are sufficient when the signed count is zero or less. Positive counts read the fourth word after the second random call. X, Y, and Z enter the local record after the initial effects. Spawn changes to that local record remain for the next iteration. Supply its last word from the caller's stack state through `caller_scratch_tail`. Use the local record only while the effect call is active.

The request height conversion uses source double `4CC6AC`, current x87 precision and rounding, `49BAE6` truncation, and a signed 32-bit store. The call handler owns real random state and sound dispatch. Keep device output muted at the audio sink.

Run the original comparison:

```sh
.local/tools/python/bin/python tools/check_mission_peer_expiry_original.py .local/reference-game/Df.exe build/mission_peer_expiry_probe --report peer-expiry-report.json
```
