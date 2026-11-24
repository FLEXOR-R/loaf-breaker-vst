// LOAF BREAKER VST: the processor. The firmware core runs on the audio thread, at 44.1 kHz (resampled to the host's
// rate), its simulated clock a millisecond per 44.1 samples; the host's transport reaches it as a MIDI clock (start
// at the next bar, 24 ticks a beat, stop), the host's notes as MIDI (48..63 the slices, 36 / 38 / 39 / 42 the drums).
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Core.h"
#include "Fm1Link.h"

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
    enum Cmd : uint8_t { CmdButton, CmdKey, CmdTurn, CmdTempo, CmdStep, CmdSet, CmdBreak };
    void send(Cmd c, int a, int b);
    void step(int op, int a, int b, int c, int d);   // a sequencer edit (lb_step)
    // the page as menus (lb_scan, lb_options, lb_set): every part of the tab shown, each setting's values
    static constexpr int MAX_PARTS = 32;
    struct PartsView { std::vector<lb_part_t> parts; uint32_t serial = 0; };
    bool partsIfNew(uint32_t& serial, PartsView& out);
    void setValue(int part, int knob, int steps);    // part p's knob k, `steps` values on (lb_set)
    int askOptions(int part, int knob, const char* partName, const char* knobName);   // an id, the answer later
    bool takeOptions(int id, lb_opts_t& out);        // the answer (false: not yet, or the page had changed)
    bool screenIfNew(uint32_t& serial, uint16_t* dst);   // the 240 x 240 screen (RGB565), when it changed
    void ledsCopy(uint8_t* dst, size_t n);
    juce::String status() const;
    // the new interface's data, copied on the audio thread (never waiting for it)
    static constexpr int WAVE_COLS = 1024;
    struct WaveView { std::vector<float> cols; std::vector<float> slices; uint32_t serial = 0; };
    bool modelIfNew(uint32_t& serial, lb_model_t& out);
    bool waveIfNew(uint32_t& serial, WaveView& out);
    bool seqIfNew(uint32_t& serial, lb_seq_t& out);  // the pattern in RAM (lb_seq), 30 a second
    bool patternsIfNew(uint32_t& serial, uint8_t* out /* 64 x 40 */);   // the 64 patterns' pictures
    void meters(float& l, float& r);     // the output's peaks since the last call
    void scope(float* out, int n);       // the last n output samples (mono)
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
    // the kicks as blobs (the library: FLEXOR, "importer des kicks de la machine, les nommer et les ranger")
    juce::MemoryBlock kickRead(const juce::MemoryBlock* fm1, int k);     // empty: none (fm1 nullptr: the VST's)
    void kickHear(const juce::MemoryBlock& blob);                        // rendered alone, played over the output
    bool putKicksFm1(juce::MemoryBlock& fm1, const std::vector<juce::MemoryBlock>& blobs, const std::vector<uint8_t>& to);
    bool putKicksVst(const std::vector<juce::MemoryBlock>& blobs, const std::vector<uint8_t>& to);
    Fm1Link link;

private:
    bool boot();                         // the core loaded and booted (with the saved image, if any)
    void coreMs();                       // one core millisecond
    void produceCore(int n);             // n more 44.1 kHz frames into the core FIFO
    void sendMidi(uint32_t pkt);
    void hostClock(int numSamples, juce::MidiBuffer& out);   // the host's transport as MIDI clock messages
    void drainCommands();

    Core core;
    juce::CriticalSection lock;
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
    // the menus' service (on the audio thread, never waiting: try-locks)
    lb_part_t partsBuf[MAX_PARTS] {};
    int nParts = 0;
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
    int whiteKey[16] {}, blackKey[11] {};
    std::atomic<double> lastAudioMs { 0.0 };
    double idleFrom = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoafBreakerProcessor)
};
