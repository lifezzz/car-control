/*
 * motion.c - the motion table and its helpers.
 *
 * Adding a motion = adding one row to g_motions (plus one enumerator in
 * motion.h).  Everything else in the program is written against that table.
 */
#include "motion.h"

#include <ctype.h>

/*
 * Wheel order of every row: front-left, front-right, rear-left, rear-right.
 * The values follow the task table:
 *
 *   forward   + + + +
 *   backward  - - - -
 *   left      - + + -
 *   right     + - - +
 *   stop      0 0 0 0
 */
const motion_desc_t g_motions[MOTION_COUNT] = {
    [MOTION_NONE]     = { "NONE",     "",         {  0,  0,  0,  0 }, false },
    [MOTION_FORWARD]  = { "FORWARD",  "forward",  {  1,  1,  1,  1 }, true  },
    [MOTION_BACKWARD] = { "BACKWARD", "backward", { -1, -1, -1, -1 }, true  },
    [MOTION_LEFT]     = { "LEFT",     "left",     { -1,  1,  1, -1 }, true  },
    [MOTION_RIGHT]    = { "RIGHT",    "right",    {  1, -1, -1,  1 }, true  },
    [MOTION_STOP]     = { "STOP",     "stop",     {  0,  0,  0,  0 }, false }
};

static const motion_desc_t *motion_desc(motion_id_t motion)
{
    if ((int)motion < 0 || motion >= MOTION_COUNT) {
        return NULL;
    }
    return &g_motions[motion];
}

/* Case insensitive, whole string comparison. */
static bool token_equals(const char *lhs, const char *rhs)
{
    while (*lhs != '\0' && *rhs != '\0') {
        if (tolower((unsigned char)*lhs) != tolower((unsigned char)*rhs)) {
            return false;
        }
        ++lhs;
        ++rhs;
    }
    return *lhs == '\0' && *rhs == '\0';
}

bool motion_parse(const char *token, motion_id_t *out)
{
    if (token == NULL || out == NULL) {
        return false;
    }
    /* MOTION_NONE is skipped on purpose: it is not a legal input. */
    for (int i = MOTION_FORWARD; i < MOTION_COUNT; ++i) {
        if (token_equals(token, g_motions[i].token)) {
            *out = (motion_id_t)i;
            return true;
        }
    }
    return false;
}

const char *motion_name(motion_id_t motion)
{
    const motion_desc_t *desc = motion_desc(motion);
    return desc != NULL ? desc->name : "NONE";
}

bool motion_accepts_speed(motion_id_t motion)
{
    const motion_desc_t *desc = motion_desc(motion);
    return desc != NULL && desc->accepts_speed;
}

void motion_wheel_speeds(motion_id_t motion, int speed, int out[WHEEL_COUNT])
{
    const motion_desc_t *desc = motion_desc(motion);

    for (int i = 0; i < WHEEL_COUNT; ++i) {
        out[i] = desc != NULL ? desc->direction[i] * speed : 0;
    }
}
