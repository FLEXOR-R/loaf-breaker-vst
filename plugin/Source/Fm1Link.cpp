#include "Fm1Link.h"

namespace {
const uint8_t RS_REQ = 0x41, BU_REQ = 0x42, BU_END = 0x43;
const uint32_t OK_ADDR = 0xC0000000u, ASK_ADDR = 0xC1000000u, FAIL_ADDR = 0xCF000000u;
const uint8_t MAGIC[4] = { 'L', 'B', 'R', 'K' };   // app.h APP_BACKUP_MAGIC

std::vector<uint8_t> pack7(const std::vector<uint8_t>& d)
{
    std::vector<uint8_t> o;
    uint32_t acc = 0, nb = 0;
    for (auto b : d) {
        acc |= (uint32_t) b << nb;
        nb += 8;
        while (nb >= 7) { o.push_back((uint8_t) (acc & 0x7F)); acc >>= 7; nb -= 7; }
    }
    if (nb) o.push_back((uint8_t) (acc & 0x7F));
    return o;
}
std::vector<uint8_t> unpack7(const uint8_t* s, size_t n)
{
    std::vector<uint8_t> o;
    uint32_t acc = 0, nb = 0;
    for (size_t i = 0; i < n; ++i) {
        acc |= (uint32_t) (s[i] & 0x7F) << nb;
        nb += 7;
        while (nb >= 8) { o.push_back((uint8_t) (acc & 0xFF)); acc >>= 8; nb -= 8; }
    }
    return o;
}
uint32_t crc32(const uint8_t* p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *p++;
        for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}
uint32_t rd32(const uint8_t* p) { return (uint32_t) p[0] | (uint32_t) p[1] << 8 | (uint32_t) p[2] << 16 | (uint32_t) p[3] << 24; }
juce::String restoreError(uint32_t code)
{
    switch (code) {
        case 10: return "Le FM-1 joue : arrete-le (PLAY), puis recommence.";
        case 11: return "Refuse sur le FM-1 (HOME) : rien n'a change.";
        case 3:  return "Le FM-1 n'a pas la meme version de LOAF BREAKER (mets-le a jour).";
        case 4: case 5: case 6: case 2: return "Transfert abime : rien n'a change, recommence.";
        case 7: case 8: case 9: return "Le FM-1 n'a pas pu ecrire sa memoire : rien n'a change.";
        default: return "Echec (code " + juce::String((int) code) + ") : rien n'a change.";
    }
}
} // namespace

juce::String Fm1Link::portName()
{
    for (auto& d : juce::MidiOutput::getAvailableDevices())
        if (d.name.containsIgnoreCase("FM-1") || d.name.containsIgnoreCase("LOAF BREAKER"))
            return d.name;
    return {};
}

void Fm1Link::fetch(Progress p, Done d)
{
    if (busy()) return;
    sending = false;
    onProgress = std::move(p);
    onDone = std::move(d);
    startThread();
}

void Fm1Link::send(const juce::MemoryBlock& img, Progress p, Done d)
{
    if (busy()) return;
    sending = true;
    image = img;
    onProgress = std::move(p);
    onDone = std::move(d);
    startThread();
}

bool Fm1Link::open(juce::String& why)
{
    juce::MidiDeviceInfo oi, ii;
    for (auto& d : juce::MidiOutput::getAvailableDevices())
        if (d.name.containsIgnoreCase("FM-1") || d.name.containsIgnoreCase("LOAF BREAKER")) { oi = d; break; }
    for (auto& d : juce::MidiInput::getAvailableDevices())
        if (d.name.containsIgnoreCase("FM-1") || d.name.containsIgnoreCase("LOAF BREAKER")) { ii = d; break; }
    if (oi.identifier.isEmpty() || ii.identifier.isEmpty()) {
        why = "FM-1 introuvable : branche-le en USB (LOAF BREAKER allume).";
        return false;
    }
    out = juce::MidiOutput::openDevice(oi.identifier);
    in = juce::MidiInput::openDevice(ii.identifier, this);
    if (! out || ! in) {
        close();
        why = "Le port du FM-1 est deja pris (Ableton ?) : desactive le FM-1 dans les preferences MIDI d'Ableton "
              "le temps du transfert, ou utilise la version autonome.";
        return false;
    }
    in->start();
    return true;
}

void Fm1Link::close()
{
    if (in) in->stop();
    in.reset();
    out.reset();
    const juce::ScopedLock sl(qLock);
    queue.clear();
}

void Fm1Link::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& m)
{
    if (! m.isSysEx()) return;
    const juce::ScopedLock sl(qLock);
    queue.emplace_back(m.getSysExData(), m.getSysExData() + m.getSysExDataSize());
    got.signal();
}

bool Fm1Link::nextSysex(std::vector<uint8_t>& o, int ms)
{
    const auto until = juce::Time::getMillisecondCounter() + (uint32_t) ms;
    for (;;) {
        {
            const juce::ScopedLock sl(qLock);
            if (! queue.empty()) { o = std::move(queue.front()); queue.pop_front(); return true; }
        }
        const auto now = juce::Time::getMillisecondCounter();
        if (now >= until || threadShouldExit()) return false;
        got.wait((int) (until - now));
    }
}

void Fm1Link::sendFrame(uint8_t cmd, const std::vector<uint8_t>& body)
{
    std::vector<uint8_t> msg { 0x00, 0x59, cmd, (uint8_t) body.size(), (uint8_t) (body.size() >> 8), 0 };
    uint32_t sum = 0;
    for (auto b : body) { msg.push_back(b); sum += b; }
    msg.push_back((uint8_t) (~sum & 0xFF));
    auto p = pack7(msg);
    out->sendMessageNow(juce::MidiMessage::createSysExMessage(p.data(), (int) p.size()));
}

void Fm1Link::sendResponse(uint32_t addr, const uint8_t* data, size_t n, uint8_t fl)
{
    std::vector<uint8_t> body { 0x00, 0x59, 0x30, (uint8_t) ((n + 8) & 0xFF), (uint8_t) (((n + 8) >> 8) & 0xFF), 0, fl,
                                (uint8_t) addr, (uint8_t) (addr >> 8), (uint8_t) (addr >> 16), (uint8_t) (addr >> 24),
                                (uint8_t) (n & 0xFF), (uint8_t) ((n >> 8) & 0xFF), 0 };
    body.insert(body.end(), data, data + n);
    uint32_t sum = 0;
    for (size_t i = 6; i < body.size(); ++i) sum += body[i];
    body.push_back((uint8_t) (~sum & 0xFF));
    auto p = pack7(body);
    out->sendMessageNow(juce::MidiMessage::createSysExMessage(p.data(), (int) p.size()));
}

void Fm1Link::progress(int pct, const juce::String& t)
{
    auto cb = onProgress;
    juce::MessageManager::callAsync([cb, pct, t] { if (cb) cb(pct, t); });
}

void Fm1Link::finish(bool ok, const juce::String& t, const juce::MemoryBlock& img)
{
    close();
    auto cb = onDone;
    juce::MessageManager::callAsync([cb, ok, t, img] { if (cb) cb(ok, t, img); });
}

void Fm1Link::run()
{
    juce::String why;
    if (! open(why)) { finish(false, why); return; }
    if (sending) doSend(); else doFetch();
}

void Fm1Link::doFetch()
{
    progress(0, "Lecture du FM-1...");
    sendFrame(BU_REQ, { MAGIC[0], MAGIC[1], MAGIC[2], MAGIC[3] });
    std::vector<uint8_t> img, s;
    bool first = true;
    for (;;) {
        if (! nextSysex(s, first ? 3000 : 10000)) {
            finish(false, first ? "Le FM-1 ne repond pas : il faut LOAF BREAKER avec la sauvegarde USB (mets-le a jour)."
                                : "Le FM-1 a arrete d'envoyer : recommence.");
            return;
        }
        auto u = unpack7(s.data(), s.size());
        if (u.size() < 7 || u[0] != 0x00 || u[1] != 0x59) continue;
        const uint32_t n = u[3] | u[4] << 8 | u[5] << 16;
        if (n + 7 != u.size()) continue;
        uint32_t sum = 0;
        for (uint32_t i = 0; i < n; ++i) sum += u[6 + i];
        if ((uint8_t) (~sum & 0xFF) != u[6 + n]) continue;
        const uint8_t cmd = u[2];
        const uint8_t* body = u.data() + 6;
        if (cmd == BU_REQ && n >= 4) {
            first = false;
            if (rd32(body) != img.size()) { finish(false, "Un morceau s'est perdu en route : recommence."); return; }
            img.insert(img.end(), body + 4, body + n);
            progress(juce::jmin(99, (int) (img.size() / 800)), "Lecture du FM-1... " + juce::String((int) img.size() / 1024) + " Ko");
        } else if (cmd == BU_END && n >= 10) {
            const uint32_t len = rd32(body), crc = rd32(body + 4);
            if (body[8] != 0) { finish(false, "Le FM-1 n'a pas pu lire sa memoire."); return; }
            if (len != img.size() || crc != crc32(img.data(), img.size())) { finish(false, "Arrive abime : recommence."); return; }
            finish(true, juce::String(body[9] & 1 ? "Lu (le FM-1 jouait : ses changements pas encore sauves n'y sont pas)."
                                                  : "Lu."), juce::MemoryBlock(img.data(), img.size()));
            return;
        }
    }
}

void Fm1Link::doSend()
{
    const auto* img = static_cast<const uint8_t*>(image.getData());
    const uint32_t n = (uint32_t) image.getSize(), crc = crc32(img, n);
    progress(0, "Envoi au FM-1...");
    sendFrame(RS_REQ, { (uint8_t) n, (uint8_t) (n >> 8), (uint8_t) (n >> 16), (uint8_t) (n >> 24),
                        (uint8_t) crc, (uint8_t) (crc >> 8), (uint8_t) (crc >> 16), (uint8_t) (crc >> 24) });
    std::vector<uint8_t> s;
    int idle = 3000;
    bool asked = false;
    for (;;) {
        if (! nextSysex(s, idle)) {
            finish(false, asked ? "Le FM-1 a arrete de demander : rien n'a change."
                                : "Le FM-1 ne repond pas : il faut LOAF BREAKER avec la sauvegarde USB (mets-le a jour).");
            return;
        }
        auto u = unpack7(s.data(), s.size());
        if (u.size() != 15 || u[0] != 0x00 || u[1] != 0x59 || u[2] != 0x30) continue;
        uint32_t sum = 0;
        for (int i = 6; i < 14; ++i) sum += u[i];
        if ((uint8_t) (~sum & 0xFF) != u[14]) continue;
        const uint8_t fl = u[6];
        const uint32_t addr = rd32(u.data() + 7), len = u[11] | u[12] << 8 | u[13] << 16;
        asked = true;
        idle = 10000;
        const uint32_t top = addr & 0xFF000000u;
        if (top == OK_ADDR || top == FAIL_ADDR || top == ASK_ADDR) {
            static const uint8_t ok[8] = { 's', 'u', 'c', 'c', 'e', 's', 's', 0 };
            sendResponse(addr, ok, 8, fl);
            if (top == ASK_ADDR) {
                progress(100, "Verifie. Sur le FM-1 : RESTORE FROM USB ? SEL = oui, HOME = non.");
                idle = 75000;
                continue;
            }
            if (top == OK_ADDR) finish(true, "Envoye : le FM-1 redemarre avec.");
            else finish(false, restoreError((addr & 0xFFFF) ? (addr & 0xFFFF) : 99));
            return;
        }
        if (len > 512 || addr >= n) { finish(false, "Le FM-1 a demande un morceau hors du fichier."); return; }
        std::vector<uint8_t> d(len, 0);
        std::memcpy(d.data(), img + addr, juce::jmin<size_t>(len, n - addr));
        sendResponse(addr, d.data(), len, fl);
        progress((int) (juce::jmin<uint64_t>(addr + len, n) * 99 / n), "Envoi au FM-1...");
    }
}
