/* 91546LED — translation between the two RC light protocols.
 *
 * Turns the receiver frame (two long symbol positions a,b) into the 9-bit code
 * that the -A board expects. Verified against all 36 measured frames.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdio.h>

/* Output: bits[0..7] = the eight data bits, *tail = trailing H in units (1 or 3). */
void translate(int a, int b, uint8_t bits[8], uint8_t *tail)
{
    for (int i = 0; i < 8; i++)
        bits[i] = 0;

    bits[0] = 1;                      /* start marker, always set          */
    if (1 + a >= 0 && 1 + a < 8)
        bits[1 + a] = 1;
    if (b <= 7 && 1 + b < 8)          /* b == 8 falls outside the 8-bit field */
        bits[1 + b] = 1;

    *tail = (a == 6 || a == 7 || b == 6 || b == 7) ? 3 : 1;
}

/* --- Sending one frame on the -A protocol -------------------------------
 *
 *   idle level  LOW
 *   unit time   509 us
 *   one bit     4 units:  bit == 1 -> H3 L1
 *                         bit == 0 -> H1 L3
 *   frame       8 bits, then a trailing H of *tail units, then low
 *   period      116478 us, repeated forever
 *
 * The mode is NOT latched anywhere. Stop repeating and the lights go out.
 *
 * Pseudocode, absolute deadlines rather than delays so the frame does not drift:
 *
 *   t = now();
 *   for (i = 0; i < 8; i++) {
 *       set_high();                  wait_until(t += UNIT);
 *       if (!bits[i]) set_low();     wait_until(t += 2*UNIT);
 *       set_low();                   wait_until(t += UNIT);
 *   }
 *   set_high();                      wait_until(t += UNIT);
 *   if (tail == 1) set_low();        wait_until(t += 2*UNIT);
 *   set_low();
 *   wait_until(frame_start += PERIOD);
 *
 * The OFF code is all eight bits zero with tail == 1. It darkens the -A within
 * one frame. Holding the line idle instead needs more than 349 ms to take effect,
 * which makes short dark phases invisible.
 */

int main(void)
{
    uint8_t bits[8], tail;

    printf("long at   -A code    trailing H\n");
    for (int a = 0; a <= 8; a++) {
        for (int b = a + 1; b <= 8; b++) {
            translate(a, b, bits, &tail);
            printf("  %d,%d     ", a, b);
            for (int i = 0; i < 8; i++)
                putchar(bits[i] ? '1' : '0');
            printf("%c   H%u\n", tail == 3 ? '1' : '0', tail);
        }
    }
    return 0;
}
