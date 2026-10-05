#!/usr/bin/env bash
set -Eeuo pipefail

project=/home/lotto-deploy/lotto-esp32
release=${1:?Usage: activate-release.sh RELEASE_DIRECTORY}
case "$release" in "$project"/releases/[a-f0-9]*) ;; *) echo 'Invalid release directory' >&2; exit 1 ;; esac
test -f "$project/.env"
test -f "$project/config/users.json"
test -f "$release/release.env"

# The lock also serializes manual invocations against GitHub deployments.
exec 9>"$project/.deploy.lock"
flock -w 300 9
previous=$project
if test -f "$project/.deployed-release"; then
    previous=$(cat "$project/.deployed-release")
fi

compose() {
    local directory=$1
    shift
    local args=(--project-name lotto-display --project-directory "$directory" --env-file "$project/.env")
    if test -f "$directory/release.env"; then
        args+=(--env-file "$directory/release.env")
    fi
    BACKEND_ENV_FILE="$project/.env" docker compose "${args[@]}" \
        -f "$directory/compose.yaml" -f "$directory/compose.nginx.yaml" "$@"
}

mkdir -p "$release/config"
ln -sfn "$project/config/users.json" "$release/config/users.json"
compose "$release" config --quiet
docker load --input "$release/backend-image.tar.gz"

rollback() {
    echo 'Deployment failed; restoring the previous release' >&2
    compose "$previous" up -d --no-build --remove-orphans backend refresh redis || true
}
trap rollback ERR
compose "$release" up -d --no-build --remove-orphans backend refresh redis

# Check Laravel through the actual host Nginx and its existing TLS configuration.
# The public URL also checks the Cloudflare edge certificate and route.
domain=$(sed -n 's/^LOTTO_DOMAIN=//p' "$project/.env" | tr -d '\r')
if [[ ! "$domain" =~ ^[a-zA-Z0-9.-]+$ ]]; then
    echo 'LOTTO_DOMAIN must be a plain hostname' >&2
    false
fi
healthy=false
for _attempt in {1..30}; do
    if curl --silent --show-error --fail --max-time 5 "https://$domain/up" >/dev/null; then
        if denied=$(curl --silent --show-error --max-time 5 --output /dev/null --write-out '%{http_code}' "https://$domain/api/results") \
            && worker=$(compose "$release" ps --status running --services refresh) \
            && [[ "$denied" == 401 && "$worker" == refresh ]]; then
            healthy=true
            break
        fi
    fi
    sleep 2
done
if [[ "$healthy" != true ]]; then
    echo 'Health/authentication/worker checks failed' >&2
    false
fi
printf '%s\n' "$release" >"$project/.deployed-release.next"
mv "$project/.deployed-release.next" "$project/.deployed-release"
trap - ERR
echo "Activated $(basename "$release")"
