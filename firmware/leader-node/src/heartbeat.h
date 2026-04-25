/*
 * heartbeat.h — liveness between zone leaders.
 *
 * Every zone has one active leader and one backup. Both send Heartbeat
 * messages on a fixed period. A missed-heartbeat streak exceeding
 * HEARTBEAT_TIMEOUT_MS triggers a Bully election.
 *
 * Also used by the backend to detect leader death — the backend tracks
 * leader-change notifications from wifi_uplink.
 */

#ifndef LEADER_HEARTBEAT_H
#define LEADER_HEARTBEAT_H

#include <stdint.h>
#include "messages.h"

/* Defaults — tune at bring-up. HEARTBEAT_TIMEOUT_MS should be at least
 * 3x HEARTBEAT_INTERVAL_MS to tolerate a normal packet drop. */
#define HEARTBEAT_INTERVAL_MS  1000
#define HEARTBEAT_TIMEOUT_MS   3500

void heartbeat_init(uint32_t my_priority);

/* Called by espnow_handler.c on each received heartbeat — resets the
 * timeout timer against the sender. */
void heartbeat_on_received(const Heartbeat *msg);

/* Poll in the main loop; returns 1 if the peer timed out since last check.
 * The stub leaves the detection logic as a TODO with a clear spec. */
int  heartbeat_check_timeout(void);

#endif /* LEADER_HEARTBEAT_H */
