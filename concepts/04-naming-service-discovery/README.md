# 04 — Naming & Service Discovery

How nodes find each other and how we name resources.

## What we did

**Hierarchical names.** Every spot has the form `lot/zone/spot`, e.g.
`lotA/zone2/spot17`. This embeds the routing in the name: a leader
processing an incoming sensor update can tell at a glance whether it
belongs to its zone.

**Discovery via Register/RegisterAck.** Sensors don't know the leader's MAC
at compile time (well — they do *now*, but only because we hard-code it for
bring-up, see `LEADER_MAC` in `config.h`). On boot, sensors send
`MSG_REGISTER` to the configured leader; the leader replies with
`MSG_REGISTER_ACK` to confirm the spot belongs to its zone. This handshake
is idempotent so reboots and leader changes are no-ops.

**No DNS / mDNS.** We didn't add it because the LAN is fixed for the class
demo. Production deployment would want it.

## Key code

- [`firmware/shared/spot_id.h`](../../firmware/shared/spot_id.h) and [`spot_id.c`](../../firmware/shared/spot_id.c) — name encoding, parsing, zone-membership check
- [`firmware/shared/messages.h`](../../firmware/shared/messages.h) — `MSG_REGISTER`/`MSG_REGISTER_ACK` definitions

## Tradeoff we made

Strings on the wire (24 bytes per id) are wasteful but readable everywhere.
A packed bitfield (~4 bytes) would save room but require lookup tables in
every log line. We chose strings; `spot_id.c` is the single place to flip
to bitfields if we ever need it.
