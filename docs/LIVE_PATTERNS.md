<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Live parts

## Create four parts without stopping

While playing, open SAVE > PROJECT. Select A-D with KNOB 1, pick SAVE with
KNOB 4 and press OCT+. An occupied slot asks before overwriting. Name the part
or keep its name, then press OCT+ again. Playback and recording keep running.
The display briefly says RAM ONLY, followed by STOP TO SAVE. The saved slot
shows RAM: before its name until it is committed to flash.

Each slot holds its own snapshot, including steps, timing, sounds, automation
and parameter locks. A-D are immediately available to the live LOAD below;
live LOAD continues to retain the current sounds. Saving B, C or D does not
replace A. Saving a slot again replaces only that slot's snapshot.

STOP automatically commits the pending snapshots, one per main-loop pass;
PARTS SAVED means all are durable. Restarting playback pauses any remaining
commits. A flash error keeps the RAM snapshot and previous durable copy, shows
RAM: SAVE ERROR and retries after one second while stopped. Uncommitted parts
are lost on power-off or reboot. User presets and renames still require STOP;
active SONG playback/armed SONG must be stopped before saving project snapshots.

The four existing 3840-byte project caches are reused (15 KiB total); no extra
project buffer is allocated. Added storage metadata is a two-byte retained
mask plus a retry flag/timestamp. Hardware timing still needs testing.

## Switch parts live

While playing, open SAVE > PROJECT. Select A-D with KNOB 1, select LOAD
with KNOB 3, then press OCT+. QUEUED A-D replaces any earlier valid request.
At track 1's loop boundary all four editable tracks restart together,
including saved step data, timing, automation and parameter locks.
The current sounds, global tempo, effects and clock source stay in place;
automation still changes the parameters it controls. An active recording ends
at launch. Stop cancels a pending launch. Stopped LOAD still loads the full project.
All four destination engines must match the current engines. SONG cannot run
alongside this launcher because both reuse the existing pattern cache.

## Validation

`tests/live_pattern_test.c` and the persistence tests cover
queued launches, RAM snapshots and commits after STOP.
The emulator accepts Web MIDI channel messages, but not MIDI clock/Start/Stop;
external-clock timing and listening still need checking on a real FM-1.
