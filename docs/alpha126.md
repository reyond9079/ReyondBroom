# Alpha 126 — TrenchBroom profile import

TrenchBroom compilation profiles using `${CPU_COUNT - 1}` previously skipped the
entire profile: the importer accepted variable identifiers but rejected this
expression. The supplied five-profile configuration reproduced five skips and
zero imported profiles.

The importer now accepts the spare-core expression, including spaces/tabs. The
runner evaluates it using the current machine's CPU count each time the profile
runs, with a minimum of one compiler thread. Saving and reopening a profile keeps
the expression; no machine-specific thread count is baked into the import.
Other TrenchBroom expressions still require manual setup. No shell evaluation was
added, and importing does not execute compiler/copy/delete tasks.

The supplied file also has `.bspw` in the first profile's VIS arguments. That is a
separate filename typo: import preserves it rather than silently changing user
commands. Use `.bsp` there before running that profile. External `D:/...` compiler
and game paths still need to exist on the user's machine.

Alpha 126 also includes the approved multitrack Sound Editor already merged in
main (REYON-4), and the alpha 125 theme/setup improvements on Windows and SDL2.
Windows packages therefore include the rebuilt float/DSP audio helper and its
complete corresponding codec sources/licenses. Engine, CSQC, plugins and game
compiler binaries are retained from the verified base releases.
