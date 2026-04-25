/*
 * espnow_handler.h — the leader's ESP-NOW inbox.
 *
 * Receives SensorUpdate, Register, Heartbeat, Election, OK, Coordinator
 * messages from peer nodes and dispatches them. The aggregator consumes
 * SensorUpdate; bully.c consumes election traffic; heartbeat.c consumes
 * heartbeats.
 *
 * Peer limit: ~20 encrypted / ~10 unencrypted per ESP-NOW documentation —
 * this caps zone size. See concepts/08-scalability/.
 */

#ifndef LEADER_ESPNOW_HANDLER_H
#define LEADER_ESPNOW_HANDLER_H

#include <stdint.h>

void espnow_handler_init(void);

/* Send a raw ESP-NOW frame to a peer. Caller is responsible for the radio
 * being in ESP-NOW mode (check radio_scheduler_current_mode()). */
int espnow_handler_send(const uint8_t peer_mac[6], const uint8_t *data, uint16_t len);

/* Register a peer so we can send to it. Returns 0 on success, negative on
 * failure (e.g., peer table full — see ZONE scalability note). */
int espnow_handler_add_peer(const uint8_t peer_mac[6]);

#endif /* LEADER_ESPNOW_HANDLER_H */
