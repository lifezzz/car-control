/*
 * command.c - turning one text line into a command_t.
 */
#include "command.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

typedef char token_buffer_t[CAR_MAX_TOKEN_LEN];

/*
 * Split a line on whitespace.
 * Returns the number of tokens, or -1 when the line cannot be tokenized
 * (too many tokens or one token longer than CAR_MAX_TOKEN_LEN-1).
 */
static int tokenize(const char *line, token_buffer_t tokens[CAR_MAX_TOKENS])
{
    int count = 0;
    const char *cursor = line;

    while (*cursor != '\0') {
        while (*cursor != '\0' && isspace((unsigned char)*cursor)) {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }
        if (count == CAR_MAX_TOKENS) {
            return -1;
        }

        size_t length = 0;
        while (*cursor != '\0' && !isspace((unsigned char)*cursor)) {
            if (length + 1 >= CAR_MAX_TOKEN_LEN) {
                return -1;
            }
            tokens[count][length++] = *cursor++;
        }
        tokens[count][length] = '\0';
        ++count;
    }
    return count;
}

/* Whole string, case insensitive comparison of command keywords. */
static bool keyword_equals(const char *lhs, const char *rhs)
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

/*
 * Strict integer parsing: the whole token must be a number and it must be
 * inside the legal speed range.
 */
static bool parse_speed(const char *token, int *out)
{
    char *end = NULL;
    long value;

    if (token == NULL || *token == '\0') {
        return false;
    }

    errno = 0;
    value = strtol(token, &end, 10);
    if (errno != 0 || end == token || *end != '\0') {
        return false; /* not a pure integer */
    }
    if (value < CAR_SPEED_MIN || value > CAR_SPEED_MAX) {
        return false; /* out of range */
    }

    *out = (int)value;
    return true;
}

/* "auto" / "manual" share the same argument grammar. */
static parse_result_t parse_control_request(token_buffer_t *tokens, int count,
                                            bool is_auto, command_t *out)
{
    const char *word = tokens[1];
    motion_id_t motion;

    /* manual release */
    if (!is_auto && keyword_equals(word, "release")) {
        if (count != 2) {
            return PARSE_UNEXPECTED_ARGUMENT;
        }
        out->type = CMD_MANUAL_RELEASE;
        return PARSE_OK;
    }

    if (!motion_parse(word, &motion)) {
        return PARSE_INVALID_MOTION;
    }

    out->type = is_auto ? CMD_AUTO : CMD_MANUAL;
    out->has_motion = true;
    out->motion = motion;

    if (!motion_accepts_speed(motion)) { /* "stop" takes no speed */
        if (count != 2) {
            return PARSE_UNEXPECTED_ARGUMENT;
        }
        return PARSE_OK;
    }

    if (count < 3) {
        return PARSE_MISSING_ARGUMENT;
    }
    if (count > 3) {
        return PARSE_UNEXPECTED_ARGUMENT;
    }
    if (!parse_speed(tokens[2], &out->speed)) {
        return PARSE_INVALID_SPEED;
    }
    out->has_speed = true;
    return PARSE_OK;
}

parse_result_t command_parse(const char *line, command_t *out)
{
    token_buffer_t tokens[CAR_MAX_TOKENS];
    int count;
    const char *head;

    if (line == NULL || out == NULL) {
        return PARSE_EMPTY;
    }

    memset(out, 0, sizeof *out);
    out->type = CMD_UNKNOWN;
    out->motion = MOTION_NONE;

    count = tokenize(line, tokens);
    if (count < 0) {
        return PARSE_TOO_LONG;
    }
    if (count == 0) {
        return PARSE_EMPTY;
    }

    head = tokens[0];

    if (keyword_equals(head, "start")) {
        if (count != 1) {
            return PARSE_UNEXPECTED_ARGUMENT;
        }
        out->type = CMD_START;
        return PARSE_OK;
    }
    if (keyword_equals(head, "step")) {
        if (count != 1) {
            return PARSE_UNEXPECTED_ARGUMENT;
        }
        out->type = CMD_STEP;
        return PARSE_OK;
    }
    if (keyword_equals(head, "emergency")) {
        if (count != 1) {
            return PARSE_UNEXPECTED_ARGUMENT;
        }
        out->type = CMD_EMERGENCY;
        return PARSE_OK;
    }
    if (keyword_equals(head, "reset")) {
        if (count != 1) {
            return PARSE_UNEXPECTED_ARGUMENT;
        }
        out->type = CMD_RESET;
        return PARSE_OK;
    }
    if (keyword_equals(head, "status")) {
        if (count != 1) {
            return PARSE_UNEXPECTED_ARGUMENT;
        }
        out->type = CMD_STATUS;
        return PARSE_OK;
    }
    if (keyword_equals(head, "quit") || keyword_equals(head, "exit")) {
        if (count != 1) {
            return PARSE_UNEXPECTED_ARGUMENT;
        }
        out->type = CMD_QUIT;
        return PARSE_OK;
    }
    if (keyword_equals(head, "auto") || keyword_equals(head, "manual")) {
        if (count < 2) {
            return PARSE_MISSING_ARGUMENT;
        }
        return parse_control_request(tokens, count, keyword_equals(head, "auto"), out);
    }

    return PARSE_UNKNOWN_COMMAND;
}
