#include "Core.h"
#include <mutex>

static juce::String libName()
{
#if JUCE_WINDOWS
    return "lbcore.dll";
#elif JUCE_MAC
    return "liblbcore.dylib";
#else
    return "liblbcore.so";
#endif
}

// the library beside the plugin's binary (a VST3 bundle's Contents/<arch>/, the Standalone's folder), or one up
static juce::File findLibrary()
{
    auto here = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    for (auto dir : { here, here.getParentDirectory(), here.getChildFile("Resources"),
                      here.getParentDirectory().getChildFile("Resources") })
        if (dir.getChildFile(libName()).existsAsFile())
            return dir.getChildFile(libName());
    return {};
}

Core::~Core() { unload(); }

const juce::MemoryBlock& Core::bankFile()
{
    static juce::MemoryBlock data;
    static std::once_flag once;
    std::call_once(once, [] {
        auto lib = findLibrary();
        for (auto dir : { lib.getParentDirectory(), lib.getParentDirectory().getParentDirectory().getChildFile("Resources") })
            if (dir.getChildFile("breaks.bank").existsAsFile()) {
                dir.getChildFile("breaks.bank").loadFileAsData(data);
                break;
            }
    });
    return data;
}

void Core::unload()
{
    lib.reset();
    if (copy.existsAsFile())
        copy.deleteFile();
    copy = juce::File();
}

bool Core::load(juce::String& error)
{
    unload();
    auto src = findLibrary();
    if (! src.existsAsFile()) {
        error = "LOAF BREAKER: " + libName() + " not found beside the plugin";
        return false;
    }
    static std::atomic<int> count { 0 };
    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("LoafBreakerCores");
    dir.createDirectory();
    // (old copies a crash left: removed when nothing holds them)
    for (auto& f : dir.findChildFiles(juce::File::findFiles, false))
        if (f.getLastModificationTime() < juce::Time::getCurrentTime() - juce::RelativeTime::days(1))
            f.deleteFile();
    copy = dir.getChildFile(juce::String(juce::Time::currentTimeMillis()) + "-" + juce::String(++count) + "-" + libName());
    if (! src.copyFileTo(copy)) {
        error = "LOAF BREAKER: cannot copy " + src.getFullPathName();
        return false;
    }
    lib = std::make_unique<juce::DynamicLibrary>();
    if (! lib->open(copy.getFullPathName())) {
        error = "LOAF BREAKER: cannot load " + copy.getFullPathName();
        lib.reset();
        return false;
    }
#define LB_FN(field, name) field = reinterpret_cast<decltype(field)>(lib->getFunction(name)); \
    if (field == nullptr) { error = "LOAF BREAKER: " name " missing in the core"; unload(); return false; }
    LB_FN(abi, "lb_abi") LB_FN(init, "lb_init") LB_FN(ms, "lb_ms") LB_FN(audio, "lb_audio")
    LB_FN(midiIn, "lb_midi_in") LB_FN(midiOut, "lb_midi_out") LB_FN(button, "lb_button") LB_FN(key, "lb_key")
    LB_FN(turn, "lb_turn") LB_FN(master, "lb_master") LB_FN(screen, "lb_screen") LB_FN(leds, "lb_leds")
    LB_FN(playing, "lb_playing") LB_FN(flashSize, "lb_flash_size") LB_FN(flashGet, "lb_flash_get")
    LB_FN(flashSet, "lb_flash_set") LB_FN(save, "lb_save") LB_FN(sync, "lb_sync") LB_FN(bank, "lb_bank")
    LB_FN(breaks, "lb_breaks") LB_FN(fm1Export, "lb_fm1_export") LB_FN(fm1Import, "lb_fm1_import")
#undef LB_FN
    if (abi() != 2) {
        error = "LOAF BREAKER: the core's version does not match the plugin's";
        unload();
        return false;
    }
    return true;
}
