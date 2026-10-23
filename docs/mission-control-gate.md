# Local control gate

`mission_control_gate` implements4718D0. Bind current dword reads at67F520,67F528 andBC5D20. Read each cell only when reached.

Return0 for single-player or host mode. For a remote client, shift BC5D20 left4 with32-bit wrap, interpret the result as signed, and divide by1000 with truncation toward0. Local control488200 clears control bits1–4 when this result is greater than2.

Source: Df.exe SHA256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. REA evidence `ev_610698f2eb763e22906a68a28d47414237cde4db69034a9a9910db30983d80c2`, retained at `.local/reports/mission-control-gate-source.json`. The original checker compares actual instructions, signed return values, read order, read widths and callback changes. This helper has no external calls or audio output.
