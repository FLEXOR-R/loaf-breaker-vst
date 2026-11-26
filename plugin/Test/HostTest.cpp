// LOAF BREAKER VST: a host's check, run by the CI: the VST3 loaded as a DAW does, 48 kHz, 512-frame blocks, the host's
// transport playing at 170 BPM (or argv[3]) from bar 1: LOAF BREAKER must follow (playing, sound), its state must reload, then the
// host stops and it must stop. Writes the sound to a WAV beside it. lbtest PATH.vst3 OUT.wav
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

struct PlayHead : juce::AudioPlayHead
{
    double rate = 48000.0, bpm = 170.0;
    int64_t sample = 0;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying(playing);
        p.setBpm(bpm);
        p.setTimeInSamples(sample);
        p.setPpqPosition((double) sample / rate * bpm / 60.0);
        p.setTimeSignature(TimeSignature { 4, 4 });
        return p;
    }
};

static double rms(const juce::AudioBuffer<float>& b)
{
    double s = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            s += (double) b.getSample(c, i) * b.getSample(c, i);
    return std::sqrt(s / (b.getNumChannels() * b.getNumSamples()));
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 3) { std::puts("lbtest PATH.vst3 OUT.wav"); return 2; }
    juce::AudioPluginFormatManager fm;
    juce::addDefaultFormatsToManager(fm);
    juce::OwnedArray<juce::PluginDescription> found;
    juce::VST3PluginFormat vst3;
    vst3.findAllTypesForFile(found, argv[1]);
    if (found.isEmpty()) { std::puts("FAIL: no plugin in the file"); return 1; }
    juce::String err;
    const double rate = 48000.0;
    const int block = 512;
    // (the controller on for this run: the plugin reads it from the user's settings, as the VST's MENU leaves it)
    std::unique_ptr<juce::PropertiesFile> ctlP;
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "LOAF BREAKER VST";
        o.filenameSuffix = ".settings";
        o.folderName = "LOAF BREAKER";
        o.osxLibrarySubFolder = "Application Support";
        ctlP = std::make_unique<juce::PropertiesFile>(o);
        ctlP->setValue("fm1ctl", true);
        ctlP->saveIfNeeded();
    }
    auto t0 = juce::Time::getMillisecondCounterHiRes();
    auto p = fm.createPluginInstance(*found[0], rate, block, err);
    if (! p) { std::printf("FAIL: %s\n", err.toRawUTF8()); return 1; }
    std::printf("loaded in %.0f ms\n", juce::Time::getMillisecondCounterHiRes() - t0);
    PlayHead ph;
    ph.rate = rate;
    if (argc > 3) ph.bpm = std::atof(argv[3]);
    p->setPlayHead(&ph);
    p->enableAllBuses();
    p->prepareToPlay(rate, block);
    const int seconds = 12;
    juce::AudioBuffer<float> all(2, (int) rate * seconds), buf(2, block);
    juce::MidiBuffer midi;
    double cpu = 0;
    for (int at = 0; at + block <= all.getNumSamples(); at += block) {
        buf.clear();
        auto c0 = juce::Time::getMillisecondCounterHiRes();
        p->processBlock(buf, midi);
        cpu += juce::Time::getMillisecondCounterHiRes() - c0;
        ph.sample += block;
        for (int c = 0; c < 2; ++c)
            all.copyFrom(c, at, buf, c, 0, block);
    }
    std::printf("render: %.1f %% of real time\n", 100.0 * cpu / (seconds * 1000.0));
    juce::AudioBuffer<float> tail(2, (int) rate * 4);
    tail.copyFrom(0, 0, all, 0, (int) rate * 8, (int) rate * 4);
    tail.copyFrom(1, 0, all, 1, (int) rate * 8, (int) rate * 4);
    const double level = rms(tail);
    std::printf("playing: rms %.4f\n", level);
    // (26 Nov 2026, FLEXOR: "compatible avec Ableton et FL Studio") the host prepared again with a longer maximum (a
    // buffer setting changed), then blocks of every size up to it, odd ones, a single sample, as FL Studio hands them
    double bigLevel = 0;
    {
        const int maxB = block * 8 + 37;
        p->prepareToPlay(rate, maxB);
        static const int SIZES[] = { maxB, 1, 37, 2048, 511, 3, maxB, 1024, 96, maxB };
        for (int rep = 0; rep < 3; ++rep)
            for (int sz : SIZES) {
                juce::AudioBuffer<float> b(2, sz);
                b.clear();
                p->processBlock(b, midi);
                ph.sample += sz;
                if (rep > 0 && sz >= 1024) bigLevel = std::max(bigLevel, rms(b));
            }
        std::printf("blocks of 1 .. %d after a new prepareToPlay: rms %.4f\n", maxB, bigLevel);
        p->prepareToPlay(rate, block);
    }
    // the state: saved, and loaded into a second instance
    juce::MemoryBlock st;
    p->getStateInformation(st);
    std::printf("state: %d KB\n", (int) (st.getSize() / 1024));
    auto q = fm.createPluginInstance(*found[0], rate, block, err);
    if (! q) { std::printf("FAIL: second instance: %s\n", err.toRawUTF8()); return 1; }
    q->setStateInformation(st.getData(), (int) st.getSize());
    // stopped: silence after a moment
    ph.playing = false;
    juce::AudioBuffer<float> stop(2, block);
    double after = 0;
    for (int i = 0; i < (int) (rate * 3 / block); ++i) {
        stop.clear();
        p->processBlock(stop, midi);
        if (i > (int) (rate * 2 / block))
            after = std::max(after, rms(stop));
    }
    std::printf("stopped: rms %.5f\n", after);
    // the FM-1 as the controller, through the DAW (its port the DAW's): channel 16, the FM-1's first white key held a
    // second, stopped: heard (the same note on another channel is not a slice: silence)
    auto ctlRun = [&](int status) {
        double lv = 0;
        juce::MidiBuffer km;
        for (int i = 0; i < (int) (rate * 1.5 / block); ++i) {
            km.clear();
            if (i == 0) km.addEvent(juce::MidiMessage((int) status, 16, 127), 0);
            if (i == (int) (rate * 1.0 / block)) km.addEvent(juce::MidiMessage((int) (status - 0x10), 16, 0), 0);
            stop.clear();
            p->processBlock(stop, km);
            if (i > 4 && i < (int) (rate * 1.0 / block)) lv = std::max(lv, rms(stop));
        }
        return lv;
    };
    double ctlOn = 1, ctlOff = 1;
    if (ctlP != nullptr) {
        ctlOff = ctlRun(0x90);
        ctlOn = ctlRun(0x9F);
        std::printf("controller: channel 16 rms %.4f, channel 1 rms %.5f\n", ctlOn, ctlOff);
    }
    p->releaseResources();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> os(juce::File(juce::File::getCurrentWorkingDirectory().getChildFile(argv[2])).createOutputStream());
    if (os) {
        os->setPosition(0);
        os->truncate();
        std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(os.get(), rate, 2, 16, {}, 0));
        if (w) { os.release(); w->writeFromAudioSampleBuffer(all, 0, all.getNumSamples()); }
    }
    if (ctlP != nullptr) { ctlP->setValue("fm1ctl", false); ctlP->saveIfNeeded(); }
    const bool ok = level > 0.01 && bigLevel > 0.01 && after < 0.01 && ctlOn > 0.005 && ctlOff < 0.005;
    std::puts(ok ? "OK" : "FAIL: no sound while the host plays, sound after it stopped, or the controller's key not heard");
    return ok ? 0 : 1;
}
