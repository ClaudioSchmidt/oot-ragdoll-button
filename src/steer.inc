// oot-ragdoll, part of main.cpp: aim direction, spin and body velocity helpers

// throw/push direction: camera forward (yaw + pitch), turned by the left stick
void Direction(float out[3]) {
    out[0] = 0; out[1] = 0; out[2] = 0;
    uintptr_t p = Rd<uintptr_t>(Main() + OFF_CAMERA);
    for (int off : {0, 0x248, 8}) { if (!p) break; p = Rd<uintptr_t>(p + off); }
    float yaw = g_facing, pitch = 0;
    if (p) {
        float fx = Rd<float>(p + 0x28), fy = Rd<float>(p + 0x2c), fz = Rd<float>(p + 0x30);
        float len = sqrtf(fx * fx + fy * fy + fz * fz);
        if (len > 1e-3f) { yaw = atan2f(-fx, -fz); pitch = asinf(fminf(1.f, fmaxf(-1.f, -fy / len))); }
    }
    float sx = g_stickX / 32767.f, sy = g_stickY / 32767.f;
    if (sx * sx + sy * sy > 0.04f) yaw += atan2f(-sx, sy);
    out[0] = cosf(pitch) * sinf(yaw); out[1] = sinf(pitch); out[2] = cosf(pitch) * cosf(yaw);
}

// d with its pitch raised to at least minDeg (same heading); -90 and below = d as it is.
// The camera usually looks down at him, so an unlifted kick from the ground goes into the ground.
void Lifted(const float d[3], float minDeg, float out[3]) {
    out[0] = d[0]; out[1] = d[1]; out[2] = d[2];
    if (minDeg <= -90.f) return;
    float h = sqrtf(d[0] * d[0] + d[2] * d[2]);
    float want = fminf(minDeg, 89.f) * 0.01745329f;
    if (h < 1e-3f || atan2f(d[1], h) >= want) return;   // straight up / down the camera: no heading to keep
    out[0] = d[0] / h * cosf(want); out[1] = sinf(want); out[2] = d[2] / h * cosf(want);
}

float Weight(s32 i) { return g_tune.forceMode == 0 ? 1.f : (i < 32 ? g_tune.w[i] : 0.f); }

// spin axis (unit) for a spin toward d (horizontal part; his facing when d is vertical): up x forward, so his head
// goes toward d. random: any axis (a kick with spin mode "random")
void SpinAxis(const float d[3], bool random, float ax[3]) {
    float h = sqrtf(d[0] * d[0] + d[2] * d[2]);
    float fx = h > 0.1f ? d[0] / h : sinf(g_facing), fz = h > 0.1f ? d[2] / h : cosf(g_facing);
    ax[0] = fz; ax[1] = 0; ax[2] = -fx;
    if (!random) return;
    u64 r = Ticks() * 0x9E3779B97F4A7C15ull;
    float rx = (float)((r >> 8) & 0xffff) / 32768.f - 1.f, ry = (float)((r >> 24) & 0xffff) / 32768.f - 1.f,
          rz = (float)((r >> 40) & 0xffff) / 32768.f - 1.f;
    float l = sqrtf(rx * rx + ry * ry + rz * rz);
    if (l > 1e-3f) { ax[0] = rx / l; ax[1] = ry / l; ax[2] = rz / l; }
}

// ---- his ragdoll bodies: container +0x60 count, +0x68 list (stride 0x138); body +0x38 = handle, +0x90 pending linear,
// +0xa8 pending angular; handle +0x8c / +0x9c / +0xac = world position ----
struct Bodies {
    uintptr_t list = 0; s32 n = 0;
    explicit Bodies(uintptr_t cont) {
        s32 c = Rd<s32>(cont + 0x60);
        uintptr_t l = Rd<uintptr_t>(cont + 0x68);
        if (l && c > 0 && c <= 64) { list = l; n = c; }
    }
    explicit operator bool() const { return n > 0; }
    uintptr_t Body(s32 i) const { return list + i * 0x138; }
    uintptr_t Handle(s32 i) const { return Rd<uintptr_t>(Body(i) + 0x38); }
};
inline void PosOf(uintptr_t h, float p[3]) { p[0] = Rd<float>(h + 0x8c); p[1] = Rd<float>(h + 0x9c); p[2] = Rd<float>(h + 0xac); }

// body handle velocity access (game calls: the Vec3 in goes through guest memory)
Vec3 GetV(uintptr_t h) { return GCallV3(Main() + OFF_BODY_GETV, h); }
Vec3 GetW(uintptr_t h) { return GCallV3(Main() + OFF_BODY_GETW, h); }
void SetV(uintptr_t h, const Vec3* v) { GCall(Main() + OFF_BODY_SETV, h, ToGuest(v, sizeof *v), u32(0)); }
void SetW(uintptr_t h, const Vec3* v) { GCall(Main() + OFF_BODY_SETW, h, ToGuest(v, sizeof *v), u32(0)); }

// add a whole-body spin of rate omega (rad/s) around axis through the bodies' centre: v += w x (p - c), angular += w
void AddSpin(const Bodies& b, const float ax[3], float omega) {
    float c[3] = {0, 0, 0}; int m = 0;
    for (s32 i = 0; i < b.n; i++) {
        uintptr_t h = b.Handle(i);
        if (!h) continue;
        float p[3]; PosOf(h, p);
        c[0] += p[0]; c[1] += p[1]; c[2] += p[2]; m++;
    }
    if (!m) return;
    for (int k = 0; k < 3; k++) c[k] /= m;
    float w[3] = {ax[0] * omega, ax[1] * omega, ax[2] * omega};
    for (s32 i = 0; i < b.n; i++) {
        uintptr_t h = b.Handle(i);
        if (!h) continue;
        float p[3]; PosOf(h, p);
        float r[3] = {p[0] - c[0], p[1] - c[1], p[2] - c[2]};
        Vec3 v = GetV(h);
        v.x += w[1] * r[2] - w[2] * r[1]; v.y += w[2] * r[0] - w[0] * r[2]; v.z += w[0] * r[1] - w[1] * r[0];
        SetV(h, &v);
        Vec3 a = GetW(h); a.x += w[0]; a.y += w[1]; a.z += w[2];
        SetW(h, &a);
    }
}

// lying on the ground. Test 1: hips low over his capsule, capsule close (the capsule rests on the ground under him).
// Test 2 (restFrames > 0): his hips had no vertical speed for a while. The capsule only chases the ragdoll at ~30 m/s
// and gets stuck on terrain (FINDINGS "Snap-back"), so after a long flight test 1 says "in the air" while he lies.
bool Grounded() {
    if (g_tune.restFrames && g_restFrames >= g_tune.restFrames) return true;
    if (!g_actor) return false;
    float ay = Rd<float>(g_actor + 0x320), dx = Rd<float>(g_actor + 0x31c) - g_bodyPos[0], dz = Rd<float>(g_actor + 0x324) - g_bodyPos[2];
    return g_bodyPos[1] - ay < g_tune.groundGap && dx * dx + dz * dz < 1.5f * 1.5f;
}
