/*
 * heartbeat.h — peer liveness + per-leader timeout detection.
 *
 * Maintains a small peer table keyed by MAC. Each entry stores the peer's
 * priority (learned from received heartbeats) and the wall-time of the
 * most recent heartbeat. Used by:
 *   - heartbeat_check_timeout: detect that the *current coordinator* has
 *     stopped heartbeating, distinct from "some other peer is silent"
 *   - bully.c: enumerate higher-priority peers when starting an election,
 *     and look up the current coordinator's MAC for sending OK
 *
 * Tunables live in config.h (HEARTBEAT_INTERVAL_MS, HEARTBEAT_TIMEOUT_MS,
 * MAX_PEER_TABLE).
 */

#ifndef LEADER_HEARTBEAT_H
#define LEADER_HEARTBEAT_H

#include <stdint.h>
#include "messages.h"

void heartbeat_init(uint32_t my_priority);

/* Called by espnow_handler.c on each received heartbeat. Updates the peer
 * table (creates entry if MAC unseen). */
void heartbeat_on_received(const Heartbeat *msg);

/* Poll in the main loop. If the current coordinator has gone silent for
 * more than HEARTBEAT_TIMEOUT_MS, calls bully_on_heartbeat_timeout()
 * exactly once per outage. Returns 1 if a timeout fired, else 0. */
int heartbeat_check_timeout(void);

/* Tell heartbeat who the current coordinator is. Bully calls this when it
 * adopts a new coordinator (either by winning or by hearing one). */
void heartbeat_set_coordinator(const uint8_t coordinator_mac[6], uint32_t priority);

/* For bully use: enumerate peers with strictly higher priority than mine.
 * Writes up to `max` MACs into out_macs, returns count written. */
int  heartbeat_get_higher_priority_peers(uint8_t out_macs[][6], int max);

/* For bully use: look up a peer's MAC by priority. Returns 0 on hit and
 * fills out_mac, -1 if no such peer is currently known. */
int  heartbeat_get_peer_mac(uint32_t priority, uint8_t out_mac[6]);

#endif /* LEADER_HEARTBEAT_H */
