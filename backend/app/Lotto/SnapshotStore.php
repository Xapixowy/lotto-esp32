<?php

namespace App\Lotto;

use Closure;
use Illuminate\Contracts\Cache\LockProvider;
use Illuminate\Support\Facades\Cache;
use LogicException;

class SnapshotStore
{
    private const string KEY = 'lotto:snapshot';

    /** @return array<string, mixed> */
    public function current(): array
    {
        return Cache::store('redis')->get(self::KEY) ?? [];
    }

    /** @param array<string, mixed> $snapshot */
    public function replace(array $snapshot): void
    {
        Cache::store('redis')->forever(self::KEY, $snapshot);
    }

    /** @param Closure(): string $callback */
    public function withRefreshLock(Closure $callback): string|false
    {
        $store = Cache::store('redis')->getStore();
        if (! $store instanceof LockProvider) {
            throw new LogicException('Results cache must support shared locks');
        }

        return $store->lock('lotto:refresh-lock', 45)->get($callback);
    }
}
