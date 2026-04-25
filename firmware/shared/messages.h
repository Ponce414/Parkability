/*
 * messages.h — over-the-wire message formats.
 *
 * Used for:
 *   - ESP-NOW messages between sensor nodes and zone leaders
 *   - ESP-NOW messages between leaders during Bully election
 *
 * Payload limit over ESP-NOW is ~250 bytes. All structs here stay well under that.
 * Everything is packed and uses fixed-width types so the same struct serializes
 * identically on ESP32-C3, ESP32, and (in the backend) Python via struct.unpack.
 */

#ifndef PARKING_MESSAGES_H
#define PARKING_MESSAGES_H

#include <stdint.h>

/* Bump when any on-wire struct changes. Receivers should drop mismatched versions. */
#define PROTOCOL_VERSION 1

/* Max length for a stringified spot id. See spot_id.h for encoding tradeoff.
 * 24 bytes covers "lotA/zone99/spot999" with room to spare. */
#define SPOT_ID_MAX_LEN 24

/* Message type tags. First byte of every ESP-NOW frame after the header. */
typedef enum {
    MSG_SENSOR_UPDATE   = 0x01,  /* sensor -> leader, state change */
    MSG_HEARTBEAT       = 0x02,  /* leader <-> leader, liveness */
    MSG_ELECTION        = 0x03,  /* bully: "I want to be leader" */
    MSG_OK              = 0x04,  /* bully: "I outrank you, stand down" */
    MSG_COORDINATOR     = 0x05,  /* bully: "I am the new leader" */
    MSG_REGISTER        = 0x06,  /* sensor -> leader, announce presence */
    MSG_REGISTER_ACK    = 0x07,  /* leader -> sensor, accept registration */
} MessageType;

/* Occupancy state. Kept as 1 byte to stay compact on the wire. */
typedef enum {
    SPOT_UNKNOWN  = 0,
    SPOT_FREE     = 1,
    SPOT_OCCUPIED = 2,
} SpotState;

/* Common header prefixed to every ESP-NOW payload.
 * MAC address is the sender's so receivers don't need to trust ESP-NOW's
 * own addr field (useful when replaying captured frames for tests). */
typedef struct __attribute__((packed)) {
    uint8_t  protocol_version;
    uint8_t  msg_type;           /* MessageType */
    uint8_t  sender_mac[6];      /* sender's STA MAC */
    uint32_t lamport_ts;         /* Lamport clock at send time */
} MessageHeader;

/* Sensor -> leader: a debounced state change. Sent only on transitions,
 * not on every poll. raw_distance_mm lets the backend re-compute occupancy
 * offline if the threshold is retuned. */
typedef struct __attribute__((packed)) {
    MessageHeader header;
    char     spot_id[SPOT_ID_MAX_LEN];   /* null-terminated */
    uint8_t  state;                      /* SpotState */
    uint16_t raw_distance_mm;            /* last raw reading */
    uint32_t wall_ts;                    /* sensor's monotonic ms since boot */
} SensorUpdate;

/* Leader -> leader (and optionally -> sensors): liveness ping.
 * node_priority is the 32-bit node id used by Bully to rank candidates. */
typedef struct __attribute__((packed)) {
    MessageHeader header;
    uint32_t node_priority;
    uint8_t  is_current_leader;          /* 0 or 1 */
} Heartbeat;

/* Bully: "I want to run for leader." Receiver with higher priority replies OK. */
typedef struct __attribute__((packed)) {
    MessageHeader header;
    uint32_t candidate_priority;
} Election;

/* Bully: "I'm higher priority than you — back off." */
typedef struct __attribute__((packed)) {
    MessageHeader header;
    uint32_t responder_priority;
} OkMessage;

/* Bully: "Election is over; I am the new leader." */
typedef struct __attribute__((packed)) {
    MessageHeader header;
    uint32_t coordinator_priority;
} Coordinator;

/* Sensor -> leader: "Here I am, please track my state."
 * Sent on boot and on leader change. Idempotent — leader de-dupes by spot_id. */
typedef struct __attribute__((packed)) {
    MessageHeader header;
    char     spot_id[SPOT_ID_MAX_LEN];
    uint32_t firmware_version;
} Register;

/* Leader -> sensor: registration accepted. Sensor caches leader MAC after this. */
typedef struct __attribute__((packed)) {
    MessageHeader header;
    char     spot_id[SPOT_ID_MAX_LEN];   /* echoed back for the sensor to match */
    uint8_t  accepted;                   /* 0 = rejected (e.g., wrong zone), 1 = ok */
} RegisterAck;

#endif /* PARKING_MESSAGES_H */
