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
  a floating window (moved by its bar, sized by its corner, kept: settings machineBox), MENU shows / hides it. Or a window of
  its own (25 Nov 2026, FLEXOR: "dans une fenêtre séparée"): MENU › Aperçu du FM-1, or the box's window icon;
  PluginEditor.cpp MachineWindow (a desktop window, native bar, square, its place kept: machineWin; settings machineSep;
  its keys go to the editor's). The mouse wheel never changes a menu's value (FLEXOR: "pas de modification des
  sliders avec la molette"); it scrolls the cards.
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
  The manager (MENU › GESTION; FLEXOR: "exporter ou importer des choses, patterns ou tracks ou kicks"; "écouter sur le
  PC les kicks de la machine ... les nommer et les ranger ... une gestion totale"): Exchange.cpp, three columns, the
  VST's 20 kicks / 64 patterns / 8 tracks, the library (each kick a .lbkick file, Documents/LOAF BREAKER/Kicks and its
  folders: renamed, moved, deleted), the FM-1's (its backup over USB, or a .bak); > hears a kick, ticks pick, sent or
  taken item by item, the rest of the other side byte for byte as it was. core/fm1sync.c: lb_fm1_list, lb_fm1_put /
  lb_fm1_take (picks: kind, from, to; on the objects' buffers, the firmware's structs: a kick memory across settings,
  steps, km2, klab, kfxm, kx2.m, the one playing in the settings' live fields; a pattern in its group's three objects,
  the group in RAM in OBJ_PAT / OBJ_STEP too, a group never saved made blank, its bars 3-4 in OBJ_DX; a track in
  pats.trk, steps.tb (its breaks translated), steps.tk), lb_kick_read / lb_fm1_put_kicks / lb_vst_put_kicks (blobs:
  "LBK1", size, the kick whole), lbcore.c lb_kick_render (the blob as the kick playing, stopped, one hit through the
  firmware's engine, the state put back: lbsnap). The image always every object in order (the FM-1 checks them all).
  Checked: a put restored in the firmware's host simulator, its backup again holds the picks (scratch test).
  Breaks (FLEXOR: "une playlist de break rangeable dans n'importe quel ordre et assignable à des touches next break et
  previous break en dessous du nom du break ... cliquer sur le nom du break ouvre une liste déroulante digeste avec
  des sous-menus ... les catégories de la page break du site"): core/build_bank.sh also writes breaks.index (NAME,
  CATEGORY as break_lib.py FOLDER, TITLE) beside the bank (Resources, the Standalone's folder); lbcore lb_break_name /
  lb_break_go (break_ask); the header's name a menu by category (MES BREAKS: the player's own), < PLAYLIST > under it;
  the playlist the project's (params.state "playlist", names); PlaylistEditor (drag to order). OFF zones: a long
  slider (> 12 values) with an OFF has it wider (14 % of its travel), shaded, filled from OFF; double click: OFF.
  The keyboard (FLEXOR: "se déplacer ... dans le séquenceur et appuyer sur entrée pour mettre une step ... tout
  mapper ... penser la VST comme un mini DAW"): KeyMap stores juce::KeyPress (modifiers) by context (0 everywhere,
  1 the grid, 2 the menus; settings "keys2"); Nav (F1-F8 domains, TAB tabs, CTRL+S/Z/C/V/B/P/G/K, F9), Seq (the grid's
  cursor, ENTER, DEL, ALT/SHIFT/CTRL+arrows its values, [ ] rotate: SeqPanel::command), Menu (MenuContent::command:
  a focus frame, LEFT/RIGHT the value).
  The arranger (FLEXOR: "penser la VST comme un mini DAW"): SONG › TRACKS shows SongPanel above the menus: the 8 tracks
  on a timeline, each item a block as long as it plays (loops x the pattern's bars; a drop its build), drag to move,
  wheel its loops, double click its pattern, + to add (a pattern, DROP, GAP), > plays the track, its END (LOOP STOP
  HOLD), the item's inspector (pattern, loops, break, kick, << >>, duplicate, delete). lbcore lb_track / lb_track_write
  (the firmware's edx_track / edx_track_write: each item checked) / lb_track_play; the processor reads the tracks on the
  audio thread while shown (tracksWanted) and writes one through a mailbox (trackLock).
  Full screen (FLEXOR: "un mode fullscreen et borderless fullscreen"): MENU › Affichage, F11 / SHIFT+F11, ESC back
  (LBE::screenMode): the Standalone's kiosk mode, or its window without its bar at the monitor's size; in a DAW the
  plugin grows to the screen (the host's window is the host's). The drawing scaled to fit, centred. KeyMap saved by
  action name (settings "keys3": new actions never shift the others) and no longer saved over at start-up.
  Next: the pages' own pictures (the kick's waveform, the drums', a finer break), a mixer.
- The look A "ANATOMIE" (25 Nov 2026, FLEXOR, on the mockups https://claude.ai/artifact/PzQ9Aav3nXg8ZX78D3zjUh: "c'est la
  bonne direction, refonte tout le vst avec cet esprit"; before it, "un vrai visuel sur le kick, une vraie waveform ...
  une waveform plus détaillée ... chaque identité de menu du firmware adaptée pour un PC"): under the tabs the sound drawn
  big (Root: SoundHero for SOUNDS › KICK / DRUMS, WavePanel the break elsewhere; none on PATTERN's grid and SONG's
  arranger), the page's parts as cards under it (MenuContent), the pads folded to a strip (Pads compact, PADS ▴ opens
  them, settings padsOpen). The kick: lbcore `lb_pic` (the firmware's own kick_rgb / kick_wave of set.kick, its picture;
  perc_rgb for SN HH CP), on the processor's picture thread (PicThread: it reads the sound's settings only; picLife held
  while it works and by whatever unloads the core), redrawn when `lb_pic_sig` moves; the view zoomed on the tab (ATTACK
  60 ms, BODY / DIRT 400, TAIL 1.5 s with its END, else 700; the chips 60 MS 400 MS 1.5 S AUTO); its card: KICK n and
  its built-in (lb_model kslot, kpre), the 20 memories (PRESETS turned: the kick), ECOUTER (lb_drum_hit 0), COPIER /
  COLLER (SEL + ARP), SAVE, the layers' levels (klev). The break: every pixel its min / max (lb_wave_mm) coloured as the
  FM-1's RGB view (ui::rgb: the lows red, the mids green, the highs blue). The cards (ui::card): a chip in the layer's
  colour (ui::layer: SUB red, PUNCH amber, GRIT green, CLICK blue, TOK violet, everywhere), a few words (ui::subtitle),
  OFF hatched (ui::hatch), values both ways filled from their 0, values with a picture as tiles (iconAt: TAIL's MODE
  and CURVE, D+ TYPE's curves, TOK's TYPE, the layers' effects, a drum's DIRT), a part of one list (PRESET) listed
  whole in a column, MIX / LEVELS / MACRO as faders, MORE's KNOBS as round knobs; an effect's page (a kick's FX1 / FX2,
  EFFECTS' slots) the effect first (lb_fx_list: the effects PRESETS goes through; picked by PRESETS turned); the FM-1's
  spare K3 PRESET hidden on the layers' FX. A firmware fix found on the way (0.9.78 TEST, page_say's 24 bytes).
  Then (FLEXOR: "pourquoi pas encore fait"): an EQ's bands (LOW MID HIGH in dB: the kick's EQ BANDS, an EQ of the
  chain) a curve with its three points dragged (gain up / down, frequency sideways: the nearest values of LOW / MID /
  HIGH and LO FREQ / MID FREQ / HI FREQ, wherever their cards are; MID Q and LEVEL read), its values under it; DIRT's
  picture D+ TYPE's curve big, bent by DRIVE; PATTERN's lanes as tall as the room (SeqPanel rowH scaled), the break's
  steps in the RGB colours; SONG's arranger in cards, each pattern block its group's colour (P1-16 red .. 49-64 blue)
  and its kick, snare and hat drawn small (lb_patterns, fetched while SONG shows).
- Any screen (25 Nov 2026, FLEXOR: "le plein écran n'est pas adapté à un écran 16:9 ... adapté à tous les écrans"): the
  editor no longer keeps 16:10 (no fixed aspect, limits 960 x 600 to 7680 x 4320); LBE::resized scales by the smaller
  ratio and gives Root the window's own shape, 1440 x 900 or more (Root::resized, Header, Tabs, Pads, SoundHero lay out
  from getWidth / getHeight: wider, the pictures and cards; taller, the menus, the sequencer, the arranger); a DAW's
  full screen takes the screen's area. Faders that stuck while dragged (FLEXOR: "des faders restent bloqués après
  avoir un peu bougé"): the cards laid out again under the hand lost it (dragP nulled); MenuContent keeps `dragKey` and
  finds it again after a rebuild.
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
