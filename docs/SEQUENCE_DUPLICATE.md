<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Duplicate a track sequence

Select a track with ALGORITHM, hold SEQ, then press **F4 (DUP x2)**.
The current steps copy directly after themselves and LEN doubles, up to 64:
16 → 32, 24 → 48, 32 → 64. Works on melodic and DRUM tracks, including odd
lengths. The result is editable and saveable immediately.

All step bytes copy together: notes/chords, drum hits, ties, accent/slide,
velocity, chance, ratchets and nudge. Automation, parameter locks and special
FX ON/OFF records also copy, preserving fine phase and press order. The
destination is replaced, including dormant events there. Data beyond the new
length and the other tracks stay intact. FX TRACK and automation PLAY switches
stay unchanged; only the configured FX track replays special FX globally.

While playing an ordinary pattern, the current pass finishes at its original
end, then the doubled pattern starts at step 1. Clock, current position, swing
and boundary overshoot are retained. Multiple DUP presses during that
transition still finish the original pass. Stop/start, Undo, a manual LEN edit,
phrase load, project INIT/load or a queued part launch cancel the temporary
end. Queued parts use the actual current loop boundary. The temporary end
takes eight runtime bytes; project and backup formats stay unchanged.

SAVE held undoes the whole action: length, all steps and automation, including
the overwritten destination. Holding SAVE again redoes it through the
existing undo mechanism. OCT− in SEQ TOOLS restores its opening state.

There is no confirmation dialog. Success shows e.g. `T1: 16 -> 32`. Failures
leave the pattern and previous Undo intact:

- `MAX 64 STEPS`: source length above 32; no partial copy.
- `AUTOMATION FULL`: not enough room in the shared 128-record pool, counting
  replaced destination events and reserved FX releases.
- `RELEASE FX FIRST`: the selected track owns an unfinished recorded FX hold.
  Release it (or toggle it off in LATCH mode) before duplicating the take.
- `STOP TO EDIT`: SONG mode keeps the existing editing guard.

`tests/duplicate_test.c` exercises real UI, motion, project and sequencer paths,
all legal lengths on all tracks, melodic/drum data, FX edges, destination
replacement, capacity, Undo/Redo, persistence and live-boundary/part-launch
interaction. It runs in `tests/run_tests.sh`, including its ASan/UBSan suite.
Existing UI layout and sequencer/motion regressions remain in the firmware
suite. Physical FM-1 playback still needs listening checks.
