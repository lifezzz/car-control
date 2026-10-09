/*
 * command.h - parsing of one input line into a command_t.
 *
 * The parser never touches the car state: it only answers "what did the user
 * ask for" (command_t) or "why is this line not a valid command"
 * (parse_result_t).  The caller (cli.c) decides what to do with it.
 */
#ifndef CAR_COMMAND_H
#define CAR_COMMAND_H

#include <stdbool.h>

#include "motion.h"

typedef enum {
    CMD_UNKNOWN = 0,
    CMD_START,
    CMD_AUTO,          /* auto <motion> [speed] */
    CMD_MANUAL,        /* manual <motion> [speed] */
    CMD_MANUAL_RELEASE,
    CMD_STEP,
    CMD_EMERGENCY,
    CMD_RESET,
    CMD_STATUS,
    CMD_QUIT
} command_type_t;

typedef struct {
    command_type_t type;
    bool has_motion;
    motion_id_t motion;
    bool has_speed;
    int speed;
} command_t;

typedef enum {
    PARSE_OK = 0,
    PARSE_EMPTY,               /* blank line, ignored silently */
    PARSE_UNKNOWN_COMMAND,
    PARSE_MISSING_ARGUMENT,
    PARSE_UNEXPECTED_ARGUMENT,
    PARSE_INVALID_MOTION,
    PARSE_INVALID_SPEED,       /* not an integer, or outside 0..100 */
    PARSE_TOO_LONG             /* line/token longer than the buffers */
} parse_result_t;

/* Parse a single input line.  The line is treated as read only. */
parse_result_t command_parse(const char *line, command_t *out);

#endif /* CAR_COMMAND_H */
