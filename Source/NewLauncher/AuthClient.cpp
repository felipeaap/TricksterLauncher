#include "AuthClient.h"

#include <sstream>
#include <string>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "json.hpp"

#include "Config.h"
#include "Logger.h"

namespace AuthClient
{

namespace
{

/// URL-encode a single string value for application/x-www-form-urlencoded.
std::string UrlEncode(const std::string& value)
{
    std::string encoded;
    encoded.reserve(value.size() * 3);

    for (unsigned char c : value)
    {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
        {
            encoded += static_cast<char>(c);
        }
        else
        {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", c);
            encoded += buf;
        }
    }
    return encoded;
}

/// Parse a host string like "example.com" or "example.com:8080" into host + port.
struct ParsedUrl
{
    bool  useSsl = false;
    std::string host;
    int   port   = 80;
    std::string path;
};

ParsedUrl ParseUrl(const std::string& url)
{
    ParsedUrl result;

    std::string remaining = url;

    if (remaining.substr(0, 8) == "https://")
    {
        result.useSsl = true;
        result.port   = 443;
        remaining     = remaining.substr(8);
    }
    else if (remaining.substr(0, 7) == "http://")
    {
        result.useSsl = false;
        result.port   = 80;
        remaining     = remaining.substr(7);
    }

    const auto slashPos = remaining.find('/');
    std::string hostPart = (slashPos != std::string::npos)
                               ? remaining.substr(0, slashPos)
                               : remaining;
    result.path = (slashPos != std::string::npos) ? remaining.substr(slashPos) : "/";

    const auto colonPos = hostPart.rfind(':');
    if (colonPos != std::string::npos)
    {
        result.host = hostPart.substr(0, colonPos);
        try { result.port = std::stoi(hostPart.substr(colonPos + 1)); }
        catch (...) {}
    }
    else
    {
        result.host = hostPart;
    }

    return result;
}

} // anonymous namespace

AuthResponse Authenticate(const std::string& username, const std::string& password)
{
    const std::string endpointUrl = config::GetResolvedAuthEndpointURL();
    if (endpointUrl.empty())
    {
        Logger::LogError("AuthClient: AuthEndpointURL (or LauncherCDN) not configured.");
        return { AuthResult::NotConfigured, "Authentication not configured." };
    }

    const ParsedUrl parsed = ParseUrl(endpointUrl);
    if (parsed.host.empty())
    {
        Logger::LogError("AuthClient: invalid AuthEndpointURL: " + endpointUrl);
        return { AuthResult::ServiceUnavailable, "Login service unavailable." };
    }

    // Build form-encoded body: auth_token=X&username=Y&password=Z
    const std::string body =
        "auth_token=" + UrlEncode(config::AuthToken)  +
        "&username="  + UrlEncode(username)            +
        "&password="  + UrlEncode(password);

    static constexpr int kConnectTimeout = 5;
    static constexpr int kReadTimeout    = 15;

    std::string rawResponse;
    int httpStatus = 0;

    try
    {
        if (parsed.useSsl)
        {
            httplib::SSLClient client(parsed.host, parsed.port);
            client.set_connection_timeout(kConnectTimeout, 0);
            client.set_read_timeout(kReadTimeout, 0);
            client.set_follow_location(false);

            const auto res = client.Post(
                parsed.path.c_str(),
                body,
                "application/x-www-form-urlencoded");

            if (!res)
            {
                Logger::LogError("AuthClient: SSL request failed to " + endpointUrl);
                return { AuthResult::ServiceUnavailable, "Login service unavailable." };
            }
            httpStatus  = res->status;
            rawResponse = res->body;
        }
        else
        {
            httplib::Client client(parsed.host, parsed.port);
            client.set_connection_timeout(kConnectTimeout, 0);
            client.set_read_timeout(kReadTimeout, 0);
            client.set_follow_location(false);

            const auto res = client.Post(
                parsed.path.c_str(),
                body,
                "application/x-www-form-urlencoded");

            if (!res)
            {
                Logger::LogError("AuthClient: request failed to " + endpointUrl);
                return { AuthResult::ServiceUnavailable, "Login service unavailable." };
            }
            httpStatus  = res->status;
            rawResponse = res->body;
        }
    }
    catch (const std::exception& ex)
    {
        Logger::LogError(std::string("AuthClient: exception: ") + ex.what());
        return { AuthResult::ServiceUnavailable, "Login service unavailable." };
    }

    // The PHP bridge always returns HTTP 200 — errors are encoded in the JSON body.
    // We parse regardless of httpStatus.
    nlohmann::json json;
    try
    {
        json = nlohmann::json::parse(rawResponse);
    }
    catch (...)
    {
        Logger::LogError("AuthClient: invalid JSON response from auth endpoint.");
        return { AuthResult::ServiceUnavailable, "Login service unavailable." };
    }

    const std::string status  = json.value("status",  "error");
    const std::string message = json.value("message", "Login failed.");

    if (status == "success")
    {
        return { AuthResult::Success, message };
    }

    // Map well-known messages to typed results so the presenter can react.
    if (message.find("banned") != std::string::npos ||
        message.find("Banned") != std::string::npos)
    {
        AuthResponse r;
        r.result      = AuthResult::Banned;
        r.message     = message;
        r.releaseDate = json.value("release_date", "");
        r.reason      = json.value("reason", "");
        return r;
    }

    if (message.find("already online") != std::string::npos)
        return { AuthResult::AlreadyOnline, message };

    if (message.find("Too many") != std::string::npos ||
        message.find("rate limit") != std::string::npos)
        return { AuthResult::RateLimit, message };

    if (message.find("unavailable") != std::string::npos ||
        httpStatus >= 500)
        return { AuthResult::ServiceUnavailable, message };

    return { AuthResult::InvalidCredentials, message };
}

} // namespace AuthClient
