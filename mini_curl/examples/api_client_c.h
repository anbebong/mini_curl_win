#ifndef API_CLIENT_C_H
#define API_CLIENT_C_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// C API wrapper cho ApiClient
typedef void* ApiClientHandle;

// Tạo ApiClient instance
ApiClientHandle api_client_create(const char* baseUrl, int verifySSL);

// Giải phóng ApiClient
void api_client_destroy(ApiClientHandle handle);

// Khởi tạo client
int api_client_init(ApiClientHandle handle);

// Cleanup client
void api_client_cleanup(ApiClientHandle handle);

// GET /api/config
int api_client_get_config(ApiClientHandle handle, char* configJson, size_t bufferSize);

// GET /api/download?id={service_id}
int api_client_download_file_by_id(ApiClientHandle handle, int serviceId, const char* outputPath, char* errorMsg, size_t errorMsgSize);

// GET /api/download?file_path={path}
int api_client_download_file_by_path(ApiClientHandle handle, const char* filePath, const char* outputPath, char* errorMsg, size_t errorMsgSize);

// GET /api/download?service_name={name}&os={os}&version={version}
int api_client_download_file_by_service_name(ApiClientHandle handle, 
                                             const char* serviceName, 
                                             const char* os, 
                                             const char* version,
                                             const char* outputDir,
                                             char* errorMsg, 
                                             size_t errorMsgSize);

// GET /app/agent - Lấy danh sách applications (cần X-Auth-Token header)
int api_client_get_applications(ApiClientHandle handle, 
                                 const char* authToken,
                                 char* responseJson, 
                                 size_t bufferSize,
                                 char* errorMsg, 
                                 size_t errorMsgSize);

#ifdef __cplusplus
}
#endif

#endif // API_CLIENT_C_H

