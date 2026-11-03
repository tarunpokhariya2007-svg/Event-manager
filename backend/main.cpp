// Entry point only: open the database, build the app, register routes, run.
#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include "api/Routes.hpp"
#include "auth/AuthConfig.hpp"
#include "auth/AuthService.hpp"
#include "auth/Crypto.hpp"
#include "auth/GoogleTokenVerifier.hpp"
#include "api/ServerConfig.hpp"
#include "database/DatabaseManager.hpp"
#include "repositories/Repositories.hpp"

int main() {
    try {
        // 1-5: configuration, open/create, foreign keys, schema/migrations, readiness check.
        // A failure aborts startup: we never serve requests without a usable database.
        const auto config = chronoplan::database::DatabaseConfig::fromEnvironment();
        chronoplan::database::DatabaseManager db(config);
        std::cout << "Database ready: " << config.path << " (schema v" << db.schemaVersion() << ")\n";

        // Authentication settings are validated up front: a production deployment with a wildcard
        // CORS origin or insecure cookies refuses to start instead of running unsafely.
        chronoplan::auth::initCrypto();
        const auto authConfig = chronoplan::auth::AuthConfig::fromEnvironment();
        const auto now = [] {
            return static_cast<long long>(std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        };
        std::shared_ptr<chronoplan::auth::IGoogleTokenVerifier> google;
        if (!authConfig.googleClientId.empty())
            google = std::make_shared<chronoplan::auth::GoogleTokenVerifier>(
                authConfig.googleClientId, std::make_shared<chronoplan::auth::HttpJwksProvider>(now), now);
        else
            std::cerr << "NOTE: GOOGLE_CLIENT_ID is not set; Google sign-in is disabled.\n";
        chronoplan::auth::AuthService authService(db.database(), authConfig, google, now);

        // 6-7: routes, then the HTTP server.
        const auto server = chronoplan::api::ServerConfig::fromEnvironment();
        chronoplan::api::ChronoApp app;
        chronoplan::api::configureCors(app, authConfig.frontendOrigin);
        chronoplan::api::registerApiRoutes(app, db.database(), authService);

        std::cout << "Listening on " << server.bindAddress << ':' << server.port
                  << " (frontend origin: " << authConfig.frontendOrigin << ", "
                  << (authConfig.production ? "production" : "development") << ")\n";

        app.bindaddr(server.bindAddress).port(server.port).multithreaded().run();
        return 0;
    } catch (const chronoplan::database::DatabaseError& ex) {
        std::cerr << "Fatal: database initialization failed: " << ex.what() << '\n';
        return 2;
    } catch (const std::invalid_argument& ex) {
        std::cerr << "Fatal: invalid configuration: " << ex.what() << '\n';
        return 3;
    } catch (const std::exception& ex) {
        std::cerr << "Fatal: " << ex.what() << '\n';
        return 1;
    }
}
