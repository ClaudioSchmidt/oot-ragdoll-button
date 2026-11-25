// oot-ragdoll, part of main.cpp: the settings file, read and written from the SETTINGS table (config.inc)

// a mask as the file has it: the BUTTONS names joined by + (none = 0)
const char* ButtonName(u64 m) {
    static char ring[4][40]; static unsigned at;
    char* o = ring[at++ % 4];
    int len = 0;
    o[0] = 0;
    for (auto& b : BUTTONS) if (b.m && (m & b.m) && len < 30) len += snprintf(o + len, sizeof ring[0] - len, len ? "+%s" : "%s", b.n);
    return len ? o : "none";
}
// "r", "r+down" (at most two), "none"; false = a name not known
bool ButtonsParse(const char* v, u64* out) {
    if (!strcmp(v, "none")) { *out = 0; return true; }
    u64 m = 0; int n = 0;
    char part[16];
    for (const char* p = v; *p; ) {
        size_t k = strcspn(p, "+");
        if (!k || k >= sizeof part) return false;
        memcpy(part, p, k); part[k] = 0;
        bool hit = false;
        for (auto& b : BUTTONS) if (b.m && !strcmp(part, b.n)) { m |= b.m; hit = true; }
        if (!hit || ++n > 2) return false;
        p += k; if (*p == '+') p++;
    }
    *out = m;
    return true;
}
constexpr const char* TRIG_FILE[3] = {"tap", "double", "hold"};
// a key as the file has it: the KEYS name, a letter / digit, else vk<hex> (1.1.0: any key can be set)
const char* KeyName(int vk) {
    for (auto& k : KEYS) if (k.vk == vk) return k.n;
    static char ring[4][8]; static unsigned at;
    char* o = ring[at++ % 4];
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) snprintf(o, sizeof ring[0], "%c", vk | 0x20);
    else if (vk > 0 && vk < 256) snprintf(o, sizeof ring[0], "vk%02x", vk);
    else return "none";
    return o;
}
bool KeyParse(const char* v, int* out) {
    for (auto& k : KEYS) if (!strcmp(v, k.n)) { *out = k.vk; return true; }
    if (v[0] && !v[1] && ((v[0] >= 'a' && v[0] <= 'z') || (v[0] >= '0' && v[0] <= '9'))) { *out = v[0] >= 'a' ? v[0] - 32 : v[0]; return true; }
    if (!strncmp(v, "vk", 2) && v[2]) {
        char* end; unsigned long n = strtoul(v + 2, &end, 16);
        if (!*end && n > 0 && n < 256) { *out = static_cast<int>(n); return true; }
    }
    return false;
}

// an action's buttons all held, or its PC key (KB_* bit)
bool BindHeld(const Bind& b, u64 held, u64 kb) { return (b.mask && (held & b.mask) == b.mask) || (held & kb); }
// get-up button; if it is the ragdoll button, use A (or B when the ragdoll button is A)
u64 GetupMask() {
    u64 g = g_cfg.getup.mask;
    if (g & g_cfg.rag.mask) g = (g_cfg.rag.mask & 1) ? 2 : 1;
    return g;
}

// one value (at `at`: the live one or the saved one) as the file has it; returns the length
int FormatSetting(const Setting& s, const void* at, char* out, size_t cap) {
    struct { void* p; } v{const_cast<void*>(at)};
    switch (s.kind) {
        case K_BOOL: return snprintf(out, cap, "%s", *static_cast<bool*>(v.p) ? "on" : "off");
        case K_FLOAT: return snprintf(out, cap, "%g", static_cast<double>(*static_cast<float*>(v.p)));
        case K_INT: case K_CHOICE: return snprintf(out, cap, "%u", *static_cast<u32*>(v.p));
        case K_BUTTON: return snprintf(out, cap, "%s", ButtonName(*static_cast<u64*>(v.p)));
        case K_BIND: {
            const Bind& b = *static_cast<Bind*>(v.p);
            return snprintf(out, cap, "%s %s %.2f", ButtonName(b.mask), TRIG_FILE[b.trig < 3 ? b.trig : 0], static_cast<double>(b.time));
        }
        case K_FORCE: { int f = *static_cast<int*>(v.p); return snprintf(out, cap, "%s", FORCES[f < 0 || f > 4 ? 2 : f]); }
        case K_KEY: return snprintf(out, cap, "%s", KeyName(*static_cast<int*>(v.p)));
        case K_TEXT: return snprintf(out, cap, "%s", static_cast<char*>(v.p));
        case K_WEIGHTS: {
            int o = 0;
            for (int i = 0; i < 24 && o < static_cast<int>(cap) - 12; i++)
                o += snprintf(out + o, cap - o, i ? ",%g" : "%g", static_cast<double>(static_cast<float*>(v.p)[i]));
            return o;
        }
    }
    return 0;
}

// false = the value is not one this setting takes (it stays as it was)
bool ParseSetting(const Setting& s, const char* v) {
    switch (s.kind) {
        case K_BOOL:
            if (!strcmp(v, "1") || !strcmp(v, "on") || !strcmp(v, "true")) *static_cast<bool*>(s.p) = true;
            else if (!strcmp(v, "0") || !strcmp(v, "off") || !strcmp(v, "false")) *static_cast<bool*>(s.p) = false;
            else return false;
            return true;
        case K_FLOAT: {
            char* end; float f = strtof(v, &end);
            if (end == v || !(f == f) || f > 1e6f || f < -1e6f) return false;
            *static_cast<float*>(s.p) = f;
            return true;
        }
        case K_INT: case K_CHOICE: {
            char* end; unsigned long n = strtoul(v, &end, 0);
            if (end == v || n > static_cast<unsigned long>(s.hi)) return false;
            *static_cast<u32*>(s.p) = static_cast<u32>(n);
            return true;
        }
        case K_BUTTON: return ButtonsParse(v, static_cast<u64*>(s.p));
        case K_BIND: {   // "r", "r tap", "r+down double 0.4" (1.1.0's lines have the buttons only: trigger and time stay)
            Bind b = *static_cast<Bind*>(s.p);
            char btn[40], trig[16]; float t;
            int n = sscanf(v, "%39s %15s %f", btn, trig, &t);
            if (n < 1 || !ButtonsParse(btn, &b.mask)) return false;
            if (n >= 2) {
                int i = 0; while (i < 3 && strcmp(trig, TRIG_FILE[i])) i++;
                if (i == 3) return false;
                b.trig = static_cast<u32>(i);
            }
            if (n >= 3) { if (!(t >= 0.1f && t <= 5.f)) return false; b.time = t; }
            *static_cast<Bind*>(s.p) = b;
            return true;
        }
        case K_FORCE:
            for (int i = 0; i < 5; i++) if (!strcmp(v, FORCES[i])) { *static_cast<int*>(s.p) = i; return true; }
            return false;
        case K_KEY: return KeyParse(v, static_cast<int*>(s.p));
        case K_TEXT: {
            size_t len = strnlen(v, s.cap - 1);
            memcpy(s.p, v, len); static_cast<char*>(s.p)[len] = 0;
            return true;
        }
        case K_WEIGHTS: {
            float* w = static_cast<float*>(s.p);
            int i = 0;
            for (const char* p = v; *p && i < 32; i++) {
                char* end; float f = strtof(p, &end);
                if (end == p) return i > 0;
                w[i] = f; p = end;
                if (*p == ',') p++;
            }
            for (; i < 32; i++) w[i] = 0;
            return true;
        }
    }
    return false;
}

constexpr size_t CFG_TEXT_MAX = 12288;

// the whole file as text, from the saved values (config.inc: g_saved*); returns its length
int WriteConfigText(char* buf, size_t cap) {
    int len = snprintf(buf, cap,
        "# oot-ragdoll %s settings. The mod writes this file (the menu's Save, Next force level); edit it with the game closed.\n"
        "# Delete a line (or the file) to get its default back. Text after # is a note.\n"
        "# controls (ragdoll, getup, nextforce, menu) = buttons trigger seconds. buttons: one or two of a b x y l r zl zr lstick rstick plus minus left up right down, joined by + (l+r), or none;\n"
        "#   trigger: tap / double / hold; seconds: double = most time between the presses, hold = how long. spin_button: held, the left stick spins / rolls him\n"
        "# keys (PC): none, a-z, 0-9, f1-f12, space tab enter shift ctrl alt left up right down insert delete home end pageup pagedown ..., or vk<hex>\n"
        "# voices: VoiceDamageL/M/S, VoiceDamageLand, VoiceFallDanger... = voice with mouth; clips Damage_B_01, FallingScream_01, Jump_S_01... = sound only; comma list = played in turn, none = off\n"
        "# voicepress_<level>: every press of the ragdoll button (start and kicks), per force level; voicefall_<level>: when he falls faster than fall_speed for fall_delay s, again every fall_repeat s; voicegetup: replaces the game's get-up voice; voicehard / voicelight: landing or wall hit from the air (10 m/s and up / below); voiceslide: bumps while he lies\n"
        "# forcevoice: one entry per force level (player, light, medium, strong, mighty), played on force change\n"
        "version = %d\n", MOD_VERSION, CONFIG_VERSION);
    Tab tab = T_COUNT;
    for (auto& s : SETTINGS) {
        if (len >= static_cast<int>(cap) - 400) break;   // (the table fits several times; never write past the text)
        if (s.tab != tab && s.tab != T_FILE) { tab = s.tab; len += snprintf(buf + len, cap - len, "\n# ---- %s ----\n", TAB_NAMES[tab]); }
        char val[200];
        FormatSetting(s, SavedOf(s), val, sizeof val);
        len += snprintf(buf + len, cap - len, "%s = %s", s.key, val);
        // a note per new value: what it does (the old keys are explained above; text values keep the line to themselves)
        bool note = s.kind != K_TEXT && s.help && s.help[0] && s.tab != T_START && s.tab != T_VOICE && s.tab != T_CONTROLS;
        if (note) len += snprintf(buf + len, cap - len, "   # %s%s%s", s.label, s.help[0] ? ": " : "", s.help);
        len += snprintf(buf + len, cap - len, "\n");
    }
    return len < static_cast<int>(cap) ? len : static_cast<int>(cap) - 1;
}

void SaveConfig() {
    if (__atomic_test_and_set(const_cast<u8*>(&g_saveLock), __ATOMIC_ACQUIRE)) return;   // another thread writes it now; the request stays
    g_saveTick = 0;
    static char buf[CFG_TEXT_MAX];
    int len = WriteConfigText(buf, sizeof buf);
    if (len > 0 && CfgSave(buf, len))   // (logs why not itself)
        RD_LOG("[oot_ragdoll] saved: ragdoll %s getup %s force %s", ButtonName(g_cfg.rag.mask), ButtonName(g_cfg.getup.mask), FORCES[g_cfg.force]);
    __atomic_clear(const_cast<u8*>(&g_saveLock), __ATOMIC_RELEASE);
}
// the menu's Save: everything as it is now
[[maybe_unused]] void SaveAll() { KeepAll(); SaveConfig(); }
// AI thread, every frame: write the file half a second after a request (the settings button's changes, an old file)
void SaveTick() {
    u64 t = g_saveTick;
    if (t && Ticks() - t > TickHz() / 2) SaveConfig();
}

// text of a settings file into the values; returns the lines that were not understood
int ReadConfigText(char* buf) {
    int version = 0, bad = 0;
    for (char* line = strtok(buf, "\r\n"); line; line = strtok(nullptr, "\r\n")) {
        char key[32], val[200];
        while (*line == ' ' || *line == '\t') line++;
        if (!line[0] || line[0] == '#') continue;
        if (sscanf(line, "%31[a-z0-9_] = %199[^#\r\n]", key, val) != 2) { bad++; RD_LOG("[oot_ragdoll] settings line not understood: %s", line); continue; }
        for (size_t n = strlen(val); n && (val[n - 1] == ' ' || val[n - 1] == '\t'); ) val[--n] = 0;
        if (!strcmp(key, "version")) { version = atoi(val); continue; }
        if (!strncmp(key, "voice", 5) && key[5] >= '1' && key[5] <= '4' && !key[6]) continue;   // the strength lists before 1.1.0
        if (!strncmp(key, "voicekickair_", 13) || !strncmp(key, "voicekickground_", 16)) continue;   // 1.1.0 test builds
        if (!strcmp(key, "voiceair") || !strcmp(key, "voiceground") || !strcmp(key, "voicestrong") || !strcmp(key, "strongforce")) continue;   // 1.1.0 test builds
        if (!strcmp(key, "menupad") || !strcmp(key, "listenvoice") || !strcmp(key, "setvoice")) continue;   // 1.1.0's settings button (gone in 1.1.1)
        // 1.1.0 names: the ragdoll button, the settings button (its double tap = next force level), its PC key
        if (!strcmp(key, "button")) snprintf(key, sizeof key, "ragdoll");
        else if (!strcmp(key, "menubutton")) snprintf(key, sizeof key, "nextforce");
        else if (!strcmp(key, "setkey")) snprintf(key, sizeof key, "forcekey");
        const Setting* found = nullptr;
        for (auto& s : SETTINGS) if (!strcmp(key, s.key)) { found = &s; break; }
        if (!found || !ParseSetting(*found, val)) { bad++; RD_LOG("[oot_ragdoll] settings line not understood: %s = %s", key, val); }
    }
    if (g_cfg.force < 0 || g_cfg.force > 4) g_cfg.force = D_CFG.force;
    if (g_cfg.menusize < MENU_SIZE_MIN) g_cfg.menusize = MENU_SIZE_MIN;
    if (version < CONFIG_VERSION) {  // older file: its voice lines are from an earlier build, take the current defaults
        memcpy(g_cfg.voicepress, D_CFG.voicepress, sizeof(g_cfg.voicepress));
        memcpy(g_cfg.voicefall, D_CFG.voicefall, sizeof(g_cfg.voicefall)); memcpy(g_cfg.voicegetup, D_CFG.voicegetup, sizeof(g_cfg.voicegetup));
        memcpy(g_cfg.voicehard, D_CFG.voicehard, sizeof(g_cfg.voicehard)); memcpy(g_cfg.voicelight, D_CFG.voicelight, sizeof(g_cfg.voicelight));
        memcpy(g_cfg.voiceslide, D_CFG.voiceslide, sizeof(g_cfg.voiceslide));
        memcpy(g_cfg.forcevoice, D_CFG.forcevoice, sizeof(g_cfg.forcevoice));
        g_cfg.impactvoice = D_CFG.impactvoice;
        RequestSave();
        RD_LOG("[oot_ragdoll] settings file version %d: voice defaults restored", version);
    }
    return bad;
}

void LoadConfig() {
    g_configLoaded = true;
    static char buf[CFG_TEXT_MAX];
    size_t size = 0;
    if (!CfgLoad(buf, sizeof(buf), &size)) return;   // (logs why itself)
    int bad = ReadConfigText(buf);
    KeepAll();
    RD_LOG("[oot_ragdoll] config: enabled %d ragdoll %s getup %s next force %s menu %s force %d, %d line(s) not understood", g_cfg.enabled,
           ButtonName(g_cfg.rag.mask), ButtonName(g_cfg.getup.mask), ButtonName(g_cfg.nextforce.mask), ButtonName(g_cfg.menu.mask), g_cfg.force, bad);
}
