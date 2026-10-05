<?php

namespace Tests\Feature;

use Carbon\CarbonImmutable;
use Illuminate\Support\Facades\Cache;
use Illuminate\Support\Facades\Http;
use Tests\TestCase;

class CompleteSnapshotTest extends TestCase
{
    protected function setUp(): void
    {
        parent::setUp();
        config(['lotto.key' => 'fake-official-key']);
        Cache::store('redis')->flush();
        Http::preventStrayRequests();
    }

    public function test_all_games_and_additional_numbers_replace_the_previous_snapshot(): void
    {
        $this->travelTo(CarbonImmutable::parse('2026-10-05T14:00:00+02:00'));
        $rows = json_decode(file_get_contents(base_path('../tests/fixtures/lotto-upstream.json')), true);
        Http::fake(['developers.lotto.pl/*' => Http::sequence()->push($rows)->push(array_map(function ($row) {
            $row['results'][0]['resultsJson'] = [7, 8, 9];

            return $row;
        }, $rows))]);
        $this->artisan('lotto:refresh')->assertExitCode(0);
        $response = $this->withToken(str_repeat('a', 32))->getJson('/api/results')->assertOk()
            ->assertJsonCount(8, 'results')
            ->assertJsonPath('results.6.groups.1.kind', 'additional')
            ->assertJsonPath('results.6.groups.1.value', [3, 9])
            ->assertJsonPath('results.0.groups.0.label', '05.10.2026 14:00');
        $expected = json_decode(file_get_contents(base_path('../tests/fixtures/results.json')), true);
        $response->assertExactJson($expected);
        $this->travel(4)->minutes();
        $this->artisan('lotto:refresh')->assertExitCode(0);
        $this->withToken(str_repeat('a', 32))->getJson('/api/results')->assertJsonCount(8, 'results')
            ->assertJsonPath('results.0.groups.0.value', [7, 8, 9]);
        Http::assertSentCount(2);
    }
}
