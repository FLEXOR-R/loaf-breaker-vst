#include "Exchange.h"
#include "Ui.h"

static int count(int k) { return k == 0 ? 20 : k == 1 ? 64 : 8; }
static const char* PFX[3] = { "K", "P", "T" };

ExchangeView::ExchangeView(LoafBreakerProcessor& p) : proc(p)
{
    setSize(1420, 720);
    libRoot().createDirectory();
    libSel = libRoot();
    refreshVst();
    scanLib();
}

juce::File ExchangeView::libRoot()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("LOAF BREAKER").getChildFile("Kicks");
}

void ExchangeView::scanLib()
{
    lib.clear();
    std::function<void(const juce::File&, int)> walk = [&](const juce::File& d, int depth) {
        auto dirs = d.findChildFiles(juce::File::findDirectories, false);
        auto files = d.findChildFiles(juce::File::findFiles, false, "*.lbkick");
        std::sort(dirs.begin(), dirs.end(), [](const juce::File& a, const juce::File& b) { return a.getFileName().compareIgnoreCase(b.getFileName()) < 0; });
        std::sort(files.begin(), files.end(), [](const juce::File& a, const juce::File& b) { return a.getFileName().compareIgnoreCase(b.getFileName()) < 0; });
        for (auto& f : files) lib.push_back({ f, false, depth });
        for (auto& s : dirs) { lib.push_back({ s, true, depth }); walk(s, depth + 1); }
    };
    walk(libRoot(), 0);
    libTop = juce::jlimit(0, juce::jmax(0, (int) lib.size() - libRows()), libTop);
    repaint();
}

void ExchangeView::refreshVst()
{
    proc.listFm1(nullptr, vst);
    repaint();
}

// the columns: the VST's, the library, the FM-1's; the actions under
static constexpr float CW = 450.0f, TOP = 92.0f;
static float colX(int c) { return 10.0f + c * (CW + 30.0f); }   // 0 VST, 1 library, 2 FM-1
static float sideX(int side) { return colX(side ? 2 : 0); }
juce::Rectangle<float> ExchangeView::kickCell(int side, int i) const
{
    return { sideX(side) + 10.0f + (i / 10) * 216.0f, TOP + 30.0f + (i % 10) * 22.0f, 210.0f, 20.0f };
}
juce::Rectangle<float> ExchangeView::patCell(int side, int i) const
{
    return { sideX(side) + 10.0f + (i % 16) * 26.8f, TOP + 284.0f + (i / 16) * 25.0f, 25.0f, 22.0f };
}
juce::Rectangle<float> ExchangeView::trkCell(int side, int i) const
{
    return { sideX(side) + 10.0f + (i % 4) * 107.0f, TOP + 416.0f + (i / 4) * 27.0f, 103.0f, 24.0f };
}
juce::Rectangle<float> ExchangeView::libRow(int i) const { return { colX(1) + 10.0f, TOP + 30.0f + i * 22.0f, CW - 20.0f, 20.0f }; }
juce::Rectangle<float> ExchangeView::btn(int i) const
{
    switch (i) {
    case 0: return { colX(2), 14.0f, 220.0f, 32.0f };            // read the FM-1
    case 1: return { colX(2) + 230.0f, 14.0f, 220.0f, 32.0f };   // open a .bak
    case 2: return { colX(1) + 10.0f, TOP + 452.0f, 140.0f, 24.0f };   // new folder
    case 3: return { colX(1) + 160.0f, TOP + 452.0f, 140.0f, 24.0f };  // open the folder
    case 4: return { colX(1) + 310.0f, TOP + 452.0f, 130.0f, 24.0f };  // untick
    case 5: return { 10.0f, 600.0f, 300.0f, 44.0f };   // VST (+ library) -> FM-1
    case 6: return { 320.0f, 600.0f, 300.0f, 44.0f };  // FM-1 (+ library) -> VST
    case 7: return { 630.0f, 600.0f, 260.0f, 44.0f };  // kicks ticked -> library
    case 8: return { 900.0f, 600.0f, 250.0f, 44.0f };  // write onto the FM-1 (or save the .bak)
    default: return { 1160.0f + (i - 9) * 86.0f, 600.0f, 82.0f, 44.0f };   // 9..11 where they land K P T
    }
}

void ExchangeView::paint(juce::Graphics& g)
{
    const auto& sk = ui::cur();
    g.fillAll(sk.bg);
    g.setColour(sk.ink);
    g.setFont(ui::labelBold(22.0f));
    g.drawText("GESTION: VST, BIBLIOTHEQUE, FM-1", 14, 10, 600, 30, juce::Justification::centredLeft);
    g.setColour(sk.dim);
    g.setFont(ui::label(12.0f));
    g.drawText("> ecoute, un clic coche. Ce qui est coche part; le reste de l'autre cote ne bouge pas.", 14, 40, 700, 18, juce::Justification::centredLeft);
    // the columns
    for (int side = 0; side < 2; ++side) {
        const auto& L = side ? fm1 : vst;
        const bool has = side == 0 || haveFm1;
        juce::Rectangle<float> pr(sideX(side), TOP - 26.0f, CW, 508.0f);
        ui::panel(g, pr, side ? (haveFm1 ? (fm1Changed ? "FM-1  (MODIFIE, PAS ENCORE ECRIT)" : fm1FromFile ? "FM-1  (SAUVEGARDE)" : "FM-1  (LU EN USB)") : "FM-1  (PAS ENCORE LU)") : "LA VST");
        if (! has) {
            g.setColour(sk.dim);
            g.setFont(ui::label(14.0f));
            g.drawText("Lis le FM-1 ou ouvre une sauvegarde.", pr, juce::Justification::centred);
            continue;
        }
        g.setColour(sk.dim);
        g.setFont(ui::label(11.5f));
        g.drawText("KICKS", (int) sideX(side) + 12, (int) TOP + 8, 200, 18, juce::Justification::centredLeft);
        g.drawText("PATTERNS (CLAIR: DES NOTES)", (int) sideX(side) + 12, (int) TOP + 262, 300, 18, juce::Justification::centredLeft);
        g.drawText("TRACKS (ELEMENTS)", (int) sideX(side) + 12, (int) TOP + 394, 300, 18, juce::Justification::centredLeft);
        for (int i = 0; i < 20; ++i) {
            auto r = kickCell(side, i);
            const bool t = tick[side][0][i];
            g.setColour(t ? sk.accent : r.contains(mouse) ? sk.panel2.brighter(0.08f) : sk.panel2);
            g.fillRect(r);
            ui::button(g, hearBtn(r), ">", false, hearBtn(r).contains(mouse), 12.0f);
            g.setColour(t ? sk.bg : sk.ink);
            g.setFont(ui::value(14.0f));
            g.drawText("K" + juce::String(i + 1), r.withTrimmedLeft(26.0f).withWidth(34.0f), juce::Justification::centredLeft);
            g.setFont(ui::label(11.5f));
            g.drawText(juce::String(L.kick[i]) + (L.kick_live == i ? "  (EN JEU)" : ""), r.withTrimmedLeft(60.0f), juce::Justification::centredLeft);
        }
        for (int i = 0; i < 64; ++i) {
            auto r = patCell(side, i);
            const bool t = tick[side][1][i];
            const int st = L.pat[i];
            g.setColour(t ? sk.accent : st == 2 ? sk.line.brighter(0.25f) : sk.panel2);
            g.fillRect(r);
            g.setColour(r.contains(mouse) ? sk.hot : sk.line);
            g.drawRect(r, 1.0f);
            g.setColour(t ? sk.bg : st == 2 ? sk.ink : sk.dim);
            g.setFont(ui::value(11.5f));
            g.drawText(juce::String(i + 1), r, juce::Justification::centred);
        }
        for (int i = 0; i < 8; ++i) {
            auto r = trkCell(side, i);
            const bool t = tick[side][2][i];
            g.setColour(t ? sk.accent : r.contains(mouse) ? sk.panel2.brighter(0.08f) : sk.panel2);
            g.fillRect(r);
            g.setColour(t ? sk.bg : L.trk[i] ? sk.ink : sk.dim);
            g.setFont(ui::label(12.0f));
            g.drawText("TRACK " + juce::String(i + 1) + (L.trk[i] ? "  (" + juce::String(L.trk[i]) + ")" : "  -"), r, juce::Justification::centred);
        }
    }
    // the library
    {
        juce::Rectangle<float> pr(colX(1), TOP - 26.0f, CW, 508.0f);
        ui::panel(g, pr, "BIBLIOTHEQUE DE KICKS (PC)  -  " + juce::String((int) std::count_if(lib.begin(), lib.end(), [](const Entry& e) { return ! e.folder; })) + " KICKS");
        g.setColour(sk.dim);
        g.setFont(ui::label(11.0f));
        g.drawText("Dossier des nouveaux: " + (libSel == libRoot() ? juce::String("(racine)") : libSel.getRelativePathFrom(libRoot())) +
                   "   -   double-clic: renommer, clic droit: ranger", (int) colX(1) + 12, (int) TOP + 8, (int) CW - 20, 18, juce::Justification::centredLeft);
        if (lib.empty()) {
            g.setColour(sk.dim);
            g.setFont(ui::label(13.0f));
            g.drawText("Vide: coche des kicks (VST ou FM-1), puis GARDER DANS LA BIBLIOTHEQUE.", pr.reduced(20.0f), juce::Justification::centred);
        }
        for (int i = 0; i < libRows() && libTop + i < (int) lib.size(); ++i) {
            const auto& e = lib[(size_t) (libTop + i)];
            auto r = libRow(i).withTrimmedLeft(e.depth * 16.0f);
            if (e.folder) {
                g.setColour(e.file == libSel ? sk.accent : sk.dim);
                g.setFont(ui::labelBold(13.0f));
                g.drawText("[ " + e.file.getFileName() + " ]", r, juce::Justification::centredLeft);
                continue;
            }
            const bool t = libTick.count(e.file.getFullPathName()) > 0;
            g.setColour(t ? sk.accent : r.contains(mouse) ? sk.panel2.brighter(0.08f) : sk.panel2);
            g.fillRect(r);
            ui::button(g, hearBtn(r), ">", false, hearBtn(r).contains(mouse), 12.0f);
            g.setColour(t ? sk.bg : sk.ink);
            g.setFont(ui::label(12.5f));
            g.drawText(e.file.getFileNameWithoutExtension(), r.withTrimmedLeft(30.0f), juce::Justification::centredLeft);
        }
        if ((int) lib.size() > libRows()) {
            g.setColour(sk.dim);
            g.drawText("(molette: " + juce::String(libTop + 1) + "-" + juce::String(juce::jmin((int) lib.size(), libTop + libRows())) + " sur " + juce::String((int) lib.size()) + ")",
                       (int) colX(1) + 12, (int) TOP + 430, 300, 18, juce::Justification::centredLeft);
        }
    }
    static const char* B[9] = { "LIRE LE FM-1 (USB)", "OUVRIR UNE SAUVEGARDE .BAK", "NOUVEAU DOSSIER", "OUVRIR LE DOSSIER", "TOUT DECOCHER",
                                "VST + BIBLIO  >>  FM-1", "FM-1 + BIBLIO  >>  VST", "KICKS COCHES  >>  BIBLIOTHEQUE", "" };
    for (int i = 0; i < 12; ++i) {
        juce::String t = i < 9 ? B[i] : juce::String();
        if (i == 8) t = fm1FromFile ? "ENREGISTRER LE .BAK" : "ECRIRE SUR LE FM-1";
        if (i >= 9) { const int k = i - 9; t = dest[k] < 0 ? juce::String(PFX[k]) + ": MEMES" : juce::String(PFX[k]) + ": DES " + PFX[k] + juce::String(dest[k] + 1); }
        const bool off = (i == 5 || i == 6 || i == 8) && ! haveFm1;
        ui::button(g, btn(i), t, i == 8 && fm1Changed, ! off && btn(i).contains(mouse), i >= 5 && i <= 8 ? 13.5f : 11.0f);
        if (off) { g.setColour(sk.bg.withAlpha(0.6f)); g.fillRect(btn(i)); }
    }
    g.setColour(sk.dim);
    g.setFont(ui::label(10.5f));
    g.drawText("OU ILS ARRIVENT", 1160, 582, 250, 16, juce::Justification::centredLeft);
    g.setColour(sk.panel);
    g.fillRect(10.0f, 660.0f, (float) getWidth() - 20.0f, 48.0f);
    g.setColour(sk.ink);
    ui::fitText(g, status, { 18.0f, 660.0f, (float) getWidth() - 36.0f, 48.0f }, ui::label(13.0f), juce::Justification::centredLeft);
}

void ExchangeView::mouseMove(const juce::MouseEvent& e) { mouse = e.position; repaint(); }
void ExchangeView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.position.x < colX(1) || e.position.x > colX(1) + CW) return;
    libTop = juce::jlimit(0, juce::jmax(0, (int) lib.size() - libRows()), libTop + (w.deltaY < 0 ? 3 : -3));
    repaint();
}

void ExchangeView::mouseDown(const juce::MouseEvent& e)
{
    const auto p = e.position;
    if (btn(0).contains(p)) { readFm1(); return; }
    if (btn(1).contains(p)) { openBak(); return; }
    if (btn(2).contains(p)) {
        auto* w = new juce::AlertWindow("Nouveau dossier", "Son nom (dans " + (libSel == libRoot() ? juce::String("la bibliotheque") : libSel.getFileName()) + ")", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor("n", "");
        w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
        w->addButton("Annuler", 0);
        juce::Component::SafePointer<ExchangeView> self(this);
        w->enterModalState(true, juce::ModalCallbackFunction::create([self, w](int r) {
            if (self && r) { auto d = self->libSel.getChildFile(juce::File::createLegalFileName(w->getTextEditorContents("n"))); d.createDirectory(); self->libSel = d; self->scanLib(); }
        }), true);
        return;
    }
    if (btn(3).contains(p)) { libSel.startAsProcess(); return; }
    if (btn(4).contains(p)) { std::memset(tick, 0, sizeof tick); libTick.clear(); repaint(); return; }
    if (btn(5).contains(p) && haveFm1) { sendToFm1(); return; }
    if (btn(6).contains(p) && haveFm1) { takeToVst(); return; }
    if (btn(7).contains(p)) { keepInLib(); return; }
    if (btn(8).contains(p) && haveFm1) { writeFm1(); return; }
    for (int k = 0; k < 3; ++k)
        if (btn(9 + k).contains(p)) {
            juce::PopupMenu m;
            m.addItem(1, "Memes numeros", true, dest[k] < 0);
            m.addSeparator();
            for (int i = 0; i < count(k); ++i)
                m.addItem(2 + i, juce::String("A partir de ") + PFX[k] + juce::String(i + 1), true, dest[k] == i);
            juce::Component::SafePointer<ExchangeView> self(this);
            m.showMenuAsync(juce::PopupMenu::Options(), [self, k](int r) { if (self && r) { self->dest[k] = r - 2; self->repaint(); } });
            return;
        }
    for (int side = 0; side < 2; ++side) {
        if (side == 1 && ! haveFm1) continue;
        for (int i = 0; i < 20; ++i)
            if (kickCell(side, i).contains(p)) {
                if (hearBtn(kickCell(side, i)).contains(p)) {   // heard: as it is there
                    auto b = proc.kickRead(side ? &fm1Img : nullptr, i);
                    if (b.getSize()) { proc.kickHear(b); say("Ecoute: " + juce::String(side ? "FM-1" : "VST") + " K" + juce::String(i + 1) + " " + juce::String((side ? fm1 : vst).kick[i])); }
                } else
                    tick[side][0][i] = ! tick[side][0][i];
                repaint();
                return;
            }
        for (int i = 0; i < 64; ++i) if (patCell(side, i).contains(p)) { tick[side][1][i] = ! tick[side][1][i]; repaint(); return; }
        for (int i = 0; i < 8; ++i) if (trkCell(side, i).contains(p)) { tick[side][2][i] = ! tick[side][2][i]; repaint(); return; }
    }
    for (int i = 0; i < libRows() && libTop + i < (int) lib.size(); ++i) {
        const auto& en = lib[(size_t) (libTop + i)];
        auto r = libRow(i).withTrimmedLeft(en.depth * 16.0f);
        if (! r.contains(p)) continue;
        if (e.mods.isPopupMenu()) { libMenu(en); return; }
        if (en.folder) { libSel = libSel == en.file ? libRoot() : en.file; repaint(); return; }
        if (hearBtn(r).contains(p)) {
            juce::MemoryBlock b;
            if (en.file.loadFileAsData(b)) { proc.kickHear(b); say("Ecoute: " + en.file.getFileNameWithoutExtension()); }
            return;
        }
        const auto k = en.file.getFullPathName();
        if (libTick.count(k)) libTick.erase(k); else libTick.insert(k);
        repaint();
        return;
    }
}

void ExchangeView::mouseDoubleClick(const juce::MouseEvent& e)
{
    for (int i = 0; i < libRows() && libTop + i < (int) lib.size(); ++i) {
        const auto en = lib[(size_t) (libTop + i)];
        if (! libRow(i).withTrimmedLeft(en.depth * 16.0f).contains(e.position) || hearBtn(libRow(i)).contains(e.position)) continue;
        auto* w = new juce::AlertWindow("Renommer", en.file.getFileNameWithoutExtension(), juce::MessageBoxIconType::NoIcon);
        w->addTextEditor("n", en.file.getFileNameWithoutExtension());
        w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
        w->addButton("Annuler", 0);
        juce::Component::SafePointer<ExchangeView> self(this);
        w->enterModalState(true, juce::ModalCallbackFunction::create([self, w, en](int r) {
            if (! self || ! r) return;
            const auto n = juce::File::createLegalFileName(w->getTextEditorContents("n").trim());
            if (n.isEmpty()) return;
            en.file.moveFileTo(en.file.getSiblingFile(n + (en.folder ? "" : ".lbkick")));
            self->libTick.clear();
            self->scanLib();
        }), true);
        return;
    }
}

void ExchangeView::libMenu(const Entry& en)
{
    juce::PopupMenu m, to;
    m.addItem(1, "Renommer...");
    std::vector<juce::File> dirs { libRoot() };
    for (auto& x : lib) if (x.folder && x.file != en.file) dirs.push_back(x.file);
    for (size_t i = 0; i < dirs.size(); ++i) to.addItem(100 + (int) i, i == 0 ? juce::String("(racine)") : dirs[i].getRelativePathFrom(libRoot()));
    if (! en.folder) m.addSubMenu("Ranger dans", to);
    m.addItem(2, en.folder ? "Supprimer le dossier (vide)" : "Supprimer");
    juce::Component::SafePointer<ExchangeView> self(this);
    m.showMenuAsync(juce::PopupMenu::Options(), [self, en, dirs](int r) {
        if (! self || ! r) return;
        if (r == 1) {
            auto* w = new juce::AlertWindow("Renommer", en.file.getFileNameWithoutExtension(), juce::MessageBoxIconType::NoIcon);
            w->addTextEditor("n", en.file.getFileNameWithoutExtension());
            w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
            w->addButton("Annuler", 0);
            w->enterModalState(true, juce::ModalCallbackFunction::create([self, w, en](int ok) {
                if (! self || ! ok) return;
                const auto n = juce::File::createLegalFileName(w->getTextEditorContents("n").trim());
                if (n.isNotEmpty()) en.file.moveFileTo(en.file.getSiblingFile(n + (en.folder ? "" : ".lbkick")));
                self->scanLib();
            }), true);
        } else if (r == 2) {
            if (en.folder) { if (! en.file.deleteFile()) self->say("Le dossier n'est pas vide."); }
            else en.file.moveToTrash() || en.file.deleteFile();
            self->libTick.erase(en.file.getFullPathName());
            self->scanLib();
        } else if (r >= 100) {
            const auto d = dirs[(size_t) (r - 100)];
            en.file.moveFileTo(d.getChildFile(en.file.getFileName()));
            self->libTick.clear();
            self->scanLib();
        }
    });
}

std::vector<lb_pick_t> ExchangeView::picks(int side) const
{
    std::vector<lb_pick_t> v;
    for (int k = 0; k < 3; ++k) {
        int at = dest[k];
        for (int i = 0; i < count(k); ++i) {
            if (! tick[side][k][i]) continue;
            const int to = at < 0 ? i : at++;
            if (to >= count(k)) break;
            v.push_back({ (uint8_t) k, (uint8_t) i, (uint8_t) to, 0 });
        }
    }
    return v;
}

std::vector<juce::MemoryBlock> ExchangeView::libBlobs() const
{
    std::vector<juce::MemoryBlock> v;
    for (auto& e : lib) {
        if (e.folder || ! libTick.count(e.file.getFullPathName())) continue;
        juce::MemoryBlock b;
        if (e.file.loadFileAsData(b)) v.push_back(b);
    }
    return v;
}

// the library's kicks ticked land after the kicks ticked on the other side (from "K: DES Kn", or K1)
static std::vector<uint8_t> libTargets(int n, int start)
{
    std::vector<uint8_t> t;
    for (int i = 0; i < n && start + i < 20; ++i) t.push_back((uint8_t) (start + i));
    return t;
}

void ExchangeView::readFm1()
{
    if (proc.link.busy()) return;
    juce::Component::SafePointer<ExchangeView> self(this);
    say("Lecture du FM-1 (il doit etre a l'arret)...");
    proc.link.fetch([self](int pct, const juce::String& t) { if (self) self->say(pct >= 0 && pct < 100 ? t + " " + juce::String(pct) + "%" : t); },
                    [self](bool ok, const juce::String& t, const juce::MemoryBlock& img) {
                        if (! self) return;
                        if (! ok) { self->say(t); return; }
                        self->fm1Img = img;
                        self->haveFm1 = self->proc.listFm1(&self->fm1Img, self->fm1);
                        self->fm1FromFile = self->fm1Changed = false;
                        self->say(self->haveFm1 ? "FM-1 lu. Ecoute ses kicks (>), coche, puis choisis ou ca va." : "Sauvegarde illisible.");
                    });
}

void ExchangeView::openBak()
{
    chooser = std::make_unique<juce::FileChooser>("Une sauvegarde du FM-1", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.bak");
    juce::Component::SafePointer<ExchangeView> self(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [self](const juce::FileChooser& fc) {
        auto f = fc.getResult();
        if (! self || f == juce::File()) return;
        juce::MemoryBlock img;
        if (! f.loadFileAsData(img)) { self->say("Lecture impossible."); return; }
        self->fm1Img = img;
        self->haveFm1 = self->proc.listFm1(&self->fm1Img, self->fm1);
        self->fm1FromFile = true;
        self->fm1Changed = false;
        self->say(self->haveFm1 ? "Sauvegarde ouverte: " + f.getFileName() : "Ce n'est pas une sauvegarde LOAF BREAKER.");
    });
}

void ExchangeView::sendToFm1()        // the VST's ticked and the library's ticked into the FM-1's backup (then ECRIRE)
{
    const auto v = picks(0);
    const auto blobs = libBlobs();
    if (v.empty() && blobs.empty()) { say("Coche d'abord dans la colonne VST ou la bibliotheque."); return; }
    int n = 0;
    if (! v.empty()) {
        juce::MemoryBlock out;
        juce::String rep;
        if (! proc.putFm1(fm1Img, v, out, rep)) { say("Echec: " + rep); return; }
        fm1Img = out;
        n += (int) v.size();
    }
    if (! blobs.empty()) {
        int start = dest[0] < 0 ? 0 : dest[0];
        for (auto& p : v) if (p.kind == LB_PICK_KICK) start = juce::jmax(start, (int) p.to + 1);
        const auto to = libTargets((int) blobs.size(), start);
        std::vector<juce::MemoryBlock> b(blobs.begin(), blobs.begin() + (long) to.size());
        if (! b.empty() && ! proc.putKicksFm1(fm1Img, b, to)) { say("Echec (bibliotheque)."); return; }
        n += (int) b.size();
    }
    proc.listFm1(&fm1Img, fm1);
    fm1Changed = true;
    say(juce::String(n) + " element(s) mis dans le FM-1 (colonne de droite). Ecoute pour verifier, puis " +
        (fm1FromFile ? "ENREGISTRER LE .BAK." : "ECRIRE SUR LE FM-1."));
}

void ExchangeView::writeFm1()
{
    juce::Component::SafePointer<ExchangeView> self(this);
    if (fm1FromFile) {
        chooser = std::make_unique<juce::FileChooser>("Enregistrer la sauvegarde",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("LOAF BREAKER.bak"), "*.bak");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles, [self](const juce::FileChooser& fc) {
            auto f = fc.getResult();
            if (self && f != juce::File()) {
                const bool ok = f.replaceWithData(self->fm1Img.getData(), self->fm1Img.getSize());
                if (ok) self->fm1Changed = false;
                self->say(ok ? "Enregistree: " + f.getFileName() : "Ecriture impossible.");
            }
        });
        return;
    }
    if (proc.link.busy()) return;
    say("Ecriture sur le FM-1: confirme sur la machine (SEL: oui)...");
    proc.link.send(fm1Img, [self](int pct, const juce::String& t) { if (self) self->say(pct >= 0 && pct < 100 ? t + " " + juce::String(pct) + "%" : t); },
                   [self](bool ok, const juce::String& t, const juce::MemoryBlock&) {
                       if (! self) return;
                       if (ok) self->fm1Changed = false;
                       self->say(ok ? "Ecrit sur le FM-1: il redemarre avec. " + t : t);
                   });
}

void ExchangeView::takeToVst()        // the FM-1's ticked and the library's ticked into the VST
{
    const auto v = picks(1);
    const auto blobs = libBlobs();
    if (v.empty() && blobs.empty()) { say("Coche d'abord dans la colonne FM-1 ou la bibliotheque."); return; }
    juce::String rep;
    if (! v.empty() && ! proc.takeFm1(fm1Img, v, rep)) { say("Echec: " + rep); return; }
    int n = (int) v.size();
    if (! blobs.empty()) {
        int start = dest[0] < 0 ? 0 : dest[0];
        for (auto& p : v) if (p.kind == LB_PICK_KICK) start = juce::jmax(start, (int) p.to + 1);
        const auto to = libTargets((int) blobs.size(), start);
        std::vector<juce::MemoryBlock> b(blobs.begin(), blobs.begin() + (long) to.size());
        if (! b.empty() && ! proc.putKicksVst(b, to)) { say("Echec (bibliotheque)."); return; }
        n += (int) b.size();
    }
    say(juce::String(n) + " element(s) mis dans la VST.");
    juce::Component::SafePointer<ExchangeView> self(this);
    juce::Timer::callAfterDelay(700, [self] { if (self) self->refreshVst(); });
}

void ExchangeView::keepInLib()         // the kicks ticked (VST, FM-1) as library files, named after them
{
    int n = 0;
    for (int side = 0; side < 2; ++side) {
        if (side == 1 && ! haveFm1) continue;
        for (int i = 0; i < 20; ++i) {
            if (! tick[side][0][i]) continue;
            auto b = proc.kickRead(side ? &fm1Img : nullptr, i);
            if (! b.getSize()) continue;
            const auto& L = side ? fm1 : vst;
            const juce::String base = juce::String(L.kick[i]) + " (" + (side ? "FM-1" : "VST") + " K" + juce::String(i + 1) + ")";
            auto f = libSel.getNonexistentChildFile(juce::File::createLegalFileName(base), ".lbkick", true);
            if (f.replaceWithData(b.getData(), b.getSize())) n++;
        }
    }
    scanLib();
    say(n ? juce::String(n) + " kick(s) gardes dans la bibliotheque" + (libSel == libRoot() ? juce::String(".") : " (" + libSel.getFileName() + ").") + " Double-clic pour les renommer."
          : juce::String("Coche d'abord des kicks (VST ou FM-1)."));
}
