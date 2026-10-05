<?php

$usersFile = env('API_USERS_FILE', base_path('config/users.json'));
$usersJson = is_string($usersFile) && is_file($usersFile) && is_readable($usersFile)
    ? file_get_contents($usersFile)
    : '{}';

return [
    'endpoint' => 'https://developers.lotto.pl/api/open/v1/lotteries/draw-results/last-results',
    'key' => env('LOTTO_API_KEY', ''),
    'users' => json_decode((string) $usersJson, true),
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
