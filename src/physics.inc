// oot-ragdoll, part of main.cpp: physics hooks: the start kick, then every frame kicks / push / spin / roll / impact voice

// direction and flip of the start kick, fixed once per ragdoll. He stands (no vertical speed of his own): the kick is
// lifted like a kick from the ground; out of a jump or fall it goes where the camera looks.
void StartKickAim() {
    float d[3]; Direction(d);
    Lifted(d, fabsf(g_vel[1]) < 2.f ? g_tune.kickLift : -90.f, g_startDir);
    g_spinScale = KickSpeed() / SPIN_REF_KICK;
    g_spinPending = g_tune.spinMode == 1 || g_tune.spinMode == 2;
}

// physics push block (runs while the start operation is active): the start kick via body +0x90, which the block then
// applies (it sets each body's speed along that vector). Switch: once, after the block's op loop. Hookshot: after each
// op-apply call of that frame, so every call writes the same again and the last one stands, as on the Switch.
void OnConsumer(uintptr_t cont) {
    if (g_state != ACTIVE || !g_ctrl || !g_firstKickOpen) return;
    if (cont != Rd<uintptr_t>(g_ctrl + 0x40)) return;
    Bodies b(cont);
    if (!b) return;
    if (!g_firstFrameDone) StartKickAim();
    float t = KickSpeed();
    for (s32 i = 0; i < b.n; i++) {
        float* v = P<float>(b.Body(i) + 0x90);
        float wi = Weight(i);  // limbs (weight 0) keep only his own speed and get dragged by the core
        for (int k = 0; k < 3; k++) v[k] = g_vel[k] + wi * t * g_startDir[k];
        v[6] = 0; v[7] = 0; v[8] = 0;  // +0xa8: drop the Weak list's spin
    }
    g_firstFrameDone = true;
}

// the push block did not run for him in the first two frames (an empty operation list: the block is skipped): the
// start kick by setting the velocities instead, so kicks and the push work in that ragdoll too
bool StartKickFallback(const Bodies& b) {
    if (++g_startWait < 2 || !b) return false;
    StartKickAim();
    float t = KickSpeed();
    for (s32 i = 0; i < b.n; i++) {
        uintptr_t h = b.Handle(i);
        if (!h) continue;
        float wi = Weight(i);
        Vec3 v{g_vel[0] + wi * t * g_startDir[0], g_vel[1] + wi * t * g_startDir[1], g_vel[2] + wi * t * g_startDir[2]};
        SetV(h, &v);
    }
    g_firstFrameDone = true;
    RD_LOG("[oot_ragdoll] start kick set directly (the push block did not run)");
    return true;
}

// body 0 (hips) after the game's physics step: speed, position, rest counter; impact = it lost a lot of speed in one
// frame (ground, wall)
void Measure(uintptr_t cont, uintptr_t h0) {
    Vec3 v = GetV(h0);
    float dx = v.x - g_impactPrev.x, dy = v.y - g_impactPrev.y, dz = v.z - g_impactPrev.z;
    float drop = sqrtf(dx * dx + dy * dy + dz * dz);
    float sp = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z), psp = sqrtf(g_impactPrev.x * g_impactPrev.x + g_impactPrev.y * g_impactPrev.y + g_impactPrev.z * g_impactPrev.z);
    // 1.2.0 (user: on a flight that got faster he hit something and screamed on): an impact is a sharp change of his
    // velocity (any direction: ground, wall, object) or a loss of IMPACT_REL of his speed in one or two frames (a hit
    // spread over two frames). Pushing into the ground/wall drops every frame: only the first frame of a hit counts,
    // and after one the next counts IMPACT_REARM frames later (bounces).
    bool same = g_impactPrevCont == cont;
    float ref = fmaxf(psp, g_impactSpeed2), lost = fmaxf(psp - sp, g_impactSpeed2 - sp);
    bool hit = same && g_lastDrop < IMPACT_MIN && (!g_impactAt || g_physFrames + 1 - g_impactAt >= IMPACT_REARM)
        && ((psp - sp > IMPACT_MIN * 0.5f && drop > IMPACT_MIN) || (lost >= IMPACT_MIN * 0.5f && lost >= IMPACT_REL * ref));
    if (hit) {
        float size = fmaxf(drop, lost);
        // 1.1.0: from the air (landing, a wall in flight): hard or light by the speed lost; while he already lies: sliding
        // (1.1.0 user: a drop of 2x hard or more is a landing even when the air counter missed the flight: a 52 m/s hit
        // right after a kick counted as sliding, silent). 1.2.0: so is a hard one that no push caused (a quick flight
        // shortly after the press: the air counter starts late)
        bool fromAir = g_airFrames >= AIR_FRAMES || size >= 2 * IMPACT_HARD || (size >= IMPACT_HARD && !Pushing());
        if (fromAir) {
            // a real hit: ends the scream at once (no wait for the last voice), the landing list per the settings; the
            // falling voice needs a new fall (g_airFrames from 0, FALL_AFTER_HIT)
            g_impactAt = g_physFrames + 1;
            g_airFrames = 0; g_fallSince = 0; g_fallLast = 0;
            char why[64]; snprintf(why, sizeof why, "impact %.1f -> %.1f m/s", ref, sp);
            if (g_cfg.impactvoice) SayWhen(size >= IMPACT_HARD ? V_HARD : V_LIGHT, size, why, true);
            else RD_LOG("[oot_ragdoll] %s: %.1f m/s, impact voice off, voices cut (%u waiting dropped)", why, size, SayCut(nullptr));
        } else if (g_cfg.impactvoice && Ticks() - g_lastSayTick > TickHz() * 4 / 10) {
            g_impactAt = g_physFrames + 1;
            SayWhen(V_SLIDE, size, "impact");   // (dragged along: waits for the last voice, as before)
        }
    }
    // 1.1.0 (user): falling voice: in the air and his hips fall faster than fall_speed. The waits count from the last
    // voice that played, of any kind: the first one fall_delay after the fall began or the last voice (a kick's scream),
    // then again every fall_repeat after the last voice. Nothing while a voice still waits in the queue
    {
        u64 now = Ticks();
        bool falling = g_cfg.kickvoice && g_airFrames >= AIR_FRAMES && v.y < -g_cfg.fallspeed;
        if (!falling) { g_fallSince = 0; g_fallLast = 0; }
        else {
            if (!g_fallSince) g_fallSince = now;
            u64 hz = TickHz(), lastPlay = g_lastPlayTick;
            u64 from = !g_fallLast && g_fallSince > lastPlay ? g_fallSince : lastPlay;
            float wait = !g_fallLast ? g_cfg.falldelay : g_cfg.fallrepeat;
            if (!g_fallLast && g_impactAt) wait = fmaxf(wait, FALL_AFTER_HIT);   // 1.2.0: after a hit, a clear new fall first
            bool due = (!g_fallLast || g_cfg.fallrepeat > 0) && now - from >= static_cast<u64>(wait * hz);
            if (due && !SayBusy()) {
                g_fallLast = now;
                SayWhen(V_FALL, -v.y, "falling");
            }
        }
    }
    g_lastDrop = same ? drop : 0.f;
    g_impactSpeed2 = same ? psp : 0.f;
    g_impactPrev = v; g_impactPrevCont = cont; g_bodySpeed = sp;  // (push/kick overwrite prev with the velocity they set)
    PosOf(h0, g_bodyPos);
    // rest counter for Grounded: up while his hips have no vertical speed, down twice as fast otherwise
    u32 cap = 2 * (g_tune.restFrames ? g_tune.restFrames : 1);
    if (fabsf(v.y) < 1.f) { if (g_restFrames < cap) g_restFrames++; }
    else g_restFrames = g_restFrames > 2 ? g_restFrames - 2 : 0;
}

// log teleports while ragdolled (open: the rare mid-ragdoll "lag + snap", FINDINGS "Snap-back")
void SnapLog() {
    Vec3 a{Rd<float>(g_actor + 0x31c), Rd<float>(g_actor + 0x320), Rd<float>(g_actor + 0x324)};
    Vec3 b{g_bodyPos[0], g_bodyPos[1], g_bodyPos[2]};
    auto dist = [](Vec3 p, Vec3 q) { return sqrtf((p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y) + (p.z - q.z) * (p.z - q.z)); };
    float ja = dist(a, g_snapActor), jb = dist(b, g_snapBody);
    if (g_physFrames > 2 && !g_warpPending && (ja > 3.f || jb > 4.f))
        RD_LOG("[oot_ragdoll] snap: actor %.1f m, body %.1f m in one frame (actor %.1f %.1f %.1f body %.1f %.1f %.1f)", ja, jb, a.x, a.y, a.z, b.x, b.y, b.z);
    g_snapActor = a; g_snapBody = b;
}

// body-part map: offset of each body from body 0 in his frame (right, up, forward), 8 bodies per log line
void LogBodyMap(const Bodies& b) {
    float fx = sinf(g_facing), fz = cosf(g_facing);
    char line[320]; int o = 0;
    for (s32 i = 0; i < b.n; i++) {
        uintptr_t h = b.Handle(i);
        if (h) {
            float p[3]; PosOf(h, p);
            float r[3] = {p[0] - g_bodyPos[0], p[1] - g_bodyPos[1], p[2] - g_bodyPos[2]};
            o += snprintf(line + o, sizeof(line) - o, " %d:%.2f,%.2f,%.2f", i, r[0] * fz - r[2] * fx, r[1], r[0] * fx + r[2] * fz);
        }
        if (o && (i % 8 == 7 || i == b.n - 1)) { RD_LOG("[oot_ragdoll] bodies (right,up,fwd from body 0):%s", line); o = 0; }
    }
}

// on the ground: the left stick moves him toward where it points (camera + stick). The part of that direction across
// his body log-rolls him about his spine, the part along his body drags him head or feet first (rollAlong).
// raw = unit vector hips -> head on the ground plane (x, z), d = aimed direction, tilt = stick 0..1.
// 0.10.0: every body is driven to the roll rate by itself and about the line of his spine. Before, the hips alone were
// measured and all bodies got the same push about their centre: with the hips already turning nothing was added and
// the rest of him stayed behind, so the roll came and went. The rate is full from 30 degrees off his body axis
// (it was scaled by the angle), and his spine direction is smoothed (it wobbles while he rolls).
void GroundRoll(const Bodies& b, const float raw[2], const float d[3], float tilt) {
    if (!g_spineOk) { g_spine[0] = raw[0]; g_spine[1] = raw[1]; g_spineOk = true; }
    else {
        float x = g_spine[0] + 0.25f * (raw[0] - g_spine[0]), z = g_spine[1] + 0.25f * (raw[1] - g_spine[1]), l = sqrtf(x * x + z * z);
        if (l > 0.3f) { g_spine[0] = x / l; g_spine[1] = z / l; } else { g_spine[0] = raw[0]; g_spine[1] = raw[1]; }   // he turned over end: take the new one
    }
    const float sx = g_spine[0], sz = g_spine[1];
    float h = sqrtf(d[0] * d[0] + d[2] * d[2]);
    if (h <= 0.1f) return;   // camera straight up / down: no direction on the ground
    float ux = d[0] / h, uz = d[2] / h;
    float al = ux * sx + uz * sz;             // along his body (+ = toward his head)
    float ac = ux * sz - uz * sx;             // across it, on the side up x spine = (sz, -sx)
    bool roll = fabsf(ac) > 0.2f, drag = g_tune.rollAlong > 0 && fabsf(al) > 0.3f;
    if (!roll && !drag) return;
    float side = ac >= 0 ? 1.f : -1.f;
    float cx = side * sz, cz = -side * sx;    // unit direction he rolls toward
    float ax[3] = {cz, 0, -cx};               // up x that direction = along his spine: his top goes toward the stick
    float top = roll ? g_tune.rollMax * tilt * fminf(1.f, fabsf(ac) * 2.f) : 0.f;
    float travel = top * g_tune.rollRadius;   // travel that matches the roll (no slipping)
    float along = drag ? g_tune.rollAlong * tilt * al : 0.f, adir = along > 0 ? 1.f : -1.f;
    for (s32 i = 0; i < b.n; i++) {
        uintptr_t hb = b.Handle(i);
        if (!hb) continue;
        Vec3 v = GetV(hb);
        if (roll) {
            Vec3 w = GetW(hb);
            float o = fminf(g_tune.spinPush, top - (w.x * ax[0] + w.z * ax[2]));
            if (o > 0) {
                w.x += o * ax[0]; w.z += o * ax[2];
                SetW(hb, &w);
                float p[3]; PosOf(hb, p);
                float r[3] = {p[0] - g_bodyPos[0], p[1] - g_bodyPos[1], p[2] - g_bodyPos[2]};   // from the spine line (through his hips)
                float ra = r[0] * ax[0] + r[2] * ax[2];
                r[0] -= ra * ax[0]; r[2] -= ra * ax[2];
                v.x += o * (ax[1] * r[2] - ax[2] * r[1]); v.y += o * (ax[2] * r[0] - ax[0] * r[2]); v.z += o * (ax[0] * r[1] - ax[1] * r[0]);
            }
            float vu = v.x * cx + v.z * cz;
            if (vu < travel) { float k = fminf(travel - vu, g_tune.spinPush * g_tune.rollRadius); v.x += k * cx; v.z += k * cz; }
        }
        if (drag) {
            float va = v.x * sx + v.z * sz;
            if (va * adir < along * adir) { float k = (along - va) * 0.2f; v.x += k * sx; v.z += k * sz; }
        }
        SetV(hb, &v);
    }
}

// spin: a kick flips him (toward the kick, or about a random axis); with stickSpin the left stick spins him in the air
// (flip toward the stick, up to spinMax) and rolls him on the ground. flip = a kick may flip him now (spin mode, spin
// button); steer = the stick may spin him now (stickSpin, spin button).
void Spin(const Bodies& b, uintptr_t h0, const float d[3], bool burst, bool grounded, bool flip, bool steer, float stick) {
    bool kick = g_spinPending || burst;
    bool wanted = g_spinPending || (burst && flip) || (steer && stick > 0.2f);
    if (!wanted || !h0) return;
    if (!kick && grounded && g_tune.headBody < static_cast<u32>(b.n)) {
        uintptr_t hh = b.Handle(g_tune.headBody);
        float hp[3] = {0, 0, 0};
        if (hh) PosOf(hh, hp);
        float sp[2] = {hp[0] - g_bodyPos[0], hp[2] - g_bodyPos[2]}, spl = sqrtf(sp[0] * sp[0] + sp[1] * sp[1]);
        if (hh && spl > 0.15f) {   // (he stands on his head: no spine direction on the ground, the air spin below)
            sp[0] /= spl; sp[1] /= spl;
            GroundRoll(b, sp, d, fminf(1.f, stick));
            return;
        }
    }
    float omega = kick ? g_tune.spinKick * (g_spinPending ? g_spinScale : KickSpeed() / SPIN_REF_KICK) : g_tune.spinPush;
    g_spinPending = false;
    if (omega <= 0) return;
    float ax[3]; SpinAxis(d, kick && g_tune.spinMode == 2, ax);
    Vec3 cw = GetW(h0);
    float cur = cw.x * ax[0] + cw.y * ax[1] + cw.z * ax[2];
    // kicks add up to spinKickMax, the stick up to spinMax at full tilt
    float top = kick ? g_tune.spinKickMax : g_tune.spinMax * fminf(1.f, stick);
    omega = fminf(omega, top - cur);
    if (omega > 0) AddSpin(b, ax, omega);
}

// brake: stick released on the ground -> damp his spin for 1 s (60 physics frames)
void SpinBrake(const Bodies& b, bool steering, bool grounded) {
    if (steering) { g_steerFrames++; g_sinceSteer = 0; } else if (g_sinceSteer < 999) g_sinceSteer++;
    if (!steering && g_steerFrames && g_sinceSteer < 60 && g_tune.spinBrake < 1.f && grounded) {
        for (s32 i = 0; i < b.n; i++) {
            uintptr_t h = b.Handle(i);
            if (!h) continue;
            Vec3 a = GetW(h); a.x *= g_tune.spinBrake; a.y *= g_tune.spinBrake; a.z *= g_tune.spinBrake;
            SetW(h, &a);
        }
    }
    if (g_sinceSteer >= 60) g_steerFrames = 0;
}

// kick (burst) and hold push: set the core bodies' velocities directly (the game's consumer math, main+0x158524; the
// push block itself is skipped while he lies, FINDINGS "Push while lying"). u = the container's ground motion.
//   kick: his speed along the kick direction goes up to the kick speed. Already that fast: the kick still adds
//         kickStack of it, until kickStackMax kick speeds.
//   push: his speed along the push direction is kept at the push speed at least; pushGrip takes away sideways drift.
// On the ground both directions are lifted (kickLift / pushLift), in the air they are the camera's.
void KickPush(uintptr_t cont, const Bodies& b, const float d[3], bool burst, bool push, bool grounded) {
    float t = burst ? KickSpeed() : 0.f, s = PushSpeed();
    float dk[3], dp[3];
    Lifted(d, grounded ? g_tune.kickLift : -90.f, dk);
    Lifted(d, grounded ? g_tune.pushLift : -90.f, dp);
    float u[3] = {Rd<float>(cont + 0xac), Rd<float>(cont + 0xb0), Rd<float>(cont + 0xb4)};
    float nuk = dk[0] * u[0] + dk[1] * u[1] + dk[2] * u[2], nup = dp[0] * u[0] + dp[1] * u[1] + dp[2] * u[2];
    float ph = sqrtf(dp[0] * dp[0] + dp[2] * dp[2]);
    float px = ph > 0.1f ? dp[0] / ph : 0.f, pz = ph > 0.1f ? dp[2] / ph : 0.f;
    for (s32 i = 0; i < b.n; i++) {
        uintptr_t h = b.Handle(i);
        if (!h) continue;
        float wi = Weight(i);
        if (wi <= 0) continue;                                 // limb: dragged along by the joints
        Vec3 w = GetV(h);
        if (burst) {
            float nv = dk[0] * w.x + dk[1] * w.y + dk[2] * w.z;
            float k = fmaxf(0.f, wi * t + nuk - nv);
            float least = wi * t * g_tune.kickStack;
            if (k < least) k = fmaxf(k, fminf(least, wi * t * g_tune.kickStackMax + nuk - nv));
            w.x += k * dk[0]; w.y += k * dk[1]; w.z += k * dk[2];
        }
        if (push) {
            float nv = dp[0] * w.x + dp[1] * w.y + dp[2] * w.z;
            float along = wi * s + nup;                        // at least s along the push (+ ground motion)
            if (nv < along) { float k = along - nv; w.x += k * dp[0]; w.y += k * dp[1]; w.z += k * dp[2]; }
            if (g_tune.pushGrip > 0 && ph > 0.1f) {             // sideways (horizontal, across the push) drift
                float a = w.x * px + w.z * pz, g = fminf(g_tune.pushGrip, 1.f);
                w.x -= g * (w.x - a * px); w.z -= g * (w.z - a * pz);
            }
        }
        SetV(h, &w);
        if (i == 0) { g_impactPrev = w; g_impactSpeed2 = 0; }  // next frame's impact check compares with what the push/kick set
    }
}

// after the push block, every physics frame (cont = the container whose update this is)
void OnPhysAfter(uintptr_t cont) {
    if (g_state != ACTIVE || !g_ctrl) return;
    if (cont != Rd<uintptr_t>(g_ctrl + 0x40)) return;
    Bodies b(cont);
    if (!g_firstFrameDone && !StartKickFallback(b)) return;
    g_firstKickOpen = false;
    LyingTick();
    bool burst = g_burstPending, push = Pushing();
    uintptr_t h0 = b ? b.Handle(0) : 0;
    if (h0) Measure(cont, h0);
    if (h0 && g_actor) { SnapLog(); g_physFrames = g_physFrames + 1; }
    if (g_tune.logMap && g_physFrames == 6 && h0) LogBodyMap(b);
    if (g_state != ACTIVE || !b) return;
    float d[3]; Direction(d);
    // on the ground, and for a fifth of a second after the tests stop saying so (rolling over an arm lifts his hips):
    // without that the stick switched between roll and air flip in the middle of a roll. A kick ends it at once.
    bool grounded = Grounded();
    if (grounded) g_groundHold = 12;
    else if (burst) g_groundHold = 0;
    else if (g_groundHold) { g_groundHold--; grounded = true; }
    // in the air / on the ground for the voices (SayWhen): a few frames on the ground end a flight
    if (grounded) { if (++g_groundRun >= 4) g_airFrames = 0; }
    else { g_groundRun = 0; if (g_airFrames < 1000000) g_airFrames++; }
    // 0.10.1: the stick has its own switch, stickSpin (on by default). Before, spin mode off also stopped the stick, so
    // the menu's "Kicks flip him" switched both
    bool button = (!g_tune.spinButton && !g_cfg.spinkey) || (g_tune.spinButton && (g_heldPlay & g_tune.spinButton) == g_tune.spinButton) || (g_heldPlay & KB_SPIN);   // (1.1.0: or its PC key; 1.1.1: all its buttons)
    bool flip = g_tune.spinMode == 1 || g_tune.spinMode == 2 || (g_tune.spinMode == 3 && button);
    bool steer = g_tune.stickSpin && button;
    float stick = sqrtf((float)g_stickX * g_stickX + (float)g_stickY * g_stickY) / 32767.f;
    bool steering = steer && stick > 0.2f;
    if (!steering) g_spineOk = false;   // the smoothed spine direction starts fresh with the next roll
    if (g_tune.logRag && h0 && g_physFrames % 10 == 0) {
        Vec3 w = GetW(h0);
        RD_LOG("[oot_ragdoll] rag stick f%u tilt %.2f spin %s%s %s, hips turn %.1f rad/s, speed %.1f m/s%s%s", g_physFrames, stick,
            g_tune.stickSpin ? "on" : "off", g_tune.stickSpin && !button ? " (button up)" : "", grounded ? "ground" : "air",
            sqrtf(w.x * w.x + w.y * w.y + w.z * w.z), g_bodySpeed, burst ? ", kick" : "", push ? ", push" : "");
    }
    Spin(b, h0, d, burst, grounded, flip, steer, stick);
    SpinBrake(b, steering, grounded);
    if (!burst && !push) return;
    g_burstPending = false;
    KickPush(cont, b, d, burst, push, grounded);
}
