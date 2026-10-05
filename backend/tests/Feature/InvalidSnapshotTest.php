<?php

namespace Tests\Feature;

use Illuminate\Support\Facades\Cache;
use Illuminate\Support\Facades\Http;
use PHPUnit\Framework\Attributes\DataProvider;
use Tests\TestCase;

class InvalidSnapshotTest extends TestCase
{
    public static function invalidCases(): array
    {
        return [['missing-game'], ['empty-values'], ['wrong-type'], ['invalid-date'], ['unknown-top-level']];
    }

    #[DataProvider('invalidCases')]
    public function test_invalid_refresh_cannot_publish_partial_results(string $case): void
    {
        Cache::store('redis')->flush();
        config(['lotto.key' => 'fake-key']);
        Http::preventStrayRequests();
        $rows = json_decode(file_get_contents(base_path('../tests/fixtures/lotto-upstream.json')), true);
        $bad = $rows;
        if ($case === 'missing-game') {
            array_pop($bad);
        } elseif ($case === 'empty-values') {
            $bad[0]['results'][0]['resultsJson'] = [];
        } elseif ($case === 'wrong-type') {
            $bad[0]['results'][0]['resultsJson'] = ['6'];
        } elseif ($case === 'invalid-date') {
            $bad[0]['drawDate'] = 'invalid';
            $bad[0]['results'][0]['drawDate'] = 'invalid';
        } else {
            $bad = ['items' => $rows];
        }
        Http::fake(['developers.lotto.pl/*' => Http::sequence()->push($rows)->push($bad)->push($rows)]);
        $this->artisan('lotto:refresh')->assertExitCode(0);
        $this->travel(4)->minutes();
        $this->artisan('lotto:refresh')->assertExitCode(1);
        $this->withToken(str_repeat('a', 32))->getJson('/api/results')->assertStatus(503)
            ->assertJsonPath('status', 'lotto_refresh_failed')->assertJsonPath('results', []);
        $this->travel(4)->minutes();
        $this->artisan('lotto:refresh')->assertExitCode(0);
        $this->withToken(str_repeat('a', 32))->getJson('/api/results')->assertOk()->assertJsonCount(8, 'results');
    }
}
