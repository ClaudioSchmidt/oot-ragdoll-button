// oot-ragdoll, part of main.cpp (RD_MENU): the settings page in HSML's shared menu, built from HSML's menu kit
// (hsml/sdk/hsml_kit.hpp, namespace hsk), so it looks and behaves as HSML's own pages at every Menu size and window
// width: tabs (ZL / ZR), rows of label | control | reset (one label column per tab; the control goes under its label
// when the page is narrow), a footer with the unsaved count, Reset <tab> and Reset all (each asks first), Advanced and
// Debug tab, and the help box with the text of the row under the pointer or the pad's focus.
// Most rows come from the SETTINGS table (config.inc): on the tab and under the heading the table names. The voice
// lists (play / remove / add), their picker and the force level's pickers are this mod's own, laid out with the kit's
// measures. The Controls tab is the kit's binding table.
// A changed value works at once. The settings file gets it with Save (in HSML's head): until then its label is in the
// kit's unsaved colour (hsk::WARN) and the footer counts the unsaved changes (config.inc: g_saved*).
// Registered in hookshot/hs_rt.cpp (RdMenuPage); hookshot/menu_preview.cpp: offline pictures in a window of its own.
// Included outside main.cpp's namespace: RdBuildMenu and the Rd* helpers are what the host calls.
#include "hsml_kit.hpp"

namespace {

int g_menuTab = T_START;     // the tab shown
int g_menuGoTab = -1;        // select this tab in the next frame (ZL / ZR, preview)
const char* g_help;          // help text of the row under the mouse / pad focus (hsk::Tip writes it; the help box, a frame later)
int g_resetTabAsk, g_resetAllAsk;   // Reset <tab> / Reset all: asking (hsk::Confirm's state)

constexpr const char* BUTTON_LABELS[] = {"none", "A", "B", "X", "Y", "L3 (left stick)", "R3 (right stick)", "L", "R", "ZL", "ZR",
    "Plus", "Minus", "D-pad left", "D-pad up", "D-pad right", "D-pad down"};
static_assert(sizeof(BUTTON_LABELS) / sizeof(BUTTON_LABELS[0]) == sizeof(BUTTONS) / sizeof(BUTTONS[0]));
// a mask as the menu shows it: "R", "ZL + R3", "none"
const char* MaskLabel(u64 m) {
    static const char* const SHORT[16] = {"A", "B", "X", "Y", "L3", "R3", "L", "R", "ZL", "ZR", "Plus", "Minus", "D-pad left", "D-pad up", "D-pad right", "D-pad down"};
    static char ring[4][48]; static unsigned at;
    char* o = ring[at++ % 4];
    int len = 0; o[0] = 0;
    for (int k = 0; k < 16; k++) if (m & (1ull << k)) len += snprintf(o + len, sizeof ring[0] - len, len ? " + %s" : "%s", SHORT[k]);
    return len ? o : "none";
}
const char* const* const TRIG_LABELS = hsk::TRIGGER_NAMES;   // Tap, Double tap, Hold (Trig's order)
constexpr const char* FORCE_LABELS[5] = {"Player", "Light", "Medium", "Strong", "Mighty"};

// Link's voices for the lists of the Voices tab. Names starting with "Voice" are his voice animations (sound + open
// mouth; the 116 names of the game's own table, FINDINGS.md "Link's voice"); the others are plain sounds (SLink keys).
// Any other name can be typed in the picker.
// 1.1.0 (user): the picker in sections by what the sound is for (what fits a ragdoll first), each null-terminated
struct VoiceSection { const char* title; const char* const* names; };
constexpr const char* VS_PAIN[] = {"VoiceDamageS", "VoiceDamageM", "VoiceDamageL", "VoiceDamageDanger", "VoiceDamageLand",
    "VoiceWallHit", "VoiceDamagePress", "VoiceDamageStand", "VoiceDamageFlail", "VoiceDamageFlailSt", "VoiceDamageBurning",
    "VoiceDamageElectric", "VoiceDamageFreeze", "VoiceDamageUnderwater", "VoiceDamageKidnapped", "VoiceGuardHit", "VoiceGuardBreak",
    "VoiceWaitCritical", nullptr};
constexpr const char* VS_FALL[] = {"VoiceFallDanger", "VoiceJumpNearCliff", "VoiceCliffSt", "VoiceWallOff", "VoiceFallingIntoWarter", nullptr};
constexpr const char* VS_DEATH[] = {"VoiceDeath", "VoiceDeathLava", "VoiceDeathSand", "VoiceDeathUnderwater", nullptr};
constexpr const char* VS_ATTACK[] = {"VoiceAttackS", "VoiceAttackM", "VoiceAttackL", "VoiceLSwordAttackS", "VoiceLSwordAttackM",
    "VoiceLSwordAttackL", "VoiceHammerAttackHorizontal", "VoiceHammerAttackVertical", "VoiceCutJumpSt", "VoiceLSwordCutJumpSt",
    "VoiceHammerCutJumpSt", "VoiceChargeSt", "VoiceChargeAttack", "VoiceChargeAttackFull", "VoiceServiceSwordSpinFirst",
    "VoiceServiceSwordSpinSecoud", "VoiceServiceSwordSpinEnd", "VoiceShieldGuard", "VoiceSwordEmpty", nullptr};
constexpr const char* VS_MOVE[] = {"VoiceJump", "VoiceBackFlip", "VoiceSideStep", "VoiceRolling", "VoiceSlide", "VoiceClimbSt",
    "VoiceClimbJump", "VoiceClimbEd", "VoiceHandClimb", "VoicePushPull", "VoicePushPullNormal", "VoiceTunnelSt", "VoiceTunnel",
    "VoiceTunnelEd", "VoiceWaterDiveJump", "VoiceSwimRiseEd", "VoiceHookshotMove", nullptr};
constexpr const char* VS_LIFT[] = {"VoiceLiftUp", "VoiceLiftUpRockM", "VoiceLiftUpKeepRockM", "VoiceLiftReadyRockM", "VoiceHoldSt",
    "VoiceLiftThrow", "VoiceLiftThrowRockM", "VoiceLiftFail", "VoiceBombThrow", "VoiceBoomerangThrow", "VoiceBoomerangCatch",
    "VoiceDekuNutThrow", "VoiceBottleSwing", nullptr};
constexpr const char* VS_ITEM[] = {"VoiceItemEmpty", "VoiceBulletEmpty", "VoiceBootsChange", "VoiceDekuStickBurned", "VoiceDinSt",
    "VoiceDin", "VoiceFaroreSt", "VoiceFarore", "VoiceNayruSt", "VoiceNayru", nullptr};
constexpr const char* VS_HORSE[] = {"VoiceHorseRideon", "VoiceHorseMoveGoS", "VoiceHorseMoveGoL", "VoiceHorseMoveGoRand",
    "VoiceHorseMoveGoRandMuteS", "VoiceHorseMoveGoRandMuteL", "VoiceHorseJumpS", "VoiceHorseJumpM", "VoiceHorseJumpL",
    "VoiceHorseDismount", "VoiceHorseDismountJump", nullptr};
constexpr const char* VS_IDLE[] = {"VoiceServiceStretchOut", "VoiceServiceStretchRelease", "VoiceServiceLookAroundWide",
    "VoiceServicRetightBelt", "VoiceServicRetightBeltSecond", "VoiceServicTapFloor", "VoiceServicTapFloorSecond",
    "VoiceServiceColdShivering", "VoiceServiceColdSneeze", "VoiceServiceHot", "VoiceServiceRain", "VoiceServiceStorm", nullptr};
constexpr const char* VS_EVENT[] = {"VoiceEvtTurnAroundSurprise", "VoiceEvtTalkYes", "VoiceEvtStretch", "VoiceEvtStretchRelease",
    "VoiceEvtBeansPlant", "VoiceTreasureBoxFront", "VoiceTreasureBoxWithKick", nullptr};
constexpr const char* VK_PAIN[] = {"Damage_S_01", "Damage_S_02", "Damage_S_03", "Damage_M_01", "Damage_M_02", "Damage_M_03",
    "Damage_B_01", "Damage_B_02", "Damage_B_03", nullptr};
constexpr const char* VK_FALL[] = {"FallingScream_01", "FallingScream_02", nullptr};
constexpr const char* VK_JUMP[] = {"Jump_S_01", "Jump_S_02", "Jump_S_03", "Jump_S_04", nullptr};
constexpr const char* VK_GEAR[] = {"Guard_Player", "GuardBreak_Player", "EquipmentNoise_GuardBreak", "Equip", "UnEquip",
    "SwordCharge_Level1", "SwordCharge_Level2", "RotationCut_ChargeFull", nullptr};
constexpr VoiceSection VOICE_SECTIONS[] = {
    {"Pain and hits", VS_PAIN}, {"Screams and falls", VS_FALL}, {"Death", VS_DEATH}, {"Attacks and effort", VS_ATTACK},
    {"Jumps, climbing, swimming", VS_MOVE}, {"Lifting and throwing", VS_LIFT}, {"Items and magic", VS_ITEM}, {"Horse", VS_HORSE},
    {"Idle and weather", VS_IDLE}, {"Story events", VS_EVENT},
    {"Sound only: pain", VK_PAIN}, {"Sound only: falling scream", VK_FALL}, {"Sound only: jump", VK_JUMP},
    {"Sound only: shield and gear", VK_GEAR}};
constexpr int VoiceCount() {   // the game's 116 voice animations + 23 sound keys, each once
    int n = 0;
    for (auto& s : VOICE_SECTIONS) for (const char* const* p = s.names; *p; p++) n++;
    return n;
}
static_assert(VoiceCount() == 116 + 23);

// ---- measures (the kit's) ----
// a control's width in a row: the rest of the row without its reset button's room (the reset is drawn by the caller:
// several resets here need the exact default back, e.g. a value shown scaled)
float CtlW() { return hsk::Mx(hsk::Room() - hsk::ResetSpace(), 1.f); }
// a tab's label column (hsk::LabelColumn): the labels of its rows that show, plus `more`
hsk::Rows LabelsOf(Tab t, const char* const* more = nullptr, int nMore = 0) {
    const char* labels[96]; int n = 0;
    for (auto& s : SETTINGS) if (s.tab == t && !(s.adv && !g_cfg.advanced) && n < 96) labels[n++] = s.label;
    for (int i = 0; i < nMore && n < 96; i++) labels[n++] = more[i];
    return hsk::LabelColumn(labels, n);
}

// a setting's default as its row shows it
void DefaultText(const Setting& s, char* out, size_t cap) {
    const void* d = DefaultOf(s);
    out[0] = 0;
    switch (s.kind) {
        case K_BOOL: snprintf(out, cap, "%s", *static_cast<const bool*>(d) ? "on" : "off"); break;
        case K_FLOAT: snprintf(out, cap, s.unit, static_cast<double>(*static_cast<const float*>(d) * s.scale)); break;
        case K_INT: snprintf(out, cap, s.unit, static_cast<int>(*static_cast<const u32*>(d))); break;
        case K_CHOICE: {
            const char* c = s.unit;
            for (u32 i = *static_cast<const u32*>(d); i && *c; i--) c += strlen(c) + 1;
            snprintf(out, cap, "%s", c);
            break;
        }
        case K_BUTTON: snprintf(out, cap, "%s", MaskLabel(*static_cast<const u64*>(d))); break;
        case K_BIND: {
            const Bind& b = *static_cast<const Bind*>(d);
            snprintf(out, cap, "%s, %s, %.2f s", MaskLabel(b.mask), TRIG_LABELS[b.trig < 3 ? b.trig : 0], static_cast<double>(b.time));
            break;
        }
        case K_FORCE: snprintf(out, cap, "%s", FORCE_LABELS[*static_cast<const int*>(d)]); break;
        case K_KEY: snprintf(out, cap, "%s", KeyName(*static_cast<const int*>(d))); break;
        case K_TEXT: snprintf(out, cap, "%s", static_cast<const char*>(d)); break;
        case K_WEIGHTS: break;
    }
}
// the reset of the setting whose control was just drawn: greyed at the default, sets the exact default back
void ResetFor(const Setting& s) {
    char t[96]; DefaultText(s, t, sizeof t);
    if (hsk::Reset(IsDefault(s), t)) ResetSetting(s);
}

// one row from the table: label | control | reset (hsk::Row: the control under the label when the page is narrow)
void DrawSetting(const Setting& s, const hsk::Rows& rows) {
    ImGui::PushID(s.key);
    const char* names[8]; int nNames = 0;   // K_CHOICE: the choices (`unit`, each ending in \0)
    if (s.kind == K_CHOICE) for (const char* c = s.unit; *c && nNames < 8; c += strlen(c) + 1) names[nNames++] = c;
    float minW = 0.f;   // (0: the kit's least width for a control)
    if (s.kind == K_BOOL) minW = ImGui::GetFrameHeight() + hsk::ResetSpace();
    else if (s.kind == K_CHOICE) minW = hsk::ChoiceW(names, nNames) + hsk::ResetSpace();
    else if (s.kind == K_FORCE) minW = hsk::ChoiceW(FORCE_LABELS, 5) + hsk::ResetSpace();
    hsk::Row(rows, s.label, s.help, minW, !IsSaved(s));
    switch (s.kind) {
        case K_BOOL: hsk::Tick("##v", static_cast<bool*>(s.p), s.help); break;
        case K_FLOAT: {   // (shown = value * scale; the reset sets the stored default, not a shown value divided back)
            float* p = static_cast<float*>(s.p);
            float shown = *p * s.scale;
            if (hsk::SliderF("##v", &shown, s.lo, s.hi, s.unit, s.help, nullptr, 0, CtlW())) *p = shown / s.scale;
            break;
        }
        case K_INT: {
            int v = static_cast<int>(*static_cast<u32*>(s.p));
            if (hsk::SliderI("##v", &v, static_cast<int>(s.lo), static_cast<int>(s.hi), s.unit, s.help, nullptr, 0, CtlW())) *static_cast<u32*>(s.p) = static_cast<u32>(v < 0 ? 0 : v);
            break;
        }
        case K_CHOICE: {
            int v = static_cast<int>(*static_cast<u32*>(s.p));
            if (hsk::Choice("##v", &v, names, nNames, s.help, nullptr, CtlW())) *static_cast<u32*>(s.p) = static_cast<u32>(v);
            break;
        }
        case K_FORCE: hsk::Choice("##v", static_cast<int*>(s.p), FORCE_LABELS, 5, s.help, nullptr, CtlW()); break;
        case K_BUTTON: {   // (the Controls tab draws its own; this for a button row on another tab)
            u64* p = static_cast<u64*>(s.p);
            bool listening = g_bindTarget == p;
            hsk::Label(listening ? "press, let go ..." : MaskLabel(*p), listening ? hsk::WARN : 0);
            hsk::Tip(s.help);
            ImGui::SameLine();
            if (hsk::Button(listening ? "Cancel###press" : "Set###press")) { g_keyTarget = nullptr; g_bindStart = Ticks(); g_bindTarget = listening ? nullptr : p; }
            break;
        }
        case K_BIND: break;
        case K_KEY: {
            int* p = static_cast<int*>(s.p);
            ImGui::SetNextItemWidth(CtlW());
            bool open = ImGui::BeginCombo("##v", KeyName(*p));
            hsk::Probed(__LINE__, "menu.inc");
            if (open) {
                for (auto& k : KEYS) if (ImGui::Selectable(k.n, k.vk == *p)) *p = k.vk;
                ImGui::EndCombo();
            }
            hsk::Tip(s.help);
            break;
        }
        case K_TEXT:
            ImGui::SetNextItemWidth(CtlW());
            ImGui::InputText("##v", static_cast<char*>(s.p), s.cap);
            hsk::Probed(__LINE__, "menu.inc");
            hsk::Tip(s.help);
            break;
        case K_WEIGHTS: break;
    }
    ResetFor(s);
    ImGui::PopID();
}

const Setting* Find(const char* key) {
    for (auto& s : SETTINGS) if (!strcmp(s.key, key)) return &s;
    return nullptr;
}

// the table's rows of one tab, under their headings; advanced rows only with the tick. Text rows of the Voices tab
// are drawn by VoiceRows
void DrawRows(Tab t, const hsk::Rows& rows) {
    const char* heading = nullptr;
    for (auto& s : SETTINGS) {
        if (s.tab != t) continue;
        if (s.section) heading = s.section;
        if (s.adv && !g_cfg.advanced) continue;
        if (t == T_VOICE && s.kind == K_TEXT) continue;   // (VoiceRows)
        if (heading) { hsk::Section(heading); heading = nullptr; }
        DrawSetting(s, rows);
    }
}

bool InPlayNow() { return g_state == ACTIVE || Ticks() - g_standTick < TickHz() / 4; }

// ---- Voices ----
constexpr int VOICE_MAX = 8, VOICE_LEN = 48;   // as PlayFromList reads a list
int ListParse(const char* list, char items[VOICE_MAX][VOICE_LEN], bool keepNone = false) {
    int n = 0;
    for (const char* p = list; *p && n < VOICE_MAX;) {
        int len = 0;
        while (p[len] && p[len] != ',') len++;
        if (len > 0 && len < VOICE_LEN) {
            memcpy(items[n], p, static_cast<size_t>(len)); items[n][len] = 0;
            if (keepNone || strcmp(items[n], "none")) n++;
        }
        p += len; if (*p == ',') p++;
    }
    return n;
}
// false = the list does not fit its buffer (nothing written)
bool ListWrite(char* buf, int cap, char items[VOICE_MAX][VOICE_LEN], int n) {
    char t[VOICE_MAX * VOICE_LEN + 8]; int o = 0;
    for (int i = 0; i < n; i++) o += snprintf(t + o, sizeof t - static_cast<size_t>(o), i ? ",%s" : "%s", items[i]);
    if (!n) o = snprintf(t, sizeof t, "none");
    if (o + 1 > cap) return false;
    memcpy(buf, t, static_cast<size_t>(o) + 1);
    return true;
}
bool ContainsCi(const char* s, const char* q) {
    if (!q[0]) return true;
    size_t n = strlen(q);
    for (; *s; s++) {
        size_t i = 0;
        while (i < n && s[i] && (s[i] | 0x20) == (q[i] | 0x20)) i++;
        if (i == n) return true;
    }
    return false;
}
// a name without , # or blanks (they would break the list / the file's line)
void CleanName(const char* in, char* out, int cap) {
    int o = 0;
    for (; *in && o < cap - 1; in++) if (*in > ' ' && *in != ',' && *in != '#' && *in < 127) out[o++] = *in;
    out[o] = 0;
}
// the play button: the voice now, on Link (heard only while he is in play)
void PlayButton(const char* id, const char* name) {
    bool off = !name[0] || !strcmp(name, "none");
    if (off) ImGui::BeginDisabled();
    if (ImGui::ArrowButton(id, ImGuiDir_Right)) SayOne(name);
    hsk::Probed(__LINE__, "menu.inc");
    if (off) ImGui::EndDisabled();
    hsk::Tip(InPlayNow() ? "Play it now." : "Play it now. Link is not in play (title, game menu, cutscene): nothing will sound until he is.");
}
// the dropdown with every known voice, a filter, and a field for any other name. pick: the chosen name. preview: the
// closed box's text instead of pick; taken: names already on the list (ticked, not selectable)
bool VoicePicker(const char* id, char* pick, int cap, bool withNone, const char* preview = nullptr,
                 const char (*taken)[VOICE_LEN] = nullptr, int nTaken = 0) {
    static char filter[VOICE_LEN];
    bool changed = false;
    bool open = ImGui::BeginCombo(id, preview ? preview : pick[0] ? pick : "choose a voice ...", ImGuiComboFlags_HeightLarge);
    hsk::Probed(__LINE__, "menu.inc");
    if (!open) return false;
    if (ImGui::IsWindowAppearing()) filter[0] = 0;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##f", "filter, or type any name + Enter", filter, sizeof filter, ImGuiInputTextFlags_EnterReturnsTrue)) {
        char name[VOICE_LEN]; CleanName(filter, name, sizeof name);
        if (name[0]) { snprintf(pick, static_cast<size_t>(cap), "%s", name); changed = true; ImGui::CloseCurrentPopup(); }
    }
    float h = ImGui::GetFrameHeight(), in = ImGui::GetStyle().ItemInnerSpacing.x;
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
    auto item = [&](const char* name) {   // name | play (1.1.0: hear it while browsing)
        if (!ContainsCi(name, filter)) return;
        bool on = false;
        for (int i = 0; i < nTaken; i++) if (!strcmp(taken[i], name)) on = true;
        ImGui::PushID(name);
        float w = ImGui::GetContentRegionAvail().x - h - in;
        if (ImGui::Selectable(name, on || !strcmp(name, pick), on ? ImGuiSelectableFlags_Disabled : 0, ImVec2(w > 0 ? w : 1.f, h))) {
            snprintf(pick, static_cast<size_t>(cap), "%s", name); changed = true;
        }
        if (on) hsk::Tip("Already on this list.");
        ImGui::SameLine(0, in);
        PlayButton("##hear", name);
        ImGui::PopID();
    };
    if (withNone) item("none");
    // 1.1.0 (user): each section folds (skim the headings, open one, try its sounds). Closed when the dropdown opens,
    // except the one with the voice chosen now; while filtering, every section with a match is open. Only sections
    // with a match show, with their count
    bool appearing = ImGui::IsWindowAppearing();
    for (auto& sec : VOICE_SECTIONS) {
        int n = 0; bool hasPick = false;
        for (const char* const* p = sec.names; *p; p++) { n += ContainsCi(*p, filter); hasPick |= !strcmp(*p, pick); }
        if (!n) continue;
        char head[64]; snprintf(head, sizeof head, "%s (%d)###%s", sec.title, n, sec.title);
        if (filter[0]) ImGui::SetNextItemOpen(true);
        else if (appearing) ImGui::SetNextItemOpen(hasPick);
        if (!ImGui::CollapsingHeader(head)) continue;
        ImGui::Indent(in);
        for (const char* const* p = sec.names; *p; p++) item(*p);
        ImGui::Unindent(in);
    }
    ImGui::PopStyleVar();
    ImGui::EndCombo();
    return changed;
}
// one situation with a list of voices (played in turn). 1.1.0 (user): the dropdown on the label's line (with
// the list's reset), it adds the voice picked in it at once; below it a line per entry: name | play | X (play and X at
// the row's end; under the name when the row is too narrow for both)
void VoiceList(const Setting& s, const hsk::Rows& rows) {
    const ImGuiStyle& st = ImGui::GetStyle();
    char* buf = static_cast<char*>(s.p);
    char items[VOICE_MAX][VOICE_LEN];
    int n = ListParse(buf, items);
    ImGui::PushID(s.key);
    float x0 = ImGui::GetCursorPosX();
    bool below = hsk::Row(rows, s.label, s.help, 0.f, !IsSaved(s));
    float cx = below ? x0 : x0 + rows.labelW;   // the control's column: the entries go under the dropdown
    float h = ImGui::GetFrameHeight(), in = st.ItemInnerSpacing.x;
    // the dropdown | reset
    char pk[VOICE_LEN] = "";
    bool full = n >= VOICE_MAX;
    const char* why = full ? "A list takes 8 voices." : nullptr;
    ImGui::SetNextItemWidth(CtlW());
    if (full) ImGui::BeginDisabled();
    if (VoicePicker("##add", pk, VOICE_LEN, false, n ? "+ add a voice ..." : "silent: + add a voice ...", items, n) && pk[0] && !full) {
        bool has = false;
        for (int i = 0; i < n; i++) if (!strcmp(items[i], pk)) has = true;
        if (!has) {
            snprintf(items[n], VOICE_LEN, "%s", pk);
            if (ListWrite(buf, s.cap, items, n + 1)) n++;
            else why = "The list is full (its line in the settings file has a fixed length).";
        }
    }
    if (full) ImGui::EndDisabled();
    hsk::Tip(why ? why : "Pick a voice: it goes on this list at once. The arrow next to a voice plays it.");
    ResetFor(s);
    // the entries
    int drop = -1;
    for (int i = 0; i < n; i++) {
        ImGui::PushID(i);
        ImGui::SetCursorPosX(cx);
        hsk::Label(items[i]);
        hsk::Tip(s.help);
        hsk::SameLineRight(h * 2 + in);   // play | X at the row's end
        PlayButton("##play", items[i]);
        ImGui::SameLine(0, in);
        if (hsk::Button("X##rm", "Remove it from this list.", h)) drop = i;
        ImGui::PopID();
    }
    if (drop >= 0) {
        for (int i = drop; i < n - 1; i++) memcpy(items[i], items[i + 1], VOICE_LEN);
        ListWrite(buf, s.cap, items, n - 1);
    }
    ImGui::Dummy(ImVec2(0, ImGui::GetFrameHeight() * 0.6f));   // a gap before the next list (user)
    ImGui::PopID();
}
// the force change sounds: one voice per force level (the list's entries by position)
void ForceVoices(const Setting& s, const hsk::Rows& rows) {
    const ImGuiStyle& st = ImGui::GetStyle();
    char* buf = static_cast<char*>(s.p);
    char items[VOICE_MAX][VOICE_LEN];
    int n = ListParse(buf, items, true);
    for (; n < 5; n++) snprintf(items[n], VOICE_LEN, "none");
    char defs[VOICE_MAX][VOICE_LEN];
    int nd = ListParse(static_cast<const char*>(DefaultOf(s)), defs, true);
    char saved[VOICE_MAX][VOICE_LEN];
    int ns = ListParse(static_cast<const char*>(SavedOf(s)), saved, true);
    ImGui::PushID(s.key);
    for (int i = 0; i < 5; i++) {
        ImGui::PushID(i);
        hsk::Row(rows, FORCE_LABELS[i], s.help, 0.f, !(i < ns && !strcmp(saved[i], items[i])));
        ImGui::SetNextItemWidth(hsk::Mx(CtlW() - ImGui::GetFrameHeight() - st.ItemInnerSpacing.x, 1.f));   // (and the play button)
        bool ch = VoicePicker("##pick", items[i], VOICE_LEN, true);
        hsk::Tip(s.help);
        ImGui::SameLine(0, st.ItemInnerSpacing.x);
        PlayButton("##try", items[i]);
        const char* d = i < nd ? defs[i] : "none";
        if (hsk::Reset(!strcmp(items[i], d), d)) { snprintf(items[i], VOICE_LEN, "%s", d); ch = true; }
        if (ch) ListWrite(buf, s.cap, items, 5);
        ImGui::PopID();
    }
    ImGui::PopID();
}
void VoiceRows(const hsk::Rows& rows) {
    // 1.1.0 (user): press and kicks per force level; one level shown at a time, at first the force level now
    static int level = -1, seenForce = -1;
    if (g_cfg.force != seenForce) { seenForce = g_cfg.force; level = g_cfg.force; }
    if (level < 0 || level > 4) level = 2;
    // (user 2026-11-26) no text lines on the page: short section heads, what they said in the heads' tooltips
    const char* hLists = "Each situation plays the voices of its list in turn, one each time; an empty list is silent. Names starting with Voice open his mouth, the others are sound only.";
    hsk::Section("Button and falling");
    hsk::Tip(hLists);
    {
        const char* hL = "Which force level's lists show below. It starts at the force level you play with now.";
        char now[5][24];
        const char* names[5];
        for (int i = 0; i < 5; i++) { snprintf(now[i], sizeof now[i], i == g_cfg.force ? "%s (now)" : "%s", FORCE_LABELS[i]); names[i] = now[i]; }
        hsk::Row(rows, "Force level", hL, hsk::ChoiceW(names, 5) + hsk::ResetSpace());
        hsk::Choice("##level", &level, names, 5, hL, nullptr, CtlW());
        static const char* const PER[] = {"voicepress_", "voicefall_"};
        for (auto k : PER) {
            char key[32]; snprintf(key, sizeof key, "%s%s", k, FORCES[level]);
            if (const Setting* s = Find(key)) VoiceList(*s, rows);
        }
    }
    hsk::Section("Landings");
    hsk::Tip("Landings and bumps, by how hard he hits.");
    static const char* const LISTS[] = {"voicehard", "voicelight", "voiceslide"};
    for (auto k : LISTS) if (const Setting* s = Find(k)) VoiceList(*s, rows);
    hsk::Section("Getting up");
    hsk::Tip(hLists);
    if (const Setting* s = Find("voicegetup")) VoiceList(*s, rows);
    if (!g_cfg.advanced) return;
    hsk::Section("Force level change");
    hsk::Tip("The force level changed (settings button twice).");
    if (const Setting* s = Find("forcevoice")) ForceVoices(*s, rows);
    hsk::Section("Button change");
    hsk::Tip("A new ragdoll button (settings button held 3 s).");
    if (const Setting* s = Find("listenvoice")) VoiceList(*s, rows);
    if (const Setting* s = Find("setvoice")) VoiceList(*s, rows);
}

// ---- the tabs ----
void TabStart() {
    const hsk::Rows rows = LabelsOf(T_START);
    if (g_state == ACTIVE) {
        char t[96]; snprintf(t, sizeof t, "Link: ragdolled, %.1f m/s, %s", static_cast<double>(g_bodySpeed), Grounded() ? "on the ground" : "in the air");
        hsk::Para(t);
    } else if (InPlayNow()) hsk::Para("Link: on his feet");
    else hsk::Hint("Link: not in play (title, game menu, cutscene, other state)");
    ImGui::Spacing();
    DrawRows(T_START, rows);
}
void TabVoices() {
    static const char* const MORE[6] = {"Force level", "Player", "Light", "Medium", "Strong", "Mighty"};   // (VoiceRows' own rows)
    const hsk::Rows rows = LabelsOf(T_VOICE, MORE, 6);
    DrawRows(T_VOICE, rows);
    VoiceRows(rows);
}

// ---- Controls (1.1.1, user: as HSML's and Immersive Camera's): the kit's binding table, one line per action: Buttons
// (one or two, Set records them), Trigger, Time, PC key; each with its reset. Under 48 letters of room: a block per
// action, each part on its own line with its head in grey ----
void KeyLabel(int vk, char* out, size_t cap) {
    snprintf(out, cap, "%s", vk ? KeyName(vk) : "none");
    if (vk && out[0] >= 'a' && out[0] <= 'z') out[0] = static_cast<char>(out[0] - 32);
}
void TabControls() {
    struct Row { const char* bind; const char* key; };   // bind: a K_BIND row, or spin_button (K_BUTTON: held)
    static const Row ROWS[] = {{"ragdoll", "ragkey"}, {"getup", "getupkey"}, {"nextforce", "forcekey"}, {"spin_button", "spinkey"}, {"menu", "menukey"}};
    constexpr int NR = static_cast<int>(sizeof ROWS / sizeof ROWS[0]);
    const char* hB = "One button or two together. Set, then press the button(s) on the pad and let go. X: none.";
    const char* hT = "Tap: on press. Double tap: two presses within the time. Hold: held for the time.";
    const char* hS = "Double tap: most time between the two presses. Hold: how long to hold.";
#ifdef HOOKSHOT
    constexpr bool KEYS_TOO = true;
    const char* hK = "A PC key that does the same (game window in front). Set, then press the key; Esc = none. X: none.";
#else
    constexpr bool KEYS_TOO = false;   // (no keyboard)
    const char* hK = "";
#endif
    const char* const HEADS_HELP[5] = {"", hB, hT, hS, hK};
    // every name gets the widest name's room, so the Set buttons line up
    float nameW = ImGui::CalcTextSize("press, let go").x, keyW = ImGui::CalcTextSize("Backspace").x;
    for (auto& r : ROWS) {
        if (const Setting* s = Find(r.bind)) nameW = fmaxf(nameW, ImGui::CalcTextSize(MaskLabel(s->kind == K_BIND ? static_cast<Bind*>(s->p)->mask : *static_cast<u64*>(s->p))).x);
        char t[16];
        if (const Setting* s = Find(r.key)) { KeyLabel(*static_cast<int*>(s->p), t, sizeof t); keyW = fmaxf(keyW, ImGui::CalcTextSize(t).x); }
    }
    // (the actions' names: the kit draws blocks when the table's columns don't fit, HSML 2.1 Phase 5c)
    const char* acts[NR]; int nActs = 0;
    for (auto& r : ROWS) if (const Setting* s = Find(r.bind)) acts[nActs++] = s->label;
    hsk::Binds b;
    if (hsk::BeginBinds(b, "binds", KEYS_TOO, nameW, keyW, HEADS_HELP, acts, nActs, 0.f, TRIG_LABELS, 3)) {
        for (int i = 0; i < NR; i++) {
            const Setting* sb = Find(ROWS[i].bind);
            const Setting* sk = Find(ROWS[i].key);
            if (!sb || !sk) continue;
            bool isBind = sb->kind == K_BIND;
            Bind* bd = isBind ? static_cast<Bind*>(sb->p) : nullptr;
            const Bind* d = isBind ? static_cast<const Bind*>(DefaultOf(*sb)) : nullptr;
            u64* mask = isBind ? &bd->mask : static_cast<u64*>(sb->p);
            u64 dmask = isBind ? d->mask : *static_cast<const u64*>(DefaultOf(*sb));
            ImGui::PushID(i);
            // the action (in the unsaved colour while the file does not have its line)
            hsk::BindCell(b, 0);
            bool saved = IsSaved(*sb) && (!KEYS_TOO || IsSaved(*sk));
            hsk::BindAction(sb->label, sb->help, !saved);
            // buttons
            hsk::BindCell(b, 1);
            switch (hsk::BindSet(b, 1, MaskLabel(*mask), g_bindTarget == mask, "press, let go", *mask != 0, *mask == dmask, MaskLabel(dmask), hB)) {
                case hsk::BIND_SET: g_keyTarget = nullptr; g_bindStart = Ticks(); g_bindTarget = mask; break;   // input.inc CapFeed
                case hsk::BIND_CANCEL: g_bindTarget = nullptr; break;
                case hsk::BIND_CLEAR: *mask = 0; break;
                case hsk::BIND_RESET: *mask = dmask; break;
            }
            // trigger, time
            if (!isBind) {   // spin: held, with the left stick
                hsk::BindCell(b, 2); hsk::Label("Held", hsk::HintCol());
                hsk::Tip("While held, the left stick spins him in the air and rolls him on the ground. none: the stick always does.");
                hsk::BindCell(b, 3); hsk::Label("-", hsk::HintCol());
            } else {
                hsk::BindCell(b, 2);
                int tr = static_cast<int>(bd->trig < 3 ? bd->trig : 0);
                if (hsk::Choice("##t", &tr, TRIG_LABELS, 3, hT, nullptr, CtlW())) bd->trig = static_cast<u32>(tr);
                if (hsk::Reset(bd->trig == d->trig, TRIG_LABELS[d->trig])) bd->trig = d->trig;
                hsk::BindCell(b, 3);
                ImGui::BeginDisabled(bd->trig == TR_TAP);
                hsk::SliderF("##s", &bd->time, 0.2f, 2.f, "%.2f s", hS, nullptr, ImGuiSliderFlags_AlwaysClamp, fmaxf(CtlW(), ImGui::CalcTextSize("0.00 s").x * 2));
                ImGui::EndDisabled();
                char dt[16]; snprintf(dt, sizeof dt, "%.2f s", static_cast<double>(d->time));
                if (hsk::Reset(bd->time == d->time, dt)) bd->time = d->time;   // (pressable at Tap too, as before)
            }
#ifdef HOOKSHOT
            // the PC key
            hsk::BindCell(b, 4);
            int* k = static_cast<int*>(sk->p);
            char t[16]; KeyLabel(*k, t, sizeof t);
            char dk[96]; DefaultText(*sk, dk, sizeof dk);
            switch (hsk::BindSet(b, 4, t, g_keyTarget == k, "press a key", *k != 0, IsDefault(*sk), dk, hK)) {
                case hsk::BIND_SET: g_bindTarget = nullptr; g_keyTarget = k; break;   // the next key (RdMenuKeyEvent)
                case hsk::BIND_CANCEL: g_keyTarget = nullptr; break;
                case hsk::BIND_CLEAR: *k = 0; break;
                case hsk::BIND_RESET: ResetSetting(*sk); break;
            }
#endif
            ImGui::PopID();
        }
        hsk::EndBinds();
    }
}

void TabDebug() {
    const hsk::Rows rows = LabelsOf(T_DEBUG);
    DrawRows(T_DEBUG, rows);
    hsk::Section("Now");
    char t[160];
    snprintf(t, sizeof t, "state %s, physics frame %u, rest frames %u, %s", g_state == ACTIVE ? "ragdoll" : "idle", g_physFrames, g_restFrames,
             g_state == ACTIVE && Grounded() ? "grounded" : "not grounded");
    hsk::Para(t);
    snprintf(t, sizeof t, "hips %.1f %.1f %.1f at %.1f m/s", static_cast<double>(g_bodyPos[0]), static_cast<double>(g_bodyPos[1]), static_cast<double>(g_bodyPos[2]), static_cast<double>(g_bodySpeed));
    hsk::Para(t);
    snprintf(t, sizeof t, "buttons %#llx, stick %d %d", static_cast<unsigned long long>(g_held), static_cast<int>(g_stickX), static_cast<int>(g_stickY));
    hsk::Para(t);
    snprintf(t, sizeof t, "ragdoll-button %s, tune @ %p", MOD_VERSION, static_cast<void*>(&g_tune));
    hsk::Para(t);
}

void ResetTab(int t) {
    for (auto& s : SETTINGS) if (s.tab == t) ResetSetting(s);
    if (t == T_START) g_cfg.menusize = D_CFG.menusize;
}
void ResetAll() {
    bool adv = g_cfg.advanced, dbg = g_cfg.debugtab;
    for (auto& s : SETTINGS) ResetSetting(s);
    g_cfg.advanced = adv; g_cfg.debugtab = dbg;
}

}  // namespace

// ---- for the host ----
void RdMenuGoTab(int tab) { g_menuGoTab = tab; }
// the pad may move through the menu now: not while a button row listens, and not right after it took its button
// (that press would activate the row again)
int RdMenuKey() { return g_cfg.menukey; }
int RdMenuDirty() { return PendingCount() != 0; }
void RdMenuSave() { SaveAll(); }
// The buttons held on all pads, on every change, while the menu has the pads (the game and OnInput get none then):
// a button row that listens takes the next named button. true = the menu must not act on this press
bool RdMenuBindPad(u64 held) { return CapFeed(held); }
void RdMenuClosed() { g_bindTarget = nullptr; g_keyTarget = nullptr; g_resetTabAsk = g_resetAllAsk = 0; }
// PC keys (Hookshot, window thread), 1.1.0: a key cell that listens takes the next key (Esc = none); a bound key holds
// its KB_* bit while down (OnInput adds them to the pad's buttons). 1 = the mod's key (the game does not get it),
// -1 = an action key the game keeps (mod off, or not in play: as the pad button), 0 = not ours
int RdMenuKeyEvent(int kind, int vk) {
    if (kind == HSML_IN_FOCUS_LOST) { g_keyHeld = 0; return 0; }
    if (kind == HSML_IN_KEY_DOWN && g_keyTarget && vk) {
        volatile int* t = g_keyTarget;
        *t = vk == 0x1b ? 0 : vk;   // (kept by Save)
        g_keyTarget = nullptr;
        return 1;
    }
    const struct { int k; u64 bit; } MAP[] = {{g_cfg.ragkey, KB_RAG}, {g_cfg.getupkey, KB_GETUP}, {g_cfg.spinkey, KB_SPIN}, {g_cfg.forcekey, KB_FORCE}};
    bool ours = false;
    for (auto& m : MAP) {
        if (!m.k || m.k != vk) continue;
        ours = true;
        if (kind == HSML_IN_KEY_DOWN) __atomic_fetch_or(const_cast<u64*>(&g_keyHeld), m.bit, __ATOMIC_SEQ_CST);
        else if (kind == HSML_IN_KEY_UP) __atomic_fetch_and(const_cast<u64*>(&g_keyHeld), ~m.bit, __ATOMIC_SEQ_CST);
    }
    if (!ours) return 0;
    return g_cfg.enabled && InPlay() ? 1 : -1;
}
// the Menu size setting as ImGui's scale (150 % = 1.5). The host gives it to RdMenuStyle (hookshot/menu_host.hpp)
// before a frame, whenever it changed (never inside a frame)
float RdMenuScale() { return static_cast<float>(g_cfg.menusize) * 0.01f; }
void RdMenuSetSize(int percent) { g_cfg.menusize = percent < MENU_SIZE_MIN ? MENU_SIZE_MIN : percent > MENU_SIZE_MAX ? MENU_SIZE_MAX : percent; }

void RdMenuPage();
// a window of its own around the page: only the offline preview (hookshot/menu_preview.cpp). In the game the page is
// drawn into HSML's menu window
void RdBuildMenu() {
    ImGuiIO& io = ImGui::GetIO();
    // the window: top left, sized for the Menu size; when that changes it grows / shrinks with its content and stays
    // on the picture
    static float winScale; static ImVec2 winPos, winSize;   // the window last frame, and the scale it was drawn at
    float scale = RdMenuScale();
    ImVec2 ds = io.DisplaySize;
    if (winScale > 0 && winScale != scale && winSize.x > 0) {
        ImVec2 sz(fminf(winSize.x * scale / winScale, ds.x), fminf(winSize.y * scale / winScale, ds.y));
        ImGui::SetNextWindowSize(sz);
        ImGui::SetNextWindowPos(ImVec2(fmaxf(fminf(winPos.x, ds.x - sz.x), 0.f), fmaxf(fminf(winPos.y, ds.y - sz.y), 0.f)));
    } else {
        float m = fminf(60, ds.y * 0.04f);
        ImGui::SetNextWindowPos(ImVec2(m, m), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(fminf(960 * scale / 1.5f, ds.x - 2 * m), fminf(900 * scale / 1.5f, ds.y - 2 * m)), ImGuiCond_FirstUseEver);   // 960 x 900 at 150 %
    }
    char title[64]; snprintf(title, sizeof title, "Ragdoll Button %s###ragdoll", MOD_VERSION);
    ImGui::Begin(title, nullptr, ImGuiWindowFlags_NoCollapse);
    winScale = scale; winPos = ImGui::GetWindowPos(); winSize = ImGui::GetWindowSize();
    RdMenuPage();
    ImGui::End();
}

// The mod's page: the tabs, the footer and the help box, laid out for the width it gets. Save and the unsaved mark are
// in HSML's header (RdMenuDirty / RdMenuSave)
void RdMenuPage() {
    hsk::helpTo = &g_help;
    const char* help = g_help; g_help = nullptr;   // (last frame's)
    // tabs; ZL / ZR step them on the pad (L / R are the menu's own)
    int last = g_cfg.debugtab ? T_DEBUG : T_DEBUG - 1;
    if (g_menuTab > last) g_menuTab = T_START;
    if (int step = hsk::TabStep()) g_menuGoTab = step > 0 ? (g_menuTab >= last ? T_START : g_menuTab + 1) : (g_menuTab <= T_START ? last : g_menuTab - 1);
    const ImGuiStyle& st = ImGui::GetStyle();
    static float footH;   // the footer's height last frame (its parts wrap in a narrow page; a reset's question adds lines)
    if (footH <= 0.f) footH = ImGui::GetFrameHeight() + st.ItemSpacing.y;
    float foot = footH + hsk::HelpBoxH() + st.ItemSpacing.y * 3 + 2;
    if (hsk::BeginTabs("tabs")) {
        for (int t = T_START; t <= last; t++) {
            if (!hsk::Tab(TAB_NAMES[t], g_menuGoTab == t)) continue;
            if (t != g_menuTab) g_resetTabAsk = 0;   // (the question was about the tab before)
            g_menuTab = t;
            ImGui::BeginChild("body", ImVec2(0, -foot), ImGuiChildFlags_NavFlattened);
            switch (t) {
                case T_START: TabStart(); break;
                case T_VOICE: TabVoices(); break;
                case T_CONTROLS: TabControls(); break;
                case T_DEBUG: TabDebug(); break;
                default: { const hsk::Rows rows = LabelsOf(static_cast<Tab>(t)); DrawRows(static_cast<Tab>(t), rows); break; }
            }
            ImGui::EndChild();
            hsk::EndTab();
        }
        hsk::EndTabs();
    }
    g_menuGoTab = -1;

    // footer, the same on every tab: a flowing row (its pieces wrap onto the next line in a narrow page); a reset asks
    // first, its question in place of the buttons
    ImGui::Separator();
    float y0 = ImGui::GetCursorPosY();
    int pending = PendingCount();
    char state[40];
    if (pending == 1) snprintf(state, sizeof state, "1 change not saved");
    else if (pending) snprintf(state, sizeof state, "%d changes not saved", pending);
    else snprintf(state, sizeof state, "all saved");
    hsk::Label(state, pending ? hsk::WARN : hsk::HintCol());
    char lab[48], q[112];
    snprintf(lab, sizeof lab, "Reset %s", TAB_NAMES[g_menuTab]);
    snprintf(q, sizeof q, "Every setting of the %s tab back to its default, its Advanced rows too?", TAB_NAMES[g_menuTab]);
    if (!g_resetAllAsk) {
        if (!g_resetTabAsk) hsk::SameLineIfFits(hsk::ButtonW(lab), hsk::WideGap());
        if (hsk::Confirm(lab, q, &g_resetTabAsk, "Reset", "This tab's settings back to default, its Advanced rows too (asks first). Save afterwards to keep it."))
            ResetTab(g_menuTab);
    }
    if (!g_resetTabAsk) {
        if (!g_resetAllAsk) hsk::SameLineIfFits(hsk::ButtonW("Reset all"), hsk::WideGap());
        if (hsk::Confirm("Reset all", "Every setting back to its default?", &g_resetAllAsk, "Reset", "Everything back to default (asks first). Save afterwards to keep it."))
            ResetAll();
    }
    hsk::SameLineIfFits(hsk::TickW("Advanced"), hsk::WideGap());
    ImGui::Checkbox("Advanced", &g_cfg.advanced);
    hsk::Probed(__LINE__, "menu.inc");
    hsk::Tip("Shows the expert rows on every tab.");
    hsk::SameLineIfFits(hsk::TickW("Debug tab"), hsk::WideGap());
    ImGui::Checkbox("Debug tab", &g_cfg.debugtab);
    hsk::Probed(__LINE__, "menu.inc");
    hsk::Tip("Shows the Debug tab (live values, the body map log).");
    footH = ImGui::GetCursorPosY() - y0;
    ImGui::Separator();
    hsk::HelpBox(help);
}
