<?php

namespace App\Lotto;

use Carbon\CarbonImmutable;
use InvalidArgumentException;

class SlideBuilder
{
    private const int MAX_GROUP_VALUES = 20;

    /**
     * @param  list<mixed>  $rows
     * @param  array<string, string>  $games
     * @return list<array{id: string, label: string, groups: list<array{label: string, numbers: list<array{value: int, type: string}>}>}>
     */
    public function build(array $rows, array $games): array
    {
        $slides = [];
        foreach ($games as $id => $label) {
            $row = collect($rows)->firstWhere('gameType', $id);
            if (! is_array($row) || ! isset($row['drawDate'], $row['results']) || ! is_array($row['results']) || $row['results'] === []) {
                throw new InvalidArgumentException('Missing required game results');
            }
            $groups = [];
            foreach ($row['results'] as $result) {
                if (! is_array($result)) {
                    throw new InvalidArgumentException('Invalid draw result');
                }
                if (($result['gameType'] ?? $id) !== $id) {
                    continue;
                }
                $drawLabel = $this->drawLabel($result['drawDate'] ?? $row['drawDate']);
                $values = $this->winningValues($result['resultsJson'] ?? null);
                $additional = $this->winningValues($result['specialResults'] ?? [], allowEmpty: true);
                $numbers = $this->numbers($id, $values, $additional);
                $groups[] = [
                    'label' => $drawLabel,
                    'numbers' => $numbers,
                ];
            }
            if ($groups === [] || count($groups) > 16) {
                throw new InvalidArgumentException('Invalid result groups');
            }
            $slides[] = ['id' => $id, 'label' => $label, 'groups' => $groups];
        }
        if (strlen(json_encode($slides, JSON_THROW_ON_ERROR)) > 24000) {
            throw new InvalidArgumentException('Results exceed the device payload budget');
        }

        return $slides;
    }

    private function drawLabel(mixed $date): string
    {
        if (! is_string($date) || ! preg_match('/^(\d{4})-(\d{2})-(\d{2})T([01]\d|2[0-3]):([0-5]\d):([0-5]\d)(?:\.\d{1,7})?(?:Z|[+-](?:[01]\d|2[0-3]):[0-5]\d)?$/', $date, $parts)
            || ! checkdate((int) $parts[2], (int) $parts[3], (int) $parts[1])) {
            throw new InvalidArgumentException('Invalid draw date');
        }

        return CarbonImmutable::parse($date, 'Europe/Warsaw')->timezone('Europe/Warsaw')->format('d.m.Y H:i');
    }

    /** @return list<int> */
    private function winningValues(mixed $values, bool $allowEmpty = false): array
    {
        if (! is_array($values) || ! array_is_list($values)
            || (! $allowEmpty && $values === []) || count($values) > self::MAX_GROUP_VALUES
            || array_any($values, fn ($value) => ! is_int($value) || $value < 0 || $value > 999)) {
            throw new InvalidArgumentException('Invalid winning values');
        }

        return $values;
    }

    /**
     * @param  list<int>  $values
     * @param  list<int>  $special
     * @return list<array{value: int, type: string}>
     */
    private function numbers(string $game, array $values, array $special): array
    {
        if ($game === 'MultiMulti' && array_diff($special, $values) !== []) {
            throw new InvalidArgumentException('Multi Multi Plus must be a drawn number');
        }
        $numbers = array_map(fn (int $value): array => [
            'value' => $value,
            'type' => $game === 'MultiMulti' && in_array($value, $special, true) ? 'special' : 'simple',
        ], $values);
        if ($game !== 'MultiMulti') {
            foreach ($special as $value) {
                $numbers[] = ['value' => $value, 'type' => 'special'];
            }
        }
        if (count($numbers) > self::MAX_GROUP_VALUES) {
            throw new InvalidArgumentException('Invalid combined winning values');
        }
        usort($numbers, fn (array $a, array $b): int => ($a['value'] <=> $b['value']) ?: strcmp($a['type'], $b['type']));

        return $numbers;
    }
}
