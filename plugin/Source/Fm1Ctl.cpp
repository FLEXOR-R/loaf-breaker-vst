#include "Fm1Ctl.h"

namespace {
bool isFm1(const juce::String& n) { return n.containsIgnoreCase("FM-1") || n.containsIgnoreCase("LOAF BREAKER"); }
}

void Fm1Ctl::start()
{
    if (! isThreadRunning())
        startThread(juce::Thread::Priority::high);
}

void Fm1Ctl::stop()
{
    if (! isThreadRunning())
        return;
    signalThreadShouldExit();
    stopThread(2000);
    if (out != nullptr)                  // (off at once, not in 3 s)
        out->sendMessageNow(juce::MidiMessage::controllerEvent(16, 127, 0));
    closePorts();
}

bool Fm1Ctl::popIn(uint32_t& msg)
{
    const auto s = inF.read(1);
    if (s.blockSize1 > 0) { msg = inB[s.startIndex1]; return true; }
    if (s.blockSize2 > 0) { msg = inB[s.startIndex2]; return true; }
    return false;
}

void Fm1Ctl::pushOut(uint32_t msg)
{
    if (! outOpen.load())
        return;
    const auto s = outF.write(1);
    if (s.blockSize1 > 0) outB[s.startIndex1] = msg;
    else if (s.blockSize2 > 0) outB[s.startIndex2] = msg;
}

void Fm1Ctl::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& m)
{
    const auto* d = m.getRawData();
    if (m.getRawDataSize() < 3 || (d[0] & 0x0F) != 0x0F || d[0] < 0x80 || d[0] >= 0xF0)
        return;                          // (channel 16's notes and CCs only: the clock and the rest are the DAW's)
    const auto s = inF.write(1);
    const uint32_t v = (uint32_t) d[0] | (uint32_t) d[1] << 8 | (uint32_t) d[2] << 16;
    if (s.blockSize1 > 0) inB[s.startIndex1] = v;
    else if (s.blockSize2 > 0) inB[s.startIndex2] = v;
}

void Fm1Ctl::openPorts()
{
    if (in == nullptr)
        for (auto& dv : juce::MidiInput::getAvailableDevices())
            if (isFm1(dv.name)) {
                in = juce::MidiInput::openDevice(dv.identifier, this);
                if (in != nullptr) { in->start(); inOpen = true; }
                break;
            }
    if (out == nullptr)
        for (auto& dv : juce::MidiOutput::getAvailableDevices())
            if (isFm1(dv.name)) {
                out = juce::MidiOutput::openDevice(dv.identifier);
                outOpen = out != nullptr;
                break;
            }
}

void Fm1Ctl::closePorts()
{
    inOpen = outOpen = false;
    if (in != nullptr) { in->stop(); in.reset(); }
    out.reset();
}

void Fm1Ctl::run()
{
    double lastLook = 0.0;
    while (! threadShouldExit()) {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (paused && paused()) {        // (a backup or a restore: its ports)
            closePorts();
            wait(50);
            continue;
        }
        if ((in == nullptr || out == nullptr) && now - lastLook > 1000.0) {   // (plugged in later, or freed)
            lastLook = now;
            openPorts();
        }
        if (out != nullptr) {
            for (;;) {
                const auto s = outF.read(1);
                uint32_t v;
                if (s.blockSize1 > 0) v = outB[s.startIndex1];
                else if (s.blockSize2 > 0) v = outB[s.startIndex2];
                else break;
                out->sendMessageNow(juce::MidiMessage((int) (v & 0xFF), (int) (v >> 8 & 0x7F), (int) (v >> 16 & 0x7F)));
            }
        }
        wait(20);                        // (woken by flush() as soon as there is something to send)
    }
}
