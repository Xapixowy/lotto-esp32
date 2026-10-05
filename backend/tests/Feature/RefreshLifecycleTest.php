<?php

namespace Tests\Feature;

use Illuminate\Support\Facades\Cache;
use Illuminate\Support\Facades\Http;
use Tests\TestCase;

class RefreshLifecycleTest extends TestCase
{
    protected function setUp(): void
    {
        parent::setUp();
        config(['lotto.key' => 'fake-official-key']);
        Cache::store('redis')->flush();
        Http::preventStrayRequests();
    }

    public function test_refreshes_are_due_every_four_minutes_and_polls_do_not_fetch(): void
    {
        $rows = json_decode(file_get_contents(base_path('../tests/fixtures/lotto-upstream.json')), true);
        Http::fake(['developers.lotto.pl/*' => Http::response($rows)]);
        $this->artisan('lotto:refresh')->assertExitCode(0);
        $this->artisan('lotto:refresh')->assertExitCode(0);
        $this->travel(239)->seconds();
        $this->artisan('lotto:refresh')->assertExitCode(0);
        $this->withToken(str_repeat('a', 32))->getJson('/api/results')->assertOk();
        Http::assertSentCount(1);
        $this->travel(1)->seconds();
        $this->artisan('lotto:refresh')->assertExitCode(0);
        Http::assertSentCount(2);
    }

    public function test_failed_refresh_hides_all_results_and_recovers_at_next_interval(): void
    {
        $rows = json_decode(file_get_contents(base_path('../tests/fixtures/lotto-upstream.json')), true);
        Http::fake(['developers.lotto.pl/*' => Http::sequence()->push($rows)->push([], 502)->push($rows)]);
        $this->artisan('lotto:refresh')->assertExitCode(0);
        $initial = $this->withToken(str_repeat('a', 32))->getJson('/api/results')->json('lotto_fetched_at');
        $this->travel(4)->minutes();
        $this->artisan('lotto:refresh')->assertExitCode(1);
        $this->withToken(str_repeat('a', 32))->getJson('/api/results')->assertStatus(503)
            ->assertJsonPath('status', 'lotto_refresh_failed')->assertJsonPath('results', [])
            ->assertJsonPath('lotto_fetched_at', $initial);
        $this->artisan('lotto:refresh')->assertExitCode(0);
        Http::assertSentCount(2);
        $this->travel(4)->minutes();
        $this->artisan('lotto:refresh')->assertExitCode(0);
        $this->withToken(str_repeat('a', 32))->getJson('/api/results')->assertOk()->assertJsonCount(8, 'results');
    }

    public function test_empty_and_stale_cache_are_explicit_status_screens(): void
    {
        $this->withToken(str_repeat('a', 32))->getJson('/api/results')->assertStatus(503)->assertJsonPath('status', 'fetching');
        $rows = json_decode(file_get_contents(base_path('../tests/fixtures/lotto-upstream.json')), true);
        Http::fake(['developers.lotto.pl/*' => Http::response($rows)]);
        $this->artisan('lotto:refresh')->assertExitCode(0);
        $this->travel(480)->seconds();
        $this->withToken(str_repeat('a', 32))->getJson('/api/results')->assertOk();
        $this->travel(1)->seconds();
        $this->withToken(str_repeat('a', 32))->getJson('/api/results')->assertStatus(503)->assertJsonPath('status', 'stale');
        Http::assertSentCount(1);
    }

    public function test_another_worker_holding_refresh_lock_prevents_upstream_calls(): void
    {
        $lock = Cache::store('redis')->getStore()->lock('lotto:refresh-lock', 45);
        $this->assertTrue($lock->get());
        try {
            $this->artisan('lotto:refresh')->assertExitCode(0);
            Http::assertNothingSent();
        } finally {
            $lock->release();
        }
    }
}
