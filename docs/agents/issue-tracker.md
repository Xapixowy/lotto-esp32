# Issue tracker: GitHub

Issues and specs live in GitHub Issues for Xapixowy/lotto-esp32.
Use the gh CLI from this repository.

## Operations

- Create: gh issue create --title "..." --body-file <file>
- Read: gh issue view <number> --comments
- List: gh issue list --state open --json number,title,body,labels
- Comment: gh issue comment <number> --body-file <file>
- Label: gh issue edit <number> --add-label <label>
- Remove label: gh issue edit <number> --remove-label <label>
- Close: gh issue close <number>

Use UTF-8 files containing actual newlines for multiline bodies.

When a skill says to publish to the issue tracker, create a
GitHub issue. When it says to fetch a ticket, read the issue
and its comments.

## Dependencies

Use native GitHub blocking dependencies where available.
Otherwise, record "Blocked by: #<number>" in the issue body.
Work on tickets only after all blockers are closed.

## Wayfinding

Keep the decision map in an issue labeled wayfinder:map.
Link child tickets as sub-issues, or use a task list in the
map and a "Part of #<map>" reference in each child.

Use wayfinder:research, wayfinder:prototype,
wayfinder:grilling, or wayfinder:task for child tickets.
Claim a ready ticket by assigning it to the driving developer.
Resolve it with a decision comment, close it, and add a
decision link to the map.

## Pull requests as a triage surface

PRs as a request surface: no.
