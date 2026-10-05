# Ubuntu VPS with existing Nginx sites

Use this path when host Nginx already serves other sites. Nginx handles public HTTPS and connects directly to PHP-FPM on `127.0.0.1:18090`. Docker runs only Laravel, its refresh worker and Redis. Caddy is disabled in this configuration. The existing papai staging site uses ports 18000 and 18080. Use `compose.nginx.yaml` instead of `compose.production.yaml`. Before starting, check for a listener on port 18090 with `ss -ltn 'sport = :18090'`; if occupied, choose a free port and change both the Compose override and Nginx template.

```text
ESP32 → HTTPS :443 → host Nginx → 127.0.0.1:18090
                                    → Docker PHP-FPM → Laravel → Redis
```

The commands below use a deployment account named `lotto-deploy`. Substitute your actual VPS host, administrator account, public SSH-key path and domain. They prepare one new virtual host; keep existing sites enabled.

## Deployment account

From your computer, copy your public key to the administrator account:

```sh
scp ~/.ssh/lotto-deploy.pub ADMIN@VPS_HOST:/tmp/lotto-deploy.pub
```

On the VPS, as an existing administrator:

```sh
sudo adduser --disabled-password --gecos "" lotto-deploy
sudo install -d -m 700 -o lotto-deploy -g lotto-deploy /home/lotto-deploy/.ssh
sudo install -m 600 -o lotto-deploy -g lotto-deploy /tmp/lotto-deploy.pub /home/lotto-deploy/.ssh/authorized_keys
sudo install -d -m 750 -o lotto-deploy -g lotto-deploy /home/lotto-deploy/lotto-esp32
```

If Docker is not installed, install Docker Engine and the Compose plugin using the [official Ubuntu instructions](https://docs.docker.com/engine/install/ubuntu/). Do not replace an existing working Docker installation. Compose must be 2.24.4 or newer for the port override.

```sh
sudo usermod -aG docker lotto-deploy
```

The account can manage the project's containers without general sudo membership. Docker-group membership grants root-level capabilities through Docker, as described in [Docker's post-installation documentation](https://docs.docker.com/engine/install/linux-postinstall/). Reconnect before using the account's new group membership. Keep your existing administrator session open while testing the new SSH login.

Test the new login from your computer using the separate deployment key:

```sh
ssh -i ~/.ssh/lotto-deploy lotto-deploy@VPS_HOST
```

## Project and private configuration

As `lotto-deploy` on the VPS:

```sh
git clone https://github.com/Xapixowy/lotto-esp32.git /home/lotto-deploy/lotto-esp32
cd /home/lotto-deploy/lotto-esp32
```

If the repository is private, arrange read-only GitHub access for this account first; do not copy your personal private SSH key to the VPS.

Copy the existing private files from your computer over SSH. These examples preserve the same API tokens used by your devices:

```sh
scp -i ~/.ssh/lotto-deploy .env lotto-deploy@VPS_HOST:/home/lotto-deploy/lotto-esp32/.env
scp -i ~/.ssh/lotto-deploy config/users.json lotto-deploy@VPS_HOST:/home/lotto-deploy/lotto-esp32/config/users.json
```

On the VPS, set `APP_URL=https://your-domain` and `LOTTO_DOMAIN=your-domain` in `.env`, retaining the generated `APP_KEY` and official key. Then:

```sh
cd /home/lotto-deploy/lotto-esp32
chmod 600 .env config/users.json
docker compose -f compose.yaml -f compose.nginx.yaml config --quiet
docker compose -f compose.yaml -f compose.nginx.yaml up -d --build --remove-orphans backend refresh redis
```

Always use both Compose files for this deployment, including when recreating services after user-file or environment changes:

```sh
docker compose -f compose.yaml -f compose.nginx.yaml up -d --force-recreate backend refresh
```

## Cloudflare-proxied Nginx virtual host

For this VPS, Lotto uses Cloudflare proxying and Full (strict), matching the existing staging site's architecture. The owner reports that the existing staging Origin Certificate covers `*.jakubchodzinski.pl`. The Lotto virtual host references the same `/etc/ssl/cloudflare/papai-staging.pem` and `/etc/ssl/cloudflare/papai-staging.key` files. Keep their existing names and permissions; renaming them would break staging's configured paths. Multiple virtual hosts can reference the same certificate/key pair.

First verify the existing certificate's hostname coverage and validity on the VPS. Replace `your-domain` with the actual Lotto hostname:

```sh
sudo openssl x509 -in /etc/ssl/cloudflare/papai-staging.pem -noout -ext subjectAltName -dates
sudo openssl x509 -in /etc/ssl/cloudflare/papai-staging.pem -noout -checkhost your-domain
```

The wildcard covers a single subdomain level, such as `lotto.jakubchodzinski.pl`; it does not alone cover the apex `jakubchodzinski.pl` or `api.lotto.jakubchodzinski.pl`. [Cloudflare's wildcard rules](https://developers.cloudflare.com/api/resources/origin_ca_certificates/) describe this scope. If the certificate is expired or does not cover Lotto, create a separate matching Origin Certificate using [Cloudflare's instructions](https://developers.cloudflare.com/ssl/origin-configuration/origin-ca/) and update only Lotto's certificate paths. Do not overwrite the shared staging certificate/key.

After coverage and validity are confirmed, install Lotto's Nginx configuration as an administrator:

```sh
sudo install -m 644 /home/lotto-deploy/lotto-esp32/deploy/nginx/lotto.conf /etc/nginx/sites-available/lotto
sudo ln -s /etc/nginx/sites-available/lotto /etc/nginx/sites-enabled/lotto
sudo nginx -t
sudo systemctl reload nginx
```

Keep existing sites enabled. Check the new site filenames before installation. Point the Lotto DNS record to the VPS and enable proxying; ensure any AAAA record also points to this server. Set the application's SSL mode to **Full (strict)**. The virtual host trusts only [Cloudflare's published ranges](https://www.cloudflare.com/ips/); recheck the official IPv4/IPv6 lists during deployment and install updated ranges before testing/reloading Nginx if they changed.

In Cloudflare, configure [Cache Rules](https://developers.cloudflare.com/cache/how-to/cache-rules/settings/) to bypass caching for the Lotto hostname and `/api/results`. The virtual host also sends `Cache-Control: no-store`. Results must remain current and authentication must reach Laravel.

With proxying enabled, the ESP32 sees Cloudflare's public **edge** certificate. Firmware `BACKEND_ROOT_CA` must validate that public chain, rather than the Origin CA certificate used between Cloudflare and Nginx. Inspect the public hostname's actual chain before choosing the root CA. [Cloudflare explains the two certificate connections](https://developers.cloudflare.com/ssl/concepts/). Track the Origin Certificate's expiry and replace it before expiration; this path does not use Certbot.

## Verification and updates

From your computer:

```sh
curl --fail https://your-domain/up
curl --silent --output /dev/null --write-out '%{http_code}\n' https://your-domain/api/results
```

The health endpoint should succeed and a request without a token should return 401. Check an authenticated request in your API client using a token from the private JSON file; expect 200 with eight games once results are ready, or an explicit 503 status during initial fetching/upstream failure. Keep tokens out of shared terminal output and command history.

Set firmware `BACKEND_URL` to `https://your-domain/api/results` and provide the root CA matching the issued certificate chain. Validate the connection on the actual ESP32 before completing issue #5.

Port 18090 now speaks FastCGI, so `curl http://127.0.0.1:18090/up` no longer works. Test the public HTTPS hostname through Nginx instead.

### One-time transition from Docker Caddy

Install the updated `deploy/nginx/lotto.conf` as an administrator and run `nginx -t` first. Then, as `lotto-deploy`, stop the old web container before starting PHP-FPM on its former port:

```sh
cd /home/lotto-deploy/lotto-esp32
git pull --ff-only origin main
docker compose -f compose.yaml -f compose.nginx.yaml --profile standalone-web stop web
docker compose -f compose.yaml -f compose.nginx.yaml up -d --build --remove-orphans backend refresh redis
```

Reload host Nginx as an administrator and check `https://lotto.jakubchodzinski.pl/up`. This transition has a short interruption while the port changes protocols. Complete it before enabling automatic deployments. Keep the previous configuration and images available until verified.

Afterwards, use [GitHub Actions deployment](github-actions.md) for code updates. All Lotto Nginx settings remain in `deploy/nginx/lotto.conf`. Host Nginx configuration changes still require administrator installation, `nginx -t`, and reload; application deployments do not change shared server settings.

The owner has verified that the existing server API works. The direct PHP-FPM transition and automated deployment still require verification on the actual server; device HTTPS remains a physical check.
