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
#include <ctime>
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
static volatile sig_atomic_t s_signo = 0;

// Persistent strings for callback safety
static std::string s_access_token;
static std::string s_refresh_token;
static std::string s_id_token;
static std::string s_email;
static std::string s_name;
static std::string s_username;
static std::string s_error_msg;
static std::string s_error_desc;

/* Đợi browser nhận xong HTML (MG_EV_CLOSE) rồi mới gọi token callback + stop server */
static struct mg_connection *s_close_watch_conn = NULL;
static int s_deliver_token_on_close = 0;
static int s_stop_on_close = 0;
static time_t s_close_watch_started = 0;

static void oidc_schedule_close_action(struct mg_connection *c, int deliver_token) {
    s_close_watch_conn = c;
    s_deliver_token_on_close = deliver_token ? 1 : 0;
    s_stop_on_close = 1;
    s_close_watch_started = time(NULL);
    printf("DEBUG: Waiting for browser to close connection (deliver_token=%d)\n", deliver_token);
}

static void oidc_handle_connection_close(struct mg_connection *c) {
    if (!s_stop_on_close || c != s_close_watch_conn) {
        return;
    }

    printf("DEBUG: OIDC callback connection closed (ev=CLOSE)\n");
    s_stop_on_close = 0;
    s_close_watch_conn = NULL;

    if (s_deliver_token_on_close) {
        s_deliver_token_on_close = 0;
        MiniHttpServerTokenCallback token_cb = NULL;
        void *user_data = NULL;
        lock_callback_mutex();
        token_cb = s_token_callback;
        user_data = s_user_data;
        unlock_callback_mutex();

        if (token_cb) {
            printf("DEBUG: About to call callback with user_data=%p\n", user_data);
            // #region agent log
            {
                std::ostringstream oss;
                oss << "{\"user_data_ptr\":" << (void*)user_data << ",\"callback_ptr\":" << (void*)token_cb
                    << ",\"has_access_token\":" << (!s_access_token.empty()) << ",\"mutex_locked\":false}";
                // write_debug_log("mini_http_server_c.cpp:181", "About to call token callback after CLOSE", oss.str().c_str());
            }
            // #endregion
            token_cb(s_access_token.c_str(), s_refresh_token.c_str(), s_id_token.c_str(),
                     s_email.c_str(), s_name.c_str(), s_username.c_str(), user_data);
            // #region agent log
            // write_debug_log("mini_http_server_c.cpp:219", "Token callback returned", "{}");
            // #endregion
            printf("DEBUG: Callback returned\n");
        }
    }

    printf("DEBUG: Stopping server after connection close\n");
    MiniHttpServer_Stop();
}

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

    if (!cpp_config.code_verifier.empty()) {
        c_config->code_verifier = (char*)malloc(cpp_config.code_verifier.length() + 1);
        strcpy(c_config->code_verifier, cpp_config.code_verifier.c_str());
    } else {
        c_config->code_verifier = NULL;
    }
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
    cpp_config.code_verifier = c_config->code_verifier ? c_config->code_verifier : "";
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

static const char OIDC_LOGO_SVG[] = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="23.13 24.43 159.35 25.03" role="img" aria-label="LinkSafe"><defs><clipPath clipPathUnits="userSpaceOnUse" id="d"><path d="m1267.554 356.294 2.134-3.535-2.148 3.51-8.008-13.869 26.175-43.36c2.236-3.706 6.326-5.963 10.727-5.922l9.203.088zm-.015-.024.001-.001.014.025z"/></clipPath><clipPath clipPathUnits="userSpaceOnUse" id="b"><path d="m1249.899 319.359 12.266-20.319c2.237-3.706 6.327-5.963 10.728-5.922l9.202.088-24.137 39.986z"/></clipPath><clipPath clipPathUnits="userSpaceOnUse" id="a"><path d="M0 1206h1800V0H0z"/></clipPath><linearGradient x1="0" y1="0" x2="1" y2="0" gradientUnits="userSpaceOnUse" gradientTransform="scale(62.89573 -62.89573) rotate(-89.447 7.84 -12.624)" spreadMethod="pad" id="e"><stop style="stop-opacity:1;stop-color:#4cbdec" offset="0"/><stop style="stop-opacity:1;stop-color:#4674b9" offset="1"/></linearGradient><linearGradient x1="0" y1="0" x2="1" y2="0" gradientUnits="userSpaceOnUse" gradientTransform="matrix(0 40.07375 40.07375 0 1265.997 293.118)" spreadMethod="pad" id="c"><stop style="stop-opacity:1;stop-color:#4cbdec" offset="0"/><stop style="stop-opacity:1;stop-color:#4674b9" offset="1"/></linearGradient></defs><g clip-path="url(#a)" transform="matrix(.35278 0 0 -.35278 -315.425 152.85)"><path d="M0 0v-17.732h23.812V-28.98H0v-18.745h17.132c5.368 0 9.72-4.352 9.72-9.72v-1.832h-41.038v70.828h41.038V9.72c0-5.368-4.352-9.72-9.72-9.72z" style="fill:#203264;fill-opacity:1;fill-rule:nonzero;stroke:none" transform="translate(1384.517 352.419)"/><path d="M0 0v-60.735h-14.196v70.778h4.153C-4.496 10.043 0 5.546 0 0" style="fill:#203264;fill-opacity:1;fill-rule:nonzero;stroke:none" transform="translate(1020.465 353.928)"/><path d="M0 0h-14.369l-32.536 48.571V0h-14.37v70.879h6.645c4.823 0 9.324-2.392 11.982-6.369l28.279-42.303v48.672h2.392C-5.362 70.879 0 65.581 0 59.046z" style="fill:#203264;fill-opacity:1;fill-rule:nonzero;stroke:none" transform="translate(1092.265 293.091)"/><path d="m0 0-22.203 27.186V-4.35h-14.196v70.778h2.357c6.538 0 11.839-5.301 11.839-11.839v-19.9L.003 62.054a11.826 11.826 0 0 0 9.182 4.374h11.504L-8.514 31.343 21.5-4.35H9.169A11.838 11.838 0 0 0 0 0" style="fill:#203264;fill-opacity:1;fill-rule:nonzero;stroke:none" transform="translate(1139.316 297.543)"/><path d="M0 0c-3.83 1.636-6.859 3.993-9.084 7.071-2.227 3.076-3.373 6.71-3.437 10.901h8.763c2.973 0 5.722-1.631 7.098-4.266a8.63 8.63 0 0 1 1.865-2.412c1.8-1.637 4.272-2.455 7.415-2.455 3.207 0 5.728.768 7.562 2.308 1.832 1.537 2.749 3.55 2.749 6.039 0 2.029-.623 3.698-1.865 5.009-1.245 1.309-2.8 2.34-4.665 3.093-1.866.752-4.437 1.587-7.71 2.505-4.453 1.308-8.07 2.602-10.852 3.879-2.783 1.276-5.172 3.207-7.169 5.794-1.998 2.586-2.995 6.039-2.995 10.361 0 4.058 1.014 7.594 3.044 10.606 2.029 3.011 4.877 5.318 8.544 6.923 3.666 1.604 7.857 2.407 12.571 2.407 7.071 0 12.816-1.719 17.235-5.156 4.419-3.438 6.858-8.234 7.317-14.388h-8.697c-3.145 0-6.024 1.605-7.846 4.169a8.758 8.758 0 0 1-1.577 1.675c-1.865 1.537-4.338 2.308-7.414 2.308-2.685 0-4.829-.688-6.433-2.063-1.605-1.375-2.406-3.373-2.406-5.99 0-1.834.605-3.356 1.817-4.567 1.211-1.212 2.716-2.194 4.518-2.946 1.8-.753 4.336-1.62 7.611-2.603 4.451-1.31 8.085-2.619 10.9-3.928 2.815-1.31 5.238-3.275 7.268-5.892 2.029-2.62 3.044-6.057 3.044-10.312 0-3.667-.95-7.071-2.848-10.213-1.899-3.143-4.681-5.647-8.347-7.513-3.668-1.866-8.021-2.799-13.062-2.799C8.134-2.455 3.83-1.637 0 0" style="fill:#203264;fill-opacity:1;fill-rule:nonzero;stroke:none" transform="translate(1180.316 296)"/><path d="M0 0v-1.699a9.759 9.759 0 0 0-9.759-9.759h-19.748v-18.151h22.612v-1.657c0-5.301-4.297-9.598-9.599-9.598h-13.013v-29.914h-14.196V0z" style="fill:#203264;fill-opacity:1;fill-rule:nonzero;stroke:none" transform="translate(1359.534 363.97)"/><path d="M0 0h-21.568v48.084c0 6.317-5.12 11.438-11.438 11.438h-2.757v-70.777H0z" style="fill:#203264;fill-opacity:1;fill-rule:nonzero;stroke:none" transform="translate(995.49 304.448)"/><path d="M0 0a6.841 6.841 0 1 0-13.682 0A6.841 6.841 0 0 0 0 0" style="fill:#4cbdec;fill-opacity:1;fill-rule:nonzero;stroke:none" transform="translate(1256.74 300.034)"/></g><g clip-path="url(#b)" transform="matrix(.35278 0 0 -.35278 -315.425 152.85)"><path d="m1249.899 319.359 12.266-20.319c2.237-3.706 6.327-5.963 10.728-5.922l9.202.088-24.137 39.986z" style="fill:url(#c);stroke:none"/></g><g clip-path="url(#d)" transform="matrix(.35278 0 0 -.35278 -315.425 152.85)"><path d="m1267.554 356.294 2.134-3.535-2.148 3.51-8.008-13.869 26.175-43.36c2.236-3.706 6.326-5.963 10.727-5.922l9.203.088zm-.015-.024.001-.001.014.025z" style="fill:url(#e);stroke:none"/></g><path d="m0 0 7.406.008c3.794.004 7.299 2 9.199 5.238l31.952 54.45-6.572 10.948z" style="fill:#4cbdec;fill-opacity:1;fill-rule:nonzero;stroke:none" transform="matrix(.35278 0 0 -.35278 115.35 49.445)"/></svg>)SVG";

static const char OIDC_HTML_LOGO_OPEN[] = "<div class=\"logo\">";
static const char OIDC_HTML_LOGO_CLOSE[] = "</div>";

static const char OIDC_HTML_HEAD[] =
    "<!DOCTYPE html><html lang=\"vi\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>LancsMaster</title><style>"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{min-height:100vh;display:flex;align-items:center;justify-content:center;"
    "font-family:Segoe UI,system-ui,-apple-system,sans-serif;"
    "background:linear-gradient(145deg,#eef4ff 0%%,#f8fafc 45%%,#e8f0fe 100%%);"
    "color:#1e293b;padding:24px}"
    ".card{max-width:420px;width:100%%;background:#fff;border-radius:16px;"
    "box-shadow:0 10px 40px rgba(15,23,42,.10);padding:36px 32px 32px;text-align:center}"
    ".logo{width:210px;max-width:88%%;margin:0 auto 22px}"
    ".logo svg{width:100%%;height:auto;display:block}"
    "h1{font-size:1.42rem;font-weight:700;margin-bottom:8px;color:#0f172a}"
    ".en{font-size:1.02rem;color:#375a7f;margin-bottom:16px}"
    ".msg{font-size:.98rem;color:#0b3d6f;background:#e8f4ff;border:1px solid #cfe7ff;"
    "border-radius:10px;padding:12px 14px;margin:16px 0 20px;word-break:break-word}"
    ".hint{font-size:.92rem;color:#375a7f;line-height:1.55}"
    ".brand{margin-top:24px;font-size:.75rem;color:#94a3b8;letter-spacing:.04em}"
    "</style></head><body><div class=\"card\">";

static const char OIDC_HTML_SUCCESS_PRE[] =
    "<!DOCTYPE html><html lang=\"vi\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>&#272;&#259;ng nh&#7853;p th&#224;nh c&#244;ng</title><style>"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{min-height:100vh;display:flex;align-items:center;justify-content:center;"
    "font-family:Segoe UI,system-ui,-apple-system,sans-serif;"
    "background:linear-gradient(145deg,#eef4ff 0%%,#f8fafc 45%%,#e8f0fe 100%%);"
    "color:#1e293b;padding:24px}"
    ".card{max-width:420px;width:100%%;background:#fff;border-radius:16px;"
    "box-shadow:0 10px 40px rgba(15,23,42,.10);padding:36px 32px 32px;text-align:center}"
    ".logo{width:210px;max-width:88%%;margin:0 auto 22px}"
    ".logo svg{width:100%%;height:auto;display:block}"
    "h1{font-size:1.42rem;font-weight:700;margin-bottom:8px;color:#0f172a}"
    ".en{font-size:1.02rem;color:#375a7f;margin-bottom:20px}"
    ".hint{font-size:.92rem;color:#375a7f;line-height:1.55;background:#e8f4ff;"
    "border:1px solid #cfe7ff;border-radius:10px;padding:14px 16px}"
    ".brand{margin-top:24px;font-size:.75rem;color:#94a3b8;letter-spacing:.04em}"
    "</style></head><body><div class=\"card\">";

static const char OIDC_HTML_SUCCESS_POST[] =
    "<h1>&#272;&#259;ng nh&#7853;p th&#224;nh c&#244;ng!</h1>"
    "<p class=\"en\">Login successful</p>"
    "<p class=\"hint\">B&#7841;n c&#243; th&#7875; &#273;&#243;ng tab n&#224;y v&#224; quay l&#7841;i &#7913;ng d&#7909;ng LancsMaster.<br>"
    "You can close this tab and return to the application.</p>"
    "<p class=\"brand\">Lancs Networks</p>"
    "</div></body></html>";

static const char OIDC_HTML_FOOT[] = "</div></body></html>";

// OIDC callback handler
static void oidc_callback_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_CLOSE) {
        oidc_handle_connection_close(c);
        return;
    }

    if (ev != MG_EV_HTTP_MSG) {
        return;
    }

    printf("DEBUG: Entered oidc_callback_handler, ev=%d (HTTP_MSG)\n", ev);
    // #region agent log
    // write_debug_log("mini_http_server_c.cpp:122", "oidc_callback_handler entry", "{\"ev\":0}");
    // #endregion

    lock_callback_mutex();
    // #region agent log
    // write_debug_log("mini_http_server_c.cpp:127", "callback_mutex locked", "{}");
    // #endregion

    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    printf("DEBUG: Received HTTP request: %.*s\n", (int)hm->uri.len, hm->uri.buf);

    if (mg_match(hm->uri, mg_str("/callback"), NULL)) {
        printf("DEBUG: Matched /callback\n");

        char code[512] = {0};
        char error[512] = {0};
        char error_description[512] = {0};

        mg_http_get_var(&hm->query, "code", code, sizeof(code));
        mg_http_get_var(&hm->query, "error", error, sizeof(error));
        mg_http_get_var(&hm->query, "error_description", error_description, sizeof(error_description));

        printf("DEBUG: code=%s, error=%s\n", code, error);

        MiniHttpServerTokenCallback token_cb = s_token_callback;
        MiniHttpServerErrorCallback error_cb = s_error_callback;
        void *user_data = s_user_data;

        if (strlen(error) > 0) {
            printf("DEBUG: Error in callback: %s\n", error);
            if (error_cb) {
                error_cb(error,
                    strlen(error_description) > 0 ? error_description : NULL,
                    user_data);
            }

            mg_http_reply(c, 200, "Content-Type: text/html; charset=utf-8\r\nConnection: close\r\n",
                "%s%s%s%s"
                "<h1>L&#7895;i &#273;&#259;ng nh&#7853;p</h1>"
                "<p class=\"en\">Login error</p>"
                "<p class=\"msg\">%s</p>"
                "<p class=\"hint\">Vui l&#242;ng th&#7917; l&#7841;i t&#7915; &#7913;ng d&#7909;ng.<br>Please try again from the application.</p>"
                "<p class=\"brand\">LancsMaster &middot; AppCI</p>%s",
                OIDC_HTML_HEAD, OIDC_HTML_LOGO_OPEN, OIDC_LOGO_SVG, OIDC_HTML_LOGO_CLOSE,
                error, OIDC_HTML_FOOT);
            oidc_schedule_close_action(c, 0);
        } else if (strlen(code) > 0) {
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
                    cpp_config.verify_ssl,
                    cpp_config.code_verifier
                );
                printf("DEBUG: ExchangeOidcToken returned\n");

                if (response.success) {
                    printf("DEBUG: Token exchange successful\n");
                    if (cpp_config.save_token && !cpp_config.token_file.empty()) {
                        printf("DEBUG: Saving token to file\n");
                        SaveTokenToFile(response, cpp_config.token_file);
                    }

                    if (token_cb) {
                        printf("DEBUG: About to assign strings\n");
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
                    }

                    /* Trả HTML ngay; token callback + stop khi browser đóng kết nối (MG_EV_CLOSE) */
                    printf("DEBUG: Sending success HTML to browser\n");
                    mg_http_reply(c, 200,
                        "Content-Type: text/html; charset=utf-8\r\nConnection: close\r\n",
                        "%s%s%s%s%s",
                        OIDC_HTML_SUCCESS_PRE, OIDC_HTML_LOGO_OPEN, OIDC_LOGO_SVG,
                        OIDC_HTML_LOGO_CLOSE, OIDC_HTML_SUCCESS_POST);
                    oidc_schedule_close_action(c, token_cb ? 1 : 0);
                    (void)user_data;
                } else {
                    printf("DEBUG: Token exchange failed: %s\n", response.error.c_str());
                    if (error_cb) {
                        s_error_msg = response.error;
                        s_error_desc = response.error_description;
                        error_cb(s_error_msg.c_str(), s_error_desc.c_str(), user_data);
                    }
                    mg_http_reply(c, 400, "Content-Type: text/html; charset=utf-8\r\nConnection: close\r\n",
                        "%s%s%s%s"
                        "<h1>Kh&#244;ng &#273;&#7893;i &#273;&#432;&#7907;c token</h1>"
                        "<p class=\"en\">Token exchange failed</p>"
                        "<p class=\"msg\">%s</p>"
                        "<p class=\"hint\">Vui l&#242;ng th&#7917; l&#7841;i t&#7915; &#7913;ng d&#7909;ng.<br>Please try again from the application.</p>"
                        "<p class=\"brand\">LancsMaster &middot; AppCI</p>%s",
                        OIDC_HTML_HEAD, OIDC_HTML_LOGO_OPEN, OIDC_LOGO_SVG, OIDC_HTML_LOGO_CLOSE,
                        response.error.c_str(), OIDC_HTML_FOOT);
                    oidc_schedule_close_action(c, 0);
                }
            } else {
                printf("DEBUG: Config invalid\n");
                mg_http_reply(c, 200, "Content-Type: text/html; charset=utf-8\r\nConnection: close\r\n",
                    "%s%s%s%s"
                    "<h1>Ch&#432;a c&#7845;u h&#236;nh OIDC</h1>"
                    "<p class=\"en\">Token exchange not configured</p>"
                    "<p class=\"hint\">Thi&#7871;u token endpoint ho&#7863;c client ID.<br>Missing token endpoint or client ID.</p>"
                    "<p class=\"brand\">LancsMaster &middot; AppCI</p>%s",
                    OIDC_HTML_HEAD, OIDC_HTML_LOGO_OPEN, OIDC_LOGO_SVG, OIDC_HTML_LOGO_CLOSE,
                    OIDC_HTML_FOOT);
                oidc_schedule_close_action(c, 0);
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
    free(config->code_verifier);
    free(config);
}

bool MiniHttpServer_GeneratePkceS256(
    char* verifier_out,
    size_t verifier_out_size,
    char* challenge_out,
    size_t challenge_out_size
) {
    if (!verifier_out || !challenge_out || verifier_out_size < 44 || challenge_out_size < 44) {
        return false;
    }

    std::string verifier;
    std::string challenge;
    if (!GeneratePkceS256(verifier, challenge)) {
        return false;
    }
    if (verifier.size() >= verifier_out_size || challenge.size() >= challenge_out_size) {
        return false;
    }

    strcpy(verifier_out, verifier.c_str());
    strcpy(challenge_out, challenge.c_str());
    printf("DEBUG: Generated PKCE S256 verifier_len=%zu challenge_len=%zu\n",
           verifier.size(), challenge.size());
    return true;
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
    s_close_watch_conn = NULL;
    s_deliver_token_on_close = 0;
    s_stop_on_close = 0;
    
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
    printf("DEBUG: mini_http_server handler=close-after-html-v3\n");
    printf("Press Ctrl+C to stop...\n");
    
    while (s_signo == 0) {
        mg_mgr_poll(&mgr, 200);
        if (s_stop_on_close && s_close_watch_started > 0 &&
            (time(NULL) - s_close_watch_started) >= 10) {
            printf("DEBUG: Close watch timeout (10s), forcing token delivery and stop\n");
            if (s_close_watch_conn != NULL) {
                oidc_handle_connection_close(s_close_watch_conn);
            } else {
                MiniHttpServer_Stop();
            }
        }
    }
    
    printf("\nShutting down server...\n");
    mg_mgr_free(&mgr);
    
    return 0;
}

int MiniHttpServer_Stop(void) {
    s_close_watch_conn = NULL;
    s_deliver_token_on_close = 0;
    s_stop_on_close = 0;
    s_signo = (sig_atomic_t)SIGTERM;
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

