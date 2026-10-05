# Ubuntu VPS with existing Nginx sites

Use this path when host Nginx already serves other sites. Nginx handles public HTTPS; the Docker web service listens on `127.0.0.1:18080`. PHP-FPM and Redis remain internal. Use `compose.nginx.yaml` instead of `compose.production.yaml`.

```text
ESP32 → HTTPS :443 → host Nginx → 127.0.0.1:18080
                                    → Docker Caddy → Laravel → Redis
```

The commands below use a deployment account named `lotto-deploy`. Substitute your actual VPS host, administrator account, public SSH-key path and domain. They prepare one new virtual host; keep existing sites enabled.

## Deployment account

From your computer, copy your public key to the administrator account:

```sh
scp ~/.ssh/id_ed25519.pub ADMIN@VPS_HOST:/tmp/lotto-deploy.pub
```

On the VPS, as an existing administrator:

```sh
sudo adduser --disabled-password --gecos "" lotto-deploy
sudo install -d -m 700 -o lotto-deploy -g lotto-deploy /home/lotto-deploy/.ssh
sudo install -m 600 -o lotto-deploy -g lotto-deploy /tmp/lotto-deploy.pub /home/lotto-deploy/.ssh/authorized_keys
sudo install -d -m 750 -o lotto-deploy -g lotto-deploy /srv/lotto-esp32
```

If Docker is not installed, install Docker Engine and the Compose plugin using the [official Ubuntu instructions](https://docs.docker.com/engine/install/ubuntu/). Do not replace an existing working Docker installation. Compose must be 2.24.4 or newer for the port override.

```sh
sudo usermod -aG docker lotto-deploy
```

The account can manage the project's containers without general sudo membership. Docker-group membership grants root-level capabilities through Docker, as described in [Docker's post-installation documentation](https://docs.docker.com/engine/install/linux-postinstall/). Reconnect before using the account's new group membership. Keep your existing administrator session open while testing the new SSH login.

## Project and private configuration

As `lotto-deploy` on the VPS:

```sh
git clone https://github.com/Xapixowy/lotto-esp32.git /srv/lotto-esp32
cd /srv/lotto-esp32
```

If the repository is private, arrange read-only GitHub access for this account first; do not copy your personal private SSH key to the VPS.

Copy the existing private files from your computer over SSH. These examples preserve the same API tokens used by your devices:

```sh
scp .env lotto-deploy@VPS_HOST:/srv/lotto-esp32/.env
scp config/users.json lotto-deploy@VPS_HOST:/srv/lotto-esp32/config/users.json
```

On the VPS, set `APP_URL=https://your-domain` and `LOTTO_DOMAIN=your-domain` in `.env`, retaining the generated `APP_KEY` and official key. Then:

```sh
cd /srv/lotto-esp32
chmod 600 .env config/users.json
docker compose -f compose.yaml -f compose.nginx.yaml config --quiet
docker compose -f compose.yaml -f compose.nginx.yaml up -d --build
curl --fail http://127.0.0.1:18080/up
```

Always use both Compose files for this deployment, including when recreating services after user-file or environment changes:

```sh
docker compose -f compose.yaml -f compose.nginx.yaml up -d --force-recreate backend refresh
```

## Nginx virtual host and HTTPS

Ensure the domain's DNS points to the VPS. If there is an AAAA record, its IPv6 address must also reach this VPS. Public HTTP/HTTPS ports must be permitted by the existing server and provider firewalls; do not replace the server's firewall rules or SSH configuration.

As an administrator, render the new site with your actual domain:

```sh
sudo sed 's/lotto.example.com/your-domain/g' /srv/lotto-esp32/deploy/nginx/lotto.conf > /tmp/lotto-nginx.conf
sudo install -m 644 /tmp/lotto-nginx.conf /etc/nginx/sites-available/lotto
sudo ln -s /etc/nginx/sites-available/lotto /etc/nginx/sites-enabled/lotto
sudo nginx -t
sudo systemctl reload nginx
```

Check those filenames and domain do not already belong to another site before installing. `nginx -t` must succeed before reloading. The proxy explicitly forwards bearer authentication and uses Nginx's [documented proxy directives](https://nginx.org/en/docs/http/ngx_http_proxy_module.html).

If Certbot is not already installed, follow the [Ubuntu certificate setup instructions](https://ubuntu.com/server/docs/how-to/security/obtain-tls-certificates/). With the HTTP site reachable, request a certificate for this domain:

```sh
sudo certbot --nginx -d your-domain --redirect
sudo nginx -t
sudo certbot renew --dry-run
```

Certbot modifies the matching site to enable HTTPS and redirect HTTP. Preserve its managed certificate settings rather than overwriting the virtual host with the initial HTTP template afterward. Confirm the installation's certificate-renewal timer is enabled.

## Verification and updates

From your computer:

```sh
curl --fail https://your-domain/up
curl --silent --output /dev/null --write-out '%{http_code}\n' https://your-domain/api/results
```

The health endpoint should succeed and a request without a token should return 401. Check an authenticated request in your API client using a token from the private JSON file; expect 200 with eight games once results are ready, or an explicit 503 status during initial fetching/upstream failure. Keep tokens out of shared terminal output and command history.

Set firmware `BACKEND_URL` to `https://your-domain/api/results` and provide the root CA matching the issued certificate chain. Validate the connection on the actual ESP32 before completing issue #5.

For code updates, as `lotto-deploy`:

```sh
cd /srv/lotto-esp32
git pull --ff-only
docker compose -f compose.yaml -f compose.nginx.yaml up -d --build
```

This document and template have not provisioned the VPS or verified its existing configuration, public certificate, or device connection. Those checks require access to the actual server and board.
