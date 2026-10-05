<?php

namespace App\Http\Controllers;

use App\Lotto\ResultsReader;
use Illuminate\Http\JsonResponse;
use Throwable;

class ResultsController extends Controller
{
    public function __invoke(ResultsReader $results): JsonResponse
    {
        try {
            $snapshot = $results->read();

            return response()->json($snapshot, $snapshot['status'] === 'ready' ? 200 : 503)
                ->header('Cache-Control', 'no-store');
        } catch (Throwable $exception) {
            report($exception);

            return response()->json(['schema_version' => 2, 'status' => 'backend_unavailable', 'results' => []], 503)
                ->header('Cache-Control', 'no-store');
        }
    }
}
