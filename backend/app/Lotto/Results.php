<?php

namespace App\Lotto;

use Carbon\CarbonImmutable;
use Illuminate\Contracts\Cache\LockProvider;
use Illuminate\Support\Facades\Cache;
use Illuminate\Support\Facades\Http;
use InvalidArgumentException;
use Throwable;

class Results
{
    public function refresh(): bool
    {
        $store = Cache::store('redis')->getStore();
        if (! $store instanceof LockProvider) {
            throw new \LogicException('Results cache must support shared locks');
        }
        $outcome = $store->lock('lotto:refresh-lock', 45)->get(function (): string {
            $previous = Cache::store('redis')->get('lotto:snapshot', []);
            if (isset($previous['attempted_at']) && now()->timestamp - $previous['attempted_at'] < config('lotto.interval')) {
                return 'skipped';
            }
            $previous['attempted_at'] = now()->timestamp;
            Cache::store('redis')->forever('lotto:snapshot', $previous);
            try {
                if (config('lotto.key') === '') {
                    throw new InvalidArgumentException('Missing official API key');
                }
                $this->fetch($previous['attempted_at']);

                return 'ready';
            } catch (Throwable) {
                $previous['status'] = 'lotto_refresh_failed';
                Cache::store('redis')->forever('lotto:snapshot', $previous);

                return 'failed';
            }
        });

        return $outcome !== 'failed';
    }

    private function fetch(int $attemptedAt): void
    {
        $rows = Http::acceptJson()->withHeaders(['secret' => config('lotto.key')])
            ->connectTimeout(5)->timeout(20)->get(config('lotto.endpoint'))->throw()->json();
        if (! is_array($rows) || ! array_is_list($rows)) {
            throw new InvalidArgumentException('Invalid results list');
        }
        $slides = [];
        foreach (config('lotto.games') as $id => $label) {
            $row = collect($rows)->firstWhere('gameType', $id);
            if (! is_array($row) || ! isset($row['drawDate'], $row['results']) || ! is_array($row['results']) || $row['results'] === []) {
                throw new InvalidArgumentException('Missing required game results');
            }
            $groups = [];
            foreach ($row['results'] as $result) {
                if (! is_array($result)) {
                    throw new InvalidArgumentException('Invalid draw result');
                }
                if (($result['gameType'] ?? $id) !== $id) {
                    continue;
                }
                $date = $result['drawDate'] ?? $row['drawDate'];
                if (! is_string($date) || ! preg_match('/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}/', $date)) {
                    throw new InvalidArgumentException('Invalid draw date');
                }
                $drawLabel = CarbonImmutable::parse($date, 'Europe/Warsaw')->timezone('Europe/Warsaw')->format('d.m.Y H:i');
                $values = $result['resultsJson'] ?? null;
                if (! is_array($values) || ! array_is_list($values) || $values === [] || count($values) > 100 || array_any($values, fn ($value) => ! is_int($value) || $value < 0 || $value > 999)) {
                    throw new InvalidArgumentException('Invalid winning values');
                }
                $groups[] = [
                    'label' => $drawLabel,
                    'kind' => 'simple',
                    'value' => $values,
                ];
                $additional = $result['specialResults'] ?? [];
                if (! is_array($additional) || ! array_is_list($additional) || count($additional) > 100 || array_any($additional, fn ($value) => ! is_int($value) || $value < 0 || $value > 999)) {
                    throw new InvalidArgumentException('Invalid additional values');
                }
                if ($additional !== []) {
                    $groups[] = ['label' => $drawLabel.' · Dodatkowe', 'kind' => 'additional', 'value' => $additional];
                }
            }
            if ($groups === [] || count($groups) > 16) {
                throw new InvalidArgumentException('Invalid result groups');
            }
            $slides[] = ['id' => $id, 'label' => $label, 'groups' => $groups];
        }
        if (strlen(json_encode($slides, JSON_THROW_ON_ERROR)) > 24000) {
            throw new InvalidArgumentException('Results exceed the device payload budget');
        }
        Cache::store('redis')->forever('lotto:snapshot', [
            'status' => 'ready', 'attempted_at' => $attemptedAt, 'lotto_fetched_at' => now()->timestamp, 'results' => $slides,
        ]);
    }

    public function read(): array
    {
        $snapshot = Cache::store('redis')->get('lotto:snapshot') ?? [
            'status' => 'fetching', 'lotto_fetched_at' => null, 'results' => [],
        ];
        $status = $snapshot['status'] ?? 'fetching';
        $fetched = $snapshot['lotto_fetched_at'] ?? null;
        if ($status === 'ready' && is_int($fetched) && now()->timestamp - $fetched > config('lotto.stale_after')) {
            $status = 'stale';
        }

        return [
            'schema_version' => 1,
            'status' => $status,
            'lotto_fetched_at' => $fetched,
            'server_time' => now()->timestamp,
            'lotto_time' => $fetched ? CarbonImmutable::createFromTimestamp($fetched)->timezone('Europe/Warsaw')->format('H:i:s') : null,
            'sync_time' => now()->timezone('Europe/Warsaw')->format('H:i:s'),
            'results' => $status === 'ready' ? $snapshot['results'] : [],
        ];
    }
}
