#include "auth/AuthService.hpp"
#include "auth/AuthErrors.hpp"
#include "auth/Crypto.hpp"
#include "auth/PasswordHasher.hpp"
#include "auth/Validation.hpp"
#include "models/Validation.hpp"

namespace chronoplan::auth {

namespace {
constexpr long long kTouchIntervalSeconds = 300;   // refresh last_seen at most every 5 minutes
}

AuthService::AuthService(database::Database& db, AuthConfig config, std::shared_ptr<IGoogleTokenVerifier> google, Clock clock)
    : db_(db),
      config_(std::move(config)),
      google_(std::move(google)),
      clock_(std::move(clock)),
      users_(db),
      sessions_(db),
      failedLogins_(config_.maxFailedLogins, config_.failedLoginWindowSeconds, clock_),
      registrations_(config_.maxFailedLogins, config_.failedLoginWindowSeconds, clock_) {
    initCrypto();
    sessions_.deleteStale(clock_(), config_.sessionIdleSeconds);
}

std::string AuthService::deriveCsrfToken(const std::string& sessionToken) {
    return sha256Hex("chronoplan-csrf-v1:" + sessionToken);
}

bool AuthService::csrfMatches(const AuthenticatedUser& user, const std::string& headerValue) {
    return !headerValue.empty() && constantTimeEquals(user.csrfToken, headerValue);
}

AuthenticatedUser AuthService::toAuthenticated(const User& u, const std::string& sessionToken) {
    AuthenticatedUser a;
    a.id = u.id;
    a.email = u.email;
    a.displayName = u.displayName;
    a.hasPassword = u.passwordHash.has_value();
    a.hasGoogle = u.googleSub.has_value();
    a.csrfToken = deriveCsrfToken(sessionToken);
    return a;
}

SignInResult AuthService::issueSession(const User& user, bool created) {
    const long long now = clock_();
    SignInResult r;
    r.sessionToken = randomToken(32);                       // 256 bits from the OS CSPRNG
    sessions_.create(user.id, sha256Hex(r.sessionToken), now, now + config_.sessionTtlSeconds);
    users_.recordLogin(user.id, now);
    r.user = toAuthenticated(user, r.sessionToken);
    r.maxAgeSeconds = config_.sessionTtlSeconds;
    r.created = created;
    return r;
}

void AuthService::requireNotThrottled(const std::string& emailKey, const std::string& clientKey) {
    if (!failedLogins_.allowed("e:" + emailKey) || !failedLogins_.allowed("ip:" + clientKey))
        throw TooManyAttemptsError("too many failed attempts, try again later");
}

SignInResult AuthService::registerUser(const std::string& rawEmail, const std::string& password,
                                       const std::string& displayName, const std::string& clientKey) {
    const std::string email = normalizeAndValidateEmail(rawEmail);
    validatePassword(password, email);

    // Each registration costs a 64 MiB Argon2 run, so cap attempts per client.
    if (!registrations_.allowed("reg:" + clientKey)) throw TooManyAttemptsError("too many attempts, try again later");
    registrations_.recordFailure("reg:" + clientKey);   // counts every attempt, not only failures

    if (users_.findByEmail(email)) throw EmailTakenError("an account with this e-mail address already exists");
    const std::string hash = PasswordHasher::hash(password);
    const User user = users_.createPasswordUser(email, hash, sanitizeDisplayName(displayName), clock_());   // UNIQUE index settles races
    return issueSession(user, /*created=*/true);
}

SignInResult AuthService::login(const std::string& rawEmail, const std::string& password, const std::string& clientKey) {
    const std::string email = normalizeAndValidateEmail(rawEmail);
    if (password.size() > kMaxPasswordBytes) throw InvalidCredentialsError("incorrect e-mail or password");
    requireNotThrottled(email, clientKey);

    const std::optional<User> user = users_.findByEmail(email);
    const bool usable = user && !user->isSystem && user->passwordHash.has_value();
    // Always run one Argon2 verification so response time does not reveal whether the account exists.
    const bool ok = PasswordHasher::verify(usable ? *user->passwordHash : PasswordHasher::dummyHash(), password) && usable;
    if (!ok) {
        failedLogins_.recordFailure("e:" + email);
        failedLogins_.recordFailure("ip:" + clientKey);
        throw InvalidCredentialsError("incorrect e-mail or password");
    }
    failedLogins_.reset("e:" + email);
    if (PasswordHasher::needsRehash(*user->passwordHash)) users_.updatePasswordHash(user->id, PasswordHasher::hash(password));
    return issueSession(*user, /*created=*/false);
}

SignInResult AuthService::loginWithGoogle(const std::string& idToken, const std::string& clientKey) {
    if (!googleEnabled()) throw GoogleNotConfiguredError("Google sign-in is not configured on this server");
    if (!failedLogins_.allowed("ip:" + clientKey)) throw TooManyAttemptsError("too many failed attempts, try again later");

    GoogleClaims claims;
    try {
        claims = google_->verify(idToken);
    } catch (const InvalidGoogleTokenError&) {
        failedLogins_.recordFailure("ip:" + clientKey);
        throw;
    }
    if (!claims.emailVerified) {
        failedLogins_.recordFailure("ip:" + clientKey);
        throw InvalidGoogleTokenError("the Google account's e-mail address is not verified");
    }

    if (auto existing = users_.findByGoogleSub(claims.sub); existing && !existing->isSystem)
        return issueSession(*existing, /*created=*/false);

    // Same e-mail already registered another way: do NOT merge. The existing account's e-mail was
    // never verified, so linking would let whoever pre-registered it keep access (account pre-hijacking).
    if (users_.findByEmail(claims.email))
        throw AccountExistsError("an account with this e-mail address already exists, sign in with your password instead");

    const User created = users_.createGoogleUser(claims.email, claims.sub, sanitizeDisplayName(claims.name), clock_());
    return issueSession(created, /*created=*/true);
}

std::optional<AuthenticatedUser> AuthService::authenticate(const std::string& sessionToken) {
    if (sessionToken.empty() || sessionToken.size() > 256) return std::nullopt;
    const std::string hash = sha256Hex(sessionToken);
    const auto session = sessions_.findByTokenHash(hash);
    if (!session) return std::nullopt;

    const long long now = clock_();
    if (now >= session->expiresAt || now - session->lastSeenAt >= config_.sessionIdleSeconds) {
        sessions_.deleteByTokenHash(hash);
        return std::nullopt;
    }
    const auto user = users_.findById(session->userId);
    if (!user || user->isSystem) return std::nullopt;
    if (now - session->lastSeenAt >= kTouchIntervalSeconds) sessions_.touch(session->id, now);
    return toAuthenticated(*user, sessionToken);
}

void AuthService::logout(const std::string& sessionToken) {
    if (!sessionToken.empty()) sessions_.deleteByTokenHash(sha256Hex(sessionToken));
}

}  // namespace chronoplan::auth
