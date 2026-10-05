<?php

namespace App\Lotto;

use Illuminate\Support\Facades\Http;
use InvalidArgumentException;

class LottoClient
{
    /** @return list<mixed> */
    public function fetch(): array
    {
        if (config('lotto.key') === '') {
            throw new InvalidArgumentException('Missing official API key');
        }
        $rows = Http::acceptJson()->withHeaders(['secret' => config('lotto.key')])
            ->connectTimeout(5)->timeout(20)->get(config('lotto.endpoint'))->throw()->json();
        if (! is_array($rows) || ! array_is_list($rows)) {
            throw new InvalidArgumentException('Invalid results list');
        }

        return $rows;
    }
}
