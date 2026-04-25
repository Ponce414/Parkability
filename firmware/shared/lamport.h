/*
 * lamport.h — Lamport logical clock.
 *
 * Used for ordering sensor events across nodes despite unsynchronized wall
 * clocks. See concepts/07-logical-time/README.md for the full rationale.
 *
 * Thread-safe: all operations take a FreeRTOS mutex internally.
 */

#ifndef PARKING_LAMPORT_H
#define PARKING_LAMPORT_H

#include <stdint.h>

/* Call once during boot, before any other lamport_* call. */
void lamport_init(void);

/* Local event: increments and returns the new timestamp. Call right before
 * stamping an outgoing message. */
uint32_t lamport_tick(void);

/* Receive event: clock becomes max(local, received) + 1, return new value.
 * Call immediately on receiving any stamped message, before acting on it. */
uint32_t lamport_update(uint32_t received_ts);

/* Read without mutating. Mainly for logging. */
uint32_t lamport_now(void);

#endif /* PARKING_LAMPORT_H */
