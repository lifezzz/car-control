/*
 * cli.c - the text front end: read a line, dispatch it, print the result.
 *
 * stdout carries the machine readable "status" blocks only, every other
 * message goes to stderr.  See cli.h for the reasoning.
 */

#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
/* With -std=c11 (strict ANSI) this is needed to see isatty()/fileno(). */
#define _POSIX_C_SOURCE 200809L
#endif

#include "cli.h"

#include <stdio.h>
#include <string.h>

#include "car.h"
#include "command.h"

#if defined(_WIN32)
#include <io.h>
#define CAR_ISATTY(fd) _isatty(fd)
#define CAR_FILENO(stream) _fileno(stream)
#else
#include <unistd.h>
#define CAR_ISATTY(fd) isatty(fd)
#define CAR_FILENO(stream) fileno(stream)
#endif

#define TEXT_SIZE 96

/* ------------------------------------------------------------------ */
/* small formatting helpers                                           */
/* ------------------------------------------------------------------ */

static void format_wheels(const int wheels[WHEEL_COUNT], char *buffer, size_t size)
{
    snprintf(buffer, size, "%d %d %d %d", wheels[0], wheels[1], wheels[2], wheels[3]);
}

/* "FORWARD 60", "STOP" or "NONE" - the text used after AUTO/MANUAL/TARGET. */
static void format_requirement(const requirement_t *request, char *buffer, size_t size)
{
    if (request == NULL || !request->present || request->motion == MOTION_NONE) {
        snprintf(buffer, size, "NONE");
        return;
    }
    if (motion_accepts_speed(request->motion)) {
        snprintf(buffer, size, "%s %d", motion_name(request->motion), request->speed);
    } else {
        snprintf(buffer, size, "%s", motion_name(request->motion));
    }
}

/* ------------------------------------------------------------------ */
/* status output (stdout, exact format required by the task)          */
/* ------------------------------------------------------------------ */

static void print_status(const car_t *car, FILE *out)
{
    control_source_t source;
    requirement_t effective;
    char auto_text[TEXT_SIZE];
    char manual_text[TEXT_SIZE];
    char target_text[TEXT_SIZE];
    char wheels_text[TEXT_SIZE];

    car_effective(car, &source, &effective);

    format_requirement(&car->auto_req, auto_text, sizeof auto_text);
    format_requirement(&car->manual_req, manual_text, sizeof manual_text);

    /* Without any stored request the effective motion is simply "stop". */
    if (effective.present && effective.motion != MOTION_NONE) {
        format_requirement(&effective, target_text, sizeof target_text);
    } else {
        snprintf(target_text, sizeof target_text, "STOP");
    }

    format_wheels(car->wheels, wheels_text, sizeof wheels_text);

    fprintf(out, "STATE %s\n", car_state_name(car->state));
    fprintf(out, "CONTROL %s\n", car_control_name(source));
    fprintf(out, "AUTO %s\n", auto_text);
    fprintf(out, "MANUAL %s\n", manual_text);
    fprintf(out, "TARGET %s\n", target_text);
    fprintf(out, "WHEELS %s\n", wheels_text);
    fflush(out); /* keep stdout in step with the stderr messages */
}

/* ------------------------------------------------------------------ */
/* command execution                                                  */
/* ------------------------------------------------------------------ */

/* Why a motion command was refused, depending on the current state. */
static const char *state_hint(car_state_t state)
{
    return state == STATE_EMERGENCY ? "run 'reset' first" : "run 'start' first";
}

static void execute_control_request(car_t *car, const command_t *command, FILE *msg)
{
    bool is_auto = (command->type == CMD_AUTO);
    const char *label = is_auto ? "auto" : "manual";
    requirement_t request;
    car_result_t result;
    char text[TEXT_SIZE];

    result = is_auto ? car_set_auto(car, command->motion, command->speed)
                     : car_set_manual(car, command->motion, command->speed);

    request.present = true;
    request.motion = command->motion;
    request.speed = command->speed;
    format_requirement(&request, text, sizeof text);

    if (result == CAR_OK) {
        fprintf(msg, "[ok] %s requirement stored: %s\n", label, text);
    } else {
        fprintf(msg, "[error] %s %s refused: car is %s, %s\n",
                label, text, car_state_name(car->state), state_hint(car->state));
    }
}

static void execute(car_t *car, const command_t *command, FILE *out, FILE *msg)
{
    char text[TEXT_SIZE];
    char wheels_text[TEXT_SIZE];

    switch (command->type) {
    case CMD_START: {
        car_result_t result = car_start(car);
        if (result == CAR_OK) {
            fprintf(msg, "[ok] started, state = RUNNING\n");
        } else if (result == CAR_ERR_ALREADY_RUNNING) {
            fprintf(msg, "[info] already running, nothing to do\n");
        } else {
            fprintf(msg, "[error] cannot start: car is %s, %s\n",
                    car_state_name(car->state), state_hint(car->state));
        }
        break;
    }

    case CMD_AUTO:
    case CMD_MANUAL:
        execute_control_request(car, command, msg);
        break;

    case CMD_MANUAL_RELEASE: {
        car_result_t result = car_release_manual(car);
        if (result != CAR_OK) {
            fprintf(msg, "[warn] no manual requirement to release, ignored\n");
            break;
        }
        control_source_t source;
        requirement_t effective;
        car_effective(car, &source, &effective);
        if (source == CONTROL_NONE) {
            fprintf(msg, "[ok] manual control released, no requirement left -> TARGET STOP\n");
        } else {
            format_requirement(&effective, text, sizeof text);
            fprintf(msg, "[ok] manual control released, now following %s %s\n",
                    car_control_name(source), text);
        }
        break;
    }

    case CMD_STEP: {
        car_result_t result = car_step(car);
        if (result == CAR_OK) {
            format_wheels(car->wheels, wheels_text, sizeof wheels_text);
            fprintf(msg, "[ok] step done, wheels = %s\n", wheels_text);
        } else {
            fprintf(msg, "[error] step refused: car is %s, %s\n",
                    car_state_name(car->state), state_hint(car->state));
        }
        break;
    }

    case CMD_EMERGENCY:
        car_emergency(car);
        fprintf(msg, "[warn] emergency stop: wheels = 0 0 0 0, state = EMERGENCY "
                     "(use 'reset' to leave this state)\n");
        break;

    case CMD_RESET:
        car_reset(car);
        fprintf(msg, "[ok] reset: state = IDLE, requirements cleared, wheels = 0 0 0 0 "
                     "(use 'start' to run again)\n");
        break;

    case CMD_STATUS:
        print_status(car, out);
        break;

    case CMD_QUIT:
    case CMD_UNKNOWN:
    default:
        /* CMD_QUIT is handled by the read loop. */
        fprintf(msg, "[error] internal error: command not handled\n");
        break;
    }
}

/* ------------------------------------------------------------------ */
/* error reporting                                                    */
/* ------------------------------------------------------------------ */

static void report_parse_error(parse_result_t result, const char *line, FILE *msg)
{
    char text[TEXT_SIZE];

    switch (result) {
    case PARSE_EMPTY:
        return; /* blank line: nothing to report */
    case PARSE_UNKNOWN_COMMAND:
        snprintf(text, sizeof text, "unknown command");
        break;
    case PARSE_MISSING_ARGUMENT:
        snprintf(text, sizeof text, "missing argument");
        break;
    case PARSE_UNEXPECTED_ARGUMENT:
        snprintf(text, sizeof text, "unexpected extra argument");
        break;
    case PARSE_INVALID_MOTION:
        snprintf(text, sizeof text, "invalid motion (expected forward|backward|left|right|stop)");
        break;
    case PARSE_INVALID_SPEED:
        snprintf(text, sizeof text, "invalid speed (expected an integer in %d..%d)",
                 CAR_SPEED_MIN, CAR_SPEED_MAX);
        break;
    case PARSE_TOO_LONG:
        snprintf(text, sizeof text, "line too long or too many arguments");
        break;
    case PARSE_OK:
    default:
        return;
    }

    fprintf(msg, "[error] %s: '%s'\n", text, line);
}

static void trim_line(char *line)
{
    size_t length = strlen(line);

    while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
        line[--length] = '\0';
    }
}

static const char *skip_spaces(const char *text)
{
    while (*text == ' ' || *text == '\t') {
        ++text;
    }
    return text;
}

/* ------------------------------------------------------------------ */
/* public interface                                                   */
/* ------------------------------------------------------------------ */

bool cli_is_interactive(void)
{
    return CAR_ISATTY(CAR_FILENO(stdin)) != 0;
}

void cli_print_banner(FILE *msg)
{
    fprintf(msg, "four wheel car control simulator\n");
    fprintf(msg, "commands: start | auto <motion> [speed] | manual <motion> [speed]\n");
    fprintf(msg, "          manual release | step | emergency | reset | status | quit\n");
    fprintf(msg, "motions : forward | backward | left | right | stop, "
                 "speed %d..%d\n", CAR_SPEED_MIN, CAR_SPEED_MAX);
}

void cli_run(car_t *car, FILE *in, FILE *out, FILE *msg)
{
    char line[CAR_MAX_LINE_LEN];

    while (fgets(line, (int)sizeof line, in) != NULL) {
        size_t length = strlen(line);

        if (length == sizeof line - 1 && line[length - 1] != '\n') {
            /* Buffer full without newline: the line is longer than supported. */
            int ch;
            while ((ch = fgetc(in)) != EOF && ch != '\n') {
                /* drop the rest of the line */
            }
            fprintf(msg, "[error] line too long (max %d characters), ignored\n",
                    CAR_MAX_LINE_LEN - 1);
            continue;
        }

        trim_line(line);

        command_t command;
        parse_result_t result = command_parse(line, &command);
        if (result != PARSE_OK) {
            report_parse_error(result, skip_spaces(line), msg);
            continue;
        }
        if (command.type == CMD_QUIT) {
            fprintf(msg, "[info] quit\n");
            break;
        }
        execute(car, &command, out, msg);
    }

    if (ferror(in)) {
        fprintf(msg, "[error] read error on input, stopping\n");
    }
}
