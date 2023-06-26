idf_component_register(SRCS download_file.c
                       INCLUDE_DIRS include
                       REQUIRES
                            esp_http_client     # esp_http_client.h included in the public header
                       PRIV_REQUIRES
                            esp_ringbuf         # uses a ringbuffer
                            mbedtls             # for certificate bundle
                            esp_timer           # for benchmarking
                      )
