<?php

namespace Tests\Feature;

use Illuminate\Support\Facades\Cache;
use Illuminate\Support\Facades\Http;
use Tests\TestCase;

class DeviceAccessTest extends TestCase
{
    public function test_two_users_share_results_and_revoked_or_invalid_tokens_are_denied(): void
    {
        Cache::store('redis')->flush();
        config(['lotto.key' => 'fake-key', 'lotto.users' => ['owner' => str_repeat('a', 32), 'guest' => str_repeat('b', 32)]]);
        Http::preventStrayRequests();
        $rows = json_decode(file_get_contents(base_path('../tests/fixtures/lotto-upstream.json')), true);
        Http::fake(['developers.lotto.pl/*' => Http::response($rows)]);
        $this->artisan('lotto:refresh')->assertExitCode(0);
        $this->withToken(str_repeat('a', 32))->getJson('/api/results')->assertOk();
        $this->withToken(str_repeat('b', 32))->getJson('/api/results')->assertOk();
        $this->withToken('invalid')->getJson('/api/results')->assertUnauthorized()->assertJsonPath('status', 'access_denied');
        config(['lotto.users' => ['owner' => str_repeat('a', 32)]]);
        $this->withToken(str_repeat('b', 32))->getJson('/api/results')->assertUnauthorized();
        Http::assertSentCount(1);
    }

    public function test_misconfigured_access_list_fails_closed_without_exposing_tokens(): void
    {
        config(['lotto.users' => ['owner' => str_repeat('a', 32), 'broken' => 'short']]);
        $response = $this->withToken(str_repeat('a', 32))->getJson('/api/results')
            ->assertStatus(503)->assertJsonPath('status', 'backend_unavailable');
        $this->assertStringNotContainsString('short', $response->getContent());
        config(['lotto.users' => null]);
        $this->getJson('/api/results')->assertStatus(503);
    }
}
