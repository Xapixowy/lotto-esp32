<?php

return [
    'endpoint' => 'https://developers.lotto.pl/api/open/v1/lotteries/draw-results/last-results',
    'key' => env('LOTTO_API_KEY', ''),
    'users' => json_decode((string) env('API_USERS_JSON', '{}'), true),
    'interval' => 240,
    'stale_after' => 480,
    'games' => [
        'Lotto' => 'Lotto',
        'MiniLotto' => 'Mini Lotto',
        'MultiMulti' => 'Multi Multi',
        'EkstraPensja' => 'Ekstra Pensja',
        'Keno' => 'Keno',
        'Szybkie600' => 'Szybkie 600',
        'EuroJackpot' => 'Eurojackpot',
        'Kaskada' => 'Kaskada',
    ],
];
