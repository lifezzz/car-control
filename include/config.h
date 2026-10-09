/*
 * config.h - global limits of the simulator.
 *
 * All tunable numbers live here so that nobody has to hunt for magic values.
 */
#ifndef CAR_CONFIG_H
#define CAR_CONFIG_H

/* Accepted range of the <speed> argument of motion commands. */
#define CAR_SPEED_MIN 0
#define CAR_SPEED_MAX 100

/* Maximum change of one wheel speed per "step" command. */
#define CAR_STEP_DELTA 20

/* Limits of the line tokenizer (see command.c). */
#define CAR_MAX_TOKENS 8
#define CAR_MAX_TOKEN_LEN 32
#define CAR_MAX_LINE_LEN 256

#endif /* CAR_CONFIG_H */
