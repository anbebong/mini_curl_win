// Mini HTTP Server Library - C Wrapper Implementation
//
// Wrapper để expose C++ functionality qua C API

#include "../include/mini_http_server.h"
#include "../include/oidc_config.h"
#include "../include/oidc_token_exchange.h"
#include "mongoose.h"
#include <signal.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// Global variables cho callbacks
static MiniHttpServerTokenCallback s_token_callback = NULL;
static MiniHttpServerErrorCallback s_error_callback = NULL;
static void* s_user_data = NULL;
static int s_signo = 0;

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

// OIDC callback handler
static void oidc_callback_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        
        if (mg_match(hm->uri, mg_str("/callback"), NULL)) {
            char code[512] = {0};
            char error[512] = {0};
            char error_description[512] = {0};
            
            mg_http_get_var(&hm->query, "code", code, sizeof(code));
            mg_http_get_var(&hm->query, "error", error, sizeof(error));
            mg_http_get_var(&hm->query, "error_description", error_description, sizeof(error_description));
            
            if (strlen(error) > 0) {
                // Có lỗi
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
                OidcConfig cpp_config;
                ConvertConfig((MiniHttpServerConfig*)s_user_data, cpp_config);
                
                if (!cpp_config.token_endpoint.empty() && !cpp_config.client_id.empty()) {
                    OidcTokenResponse response = ExchangeOidcToken(
                        cpp_config.token_endpoint,
                        std::string(code),
                        cpp_config.redirect_uri,
                        cpp_config.client_id,
                        cpp_config.client_secret,
                        cpp_config.verify_ssl
                    );
                    
                    if (response.success) {
                        // Save token nếu được cấu hình
                        if (cpp_config.save_token && !cpp_config.token_file.empty()) {
                            SaveTokenToFile(response, cpp_config.token_file);
                        }
                        
                        // Gọi callback
                        if (s_token_callback) {
                            s_token_callback(
                                response.access_token.c_str(),
                                response.refresh_token.empty() ? NULL : response.refresh_token.c_str(),
                                response.id_token.empty() ? NULL : response.id_token.c_str(),
                                response.email.empty() ? NULL : response.email.c_str(),
                                response.name.empty() ? NULL : response.name.c_str(),
                                response.username.empty() ? NULL : response.username.c_str(),
                                s_user_data
                            );
                        }
                        
                        mg_http_reply(c, 200, "Content-Type: text/html\r\n",
                            "<!DOCTYPE html>\n"
                            "<html><head><title>Success</title></head>\n"
                            "<body><h1>Login successful</h1><p>You can close this window.</p></body></html>\n");
                    } else {
                        if (s_error_callback) {
                            s_error_callback(response.error.c_str(),
                                response.error_description.empty() ? NULL : response.error_description.c_str(),
                                s_user_data);
                        }
                        
                        mg_http_reply(c, 200, "Content-Type: text/html\r\n",
                            "<!DOCTYPE html>\n"
                            "<html><head><title>Error</title></head>\n"
                            "<body><h1>Error: %s</h1></body></html>\n",
                            response.error.c_str());
                    }
                } else {
                    mg_http_reply(c, 200, "Content-Type: text/html\r\n",
                        "<!DOCTYPE html>\n"
                        "<html><head><title>Not Configured</title></head>\n"
                        "<body><h1>Token exchange not configured</h1></body></html>\n");
                }
            }
        } else if (mg_match(hm->uri, mg_str("/health"), NULL)) {
            mg_http_reply(c, 200, "Content-Type: application/json\r\n",
                "{\"status\":\"ok\"}\n");
        } else {
            mg_http_reply(c, 200, "Content-Type: text/html\r\n",
                "<!DOCTYPE html>\n"
                "<html><head><title>Server</title></head>\n"
                "<body><h1>Server is running</h1></body></html>\n");
        }
    }
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
    
    s_token_callback = token_callback;
    s_error_callback = error_callback;
    s_user_data = (void*)config;  // Store config pointer
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

