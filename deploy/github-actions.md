# Automatic tests and VPS deployment

`.github/workflows/ci-cd.yml` tests every PR and push to `main`: Laravel tests against real Redis, Pint, PHPStan, firmware controller tests, Arduino compilation, script linting and production Nginx/FastCGI HTTP checks. Successful `main` runs build an image tagged with the commit SHA, transfer it over verified SSH and activate it on the VPS. Actions also supports a manual run on `main`.

The VPS must be Ubuntu **x86-64** (`uname -m` returns `x86_64`), with Docker, Compose 2.24.4+, `curl`, `flock`, and the `lotto-deploy` account. The GitHub runner builds Linux AMD64 images. Complete the direct PHP-FPM transition in [ubuntu-nginx.md](ubuntu-nginx.md) before enabling deployment. No registry credentials, server GitHub checkout credentials or PHP installation are needed for deployments.

## One-time GitHub setup

In repository Settings → Environments, create `production`. To deploy without human approval, leave required reviewers disabled and restrict deployment branches to `main`. GitHub documents [environments and deployment controls](https://docs.github.com/en/actions/how-tos/deploy/configure-and-manage-deployments/manage-environments).

In Settings → Secrets and variables → Actions, configure:

| Kind | Name | Value |
| --- | --- | --- |
| Variable | `VPS_HOST` | VPS hostname or IP, without username or `https://` |
| Variable | `VPS_PORT` | SSH port; defaults to `22` |
| Secret | `VPS_SSH_PRIVATE_KEY` | Dedicated Ed25519 private key for Actions |
| Secret | `VPS_SSH_KNOWN_HOSTS` | Verified SSH host-key entry, including `[host]:port` for nondefault ports |

Generate a separate CI key on your computer, choosing an unused filename. It needs no passphrase for unattended SSH:

```sh
ssh-keygen -t ed25519 -f ~/.ssh/lotto-actions -C lotto-github-actions
```

Append **only its public key** to `/home/lotto-deploy/.ssh/authorized_keys` on the VPS. Put the private file's complete contents in `VPS_SSH_PRIVATE_KEY` through GitHub's secret editor; never commit it or share it in chat. Your personal `lotto-deploy` key can retain its passphrase.

Get the server's Ed25519 host-key fingerprint through your existing trusted administrator session:

```sh
sudo ssh-keygen -lf /etc/ssh/ssh_host_ed25519_key.pub
```

On your computer, collect the public host-key entry into a temporary file and compare its fingerprint with the trusted value:

```sh
ssh-keyscan -t ed25519 -p 22 VPS_HOST > /tmp/lotto-known-hosts
ssh-keygen -lf /tmp/lotto-known-hosts
```

Use your actual host and port. Only after they match, store the file contents as `VPS_SSH_KNOWN_HOSTS`. The workflow requires strict host-key checking and never learns a server key automatically during deployment.

Set branch protection for `main` to require the `checks` job before merging. Do not put `LOTTO_API_KEY`, `APP_KEY`, user tokens or firmware Wi-Fi credentials into Actions: `.env` and `config/users.json` remain under `/home/lotto-deploy/lotto-esp32` with their existing permissions.

## Release behavior

Each release lives under `/home/lotto-deploy/lotto-esp32/releases/<commit-sha>`. It contains Compose definitions, the activation script, an immutable image archive and its image tag. The backend and worker use that same image and the existing private files. Redis is retained through deployment, including its current snapshot; no migrations run.

Deployments are serialized by GitHub concurrency and a server-side `flock`. Superseded main commits skip deployment. Activation recreates changed containers, checks public HTTPS `/up`, rejects unauthenticated results requests with 401, and verifies that the refresh worker is running. It does not call Lotto from the health check. If activation or health checks fail, it attempts to restore the last successful release and marks the workflow failed. Controlled failure tests exercise activation failure, failed health checks and successful activation; actual VPS rollback remains to be verified. Rollback failure is visible in Actions output and requires intervention. This is a brief restart, not zero downtime. Health checks verify service access, not fresh Lotto results or physical firmware behavior.

`.deployed-release` records the last successful directory. Before the first automated deployment, the existing project checkout is the fallback. Keep that checkout and its existing local image until the first deployment succeeds. To roll back deliberately:

```sh
bash /home/lotto-deploy/lotto-esp32/releases/PREVIOUS_SHA/activate-release.sh \
  /home/lotto-deploy/lotto-esp32/releases/PREVIOUS_SHA
```

The CI image excludes private files. Releases and old image archives are retained so rollback remains possible; periodically remove obsolete releases/images yourself after choosing which versions to keep. Public TLS, SSH reachability and the first actual deployment cannot be verified without your VPS setup.

Once Actions manages the stack, recreate services from the active release after editing user tokens or `.env`, preserving its image tag:

```sh
cd "$(cat /home/lotto-deploy/lotto-esp32/.deployed-release)"
BACKEND_ENV_FILE=/home/lotto-deploy/lotto-esp32/.env docker compose \
  --project-name lotto-display --env-file /home/lotto-deploy/lotto-esp32/.env \
  --env-file release.env -f compose.yaml -f compose.nginx.yaml \
  up -d --no-build --force-recreate backend refresh
```
