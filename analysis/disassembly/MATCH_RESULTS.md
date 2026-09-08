# Match-result state machine

The result object at `0x0041459C` is initialized by `0x00414560` and advances
through nine states. The native port mirrors the recovered ordering and uses
the embedded source VOC duration for each original sound-playing wait.

| State | Recovered behavior |
|---:|---|
| 1 | Select `[fighter] WINS`, set state 2, and start the winner's fighter cue. A full-health non-draw also enables `FLAWLESS VICTORY` text. |
| 2 | Wait for the fighter cue to stop. |
| 3 | Wait until the old counter exceeds 20, enter state 4, and play VOC 250 (`WINS`). |
| 4 | Wait for VOC 250; branch to state 5 for a full-health winner or state 7 otherwise. |
| 5 | Wait until the old counter exceeds 20, enter state 6, and play VOC 1007 (`FLAWLESS VICTORY`). |
| 6 | Wait for VOC 1007, then enter state 7. |
| 7 | Wait until the old counter exceeds 20, enter state 8, and, when fighter field `+0x770` is set, show the fatality graphic and play VOC 1008 (`FATALITY`). |
| 8 | Wait for the fatality cue, then enter state 9. |
| 9 | Wait until the old counter exceeds 50, then deactivate the result object so normal tournament/two-player routing resumes. |

The FINISH object at `0x00414484` has its own 28-stage cadence. Its random
component-1/component-2 fallback is guarded by the winner's CPU flag and a
health comparison; it is not applied to a human winner. A recognized human
component recipe can still set the fatality result flag during the prompt.
