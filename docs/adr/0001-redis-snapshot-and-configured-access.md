# Redis snapshots and configured API access

Laravel serves lottery results from an ephemeral Redis snapshot, without a database. A startup fetch and subsequent four-minute scheduled fetches call the official all-games latest-results endpoint; each complete, validated response replaces the previous snapshot atomically. Device requests only read cached results, so adding displays does not increase calls to Lotto. No draw history is accumulated and Redis persistence is not required.

Read access uses named bearer tokens loaded from backend configuration at startup, with identical permissions and no registration or account-management UI. Configuration changes require restarting the backend. This deliberately avoids database-backed accounts for a personal deployment that can grant access to additional users.
