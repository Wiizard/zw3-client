# Find Match playlist presentation

The existing hover/focus details card reads `zwnet_catalog_selected_population`
and `zwnet_catalog_selected_image`. Both mouse hover and keyboard/controller
focus use `ZWNET_HighlightPlaylist`; no additional menu or row label is needed.

The authenticated playlist catalog and periodic status response supply
`player_count`. This counts unique players with unexpired online presence who
are searching, voting, waiting for server startup, connecting or playing that
playlist. Queue/match handover is deduplicated. Idle party members, departed
players, ended matches and stale running instances are excluded. Missing/stale
metrics show the existing unavailable label instead of an invented zero.

The backend playlist editor accepts an optional PNG/JPEG image. It fits the
image into a 16:9 card, uploads a bounded PNG through the existing manager
authentication, and stores the resulting `image_url` in the playlist revision.
Saving applies it to the catalog; clearing the image restores the map preview.

The client only accepts content-addressed image paths on its configured ZWNET
backend. It downloads the focused playlist image asynchronously, verifies the
SHA-256 and dimensions, and creates a material using the existing image loader
on the main pipeline. Until the image is available, the map preview remains.
Duplicate downloads are suppressed, failures are throttled, obsolete menu
sessions cannot publish an image, and at most eight images are cached.

Backend contract:

- `PUT /api/admin/playlists/images`: authenticated developer/admin PNG upload.
  Returns `image_url`, `sha256`, `width` and `height`.
- `GET /zwnet/playlist-images/<sha256>.png`: immutable image bytes.
- Playlist revision/catalog: optional `image_url`; catalog also includes the
  authoritative `player_count`.
- `/api/status`: public playlist summaries include `player_count`. Restricted
  playlists remain confined to the authenticated catalog.

No game files are needed in this Git repository. Menu contracts can be checked
against a private installation using `ZW3_GAME_ROOT`; native image validation
and production loader tests run through `scripts/test-playlist-images.ps1`.
