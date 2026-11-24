// LOAF BREAKER VST: one firmware core, its own copy of the core library (lbcore.dll / liblbcore.so) loaded from a
// temporary file: each instance of the plugin has its own (the firmware keeps its state in globals). Not thread-safe:
// the processor calls it under its lock.
#pragma once
#include <juce_core/juce_core.h>
#include <cstdint>

class Core
{
public:
    Core() = default;
    ~Core();
    bool load(juce::String& error);      // a fresh copy of the library, not yet booted
    bool loaded() const { return lib != nullptr; }
    void unload();

    int (*abi)() = nullptr;
    void (*init)(uint32_t ms) = nullptr;
    void (*ms)(int frame) = nullptr;
    void (*audio)(float* lr, uint32_t frames) = nullptr;
    int (*midiIn)(uint32_t pkt) = nullptr;
    int (*midiOut)(uint32_t* pkt) = nullptr;
    void (*button)(int b, int down) = nullptr;
    void (*key)(int k, int down, int vel) = nullptr;
    void (*turn)(int role, int detents) = nullptr;
    void (*master)(int v) = nullptr;
    uint32_t (*screen)(uint16_t* rgb565) = nullptr;
    uint32_t (*leds)(uint8_t* out, uint32_t n) = nullptr;
    int (*playing)() = nullptr;
    uint32_t (*flashSize)() = nullptr;
    uint32_t (*flashGet)(uint8_t* out, uint32_t n) = nullptr;
    int (*flashSet)(const uint8_t* in, uint32_t n) = nullptr;
    void (*save)() = nullptr;
    void (*sync)(int on) = nullptr;

private:
    std::unique_ptr<juce::DynamicLibrary> lib;
    juce::File copy;
};
