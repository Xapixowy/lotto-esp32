#!/bin/sh
set -eu
php artisan config:cache --no-interaction
exec "$@"
