-- Parking lot backend schema.
--
-- One SQLite file, read by db.py. Also serves as the experiment log for
-- the concepts/ experiments; do not truncate `events` casually.

CREATE TABLE IF NOT EXISTS lots (
  lot_id TEXT PRIMARY KEY,
  name TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS zones (
  zone_id TEXT PRIMARY KEY,
  lot_id TEXT NOT NULL REFERENCES lots(lot_id),
  current_leader_mac TEXT,
  last_leader_change_ts INTEGER
);

CREATE TABLE IF NOT EXISTS spots (
  spot_id TEXT PRIMARY KEY,
  zone_id TEXT NOT NULL REFERENCES zones(zone_id),
  current_state TEXT NOT NULL CHECK (current_state IN ('OCCUPIED', 'FREE', 'UNKNOWN')),
  last_update_wall_ts INTEGER,
  last_lamport_ts INTEGER,
  last_raw_distance_mm INTEGER
);

-- Append-only log. Source of truth for propagation-delay and ordering
-- experiments. Never UPDATE or DELETE here.
CREATE TABLE IF NOT EXISTS events (
  event_id INTEGER PRIMARY KEY AUTOINCREMENT,
  spot_id TEXT NOT NULL REFERENCES spots(spot_id),
  state TEXT NOT NULL,
  lamport_ts INTEGER NOT NULL,
  wall_ts INTEGER NOT NULL,
  raw_distance_mm INTEGER,
  leader_mac TEXT,
  received_wall_ts INTEGER NOT NULL  -- backend receive time, for propagation delay analysis
);

CREATE INDEX IF NOT EXISTS idx_events_spot_wall ON events(spot_id, wall_ts);
CREATE INDEX IF NOT EXISTS idx_events_lamport ON events(lamport_ts);

-- Seed a default lot so first-boot has something to render.
INSERT OR IGNORE INTO lots (lot_id, name) VALUES ('lotA', 'Lot A (bring-up)');
INSERT OR IGNORE INTO zones (zone_id, lot_id) VALUES ('zone1', 'lotA');
