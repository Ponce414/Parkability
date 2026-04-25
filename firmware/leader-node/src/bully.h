/*
 * bully.h — Bully algorithm for leader election.
 *
 * Triggered when a node suspects the current leader is dead (heartbeat
 * timeout). Any node may start an election; the highest-priority node
 * (largest node_id) ends up as coordinator.
 *
 * ----------------------------------------------------------------------
 * State transition table (implement in bully.c):
 *
 *   IDLE
 *     on heartbeat_timeout:
 *       -> send ELECTION to all peers with priority > me
 *       -> state = WAITING_FOR_OK, start T_OK timer
 *
 *   WAITING_FOR_OK
 *     on OK received:
 *       -> cancel T_OK, state = ELECTION_IN_PROGRESS, start T_COORD timer
 *     on T_OK expires (nobody outranked us):
 *       -> send COORDINATOR to all peers, state = COORDINATOR
 *
 *   ELECTION_IN_PROGRESS
 *     on COORDINATOR received:
 *       -> adopt sender as leader, state = IDLE
 *     on T_COORD expires (the higher-priority node that said OK died):
 *       -> restart election, state = WAITING_FOR_OK
 *
 *   COORDINATOR
 *     on ELECTION received from lower priority:
 *       -> reply OK, re-announce COORDINATOR
 *     on ELECTION received from higher priority:
 *       -> state = WAITING_FOR_OK (we just lost rank? probably a new node joined)
 *
 * ----------------------------------------------------------------------
 * Priority: 32-bit node_id derived from MAC (see bully_init). Larger wins.
 */

#ifndef LEADER_BULLY_H
#define LEADER_BULLY_H

#include <stdint.h>
#include "messages.h"

typedef enum {
    BULLY_IDLE,
    BULLY_WAITING_FOR_OK,
    BULLY_ELECTION_IN_PROGRESS,
    BULLY_COORDINATOR,
} BullyState;

/* Init with this node's priority (derived from MAC: last 4 bytes as uint32).
 * Also registers known peers so we know who to send ELECTION to. */
void bully_init(uint32_t my_priority);

BullyState bully_current_state(void);

/* Called by heartbeat.c when the current leader's heartbeat has been missed. */
void bully_on_heartbeat_timeout(void);

/* Inbound message handlers, called by espnow_handler.c. */
void bully_on_election_received(const Election *msg);
void bully_on_ok_received(const OkMessage *msg);
void bully_on_coordinator_received(const Coordinator *msg);

#endif /* LEADER_BULLY_H */
