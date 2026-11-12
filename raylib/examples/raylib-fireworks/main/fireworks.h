/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Self-playing fireworks scene: night sky, city skyline, rockets and particle
 * bursts. Immediate-mode raylib only, no assets, no input.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Set the canvas size the scene draws within. Call once before updating. */
void fireworks_init(int width, int height);

/* Advance the simulation by dt seconds. */
void fireworks_update(float dt);

/* Draw one frame at time_now seconds, which drives the sky and animation. */
void fireworks_draw(float time_now);

#ifdef __cplusplus
}
#endif
