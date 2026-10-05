<?php

namespace Tests\Feature;

use Illuminate\Http\Client\ConnectionException;
use Illuminate\Http\Client\Factory;
use Illuminate\Http\Client\Pool;
use Illuminate\Support\Facades\Cache;
use Illuminate\Support\Facades\Http;
use PHPUnit\Framework\Attributes\DataProvider;
use RuntimeException;
use Symfony\Component\Process\Process;
use Tests\TestCase;

class StartupAccessTest extends TestCase
{
    private ?Process $server = null;

    private string $directory;

    private string $url;

    protected function setUp(): void
    {
        parent::setUp();
        $this->directory = sys_get_temp_dir().'/lotto-startup-'.bin2hex(random_bytes(8));
        mkdir($this->directory, 0700);
    }

    protected function tearDown(): void
    {
        $this->server?->stop();
        if (isset($this->directory)) {
            @unlink($this->directory.'/config.php');
            @unlink($this->directory.'/users.json');
            rmdir($this->directory);
        }
        parent::tearDown();
    }

    public function test_startup_loads_users_and_restart_revokes_a_token_without_refetching_results(): void
    {
        Cache::store('redis')->flush();
        config(['lotto.key' => 'fake-key']);
        Http::preventStrayRequests();
        $rows = json_decode(file_get_contents(base_path('../tests/fixtures/lotto-upstream.json')), true);
        Http::fake(['developers.lotto.pl/*' => Http::response($rows)]);
        $this->artisan('lotto:refresh')->assertExitCode(0);

        $owner = str_repeat('a', 32);
        $guest = str_repeat('b', 32);
        $newGuest = str_repeat('c', 32);
        $client = new Factory;
        file_put_contents($this->directory.'/users.json', json_encode(['owner' => $owner, 'guest' => $guest], JSON_THROW_ON_ERROR));
        $this->startServer();

        $responses = $client->pool(fn (Pool $pool) => [
            $pool->as('owner')->withToken($owner)->get($this->url.'/api/results'),
            $pool->as('guest')->withToken($guest)->get($this->url.'/api/results'),
        ]);
        $first = $responses['owner'];
        $second = $responses['guest'];
        $this->assertSame(200, $first->status());
        $this->assertSame(200, $second->status());
        $this->assertSame($first->json('results'), $second->json('results'));
        $this->assertSame($first->json('lotto_fetched_at'), $second->json('lotto_fetched_at'));
        $this->assertSame(401, $client->get($this->url.'/api/results')->status());
        $this->assertSame(401, $client->withToken($newGuest)->get($this->url.'/api/results')->status());

        file_put_contents($this->directory.'/users.json', json_encode(['owner' => $owner, 'new-guest' => $newGuest], JSON_THROW_ON_ERROR));
        $this->assertSame(200, $client->withToken($guest)->get($this->url.'/api/results')->status());
        $this->assertSame(401, $client->withToken($newGuest)->get($this->url.'/api/results')->status());
        $this->server->stop();
        $this->startServer();
        $ownerResponse = $client->withToken($owner)->get($this->url.'/api/results');
        $this->assertSame(200, $ownerResponse->status());
        $this->assertSame($first->json('results'), $ownerResponse->json('results'));
        $this->assertSame($first->json('lotto_fetched_at'), $ownerResponse->json('lotto_fetched_at'));
        $this->assertSame(200, $client->withToken($newGuest)->get($this->url.'/api/results')->status());
        $revoked = $client->withToken($guest)->get($this->url.'/api/results');
        $this->assertSame(401, $revoked->status());
        $this->assertSame('access_denied', $revoked->json('status'));
        $this->assertStringNotContainsString($guest, $revoked->body());
        Http::assertSentCount(1);
    }

    /** @return array<string, array{string|null}> */
    public static function invalidUserFiles(): array
    {
        return [
            'missing' => [null],
            'malformed' => ['{'],
            'empty' => ['{}'],
            'short token' => ['{"owner":"short"}'],
        ];
    }

    #[DataProvider('invalidUserFiles')]
    public function test_invalid_user_files_deny_access_without_exposing_configuration(?string $contents): void
    {
        if ($contents !== null) {
            file_put_contents($this->directory.'/users.json', $contents);
        }
        $this->startServer();
        $response = (new Factory)->withToken(str_repeat('a', 32))->get($this->url.'/api/results');
        $this->assertSame(503, $response->status());
        $this->assertSame('backend_unavailable', $response->json('status'));
        $this->assertStringNotContainsString('short', $response->body());
        $this->assertStringNotContainsString($this->directory, $response->body());
    }

    private function startServer(): void
    {
        $socket = stream_socket_server('tcp://127.0.0.1:0');
        if ($socket === false) {
            throw new RuntimeException('Cannot allocate a test HTTP port');
        }
        $address = stream_socket_get_name($socket, false);
        fclose($socket);
        $this->url = 'http://'.$address;
        $this->server = new Process([
            'sh', base_path('../docker/entrypoint.sh'), PHP_BINARY,
            '-S', $address, '-t', public_path(), public_path('index.php'),
        ], base_path(), [
            'APP_ENV' => 'production',
            'APP_DEBUG' => 'false',
            'APP_KEY' => 'base64:'.base64_encode(str_repeat('k', 32)),
            'APP_CONFIG_CACHE' => $this->directory.'/config.php',
            'API_USERS_FILE' => $this->directory.'/users.json',
            'API_USERS_JSON' => false,
            'LOTTO_API_KEY' => '',
            'REDIS_HOST' => config('database.redis.cache.host'),
            'REDIS_PORT' => (string) config('database.redis.cache.port'),
            'REDIS_CACHE_DB' => (string) config('database.redis.cache.database'),
            'REDIS_PREFIX' => config('database.redis.options.prefix'),
        ], timeout: null);
        $this->server->start();
        $deadline = microtime(true) + 5;
        do {
            try {
                if ((new Factory)->timeout(1)->get($this->url.'/up')->successful()) {
                    return;
                }
            } catch (ConnectionException) {
            }
            usleep(50000);
        } while ($this->server->isRunning() && microtime(true) < $deadline);

        throw new RuntimeException('Test HTTP server did not become ready');
    }
}
