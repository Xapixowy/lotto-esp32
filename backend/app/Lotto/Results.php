<?php

namespace App\Lotto;

use Throwable;

class Results
{
    public function __construct(
        private LottoClient $client,
        private SlideBuilder $builder,
        private SnapshotStore $snapshots,
    ) {}

    public function refresh(): bool
    {
        $outcome = $this->snapshots->withRefreshLock(function (): string {
            $previous = $this->snapshots->current();
            if (isset($previous['attempted_at']) && now()->timestamp - $previous['attempted_at'] < config('lotto.interval')) {
                return 'skipped';
            }

            $previous['attempted_at'] = now()->timestamp;
            $this->snapshots->replace($previous);

            try {
                $slides = $this->builder->build($this->client->fetch(), config('lotto.games'));
                $this->snapshots->replace([
                    'status' => 'ready',
                    'attempted_at' => $previous['attempted_at'],
                    'lotto_fetched_at' => now()->timestamp,
                    'results' => $slides,
                ]);

                return 'ready';
            } catch (Throwable $exception) {
                report($exception);
                $previous['status'] = 'lotto_refresh_failed';
                $this->snapshots->replace($previous);

                return 'failed';
            }
        });

        return $outcome !== 'failed';
    }
}
