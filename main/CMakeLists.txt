idf_component_register(SRCS "wifi_app.c" "ws2812_api.c" "colors.c" "http_server.c" "app_nvs.c" "main.c"
                    INCLUDE_DIRS "."
                    EMBED_FILES web_page/app.css web_page/app.js web_page/favicon.ico web_page/index.html web_page/jquery-3.6.1.min.js)