/*
 * motion.h - the "motion" vocabulary of the car.
 *
 * A motion is described purely as data: a name and the direction every wheel
 * turns in.  Adding a new motion (e.g. "turn_around") therefore means adding
 * one row to g_motions in motion.c - the parser, the "status" printer and the
 * wheel-target computation all pick it up automatically.
 */
#ifndef CAR_MOTION_H
#define CAR_MOTION_H

#include <stdbool.h>

#include "config.h"

/* Wheel order used everywhere in this project. */
typedef enum {
    WHEEL_FRONT_LEFT = 0,
    WHEEL_FRONT_RIGHT,
    WHEEL_REAR_LEFT,
    WHEEL_REAR_RIGHT,
    WHEEL_COUNT
} wheel_id_t;

typedef enum {
    MOTION_NONE = 0, /* "no requirement stored"; never a legal command argument */
    MOTION_FORWARD,
    MOTION_BACKWARD,
    MOTION_LEFT,
    MOTION_RIGHT,
    MOTION_STOP,
    MOTION_COUNT
} motion_id_t;

/*
 * direction[i] is -1, 0 or +1 and is multiplied by the requested speed to get
 * the target speed of wheel i.
 * accepts_speed is false only for "stop", which takes no <speed> argument.
 */
typedef struct {
    const char *name;           /* upper case name used by "status" */
    const char *token;          /* lower case token accepted on input */
    int direction[WHEEL_COUNT]; /* front-left, front-right, rear-left, rear-right */
    bool accepts_speed;
} motion_desc_t;

extern const motion_desc_t g_motions[MOTION_COUNT];

/* Case insensitive lookup of an input token.  false == unknown motion. */
bool motion_parse(const char *token, motion_id_t *out);

/* Upper case name for "status" output, e.g. "FORWARD". Never NULL. */
const char *motion_name(motion_id_t motion);

/* true when the motion takes a <speed> argument. */
bool motion_accepts_speed(motion_id_t motion);

/* Expand "motion + speed" into the four wheel target speeds. */
void motion_wheel_speeds(motion_id_t motion, int speed, int out[WHEEL_COUNT]);

#endif /* CAR_MOTION_H */
