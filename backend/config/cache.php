<?php

return [
    'default' => env('CACHE_STORE', 'redis'),
    'stores' => [
        'array' => ['driver' => 'array'],
        'redis' => ['driver' => 'redis', 'connection' => 'cache', 'lock_connection' => 'cache'],
    ],
    'prefix' => env('CACHE_PREFIX', 'lotto:'),
    'serializable_classes' => false,
];
