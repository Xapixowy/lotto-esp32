FROM php:8.5-fpm-alpine AS base
RUN apk add --no-cache icu-libs libzip oniguruma \
    && apk add --no-cache --virtual .build-deps $PHPIZE_DEPS icu-dev libzip-dev oniguruma-dev \
    && docker-php-ext-install intl mbstring zip bcmath pcntl \
    && apk del .build-deps
COPY --from=composer:2 /usr/bin/composer /usr/bin/composer
WORKDIR /srv/backend
COPY backend/composer.json backend/composer.lock ./

FROM base AS production
RUN composer install --no-dev --no-interaction --no-scripts --prefer-dist
COPY backend/ .
RUN composer dump-autoload --no-dev --optimize --no-scripts \
    && chown -R www-data:www-data storage bootstrap/cache
COPY docker/entrypoint.sh /usr/local/bin/lotto-entrypoint
RUN chmod +x /usr/local/bin/lotto-entrypoint
ENTRYPOINT ["lotto-entrypoint"]
CMD ["php-fpm"]

FROM base AS development
RUN composer install --no-interaction --no-scripts --prefer-dist
COPY backend/ .
COPY tests/ /srv/tests/
RUN composer dump-autoload --optimize --no-scripts
CMD ["php", "artisan", "test"]

FROM caddy:2-alpine AS web
COPY backend/public /srv/backend/public
COPY docker/Caddyfile /etc/caddy/Caddyfile
