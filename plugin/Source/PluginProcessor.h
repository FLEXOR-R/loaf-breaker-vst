// LOAF BREAKER VST: the processor. The firmware core runs on the audio thread, at 44.1 kHz (resampled to the host's
// rate), its simulated clock a millisecond per 44.1 samples; the host's transport reaches it as a MIDI clock (start
// at the next bar, 24 ticks a beat, stop), the host's notes as MIDI (48..63 the slices, 36 / 38 / 39 / 42 the drums).
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Core.h"
#include "Fm1Link.h"
#include "Fm1Ctl.h"

class LoafBreakerProcessor : public juce::AudioProcessor
{
public:
    LoafBreakerProcessor();
    ~LoafBreakerProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "LOAF BREAKER"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // the editor's side: commands queued for the audio thread, the screen and LEDs read back
    enum Cmd : uint8_t { CmdButton, CmdKey, CmdTurn, CmdTempo, CmdStep, CmdSet, CmdBreak, CmdTrack, CmdKeyKnob };
    void send(Cmd c, int a, int b);
    void step(int op, int a, int b, int c, int d);   // a sequencer edit (lb_step)
    // the page as menus (lb_scan, lb_options, lb_set): every part of the tab shown, each setting's values
    static constexpr int MAX_PARTS = 32;
    struct PartsView { std::vector<lb_part_t> parts; juce::StringArray fx; int fxCur = -1; uint32_t serial = 0; };   // (fx: an effect's page, the effects PRESETS picks)
    bool partsIfNew(uint32_t& serial, PartsView& out);
    void setValue(int part, int knob, int steps);    // part p's knob k, `steps` values on (lb_set)
    int askOptions(int part, int knob, const char* partName, const char* knobName);   // an id, the answer later
    bool takeOptions(int id, lb_opts_t& out);        // the answer (false: not yet, or the page had changed)
    bool screenIfNew(uint32_t& serial, uint16_t* dst);   // the 240 x 240 screen (RGB565), when it changed
    void ledsCopy(uint8_t* dst, size_t n);
    juce::String status() const;
    // the new interface's data, copied on the audio thread (never waiting for it)
    static constexpr int WAVE_COLS = 1408;
    struct WaveView { std::vector<float> cols; std::vector<float> slices; std::vector<float> mm; uint32_t serial = 0; };
    // the sound's picture (FLEXOR: "dans le menu kick un vrai visuel sur le kick, une vraie waveform"): the kick (0) or a
    // drum (1..3) as the FM-1 draws it (lb_pic), n columns over ms, worked out on its own thread when the sound changes
    struct PicView { std::vector<float> cols; int what = -1, ms = 0, n = 0; uint32_t serial = 0; };
    void picWant(int what, int ms, int n) { picWhat = what; picMs = ms; picN = n; }   // (what -1: none wanted)
    bool picIfNew(uint32_t& serial, PicView& out);
    void hear(int d) { send(CmdKey, -1 - d, 0); }   // a sound heard alone: 0 the kick, 1..3 SN HH CP
    bool modelIfNew(uint32_t& serial, lb_model_t& out);
    bool waveIfNew(uint32_t& serial, WaveView& out);
    bool seqIfNew(uint32_t& serial, lb_seq_t& out);  // the pattern in RAM (lb_seq), 30 a second
    bool patternsIfNew(uint32_t& serial, uint8_t* out /* 64 x 40 */);   // the 64 patterns' pictures
    void meters(float& l, float& r);     // the output's peaks since the last call
    void scope(float* out, int n);       // the last n output samples (mono)
    // the Master's waveform (FLEXOR: "une waveform intégrée dans la partie supérieure droite ... la waveform du Master ...
    // l'agrandir ou la faire flotter"): a column every ~2.7 ms of the output (its min, max and three bands' energy),
    // the newest last; n columns at most MW_N
    struct MwCol { float mn = 0, mx = 0, lo = 0, mi = 0, hi = 0; };
    static constexpr int MW_N = 8192;
    int masterCols(MwCol* out, int n) const;
    double masterColsPerSec() const { return hostRate / (double) mwLen; }
    int keyIndex(bool black, int i) const { return black ? blackKey[i % 11] : whiteKey[i % 16]; }
    void idlePump();                     // the editor's timer: the core run silently while no audio runs

    juce::AudioProcessorValueTreeState params;

    // the FM-1 (FLEXOR: "envoyer ce que j'ai fait sur le VST sur la machine"): the state as an FM-1 backup image
    // (the breaks translated, core/fm1sync.c) and back; the USB link
    bool exportFm1(juce::MemoryBlock& image, juce::String& report);
    bool importFm1(const juce::MemoryBlock& image, juce::String& report);
    // the pick (FLEXOR: "exporter ou importer des choses, patterns ou tracks ou kicks"): some kicks, patterns, tracks
    bool listFm1(const juce::MemoryBlock* image, lb_list_t& out);   // nullptr: the VST's own
    bool putFm1(const juce::MemoryBlock& fm1, const std::vector<lb_pick_t>& picks, juce::MemoryBlock& out, juce::String& report);
    bool takeFm1(const juce::MemoryBlock& fm1, const std::vector<lb_pick_t>& picks, juce::String& report);
    // the breaks (FLEXOR: "une playlist de break rangeable dans n'importe quel ordre ... next break et previous break ...
    // une liste déroulante digeste avec des sous-menus"): each its name (the core's order), its category and title
    // (breaks.index), the one playing; the project's playlist (names, in its order: saved with the project)
    struct BreakInfo { juce::String name, cat, title; };
    std::vector<BreakInfo> breaks();
    int breakNow();
    void breakGo(int i);
    void breakStep(int dir);                     // the playlist's next / previous (none: every break, in order)
    juce::StringArray playlist();
    void setPlaylist(const juce::StringArray& names);
    // the song (the arranger): the 8 tracks, read on the audio thread while wanted; one written whole; played
    std::atomic<bool> tracksWanted { false };
    bool tracksIfNew(uint32_t& serial, std::vector<lb_track_t>& out);
    void trackWrite(const lb_track_t& t);
    void trackPlay(int op, int t) { send(CmdTrack, op, t); }
    // the kicks as blobs (the library: FLEXOR, "importer des kicks de la machine, les nommer et les ranger")
    juce::MemoryBlock kickRead(const juce::MemoryBlock* fm1, int k);     // empty: none (fm1 nullptr: the VST's)
    void kickHear(const juce::MemoryBlock& blob);                        // rendered alone, played over the output
    bool putKicksFm1(juce::MemoryBlock& fm1, const std::vector<juce::MemoryBlock>& blobs, const std::vector<uint8_t>& to);
    bool putKicksVst(const std::vector<juce::MemoryBlock>& blobs, const std::vector<uint8_t>& to);
    Fm1Link link;
    // the FM-1 as the controller (Fm1Ctl.h): on / off (kept in the user's settings), its state (0 off, 1 waiting for
    // the FM-1, 2 playing it), the keys (bit k: key k, 0..26) and buttons (bit B_*) it holds now, for the pads
    // a key's own settings (FLEXOR: "gérer les paramètres des touches du clavier en faisant un clic gauche dessus"): the key
    // looked at (black 0 a white key 0..15, 1 a black key 0..10, 11 the REC key; -1 none), its knobs read 30 a second
    void keyKnobsWant(int black, int i) { kkWant = black < 0 ? -1 : black * 32 + i; }
    bool keyKnobsIfNew(uint32_t& serial, lb_keyknobs_t& out);
    void keyKnobTurn(int black, int i, int k, int d);
    void setController(bool on);
    bool controller() const { return ctlWant.load(); }
    int controllerState() const { return ctlState.load(); }
    uint32_t fm1Keys() const { return ctlKeys.load(); }
    uint32_t fm1Buttons() const { return ctlBtns.load(); }
    double bufferMs() const { return blockSize.load() * 1000.0 / juce::jmax(8000.0, hostRate); }   // (the host's buffer, ms)
    int bufferSize() const { return blockSize.load(); }

private:
    bool boot();                         // the core loaded and booted (with the saved image, if any)
    void coreMs();                       // one core millisecond
    void produceCore(int n);             // n more 44.1 kHz frames into the core FIFO
    void sendMidi(uint32_t pkt);
    void hostClock(int numSamples, juce::MidiBuffer& out);   // the host's transport as MIDI clock messages
    void drainCommands();

    Core core;
    juce::CriticalSection lock;
    // the pictures' thread: lb_pic reads the sound's settings only (the core's other functions stay the audio thread's);
    // picLife held while it works, and by whatever unloads the core
    struct PicThread : juce::Thread {
        explicit PicThread(LoafBreakerProcessor& p) : juce::Thread("LOAF pictures"), proc(p) {}
        LoafBreakerProcessor& proc;
        void run() override;
    };
    std::unique_ptr<PicThread> picThread;
    juce::CriticalSection picLife;
    juce::SpinLock picLock;
    PicView picBuf;
    std::atomic<int> picWhat { -1 }, picMs { 400 }, picN { 1000 };
    std::atomic<bool> ready { false };
    juce::String error;
    juce::MemoryBlock pendingFlash;
    std::atomic<bool> reloadWanted { false };

    // the core's clock
    uint64_t coreFrames = 0, coreMsDone = 0;
    // resampling: the core's output FIFO (interleaved) and one interpolator a channel
    std::vector<float> fifo;
    int fifoN = 0;
    juce::LagrangeInterpolator interp[2];
    double hostRate = 44100.0;
    std::vector<float> tmpL, tmpR, chunk;

    // the host's transport
    bool hostWasPlaying = false, clockRunning = false;
    double startAtPpq = -1.0;
    int64_t nextTick = 0;

    // editor <-> audio
    juce::AbstractFifo cmdFifo { 512 };
    struct CmdItem { uint8_t cmd; int8_t a; int16_t b; int8_t c = 0, d = 0, e = 0; };
    CmdItem cmdBuf[512];
    juce::SpinLock screenLock;
    uint16_t screenBuf[240 * 240] {};
    uint32_t screenSerial = 0, coreScreenWrites = 0xFFFFFFFFu;
    uint8_t ledBuf[64] {};
    std::atomic<int> playingNow { 0 };
    std::atomic<int> nBreaks { 0 };
    int msSinceScreen = 0;
    lb_model_t modelBuf {};
    uint32_t modelSerial = 0;
    WaveView waveBuf;
    lb_seq_t seqBuf {};
    uint32_t seqSerial = 0;
    uint8_t patsBuf[64 * 40] {};
    uint32_t patsSerial = 0, patsAge = 0;
    bool patsWanted = true;
    lb_track_t tracksBuf[8] {};
    uint32_t tracksSerial = 0, tracksAge = 0;
    juce::SpinLock trackLock;
    lb_track_t trackOut[8] {};
    uint8_t trackPending = 0;            // (one bit a track: several written in a row, each kept)
    // the menus' service (on the audio thread, never waiting: try-locks)
    lb_part_t partsBuf[MAX_PARTS] {};
    int nParts = 0;
    char fxNames[32][16] {};
    int nFx = 0, fxCur = 0, fxAge = 0;
    uint32_t partsSerial = 0, partsAge = 0;
    bool partsWanted = true;
    juce::SpinLock menuLock;
    struct OptAsk { int id = -1, part = 0, knob = 0; char pname[24] {}, kname[16] {}; };
    struct OptAnswer { int id = -1; bool ok = false; lb_opts_t o {}; };
    OptAsk asks[16];
    int nAsks = 0, nextAsk = 1;
    std::unique_ptr<OptAnswer[]> answers { new OptAnswer[16] };
    void serveMenus();
    // a kick heard (kickHear): its 44.1 kHz frames, played over the output
    juce::SpinLock previewLock;
    std::vector<float> preview;
    double previewAt = 0.0;
    int lastPat = -1;
    uint32_t coreWaveSerial = 0;
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };
    std::vector<float> scopeRing = std::vector<float>(4096, 0.0f);
    std::atomic<int> scopeAt { 0 };
    std::vector<MwCol> mw = std::vector<MwCol>(MW_N);
    std::atomic<int> mwAt { 0 };
    int mwLen = 128, mwN = 0;
    MwCol mwAcc;
    float mwLp1 = 0.0f, mwLp2 = 0.0f;
    void mwFeed(float x);
    int whiteKey[16] {}, blackKey[11] {};
    std::atomic<double> lastAudioMs { 0.0 };
    double idleFrom = 0.0;
    // the controller (the audio thread's, but the atomics)
    Fm1Ctl ctl;
    std::atomic<bool> ctlWant { false };
    std::atomic<int> kkWant { -1 };
    lb_keyknobs_t kkBuf {};
    uint32_t kkSerial = 0;
    std::atomic<int> blockSize { 512 };
    std::atomic<int> ctlState { 0 };
    std::atomic<uint32_t> ctlKeys { 0 }, ctlBtns { 0 };
    double ctlAnswer = 0.0, ctlKeepT = 0.0, ctlLedT = 0.0, ctlFullT = 0.0;
    uint8_t ctlSent[43] {};
    int ctlMlo = 0;
    bool ctlWas = false;
    void ctlServe();                     // the keepalive and the LEDs to the FM-1, its messages from its own port
    void ctlApply(uint32_t msg);         // one from the FM-1: status | d1 << 8 | d2 << 16 (channel 16)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoafBreakerProcessor)
};
