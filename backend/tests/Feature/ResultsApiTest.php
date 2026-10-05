<?php

namespace Tests\Feature;

use Illuminate\Support\Facades\Cache;
use Illuminate\Support\Facades\Http;
use Tests\TestCase;

class ResultsApiTest extends TestCase
{
    protected function setUp(): void
    {
        parent::setUp();
        config(['lotto.games' => ['Lotto' => 'Lotto'], 'lotto.key' => 'fake-official-key']);
        Cache::store('redis')->flush();
        Http::preventStrayRequests();
    }

    public function test_authorized_display_reads_refreshed_results_without_calling_lotto(): void
    {
        Http::fake(['developers.lotto.pl/*' => Http::response([
            ['gameType' => 'Lotto', 'drawDate' => '2026-10-05T14:00:00+02:00', 'results' => [
                ['gameType' => 'Lotto', 'resultsJson' => [1, 4, 12, 24, 36, 41], 'specialResults' => []],
            ]],
        ])]);

        $this->artisan('lotto:refresh')->assertExitCode(0);
        $this->withToken(str_repeat('a', 32))->getJson('/api/results')
            ->assertOk()->assertJsonPath('status', 'ready')
            ->assertJsonPath('results.0.label', 'Lotto')
            ->assertJsonPath('results.0.groups.0.value', [1, 4, 12, 24, 36, 41]);
        $this->withToken(str_repeat('a', 32))->getJson('/api/results')->assertOk();
        Http::assertSentCount(1);
    }
}
