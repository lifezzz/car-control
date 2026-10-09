/*
 * cli.h - the text front end.
 *
 * Stream policy (important for automated grading):
 *   - stdout : ONLY the six line "status" blocks, in the exact format that the
 *              task requires, with nothing in between;
 *   - stderr : every human readable feedback (ok / error / hints) and the
 *              interactive banner.
 * That way `./car < test.txt > out.txt` yields a file that can be compared
 * against the expected status blocks directly.
 */
#ifndef CAR_CLI_H
#define CAR_CLI_H

#include <stdbool.h>
#include <stdio.h>

#include "car.h"

/* true when stdin is a terminal (piped input suppresses the banner). */
bool cli_is_interactive(void);

void cli_print_banner(FILE *msg);

/* Read commands line by line until EOF, "quit"/"exit" or a read error. */
void cli_run(car_t *car, FILE *in, FILE *out, FILE *msg);

#endif /* CAR_CLI_H */
