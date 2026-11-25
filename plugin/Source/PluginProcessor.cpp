#include "PluginProcessor.h"
#include "Ui.h"
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
    picThread = std::make_unique<PicThread>(*this);
    picThread->startThread(juce::Thread::Priority::low);
    ctl.paused = [this] { return link.busy(); };
    if (ui::settings().getUserSettings()->getBoolValue("fm1ctl", false))
        setController(true);
}

LoafBreakerProcessor::~LoafBreakerProcessor()
{
    ctlWant = false;
    ctl.stop();
    if (picThread) picThread->stopThread(2000);
    const juce::ScopedLock pl(picLife);
    const juce::ScopedLock sl(lock);
    core.unload();
}

// the pictures: the sound looked at 25 times a second (its signature), drawn again when it, or what is wanted, changed
void LoafBreakerProcessor::PicThread::run()
{
    uint32_t lastSig = 0;
    int lastWhat = -2, lastMs = 0, lastN = 0;
    std::vector<float> tmp;
    while (! threadShouldExit()) {
        wait(40);
        const int what = proc.picWhat, ms = proc.picMs, n = juce::jlimit(16, 2048, (int) proc.picN);
        if (what < 0) continue;
        const juce::ScopedTryLock life(proc.picLife);
        if (! life.isLocked() || ! proc.ready || ! proc.core.loaded() || ! proc.core.pic) continue;
        const uint32_t sig = proc.core.picSig(what);
        if (sig == lastSig && what == lastWhat && ms == lastMs && n == lastN) continue;
        tmp.assign((size_t) n * 5, 0.0f);
        proc.core.pic(what, n, ms, tmp.data());
        lastSig = sig, lastWhat = what, lastMs = ms, lastN = n;
        const juce::SpinLock::ScopedLockType sl(proc.picLock);
        proc.picBuf.cols.swap(tmp);
        proc.picBuf.what = what, proc.picBuf.ms = ms, proc.picBuf.n = n;
        proc.picBuf.serial++;
    }
}

bool LoafBreakerProcessor::picIfNew(uint32_t& serial, PicView& out)
{
    const juce::SpinLock::ScopedLockType sl(picLock);
    if (serial == picBuf.serial || picBuf.cols.empty()) return false;
    out = picBuf;
    serial = picBuf.serial;
    return true;
}

bool LoafBreakerProcessor::boot()
{
    const juce::ScopedLock pl(picLife);
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
    blockSize = samplesPerBlock;
    mwLen = juce::jmax(16, (int) std::lround(sampleRate / 375.0));   // (~375 columns a second, whatever the rate)         // (the host's buffer: most of the delay a key from the FM-1 waits)
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

void LoafBreakerProcessor::step(int op, int a, int b, int c, int d)
{
    const auto scope = cmdFifo.write(1);
    const CmdItem it { (uint8_t) CmdStep, (int8_t) op, (int16_t) a, (int8_t) b, (int8_t) c, (int8_t) juce::jlimit(-127, 127, d) };
    if (scope.blockSize1 > 0) cmdBuf[scope.startIndex1] = it;
    else if (scope.blockSize2 > 0) cmdBuf[scope.startIndex2] = it;
}

void LoafBreakerProcessor::drainCommands()
{
    {                                    // (a track written whole, from the arranger)
        const juce::SpinLock::ScopedTryLockType tl(trackLock);
        if (tl.isLocked() && trackPending) {
            for (int t = 0; t < 8; ++t)
                if (trackPending >> t & 1) core.trackWrite(&trackOut[t]);
            trackPending = 0;
            tracksAge = 99;
        }
    }
    const auto scope = cmdFifo.read(cmdFifo.getNumReady());
    auto run = [this](const CmdItem& it) {
        if (it.cmd == CmdButton) core.button(it.a, it.b);
        else if (it.cmd == CmdKey && it.a < 0) core.drumHit(-1 - it.a);   // (a sound heard alone: drumHit)
        else if (it.cmd == CmdKey) core.key(it.a, it.b != 0, it.b > 0 ? it.b : 100);
        else if (it.cmd == CmdTurn) core.turn(it.a, it.b);
        else if (it.cmd == CmdTempo) core.tempo(it.b);
        else if (it.cmd == CmdStep) { core.step(it.a, it.b, (uint8_t) it.c, it.d, it.e); patsWanted = true; }
        else if (it.cmd == CmdSet) { core.set(it.a, it.c, it.b); partsWanted = true; }
        else if (it.cmd == CmdBreak) core.breakGo(it.b);
        else if (it.cmd == CmdTrack) { core.trackPlay(it.a, it.b); tracksAge = 99; }
        else if (it.cmd == CmdKeyKnob) core.keyKnobTurn(it.a >> 5, it.a & 31, it.c, it.b);
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
                if (const int kw = kkWant.load(); kw >= 0) {   // the key looked at: its settings
                    lb_keyknobs_t k {};
                    if (core.keyKnobs(kw >> 5, kw & 31, &k)) { kkBuf = k; kkSerial++; }
                }
                core.seq(&seqBuf);
                seqSerial++;
                if (patsWanted || ++patsAge >= 30 || seqBuf.pat != lastPat) {   // (a second, an edit, another pattern)
                    core.patterns(patsBuf);
                    patsSerial++;
                    patsAge = 0;
                    patsWanted = false;
                    lastPat = seqBuf.pat;
                }
                serveMenus();
                if (tracksWanted && ++tracksAge >= 4) {   // (the arranger shown: the tracks 7 times a second)
                    for (int t = 0; t < 8; ++t) core.track(t, &tracksBuf[t]);
                    tracksSerial++;
                    tracksAge = 0;
                }
                const uint32_t ws = core.wave(nullptr, 0, nullptr, nullptr);
                if (ws != coreWaveSerial) {   // another break: its waveform, once
                    float sl[32];
                    int ns = 0;
                    waveBuf.cols.resize(4 * WAVE_COLS);
                    core.wave(waveBuf.cols.data(), WAVE_COLS, sl, &ns);
                    waveBuf.slices.assign(sl, sl + ns);
                    waveBuf.mm.resize(2 * WAVE_COLS);
                    core.waveMM(waveBuf.mm.data(), WAVE_COLS);
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

// ---- the FM-1 as the controller (Fm1Ctl.h says the messages) ----
void LoafBreakerProcessor::setController(bool on)
{
    ctlWant = on;
    ui::settings().getUserSettings()->setValue("fm1ctl", on);
    if (on)
        ctl.start();
    else {
        ctl.stop();
        ctlState = 0;
        ctlKeys = ctlBtns = 0;
    }
}

void LoafBreakerProcessor::ctlApply(uint32_t msg)
{
    const int st = (int) (msg & 0xFF), a = (int) (msg >> 8 & 0x7F), v = (int) (msg >> 16 & 0x7F);
    const bool down = st == 0x9F && v > 0;
    if (st == 0x9F || st == 0x8F) {
        if (a < 14) {                    // a button (LB_* = B_*)
            core.button(a, down ? 1 : 0);
            ctlBtns = down ? ctlBtns.load() | 1u << a : ctlBtns.load() & ~(1u << a);
        } else if (a >= 16 && a < 43) {  // a key, F3 .. G5
            core.key(a - 16, down, 127);
            ctlKeys = down ? ctlKeys.load() | 1u << (a - 16) : ctlKeys.load() & ~(1u << (a - 16));
        }
    } else if (st == 0xBF) {
        if (a >= 16 && a < 23 && v != 64) {
            const int r = a - 16, d = v - 64;
            core.turnHw(r, d);           // (LB_SELECT .. LB_K4 = EN_*: as the FM-1 reads them, SELECT the tempo there)
        }
        else if (a == 25)
            ctlMlo = v & 7;
        else if (a == 24)
            core.master(v << 3 | ctlMlo);
        else if (a == 127 && v)
            ctlAnswer = juce::Time::getMillisecondCounterHiRes();
    }
}

void LoafBreakerProcessor::ctlServe()
{
    if (! ctlWant.load()) {
        ctlWas = false;
        return;
    }
    const double now = juce::Time::getMillisecondCounterHiRes();
    uint32_t msg;
    while (ctl.popIn(msg))
        ctlApply(msg);
    bool pushed = false;
    if (! ctlWas || now - ctlKeepT >= 1000.0) {   // on, and every second: the FM-1 stays CONTROLLER (3 s without: off)
        ctlWas = true;
        ctlKeepT = now;
        ctl.pushOut(0xBF | 127u << 8 | 1u << 16);
        pushed = true;
    }
    const bool alive = now - ctlAnswer < 2500.0;
    if (alive && ctlState.load() != 2)   // (just come into CONTROLLER: it is dark, all its lights again)
        ctlFullT = 0.0;
    ctlState = alive ? 2 : 1;
    if (! alive && (ctlKeys.load() | ctlBtns.load())) {   // (gone, unplugged: what it held let go)
        for (int k = 0; k < 27; ++k)
            if (ctlKeys.load() >> k & 1u) core.key(k, false, 0);
        for (int b = 0; b < 14; ++b)
            if (ctlBtns.load() >> b & 1u) core.button(b, 0);
        ctlKeys = ctlBtns = 0;
    }
    // the lights: every audio block (FLEXOR: "la latence très minimale"), the changed ones; all again every 5 s
    uint32_t o[6] {};
    core.lights(o);
    const bool full = now - ctlFullT >= 5000.0;
    if (full)
        ctlFullT = now;
    for (int i = 0; i < 43; ++i) {
        uint8_t lv = 0;
        if (i <= 14)                     // the buttons, 14 PLAY's green
            lv = o[0] >> i & 1u ? 3 : o[1] >> i & 1u ? 1 : 0;
        else if (i >= 16) {
            const int k = i - 16;
            lv = o[2] >> k & 1u ? 3 : o[3] >> k & 1u ? 2 : o[4] >> k & 1u ? 1 : 0;
        } else
            continue;
        if (lv != ctlSent[i] || full) {
            ctlSent[i] = lv;
            ctl.pushOut(lv ? (0x9Fu | (uint32_t) i << 8 | (uint32_t) lv << 16) : (0x8Fu | (uint32_t) i << 8));
            pushed = true;
        }
    }
    if (pushed)
        ctl.flush();
}

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
    const bool ctlOn = ctlWant.load();
    for (const auto m : midi) {
        const auto* d = m.getMessage().getRawData();
        if (ctlOn && m.getMessage().getRawDataSize() >= 3 && d[0] >= 0x80 && d[0] < 0xF0 && (d[0] & 0x0F) == 0x0F) {
            if (! ctl.inOpen.load())     // (the FM-1 through the DAW: its port is the DAW's; else its own port has it)
                ctlApply((uint32_t) d[0] | (uint32_t) d[1] << 8 | (uint32_t) d[2] << 16);
            continue;                    // (channel 16 is the FM-1's while it plays the VST: never notes)
        }
        events.addEvent(m.getMessage(), m.samplePosition);
    }
    midi.clear();
    ctlServe();

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
    {                                    // a kick heard from the library or a backup, over it
        const juce::SpinLock::ScopedTryLockType pl(previewLock);
        if (pl.isLocked() && ! preview.empty()) {
            const double step = CORE_RATE / hostRate;
            const size_t frames = preview.size() / 2;
            for (int i = 0; i < n && previewAt < (double) frames; ++i, previewAt += step) {
                const size_t k = (size_t) previewAt;
                outL[i] += preview[2 * k] * gain;
                if (outR) outR[i] += preview[2 * k + 1] * gain;
            }
            if (previewAt >= (double) frames) preview.clear();
        }
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
            mwFeed(0.5f * (a + b));      // the Master's waveform
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
    ctlServe();
    const int frames = juce::jlimit(0, 44100 / 4, (int) ((now - idleFrom) * CORE_RATE / 1000.0));
    idleFrom += frames * 1000.0 / CORE_RATE;
    int left = frames;
    while (left > 0) {                   // (the sound drawn and dropped, in pieces the FIFO holds)
        const int m = juce::jmin(left, 256);
        produceCore(m);
        const float g = params.getRawParameterValue("volume")->load();
        for (int i = 0; i < m; ++i)      // (no audio running: the Master's waveform all the same, as the core plays)
            mwFeed(0.5f * (fifo[(size_t) (2 * i)] + fifo[(size_t) (2 * i + 1)]) * g);
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

bool LoafBreakerProcessor::keyKnobsIfNew(uint32_t& serial, lb_keyknobs_t& out)
{
    const juce::SpinLock::ScopedLockType sl(screenLock);
    if (serial == kkSerial)
        return false;
    out = kkBuf;
    serial = kkSerial;
    return true;
}

void LoafBreakerProcessor::keyKnobTurn(int black, int i, int k, int d)
{
    if (d == 0) return;
    const auto scope = cmdFifo.write(1);
    const CmdItem it { (uint8_t) CmdKeyKnob, (int8_t) (black * 32 + i), (int16_t) juce::jlimit(-100, 100, d), (int8_t) k, 0, 0 };
    if (scope.blockSize1 > 0) cmdBuf[scope.startIndex1] = it;
    else if (scope.blockSize2 > 0) cmdBuf[scope.startIndex2] = it;
}

void LoafBreakerProcessor::setValue(int part, int knob, int steps)
{
    if (steps == 0) return;
    const auto scope = cmdFifo.write(1);
    const CmdItem it { (uint8_t) CmdSet, (int8_t) part, (int16_t) juce::jlimit(-1000, 1000, steps), (int8_t) knob, 0, 0 };
    if (scope.blockSize1 > 0) cmdBuf[scope.startIndex1] = it;
    else if (scope.blockSize2 > 0) cmdBuf[scope.startIndex2] = it;
}

// (the audio thread, 30 times a second, the screen lock held): the page scanned again when it may have changed (an
// edit, or every few frames), then one setting's values looked at, if asked
void LoafBreakerProcessor::serveMenus()
{
    const juce::SpinLock::ScopedTryLockType tl(menuLock);
    if (! tl.isLocked()) return;
    if (partsWanted || ++partsAge >= 4) {
        nParts = core.scan(partsBuf, MAX_PARTS);
        if (++fxAge >= 4 || partsWanted) {   // (an effect's page: the effects PRESETS goes through, twice a second)
            nFx = core.fxList(fxNames, 32, &fxCur);
            fxAge = 0;
        }
        partsSerial++;
        partsAge = 0;
        partsWanted = false;
    }
    if (nAsks > 0) {
        const OptAsk a = asks[0];
        for (int i = 1; i < nAsks; ++i) asks[i - 1] = asks[i];
        nAsks--;
        auto& r = answers[a.id % 16];
        r.id = a.id;
        r.ok = a.part < nParts && std::strcmp(partsBuf[a.part].part, a.pname) == 0
               && std::strcmp(partsBuf[a.part].kname[a.knob & 3], a.kname) == 0
               && core.options(a.part, a.knob, &r.o) > 0;
    }
}

bool LoafBreakerProcessor::partsIfNew(uint32_t& serial, PartsView& out)
{
    const juce::SpinLock::ScopedLockType sl(menuLock);
    if (serial == partsSerial) return false;
    out.parts.assign(partsBuf, partsBuf + nParts);
    out.fx.clear();
    for (int i = 0; i < nFx; ++i) out.fx.add(juce::String(fxNames[i]));
    out.fxCur = nFx > 0 ? fxCur : -1;
    out.serial = serial = partsSerial;
    return true;
}

int LoafBreakerProcessor::askOptions(int part, int knob, const char* partName, const char* knobName)
{
    const juce::SpinLock::ScopedLockType sl(menuLock);
    if (nAsks >= 16) return -1;
    OptAsk& a = asks[nAsks++];
    a.id = nextAsk++;
    a.part = part;
    a.knob = knob;
    juce::String(partName).copyToUTF8(a.pname, sizeof a.pname);
    juce::String(knobName).copyToUTF8(a.kname, sizeof a.kname);
    answers[a.id % 16].id = -1;
    return a.id;
}

bool LoafBreakerProcessor::takeOptions(int id, lb_opts_t& out)
{
    const juce::SpinLock::ScopedLockType sl(menuLock);
    auto& r = answers[id % 16];
    if (r.id != id) return false;
    r.id = -1;
    if (! r.ok) { out.n = 0; return true; }
    out = r.o;
    return true;
}

bool LoafBreakerProcessor::tracksIfNew(uint32_t& serial, std::vector<lb_track_t>& out)
{
    const juce::SpinLock::ScopedLockType sl(screenLock);
    if (serial == tracksSerial) return false;
    out.assign(tracksBuf, tracksBuf + 8);
    serial = tracksSerial;
    return true;
}

void LoafBreakerProcessor::trackWrite(const lb_track_t& t)
{
    const juce::SpinLock::ScopedLockType sl(trackLock);
    const int i = juce::jlimit(0, 7, t.track);
    trackOut[i] = t;
    trackPending = (uint8_t) (trackPending | 1 << i);
}

bool LoafBreakerProcessor::seqIfNew(uint32_t& serial, lb_seq_t& out)
{
    const juce::SpinLock::ScopedLockType sl(screenLock);
    if (serial == seqSerial)
        return false;
    out = seqBuf;
    serial = seqSerial;
    return true;
}

bool LoafBreakerProcessor::patternsIfNew(uint32_t& serial, uint8_t* out)
{
    const juce::SpinLock::ScopedLockType sl(screenLock);
    if (serial == patsSerial)
        return false;
    std::memcpy(out, patsBuf, sizeof patsBuf);
    serial = patsSerial;
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

void LoafBreakerProcessor::mwFeed(float x)   // (a column every mwLen samples: its extremes, its three bands' energy)
{
    mwLp1 += (x - mwLp1) * 0.026f;       // (~200 Hz at 48 kHz)
    mwLp2 += (x - mwLp2) * 0.23f;        // (~2 kHz)
    if (mwN == 0) mwAcc.mn = mwAcc.mx = x;
    mwAcc.mn = juce::jmin(mwAcc.mn, x);
    mwAcc.mx = juce::jmax(mwAcc.mx, x);
    mwAcc.lo += mwLp1 * mwLp1;
    mwAcc.mi += (mwLp2 - mwLp1) * (mwLp2 - mwLp1);
    mwAcc.hi += (x - mwLp2) * (x - mwLp2);
    if (++mwN >= mwLen) {
        const float k = 1.0f / (float) mwN;
        MwCol c = mwAcc;
        c.lo = std::sqrt(c.lo * k), c.mi = std::sqrt(c.mi * k), c.hi = std::sqrt(c.hi * k);
        const int w = mwAt.load();
        mw[(size_t) (w % MW_N)] = c;
        mwAt = w + 1;
        mwAcc = MwCol {};
        mwN = 0;
    }
}

int LoafBreakerProcessor::masterCols(MwCol* out, int n) const
{
    const int at = mwAt.load();
    n = juce::jmin(n, MW_N - 1, at);
    for (int i = 0; i < n; ++i)
        out[i] = mw[(size_t) ((at - n + i) % MW_N)];
    return n;
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

bool LoafBreakerProcessor::listFm1(const juce::MemoryBlock* image, lb_list_t& out)
{
    const juce::ScopedLock sl(lock);
    if (! ready || ! core.loaded())
        return false;
    return core.fm1List(image ? static_cast<const uint8_t*>(image->getData()) : nullptr, image ? (uint32_t) image->getSize() : 0u, &out) == 0;
}

bool LoafBreakerProcessor::putFm1(const juce::MemoryBlock& fm1, const std::vector<lb_pick_t>& picks, juce::MemoryBlock& out, juce::String& report)
{
    const juce::ScopedLock sl(lock);
    if (! ready || ! core.loaded())
        return false;
    out.setSize(200000);
    char rep[512] = {};
    const uint32_t n = core.fm1Put(static_cast<const uint8_t*>(fm1.getData()), (uint32_t) fm1.getSize(), picks.data(), (int) picks.size(),
                                   static_cast<uint8_t*>(out.getData()), (uint32_t) out.getSize(), rep, sizeof rep);
    out.setSize(n);
    report = rep;
    return n > 0;
}

bool LoafBreakerProcessor::takeFm1(const juce::MemoryBlock& fm1, const std::vector<lb_pick_t>& picks, juce::String& report)
{
    {
        const juce::ScopedLock sl(lock);
        if (! ready || ! core.loaded())
            return false;
        char rep[512] = {};
        const int r = core.fm1Take(static_cast<const uint8_t*>(fm1.getData()), (uint32_t) fm1.getSize(), picks.data(), (int) picks.size(), rep, sizeof rep);
        report = r < 0 ? juce::String("Ce n'est pas une sauvegarde LOAF BREAKER.") : juce::String(rep);
        if (r < 0)
            return false;
        juce::MemoryBlock flash(core.flashSize());
        core.flashGet(static_cast<uint8_t*>(flash.getData()), (uint32_t) flash.getSize());
        pendingFlash = flash;
    }
    boot();                              // a fresh core with them
    return true;
}

std::vector<LoafBreakerProcessor::BreakInfo> LoafBreakerProcessor::breaks()
{
    std::vector<BreakInfo> v;
    const juce::ScopedLock sl(lock);
    if (! ready || ! core.loaded())
        return v;
    const auto& idx = Core::breakIndex();
    const int n = core.breaks();
    for (int i = 0; i < n; ++i) {
        char t[32] = {};
        core.breakName(i, t, sizeof t);
        BreakInfo b { juce::String(t).trim(), "MES BREAKS", {} };   // (not in the index: the player's own)
        auto it = idx.find(b.name);
        if (it != idx.end()) { b.cat = it->second.first; b.title = it->second.second; }
        v.push_back(b);
    }
    return v;
}

int LoafBreakerProcessor::breakNow()
{
    const juce::ScopedLock sl(lock);
    return ready && core.loaded() ? core.get(0) : -1;
}

void LoafBreakerProcessor::breakGo(int i) { send(CmdBreak, 0, i); }

juce::StringArray LoafBreakerProcessor::playlist()
{
    return juce::StringArray::fromTokens(params.state.getProperty("playlist").toString(), "|", "");
}

void LoafBreakerProcessor::setPlaylist(const juce::StringArray& names)
{
    params.state.setProperty("playlist", names.joinIntoString("|"), nullptr);
}

void LoafBreakerProcessor::breakStep(int dir)
{
    const auto all = breaks();
    const int now = breakNow();
    if (all.empty() || now < 0) return;
    const auto pl = playlist();
    std::vector<int> order;                  // the playlist's breaks the bank has, in its order (none: every break)
    for (auto& n : pl)
        for (int i = 0; i < (int) all.size(); ++i)
            if (all[(size_t) i].name == n) { order.push_back(i); break; }
    if (order.empty())
        for (int i = 0; i < (int) all.size(); ++i) order.push_back(i);
    int at = -1;
    for (int k = 0; k < (int) order.size(); ++k) if (order[(size_t) k] == now) at = k;
    const int m = (int) order.size();
    const int next = at < 0 ? (dir > 0 ? 0 : m - 1) : ((at + dir) % m + m) % m;
    breakGo(order[(size_t) next]);
}

juce::MemoryBlock LoafBreakerProcessor::kickRead(const juce::MemoryBlock* fm1, int k)
{
    const juce::ScopedLock sl(lock);
    if (! ready || ! core.loaded())
        return {};
    juce::MemoryBlock b(core.kickSize());
    if (! core.kickRead(fm1 ? static_cast<const uint8_t*>(fm1->getData()) : nullptr, fm1 ? (uint32_t) fm1->getSize() : 0u, k,
                        static_cast<uint8_t*>(b.getData())))
        return {};
    return b;
}

void LoafBreakerProcessor::kickHear(const juce::MemoryBlock& blob)
{
    std::vector<float> lr(2 * 66150);    // (1.5 s)
    {
        const juce::ScopedLock sl(lock);
        if (! ready || ! core.loaded() || blob.getSize() != core.kickSize())
            return;
        if (! core.kickRender(static_cast<const uint8_t*>(blob.getData()), lr.data(), 66150))
            return;
    }
    const juce::SpinLock::ScopedLockType pl(previewLock);
    preview = std::move(lr);
    previewAt = 0.0;
}

bool LoafBreakerProcessor::putKicksFm1(juce::MemoryBlock& fm1, const std::vector<juce::MemoryBlock>& blobs, const std::vector<uint8_t>& to)
{
    const juce::ScopedLock sl(lock);
    if (! ready || ! core.loaded() || blobs.empty())
        return false;
    juce::MemoryBlock all;
    for (auto& b : blobs) all.append(b.getData(), b.getSize());
    juce::MemoryBlock out(200000);
    const uint32_t n = core.fm1PutKicks(static_cast<const uint8_t*>(fm1.getData()), (uint32_t) fm1.getSize(), static_cast<const uint8_t*>(all.getData()),
                                        to.data(), (int) blobs.size(), static_cast<uint8_t*>(out.getData()), (uint32_t) out.getSize());
    if (! n) return false;
    out.setSize(n);
    fm1 = out;
    return true;
}

bool LoafBreakerProcessor::putKicksVst(const std::vector<juce::MemoryBlock>& blobs, const std::vector<uint8_t>& to)
{
    {
        const juce::ScopedLock sl(lock);
        if (! ready || ! core.loaded() || blobs.empty())
            return false;
        juce::MemoryBlock all;
        for (auto& b : blobs) all.append(b.getData(), b.getSize());
        if (core.vstPutKicks(static_cast<const uint8_t*>(all.getData()), to.data(), (int) blobs.size()) <= 0)
            return false;
        juce::MemoryBlock flash(core.flashSize());
        core.flashGet(static_cast<uint8_t*>(flash.getData()), (uint32_t) flash.getSize());
        pendingFlash = flash;
    }
    boot();
    return true;
}

juce::AudioProcessorEditor* LoafBreakerProcessor::createEditor() { return new LoafBreakerEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LoafBreakerProcessor(); }
