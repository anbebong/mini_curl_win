// Example C: Sử dụng API Client
// Compile với: gcc -o example_api_simple.exe example_api_simple.c api_client_c.cpp api_client.h winhttp_curl.cpp -lwinhttp -lstdc++

#include "api_client_c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 8192
#define ERROR_MSG_SIZE 512

int main() {
    printf("=== API Client Example (C) ===\n\n");
    
    // Tạo ApiClient
    ApiClientHandle client = api_client_create("https://uat.lab.linksafe.vn/pam-api", 1);
    if (!client) {
        printf("Failed to create API client!\n");
        return 1;
    }
    
    // Khởi tạo client
    if (!api_client_init(client)) {
        printf("Failed to initialize API client!\n");
        api_client_destroy(client);
        return 1;
    }
    
    printf("API Client initialized successfully.\n\n");
    
    // // Example 1: Lấy config
    // printf("[1] Lay config tu server...\n");
    // char configJson[BUFFER_SIZE];
    // if (api_client_get_config(client, configJson, BUFFER_SIZE)) {
    //     printf("Success: Config retrieved successfully!\n");
    //     printf("Config length: %zu bytes\n", strlen(configJson));
        
    //     // Lưu config vào file
    //     FILE* configFile = fopen("agent-config.json", "w");
    //     if (configFile) {
    //         fprintf(configFile, "%s", configJson);
    //         fclose(configFile);
    //         printf("Success: Config saved to agent-config.json\n");
    //     }
    // } else {
    //     printf("Error: Failed to get config\n");
    // }
    
    // printf("\n");
    
    // // Example 4: Download file bằng service name (GET với query parameters)
    // printf("[4] Download file bang service name (GET /api/download?service_name=LANCS-ZTNA&os=win64)...\n");
    // char errorMsg4[ERROR_MSG_SIZE];
    // // Không chỉ định outputDir -> mặc định lưu vào downloads/ trong thư mục chứa file thực thi
    // if (api_client_download_file_by_service_name(client, "LANCS-ZTNA", "win64", "", "", 
    //                                               errorMsg4, ERROR_MSG_SIZE)) {
    //     printf("Success: File downloaded successfully (ten file tu server)\n");
    // } else {
    //     printf("Error: Failed to download file by service name\n");
    //     if (strlen(errorMsg4) > 0) {
    //         printf("  Details: %s\n", errorMsg4);
    //     }
    // }
    
    printf("\n");
    
    // Example 5: Lấy danh sách applications (GET /app/agent)
    printf("[5] Lay danh sach applications (GET /app/agent)...\n");
    printf("Note: Can X-Auth-Token header. Neu chua co token, se bao loi 401.\n");
    
    // Thay YOUR_AUTH_TOKEN bằng token thực tế của bạn
    const char* authToken = "YOUR_AUTH_TOKEN_HERE";
    char appsJson[BUFFER_SIZE];
    char errorMsg5[ERROR_MSG_SIZE];
    
    if (api_client_get_applications(client, authToken, appsJson, BUFFER_SIZE, errorMsg5, ERROR_MSG_SIZE)) {
        printf("Success: Applications retrieved successfully!\n");
        printf("Response length: %zu bytes\n", strlen(appsJson));
        
        // Lưu response vào file để xem
        FILE* appsFile = fopen("applications.json", "w");
        if (appsFile) {
            fprintf(appsFile, "%s", appsJson);
            fclose(appsFile);
            printf("Success: Response saved to applications.json\n");
        }
        
        // In một phần response để xem (nếu quá dài)
        if (strlen(appsJson) > 500) {
            printf("Response preview (first 500 chars):\n%.500s...\n", appsJson);
        } else {
            printf("Response:\n%s\n", appsJson);
        }
    } else {
        printf("Error: Failed to get applications\n");
        if (strlen(errorMsg5) > 0) {
            printf("  Details: %s\n", errorMsg5);
        }
        printf("  Note: Co the do chua co auth token hoac token khong hop le.\n");
    }
    
    printf("\n");
    printf("=== Done ===\n");
    
    // Cleanup
    api_client_cleanup(client);
    api_client_destroy(client);
    
    return 0;
}

