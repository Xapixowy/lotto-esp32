#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
scratch=$(mktemp -d)
project=lotto-vps-smoke
cleanup() {
    docker compose --project-name "$project" --env-file .env.example -f compose.yaml -f compose.nginx.yaml -f "$scratch/override.yaml" down >/dev/null 2>&1 || true
    rm -rf "$scratch"
}
trap cleanup EXIT
openssl req -x509 -newkey rsa:2048 -nodes -days 1 -subj '/CN=lotto.jakubchodzinski.pl' \
    -addext 'subjectAltName=DNS:lotto.jakubchodzinski.pl' \
    -keyout "$scratch/papai-staging.key" -out "$scratch/papai-staging.pem" >/dev/null 2>&1
chmod 644 "$scratch/papai-staging.key"
sed 's/127.0.0.1:18090/backend:9000/g' deploy/nginx/lotto.conf > "$scratch/lotto.conf"
cat > "$scratch/override.yaml" <<EOF
services:
  backend:
    ports: !reset []
    volumes: !override
      - $root/tests/fixtures/users.json:/run/lotto/users.json:ro
  refresh:
    volumes: !override
      - $root/tests/fixtures/users.json:/run/lotto/users.json:ro
  smoke:
    image: nginx:stable-alpine
    ports:
      - '127.0.0.1::443'
    volumes:
      - $scratch/lotto.conf:/etc/nginx/conf.d/default.conf:ro
      - $scratch:/etc/ssl/cloudflare:ro
    depends_on:
      - backend
EOF
export BACKEND_ENV_FILE="$root/.env.example"
compose=(docker compose --project-name "$project" --env-file .env.example -f compose.yaml -f compose.nginx.yaml -f "$scratch/override.yaml")
"${compose[@]}" up -d --build backend redis smoke
"${compose[@]}" exec -T smoke nginx -t
port=$("${compose[@]}" port smoke 443)
port=${port##*:}
url="https://lotto.jakubchodzinski.pl:$port"
curl_args=(--silent --show-error --cacert "$scratch/papai-staging.pem" --resolve "lotto.jakubchodzinski.pl:$port:127.0.0.1" --max-time 5)
for _attempt in {1..30}; do
    if curl "${curl_args[@]}" --fail "$url/up" >/dev/null; then break; fi
    sleep 1
done
test "$(curl "${curl_args[@]}" --output /dev/null --write-out '%{http_code}' "$url/up")" = 200
test "$(curl "${curl_args[@]}" --output /dev/null --write-out '%{http_code}' "$url/api/results")" = 401
token=$(docker compose -f compose.test.yaml run --rm --no-deps tests php -r 'echo array_values(json_decode(file_get_contents("/srv/tests/fixtures/users.json"), true))[0];')
printf 'Authorization: Bearer %s\n' "$token" > "$scratch/header"
test "$(curl "${curl_args[@]}" --header @"$scratch/header" --output "$scratch/result.json" --write-out '%{http_code}' "$url/api/results")" = 503
rg -q '"status":"fetching"' "$scratch/result.json"
test "$(curl "${curl_args[@]}" --output /dev/null --write-out '%{http_code}' "$url/unknown")" = 404
echo 'Host Nginx → PHP-FPM: health 200, denied 401, authenticated fetching 503, unknown 404'
