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
for service in backend refresh; do
    if [[ "$*" == *"exec -T $service php -r"* ]]; then
        if [[ "${SCENARIO:-}" == "$service-php-mismatch" ]]; then
            printf '8.5.11'
        else
            printf '8.5.12'
        fi
    fi
done
SH
cat > /tmp/commands/curl <<'SH'
#!/bin/bash
if [[ "${SCENARIO:-}" == health-failure ]]; then exit 7; fi
if [[ "$*" == *'/api/results'* ]]; then printf 401; fi
SH
printf '#!/bin/sh\nexit 0\n' > /tmp/commands/sleep
chmod +x /tmp/commands/*

for scenario in activation-failure backend-php-mismatch refresh-php-mismatch health-failure success; do
    export SCENARIO=$scenario
    previous="$project/releases/def"
    release="$project/releases/abc"
    mkdir -p "$previous" "$release"
    cp /src/compose.yaml /src/compose.nginx.yaml "$previous/"
    cp /src/compose.yaml /src/compose.nginx.yaml "$release/"
    printf 'BACKEND_IMAGE=previous\n' > "$previous/release.env"
    printf 'BACKEND_IMAGE=current\nEXPECTED_PHP_VERSION=8.5.12\n' > "$release/release.env"
    printf '%s\n' "$previous" > "$project/.deployed-release"
    : > /tmp/docker-calls
    if bash /src/deploy/activate-release.sh "$release" > /tmp/activation-output 2>&1; then
        test "$scenario" = success
        test "$(cat "$project/.deployed-release")" = "$release"
        if grep -q 'restoring the previous release' /tmp/activation-output; then exit 1; fi
        for service in backend refresh; do
            grep -q "exec -T $service php -r" /tmp/docker-calls
        done
    else
        test "$scenario" != success
        test "$(cat "$project/.deployed-release")" = "$previous"
        grep -q 'restoring the previous release' /tmp/activation-output
        grep -q "$previous/compose.yaml.*up -d --no-build --remove-orphans backend refresh redis" /tmp/docker-calls
        if [[ "$scenario" == *-php-mismatch ]]; then
            grep -q 'expected PHP 8.5.12, got 8.5.11' /tmp/activation-output
        fi
    fi
    echo "Deployment scenario passed: $scenario"
done

for version in '' '8.5'; do
    printf 'BACKEND_IMAGE=current\nEXPECTED_PHP_VERSION=%s\n' "$version" > "$release/release.env"
    : > /tmp/docker-calls
    if bash /src/deploy/activate-release.sh "$release" > /tmp/activation-output 2>&1; then exit 1; fi
    grep -q 'Release must declare an exact EXPECTED_PHP_VERSION' /tmp/activation-output
    test ! -s /tmp/docker-calls
    test "$(cat "$project/.deployed-release")" = "$release"
done
echo 'Release metadata validation passed: missing and incomplete PHP versions rejected before activation'
