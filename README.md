# Lotto Display

Polish lottery results on an ESP32 Cheap Yellow Display, backed by a Dockerized Laravel API and ephemeral Redis. No database. The backend replaces one shared snapshot every four minutes; devices read it every 15 seconds. Dark-mode results use large numbers and show one complete result group at a time.

## Run the backend with Docker

You need Docker with Compose. PHP, Composer, Redis, and a web server run inside containers.

```sh
cp .env.example .env
cp config/users.example.json config/users.json
docker compose build
docker compose run --rm --no-deps --entrypoint php backend artisan key:generate --show
```

Copy the generated `base64:…` value into `APP_KEY` in `.env`. Set `LOTTO_API_KEY` to your official Lotto key. Generate an independent access token for each user:

```sh
docker compose run --rm --no-deps --entrypoint php backend -r 'echo bin2hex(random_bytes(32)), PHP_EOL;'
```

Put the tokens in the private `config/users.json` file, for example:

```json
{
  "owner": "your-generated-token",
  "guest": "another-generated-token"
}
```

These are named API permissions, with no login or registration. Each token grants the same read access. Use at least 32 characters per token; placeholder tokens are rejected. Keep `.env` and `config/users.json` private. Docker mounts the JSON file read-only into the backend and refresh worker; it is excluded from Git and image builds. Missing, malformed or invalid user configuration denies API access. `API_USERS_JSON` is no longer used.

```sh
docker compose up -d
docker compose logs --tail=30 refresh
```

The local endpoint is `http://localhost:8080/api/results`. Supply `Authorization: Bearer <your-token>` in your API client. The ESP32 requires HTTPS; the local HTTP endpoint is for backend checks.

`backend` runs PHP-FPM, `web` proxies requests through Caddy, `refresh` fetches independently, and `redis` holds results. Redis has no published port or persistent volume. Restarting Redis clears results; the worker refetches within five seconds when the cache is empty. Multiple worker instances share a Redis lock and due-time marker, so they do not multiply upstream calls.

After editing `.env` or `config/users.json`, recreate both Laravel services so their startup-cached configuration agrees. Changes to the JSON file do not grant or revoke access until recreation:

```sh
docker compose up -d --force-recreate backend refresh
```

To stop the stack:

```sh
docker compose down
```

## Deploy on a VPS

If your Ubuntu VPS already uses Nginx for other sites, follow [the Nginx deployment guide](deploy/ubuntu-nginx.md). Host Nginx connects directly to loopback-only PHP-FPM on port 18090; this deployment runs three containers and disables Caddy. Use `compose.nginx.yaml` for that path. Existing installations need the documented one-time Nginx/FastCGI transition.

[GitHub Actions setup](deploy/github-actions.md) enables automatic tests for PRs and deployment after successful merges to `main`, with immutable images, health checks and rollback. Configure the VPS variables and dedicated SSH secrets once. The private API settings stay on the server.

For a VPS where Caddy should own public ports 80/443, use the following path:

Point a domain's DNS at your VPS and allow ports 80 and 443. Set `LOTTO_DOMAIN` and `APP_URL=https://your-domain` in `.env`. Use Docker Compose 2.24.4 or later; the production override uses `!override`.

```sh
docker compose -f compose.yaml -f compose.production.yaml up -d --build
```

Caddy obtains and renews a public HTTPS certificate. Its certificate state uses Docker volumes; result data does not. The public device URL is `https://your-domain/api/results`. PHP-FPM and Redis remain internal. Recreate services after changing tokens using the same two Compose files.

## Arduino IDE: build and flash yourself

1. Install **esp32 by Espressif Systems** in Boards Manager. Its board URL is `https://espressif.github.io/arduino-esp32/package_esp32_index.json`.
2. In Library Manager install **ArduinoJson** (7.x), **Adafruit ILI9341**, **Adafruit ST7735 and ST7789 Library**, **U8g2_for_Adafruit_GFX**, and **XPT2046_Touchscreen**. Accept Adafruit's dependencies.
3. Open `firmware/LottoDisplay/LottoDisplay.ino`.
4. Select **ESP32 Dev Module**, your USB port, and **Partition Scheme → Huge APP (3MB No OTA/1MB SPIFFS)**. Start with upload speed 115200 if necessary.
5. Leave display-check mode enabled for the first upload. With no `Secrets.h`, the sketch uses `Secrets.example.h` and does not connect to Wi-Fi or any API. Click **Verify**, then **Upload**. If connecting stalls, hold BOOT while the upload begins and release it once writing starts.
6. Open Serial Monitor at 115200. Confirm landscape output, readable Polish text, arrows, and the lock. Touch diagnostics print raw and mapped coordinates. Sample numbers are demonstration data, not live results.

### Board and touch verification

The supplied hardware profile targets the standard **ESP32-2432S028R**: ILI9341 LCD with XPT2046 resistive touch, separate SPI buses, and backlight GPIO 21. Your board's exact driver is still unverified; `ESP32-32E` and `HSD028309 G5` do not establish the panel controller.

If the display is blank or garbled, check the PCB model and seller documentation before changing `Hardware.h`. It includes an ST7789 switch, rotation/inversion settings, and pin mappings for known variants. Do not assume all yellow boards share the same profile. Use the [CYD reference repository](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) to identify the board.

In display-check mode, tap the edges and controls, inspect raw coordinates, and adjust touch bounds, swapping, or flips in `Hardware.h` as needed. Confirm the arrows and lock correspond to their physical touch regions before enabling live mode. Display-check data will eventually show the freshness screen after eight minutes; reset the board to repeat checks.

### Enable live results

Copy `Secrets.example.h` to `Secrets.h` in the sketch folder. This file is ignored by Git. Set:

- `DISPLAY_CHECK_ONLY` to `false`.
- Your Wi-Fi SSID/password (ESP32 uses 2.4 GHz Wi-Fi).
- `BACKEND_URL` to the full HTTPS results endpoint.
- `API_TOKEN` to one configured backend token.
- `BACKEND_ROOT_CA` to the root certificate that validates your VPS's HTTPS chain, in PEM format. Get it from the certificate authority. This is the root CA, not the server's leaf certificate.

Language is Polish. Rebuild and upload after changing credentials. Firmware uses certificate validation and obtains time through NTP for TLS verification; it does not disable HTTPS verification or follow redirects with your token. Until Wi-Fi/time/TLS is ready, it shows a connection status and retries.

## Display behavior

- Order and content come from the backend: Lotto, Mini Lotto, Multi Multi, Ekstra Pensja, Keno, Szybkie 600, Eurojackpot, Kaskada by default.
- Each slide has labeled groups. One group fits on one screen: up to 20 numbers in a five-column, four-row grid using the large number font. Groups are never split or scrolled. Each group stays for 10 seconds before the next group/game; oversized groups show a data error rather than truncated results.
- Arrows move between group pages, continuing into adjacent games and wrapping at the ends. The bottom-right lock holds the exact page for five minutes. Arrows still work while locked and do not extend the lock. Tap the lock again to unlock immediately.
- The top progress line shrinks until the next page. It resets on manual navigation or unlocking and stays gray while locked. Arrow and lock taps briefly invert the button colors for 150 ms.
- `Lotto` and `Sync` appear at bottom-left in Warsaw time. They mean the last successful upstream fetch and last successful device retrieval, respectively, rather than draw times.
- Normal polls retain results and show `Odświeżanie...` at upper-left. Errors replace the entire screen and hide controls. Recovery preserves the selected game and any unexpired lock.
- Wi-Fi failure, unreachable API, denied access, unavailable backend, failed Lotto refresh, invalid responses and stale data are distinct statuses. Results are stale after more than eight minutes without a successful official fetch.

## API contract

`GET /api/results` returns `schema_version`, `status`, `lotto_fetched_at` (Unix seconds or null), `server_time` (Unix seconds), Warsaw `lotto_time`/`sync_time` strings, and `results`.

Each result has a stable `id`, `label`, and ordered `groups`. Schema version 2 draw groups contain only `label` and `numbers`, an ascending array of `{value, type}` objects. Types are `simple` and `special`; firmware shows special numbers in yellow/gold on the same draw page. Multi Multi Plus marks an existing winning number, while separate special pools (Eurojackpot and Ekstra Pensja) retain both entries if values overlap. There is no separate “Dodatkowe” page. Version 2 removes group-level `kind` and `value`; backend and firmware must be updated together. Backend snapshots replace previous results; neither component accumulates historical draws. The official all-games response determines how many groups are available.

Ready responses use HTTP 200. `fetching`, `lotto_refresh_failed`, `stale`, and `backend_unavailable` use HTTP 503, with no displayed results. Invalid credentials use HTTP 401 and `access_denied`. Errors never expose upstream exceptions or tokens. Firmware rejects unsupported schema versions, duplicate game IDs, invalid or unsorted numbers and unsupported number types.

The backend validates all eight required games before atomic replacement, preserves the previous successful snapshot internally after failure, and retries on the four-minute schedule. Requests never initiate official Lotto calls. Backend payloads support at most 20 values and 16 groups per game, with a 24 KB encoded result budget. Firmware accepts up to 20 values per group so each complete group fits on one screen.

## Tests and type checks, entirely in Docker

```sh
docker compose -f compose.test.yaml build tests
docker compose -f compose.test.yaml run --rm tests
docker compose -f compose.test.yaml run --rm tests composer typecheck
docker compose -f compose.test.yaml run --rm firmware-tests
```

Backend feature tests use isolated real Redis and fake external Lotto responses. They verify snapshot replacement, access, scheduling/nonoverlap, errors and recovery. The startup-access integration test runs the actual configuration-caching entrypoint and a local HTTP server, checks two clients against the shared snapshot, edits the private JSON file, and verifies permissions change only after restart without replacing the snapshot. Its configuration cache and user file are isolated and removed afterward. Native C++ tests exercise display-controller timing and touch behavior. Most fixtures are synthetic; `lotto-upstream.authenticated.json` contains only public draw fields from one authenticated official response on 5 October 2026. Its regression checks selected games, excludes Plus/Premia, and verifies sorted numbers, special pools, Multi Multi Plus and Warsaw draw times without live requests during tests.

Optional Docker compile check without touching the connected ESP32:

```sh
docker compose -f compose.test.yaml build firmware-build
docker compose -f compose.test.yaml run --rm firmware-build
```

The first build downloads ESP32 tooling and can take several minutes. This compiles the sketch and does not upload. The baseline compiler image uses ESP32 core 3.3.0; Arduino IDE 3.3.x is the intended board-package family.

## Physical smoke check still required

After uploading, verify Polish glyphs, dark-mode readability at the viewing distance, touch calibration, paging, arrows and the lock. Then verify live results on the device and HTTPS connection with your configured token/root CA. Disconnect Wi-Fi, revoke a token in `config/users.json` and recreate the backend, stop the refresh worker for over eight minutes, and restart Redis; confirm each status and recovery. One authenticated official response has validated all eight backend mappings; tests cannot establish your panel driver, physical usability or the actual VPS certificate chain.
