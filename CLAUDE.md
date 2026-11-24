# LOAF BREAKER VST: notes for Claude sessions

FLEXOR's private plugin (24 Nov 2026: "transformer LOAF BREAKER en VST ... propre, pas dépendre d'un écran de 240x240,
un séquenceur plus grand, plus de possibilités"; private, shared with a friend, never public). Reply in French.

- `firmware/`: FLEXOR-R/loaf-breaker (public, branch dev) as a submodule, with its `base` (fm1-base); never edit it
  here: firmware changes go to that repository (its own CLAUDE.md rules), then this submodule is moved on.
- `core/`: the firmware as a library, `lbcore.c` (fm1-base's host/rfm1.c, the host simulator embeddable, compiled
  natively + the C API of `lbcore.h`); `build_core.sh OUT [CC]` (liblbcore.so; lbcore.dll with x86_64-w64-mingw32-gcc:
  depends on KERNEL32 and msvcrt only). Needs the firmware's generated headers: `(cd firmware && ./build.sh --host)`.
  The first start's mode question is answered EXPERT (lb_ms); the start screen run silently by lb_init(5000).
- LB_BIG (24 Nov 2026, FLEXOR: "casser tout ce qui limite techniquement la version fm1 ... tout ce qui est une limite
  sur fm1 et pas sur pc doit être optimisé"): the core is built with -DLB_BIG=1, the firmware's switch for the PC
  (app.c, app.h; the FM-1 never has it, its image unchanged): the break bank a file (`core/build_bank.sh`:
  breaks.bank, every break of breaks.txt at 44.1 kHz 16-bit PCM, up to 128 of 23 s; lb_bank before lb_init; the
  plugin's Core::bankFile, VST3 Contents/Resources), WK_NB 128 (each break's white keys and split; kx2 over four store
  objects), the dub line 2 s, the output 24-bit (lb_audio from sim_render). Next limits to lift (the plan): patterns
  all in RAM (no SAVE OR STOP), more kicks, a bigger drum kit, longer breaks and more bars, the player's breaks by
  drag and drop, then the PC interface.
- `plugin/`: JUCE 9.0.6 (FetchContent, or -DJUCE_DIR=), VST3 + Standalone (AU on a Mac). `Core.cpp` loads a COPY of
  the core library per instance (a temporary file: the firmware's state is global, so each instance has its own;
  setStateInformation reboots a fresh copy). The audio thread runs the core: 44.1 kHz, a core millisecond each 44.1
  frames (lb_ms; its frame every 15 ms), resampled to the host's rate (LagrangeInterpolator). The host's transport
  becomes the firmware's MIDI clock in (start at the next 4/4 bar, 24 ticks a beat, stop: hostClock), host MIDI goes in
  as USB-MIDI packets (48..63 slices, 36/38/39/42 drums). State: the firmware's flash image after lb_save (SAVE),
  gzipped, plus the parameters (volume, sync).
- The window (step 1): the FM-1's screen x2 and its whole panel (14 buttons, 7 encoders, 27 keys; the computer's keys,
  space PLAY). Step 2 (planned with FLEXOR): a PC interface (big sequencer, kick, breaks by drag and drop, song),
  separate outputs, automation, MIDI drag to the DAW, the FM-1 as a controller.
- Tests: `plugin/Test/HostTest.cpp` (lbtest PATH.vst3 OUT.wav: loads it as a DAW, 48 kHz, host playing 170 BPM: sound,
  state reload, silence after stop) and pluginval strictness 5, on Linux and Windows in CI.
- **The FM-1 first, always** (FLEXOR, 24 Nov 2026: "synchroniser la VST et loaf breaker pour la fm1 ... toujours
  d'abord s'occuper de la fm1, me demander si il faut publier, et ensuite faire les modifs et possiblement la
  publication de la vst"): a change is made in the firmware (FLEXOR-R/loaf-breaker, dev, its TEST) first; then ask
  FLEXOR whether to publish it (his "publie": the firmware's release, as its CLAUDE.md says); then the VST: the
  `firmware` submodule moved to that commit (the released one: the loaf-vA.B.C tag's), the VST's own changes, pushed
  to main (a TEST), and published only if FLEXOR wants (`git push origin main:release`). The VST's version is the
  firmware's it carries (vst-vA.B.C = loaf-vA.B.C).
- CI `.github/workflows/build.yml`: main -> TEST (pre-release `vst-test`, replaced each push,
  LOAF-BREAKER-VST-A.B.C-testN-Windows.zip); release -> `vst-vA.B.C`. Core on Linux, plugin on Windows; INSTALL.txt as
  LISEZ-MOI.txt. Private repo: Actions minutes are limited (Windows counts double): don't push trivial changes one by
  one.
