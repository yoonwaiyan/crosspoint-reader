# Clock Mode

A sleep-tracker clock overlay for the Xteink X4 e-reader running CrossPoint firmware.

## What it does

- Displays the current time (`HH:MM`) and date on the e-ink screen, updated every minute.
- Logs **sleep** and **wake** events to a JSON-lines file on the SD card.
- Optionally POSTs each event to a serverless HTTPS endpoint (Lambda / Cloud Function / Cloudflare Worker).
- Syncs time from NTP on demand so the RTC stays accurate without manual configuration.

## Entering clock mode

Select **Clock** from the CrossPoint home menu. The EPUB reader is unaffected — pressing **Back** or **Confirm** returns to the home screen at any time.

## Button mapping

| Button | Action |
|--------|--------|
| **Back** or **Confirm** | Return to CrossPoint home screen |
| **PageBack (Up)** | Log a `wake` event → NTP time sync |
| **PageForward (Down)** | Log a `sleep` event |

The intent is: press **Up** when you wake up (logs the event and corrects the clock), press **Down** when you go to sleep.

## SD card files

| Path | Description |
|------|-------------|
| `/clock-config.json` | Optional configuration (see below) |
| `/sleep-log.jsonl` | Permanent append-only event log |
| `/sleep-log-pending.jsonl` | Events awaiting a successful HTTPS POST; deleted once all lines are flushed |

### Event format (`sleep-log.jsonl`)

One JSON object per line, UTC timestamp:

```jsonl
{"event":"wake","ts":"2026-05-06T07:14:08Z"}
{"event":"sleep","ts":"2026-05-06T22:53:01Z"}
```

## Configuration (`/clock-config.json`)

Place this file at the **root of the SD card**. All fields are optional — the clock works without it.

```json
{
  "endpoint_url": "https://your-endpoint.example.com/events",
  "auth_token": "your-shared-secret",
  "timezone_offset_minutes": 480,
  "ntp_server": "pool.ntp.org"
}
```

### Field reference

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `endpoint_url` | string | *(empty — POST disabled)* | HTTPS URL to POST each event to. Must be `https://`. Leave blank to disable remote logging. |
| `auth_token` | string | *(empty)* | Sent as `Authorization: Bearer <token>`. Use a shared secret known only to the device and your endpoint. |
| `timezone_offset_minutes` | integer | `0` (UTC) | Your UTC offset **in minutes**. Examples: `480` = UTC+8 (Kuala Lumpur / Singapore), `60` = UTC+1 (London BST), `-300` = UTC-5 (New York EST). Affects the displayed local time only — event timestamps are always UTC. |
| `ntp_server` | string | `"pool.ntp.org"` | NTP server used when PageBack is pressed. Change to a regional pool (e.g. `"asia.pool.ntp.org"`) for lower latency. |

### Example configs

**UTC+8 with a Cloudflare Worker endpoint:**
```json
{
  "endpoint_url": "https://my-worker.my-account.workers.dev/sleep",
  "auth_token": "s3cr3t",
  "timezone_offset_minutes": 480
}
```

**Offline-only (no remote posting), London time:**
```json
{
  "timezone_offset_minutes": 60
}
```

**UTC with custom NTP server, no auth:**
```json
{
  "endpoint_url": "https://api.example.com/log",
  "ntp_server": "time.cloudflare.com"
}
```

## HTTPS POST format

When `endpoint_url` is set, each event is POSTed as a JSON body immediately after it is logged:

```
POST https://your-endpoint.example.com/events
Content-Type: application/json
Authorization: Bearer your-shared-secret

{"event":"wake","ts":"2026-05-06T07:14:08Z"}
```

A `2xx` response marks the event as synced. Any other response (or a network failure) appends the line to `/sleep-log-pending.jsonl` for retry.

## Offline / retry behaviour

If the device is offline when an event fires, or if the POST fails, the event is queued in `/sleep-log-pending.jsonl`. The next time **PageBack** is pressed and WiFi connects successfully, all pending events are flushed to the endpoint in order. The pending file is deleted once every queued line is acknowledged with a `2xx` response.

## WiFi credentials

Clock mode reuses the WiFi credentials already saved in CrossPoint (via **Settings → WiFi Networks**). No separate WiFi setup is needed. When PageBack is pressed and no saved credentials are available, the standard WiFi selection screen appears.

## NTP sync notes

- NTP sync is triggered manually by pressing **PageBack** (Up). There is no automatic background sync.
- The sync attempt has a 5-second timeout. If it times out, `"Sync failed"` is shown briefly and the local RTC time continues to be used.
- WiFi is brought up for the NTP + POST burst only, then shut down to preserve battery.

## Ghosting / display health

- By default the clock uses a **fast partial refresh** every minute — minimal flicker, long panel life.
- A **full panel refresh** (slow, clears all ghosting) fires automatically every 60 renders (~1 hour).
