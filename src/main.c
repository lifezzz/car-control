/*
 * main.c - entry point.
 *
 * The program is deliberately thin here: build the car, hand the three
 * standard streams to the text front end.  All behaviour lives in car.c
 * (logic) and cli.c (text interface), which keeps this file trivial to read
 * and the rest testable on its own.
 */
#include <stdio.h>

#include "car.h"
#include "cli.h"

int main(void)
{
    car_t car;

    car_init(&car);

    /* A banner is helpful interactively but must not pollute piped input. */
    if (cli_is_interactive()) {
        cli_print_banner(stderr);
    }

    cli_run(&car, stdin, stdout, stderr);
    return 0;
}
