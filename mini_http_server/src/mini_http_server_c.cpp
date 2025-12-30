// Mini HTTP Server Library - C Wrapper Implementation
//
// Wrapper để expose C++ functionality qua C API

#include "../include/mini_http_server.h"
#include "../include/oidc_config.h"
#include "../include/oidc_token_exchange.h"
#include "mongoose.h"
#ifndef _WIN32
// Chỉ include curl trên Linux, Windows không cần
#include <curl/curl.h>
#endif
#include <signal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fstream>
#include <chrono>
#include <sstream>

// Đảm bảo C++11 được bật
#if !defined(__cplusplus) || __cplusplus < 201103L
#error "This file requires C++11 or later. Please enable C++11 in your compiler settings."
#endif

// Mutex implementation - phân biệt Windows và Linux
#ifdef _WIN32
// Windows: Sử dụng CRITICAL_SECTION (tương thích tốt hơn với MinGW)
#include <windows.h>
static CRITICAL_SECTION callback_mutex;
static int callback_mutex_initialized = 0;

static void init_callback_mutex(void) {
    if (callback_mutex_initialized == 0) {
        InitializeCriticalSection(&callback_mutex);
        callback_mutex_initialized = 1;
    }
}

static void lock_callback_mutex(void) {
    if (callback_mutex_initialized == 0) {
        init_callback_mutex();
    }
    EnterCriticalSection(&callback_mutex);
}

static void unlock_callback_mutex(void) {
    if (callback_mutex_initialized != 0) {
        LeaveCriticalSection(&callback_mutex);
    }
}
#else
// Linux: Sử dụng std::mutex (C++11)
#include <thread>
#include <mutex>
static std::mutex callback_mutex;

static void lock_callback_mutex(void) {
    callback_mutex.lock();
}

static void unlock_callback_mutex(void) {
    callback_mutex.unlock();
}
#endif

// Global variables cho callbacks
static MiniHttpServerTokenCallback s_token_callback = NULL;
static MiniHttpServerErrorCallback s_error_callback = NULL;
static void* s_user_data = NULL;
static const MiniHttpServerConfig* s_config = NULL;
static int s_signo = 0;

// Persistent strings for callback safety
static std::string s_access_token;
static std::string s_refresh_token;
static std::string s_id_token;
static std::string s_email;
static std::string s_name;
static std::string s_username;
static std::string s_error_msg;
static std::string s_error_desc;

// Signal handler
static void signal_handler(int signo) {
    s_signo = signo;
}

// Convert C++ OidcConfig to C MiniHttpServerConfig
static void ConvertConfig(const OidcConfig& cpp_config, MiniHttpServerConfig* c_config) {
    // Allocate và copy strings
    if (!cpp_config.token_endpoint.empty()) {
        c_config->token_endpoint = (char*)malloc(cpp_config.token_endpoint.length() + 1);
        strcpy(c_config->token_endpoint, cpp_config.token_endpoint.c_str());
    } else {
        c_config->token_endpoint = NULL;
    }
    
    if (!cpp_config.redirect_uri.empty()) {
        c_config->redirect_uri = (char*)malloc(cpp_config.redirect_uri.length() + 1);
        strcpy(c_config->redirect_uri, cpp_config.redirect_uri.c_str());
    } else {
        c_config->redirect_uri = NULL;
    }
    
    if (!cpp_config.client_id.empty()) {
        c_config->client_id = (char*)malloc(cpp_config.client_id.length() + 1);
        strcpy(c_config->client_id, cpp_config.client_id.c_str());
    } else {
        c_config->client_id = NULL;
    }
    
    if (!cpp_config.client_secret.empty()) {
        c_config->client_secret = (char*)malloc(cpp_config.client_secret.length() + 1);
        strcpy(c_config->client_secret, cpp_config.client_secret.c_str());
    } else {
        c_config->client_secret = NULL;
    }
    
    if (!cpp_config.token_file.empty()) {
        c_config->token_file = (char*)malloc(cpp_config.token_file.length() + 1);
        strcpy(c_config->token_file, cpp_config.token_file.c_str());
    } else {
        c_config->token_file = NULL;
    }
    
    if (!cpp_config.listening_addr.empty()) {
        c_config->listening_addr = (char*)malloc(cpp_config.listening_addr.length() + 1);
        strcpy(c_config->listening_addr, cpp_config.listening_addr.c_str());
    } else {
        c_config->listening_addr = NULL;
    }
    
    c_config->verify_ssl = cpp_config.verify_ssl;
    c_config->save_token = cpp_config.save_token;
}

// Convert C MiniHttpServerConfig to C++ OidcConfig
static void ConvertConfig(const MiniHttpServerConfig* c_config, OidcConfig& cpp_config) {
    cpp_config.token_endpoint = c_config->token_endpoint ? c_config->token_endpoint : "";
    cpp_config.redirect_uri = c_config->redirect_uri ? c_config->redirect_uri : "";
    cpp_config.client_id = c_config->client_id ? c_config->client_id : "";
    cpp_config.client_secret = c_config->client_secret ? c_config->client_secret : "";
    cpp_config.token_file = c_config->token_file ? c_config->token_file : "";
    cpp_config.listening_addr = c_config->listening_addr ? c_config->listening_addr : "http://localhost:8085";
    cpp_config.verify_ssl = c_config->verify_ssl;
    cpp_config.save_token = c_config->save_token;
}

// // Helper function to write debug log
// static void write_debug_log(const char* location, const char* message, const char* data_json) {
//     // #region agent log
//     try {
//         std::ofstream log_file("/tmp/debug.log", std::ios::app);
//         if (log_file.is_open()) {
//             auto now = std::chrono::system_clock::now();
//             auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
//             log_file << "{\"sessionId\":\"debug-session\",\"runId\":\"run1\",\"location\":\"" << location 
//                      << "\",\"message\":\"" << message << "\",\"data\":" << (data_json ? data_json : "{}") 
//                      << ",\"timestamp\":" << timestamp << "}\n";
//             log_file.close();
//         }
//     } catch (...) {}
//     // #endregion
// }

// COMMENTED OUT FOR TESTING - OIDC callback handler
static void oidc_callback_handler(struct mg_connection *c, int ev, void *ev_data) {
    printf("DEBUG: Entered oidc_callback_handler, ev=%d\n", ev);
    // #region agent log
    // write_debug_log("mini_http_server_c.cpp:122", "oidc_callback_handler entry", "{\"ev\":0}");
    // #endregion
    lock_callback_mutex();
    // #region agent log
    // write_debug_log("mini_http_server_c.cpp:127", "callback_mutex locked", "{}");
    // #endregion
    
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        
        printf("DEBUG: Received HTTP request: %.*s\n", (int)hm->uri.len, hm->uri.buf);
        
        // Temporarily comment out /callback handling
        if (mg_match(hm->uri, mg_str("/callback"), NULL)) {
            printf("DEBUG: Matched /callback\n");
            
            char code[512] = {0};
            char error[512] = {0};
            char error_description[512] = {0};
            
            mg_http_get_var(&hm->query, "code", code, sizeof(code));
            mg_http_get_var(&hm->query, "error", error, sizeof(error));
            mg_http_get_var(&hm->query, "error_description", error_description, sizeof(error_description));
            
            printf("DEBUG: code=%s, error=%s\n", code, error);
            
            if (strlen(error) > 0) {
                // Có lỗi
                printf("DEBUG: Error in callback: %s\n", error);
                if (s_error_callback) {
                    s_error_callback(error, 
                        strlen(error_description) > 0 ? error_description : NULL,
                        s_user_data);
                }
                
                mg_http_reply(c, 200, "Content-Type: text/html\r\n",
                    "<!DOCTYPE html>\n"
                    "<html><head><title>OIDC Error</title></head>\n"
                    "<body><h1>Error: %s</h1></body></html>\n", error);
            } else if (strlen(code) > 0) {
                // Có authorization code - exchange thành tokens
                printf("DEBUG: Calling ConvertConfig\n");
                OidcConfig cpp_config;
                ConvertConfig(s_config, cpp_config);
                printf("DEBUG: ConvertConfig done\n");
                
                if (!cpp_config.token_endpoint.empty() && !cpp_config.client_id.empty()) {
                    printf("DEBUG: Config valid, calling ExchangeOidcToken\n");
                    OidcTokenResponse response = ExchangeOidcToken(
                        cpp_config.token_endpoint,
                        std::string(code),
                        cpp_config.redirect_uri,
                        cpp_config.client_id,
                        cpp_config.client_secret,
                        cpp_config.verify_ssl
                    );
                    printf("DEBUG: ExchangeOidcToken returned\n");
                    
                    // Handle response
                    if (response.success) {
                        printf("DEBUG: Token exchange successful\n");
                        // Save token if configured
                        if (cpp_config.save_token && !cpp_config.token_file.empty()) {
                            printf("DEBUG: Saving token to file\n");
                            SaveTokenToFile(response, cpp_config.token_file);
                        }
                        // Call token callback
                        if (s_token_callback) {
                            printf("DEBUG: About to assign strings\n");
                            // Store strings persistently for callback safety
                            s_access_token = response.access_token;
                            printf("DEBUG: Assigned access_token\n");
                            s_refresh_token = response.refresh_token;
                            printf("DEBUG: Assigned refresh_token\n");
                            s_id_token = response.id_token;
                            printf("DEBUG: Assigned id_token\n");
                            s_email = response.email;
                            printf("DEBUG: Assigned email\n");
                            s_name = response.name;
                            printf("DEBUG: Assigned name\n");
                            s_username = response.username;
                            printf("DEBUG: Assigned username\n");
                            printf("DEBUG: About to call callback with user_data=%p\n", s_user_data);
                            // #region agent log
                            {
                                std::ostringstream oss;
                                oss << "{\"user_data_ptr\":" << (void*)s_user_data << ",\"callback_ptr\":" << (void*)s_token_callback 
                                    << ",\"has_access_token\":" << (!s_access_token.empty()) << ",\"mutex_locked\":true}";
                                // write_debug_log("mini_http_server_c.cpp:181", "About to call token callback (HYPOTHESIS A: deadlock risk)", oss.str().c_str());
                            }
                            // #endregion
                            // #region agent log
                            // write_debug_log("mini_http_server_c.cpp:216", "About to invoke s_token_callback", "{}");
                            // #endregion
                            s_token_callback(s_access_token.c_str(), s_refresh_token.c_str(), s_id_token.c_str(), 
                                           s_email.c_str(), s_name.c_str(), s_username.c_str(), s_user_data);
                            // #region agent log
                            // write_debug_log("mini_http_server_c.cpp:219", "Token callback returned (CRITICAL: if this log missing, segfault in callback)", "{}");
                            // #endregion
                            printf("DEBUG: Callback returned\n");
                        }
                        mg_http_reply(c, 200, "Content-Type: text/html\r\n",
                            "<!DOCTYPE html>\n"
                            "<html><head><title>Success</title></head>\n"
                            "<body><h1>Login successful!</h1><p>You can close this window.</p></body></html>\n");
                    } else {
                        printf("DEBUG: Token exchange failed: %s\n", response.error.c_str());
                        // Call error callback
                        if (s_error_callback) {
                            s_error_msg = response.error;
                            s_error_desc = response.error_description;
                            s_error_callback(s_error_msg.c_str(), s_error_desc.c_str(), s_user_data);
                        }
                        mg_http_reply(c, 400, "Content-Type: text/html\r\n",
                            "<!DOCTYPE html>\n"
                            "<html><head><title>Error</title></head>\n"
                            "<body><h1>Error: %s</h1><p>%s</p></body></html>\n", response.error.c_str(), response.error_description.c_str());
                    }
                } else {
                    printf("DEBUG: Config invalid\n");
                    mg_http_reply(c, 200, "Content-Type: text/html\r\n",
                        "<!DOCTYPE html>\n"
                        "<html><head><title>Not Configured</title></head>\n"
                        "<body><h1>Token exchange not configured</h1></body></html>\n");
                }
            }
        }
        else if (mg_match(hm->uri, mg_str("/health"), NULL)) {
            mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                "{\"status\":\"ok\"}\n");
        } else {
            mg_http_reply(c, 200, "Content-Type: text/html\r\n",
                "<!DOCTYPE html>\n"
                "<html><head><title>Server</title></head>\n"
                "<body><h1>Server is running</h1></body></html>\n");
        }
    }
    unlock_callback_mutex();
}

// Simple HTTP handler
static void simple_http_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        
        if (mg_match(hm->uri, mg_str("/"), NULL)) {
            mg_http_reply(c, 200, "Content-Type: text/html\r\n",
                "<!DOCTYPE html>\n"
                "<html><head><title>Simple Server</title></head>\n"
                "<body><h1>Simple HTTP Server</h1></body></html>\n");
        } else if (mg_match(hm->uri, mg_str("/health"), NULL)) {
            mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                "{\"status\":\"ok\"}\n");
        } else {
            mg_http_reply(c, 404, "Content-Type: text/html\r\n",
                "<!DOCTYPE html>\n"
                "<html><head><title>404</title></head>\n"
                "<body><h1>404 Not Found</h1></body></html>\n");
        }
    }
}

// C API Implementation

MiniHttpServerConfig* MiniHttpServerConfig_New(void) {
    MiniHttpServerConfig* config = (MiniHttpServerConfig*)calloc(1, sizeof(MiniHttpServerConfig));
    if (config) {
        config->verify_ssl = true;
        config->save_token = false;
        config->listening_addr = (char*)malloc(strlen("http://localhost:8085") + 1);
        strcpy(config->listening_addr, "http://localhost:8085");
    }
    return config;
}

bool MiniHttpServerConfig_LoadFromFile(MiniHttpServerConfig* config, const char* filename) {
    if (!config || !filename) return false;
    
    OidcConfig cpp_config;
    if (!LoadOidcConfigFromFile(filename, cpp_config)) {
        return false;
    }
    
    ConvertConfig(cpp_config, config);
    return true;
}

bool MiniHttpServerConfig_LoadFromParams(
    MiniHttpServerConfig* config,
    const char* token_endpoint,
    const char* redirect_uri,
    const char* client_id,
    const char* client_secret,
    const char* token_file,
    int verify_ssl,
    int save_token,
    const char* listening_addr
) {
    if (!config) return false;
    
    OidcConfig cpp_config;
    if (!LoadOidcConfigFromParams(
        token_endpoint ? token_endpoint : "",
        redirect_uri ? redirect_uri : "",
        client_id ? client_id : "",
        client_secret ? client_secret : "",
        token_file ? token_file : "",
        verify_ssl,
        save_token,
        listening_addr ? listening_addr : "",
        cpp_config
    )) {
        return false;
    }
    
    ConvertConfig(cpp_config, config);
    return true;
}

void MiniHttpServerConfig_Free(MiniHttpServerConfig* config) {
    if (!config) return;
    
    free(config->token_endpoint);
    free(config->redirect_uri);
    free(config->client_id);
    free(config->client_secret);
    free(config->token_file);
    free(config->listening_addr);
    free(config);
}

int MiniHttpServer_Start(
    const MiniHttpServerConfig* config,
    MiniHttpServerTokenCallback token_callback,
    MiniHttpServerErrorCallback error_callback,
    void* user_data
) {
    if (!config) return 1;
    
#ifndef _WIN32
    // Initialize libcurl global state in main thread (chỉ cần trên Linux)
    static bool curl_initialized = false;
    if (!curl_initialized) {
        printf("DEBUG: Initializing libcurl global state in main thread\n");
        if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
            fprintf(stderr, "Failed to initialize libcurl global state\n");
            return 1;
        }
        curl_initialized = true;
        printf("DEBUG: libcurl global state initialized\n");
    }
#endif
    
    lock_callback_mutex();
    s_token_callback = token_callback;
    s_error_callback = error_callback;
    s_config = config;
    s_user_data = user_data;
    unlock_callback_mutex();
    s_signo = 0;
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    
    struct mg_connection *c = mg_http_listen(&mgr, config->listening_addr, oidc_callback_handler, NULL);
    if (c == NULL) {
        fprintf(stderr, "Error: Cannot start server on %s\n", config->listening_addr);
        return 1;
    }
    
    printf("OIDC Callback Server started on %s\n", config->listening_addr);
    printf("Press Ctrl+C to stop...\n");
    
    while (s_signo == 0) {
        mg_mgr_poll(&mgr, 1000);
    }
    
    printf("\nShutting down server...\n");
    mg_mgr_free(&mgr);
    
    return 0;
}

int MiniHttpServer_Stop(void) {
    // Set signal để stop server loop
    s_signo = SIGTERM;
    return 0;
}

int MiniHttpServer_StartSimple(const char* listening_addr, void* user_data) {
    if (!listening_addr) return 1;
    
    s_signo = 0;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    
    struct mg_connection *c = mg_http_listen(&mgr, listening_addr, simple_http_handler, NULL);
    if (c == NULL) {
        fprintf(stderr, "Error: Cannot start server on %s\n", listening_addr);
        return 1;
    }
    
    printf("Simple HTTP Server started on %s\n", listening_addr);
    printf("Press Ctrl+C to stop...\n");
    
    while (s_signo == 0) {
        mg_mgr_poll(&mgr, 1000);
    }
    
    printf("\nShutting down server...\n");
    mg_mgr_free(&mgr);
    
    return 0;
}

