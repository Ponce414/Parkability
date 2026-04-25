/*
 * bully.c — skeleton for Bully leader election.
 *
 * STATUS: stub. State machine frame and handlers exist; logic is pseudocode.
 * This is a core distributed-systems learning exercise — the team implements it.
 *
 * See bully.h for the state transition table. Below each handler is the
 * pseudocode the team should translate into real code.
 */

#include "bully.h"

#include "esp_log.h"

static const char *TAG = "bully";

static BullyState g_state = BULLY_IDLE;
static uint32_t   g_my_priority = 0;

void bully_init(uint32_t my_priority)
{
    g_my_priority = my_priority;
    g_state = BULLY_IDLE;
    ESP_LOGI(TAG, "bully init priority=0x%08x state=IDLE", (unsigned)my_priority);
}

BullyState bully_current_state(void) { return g_state; }

void bully_on_heartbeat_timeout(void)
{
    ESP_LOGW(TAG, "heartbeat_timeout in state %d", g_state);
    /* TODO(team):
     *   if (g_state == COORDINATOR) return;  // we don't elect ourselves out
     *   for each peer with priority > g_my_priority:
     *       send ELECTION { candidate_priority = g_my_priority }
     *   if no such peers existed:
     *       broadcast COORDINATOR, g_state = COORDINATOR
     *   else:
     *       g_state = WAITING_FOR_OK
     *       start T_OK timer (see heartbeat.c for suggested durations)
     */
}

void bully_on_election_received(const Election *msg)
{
    ESP_LOGI(TAG, "election from priority=0x%08x (mine=0x%08x)",
             (unsigned)msg->candidate_priority, (unsigned)g_my_priority);
    /* TODO(team):
     *   if (msg->candidate_priority < g_my_priority):
     *       reply OK to sender (MessageType MSG_OK)
     *       bully_on_heartbeat_timeout();  // start our own election
     *   else:
     *       // ignore — they outrank us, they'll win
     */
}

void bully_on_ok_received(const OkMessage *msg)
{
    ESP_LOGI(TAG, "OK from priority=0x%08x", (unsigned)msg->responder_priority);
    /* TODO(team):
     *   if (g_state != WAITING_FOR_OK) return;
     *   cancel T_OK
     *   g_state = ELECTION_IN_PROGRESS
     *   start T_COORD timer
     */
}

void bully_on_coordinator_received(const Coordinator *msg)
{
    ESP_LOGI(TAG, "coordinator announced priority=0x%08x", (unsigned)msg->coordinator_priority);
    /* TODO(team):
     *   adopt sender as current leader (update heartbeat target)
     *   g_state = IDLE
     *   notify wifi_uplink so backend learns about the new leader (optional —
     *     the new leader itself should also publish this)
     */
}
