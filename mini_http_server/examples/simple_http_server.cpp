// Simple HTTP Server Example sử dụng Mongoose
//
// Đây là một ví dụ đơn giản về cách tạo HTTP server với Mongoose
// Không có OIDC, chỉ là một HTTP server cơ bản
//
// Build: cmake --build build
// Run: .\build\bin\simple_http_server.exe
//
// Server sẽ lắng nghe trên http://localhost:8080

#include <signal.h>
#include <cstdio>
#include <cstring>
#include "mongoose.h"

static int s_signo = 0;

// Signal handler để dừng server gracefully
static void signal_handler(int signo) {
    s_signo = signo;
}

// HTTP request handler
static void http_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        
        // Xử lý root endpoint
        if (mg_match(hm->uri, mg_str("/"), NULL)) {
            mg_http_reply(c, 200, "Content-Type: text/html\r\n",
                "<!DOCTYPE html>\n"
                "<html><head><title>Simple HTTP Server</title></head>\n"
                "<body>\n"
                "<h1>Welcome to Simple HTTP Server</h1>\n"
                "<p>This is a simple HTTP server example using Mongoose.</p>\n"
                "<ul>\n"
                "<li><a href=\"/hello\">Hello endpoint</a></li>\n"
                "<li><a href=\"/api/info\">API Info</a></li>\n"
                "<li><a href=\"/health\">Health Check</a></li>\n"
                "</ul>\n"
                "</body></html>\n");
        }
        // Xử lý /hello endpoint
        else if (mg_match(hm->uri, mg_str("/hello"), NULL)) {
            mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                "{\"message\": \"Hello, World!\", \"status\": \"ok\"}\n");
        }
        // Xử lý /api/info endpoint
        else if (mg_match(hm->uri, mg_str("/api/info"), NULL)) {
            mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                "{\n"
                "  \"server\": \"Simple HTTP Server\",\n"
                "  \"version\": \"1.0.0\",\n"
                "  \"framework\": \"Mongoose\",\n"
                "  \"status\": \"running\"\n"
                "}\n");
        }
        // Xử lý /health endpoint
        else if (mg_match(hm->uri, mg_str("/health"), NULL)) {
            mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                "{\"status\": \"healthy\"}\n");
        }
        // 404 Not Found
        else {
            mg_http_reply(c, 404, "Content-Type: text/html\r\n",
                "<!DOCTYPE html>\n"
                "<html><head><title>404 Not Found</title></head>\n"
                "<body>\n"
                "<h1>404 Not Found</h1>\n"
                "<p>The requested resource was not found.</p>\n"
                "<a href=\"/\">Go to home</a>\n"
                "</body></html>\n");
        }
    }
}

int main(int argc, char *argv[]) {
    struct mg_mgr mgr;
    struct mg_connection *c;
    
    // Parse listening address từ command line (mặc định: http://localhost:8080)
    const char *listening_addr = (argc > 1) ? argv[1] : "http://localhost:8080";
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize Mongoose event manager
    mg_mgr_init(&mgr);
    
    // Create listening connection
    c = mg_http_listen(&mgr, listening_addr, http_handler, NULL);
    if (c == NULL) {
        fprintf(stderr, "Error: Cannot start server on %s\n", listening_addr);
        return 1;
    }
    
    printf("Simple HTTP Server started\n");
    printf("Listening on: %s\n", listening_addr);
    printf("\nAvailable endpoints:\n");
    printf("  GET /              - Home page\n");
    printf("  GET /hello         - Hello JSON response\n");
    printf("  GET /api/info      - Server info (JSON)\n");
    printf("  GET /health        - Health check (JSON)\n");
    printf("\nPress Ctrl+C to stop...\n\n");
    
    // Event loop
    while (s_signo == 0) {
        mg_mgr_poll(&mgr, 1000);  // 1 second timeout
    }
    
    // Cleanup
    printf("\nShutting down server...\n");
    mg_mgr_free(&mgr);
    printf("Server stopped.\n");
    
    return 0;
}

