# Backend-owned display content

The backend defines ordered slides using stable game IDs, labels, and labeled result groups with value arrays and kinds such as simple and additional. Firmware renders these groups generically and paginates content that exceeds the screen. Each replacement snapshot is authoritative; firmware does not append results or infer history. This keeps game-specific content changes in the backend while retaining a bounded rendering contract on the device.
