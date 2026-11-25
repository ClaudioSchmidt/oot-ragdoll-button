// oot-ragdoll, part of main.cpp: speed camera
// speed camera: while he flies, the game camera pulls back and widens a little (scaled around the target, restored after).
// Pre runs before the camera update; true = the eye / FOV were changed, Post (after the update) puts them back.
double g_camEye[3]; float g_camFov;   // the game's own values, kept between Pre and Post
bool OnCamApplyPre(uintptr_t mgr) {
    {
        static float k;       // smoothed effect 0..1
        static u64 last;
        uintptr_t c = Rd<uintptr_t>(Main() + OFF_CAMERA);
        c = c ? Rd<uintptr_t>(c) : 0;
        c = c ? Rd<uintptr_t>(c + 0x248) : 0;
        uintptr_t view = c ? Rd<uintptr_t>(c + 8) : 0;
        uintptr_t mv = Rd<uintptr_t>(mgr + 0x18);
        if (!view || !mv || Rd<uintptr_t>(mv + 8) != view) return false;
        u64 now = Ticks();
        float dt = last ? fminf((float)(now - last) / TickHz(), 0.1f) : 0.f;
        last = now;
        float span = fmaxf(g_tune.camHi - g_tune.camLo, 0.1f);
        float want = g_state == ACTIVE ? fminf(fmaxf((g_bodySpeed - g_tune.camLo) / span, 0.f), 1.f) : 0.f;
        float tc = want > k ? g_tune.camIn : g_tune.camOut;
        k += (want - k) * (tc > 0.01f ? fminf(dt / tc, 1.f) : 1.f);
        if (k < 0.001f) { k = 0; return false; }
        double* eye = P<double>(mgr + 0xe8);
        const double* at = P<double>(mgr + 0x110);
        float* fov = P<float>(mgr + 0x13c);
        double e0 = eye[0], e1 = eye[1], e2 = eye[2];
        float f = *fov;
        g_camEye[0] = e0; g_camEye[1] = e1; g_camEye[2] = e2; g_camFov = f;
        double m = 1.0 + g_tune.camDist * k;
        double d[3] = {e0 - at[0], e1 - at[1], e2 - at[2]};
        double len = sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]), back = (m - 1.0) * (1.0 - g_tune.camLift);
        for (int i = 0; i < 3; i++) eye[i] += d[i] * back;
        eye[1] += len * (m - 1.0) * g_tune.camLift;  // crane up: behind him the ground often rises, above him it doesn't
        if (eye[1] < e1) eye[1] = e1;
        *fov = fminf(f + g_tune.camFov * k * 0.01745329f, 2.6f);
        return true;
    }
}
void OnCamApplyPost(uintptr_t mgr) {
    double* eye = P<double>(mgr + 0xe8);
    eye[0] = g_camEye[0]; eye[1] = g_camEye[1]; eye[2] = g_camEye[2];
    *P<float>(mgr + 0x13c) = g_camFov;  // the game's own camera state stays untouched
}
