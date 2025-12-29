// Mini Curl - C API Implementation for DLL
//
// Implementation các C functions để export từ DLL
// Cho phép load DLL trực tiếp bằng LoadLibrary/GetProcAddress

#include "../include/mini_curl_c.h"
#include "../include/winhttp_curl.h"
#include <string>
#include <cstring>
#include <sstream>

// Define export khi build DLL
#ifdef BUILD_DLL
#define MINI_CURL_C_EXPORTS
#endif

// Internal structure
struct MiniCurlInternal {
    WinHttpCurl http;
    bool initialized;
    
    MiniCurlInternal() : initialized(false) {}
};

// Helper function để parse custom headers từ string
static void ParseCustomHeaders(const char* customHeaders, HttpOptions& httpOptions) {
    if (!customHeaders) return;
    
    std::string headersStr = customHeaders;
    std::istringstream stream(headersStr);
    std::string line;
    while (std::getline(stream, line)) {
        // Remove \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        // Find colon separator
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string headerName = line.substr(0, colonPos);
            std::string headerValue = line.substr(colonPos + 1);
            // Trim whitespace
            while (!headerName.empty() && (headerName[0] == ' ' || headerName[0] == '\t')) {
                headerName.erase(0, 1);
            }
            while (!headerName.empty() && (headerName.back() == ' ' || headerName.back() == '\t')) {
                headerName.pop_back();
            }
            while (!headerValue.empty() && (headerValue[0] == ' ' || headerValue[0] == '\t')) {
                headerValue.erase(0, 1);
            }
            while (!headerValue.empty() && (headerValue.back() == ' ' || headerValue.back() == '\t')) {
                headerValue.pop_back();
            }
            if (!headerName.empty()) {
                httpOptions.headers[headerName] = headerValue;
            }
        }
    }
}

// Helper function để convert headers map thành string
static char* ConvertHeadersToString(const std::map<std::string, std::string>& headers) {
    if (headers.empty()) {
        return NULL;
    }
    
    std::string headersStr;
    for (const auto& header : headers) {
        if (!headersStr.empty()) {
            headersStr += "\n";
        }
        headersStr += header.first + ": " + header.second;
    }
    
    char* result = (char*)malloc(headersStr.length() + 1);
    strcpy(result, headersStr.c_str());
    return result;
}

extern "C" {

MiniCurlHandle mini_curl_c_create(void) {
    return new MiniCurlInternal();
}

void mini_curl_c_destroy(MiniCurlHandle handle) {
    if (handle) {
        MiniCurlInternal* curl = (MiniCurlInternal*)handle;
        if (curl->initialized) {
            curl->http.Cleanup();
        }
        delete curl;
    }
}

int mini_curl_c_init(MiniCurlHandle handle) {
    if (!handle) return 0;
    MiniCurlInternal* curl = (MiniCurlInternal*)handle;
    curl->initialized = curl->http.Init();
    return curl->initialized ? 1 : 0;
}

void mini_curl_c_cleanup(MiniCurlHandle handle) {
    if (handle) {
        MiniCurlInternal* curl = (MiniCurlInternal*)handle;
        if (curl->initialized) {
            curl->http.Cleanup();
            curl->initialized = false;
        }
    }
}

MiniCurlResponse* mini_curl_c_get(MiniCurlHandle handle, 
                                   const char* url, 
                                   const MiniCurlOptions* options) {
    if (!handle || !url) {
        MiniCurlResponse* response = (MiniCurlResponse*)malloc(sizeof(MiniCurlResponse));
        response->statusCode = 0;
        response->body = NULL;
        response->bodySize = 0;
        response->error = (char*)malloc(64);
        strcpy(response->error, "Invalid handle or URL");
        response->headers = NULL;
        return response;
    }
    
    MiniCurlInternal* curl = (MiniCurlInternal*)handle;
    if (!curl->initialized) {
        MiniCurlResponse* response = (MiniCurlResponse*)malloc(sizeof(MiniCurlResponse));
        response->statusCode = 0;
        response->body = NULL;
        response->bodySize = 0;
        response->error = (char*)malloc(64);
        strcpy(response->error, "Not initialized");
        response->headers = NULL;
        return response;
    }
    
    // Convert options
    HttpOptions httpOptions;
    if (options) {
        if (options->userAgent) {
            httpOptions.userAgent = options->userAgent;
        }
        httpOptions.timeout = options->timeout > 0 ? options->timeout : 30;
        httpOptions.followRedirects = options->followRedirects != 0;
        httpOptions.verifySSL = options->verifySSL != 0;
        if (options->proxy) {
            httpOptions.proxy = options->proxy;
        }
        if (options->username && options->password) {
            httpOptions.username = options->username;
            httpOptions.password = options->password;
        }
        // Parse custom headers
        ParseCustomHeaders(options->customHeaders, httpOptions);
    }
    
    // Call C++ API
    HttpResponse httpResponse = curl->http.Get(url, httpOptions);
    
    // Convert to C response
    MiniCurlResponse* response = (MiniCurlResponse*)malloc(sizeof(MiniCurlResponse));
    response->statusCode = httpResponse.statusCode;
    response->bodySize = httpResponse.body.length();
    
    if (httpResponse.body.length() > 0) {
        // Dùng memcpy thay vì strcpy để copy binary data đúng cách (không dừng khi gặp null byte)
        response->body = (char*)malloc(httpResponse.body.length());
        if (response->body) {
            memcpy(response->body, httpResponse.body.data(), httpResponse.body.length());
        }
    } else {
        response->body = NULL;
    }
    
    if (!httpResponse.error.empty()) {
        response->error = (char*)malloc(httpResponse.error.length() + 1);
        strcpy(response->error, httpResponse.error.c_str());
    } else {
        response->error = NULL;
    }
    
    // Convert headers map to string
    if (!httpResponse.headers.empty()) {
        std::string headersStr;
        for (const auto& header : httpResponse.headers) {
            if (!headersStr.empty()) {
                headersStr += "\n";
            }
            headersStr += header.first + ": " + header.second;
        }
        response->headers = (char*)malloc(headersStr.length() + 1);
        strcpy(response->headers, headersStr.c_str());
    } else {
        response->headers = NULL;
    }
    
    return response;
}

MiniCurlResponse* mini_curl_c_post(MiniCurlHandle handle,
                                    const char* url,
                                    const char* data,
                                    const char* contentType,
                                    const MiniCurlOptions* options) {
    if (!handle || !url) {
        MiniCurlResponse* response = (MiniCurlResponse*)malloc(sizeof(MiniCurlResponse));
        response->statusCode = 0;
        response->body = NULL;
        response->bodySize = 0;
        response->error = (char*)malloc(64);
        strcpy(response->error, "Invalid handle or URL");
        response->headers = NULL;
        return response;
    }
    
    MiniCurlInternal* curl = (MiniCurlInternal*)handle;
    if (!curl->initialized) {
        MiniCurlResponse* response = (MiniCurlResponse*)malloc(sizeof(MiniCurlResponse));
        response->statusCode = 0;
        response->body = NULL;
        response->bodySize = 0;
        response->error = (char*)malloc(64);
        strcpy(response->error, "Not initialized");
        response->headers = NULL;
        return response;
    }
    
    // Convert options
    HttpOptions httpOptions;
    if (options) {
        if (options->userAgent) {
            httpOptions.userAgent = options->userAgent;
        }
        httpOptions.timeout = options->timeout > 0 ? options->timeout : 30;
        httpOptions.followRedirects = options->followRedirects != 0;
        httpOptions.verifySSL = options->verifySSL != 0;
        // Parse custom headers
        ParseCustomHeaders(options->customHeaders, httpOptions);
    }
    
    if (contentType) {
        httpOptions.headers["Content-Type"] = contentType;
    } else if (data) {
        httpOptions.headers["Content-Type"] = "application/json";
    }
    
    // Call C++ API
    HttpResponse httpResponse = curl->http.Post(url, data ? data : "", httpOptions);
    
    // Convert to C response
    MiniCurlResponse* response = (MiniCurlResponse*)malloc(sizeof(MiniCurlResponse));
    response->statusCode = httpResponse.statusCode;
    response->bodySize = httpResponse.body.length();
    
    if (httpResponse.body.length() > 0) {
        // Dùng memcpy thay vì strcpy để copy binary data đúng cách (không dừng khi gặp null byte)
        response->body = (char*)malloc(httpResponse.body.length());
        if (response->body) {
            memcpy(response->body, httpResponse.body.data(), httpResponse.body.length());
        }
    } else {
        response->body = NULL;
    }
    
    if (!httpResponse.error.empty()) {
        response->error = (char*)malloc(httpResponse.error.length() + 1);
        strcpy(response->error, httpResponse.error.c_str());
    } else {
        response->error = NULL;
    }
    
    // Convert headers
    response->headers = ConvertHeadersToString(httpResponse.headers);
    
    return response;
}

MiniCurlResponse* mini_curl_c_put(MiniCurlHandle handle,
                                    const char* url,
                                    const char* data,
                                    const char* contentType,
                                    const MiniCurlOptions* options) {
    if (!handle || !url) {
        MiniCurlResponse* response = (MiniCurlResponse*)malloc(sizeof(MiniCurlResponse));
        response->statusCode = 0;
        response->body = NULL;
        response->bodySize = 0;
        response->error = (char*)malloc(64);
        strcpy(response->error, "Invalid handle or URL");
        response->headers = NULL;
        return response;
    }
    
    MiniCurlInternal* curl = (MiniCurlInternal*)handle;
    if (!curl->initialized) {
        MiniCurlResponse* response = (MiniCurlResponse*)malloc(sizeof(MiniCurlResponse));
        response->statusCode = 0;
        response->body = NULL;
        response->bodySize = 0;
        response->error = (char*)malloc(64);
        strcpy(response->error, "Not initialized");
        response->headers = NULL;
        return response;
    }
    
    HttpOptions httpOptions;
    if (options) {
        if (options->userAgent) {
            httpOptions.userAgent = options->userAgent;
        }
        httpOptions.timeout = options->timeout > 0 ? options->timeout : 30;
        httpOptions.followRedirects = options->followRedirects != 0;
        httpOptions.verifySSL = options->verifySSL != 0;
    }
    
    if (contentType) {
        httpOptions.headers["Content-Type"] = contentType;
    } else if (data) {
        httpOptions.headers["Content-Type"] = "application/json";
    }
    
    HttpResponse httpResponse = curl->http.Put(url, data ? data : "", httpOptions);
    
    MiniCurlResponse* response = (MiniCurlResponse*)malloc(sizeof(MiniCurlResponse));
    response->statusCode = httpResponse.statusCode;
    response->bodySize = httpResponse.body.length();
    
    if (httpResponse.body.length() > 0) {
        // Dùng memcpy thay vì strcpy để copy binary data đúng cách (không dừng khi gặp null byte)
        response->body = (char*)malloc(httpResponse.body.length());
        if (response->body) {
            memcpy(response->body, httpResponse.body.data(), httpResponse.body.length());
        }
    } else {
        response->body = NULL;
    }
    
    if (!httpResponse.error.empty()) {
        response->error = (char*)malloc(httpResponse.error.length() + 1);
        strcpy(response->error, httpResponse.error.c_str());
    } else {
        response->error = NULL;
    }
    
    // Convert headers
    response->headers = ConvertHeadersToString(httpResponse.headers);
    
    return response;
}

MiniCurlResponse* mini_curl_c_delete(MiniCurlHandle handle,
                                       const char* url,
                                       const MiniCurlOptions* options) {
    if (!handle || !url) {
        MiniCurlResponse* response = (MiniCurlResponse*)malloc(sizeof(MiniCurlResponse));
        response->statusCode = 0;
        response->body = NULL;
        response->bodySize = 0;
        response->error = (char*)malloc(64);
        strcpy(response->error, "Invalid handle or URL");
        response->headers = NULL;
        return response;
    }
    
    MiniCurlInternal* curl = (MiniCurlInternal*)handle;
    if (!curl->initialized) {
        MiniCurlResponse* response = (MiniCurlResponse*)malloc(sizeof(MiniCurlResponse));
        response->statusCode = 0;
        response->body = NULL;
        response->bodySize = 0;
        response->error = (char*)malloc(64);
        strcpy(response->error, "Not initialized");
        response->headers = NULL;
        return response;
    }
    
    HttpOptions httpOptions;
    if (options) {
        if (options->userAgent) {
            httpOptions.userAgent = options->userAgent;
        }
        httpOptions.timeout = options->timeout > 0 ? options->timeout : 30;
        httpOptions.followRedirects = options->followRedirects != 0;
        httpOptions.verifySSL = options->verifySSL != 0;
        // Parse custom headers
        ParseCustomHeaders(options->customHeaders, httpOptions);
    }
    
    HttpResponse httpResponse = curl->http.Delete(url, httpOptions);
    
    MiniCurlResponse* response = (MiniCurlResponse*)malloc(sizeof(MiniCurlResponse));
    response->statusCode = httpResponse.statusCode;
    response->bodySize = httpResponse.body.length();
    
    if (httpResponse.body.length() > 0) {
        // Dùng memcpy thay vì strcpy để copy binary data đúng cách (không dừng khi gặp null byte)
        response->body = (char*)malloc(httpResponse.body.length());
        if (response->body) {
            memcpy(response->body, httpResponse.body.data(), httpResponse.body.length());
        }
    } else {
        response->body = NULL;
    }
    
    if (!httpResponse.error.empty()) {
        response->error = (char*)malloc(httpResponse.error.length() + 1);
        strcpy(response->error, httpResponse.error.c_str());
    } else {
        response->error = NULL;
    }
    
    // Convert headers
    response->headers = ConvertHeadersToString(httpResponse.headers);
    
    return response;
}

MiniCurlResponse* mini_curl_c_patch(MiniCurlHandle handle,
                                     const char* url,
                                     const char* data,
                                     const char* contentType,
                                     const MiniCurlOptions* options) {
    if (!handle || !url) {
        MiniCurlResponse* response = (MiniCurlResponse*)malloc(sizeof(MiniCurlResponse));
        response->statusCode = 0;
        response->body = NULL;
        response->bodySize = 0;
        response->error = (char*)malloc(64);
        strcpy(response->error, "Invalid handle or URL");
        response->headers = NULL;
        return response;
    }
    
    MiniCurlInternal* curl = (MiniCurlInternal*)handle;
    if (!curl->initialized) {
        MiniCurlResponse* response = (MiniCurlResponse*)malloc(sizeof(MiniCurlResponse));
        response->statusCode = 0;
        response->body = NULL;
        response->bodySize = 0;
        response->error = (char*)malloc(64);
        strcpy(response->error, "Not initialized");
        response->headers = NULL;
        return response;
    }
    
    HttpOptions httpOptions;
    if (options) {
        if (options->userAgent) {
            httpOptions.userAgent = options->userAgent;
        }
        httpOptions.timeout = options->timeout > 0 ? options->timeout : 30;
        httpOptions.followRedirects = options->followRedirects != 0;
        httpOptions.verifySSL = options->verifySSL != 0;
    }
    
    if (contentType) {
        httpOptions.headers["Content-Type"] = contentType;
    } else if (data) {
        httpOptions.headers["Content-Type"] = "application/json";
    }
    
    HttpResponse httpResponse = curl->http.Patch(url, data ? data : "", httpOptions);
    
    MiniCurlResponse* response = (MiniCurlResponse*)malloc(sizeof(MiniCurlResponse));
    response->statusCode = httpResponse.statusCode;
    response->bodySize = httpResponse.body.length();
    
    if (httpResponse.body.length() > 0) {
        // Dùng memcpy thay vì strcpy để copy binary data đúng cách (không dừng khi gặp null byte)
        response->body = (char*)malloc(httpResponse.body.length());
        if (response->body) {
            memcpy(response->body, httpResponse.body.data(), httpResponse.body.length());
        }
    } else {
        response->body = NULL;
    }
    
    if (!httpResponse.error.empty()) {
        response->error = (char*)malloc(httpResponse.error.length() + 1);
        strcpy(response->error, httpResponse.error.c_str());
    } else {
        response->error = NULL;
    }
    
    // Convert headers
    response->headers = ConvertHeadersToString(httpResponse.headers);
    
    return response;
}

MiniCurlResponse* mini_curl_c_request(MiniCurlHandle handle,
                                       const char* method,
                                       const char* url,
                                       const char* data,
                                       const char* contentType,
                                       const MiniCurlOptions* options) {
    if (!handle || !url || !method) {
        MiniCurlResponse* response = (MiniCurlResponse*)malloc(sizeof(MiniCurlResponse));
        response->statusCode = 0;
        response->body = NULL;
        response->bodySize = 0;
        response->error = (char*)malloc(64);
        strcpy(response->error, "Invalid handle, URL, or method");
        return response;
    }
    
    MiniCurlInternal* curl = (MiniCurlInternal*)handle;
    if (!curl->initialized) {
        MiniCurlResponse* response = (MiniCurlResponse*)malloc(sizeof(MiniCurlResponse));
        response->statusCode = 0;
        response->body = NULL;
        response->bodySize = 0;
        response->error = (char*)malloc(64);
        strcpy(response->error, "Not initialized");
        return response;
    }
    
    HttpOptions httpOptions;
    if (options) {
        if (options->userAgent) {
            httpOptions.userAgent = options->userAgent;
        }
        httpOptions.timeout = options->timeout > 0 ? options->timeout : 30;
        httpOptions.followRedirects = options->followRedirects != 0;
        httpOptions.verifySSL = options->verifySSL != 0;
    }
    
    if (contentType && data) {
        httpOptions.headers["Content-Type"] = contentType;
    } else if (data) {
        httpOptions.headers["Content-Type"] = "application/json";
    }
    
    HttpResponse httpResponse = curl->http.Request(method, url, data ? data : "", httpOptions);
    
    MiniCurlResponse* response = (MiniCurlResponse*)malloc(sizeof(MiniCurlResponse));
    response->statusCode = httpResponse.statusCode;
    response->bodySize = httpResponse.body.length();
    
    if (httpResponse.body.length() > 0) {
        // Dùng memcpy thay vì strcpy để copy binary data đúng cách (không dừng khi gặp null byte)
        response->body = (char*)malloc(httpResponse.body.length());
        if (response->body) {
            memcpy(response->body, httpResponse.body.data(), httpResponse.body.length());
        }
    } else {
        response->body = NULL;
    }
    
    if (!httpResponse.error.empty()) {
        response->error = (char*)malloc(httpResponse.error.length() + 1);
        strcpy(response->error, httpResponse.error.c_str());
    } else {
        response->error = NULL;
    }
    
    // Convert headers
    response->headers = ConvertHeadersToString(httpResponse.headers);
    
    return response;
}

void mini_curl_c_response_free(MiniCurlResponse* response) {
    if (!response) return;
    
    if (response->body) {
        free(response->body);
    }
    if (response->error) {
        free(response->error);
    }
    if (response->headers) {
        free(response->headers);
    }
    free(response);
}

} // extern "C"

