#ifndef WINHTTP_CURL_H
#define WINHTTP_CURL_H

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <map>
#include <vector>

// DLL Export/Import macros
#ifdef MINI_CURL_EXPORTS
    #define MINI_CURL_API __declspec(dllexport)
#else
    #define MINI_CURL_API __declspec(dllimport)
#endif

// For static linking, no import/export needed
#ifdef MINI_CURL_STATIC
    #undef MINI_CURL_API
    #define MINI_CURL_API
#endif

#pragma comment(lib, "winhttp.lib")

// Response structure similar to curl
struct MINI_CURL_API HttpResponse {
    int statusCode;
    std::string body;
    std::map<std::string, std::string> headers;
    std::string error;
    
    HttpResponse() : statusCode(0) {}
};

// Options for HTTP request
struct MINI_CURL_API HttpOptions {
    std::map<std::string, std::string> headers;
    std::string userAgent;
    int timeout; // in seconds
    bool followRedirects;
    bool verifySSL; // Verify SSL certificate (true = verify, false = skip verification)
    std::string proxy;
    std::string proxyUser;
    std::string proxyPassword;
    std::string username;
    std::string password;
    
    HttpOptions() : timeout(30), followRedirects(true), verifySSL(true) {
        userAgent = "WinHTTP-Curl/1.0";
    }
};

// Main class for HTTP requests
class MINI_CURL_API WinHttpCurl {
private:
    HINTERNET hSession;
    bool initialized;
    
    static std::wstring StringToWString(const std::string& str);
    static std::string WStringToString(const std::wstring& wstr);
    static void ParseUrl(const std::string& url, std::string& scheme, 
                        std::string& host, std::string& path, int& port);
    static void ParseHeaders(const std::string& headersStr, 
                            std::map<std::string, std::string>& headers);
    
public:
    WinHttpCurl();
    ~WinHttpCurl();
    
    bool Init();
    void Cleanup();
    
    // GET request
    HttpResponse Get(const std::string& url, const HttpOptions& options = HttpOptions());
    
    // POST request
    HttpResponse Post(const std::string& url, const std::string& data, 
                     const HttpOptions& options = HttpOptions());
    
    // PUT request
    HttpResponse Put(const std::string& url, const std::string& data, 
                    const HttpOptions& options = HttpOptions());
    
    // DELETE request
    HttpResponse Delete(const std::string& url, const HttpOptions& options = HttpOptions());
    
    // PATCH request
    HttpResponse Patch(const std::string& url, const std::string& data, 
                      const HttpOptions& options = HttpOptions());
    
    // Generic request method
    HttpResponse Request(const std::string& method, const std::string& url, 
                        const std::string& data = "", const HttpOptions& options = HttpOptions());
};

// Convenience functions (similar to curl command line)
namespace curl {
    HttpResponse get(const std::string& url, const HttpOptions& options = HttpOptions());
    HttpResponse post(const std::string& url, const std::string& data, 
                     const HttpOptions& options = HttpOptions());
    HttpResponse put(const std::string& url, const std::string& data, 
                    const HttpOptions& options = HttpOptions());
    HttpResponse del(const std::string& url, const HttpOptions& options = HttpOptions());
    HttpResponse patch(const std::string& url, const std::string& data, 
                      const HttpOptions& options = HttpOptions());
}

#endif // WINHTTP_CURL_H

