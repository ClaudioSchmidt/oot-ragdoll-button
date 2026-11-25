// oot-ragdoll, part of main.cpp (RD_MENU: the Hookshot build): the settings menu, in the style of Immersive First
// Person's (its helpers are taken from there): a window at the top left, tabs (ZL / ZR), rows of label | control | a
// small reset button, a footer with Save / Reset / Advanced / Debug tab and a help box.
// Most rows come from the SETTINGS table (config.inc): on the tab and under the heading the table names. Voices
// (lists with play / remove / add) and the Menu size row are drawn by hand.
// A changed value works at once. The settings file gets it with Save: until then its label is yellow and the footer
// counts the unsaved changes (config.inc: g_saved*).
// Drawn with Dear ImGui into HSML's shared menu (RdMenuPage, registered in hookshot/hs_rt.cpp); hookshot/menu_preview.cpp: offline pictures.
// Included outside main.cpp's namespace: RdBuildMenu and the Rd* helpers are what the host calls.

namespace {

int g_menuTab = T_START;     // the tab shown
int g_menuGoTab = -1;        // select this tab in the next frame (ZL / ZR, preview)
const char* g_help;          // help text of the row under the mouse / pad focus (help box, a frame later)
float g_labW;                // the label column of the tab being drawn

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
constexpr const char* TRIG_LABELS[3] = {"Tap", "Double tap", "Hold"};
constexpr const char* FORCE_LABELS[5] = {"Player", "Light", "Medium", "Strong", "Mighty"};
const ImVec4 UNSAVED(1.f, 0.82f, 0.35f, 1.f);   // a label whose value the file does not have yet

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

// ---- help: the box at the bottom, and a tooltip under the mouse ----
void Help(const char* h) {
    if (!h || !h[0]) return;
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) || ImGui::IsItemFocused()) g_help = h;
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_ForTooltip) && ImGui::BeginTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26);
        ImGui::TextUnformatted(h);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// ---- reset button: a small square with an undo arrow right after the control just drawn; greyed at the default ----
// hsml 2026-11-24 (user: the reset buttons as HSML's menu has them): as wide as an X button, its arrow the same
constexpr float PI_F = 3.14159265f;
float ResetW() { return ImGui::CalcTextSize("X").x + ImGui::GetStyle().FramePadding.x * 2; }
float ResetSpace() { return ResetW() + ImGui::GetStyle().ItemInnerSpacing.x; }
bool ResetCore(bool atDef, const char* lead, const char* what) {
    static char tip[200];
    ImGuiID id = ImGui::GetItemID();
    float h = ImGui::GetFrameHeight();
    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::PushID(static_cast<int>(id));
    if (atDef) ImGui::BeginDisabled();
    bool hit = ImGui::Button("##rst", ImVec2(ResetW(), h));
    // (as HSML's menu draws it: a three-quarter circle with an arrow head at its start)
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    ImVec2 c((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
    float r = fminf(b.x - a.x, b.y - a.y) * 0.3f, th = fmaxf(1.f, r * 0.22f);
    ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);
    dl->PathArcTo(c, r * 0.8f, PI_F * 0.75f, PI_F * 2.25f, 16);
    dl->PathStroke(col, 0, th);
    ImVec2 e(c.x + cosf(PI_F * 0.75f) * r * 0.8f, c.y + sinf(PI_F * 0.75f) * r * 0.8f);
    dl->AddTriangleFilled(ImVec2(e.x - r * 0.45f, e.y - r * 0.1f), ImVec2(e.x + r * 0.35f, e.y - r * 0.25f), ImVec2(e.x - r * 0.05f, e.y + r * 0.5f), col);
    if (atDef) ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) || ImGui::IsItemFocused()) {
        if (what && what[0]) snprintf(tip, sizeof tip, "%s (%s)", lead, what); else snprintf(tip, sizeof tip, "%s", lead);
        g_help = tip;
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_ForTooltip)) ImGui::SetTooltip("%s", tip);
    }
    ImGui::PopID();
    return hit && !atDef;
}
// a button that acts on the second press within 4 s; id: fixed (the label changes)
bool ConfirmButton(const char* label, const char* id) {
    static ImGuiID armed;
    static double t;
    ImGuiID me = ImGui::GetID(id);
    if (armed == me && ImGui::GetTime() - t > 4) armed = 0;
    char buf[96];
    snprintf(buf, sizeof buf, armed == me ? "Press again: %s###%s" : "%s###%s", label, id);
    if (!ImGui::Button(buf)) return false;
    if (armed == me) { armed = 0; return true; }
    armed = me; t = ImGui::GetTime();
    return false;
}

// ---- rows: label column (as wide as the tab's widest label) | control ----
bool BeginRows(const char* id) {
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingFixedFit)) return false;
    ImGui::TableSetupColumn("l", ImGuiTableColumnFlags_WidthFixed, g_labW);
    ImGui::TableSetupColumn("v", ImGuiTableColumnFlags_WidthStretch);
    return true;
}
// the label (yellow while the file does not have the value) and the move to the control's cell
void RowLabel(const char* label, const char* help, bool saved = true) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    if (!saved) ImGui::PushStyleColor(ImGuiCol_Text, UNSAVED);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + g_labW);
    ImGui::TextUnformatted(label);
    ImGui::PopTextWrapPos();
    if (!saved) ImGui::PopStyleColor();
    Help(help);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-ResetSpace());   // room for the reset button
}
// the label column for a tab: its widest label showing, at least 12 letters, at most 45 % of the window
void LabelWidth(Tab t, const char* const* more = nullptr, int nMore = 0) {
    float w = ImGui::GetFontSize() * 6;
    for (auto& s : SETTINGS)
        if ((s.tab == t || (t == T_CONTROLS && !strcmp(s.key, "spin_button"))) && !(s.adv && !g_cfg.advanced))
            w = fmaxf(w, ImGui::CalcTextSize(s.label).x);
    for (int i = 0; i < nMore; i++) w = fmaxf(w, ImGui::CalcTextSize(more[i]).x);
    g_labW = fminf(w + ImGui::GetStyle().ItemSpacing.x, ImGui::GetContentRegionAvail().x * 0.45f);
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
void ResetFor(const Setting& s) {
    char t[96]; DefaultText(s, t, sizeof t);
    if (ResetCore(IsDefault(s), "Back to default", t)) ResetSetting(s);
}

// one row from the table: label | control [Press] | reset
void DrawSetting(const Setting& s) {
    ImGui::PushID(s.key);
    RowLabel(s.label, s.help, IsSaved(s));
    switch (s.kind) {
        case K_BOOL: ImGui::Checkbox("##v", static_cast<bool*>(s.p)); break;
        case K_FLOAT: {
            float* p = static_cast<float*>(s.p);
            float shown = *p * s.scale;
            if (ImGui::SliderFloat("##v", &shown, s.lo, s.hi, s.unit)) *p = shown / s.scale;
            break;
        }
        case K_INT: {
            int v = static_cast<int>(*static_cast<u32*>(s.p));
            if (ImGui::SliderInt("##v", &v, static_cast<int>(s.lo), static_cast<int>(s.hi), s.unit)) *static_cast<u32*>(s.p) = static_cast<u32>(v < 0 ? 0 : v);
            break;
        }
        case K_CHOICE: {
            int v = static_cast<int>(*static_cast<u32*>(s.p));
            if (ImGui::Combo("##v", &v, s.unit)) *static_cast<u32*>(s.p) = static_cast<u32>(v);
            break;
        }
        case K_FORCE: ImGui::Combo("##v", static_cast<int*>(s.p), FORCE_LABELS, 5); break;
        case K_BUTTON: {   // (the Controls tab draws its own; this for a button row on another tab)
            u64* p = static_cast<u64*>(s.p);
            bool listening = g_bindTarget == p;
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(listening ? "press, let go ..." : MaskLabel(*p));
            Help(s.help);
            ImGui::SameLine();
            if (ImGui::SmallButton(listening ? "Cancel###press" : "Set###press")) { g_keyTarget = nullptr; g_bindStart = Ticks(); g_bindTarget = listening ? nullptr : p; }
            break;
        }
        case K_BIND: break;
        case K_KEY: {
            int* p = static_cast<int*>(s.p);
            if (ImGui::BeginCombo("##v", KeyName(*p))) {
                for (auto& k : KEYS) if (ImGui::Selectable(k.n, k.vk == *p)) *p = k.vk;
                ImGui::EndCombo();
            }
            break;
        }
        case K_TEXT: ImGui::InputText("##v", static_cast<char*>(s.p), s.cap); break;
        case K_WEIGHTS: break;
    }
    if (s.kind != K_BUTTON && s.kind != K_BIND) Help(s.help);
    ResetFor(s);
    ImGui::PopID();
}

const Setting* Find(const char* key) {
    for (auto& s : SETTINGS) if (!strcmp(s.key, key)) return &s;
    return nullptr;
}

// the table's rows of one tab, under their headings; advanced rows only with the tick. Text rows of the Voices tab
// are drawn by VoiceRows
void DrawRows(Tab t) {
    const char* heading = nullptr; bool table = false;
    for (auto& s : SETTINGS) {
        if (s.tab != t) continue;
        if (s.section) heading = s.section;
        if (s.adv && !g_cfg.advanced) continue;
        if (t == T_VOICE && s.kind == K_TEXT) continue;   // (VoiceRows)
        if (heading) {
            if (table) { ImGui::EndTable(); table = false; }
            ImGui::SeparatorText(heading); heading = nullptr;
        }
        if (!table) { table = BeginRows("rows"); if (!table) continue; }
        DrawSetting(s);
    }
    if (table) ImGui::EndTable();
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
    if (off) ImGui::EndDisabled();
    Help(InPlayNow() ? "Play it now." : "Play it now. Link is not in play (title, game menu, cutscene): nothing will sound until he is.");
}
// the dropdown with every known voice, a filter, and a field for any other name. pick: the chosen name. preview: the
// closed box's text instead of pick; taken: names already on the list (ticked, not selectable)
bool VoicePicker(const char* id, char* pick, int cap, bool withNone, const char* preview = nullptr,
                 const char (*taken)[VOICE_LEN] = nullptr, int nTaken = 0) {
    static char filter[VOICE_LEN];
    bool changed = false;
    if (!ImGui::BeginCombo(id, preview ? preview : pick[0] ? pick : "choose a voice ...", ImGuiComboFlags_HeightLarge)) return false;
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
        if (on) Help("Already on this list.");
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
// the list's reset), it adds the voice picked in it at once; below it a line per entry: name | play | X (X in the
// reset column, as Immersive First Person's lists)
void VoiceList(const Setting& s) {
    const ImGuiStyle& st = ImGui::GetStyle();
    char* buf = static_cast<char*>(s.p);
    char items[VOICE_MAX][VOICE_LEN];
    int n = ListParse(buf, items);
    ImGui::PushID(s.key);
    RowLabel(s.label, s.help, IsSaved(s));
    float h = ImGui::GetFrameHeight(), in = st.ItemInnerSpacing.x;
    // the dropdown | reset
    char pk[VOICE_LEN] = "";
    bool full = n >= VOICE_MAX;
    const char* why = full ? "A list takes 8 voices." : nullptr;
    ImGui::SetNextItemWidth(-ResetSpace());
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
    Help(why ? why : "Pick a voice: it goes on this list at once. The arrow next to a voice plays it.");
    ResetFor(s);
    // the entries
    int drop = -1;
    for (int i = 0; i < n; i++) {
        ImGui::PushID(i);
        float x0 = ImGui::GetCursorPosX(), w = ImGui::GetContentRegionAvail().x;
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(items[i]);
        Help(s.help);
        ImGui::SameLine(0, 0);
        ImGui::SetCursorPosX(fmaxf(ImGui::GetCursorPosX() + in, x0 + w - (h * 2 + in)));   // play | X at the right edge
        PlayButton("##play", items[i]);
        ImGui::SameLine(0, in);
        if (ImGui::Button("X##rm", ImVec2(h, h))) drop = i;
        Help("Remove it from this list.");
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
void ForceVoices(const Setting& s) {
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
        RowLabel(FORCE_LABELS[i], s.help, i < ns && !strcmp(saved[i], items[i]));
        ImGui::SetNextItemWidth(-(ResetSpace() + ImGui::GetFrameHeight() + st.ItemInnerSpacing.x));
        bool ch = VoicePicker("##pick", items[i], VOICE_LEN, true);
        Help(s.help);
        ImGui::SameLine(0, st.ItemInnerSpacing.x);
        PlayButton("##try", items[i]);
        const char* d = i < nd ? defs[i] : "none";
        if (ResetCore(!strcmp(items[i], d), "Back to default", d)) { snprintf(items[i], VOICE_LEN, "%s", d); ch = true; }
        if (ch) ListWrite(buf, s.cap, items, 5);
        ImGui::PopID();
    }
    ImGui::PopID();
}
void VoiceRows() {
    // 1.1.0 (user): press and kicks per force level; one level shown at a time, at first the force level now
    static int level = -1, seenForce = -1;
    if (g_cfg.force != seenForce) { seenForce = g_cfg.force; level = g_cfg.force; }
    if (level < 0 || level > 4) level = 2;
    ImGui::SeparatorText("Ragdoll button and falling, per force level");
    ImGui::TextWrapped("Each situation plays the voices of its list in turn, one each time; an empty list is silent. Names starting with Voice open his mouth, the others are sound only.");
    if (BeginRows("levels")) {
        const char* hL = "Which force level's lists show below. It starts at the force level you play with now.";
        RowLabel("Force level", hL);
        char now[5][24];
        const char* names[5];
        for (int i = 0; i < 5; i++) { snprintf(now[i], sizeof now[i], i == g_cfg.force ? "%s (now)" : "%s", FORCE_LABELS[i]); names[i] = now[i]; }
        ImGui::Combo("##level", &level, names, 5);
        Help(hL);
        static const char* const PER[] = {"voicepress_", "voicefall_"};
        for (auto k : PER) {
            char key[32]; snprintf(key, sizeof key, "%s%s", k, FORCES[level]);
            if (const Setting* s = Find(key)) VoiceList(*s);
        }
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Landings and bumps, by how hard he hits");
    if (BeginRows("impacts")) {
        static const char* const LISTS[] = {"voicehard", "voicelight", "voiceslide"};
        for (auto k : LISTS) if (const Setting* s = Find(k)) VoiceList(*s);
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Getting up");
    if (BeginRows("getup")) {
        if (const Setting* s = Find("voicegetup")) VoiceList(*s);
        ImGui::EndTable();
    }
    if (!g_cfg.advanced) return;
    ImGui::SeparatorText("Force level changed (settings button twice)");
    if (BeginRows("force")) {
        if (const Setting* s = Find("forcevoice")) ForceVoices(*s);
        ImGui::EndTable();
    }
    ImGui::SeparatorText("New ragdoll button (settings button held 3 s)");
    if (BeginRows("bind")) {
        if (const Setting* s = Find("listenvoice")) VoiceList(*s);
        if (const Setting* s = Find("setvoice")) VoiceList(*s);
        ImGui::EndTable();
    }
}

// ---- the tabs ----
void TabStart() {
    LabelWidth(T_START);
    if (g_state == ACTIVE) ImGui::Text("Link: ragdolled, %.1f m/s, %s", static_cast<double>(g_bodySpeed), Grounded() ? "on the ground" : "in the air");
    else if (InPlayNow()) ImGui::TextUnformatted("Link: on his feet");
    else ImGui::TextDisabled("Link: not in play (title, game menu, cutscene, other state)");
    ImGui::Spacing();
    DrawRows(T_START);
    ImGui::SeparatorText("How to use this menu");
    ImGui::TextWrapped("Pad: D-pad or left stick moves, A changes a row (left / right on a slider), B goes back, ZL / ZR change the tab. "
                       "Mouse: drag a slider, Ctrl + click to type a value.\n"
                       "A change works at once. Save (at the top) keeps it for the next start; a yellow label is not saved yet.\n"
                       "The menu's size and who gets the controller while it is open are on the HSML page.");
}

// ---- Controls (1.1.1, user: as HSML's and Immersive Camera's): a table, one line per action: Buttons (one or two,
// Set records them), Trigger, Time, PC key; each with its reset. Too narrow for the columns (under 48 letters): a block
// per action, each part on its own line with its head in grey ----
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
    constexpr int COLS = 5;
    const char* hK = "A PC key that does the same (game window in front). Set, then press the key; Esc = none. X: none.";
#else
    constexpr int COLS = 4;   // (no keyboard)
    const char* hK = "";
#endif
    const char* const HDR[5] = {"", "Buttons", "Trigger", "Time", "PC key"};
    const char* const HDRH[5] = {"", hB, hT, hS, hK};
    const ImGuiStyle& st = ImGui::GetStyle();
    bool narrow = ImGui::GetContentRegionAvail().x < ImGui::GetFontSize() * 48;
    // every name gets the widest name's room, so the Set buttons line up
    float nameW = fmaxf(ImGui::CalcTextSize("ZL + R3").x, ImGui::CalcTextSize("press, let go").x), keyW = ImGui::CalcTextSize("Backspace").x;
    for (auto& r : ROWS) {
        if (const Setting* s = Find(r.bind)) nameW = fmaxf(nameW, ImGui::CalcTextSize(MaskLabel(s->kind == K_BIND ? static_cast<Bind*>(s->p)->mask : *static_cast<u64*>(s->p))).x);
        char t[16];
        if (const Setting* s = Find(r.key)) { KeyLabel(*static_cast<int*>(s->p), t, sizeof t); keyW = fmaxf(keyW, ImGui::CalcTextSize(t).x); }
    }
    if (narrow) nameW = keyW = fmaxf(nameW, keyW);   // (one under the other: the Set buttons in line)
    auto cell = [&](int c) {
        if (!narrow) { if (!c) ImGui::TableNextRow(); ImGui::TableNextColumn(); return; }
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        if (!c) return;
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("  %s", HDR[c]); Help(HDRH[c]);
        ImGui::TableSetColumnIndex(1);
    };
    auto nameCell = [&](const char* s, float w, bool warn) {
        float x0 = ImGui::GetCursorPosX();
        ImGui::AlignTextToFramePadding();
        if (warn) ImGui::TextColored(UNSAVED, "%s", s); else ImGui::TextUnformatted(s);
        ImGui::SameLine();
        ImGui::SetCursorPosX(x0 + w + st.ItemSpacing.x);
    };
    auto clearBtn = [](bool can, const char* id) {
        ImGui::SameLine();
        if (!can) ImGui::BeginDisabled();
        bool hit = ImGui::SmallButton(id);
        if (!can) ImGui::EndDisabled();
        Help("None for this.");
        return hit && can;
    };
    if (ImGui::BeginTable("binds", narrow ? 2 : COLS, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
        if (narrow) ImGui::TableSetupColumn("v", ImGuiTableColumnFlags_WidthStretch);
        else {
            ImGui::TableSetupColumn(HDR[1], ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn(HDR[2], ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn(HDR[3], ImGuiTableColumnFlags_WidthStretch);
            if (COLS > 4) ImGui::TableSetupColumn(HDR[4], ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
            for (int c = 0; c < COLS; c++) { ImGui::TableNextColumn(); ImGui::TableHeader(HDR[c]); if (c) Help(HDRH[c]); }
        }
        for (int i = 0; i < NR; i++) {
            const Setting* sb = Find(ROWS[i].bind);
            const Setting* sk = Find(ROWS[i].key);
            if (!sb || !sk) continue;
            bool isBind = sb->kind == K_BIND;
            Bind* b = isBind ? static_cast<Bind*>(sb->p) : nullptr;
            const Bind* d = isBind ? static_cast<const Bind*>(DefaultOf(*sb)) : nullptr;
            u64* mask = isBind ? &b->mask : static_cast<u64*>(sb->p);
            u64 dmask = isBind ? d->mask : *static_cast<const u64*>(DefaultOf(*sb));
            ImGui::PushID(i);
            // the action (yellow while the file does not have its line)
            cell(0);
            bool saved = IsSaved(*sb) && (COLS < 5 || IsSaved(*sk));
            ImGui::AlignTextToFramePadding();
            if (!saved) ImGui::PushStyleColor(ImGuiCol_Text, UNSAVED);
            ImGui::TextUnformatted(sb->label);
            if (!saved) ImGui::PopStyleColor();
            Help(sb->help);
            // buttons
            cell(1);
            if (g_bindTarget == mask) {
                nameCell("press, let go", nameW, true);
                if (ImGui::SmallButton("Cancel##b")) g_bindTarget = nullptr;
            } else {
                nameCell(MaskLabel(*mask), nameW, false);
                if (ImGui::SmallButton("Set##b")) { g_keyTarget = nullptr; g_bindStart = Ticks(); g_bindTarget = mask; }   // input.inc CapFeed
                Help(hB);
                if (clearBtn(*mask != 0, "X##b")) *mask = 0;
                if (ResetCore(*mask == dmask, "Back to default", MaskLabel(dmask))) *mask = dmask;
            }
            // trigger, time
            if (!isBind) {   // spin: held, with the left stick
                cell(2); ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("Held");
                Help("While held, the left stick spins him in the air and rolls him on the ground. none: the stick always does.");
                cell(3); ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("-");
            } else {
                cell(2);
                ImGui::SetNextItemWidth(fmaxf(ImGui::GetContentRegionAvail().x - ResetSpace(), ImGui::CalcTextSize("Double tap").x + ImGui::GetFrameHeight() + st.FramePadding.x * 2));
                int tr = static_cast<int>(b->trig < 3 ? b->trig : 0);
                if (ImGui::Combo("##t", &tr, TRIG_LABELS, 3)) b->trig = static_cast<u32>(tr);
                Help(hT);
                if (ResetCore(b->trig == d->trig, "Back to default", TRIG_LABELS[d->trig])) b->trig = d->trig;
                cell(3);
                ImGui::BeginDisabled(b->trig == TR_TAP);
                ImGui::SetNextItemWidth(fmaxf(ImGui::GetContentRegionAvail().x - ResetSpace(), ImGui::CalcTextSize("0.00 s").x * 2));
                ImGui::SliderFloat("##s", &b->time, 0.2f, 2.f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
                Help(hS);
                ImGui::EndDisabled();
                char dt[16]; snprintf(dt, sizeof dt, "%.2f s", static_cast<double>(d->time));
                if (ResetCore(b->time == d->time, "Back to default", dt)) b->time = d->time;
            }
#ifdef HOOKSHOT
            // the PC key
            cell(4);
            int* k = static_cast<int*>(sk->p);
            if (g_keyTarget == k) {
                nameCell("press a key", keyW, true);
                if (ImGui::SmallButton("Cancel##k")) g_keyTarget = nullptr;
            } else {
                char t[16]; KeyLabel(*k, t, sizeof t);
                nameCell(t, keyW, false);
                if (ImGui::SmallButton("Set##k")) { g_bindTarget = nullptr; g_keyTarget = k; }   // the next key (RdMenuKeyEvent)
                Help(hK);
                if (clearBtn(*k != 0, "X##k")) *k = 0;
                ResetFor(*sk);
            }
#endif
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::Spacing();
    ImGui::TextDisabled("Defaults: Ragdoll R, Get up B, Next force level D-pad down twice.");
    ImGui::TextDisabled("The menu itself opens with its own binding (HSML's page).");
}

void TabDebug() {
    LabelWidth(T_DEBUG);
    DrawRows(T_DEBUG);
    ImGui::SeparatorText("Now");
    ImGui::Text("state %s, physics frame %u, rest frames %u, %s", g_state == ACTIVE ? "ragdoll" : "idle", g_physFrames, g_restFrames,
                g_state == ACTIVE && Grounded() ? "grounded" : "not grounded");
    ImGui::Text("hips %.1f %.1f %.1f at %.1f m/s", static_cast<double>(g_bodyPos[0]), static_cast<double>(g_bodyPos[1]), static_cast<double>(g_bodyPos[2]), static_cast<double>(g_bodySpeed));
    ImGui::Text("buttons %#llx, stick %d %d", static_cast<unsigned long long>(g_held), static_cast<int>(g_stickX), static_cast<int>(g_stickY));
    ImGui::Text("ragdoll-button %s, tune @ %p", MOD_VERSION, static_cast<void*>(&g_tune));
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
void RdMenuClosed() { g_bindTarget = nullptr; g_keyTarget = nullptr; }
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
    static const char* help;   // last frame's
    help = g_help; g_help = nullptr;
    {   // this page's own binding, if any (the menu's own is HSML's)
        char t[96]; int n = 0;
        if (g_cfg.menu.mask) n += snprintf(t + n, sizeof t - n, "%s%s", MaskLabel(g_cfg.menu.mask), g_cfg.menu.trig == TR_DOUBLE ? " twice" : g_cfg.menu.trig == TR_HOLD ? " held" : "");
#ifdef HOOKSHOT
        if (g_cfg.menukey) { char k[16]; KeyLabel(g_cfg.menukey, k, sizeof k); n += snprintf(t + n, sizeof t - n, "%s%s", n ? " or " : "", k); }
#endif
        if (n) ImGui::TextDisabled("%s: this page | ZL / ZR: tabs", t); else ImGui::TextDisabled("ZL / ZR: tabs");
    }
    // tabs; ZL / ZR switch them on the pad (L / R are ImGui's own navigation)
    int last = g_cfg.debugtab ? T_DEBUG : T_DEBUG - 1;
    if (g_menuTab > last) g_menuTab = T_START;
    if (ImGui::IsKeyPressed(ImGuiKey_GamepadR2, false)) g_menuGoTab = g_menuTab >= last ? T_START : g_menuTab + 1;
    if (ImGui::IsKeyPressed(ImGuiKey_GamepadL2, false)) g_menuGoTab = g_menuTab <= T_START ? last : g_menuTab - 1;
    const ImGuiStyle& st = ImGui::GetStyle();
    float helpH = ImGui::GetTextLineHeight() * 3 + st.WindowPadding.y * 2;
    static int footRows = 1;   // the footer's lines last frame (its parts wrap in a narrow window)
    float foot = (ImGui::GetFrameHeight() + st.ItemSpacing.y) * static_cast<float>(footRows) + helpH + st.ItemSpacing.y * 3 + 2;
    if (ImGui::BeginTabBar("tabs")) {
        for (int t = T_START; t <= last; t++) {
            if (!ImGui::BeginTabItem(TAB_NAMES[t], nullptr, g_menuGoTab == t ? ImGuiTabItemFlags_SetSelected : 0)) continue;
            g_menuTab = t;
            ImGui::BeginChild("body", ImVec2(0, -foot), ImGuiChildFlags_NavFlattened);
            switch (t) {
                case T_START: TabStart(); break;
                case T_VOICE: LabelWidth(T_VOICE); DrawRows(T_VOICE); VoiceRows(); break;
                case T_CONTROLS: TabControls(); break;
                case T_DEBUG: TabDebug(); break;
                default: LabelWidth(static_cast<Tab>(t)); DrawRows(static_cast<Tab>(t)); break;
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    g_menuGoTab = -1;

    // footer, the same on every tab
    ImGui::Separator();
    int rows = 1;
    auto fits = [&rows](float w) {   // a part that no longer fits goes to the next line
        ImGui::SameLine();
        if (ImGui::GetContentRegionAvail().x < w) { ImGui::NewLine(); rows++; }
    };
    auto tickW = [&](const char* lab) { return ImGui::GetFrameHeight() + st.ItemInnerSpacing.x + ImGui::CalcTextSize(lab).x + st.ItemSpacing.x; };
    int pending = PendingCount();
    ImGui::AlignTextToFramePadding();
    if (pending == 1) ImGui::TextColored(UNSAVED, "1 change not saved");
    else if (pending) ImGui::TextColored(UNSAVED, "%d changes not saved", pending);
    else ImGui::TextDisabled("all saved");
    {
        char lab[48]; snprintf(lab, sizeof lab, "Reset %s", TAB_NAMES[g_menuTab]);
        fits(ImGui::CalcTextSize(lab).x + ImGui::CalcTextSize("Press again: ").x + st.FramePadding.x * 2 + st.ItemSpacing.x);
        if (ConfirmButton(lab, "rtab")) ResetTab(g_menuTab);
        Help("This tab's settings back to default, its Advanced rows too (press twice). Save afterwards to keep it.");
    }
    fits(ImGui::CalcTextSize("Press again: Reset all").x + st.FramePadding.x * 2 + st.ItemSpacing.x);
    if (ConfirmButton("Reset all", "rall")) ResetAll();
    Help("Everything back to default (press twice). Save afterwards to keep it.");
    fits(tickW("Advanced"));
    ImGui::Checkbox("Advanced", &g_cfg.advanced);
    Help("Shows the expert rows on every tab.");
    fits(tickW("Debug tab"));
    ImGui::Checkbox("Debug tab", &g_cfg.debugtab);
    Help("Shows the Debug tab (live values, the body map log).");
    footRows = rows;
    ImGui::Separator();
    ImGui::BeginChild("help", ImVec2(0, helpH), ImGuiChildFlags_None, ImGuiWindowFlags_NoNav);
    ImGui::TextWrapped("%s", help && help[0] ? help : "Point at a setting to see what it does.");
    ImGui::EndChild();
}
