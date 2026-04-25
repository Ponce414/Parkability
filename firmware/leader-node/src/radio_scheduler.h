/*
 * radio_scheduler.h — isolates the ESP32-C3 single-radio sharing strategy.
 *
 * ESP-NOW and WiFi STA both use the 2.4GHz radio. On the C3 they can't run
 * truly concurrently — the chip can be associated to a WiFi AP OR broadcasting
 * ESP-NOW on a chosen channel, not both cleanly. This module owns the policy
 * for switching between the two so the rest of the leader firmware never has
 * to think about the radio mode it's in.
 *
 * Replacing C3 with a dual-radio chip (C5, S3 with dual-mode) would mean
 * rewriting ONLY this file.
 */

#ifndef LEADER_RADIO_SCHEDULER_H
#define LEADER_RADIO_SCHEDULER_H

#include <stdint.h>

typedef enum {
    RADIO_MODE_ESPNOW,
    RADIO_MODE_WIFI,
} RadioMode;

typedef struct {
    uint32_t espnow_windows;
    uint32_t wifi_windows;
    uint32_t mode_switches;
    uint32_t espnow_dropped_due_to_mode; /* tried to send but wifi had the radio */
    uint32_t wifi_dropped_due_to_mode;
} RadioStats;

/* Start the scheduler task. Must be called exactly once, after WiFi init. */
void radio_scheduler_start(void);

/* What mode is the radio in RIGHT NOW. Callers should check this before
 * attempting to send; if they disagree with current mode, either queue the
 * message or drop it and bump a stat. */
RadioMode radio_scheduler_current_mode(void);

/* Request an immediate switch. Used by aggregator when it has a full batch
 * ready for the backend and doesn't want to wait for the next WiFi window. */
void radio_scheduler_request_switch(RadioMode mode);

/* Diagnostics — expose to both logs and the backend so we can graph the
 * mode-switching behavior during experiments (concepts/02). */
RadioStats radio_scheduler_get_stats(void);

#endif /* LEADER_RADIO_SCHEDULER_H */
