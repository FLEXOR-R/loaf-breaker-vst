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
    enum Cmd : uint8_t { CmdButton, CmdKey, CmdTurn, CmdTempo };
    void send(Cmd c, int a, int b);
    bool screenIfNew(uint32_t& serial, uint16_t* dst);   // the 240 x 240 screen (RGB565), when it changed
    void ledsCopy(uint8_t* dst, size_t n);
    juce::String status() const;
    // the new interface's data, copied on the audio thread (never waiting for it)
    static constexpr int WAVE_COLS = 1024;
    struct WaveView { std::vector<float> cols; std::vector<float> slices; uint32_t serial = 0; };
    bool modelIfNew(uint32_t& serial, lb_model_t& out);
    bool waveIfNew(uint32_t& serial, WaveView& out);
    void meters(float& l, float& r);     // the output's peaks since the last call
    void scope(float* out, int n);       // the last n output samples (mono)
    int keyIndex(bool black, int i) const { return black ? blackKey[i % 11] : whiteKey[i % 16]; }
    void idlePump();                     // the editor's timer: the core run silently while no audio runs

    juce::AudioProcessorValueTreeState params;

    // the FM-1 (FLEXOR: "envoyer ce que j'ai fait sur le VST sur la machine"): the state as an FM-1 backup image
    // (the breaks translated, core/fm1sync.c) and back; the USB link
    bool exportFm1(juce::MemoryBlock& image, juce::String& report);
    bool importFm1(const juce::MemoryBlock& image, juce::String& report);
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
    struct CmdItem { uint8_t cmd; int8_t a; int16_t b; };
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
    uint32_t coreWaveSerial = 0;
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };
    std::vector<float> scopeRing = std::vector<float>(4096, 0.0f);
    std::atomic<int> scopeAt { 0 };
    int whiteKey[16] {}, blackKey[11] {};
    std::atomic<double> lastAudioMs { 0.0 };
    double idleFrom = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoafBreakerProcessor)
};
