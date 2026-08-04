#ifndef SOUND_H
#define SOUND_H
// ============================================================
//  SOUND.H — every sound effect, SYNTHESIZED IN CODE.
//
//  Same idea as the 8x8 sprites in gamedata.h: there are no asset
//  files and no sound designer. A sound here is a few numbers —
//  pitch, how fast the pitch falls, how much noise is mixed in, how
//  fast it fades — and the samples are generated into a buffer at
//  startup. Change a number, hear the difference.
//
//  C CONCEPT — digital audio: a sound is just an array of numbers
//  (SAMPLE_RATE of them per second), each one the speaker cone's
//  position at that instant. A sine wave makes a tone; random values
//  make noise; multiplying by a falling number makes it fade out.
//
//  Depends on: raylib only. Sits at the very bottom of the include
//  order, so anything may make a noise.
// ============================================================

#include <math.h>
#include <stdlib.h>
#include "raylib.h"

#define SFX_SAMPLE_RATE 22050

typedef enum {
    SFX_TREE_BREAK = 0,   // splintering wood
    SFX_ROCK_BREAK,       // stone crumbling
    SFX_MOB_CHIRP,        // the natives, chattering somewhere nearby
    SFX_MOB_DIE,          // ...and one of them not chattering any more
    SFX_HURT,             // that was you
    SFX_PICKUP,           // something went in your pocket
    SFX_CRAFT_DONE,       // a build finished
    SFX_MACHINE_FAIL,     // a machine wore out and died
    SFX_PLACE,            // a block went down
    SFX_COUNT
} SfxID;

static Sound sfx[SFX_COUNT];
static bool  sfxReady = false;
// Anti-spam: the same sound can't retrigger faster than this, which
// is what stops a collapsing rock face from becoming a buzzsaw.
static float sfxCooldown[SFX_COUNT];
static const float SFX_MIN_GAP[SFX_COUNT] = {
    [SFX_TREE_BREAK]   = 0.06f,
    [SFX_ROCK_BREAK]   = 0.06f,
    [SFX_MOB_CHIRP]    = 0.35f,
    [SFX_MOB_DIE]      = 0.05f,
    [SFX_HURT]         = 0.25f,
    [SFX_PICKUP]       = 0.08f,
    [SFX_CRAFT_DONE]   = 0.05f,
    [SFX_MACHINE_FAIL] = 0.10f,
    [SFX_PLACE]        = 0.05f,
};

// ─── The synthesizer ──────────────────────────────────────
// One recipe makes one sound:
//   seconds   how long it lasts
//   hz0 → hz1 the pitch it slides between (falling = a thud,
//             rising = a chirp)
//   noise     0 = pure tone, 1 = pure hiss (wood and stone are hiss)
//   decay     how sharply it fades (bigger = snappier)
//   crunch    quantizes the wave — cheap grit, makes it sound
//             8-bit rather than clean
static Sound SfxSynth(float seconds, float hz0, float hz1,
                      float noise, float decay, float crunch) {
    int count = (int)(SFX_SAMPLE_RATE * seconds);
    if (count < 1) count = 1;
    short *samples = (short *)RL_MALLOC((size_t)count * sizeof(short));
    if (samples == NULL) return (Sound){ 0 };

    float phase = 0;
    for (int i = 0; i < count; i++) {
        float t = (float)i / count;                       // 0..1 through the sound
        float hz = hz0 + (hz1 - hz0) * t;
        phase += 2.0f * PI * hz / SFX_SAMPLE_RATE;
        float tone = sinf(phase);
        float hiss = (float)rand() / (RAND_MAX / 2.0f) - 1.0f;
        float v = tone * (1.0f - noise) + hiss * noise;
        v *= expf(-decay * t);                            // the fade
        if (crunch > 0) {                                 // step it into levels
            float steps = 32.0f * (1.0f - crunch) + 3.0f;
            v = roundf(v * steps) / steps;
        }
        if (v > 1) v = 1;
        if (v < -1) v = -1;
        samples[i] = (short)(v * 20000.0f);
    }

    Wave wave = { (unsigned int)count, SFX_SAMPLE_RATE, 16, 1, samples };
    Sound s = LoadSoundFromWave(wave);
    RL_FREE(samples);
    return s;
}

// Called once, AFTER the window exists (the audio device needs it).
static void SfxInit(void) {
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return;
    //                          len    hz0    hz1   noise decay crunch
    sfx[SFX_TREE_BREAK]   = SfxSynth(0.38f,  420,   90,  0.80f,  5.0f, 0.5f);
    sfx[SFX_ROCK_BREAK]   = SfxSynth(0.30f,  260,   60,  0.92f,  6.0f, 0.6f);
    sfx[SFX_MOB_CHIRP]    = SfxSynth(0.16f,  300,  680,  0.25f,  4.0f, 0.7f);
    sfx[SFX_MOB_DIE]      = SfxSynth(0.28f,  520,   70,  0.55f,  5.5f, 0.6f);
    sfx[SFX_HURT]         = SfxSynth(0.22f,  180,   70,  0.45f,  6.0f, 0.4f);
    sfx[SFX_PICKUP]       = SfxSynth(0.09f,  700, 1150,  0.05f,  6.0f, 0.3f);
    sfx[SFX_CRAFT_DONE]   = SfxSynth(0.14f,  540,  900,  0.05f,  4.5f, 0.3f);
    sfx[SFX_MACHINE_FAIL] = SfxSynth(0.55f,  240,   40,  0.65f,  3.0f, 0.7f);
    sfx[SFX_PLACE]        = SfxSynth(0.10f,  200,  120,  0.60f,  8.0f, 0.6f);
    for (int i = 0; i < SFX_COUNT; i++) sfxCooldown[i] = 0;
    sfxReady = true;
}

static void SfxShutdown(void) {
    if (!sfxReady) return;
    for (int i = 0; i < SFX_COUNT; i++) UnloadSound(sfx[i]);
    CloseAudioDevice();
    sfxReady = false;
}

static void SfxUpdate(float dt) {
    for (int i = 0; i < SFX_COUNT; i++)
        if (sfxCooldown[i] > 0) sfxCooldown[i] -= dt;
}

// Play one, with a random pitch wobble so repeats don't sound like a
// machine gun of identical clicks.
static void SfxPlay(SfxID id, float volume, float pitchVary) {
    if (!sfxReady || id < 0 || id >= SFX_COUNT) return;
    if (sfxCooldown[id] > 0) return;
    sfxCooldown[id] = SFX_MIN_GAP[id];
    if (volume < 0) volume = 0;
    if (volume > 1) volume = 1;
    SetSoundVolume(sfx[id], volume);
    SetSoundPitch(sfx[id], 1.0f + pitchVary * (GetRandomValue(-100, 100) / 1000.0f));
    PlaySound(sfx[id]);
}

// The common case: a noise happening somewhere in the world, quieter
// the farther away it is. `listener` is the player.
static void SfxPlayAt(SfxID id, Vector2 at, Vector2 listener, float volume) {
    float d = Vector2Distance(at, listener);
    float falloff = 1.0f - d / 900.0f;      // silent beyond ~37 tiles
    if (falloff <= 0.02f) return;
    SfxPlay(id, volume * falloff, 1.5f);
}

#endif // SOUND_H
