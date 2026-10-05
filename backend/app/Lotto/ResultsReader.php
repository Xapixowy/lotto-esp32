<?php

namespace App\Lotto;

use Carbon\CarbonImmutable;

class ResultsReader
{
    public function __construct(private SnapshotStore $snapshots) {}

    /**
     * @return array{schema_version: int, status: string, lotto_fetched_at: int|null, server_time: int, lotto_time: string|null, sync_time: string, results: list<array{id: string, label: string, groups: list<array{label: string, numbers: list<array{value: int, type: string}>}>}>}
     */
    public function read(): array
    {
        $snapshot = $this->snapshots->current() ?: [
            'status' => 'fetching', 'lotto_fetched_at' => null, 'results' => [],
        ];
        $status = $snapshot['status'] ?? 'fetching';
        $fetched = $snapshot['lotto_fetched_at'] ?? null;
        if ($status === 'ready' && is_int($fetched) && now()->timestamp - $fetched > config('lotto.stale_after')) {
            $status = 'stale';
        }

        return [
            'schema_version' => 2,
            'status' => $status,
            'lotto_fetched_at' => $fetched,
            'server_time' => now()->timestamp,
            'lotto_time' => $fetched ? CarbonImmutable::createFromTimestamp($fetched)->timezone('Europe/Warsaw')->format('H:i:s') : null,
            'sync_time' => now()->timezone('Europe/Warsaw')->format('H:i:s'),
            'results' => $status === 'ready' ? array_map(function (array $slide): array {
                $slide['groups'] = array_map(fn (array $group): array => [
                    'label' => $group['label'],
                    'numbers' => $group['numbers'],
                ], $slide['groups']);

                return $slide;
            }, $snapshot['results']) : [],
        ];
    }
}
