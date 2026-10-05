<?php

use App\Http\Middleware\DeviceAccess;
use App\Lotto\Results;
use Illuminate\Support\Facades\Route;

Route::get('/results', function (Results $results) {
    try {
        $snapshot = $results->read();

        return response()->json($snapshot, $snapshot['status'] === 'ready' ? 200 : 503)->header('Cache-Control', 'no-store');
    } catch (Throwable) {
        return response()->json(['schema_version' => 1, 'status' => 'backend_unavailable', 'results' => []], 503)
            ->header('Cache-Control', 'no-store');
    }
})->middleware(DeviceAccess::class);
