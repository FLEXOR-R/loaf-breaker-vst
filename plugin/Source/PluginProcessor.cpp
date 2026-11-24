#include "PluginProcessor.h"
#include "PluginEditor.h"

static constexpr double CORE_RATE = 44100.0;
static uint32_t pktRT(uint8_t b) { return 0x0Fu | (uint32_t) b << 8; }

static juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "volume", 1 }, "Volume",
                                                      juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID { "sync", 1 }, "Host sync", true));
    return l;
}

LoafBreakerProcessor::LoafBreakerProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      params(*this, nullptr, "LOAF", layout())
{
    fifo.assign(2 * 8192, 0.0f);          // (before any prepareToPlay: the idle pump uses them too)
    tmpL.assign(8192, 0.0f);
    tmpR.assign(8192, 0.0f);
    chunk.assign(2 * 256, 0.0f);
    boot();
}

LoafBreakerProcessor::~LoafBreakerProcessor()
{
    const juce::ScopedLock sl(lock);
    core.unload();
}

bool LoafBreakerProcessor::boot()
{
    const juce::ScopedLock sl(lock);
    ready = false;
    if (! core.load(error))
        return false;
    if (pendingFlash.getSize() > 0)
        core.flashSet(static_cast<const uint8_t*>(pendingFlash.getData()), (uint32_t) pendingFlash.getSize());
    const auto& bank = Core::bankFile();     // (every break, HD: the FM-1's eight compressed ones without it)
    core.bank(static_cast<const uint8_t*>(bank.getData()), (uint32_t) bank.getSize());
    core.init(5000);                     // the start screen run through silently (5 s of the core's time)
    core.master(1023);
    nBreaks = core.breaks();
    for (int i = 0; i < 16; ++i) whiteKey[i] = core.keyIndex(0, i);
    for (int i = 0; i < 11; ++i) blackKey[i] = core.keyIndex(1, i);
    coreWaveSerial = 0;
    core.sync(1);
    coreFrames = 0;
    coreMsDone = 0;
    fifoN = 0;
    clockRunning = false;
    hostWasPlaying = false;
    coreScreenWrites = 0xFFFFFFFFu;
    error.clear();
    ready = true;
    return true;
}

bool LoafBreakerProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void LoafBreakerProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const juce::ScopedLock sl(lock);
    hostRate = sampleRate;
    const int maxCore = (int) std::ceil(samplesPerBlock * CORE_RATE / sampleRate) + 64;
    fifo.assign((size_t) (maxCore + 1024) * 2, 0.0f);
    tmpL.assign((size_t) maxCore + 1024, 0.0f);
    tmpR.assign((size_t) maxCore + 1024, 0.0f);
    chunk.assign(2 * 256, 0.0f);
    fifoN = 0;
    interp[0].reset();
    interp[1].reset();
}

void LoafBreakerProcessor::send(Cmd c, int a, int b)
{
    const auto scope = cmdFifo.write(1);
    if (scope.blockSize1 > 0)
        cmdBuf[scope.startIndex1] = { (uint8_t) c, (int8_t) a, (int16_t) b };
    else if (scope.blockSize2 > 0)
        cmdBuf[scope.startIndex2] = { (uint8_t) c, (int8_t) a, (int16_t) b };
}

void LoafBreakerProcessor::drainCommands()
{
    const auto scope = cmdFifo.read(cmdFifo.getNumReady());
    auto run = [this](const CmdItem& it) {
        if (it.cmd == CmdButton) core.button(it.a, it.b);
        else if (it.cmd == CmdKey) core.key(it.a, it.b != 0, it.b > 0 ? it.b : 100);
        else if (it.cmd == CmdTurn) core.turn(it.a, it.b);
    };
    for (int i = 0; i < scope.blockSize1; ++i) run(cmdBuf[scope.startIndex1 + i]);
    for (int i = 0; i < scope.blockSize2; ++i) run(cmdBuf[scope.startIndex2 + i]);
}

void LoafBreakerProcessor::coreMs()
{
    const bool frame = (coreMsDone % 15u) == 0u;
    core.ms(frame ? 1 : 0);
    coreMsDone++;
    if (frame) {                         // the screen and the LEDs, copied when the core drew
        uint32_t w = core.screen(nullptr);
        if (w != coreScreenWrites) {
            const juce::SpinLock::ScopedTryLockType tl(screenLock);
            if (tl.isLocked()) {
                core.screen(screenBuf);
                core.leds(ledBuf, sizeof ledBuf);
                coreScreenWrites = w;
                screenSerial++;
            }
        }
        playingNow = core.playing();
        if ((coreMsDone / 15u) % 2u == 0u) {   // the page's model, every other frame (30 a second)
            const juce::SpinLock::ScopedTryLockType tl(screenLock);
            if (tl.isLocked()) {
                core.model(&modelBuf);
                modelSerial++;
                const uint32_t ws = core.wave(nullptr, 0, nullptr, nullptr);
                if (ws != coreWaveSerial) {   // another break: its waveform, once
                    float sl[32];
                    int ns = 0;
                    waveBuf.cols.resize(4 * WAVE_COLS);
                    core.wave(waveBuf.cols.data(), WAVE_COLS, sl, &ns);
                    waveBuf.slices.assign(sl, sl + ns);
                    waveBuf.serial = ws;
                    coreWaveSerial = ws;
                }
            }
        }
    }
}

void LoafBreakerProcessor::produceCore(int n)
{
    while (n > 0) {
        const uint64_t nextMs = (coreMsDone + 1) * 441u / 10u;   // the core's next millisecond, in frames
        if (coreFrames >= nextMs) {
            coreMs();
            continue;
        }
        const int m = (int) std::min<uint64_t>((uint64_t) std::min(n, 256), nextMs - coreFrames);
        core.audio(chunk.data(), (uint32_t) m);
        std::copy(chunk.begin(), chunk.begin() + 2 * m, fifo.begin() + 2 * fifoN);
        fifoN += m;
        coreFrames += (uint64_t) m;
        n -= m;
    }
}

void LoafBreakerProcessor::sendMidi(uint32_t pkt) { core.midiIn(pkt); }

// the host's transport, as LOAF BREAKER's MIDI clock in: started at the host's next bar (4/4), 24 ticks a beat, stopped
// with the host; messages put in `out` at their sample offsets (as the 0xF0.. bytes, sent to the core in order)
void LoafBreakerProcessor::hostClock(int numSamples, juce::MidiBuffer& out)
{
    auto* ph = getPlayHead();
    if (ph == nullptr)
        return;
    const auto pos = ph->getPosition();
    if (! pos.hasValue())
        return;
    const bool playing = pos->getIsPlaying();
    const double bpm = pos->getBpm().orFallback(170.0);
    const auto ppqOpt = pos->getPpqPosition();
    if (! playing || ! ppqOpt.hasValue()) {
        if (clockRunning || hostWasPlaying)
            out.addEvent(juce::MidiMessage::midiStop(), 0);
        clockRunning = false;
        hostWasPlaying = false;
        return;
    }
    const double ppq0 = *ppqOpt, spq = hostRate * 60.0 / bpm;   // samples a quarter note
    if (! hostWasPlaying) {               // just started: LOAF BREAKER starts at the next bar (now if on one)
        const double bar = std::ceil(ppq0 / 4.0 - 1e-6) * 4.0;
        startAtPpq = bar;
        nextTick = (int64_t) std::ceil(ppq0 * 24.0 - 1e-6);
        hostWasPlaying = true;
    }
    const double ppq1 = ppq0 + numSamples / spq;
    if (! clockRunning && startAtPpq < ppq1) {
        const int at = juce::jlimit(0, numSamples - 1, (int) ((startAtPpq - ppq0) * spq));
        out.addEvent(juce::MidiMessage::midiStart(), at);
        clockRunning = true;
        nextTick = (int64_t) std::llround(startAtPpq * 24.0);
    }
    if ((double) nextTick < ppq0 * 24.0 - 48.0)   // (the host jumped: a loop, a locate)
        nextTick = (int64_t) std::ceil(ppq0 * 24.0 - 1e-6);
    while ((double) nextTick / 24.0 < ppq1) {
        const int at = juce::jlimit(0, numSamples - 1, (int) (((double) nextTick / 24.0 - ppq0) * spq));
        out.addEvent(juce::MidiMessage::midiClock(), at);
        nextTick++;
    }
}

void LoafBreakerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    const juce::ScopedTryLock sl(lock);
    if (! sl.isLocked() || ! ready || ! core.loaded())
        return;
    lastAudioMs = juce::Time::getMillisecondCounterHiRes();
    drainCommands();
    juce::MidiBuffer events;
    if (params.getRawParameterValue("sync")->load() > 0.5f)
        hostClock(n, events);
    core.sync(params.getRawParameterValue("sync")->load() > 0.5f);
    for (const auto m : midi)
        events.addEvent(m.getMessage(), m.samplePosition);
    midi.clear();

    const double ratio = CORE_RATE / hostRate;
    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    const float gain = params.getRawParameterValue("volume")->load();
    int done = 0;
    auto it = events.cbegin();
    while (done < n) {
        // the events due now, to the core; then the samples up to the next one
        int until = n;
        while (it != events.cend() && (*it).samplePosition <= done) {
            const auto msg = (*it).getMessage();
            const auto* d = msg.getRawData();
            const int len = msg.getRawDataSize();
            if (len >= 1 && d[0] >= 0xF8)
                sendMidi(pktRT(d[0]));
            else if (len >= 2 && d[0] >= 0x80 && d[0] < 0xF0)
                sendMidi((uint32_t) (d[0] >> 4) | (uint32_t) d[0] << 8 | (uint32_t) d[1] << 16
                         | (uint32_t) (len > 2 ? d[2] : 0) << 24);
            ++it;
        }
        if (it != events.cend())
            until = juce::jmax(done + 1, (*it).samplePosition);
        const int m = until - done;
        if (hostRate == CORE_RATE) {
            produceCore(m);
            for (int i = 0; i < m; ++i) {
                outL[done + i] = fifo[2 * i] * gain;
                if (outR) outR[done + i] = fifo[2 * i + 1] * gain;
            }
            fifoN = 0;
        } else {
            const int need = (int) std::ceil(m * ratio) + 4;
            if (fifoN < need)
                produceCore(need - fifoN);
            for (int i = 0; i < fifoN; ++i) {
                tmpL[(size_t) i] = fifo[2 * i];
                tmpR[(size_t) i] = fifo[2 * i + 1];
            }
            const int used = interp[0].process(ratio, tmpL.data(), outL + done, m);
            if (outR) interp[1].process(ratio, tmpR.data(), outR + done, m);
            for (int i = 0; i < m; ++i) {
                outL[done + i] *= gain;
                if (outR) outR[done + i] *= gain;
            }
            const int left = juce::jmax(0, fifoN - used);
            std::copy(fifo.begin() + 2 * used, fifo.begin() + 2 * (used + left), fifo.begin());
            fifoN = left;
        }
        done = until;
    }
    {                                    // the meters and the scope
        float pl = 0.0f, pr = 0.0f;
        int at = scopeAt.load();
        for (int i = 0; i < n; ++i) {
            const float a = outL[i], b = outR ? outR[i] : a;
            pl = juce::jmax(pl, std::abs(a));
            pr = juce::jmax(pr, std::abs(b));
            scopeRing[(size_t) at] = 0.5f * (a + b);
            at = (at + 1) % (int) scopeRing.size();
        }
        scopeAt = at;
        peakL = juce::jmax(peakL.load(), pl);
        peakR = juce::jmax(peakR.load(), pr);
    }
    uint32_t pkt;
    while (core.midiOut(&pkt))           // (the core's MIDI out: not passed on yet)
        ;
}

void LoafBreakerProcessor::idlePump()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (now - lastAudioMs.load() < 250.0) {
        idleFrom = 0.0;
        return;
    }
    if (idleFrom == 0.0) {
        idleFrom = now;
        return;
    }
    const juce::ScopedTryLock sl(lock);
    if (! sl.isLocked() || ! ready || ! core.loaded())
        return;
    drainCommands();
    const int frames = juce::jlimit(0, 44100 / 4, (int) ((now - idleFrom) * CORE_RATE / 1000.0));
    idleFrom += frames * 1000.0 / CORE_RATE;
    int left = frames;
    while (left > 0) {                   // (the sound drawn and dropped, in pieces the FIFO holds)
        const int m = juce::jmin(left, 256);
        produceCore(m);
        fifoN = 0;
        left -= m;
    }
}

bool LoafBreakerProcessor::modelIfNew(uint32_t& serial, lb_model_t& out)
{
    const juce::SpinLock::ScopedLockType sl(screenLock);
    if (serial == modelSerial)
        return false;
    out = modelBuf;
    serial = modelSerial;
    return true;
}

bool LoafBreakerProcessor::waveIfNew(uint32_t& serial, WaveView& out)
{
    const juce::SpinLock::ScopedLockType sl(screenLock);
    if (serial == waveBuf.serial || waveBuf.cols.empty())
        return false;
    out = waveBuf;
    serial = waveBuf.serial;
    return true;
}

void LoafBreakerProcessor::meters(float& l, float& r)
{
    l = peakL.exchange(0.0f);
    r = peakR.exchange(0.0f);
}

void LoafBreakerProcessor::scope(float* out, int n)
{
    const int sz = (int) scopeRing.size(), at = scopeAt.load();
    for (int i = 0; i < n; ++i)
        out[i] = scopeRing[(size_t) ((at - n + i + sz * 2) % sz)];
}

bool LoafBreakerProcessor::screenIfNew(uint32_t& serial, uint16_t* dst)
{
    const juce::SpinLock::ScopedLockType sl(screenLock);
    if (serial == screenSerial)
        return false;
    std::memcpy(dst, screenBuf, sizeof screenBuf);
    serial = screenSerial;
    return true;
}

void LoafBreakerProcessor::ledsCopy(uint8_t* dst, size_t n)
{
    const juce::SpinLock::ScopedLockType sl(screenLock);
    std::memcpy(dst, ledBuf, juce::jmin(n, sizeof ledBuf));
}

juce::String LoafBreakerProcessor::status() const
{
    if (error.isNotEmpty())
        return error;
    return juce::String(playingNow.load() ? "PLAYING" : "STOPPED") + " · " + juce::String(nBreaks.load()) + " BREAKS";
}

// the project's state: the firmware's saved objects (SAVE done first), deflated; the plugin's parameters
void LoafBreakerProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    juce::MemoryBlock flash;
    {
        const juce::ScopedLock sl(lock);
        if (ready && core.loaded()) {
            core.save();
            flash.setSize(core.flashSize());
            core.flashGet(static_cast<uint8_t*>(flash.getData()), (uint32_t) flash.getSize());
        } else
            flash = pendingFlash;
    }
    juce::MemoryOutputStream out(dest, false);
    out.writeInt(0x4C42524Bu);           // "LBRK"
    out.writeInt(1);
    auto p = params.copyState().createXml();
    out.writeString(p ? p->toString() : juce::String());
    juce::MemoryBlock z;
    {
        juce::MemoryOutputStream zs(z, false);
        juce::GZIPCompressorOutputStream gz(zs, 6);
        gz.write(flash.getData(), flash.getSize());
    }
    out.writeInt64((int64_t) z.getSize());
    out.write(z.getData(), z.getSize());
}

void LoafBreakerProcessor::setStateInformation(const void* data, int size)
{
    juce::MemoryInputStream in(data, (size_t) size, false);
    if (in.readInt() != 0x4C42524B || in.readInt() != 1)
        return;
    if (auto x = juce::parseXML(in.readString()))
        params.replaceState(juce::ValueTree::fromXml(*x));
    const auto zn = in.readInt64();
    juce::MemoryBlock z;
    in.readIntoMemoryBlock(z, (int) zn);   // (the image deflated: well under 2 GB)
    juce::MemoryInputStream zi(z, false);
    juce::GZIPDecompressorInputStream gz(zi);
    juce::MemoryBlock flash;
    gz.readIntoMemoryBlock(flash);
    pendingFlash = flash;
    boot();                              // a fresh core with this project's image
}

bool LoafBreakerProcessor::exportFm1(juce::MemoryBlock& image, juce::String& report)
{
    const juce::ScopedLock sl(lock);
    if (! ready || ! core.loaded())
        return false;
    image.setSize(100000);
    char rep[1024] = {};
    const uint32_t n = core.fm1Export(static_cast<uint8_t*>(image.getData()), (uint32_t) image.getSize(), rep, sizeof rep);
    image.setSize(n);
    report = juce::String(rep);
    return n > 0;
}

bool LoafBreakerProcessor::importFm1(const juce::MemoryBlock& image, juce::String& report)
{
    {
        const juce::ScopedLock sl(lock);
        if (! ready || ! core.loaded())
            return false;
        char rep[1024] = {};
        const int r = core.fm1Import(static_cast<const uint8_t*>(image.getData()), (uint32_t) image.getSize(), rep, sizeof rep);
        report = r == 0 ? juce::String(rep) : r == 1 ? juce::String("Ce n'est pas une sauvegarde LOAF BREAKER.")
                                                     : juce::String("Sauvegarde abimee.");
        if (r != 0)
            return false;
        juce::MemoryBlock flash(core.flashSize());
        core.flashGet(static_cast<uint8_t*>(flash.getData()), (uint32_t) flash.getSize());
        pendingFlash = flash;
    }
    boot();                              // a fresh core with it
    return true;
}

juce::AudioProcessorEditor* LoafBreakerProcessor::createEditor() { return new LoafBreakerEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LoafBreakerProcessor(); }
