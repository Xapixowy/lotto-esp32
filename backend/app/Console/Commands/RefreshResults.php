<?php

namespace App\Console\Commands;

use App\Lotto\Results;
use Illuminate\Console\Command;

class RefreshResults extends Command
{
    protected $signature = 'lotto:refresh';

    protected $description = 'Replace the shared lottery results snapshot';

    public function handle(Results $results): int
    {
        if (! $results->refresh()) {
            $this->error('Lotto refresh failed. Retrying at the next scheduled interval.');

            return self::FAILURE;
        }

        return self::SUCCESS;
    }
}
