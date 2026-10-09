/*
 * car.h - the car itself: state machine, control requests and wheel speeds.
 *
 * This layer knows nothing about text input or output; it only offers
 * operations (car_start, car_step, ...) that report what happened through
 * car_result_t, so the same core can be driven by a REPL, a file or a test.
 */
#ifndef CAR_CAR_H
#define CAR_CAR_H

#include <stdbool.h>

#include "config.h"
#include "motion.h"

typedef enum {
    STATE_IDLE = 0,
    STATE_RUNNING,
    STATE_EMERGENCY,
    STATE_COUNT
} car_state_t;

/* Which requirement is currently in charge. */
typedef enum {
    CONTROL_NONE = 0,
    CONTROL_AUTO,
    CONTROL_MANUAL,
    CONTROL_COUNT
} control_source_t;

/* One stored control request: a motion plus its speed. */
typedef struct {
    bool present;       /* is a request stored at all? */
    motion_id_t motion; /* MOTION_NONE when absent */
    int speed;          /* 0 when the motion takes no speed */
} requirement_t;

typedef struct {
    car_state_t state;
    requirement_t auto_req;   /* last "auto ..." command   */
    requirement_t manual_req; /* last "manual ..." command; present == manual engaged */
    int wheels[WHEEL_COUNT];  /* actual wheel speeds */
} car_t;

/* Outcome of an operation, so callers can print a meaningful message. */
typedef enum {
    CAR_OK = 0,
    CAR_ERR_NOT_RUNNING,     /* motion command while not RUNNING   */
    CAR_ERR_ALREADY_RUNNING, /* "start" while RUNNING              */
    CAR_ERR_EMERGENCY,       /* "start" while latched in EMERGENCY */
    CAR_ERR_NO_MANUAL        /* "manual release" without manual    */
} car_result_t;

/* Reset to power-on state: IDLE, no requirements, wheels at 0. */
void car_init(car_t *car);

/* Same as car_init, exposed as the "reset" operation. */
void car_reset(car_t *car);

car_result_t car_start(car_t *car);
car_result_t car_set_auto(car_t *car, motion_id_t motion, int speed);
car_result_t car_set_manual(car_t *car, motion_id_t motion, int speed);
car_result_t car_release_manual(car_t *car);
car_result_t car_step(car_t *car);

/* Always succeeds: accessible from every state. */
void car_emergency(car_t *car);

/*
 * Arbitration between the two control sources: manual wins over auto.
 * Both out parameters are optional (NULL allowed).
 */
void car_effective(const car_t *car, control_source_t *source, requirement_t *requirement);

/* Wheel speeds the car is currently aiming at (all zero without a request). */
void car_target_wheels(const car_t *car, int out[WHEEL_COUNT]);

const char *car_state_name(car_state_t state);
const char *car_control_name(control_source_t source);

#endif /* CAR_CAR_H */
