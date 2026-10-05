<?php

namespace App\Console\Commands;

use App\Lotto\Results;
use Illuminate\Console\Command;
use Throwable;

class WorkResults extends Command
{
    protected $signature = 'lotto:work';

    protected $description = 'Keep the shared results snapshot refreshed independently of display polls';

    private bool $running = true;

    public function handle(Results $results): int
    {
        pcntl_async_signals(true);
        pcntl_signal(SIGTERM, function (): void {
            $this->running = false;
        });
        pcntl_signal(SIGINT, function (): void {
            $this->running = false;
        });
        while ($this->running) {
            try {
                if (! $results->refresh()) {
                    $this->error('Lotto refresh failed; the next attempt is scheduled in four minutes.');
                }
            } catch (Throwable $exception) {
                report($exception);
                $this->error('Results refresh encountered an error; retrying.');
            }
            sleep(5);
        }

        return self::SUCCESS;
    }
}
