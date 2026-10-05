#!/usr/bin/env bash
# Runs inside an isolated Debian container, with controlled Docker/HTTP boundaries.
set -euo pipefail
project=/home/lotto-deploy/lotto-esp32
mkdir -p "$project/config" /tmp/commands
printf 'LOTTO_DOMAIN=lotto.example.com\n' > "$project/.env"
printf '{}\n' > "$project/config/users.json"
export PATH="/tmp/commands:$PATH"
cat > /tmp/commands/docker <<'SH'
#!/bin/bash
printf '%s\n' "$*" >> /tmp/docker-calls
if [[ "$*" == *'up -d'* && "$*" == *'/releases/abc/'* && "${SCENARIO:-}" == activation-failure ]]; then
    exit 1
fi
if [[ "$*" == *'ps --status running --services refresh'* ]]; then echo refresh; fi
SH
cat > /tmp/commands/curl <<'SH'
#!/bin/bash
if [[ "${SCENARIO:-}" == health-failure ]]; then exit 7; fi
if [[ "$*" == *'/api/results'* ]]; then printf 401; fi
SH
printf '#!/bin/sh\nexit 0\n' > /tmp/commands/sleep
chmod +x /tmp/commands/*

for scenario in activation-failure health-failure success; do
    export SCENARIO=$scenario
    previous="$project/releases/def"
    release="$project/releases/abc"
    mkdir -p "$previous" "$release"
    cp /src/compose.yaml /src/compose.nginx.yaml "$previous/"
    cp /src/compose.yaml /src/compose.nginx.yaml "$release/"
    printf 'BACKEND_IMAGE=previous\n' > "$previous/release.env"
    printf 'BACKEND_IMAGE=current\n' > "$release/release.env"
    printf '%s\n' "$previous" > "$project/.deployed-release"
    : > /tmp/docker-calls
    if bash /src/deploy/activate-release.sh "$release" > /tmp/activation-output 2>&1; then
        test "$scenario" = success
        test "$(cat "$project/.deployed-release")" = "$release"
        if grep -q 'restoring the previous release' /tmp/activation-output; then exit 1; fi
    else
        test "$scenario" != success
        test "$(cat "$project/.deployed-release")" = "$previous"
        grep -q 'restoring the previous release' /tmp/activation-output
        grep -q "$previous/compose.yaml.*up -d --no-build --remove-orphans backend refresh redis" /tmp/docker-calls
    fi
    echo "Deployment scenario passed: $scenario"
done
