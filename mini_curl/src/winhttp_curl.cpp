#include "../include/winhttp_curl.h"
#include <sstream>
#include <algorithm>
#include <iomanip>

// Define export when building DLL
#ifdef BUILD_DLL
#define MINI_CURL_EXPORTS
#endif

// Convert string to wide string
std::wstring WinHttpCurl::StringToWString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// Convert wide string to string
std::string WinHttpCurl::WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Parse URL into components
void WinHttpCurl::ParseUrl(const std::string& url, std::string& scheme, 
                           std::string& host, std::string& path, int& port) {
    scheme = "http";
    host = "";
    path = "/";
    port = 80;
    
    if (url.empty()) {
        return;
    }
    
    std::string urlLower = url;
    std::transform(urlLower.begin(), urlLower.end(), urlLower.begin(), ::tolower);
    
    size_t schemePos = urlLower.find("://");
    if (schemePos != std::string::npos) {
        scheme = url.substr(0, schemePos);
        std::transform(scheme.begin(), scheme.end(), scheme.begin(), ::tolower);
        urlLower = urlLower.substr(schemePos + 3);
    }
    
    size_t pathPos = urlLower.find('/');
    size_t portPos = urlLower.find(':');
    
    if (pathPos != std::string::npos) {
        path = urlLower.substr(pathPos);
        urlLower = urlLower.substr(0, pathPos);
    }
    
    if (portPos != std::string::npos && (pathPos == std::string::npos || portPos < pathPos)) {
        host = urlLower.substr(0, portPos);
        std::string portStr = urlLower.substr(portPos + 1);
        if (pathPos != std::string::npos) {
            size_t portEnd = portStr.find('/');
            if (portEnd != std::string::npos) {
                portStr = portStr.substr(0, portEnd);
            }
        }
        try {
            port = std::stoi(portStr);
            if (port <= 0 || port > 65535) {
                port = (scheme == "https") ? 443 : 80;
            }
        } catch (...) {
            port = (scheme == "https") ? 443 : 80;
        }
    } else {
        host = urlLower;
    }
    
    if (scheme == "https") {
        if (port == 80) port = 443;
    }
}

// Parse headers string into map
void WinHttpCurl::ParseHeaders(const std::string& headersStr, 
                               std::map<std::string, std::string>& headers) {
    std::istringstream stream(headersStr);
    std::string line;
    
    while (std::getline(stream, line)) {
        // Remove CR if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos && colonPos > 0) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            
            // Trim whitespace from key
            size_t keyStart = key.find_first_not_of(" \t");
            if (keyStart != std::string::npos) {
                size_t keyEnd = key.find_last_not_of(" \t");
                key = key.substr(keyStart, keyEnd - keyStart + 1);
            } else {
                key.clear();
            }
            
            // Trim whitespace from value
            size_t valueStart = value.find_first_not_of(" \t");
            if (valueStart != std::string::npos) {
                size_t valueEnd = value.find_last_not_of(" \t");
                value = value.substr(valueStart, valueEnd - valueStart + 1);
            } else {
                value.clear();
            }
            
            if (!key.empty()) {
                headers[key] = value;
            }
        }
    }
}

// Constructor
WinHttpCurl::WinHttpCurl() : hSession(NULL), initialized(false) {
}

// Destructor
WinHttpCurl::~WinHttpCurl() {
    Cleanup();
}

// Initialize WinHTTP session
bool WinHttpCurl::Init() {
    if (initialized) return true;
    
    hSession = WinHttpOpen(L"WinHTTP-Curl/1.0",
                          WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                          WINHTTP_NO_PROXY_NAME,
                          WINHTTP_NO_PROXY_BYPASS, 0);
    
    if (hSession == NULL) {
        return false;
    }
    
    initialized = true;
    return true;
}

// Cleanup WinHTTP session
void WinHttpCurl::Cleanup() {
    if (hSession != NULL) {
        WinHttpCloseHandle(hSession);
        hSession = NULL;
    }
    initialized = false;
}

// Generic request method
HttpResponse WinHttpCurl::Request(const std::string& method, const std::string& url, 
                                 const std::string& data, const HttpOptions& options) {
    HttpResponse response;
    
    if (!initialized && !Init()) {
        response.error = "Failed to initialize WinHTTP";
        return response;
    }
    
    // Parse URL
    std::string scheme, host, path;
    int port;
    ParseUrl(url, scheme, host, path, port);
    
    if (host.empty()) {
        response.error = "Invalid URL";
        return response;
    }
    
    std::wstring wHost = StringToWString(host);
    std::wstring wPath = StringToWString(path);
    
    // Connect to server
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
    if (hConnect == NULL) {
        DWORD error = GetLastError();
        response.error = "Failed to connect: " + std::to_string(error);
        return response;
    }
    
    // Determine request flags
    DWORD requestFlags = 0;
    if (scheme == "https") {
        requestFlags = WINHTTP_FLAG_SECURE;
    }
    
    // Open request
    std::wstring wMethod = StringToWString(method);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wMethod.c_str(), wPath.c_str(),
                                           NULL, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, requestFlags);
    
    if (hRequest == NULL) {
        response.error = "Failed to open request";
        WinHttpCloseHandle(hConnect);
        return response;
    }
    
    // Set SSL certificate verification
    if (scheme == "https" && !options.verifySSL) {
        DWORD securityFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                              SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                              SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                              SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS,
                        &securityFlags, sizeof(securityFlags));
    }
    
    // Set timeout
    DWORD timeout = options.timeout * 1000; // Convert to milliseconds
    WinHttpSetTimeouts(hRequest, timeout, timeout, timeout, timeout);
    
    // Set user agent
    if (!options.userAgent.empty()) {
        std::wstring wUserAgent = StringToWString(options.userAgent);
        WinHttpAddRequestHeaders(hRequest, (L"User-Agent: " + wUserAgent).c_str(),
                                -1, WINHTTP_ADDREQ_FLAG_ADD);
    }
    
    // Add custom headers
    bool hasContentLength = false;
    for (const auto& header : options.headers) {
        std::string headerName = header.first;
        std::transform(headerName.begin(), headerName.end(), headerName.begin(), ::tolower);
        if (headerName == "content-length") {
            hasContentLength = true;
        }
        std::string headerLine = header.first + ": " + header.second;
        std::wstring wHeaderLine = StringToWString(headerLine);
        WinHttpAddRequestHeaders(hRequest, wHeaderLine.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);
    }
    
    // Set Content-Length header if not already set and we have data
    if (!data.empty() && !hasContentLength) {
        std::string contentLengthHeader = "Content-Length: " + std::to_string(data.length());
        std::wstring wContentLength = StringToWString(contentLengthHeader);
        WinHttpAddRequestHeaders(hRequest, wContentLength.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    }
    
    // Set proxy if specified
    if (!options.proxy.empty()) {
        std::wstring wProxy = StringToWString(options.proxy);
        WINHTTP_PROXY_INFO proxyInfo;
        proxyInfo.dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
        proxyInfo.lpszProxy = const_cast<LPWSTR>(wProxy.c_str());
        proxyInfo.lpszProxyBypass = NULL;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_PROXY, &proxyInfo, sizeof(proxyInfo));
    }
    
    // Set authentication if provided
    if (!options.username.empty() && !options.password.empty()) {
        std::wstring wUsername = StringToWString(options.username);
        std::wstring wPassword = StringToWString(options.password);
        WinHttpSetCredentials(hRequest, WINHTTP_AUTH_TARGET_SERVER,
                             WINHTTP_AUTH_SCHEME_BASIC, wUsername.c_str(),
                             wPassword.c_str(), NULL);
    }
    
    // Send request
    BOOL bResults = FALSE;
    if (data.empty()) {
        bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                     WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    } else {
        std::string requestData = data;
        DWORD dataLength = (DWORD)requestData.length();
        bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                     (LPVOID)requestData.c_str(), dataLength, dataLength, 0);
    }
    
    if (!bResults) {
        DWORD error = GetLastError();
        std::ostringstream errorMsg;
        errorMsg << "Failed to send request (Error code: " << error << ")";
        response.error = errorMsg.str();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return response;
    }
    
    // Receive response
    bResults = WinHttpReceiveResponse(hRequest, NULL);
    if (!bResults) {
        response.error = "Failed to receive response";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return response;
    }
    
    // Get status code
    DWORD dwStatusCode = 0;
    DWORD dwStatusCodeSize = sizeof(dwStatusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                       WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwStatusCodeSize,
                       WINHTTP_NO_HEADER_INDEX);
    response.statusCode = dwStatusCode;
    
    // Get response headers
    DWORD dwHeaderSize = 0;
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                       WINHTTP_HEADER_NAME_BY_INDEX, NULL, &dwHeaderSize,
                       WINHTTP_NO_HEADER_INDEX);
    
    if (dwHeaderSize > 0) {
        std::vector<wchar_t> headerBuffer(dwHeaderSize / sizeof(wchar_t));
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                           WINHTTP_HEADER_NAME_BY_INDEX, headerBuffer.data(), &dwHeaderSize,
                           WINHTTP_NO_HEADER_INDEX);
        std::string headersStr = WStringToString(std::wstring(headerBuffer.data()));
        ParseHeaders(headersStr, response.headers);
    }
    
    // Read response body
    std::string responseBody;
    DWORD dwDownloaded = 0;
    do {
        DWORD dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
            break;
        }
        
        if (dwSize == 0) break;
        
        std::vector<char> buffer(dwSize);
        if (!WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
            break;
        }
        
        responseBody.append(buffer.data(), dwDownloaded);
    } while (dwDownloaded > 0);
    
    response.body = responseBody;
    
    // Handle redirects if enabled
    if (options.followRedirects && (response.statusCode == 301 || response.statusCode == 302 ||
                                   response.statusCode == 303 || response.statusCode == 307 ||
                                   response.statusCode == 308)) {
        auto locationIt = response.headers.find("Location");
        if (locationIt != response.headers.end()) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            return Request(method, locationIt->second, data, options);
        }
    }
    
    // Cleanup
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    
    return response;
}

// GET request
HttpResponse WinHttpCurl::Get(const std::string& url, const HttpOptions& options) {
    return Request("GET", url, "", options);
}

// POST request
HttpResponse WinHttpCurl::Post(const std::string& url, const std::string& data, 
                              const HttpOptions& options) {
    HttpOptions opts = options;
    if (opts.headers.find("Content-Type") == opts.headers.end()) {
        opts.headers["Content-Type"] = "application/x-www-form-urlencoded";
    }
    return Request("POST", url, data, opts);
}

// PUT request
HttpResponse WinHttpCurl::Put(const std::string& url, const std::string& data, 
                             const HttpOptions& options) {
    HttpOptions opts = options;
    if (opts.headers.find("Content-Type") == opts.headers.end()) {
        opts.headers["Content-Type"] = "application/x-www-form-urlencoded";
    }
    return Request("PUT", url, data, opts);
}

// DELETE request
HttpResponse WinHttpCurl::Delete(const std::string& url, const HttpOptions& options) {
    return Request("DELETE", url, "", options);
}

// PATCH request
HttpResponse WinHttpCurl::Patch(const std::string& url, const std::string& data, 
                               const HttpOptions& options) {
    HttpOptions opts = options;
    if (opts.headers.find("Content-Type") == opts.headers.end()) {
        opts.headers["Content-Type"] = "application/x-www-form-urlencoded";
    }
    return Request("PATCH", url, data, opts);
}

// Convenience functions
namespace curl {
    HttpResponse get(const std::string& url, const HttpOptions& options) {
        WinHttpCurl client;
        if (!client.Init()) {
            HttpResponse response;
            response.error = "Failed to initialize WinHTTP";
            return response;
        }
        return client.Get(url, options);
    }
    
    HttpResponse post(const std::string& url, const std::string& data, 
                     const HttpOptions& options) {
        WinHttpCurl client;
        if (!client.Init()) {
            HttpResponse response;
            response.error = "Failed to initialize WinHTTP";
            return response;
        }
        return client.Post(url, data, options);
    }
    
    HttpResponse put(const std::string& url, const std::string& data, 
                    const HttpOptions& options) {
        WinHttpCurl client;
        if (!client.Init()) {
            HttpResponse response;
            response.error = "Failed to initialize WinHTTP";
            return response;
        }
        return client.Put(url, data, options);
    }
    
    HttpResponse del(const std::string& url, const HttpOptions& options) {
        WinHttpCurl client;
        if (!client.Init()) {
            HttpResponse response;
            response.error = "Failed to initialize WinHTTP";
            return response;
        }
        return client.Delete(url, options);
    }
    
    HttpResponse patch(const std::string& url, const std::string& data, 
                      const HttpOptions& options) {
        WinHttpCurl client;
        if (!client.Init()) {
            HttpResponse response;
            response.error = "Failed to initialize WinHTTP";
            return response;
        }
        return client.Patch(url, data, options);
    }
}

