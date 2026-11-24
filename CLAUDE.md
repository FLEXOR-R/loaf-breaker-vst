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
- FM-1 link (24 Nov 2026, FLEXOR: "envoyer ce que j'ai fait sur le VST sur la machine ... un système de traduction et
  de synchronisation efficace qui lie les deux"): the firmware's backup and restore over USB (its fm1.toml backup =
  true, app.h APP_BACKUP_*: magic LBRK). `core/fm1sync.c` (in lbcore.c): lb_fm1_export (the VST's saved objects as an
  FM-1 backup image) and lb_fm1_import (one into the VST's store, then a fresh core): the breaks translated by name
  (one the FM-1 lacks: its break of the nearest tempo, reported; the FM-1's own breaks: the VST's first) in set.brk,
  set.drop_brk, steps.dm[d][2], steps.tb (pattern items), kx2.sp's B; kx2 (24 breaks on the FM-1, 128 here) rebuilt.
  `plugin/Source/Fm1Link.*`: fm1-base's backup protocol (tools/fm1_backup.py's) on the FM-1's MIDI port, its own
  thread; the window's FM-1 button: send, fetch, export / import a .bak. Tested: the VST's export restored in the
  host simulator (scenario restore), its backup imported back (the breaks and CHAOS as they were).
- `plugin/`: JUCE 9.0.6 (FetchContent, or -DJUCE_DIR=), VST3 + Standalone (AU on a Mac). `Core.cpp` loads a COPY of
  the core library per instance (a temporary file: the firmware's state is global, so each instance has its own;
  setStateInformation reboots a fresh copy). The audio thread runs the core: 44.1 kHz, a core millisecond each 44.1
  frames (lb_ms; its frame every 15 ms), resampled to the host's rate (LagrangeInterpolator). The host's transport
  becomes the firmware's MIDI clock in (start at the next 4/4 bar, 24 ticks a beat, stop: hostClock), host MIDI goes in
  as USB-MIDI packets (48..63 slices, 36/38/39/42 drums). State: the firmware's flash image after lb_save (SAVE),
  gzipped, plus the parameters (volume, sync).
- The window (24 Nov 2026, FLEXOR: "une refonte complète ... changer complètement de style et de façon d'être
  contrôlée ... des contrôles contextuels qui affichent ce qu'ils modifient ... des catégories qui contiennent des
  onglets ... la waveform en permanence, des visuels"; the FM-1 link set aside, "laisse tomber la liaison"): drawn at
  1440 x 900, scaled to the window (PluginEditor.cpp Root). LOAF LORD's look (Ui.cpp: skins LOAF LORD black and bone,
  the default, BLOOD TOXIC ICE RUST PAPER; Barlow Condensed, OFL, in Resources/ (FLEXOR: "la police est illisible, ne pas mettre de
  police pixélisée": never a pixel font); the grain and halftone tile, the
  vignette; his face and gothic title, Resources/loaflord_*.png from FLEXOR's poster). Header (tempo, break, pattern,
  the FM-1's message, meters, MENU: skin, keyboard, the FM-1's screen on / off, the machine link), the categories
  (LIVE SOUNDS PATTERN SONG MIX EFFECTS ARP SYSTEM: the FM-1's page buttons), the page's tabs (a click: SELECT turned
  by the difference; SOUNDS: KICK DRUMS BREAK, SEL + EDIT until there) and its part (< >: ALGORITHM), the break's
  waveform always (lb_wave: 1024 columns, peak and three bands; slices, playhead, 32 steps; a click plays a slice), the
  four knobs named as the page's (lb_model: knob_name / knob_value / foot_norm; SEL's when SHIFT is held), the FM-1's
  screen (optional), the output's scope, the 12 performance pads named as the layout's functions and the 16 slice
  pads, each with its key. The model: lbcore.c `lb_model`, the tabs and part noted as the firmware draws them
  (app/ui/main.c `lb_cap_tab` / `lb_cap_part`, LB_BIG only). KeyMap.cpp: every action a key, the settings' CLAVIER. The tempo (FLEXOR: "certaines commandes ... montent le BPM"): the header's
  number dragged or the wheel, TEMPO - / + (N, M), lb_tempo; SELECT never the tempo here (the firmware's LB_BIG: HOME's
  tabs, nothing elsewhere); SELECT, ALGORITHM and PRESET handed over a detent at a time (lbcore `lb_nav`: the pages
  move a tab a reading).
  Menus (24 Nov 2026, FLEXOR: "des vrais menus, pas juste quelque chose pour contrôler le petit aperçu ... l'aperçu dans
  un coin, régler sa taille"; "des crans plus ou moins rapprochés selon s'il y a 4 options ou 99 ... pas de potard qui
  tourne dans le vide"): the four knobs gone; every part of the tab shown at once, a card each (MenuContent), each
  setting drawn after the values it can take: 2 a switch, a few short ones buttons, names a list, amounts a slider with
  its ends (its steps' width after their count; double / right click: every value in a list). The core looks ahead
  through the firmware's own code: lbcore `lb_scan` (every part: ALGORITHM's round), `lb_options` (every value: the
  knob turned until it stops or comes round), `lb_set` (part p's knob k, n values on, the part shown back), the whole
  state kept and put back around them (core/lbsnap.c: the library's writable memory, ELF's RW segment past RELRO, PE's
  writable sections; ~1-5 ms a setting, one a frame on the audio thread: the processor's serveMenus). The FM-1's screen
  a floating window (moved by its bar, sized by its corner, kept: settings machineBox), MENU shows / hides it.
  The sequencer (FLEXOR: "il me faut un vrai séquenceur"): PATTERN's FULL / DRUMS tabs show SeqPanel: the 64 patterns,
  the break's 32 steps (each its slice's own waveform), KICK SN HH CP 16/32/64, click / drag to paint, the wheel (a
  break step's slice, a drum's HITS), the step's settings, the bar's and the lane's tools (lbcore `lb_seq`, `lb_step`:
  SEQ's own code and undo; `lb_patterns` the bank's pictures). core/coretest.c checks it all in CI (Linux, Windows).
  Skins (FLEXOR: "refais les équivalents des skins présents dans la machine ... même idée sur la façon que le skin a
  d'interagir avec les steps du séquenceur"): the VST's six, the FM-1's 24 (Ui.cpp FM1_RGB, the firmware's SKIN_RGB:
  its block the panels, its secondary the accent, its values the vivid one, its white), COMME LE FM-1 (lb_model.skin,
  SETUP's), PERSO (nine colours picked, settings skinPerso); the sequencer as the FM-1's SEQ: steps on in the accent,
  the lane edited vivid, the playhead white, HITS split, CHANCE the lower half dark, NUDGE a white mark, a note a white
  line. The header: LOAF BREAKER, BY FLEXOR (FLEXOR: "et c'est tout").
  Next: a song arranger (the tracks' items), the kick's picture, a mixer.
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
