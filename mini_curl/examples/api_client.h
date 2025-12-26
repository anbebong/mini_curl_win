#ifndef API_CLIENT_H
#define API_CLIENT_H

#include "../include/winhttp_curl.h"
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <windows.h>
#include <direct.h>
#include <errno.h>

// URL encode function
static std::string UrlEncode(const std::string& str) {
    std::ostringstream encoded;
    encoded.fill('0');
    encoded << std::hex;

    for (char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
            encoded << c;
        } else {
            encoded << '%' << std::setw(2) << int((unsigned char)c);
        }
    }

    return encoded.str();
}

// Get executable directory path
static std::string GetExecutableDirectory() {
    char buffer[MAX_PATH];
    DWORD length = GetModuleFileNameA(NULL, buffer, MAX_PATH);
    if (length == 0) {
        return ".";
    }
    
    std::string exePath(buffer, length);
    size_t lastSlash = exePath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        return exePath.substr(0, lastSlash);
    }
    return ".";
}

// Create directory if not exists
static bool CreateDirectoryIfNotExists(const std::string& dirPath) {
    if (dirPath.empty()) {
        return false;
    }
    
    // Try to create directory
    int result = _mkdir(dirPath.c_str());
    if (result == 0 || errno == EEXIST) {
        return true;
    }
    
    // If failed, try to create parent directories
    size_t pos = dirPath.find_last_of("\\/");
    if (pos != std::string::npos && pos > 0) {
        std::string parent = dirPath.substr(0, pos);
        if (CreateDirectoryIfNotExists(parent)) {
            result = _mkdir(dirPath.c_str());
            return (result == 0 || errno == EEXIST);
        }
    }
    
    return false;
}

// Client để gọi các API endpoints
class ApiClient {
private:
    std::string baseUrl;
    WinHttpCurl client;
    bool initialized;
    bool verifySSL;

public:
    ApiClient(const std::string& url = "http://localhost:8766", bool verifyCert = true) 
        : baseUrl(url), initialized(false), verifySSL(verifyCert) {
    }
    
    // Set SSL certificate verification
    void SetVerifySSL(bool verify) {
        verifySSL = verify;
    }

    bool Init() {
        initialized = client.Init();
        return initialized;
    }

    void Cleanup() {
        if (initialized) {
            client.Cleanup();
            initialized = false;
        }
    }

    // GET /api/config - Lấy config từ server
    HttpResponse GetConfig(std::string& configJson) {
        if (!initialized) {
            HttpResponse error;
            error.error = "Client not initialized. Call Init() first.";
            return error;
        }

        std::string url = baseUrl + "/api/config";
        HttpOptions options;
        options.verifySSL = verifySSL;
        HttpResponse response = client.Get(url, options);
        
        if (response.statusCode == 200) {
            configJson = response.body;
        }
        
        return response;
    }

    // GET /app/agent - Lấy danh sách applications (cần X-Auth-Token header)
    HttpResponse GetApplications(const std::string& authToken, std::string& responseJson) {
        if (!initialized) {
            HttpResponse error;
            error.error = "Client not initialized. Call Init() first.";
            return error;
        }

        std::string url = baseUrl + "/app/agent";
        HttpOptions options;
        options.verifySSL = verifySSL;
        options.headers["X-Auth-Token"] = authToken;
        
        HttpResponse response = client.Get(url, options);
        
        if (response.statusCode == 200) {
            responseJson = response.body;
        }
        
        return response;
    }

    // GET /api/download?id={service_id} - Download file bằng service ID
    bool DownloadFileById(int serviceId, const std::string& outputPath, std::string* errorMsg = nullptr) {
        if (!initialized) {
            if (errorMsg) *errorMsg = "Client not initialized";
            return false;
        }

        std::stringstream url;
        url << baseUrl << "/api/download?id=" << serviceId;
        
        HttpOptions options;
        options.verifySSL = verifySSL;
        HttpResponse response = client.Get(url.str(), options);
        
        if (response.statusCode == 200) {
            if (response.body.empty()) {
                if (errorMsg) *errorMsg = "Response body is empty";
                return false;
            }
            
            std::ofstream file(outputPath, std::ios::binary);
            if (!file.is_open()) {
                if (errorMsg) *errorMsg = "Failed to create output file: " + outputPath;
                return false;
            }
            
            file.write(response.body.c_str(), response.body.size());
            file.close();
            return true;
        }
        
        if (errorMsg) {
            *errorMsg = "HTTP " + std::to_string(response.statusCode);
            if (!response.error.empty()) {
                *errorMsg += ": " + response.error;
            }
            if (!response.body.empty() && response.body.size() < 500) {
                *errorMsg += " - " + response.body;
            }
        }
        
        return false;
    }

    // GET /api/download?file_path={path} - Download file bằng file path
    bool DownloadFileByPath(const std::string& filePath, const std::string& outputPath, std::string* errorMsg = nullptr) {
        if (!initialized) {
            if (errorMsg) *errorMsg = "Client not initialized";
            return false;
        }

        std::stringstream url;
        url << baseUrl << "/api/download?file_path=" << UrlEncode(filePath);
        
        HttpOptions options;
        options.verifySSL = verifySSL;
        HttpResponse response = client.Get(url.str(), options);
        
        if (response.statusCode == 200) {
            if (response.body.empty()) {
                if (errorMsg) *errorMsg = "Response body is empty";
                return false;
            }
            
            std::ofstream file(outputPath, std::ios::binary);
            if (!file.is_open()) {
                if (errorMsg) *errorMsg = "Failed to create output file: " + outputPath;
                return false;
            }
            
            file.write(response.body.c_str(), response.body.size());
            file.close();
            return true;
        }
        
        if (errorMsg) {
            *errorMsg = "HTTP " + std::to_string(response.statusCode);
            if (!response.error.empty()) {
                *errorMsg += ": " + response.error;
            }
            if (!response.body.empty() && response.body.size() < 500) {
                *errorMsg += " - " + response.body;
            }
        }
        
        return false;
    }

    // GET /api/download?service_name={name}&os={os}&version={version} - Download file bằng service_name
    // Hỗ trợ GET request với query parameters như curl example
    // outputDir: Thư mục lưu file (mặc định: "downloads" trong thư mục chứa file thực thi)
    bool DownloadFileByServiceName(const std::string& serviceName, 
                                   const std::string& os, 
                                   const std::string& version = "",
                                   const std::string& outputDir = "",
                                   std::string* errorMsg = nullptr) {
        if (!initialized) {
            if (errorMsg) *errorMsg = "Client not initialized";
            return false;
        }

        // Tạo URL với query parameters
        std::stringstream url;
        url << baseUrl << "/api/download?service_name=" << UrlEncode(serviceName);
        
        if (!os.empty()) {
            url << "&os=" << UrlEncode(os);
        }
        
        if (!version.empty()) {
            url << "&version=" << UrlEncode(version);
        }
        
        HttpOptions options;
        options.verifySSL = verifySSL;
        HttpResponse response = client.Get(url.str(), options);
        
        if (response.statusCode == 200) {
            if (response.body.empty()) {
                if (errorMsg) *errorMsg = "Response body is empty";
                return false;
            }
            
            // Lấy filename từ server (Content-Disposition header)
            std::string fileName;
            auto it = response.headers.find("Content-Disposition");
            if (it != response.headers.end()) {
                std::string contentDisposition = it->second;
                size_t filenamePos = contentDisposition.find("filename=");
                if (filenamePos != std::string::npos) {
                    size_t start = contentDisposition.find('"', filenamePos);
                    if (start != std::string::npos) {
                        start++;
                        size_t end = contentDisposition.find('"', start);
                        if (end != std::string::npos) {
                            fileName = contentDisposition.substr(start, end - start);
                        } else {
                            // Tìm dấu ; hoặc kết thúc string
                            end = contentDisposition.find(';', start);
                            if (end == std::string::npos) {
                                end = contentDisposition.length();
                            }
                            fileName = contentDisposition.substr(start, end - start);
                        }
                    } else {
                        // Không có dấu ", thử tìm sau dấu =
                        start = filenamePos + 9; // "filename=" length
                        size_t end = contentDisposition.find(';', start);
                        if (end == std::string::npos) {
                            end = contentDisposition.length();
                        }
                        fileName = contentDisposition.substr(start, end - start);
                        // Trim whitespace
                        size_t first = fileName.find_first_not_of(" \t");
                        if (first != std::string::npos) {
                            size_t last = fileName.find_last_not_of(" \t");
                            fileName = fileName.substr(first, last - first + 1);
                        }
                    }
                }
            }
            
            // Nếu không có filename từ server, báo lỗi
            if (fileName.empty()) {
                if (errorMsg) *errorMsg = "No filename in Content-Disposition header";
                return false;
            }
            
            // Xác định thư mục lưu file
            std::string finalDir = outputDir;
            if (finalDir.empty()) {
                // Mặc định: downloads trong thư mục chứa file thực thi
                finalDir = GetExecutableDirectory() + "\\downloads";
            }
            
            // Tạo thư mục nếu chưa tồn tại
            if (!CreateDirectoryIfNotExists(finalDir)) {
                if (errorMsg) *errorMsg = "Failed to create directory: " + finalDir;
                return false;
            }
            
            // Tạo đường dẫn file đầy đủ
            std::string finalPath = finalDir;
            if (finalPath.back() != '\\' && finalPath.back() != '/') {
                finalPath += "\\";
            }
            finalPath += fileName;
            
            std::ofstream file(finalPath, std::ios::binary);
            if (!file.is_open()) {
                if (errorMsg) *errorMsg = "Failed to create output file: " + finalPath;
                return false;
            }
            
            file.write(response.body.c_str(), response.body.size());
            file.close();
            return true;
        }
        
        if (errorMsg) {
            *errorMsg = "HTTP " + std::to_string(response.statusCode);
            if (!response.error.empty()) {
                *errorMsg += ": " + response.error;
            }
            if (!response.body.empty() && response.body.size() < 500) {
                *errorMsg += " - " + response.body;
            }
        }
        
        return false;
    }

    ~ApiClient() {
        Cleanup();
    }
};

#endif // API_CLIENT_H

