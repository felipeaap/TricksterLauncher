#pragma once

#include <string>

namespace AuthClient
{
    /// Result codes returned by the launcher_auth.php endpoint.
    enum class AuthResult
    {
        Success,             ///< Login successful — open the game.
        InvalidCredentials,  ///< 401: wrong username or password.
        Banned,              ///< 403: account is banned.
        AlreadyOnline,       ///< 409: player session already active.
        RateLimit,           ///< 429: too many attempts.
        ServiceUnavailable,  ///< 5xx / network error.
        NotConfigured,       ///< AuthEndpointURL or AuthToken not set in config.
    };

    /// Full response from the auth endpoint.
    struct AuthResponse
    {
        AuthResult  result  = AuthResult::ServiceUnavailable;
        std::string message;         ///< Human-readable message for the UI.
        std::string releaseDate;     ///< Only set when result == Banned.
        std::string reason;          ///< Only set when result == Banned.
    };

    /// POST to config::AuthEndpointURL following the launcher_auth.php protocol:
    ///   auth_token=<config::AuthToken>&username=<username>&password=<password>
    ///
    /// Blocking - call from a worker thread.
    AuthResponse Authenticate(const std::string& username, const std::string& password);

} // namespace AuthClient
