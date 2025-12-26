#include "api_client_c.h"
#include "api_client.h"
#include <string>
#include <cstring>

extern "C" {

ApiClientHandle api_client_create(const char* baseUrl, int verifySSL) {
    if (!baseUrl) {
        baseUrl = "http://localhost:8766";
    }
    ApiClient* client = new ApiClient(baseUrl, verifySSL != 0);
    return reinterpret_cast<ApiClientHandle>(client);
}

void api_client_destroy(ApiClientHandle handle) {
    if (handle) {
        ApiClient* client = reinterpret_cast<ApiClient*>(handle);
        delete client;
    }
}

int api_client_init(ApiClientHandle handle) {
    if (!handle) {
        return 0;
    }
    ApiClient* client = reinterpret_cast<ApiClient*>(handle);
    return client->Init() ? 1 : 0;
}

void api_client_cleanup(ApiClientHandle handle) {
    if (handle) {
        ApiClient* client = reinterpret_cast<ApiClient*>(handle);
        client->Cleanup();
    }
}

int api_client_get_config(ApiClientHandle handle, char* configJson, size_t bufferSize) {
    if (!handle || !configJson || bufferSize == 0) {
        return 0;
    }
    
    ApiClient* client = reinterpret_cast<ApiClient*>(handle);
    std::string config;
    HttpResponse response = client->GetConfig(config);
    
    if (response.statusCode == 200) {
        size_t copySize = config.length();
        if (copySize >= bufferSize) {
            copySize = bufferSize - 1;
        }
        strncpy(configJson, config.c_str(), copySize);
        configJson[copySize] = '\0';
        return 1;
    }
    
    return 0;
}

int api_client_download_file_by_id(ApiClientHandle handle, int serviceId, const char* outputPath, char* errorMsg, size_t errorMsgSize) {
    if (!handle) {
        if (errorMsg && errorMsgSize > 0) {
            strncpy(errorMsg, "Invalid handle", errorMsgSize - 1);
            errorMsg[errorMsgSize - 1] = '\0';
        }
        return 0;
    }
    
    ApiClient* client = reinterpret_cast<ApiClient*>(handle);
    std::string error;
    bool success = client->DownloadFileById(serviceId, outputPath ? outputPath : "", &error);
    
    if (!success && errorMsg && errorMsgSize > 0) {
        size_t copySize = error.length();
        if (copySize >= errorMsgSize) {
            copySize = errorMsgSize - 1;
        }
        strncpy(errorMsg, error.c_str(), copySize);
        errorMsg[copySize] = '\0';
    }
    
    return success ? 1 : 0;
}

int api_client_download_file_by_path(ApiClientHandle handle, const char* filePath, const char* outputPath, char* errorMsg, size_t errorMsgSize) {
    if (!handle) {
        if (errorMsg && errorMsgSize > 0) {
            strncpy(errorMsg, "Invalid handle", errorMsgSize - 1);
            errorMsg[errorMsgSize - 1] = '\0';
        }
        return 0;
    }
    
    ApiClient* client = reinterpret_cast<ApiClient*>(handle);
    std::string error;
    bool success = client->DownloadFileByPath(filePath ? filePath : "", outputPath ? outputPath : "", &error);
    
    if (!success && errorMsg && errorMsgSize > 0) {
        size_t copySize = error.length();
        if (copySize >= errorMsgSize) {
            copySize = errorMsgSize - 1;
        }
        strncpy(errorMsg, error.c_str(), copySize);
        errorMsg[copySize] = '\0';
    }
    
    return success ? 1 : 0;
}

int api_client_download_file_by_service_name(ApiClientHandle handle, 
                                             const char* serviceName, 
                                             const char* os, 
                                             const char* version,
                                             const char* outputDir,
                                             char* errorMsg, 
                                             size_t errorMsgSize) {
    if (!handle) {
        if (errorMsg && errorMsgSize > 0) {
            strncpy(errorMsg, "Invalid handle", errorMsgSize - 1);
            errorMsg[errorMsgSize - 1] = '\0';
        }
        return 0;
    }
    
    ApiClient* client = reinterpret_cast<ApiClient*>(handle);
    std::string error;
    bool success = client->DownloadFileByServiceName(
        serviceName ? serviceName : "",
        os ? os : "",
        version ? version : "",
        outputDir ? outputDir : "",
        &error
    );
    
    if (!success && errorMsg && errorMsgSize > 0) {
        size_t copySize = error.length();
        if (copySize >= errorMsgSize) {
            copySize = errorMsgSize - 1;
        }
        strncpy(errorMsg, error.c_str(), copySize);
        errorMsg[copySize] = '\0';
    }
    
    return success ? 1 : 0;
}

int api_client_get_applications(ApiClientHandle handle, 
                                 const char* authToken,
                                 char* responseJson, 
                                 size_t bufferSize,
                                 char* errorMsg, 
                                 size_t errorMsgSize) {
    if (!handle) {
        if (errorMsg && errorMsgSize > 0) {
            strncpy(errorMsg, "Invalid handle", errorMsgSize - 1);
            errorMsg[errorMsgSize - 1] = '\0';
        }
        return 0;
    }
    
    if (!authToken) {
        if (errorMsg && errorMsgSize > 0) {
            strncpy(errorMsg, "Auth token is required", errorMsgSize - 1);
            errorMsg[errorMsgSize - 1] = '\0';
        }
        return 0;
    }
    
    ApiClient* client = reinterpret_cast<ApiClient*>(handle);
    std::string response;
    HttpResponse httpResponse = client->GetApplications(authToken, response);
    
    if (httpResponse.statusCode == 200) {
        if (responseJson && bufferSize > 0) {
            size_t copySize = response.length();
            if (copySize >= bufferSize) {
                copySize = bufferSize - 1;
            }
            strncpy(responseJson, response.c_str(), copySize);
            responseJson[copySize] = '\0';
        }
        return 1;
    }
    
    if (errorMsg && errorMsgSize > 0) {
        std::string error = "HTTP " + std::to_string(httpResponse.statusCode);
        if (!httpResponse.error.empty()) {
            error += ": " + httpResponse.error;
        }
        if (!response.empty() && response.length() < 500) {
            error += " - " + response;
        }
        size_t copySize = error.length();
        if (copySize >= errorMsgSize) {
            copySize = errorMsgSize - 1;
        }
        strncpy(errorMsg, error.c_str(), copySize);
        errorMsg[copySize] = '\0';
    }
    
    return 0;
}

} // extern "C"

