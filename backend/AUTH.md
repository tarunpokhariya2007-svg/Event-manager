# ChronoPlan authentication

## What it does
- Email/password accounts (Argon2id via libsodium) and Google sign-in (ID token verified on the server: RS256 signature against Google's published keys, issuer, audience, expiry).
- Sessions: 256-bit random token in an `HttpOnly` cookie (`chronoplan_session`); only its SHA-256 is stored in SQLite. Absolute lifetime 14 days, idle timeout 7 days, deleted on logout.
- CSRF: every non-GET request to a protected route needs `X-CSRF-Token` (derived from the session token; returned by login/register/`GET /auth/me`) **and** an `Origin` equal to `CHRONOPLAN_CORS_ORIGIN`. Public sign-in POSTs must be `application/json`.
- CORS: only the configured frontend origin, with credentials.
- Data isolation: every repository is constructed for one user and every SQL statement filters on `user_id`. `task_dependencies` has composite foreign keys, so an edge between two users' tasks is impossible at the database level.
- Brute-force protection: 10 failures / 15 min per e-mail and per client address (in memory, resets on restart); registrations are capped per client too.

## API
| Method | Path | Notes |
|---|---|---|
| POST | `/api/v1/auth/register` | `{email, password, display_name?}` -> 201 `{user, csrf_token}` + cookie; 409 `email_taken` |
| POST | `/api/v1/auth/login` | `{email, password}` -> 200; 401 `invalid_credentials`; 429 `too_many_attempts` |
| POST | `/api/v1/auth/google` | `{credential}` -> 200/201; 401 `invalid_google_token`; 409 `account_exists`; 503 `google_not_configured` / `google_unavailable` |
| GET | `/api/v1/auth/me` | 200 `{user, csrf_token}` or 401 |
| POST | `/api/v1/auth/logout` | needs CSRF; 204 and the cookie is cleared |

All other `/api/v1/*` routes except `GET /api/v1/schedule/optimize/health` return 401 without a valid session.
A Google sign-in whose e-mail already belongs to a password account is refused (409) rather than merged, because that e-mail was never verified (account pre-hijacking). There is no password reset: it needs e-mail delivery, which this project does not have.

## Database migration
`db/migrations/002_auth.sql` runs automatically at startup inside one transaction (`PRAGMA user_version` 1 -> 2). It creates `users` and `sessions` and rebuilds `tasks`, `events`, `task_dependencies` with a `user_id` (SQLite cannot change a primary key in place). Ids are now unique **per user**. **Back up the file first** (`cp data/chronoplan.db data/chronoplan.db.bak`; with WAL also copy `-wal`/`-shm`, or run `sqlite3 data/chronoplan.db "PRAGMA wal_checkpoint(TRUNCATE)"`).

Rows that existed before authentication are kept and owned by a locked system user (`legacy-data@chronoplan.invalid`, cannot sign in). No account sees them until you move them, after registering your own account:
```sql
UPDATE tasks             SET user_id = (SELECT id FROM users WHERE email = 'you@example.com') WHERE user_id = (SELECT id FROM users WHERE is_system = 1);
UPDATE events            SET user_id = (SELECT id FROM users WHERE email = 'you@example.com') WHERE user_id = (SELECT id FROM users WHERE is_system = 1);
UPDATE task_dependencies SET user_id = (SELECT id FROM users WHERE email = 'you@example.com') WHERE user_id = (SELECT id FROM users WHERE is_system = 1);
```
(Run all three in one `BEGIN; ... COMMIT;`; the composite foreign keys need the three updates together, so use `PRAGMA defer_foreign_keys = ON;` first.)

## Environment variables
Backend (`backend/.env.example` has the full list): `CHRONOPLAN_ENV`, `CHRONOPLAN_CORS_ORIGIN` (exact origin, never `*`), `GOOGLE_CLIENT_ID`, `CHRONOPLAN_COOKIE_SECURE`, `CHRONOPLAN_COOKIE_SAMESITE`, `CHRONOPLAN_SESSION_TTL_SECONDS`, `CHRONOPLAN_SESSION_IDLE_SECONDS`, `CHRONOPLAN_TRUST_X_FORWARDED_FOR`.
Frontend: `VITE_API_BASE_URL`, `VITE_GOOGLE_CLIENT_ID`.
Production (`CHRONOPLAN_ENV=production`) refuses to start unless the origin is `https://...` and cookies are Secure.

## Google Cloud setup
1. https://console.cloud.google.com -> create/select a project -> **APIs & Services -> OAuth consent screen**: choose External, fill app name and support e-mail, add scope `openid email profile`; add yourself as a test user while in Testing.
2. **Credentials -> Create credentials -> OAuth client ID -> Web application**.
3. **Authorized JavaScript origins**: `http://localhost:5173` (and your production origin, e.g. `https://app.example.com`). No redirect URI is needed for the ID-token button flow.
4. Copy the **Client ID** (`xxxx.apps.googleusercontent.com`) into backend `GOOGLE_CLIENT_ID` and frontend `VITE_GOOGLE_CLIENT_ID`. A client secret is not used and must not be added anywhere.

## Build and run
```
cd backend
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake   # installs libsodium, openssl, curl too
cmake --build build -j && ctest --test-dir build --output-on-failure
CHRONOPLAN_CORS_ORIGIN=http://localhost:5173 GOOGLE_CLIENT_ID=... ./build/chronoplan

cd frontend && npm install && npm run build && npm run dev
```
The frontend dev server and API are different origins but the same *site* (`localhost`), so `SameSite=Lax` cookies work; for different registrable domains use `SameSite=None` + HTTPS.
