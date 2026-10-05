<?php

namespace App\Http\Middleware;

use Closure;
use Illuminate\Http\Request;
use Symfony\Component\HttpFoundation\Response;

class DeviceAccess
{
    public function handle(Request $request, Closure $next): Response
    {
        $token = $request->bearerToken();
        $users = config('lotto.users');
        if (! is_array($users) || $users === [] || array_is_list($users)) {
            return response()->json(['schema_version' => 2, 'status' => 'backend_unavailable'], 503);
        }
        foreach ($users as $name => $configured) {
            if (! is_string($name) || $name === '' || ! is_string($configured) || strlen($configured) < 32 || str_contains($configured, 'REPLACE_')) {
                return response()->json(['schema_version' => 2, 'status' => 'backend_unavailable'], 503);
            }
        }
        foreach ($users as $configured) {
            if (is_string($token) && hash_equals($configured, $token)) {
                return $next($request);
            }
        }

        return response()->json(['schema_version' => 2, 'status' => 'access_denied'], 401);
    }
}
