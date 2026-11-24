// LOAF BREAKER VST: a host's check, run by the CI: the VST3 loaded as a DAW does, 48 kHz, 512-frame blocks, the host's
// transport playing at 170 BPM from bar 1: LOAF BREAKER must follow (playing, sound), its state must reload, then the
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
    auto t0 = juce::Time::getMillisecondCounterHiRes();
    auto p = fm.createPluginInstance(*found[0], rate, block, err);
    if (! p) { std::printf("FAIL: %s\n", err.toRawUTF8()); return 1; }
    std::printf("loaded in %.0f ms\n", juce::Time::getMillisecondCounterHiRes() - t0);
    PlayHead ph;
    ph.rate = rate;
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
    p->releaseResources();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> os(juce::File(juce::File::getCurrentWorkingDirectory().getChildFile(argv[2])).createOutputStream());
    if (os) {
        os->setPosition(0);
        os->truncate();
        std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(os.get(), rate, 2, 16, {}, 0));
        if (w) { os.release(); w->writeFromAudioSampleBuffer(all, 0, all.getNumSamples()); }
    }
    const bool ok = level > 0.01 && after < 0.01;
    std::puts(ok ? "OK" : "FAIL: no sound while the host plays, or sound after it stopped");
    return ok ? 0 : 1;
}
