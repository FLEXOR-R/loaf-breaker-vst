// LOAF BREAKER VST: one firmware core, its own copy of the core library (lbcore.dll / liblbcore.so) loaded from a
// temporary file: each instance of the plugin has its own (the firmware keeps its state in globals). Not thread-safe:
// the processor calls it under its lock.
#pragma once
#include <juce_core/juce_core.h>
#include <cstdint>
#include "lbcore.h"

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
    void (*tempo)(int detents) = nullptr;
    void (*seq)(lb_seq_t* q) = nullptr;
    int (*step)(int op, int a, int b, int c, int d) = nullptr;
    void (*patterns)(uint8_t* out) = nullptr;
    int (*scan)(lb_part_t* out, int max) = nullptr;
    int (*options)(int part, int knob, lb_opts_t* o) = nullptr;
    int (*set)(int part, int knob, int steps) = nullptr;
    void (*master)(int v) = nullptr;
    uint32_t (*screen)(uint16_t* rgb565) = nullptr;
    uint32_t (*leds)(uint8_t* out, uint32_t n) = nullptr;
    void (*lights)(uint32_t* o) = nullptr;
    void (*turnHw)(int role, int detents) = nullptr;
    int (*playing)() = nullptr;
    uint32_t (*flashSize)() = nullptr;
    uint32_t (*flashGet)(uint8_t* out, uint32_t n) = nullptr;
    int (*flashSet)(const uint8_t* in, uint32_t n) = nullptr;
    void (*save)() = nullptr;
    void (*sync)(int on) = nullptr;
    void (*bank)(const uint8_t* p, uint32_t n) = nullptr;
    int (*breaks)() = nullptr;
    int (*get)(int what) = nullptr;
    void (*model)(lb_model_t* m) = nullptr;
    uint32_t (*wave)(float* cols, int n, float* slices, int* nslices) = nullptr;
    int (*keyIndex)(int black, int i) = nullptr;
    uint32_t (*fm1Export)(uint8_t* out, uint32_t cap, char* report, uint32_t rcap) = nullptr;
    int (*fm1Import)(const uint8_t* img, uint32_t n, char* report, uint32_t rcap) = nullptr;
    int (*fm1List)(const uint8_t* img, uint32_t n, lb_list_t* o) = nullptr;
    uint32_t (*fm1Put)(const uint8_t* fm1, uint32_t n, const lb_pick_t* p, int np, uint8_t* out, uint32_t cap, char* rep, uint32_t rcap) = nullptr;
    uint32_t (*kickSize)() = nullptr;
    int (*kickRead)(const uint8_t* img, uint32_t n, int k, uint8_t* blob) = nullptr;
    uint32_t (*kickRender)(const uint8_t* blob, float* lr, uint32_t frames) = nullptr;
    uint32_t (*fm1PutKicks)(const uint8_t* fm1, uint32_t n, const uint8_t* blobs, const uint8_t* to, int nk, uint8_t* out, uint32_t cap) = nullptr;
    int (*vstPutKicks)(const uint8_t* blobs, const uint8_t* to, int nk) = nullptr;
    int (*breakName)(int i, char* out, int cap) = nullptr;
    void (*breakGo)(int i) = nullptr;
    void (*track)(int t, lb_track_t* o) = nullptr;
    void (*trackWrite)(const lb_track_t* t) = nullptr;
    void (*trackPlay)(int op, int t) = nullptr;
    uint32_t (*picSig)(int what) = nullptr;
    int (*pic)(int what, int n, int ms, float* out) = nullptr;
    int (*waveMM)(float* mm, int n) = nullptr;
    int (*fxList)(char (*names)[16], int max, int* cur) = nullptr;
    void (*drumHit)(int d) = nullptr;
    int (*fm1Take)(const uint8_t* fm1, uint32_t n, const lb_pick_t* p, int np, char* rep, uint32_t rcap) = nullptr;
    // the break bank beside the library (breaks.bank: every break, 44.1 kHz 16-bit), read once, shared by every instance
    static const juce::MemoryBlock& bankFile();
    static const std::map<juce::String, std::pair<juce::String, juce::String>>& breakIndex();

private:
    std::unique_ptr<juce::DynamicLibrary> lib;
    juce::File copy;
};
