// Simple HTTP Server Example - C
//
// Ví dụ đơn giản về cách sử dụng mini_http_server library từ C code
//
// Build: cmake --build build
// Run: .\build\bin\example_simple.exe

#include <stdio.h>
#include <stdlib.h>
#include "mini_http_server.h"

int main(int argc, char *argv[]) {
    const char* listening_addr = (argc > 1) ? argv[1] : "http://localhost:8080";
    
    printf("Starting simple HTTP server...\n");
    printf("Listening on: %s\n", listening_addr);
    printf("\nAvailable endpoints:\n");
    printf("  GET /       - Home page\n");
    printf("  GET /health  - Health check\n");
    printf("\nPress Ctrl+C to stop...\n\n");
    
    // Start simple HTTP server (không có OIDC)
    int result = MiniHttpServer_StartSimple(listening_addr, NULL);
    
    if (result != 0) {
        fprintf(stderr, "Failed to start server\n");
        return 1;
    }
    
    return 0;
}

