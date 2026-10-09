/*
 * car.c - state machine, control arbitration and wheel dynamics.
 */
#include "car.h"

#include <string.h>

/* A request that is "not there at all". */
static requirement_t requirement_absent(void)
{
    requirement_t request;

    request.present = false;
    request.motion = MOTION_NONE;
    request.speed = 0;
    return request;
}

static void store_requirement(requirement_t *target, motion_id_t motion, int speed)
{
    target->present = true;
    target->motion = motion;
    target->speed = speed;
}

void car_init(car_t *car)
{
    if (car == NULL) {
        return;
    }
    memset(car, 0, sizeof *car);
    car->state = STATE_IDLE;
    car->auto_req = requirement_absent();
    car->manual_req = requirement_absent();
    /* car->wheels is already zeroed by memset(). */
}

void car_reset(car_t *car)
{
    /* "reset" and power-on are the same thing: IDLE, no requests, wheels 0. */
    car_init(car);
}

car_result_t car_start(car_t *car)
{
    switch (car->state) {
    case STATE_IDLE:
        car->state = STATE_RUNNING;
        return CAR_OK;
    case STATE_RUNNING:
        return CAR_ERR_ALREADY_RUNNING;
    case STATE_EMERGENCY:
    default:
        /* An emergency stop is latched: it has to be cleared by "reset". */
        return CAR_ERR_EMERGENCY;
    }
}

/*
 * Motion commands are only accepted while RUNNING.  They store the request,
 * they never touch the actual wheel speeds.
 */
car_result_t car_set_auto(car_t *car, motion_id_t motion, int speed)
{
    if (car->state != STATE_RUNNING) {
        return CAR_ERR_NOT_RUNNING;
    }
    store_requirement(&car->auto_req, motion, speed);
    return CAR_OK;
}

car_result_t car_set_manual(car_t *car, motion_id_t motion, int speed)
{
    if (car->state != STATE_RUNNING) {
        return CAR_ERR_NOT_RUNNING;
    }
    store_requirement(&car->manual_req, motion, speed);
    return CAR_OK;
}

car_result_t car_release_manual(car_t *car)
{
    if (!car->manual_req.present) {
        return CAR_ERR_NO_MANUAL;
    }
    car->manual_req = requirement_absent();
    return CAR_OK;
}

void car_effective(const car_t *car, control_source_t *source, requirement_t *requirement)
{
    control_source_t chosen_source;
    requirement_t chosen_request;

    if (car->manual_req.present) {
        /* Manual control always has priority over automatic control. */
        chosen_source = CONTROL_MANUAL;
        chosen_request = car->manual_req;
    } else if (car->auto_req.present) {
        chosen_source = CONTROL_AUTO;
        chosen_request = car->auto_req;
    } else {
        chosen_source = CONTROL_NONE;
        chosen_request = requirement_absent();
    }

    if (source != NULL) {
        *source = chosen_source;
    }
    if (requirement != NULL) {
        *requirement = chosen_request;
    }
}

void car_target_wheels(const car_t *car, int out[WHEEL_COUNT])
{
    requirement_t effective;

    car_effective(car, NULL, &effective);

    if (!effective.present) {
        for (int i = 0; i < WHEEL_COUNT; ++i) {
            out[i] = 0;
        }
        return;
    }
    motion_wheel_speeds(effective.motion, effective.speed, out);
}

car_result_t car_step(car_t *car)
{
    int target[WHEEL_COUNT];

    if (car->state != STATE_RUNNING) {
        return CAR_ERR_NOT_RUNNING;
    }

    car_target_wheels(car, target);

    for (int i = 0; i < WHEEL_COUNT; ++i) {
        /*
         * Move at most CAR_STEP_DELTA towards the target.  Because the step is
         * clamped instead of jumping straight to the target, a reversal such
         * as 40 -> -40 passes through 20, 0, -20, -40.
         */
        int delta = target[i] - car->wheels[i];

        if (delta > CAR_STEP_DELTA) {
            delta = CAR_STEP_DELTA;
        } else if (delta < -CAR_STEP_DELTA) {
            delta = -CAR_STEP_DELTA;
        }
        car->wheels[i] += delta;
    }
    return CAR_OK;
}

void car_emergency(car_t *car)
{
    car->state = STATE_EMERGENCY;
    for (int i = 0; i < WHEEL_COUNT; ++i) {
        car->wheels[i] = 0; /* immediate, not through "step" */
    }
    /*
     * The stored requests are kept: from EMERGENCY only "status" and "reset"
     * are meaningful, and "reset" is what clears the requests.
     */
}

const char *car_state_name(car_state_t state)
{
    switch (state) {
    case STATE_IDLE:
        return "IDLE";
    case STATE_RUNNING:
        return "RUNNING";
    case STATE_EMERGENCY:
        return "EMERGENCY";
    default:
        return "UNKNOWN";
    }
}

const char *car_control_name(control_source_t source)
{
    switch (source) {
    case CONTROL_NONE:
        return "NONE";
    case CONTROL_AUTO:
        return "AUTO";
    case CONTROL_MANUAL:
        return "MANUAL";
    default:
        return "UNKNOWN";
    }
}
