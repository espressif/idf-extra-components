/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Self-playing fireworks scene: night sky, city skyline, rockets and particle
 * bursts. Immediate-mode raylib only, no assets, no input.
 */

#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "raylib.h"
#include "fireworks.h"

#define GRAVITY        90.0f
#define MAX_ROCKETS    4
#define MAX_PARTICLES  224
#define MAX_STARS      40
#define MAX_BUILDINGS  16

typedef struct {
    Vector2 pos;
    Vector2 vel;
    float fuse;    /* seconds of ascent left */
    float hue;
} rocket_t;

typedef struct {
    Vector2 pos;
    Vector2 vel;
    float life;
    float max_life;
    float hue;     /* negative draws white, for the burst flash */
    float radius;
} particle_t;

typedef struct {
    int x, w, h;
} building_t;

typedef struct {
    int x, y;
    float phase, speed;
} star_t;

/* Fixed-step simulation clock: the show must advance with wall time whatever
 * the frame rate does. */
#define SIM_STEP  (1.0f / 30.0f)
#define SIM_MAX_STEPS 8

static int s_width;
static int s_height;

static rocket_t s_rockets[MAX_ROCKETS];
static particle_t s_particles[MAX_PARTICLES];
static building_t s_buildings[MAX_BUILDINGS];
static star_t s_stars[MAX_STARS];
static int s_building_count;
static float s_next_launch;
static float s_sim_accum;

static unsigned hash32(unsigned x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static void sim_step(float dt);

static void spawn_rocket(rocket_t *r)
{
    float speed = (float)GetRandomValue(150, 205);
    r->pos.x = (float)GetRandomValue(40, s_width - 40);
    r->pos.y = (float)s_height - 24.0f;
    r->vel = (Vector2) {
        GetRandomValue(-20, 20), -speed
    };
    r->hue = (float)GetRandomValue(0, 359);
    r->fuse = speed / GRAVITY * GetRandomValue(82, 100) / 100.0f;
}

static particle_t *free_particle(void)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (s_particles[i].life <= 0.0f) {
            return &s_particles[i];
        }
    }
    return NULL;
}

static void burst(Vector2 at, float hue)
{
    const int count = GetRandomValue(44, 64);
    const float speed = (float)GetRandomValue(55, 95);

    for (int i = 0; i < count; i++) {
        particle_t *p = free_particle();
        if (p == NULL) {
            return;
        }
        float angle = (2.0f * PI * i) / count + GetRandomValue(-60, 60) / 1000.0f;
        float v = speed * (0.5f + GetRandomValue(0, 100) / 100.0f);
        p->pos = at;
        p->vel = (Vector2) {
            cosf(angle) * v, sinf(angle) * v
        };
        p->life = p->max_life = GetRandomValue(120, 200) / 100.0f;
        p->hue = hue + GetRandomValue(-14, 14);
        p->radius = GetRandomValue(15, 30) / 10.0f;
    }

    /* fast white sparks read as the initial flash */
    for (int i = 0; i < 10; i++) {
        particle_t *p = free_particle();
        if (p == NULL) {
            return;
        }
        float angle = (2.0f * PI * i) / 10.0f;
        p->pos = at;
        p->vel = (Vector2) {
            cosf(angle) * 150.0f, sinf(angle) * 150.0f
        };
        p->life = p->max_life = 0.15f;
        p->hue = -1.0f;
        p->radius = 2.0f;
    }
}

void fireworks_init(int width, int height)
{
    s_width = width;
    s_height = height;

    for (int i = 0; i < MAX_STARS; i++) {
        s_stars[i].x = GetRandomValue(0, s_width - 1);
        s_stars[i].y = GetRandomValue(0, s_height / 2);
        s_stars[i].phase = GetRandomValue(0, 628) / 100.0f;
        s_stars[i].speed = GetRandomValue(4, 20) / 10.0f;
    }

    int x = 0;
    while ((x < s_width) && (s_building_count < MAX_BUILDINGS)) {
        int w = GetRandomValue(22, 48);
        int h = GetRandomValue(42, 92);
        if (x + w > s_width) {
            w = s_width - x;
            if (w < 14) {
                /* a sliver cannot hold windows; fold it into the neighbour */
                s_buildings[s_building_count - 1].w += w;
                break;
            }
        }
        s_buildings[s_building_count++] = (building_t) {
            .x = x, .w = w, .h = h
        };
        x += w;
    }

    /* Bake nothing: the sky is drawn per frame as flat bands, which costs the
     * framebuffer fill time and nothing else (no sampler, no render texture). */
}

void fireworks_update(float dt)
{
    /* accumulate real time, run whole simulation steps */
    s_sim_accum += dt;
    for (int step = 0; (step < SIM_MAX_STEPS) && (s_sim_accum >= SIM_STEP); step++) {
        s_sim_accum -= SIM_STEP;
        sim_step(SIM_STEP);
    }
}

static void sim_step(float dt)
{
    float now = (float)GetTime();

    if (now >= s_next_launch) {
        for (int i = 0; i < MAX_ROCKETS; i++) {
            if (s_rockets[i].fuse <= 0.0f) {
                spawn_rocket(&s_rockets[i]);
                break;
            }
        }
        s_next_launch = now + GetRandomValue(25, 110) / 100.0f;
    }

    for (int i = 0; i < MAX_ROCKETS; i++) {
        rocket_t *r = &s_rockets[i];
        if (r->fuse <= 0.0f) {
            continue;
        }
        r->pos.x += r->vel.x * dt;
        r->pos.y += r->vel.y * dt;
        r->vel.y += GRAVITY * dt;
        r->fuse -= dt;
        if (r->fuse <= 0.0f) {
            burst(r->pos, r->hue);
        }
    }

    for (int i = 0; i < MAX_PARTICLES; i++) {
        particle_t *p = &s_particles[i];
        if (p->life <= 0.0f) {
            continue;
        }
        p->pos.x += p->vel.x * dt;
        p->pos.y += p->vel.y * dt;
        p->vel.y += GRAVITY * 0.55f * dt;
        p->vel.x *= 1.0f - 1.6f * dt;
        p->vel.y *= 1.0f - 1.6f * dt;
        p->life -= dt;
    }
}

void fireworks_draw(float t)
{
    const int bands = 60;
    const int band_h = s_height / bands;
    for (int i = 0; i < bands; i++) {
        float k = (float)i / (bands - 1);
        DrawRectangle(0, i * band_h, s_width, band_h,
                      ColorFromHSV(226.0f, 0.65f - 0.10f * k, 0.10f - 0.065f * k));
    }

    /* A dot this small is a rectangle on screen; one quad instead of the 36
     * triangles DrawCircle emits makes it ~25x cheaper. */
    for (int i = 0; i < MAX_STARS; i++) {
        float twinkle = 0.5f + 0.5f * sinf(t * s_stars[i].speed + s_stars[i].phase);
        DrawRectangle(s_stars[i].x, s_stars[i].y, 2, 2,
                      Fade(RAYWHITE, 0.2f + 0.5f * twinkle));
    }

    for (int i = 0; i < MAX_ROCKETS; i++) {
        rocket_t *r = &s_rockets[i];
        if (r->fuse <= 0.0f) {
            continue;
        }
        Vector2 tail = { r->pos.x - r->vel.x * 0.06f, r->pos.y - r->vel.y * 0.06f };
        DrawLineEx(tail, r->pos, 2.0f, Fade(ORANGE, 0.8f));
        DrawRectangle((int)r->pos.x - 1, (int)r->pos.y - 1, 3, 3, Fade(RAYWHITE, 0.9f));
    }

    for (int i = 0; i < MAX_PARTICLES; i++) {
        particle_t *p = &s_particles[i];
        if (p->life <= 0.0f) {
            continue;
        }
        float alpha = p->life / p->max_life;
        Color color = (p->hue < 0.0f) ? RAYWHITE : ColorFromHSV(p->hue, 0.70f, 1.0f);
        Vector2 tail = { p->pos.x - p->vel.x * 0.05f, p->pos.y - p->vel.y * 0.05f };
        DrawLineEx(tail, p->pos, 1.5f, Fade(color, alpha * 0.5f));
        int size = (int)(p->radius * 2.0f);
        DrawRectangle((int)p->pos.x - size / 2, (int)p->pos.y - size / 2, size, size,
                      Fade(color, alpha));
    }

    const Color silhouette = ColorFromHSV(228.0f, 0.45f, 0.05f);
    for (int i = 0; i < s_building_count; i++) {
        building_t *b = &s_buildings[i];
        DrawRectangle(b->x, s_height - b->h, b->w, b->h, silhouette);
        for (int k = 0; k < 4; k++) {
            unsigned seed = hash32((unsigned)(i * 97 + k * 31));
            if ((seed & 3u) == 0u) {
                continue;
            }
            int wx = b->x + 4 + (int)(seed % (unsigned)(b->w - 9));
            int wy = s_height - b->h + 5 + (int)(hash32(seed + 7) % (unsigned)(b->h - 12));
            float glow = 0.35f + 0.35f * sinf(t * 0.6f + (float)(seed & 63u));
            DrawRectangle(wx, wy, 2, 3, Fade(ColorFromHSV(42.0f, 0.55f, 1.0f), glow));
        }
    }

    if (t < 4.0f) {
        int w = MeasureText("raylib fireworks", 28);
        DrawText("raylib fireworks", (s_width - w) / 2, 92, 28,
                 Fade(RAYWHITE, 1.0f - t / 4.0f));
    } else {
        DrawText("raylib fireworks", 8, 6, 12, Fade(RAYWHITE, 0.45f));
    }
}
