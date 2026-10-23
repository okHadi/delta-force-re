# Audio listener

`mission_audio_listener` implements43EBA0 with eight ordered dword stores. Bind6E06A8/AC/B0 to the position reference used by `MissionAudioFrameCells`. Bind6E06B4 to its yaw reference. Keep mirrored position cells6E4708/4704/4700 and pitch6E06B8 in current shared storage.

The helper preserves all input bits and stops on a failed store. It opens no audio device.

Source: Df.exe SHA256 `abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff`. REA evidence `ev_c186cad1381707999ba4a09582f7b0b4cb65de950556ab771e96ff88cd5c6afa`, retained at `.local/reports/mission-audio-listener-source.json`. The original checker compares all eight cells and their complete store order, widths and values.
