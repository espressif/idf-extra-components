/* CoAP server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
 * WARNING
 * libcoap is not multi-thread safe, so only this thread must make any coap_*()
 * calls.  Any external (to this thread) data transmitted in/out via libcoap
 * therefore has to be passed in/out by xQueue*() via this thread.
 */

#include <string.h>
#include <sys/socket.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include "nvs_flash.h"

#include "protocol_examples_common.h"

#include "coap3/coap.h"

#ifndef CONFIG_COAP_SERVER_SUPPORT
#error COAP_SERVER_SUPPORT needs to be enabled
#endif /* COAP_SERVER_SUPPORT */

/* The examples use simple Pre-Shared-Key configuration that you can set via
   'idf.py menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_COAP_PSK_KEY "some-agreed-preshared-key"

   Note: PSK will only be used if the URI is prefixed with coaps://
   instead of coap:// and the PSK must be one that the server supports
   (potentially associated with the IDENTITY)
*/
#define EXAMPLE_COAP_PSK_KEY CONFIG_EXAMPLE_COAP_PSK_KEY

/* The examples use CoAP Logging Level that
   you can set via 'idf.py menuconfig'.

   If you'd rather not, just change the below entry to a value
   that is between 0 and 7 with
   the config you want - ie #define EXAMPLE_COAP_LOG_DEFAULT_LEVEL 7

   Caution: Logging is enabled in libcoap only up to level as defined
   by 'idf.py menuconfig' to reduce code size.
*/
#define EXAMPLE_COAP_LOG_DEFAULT_LEVEL CONFIG_COAP_LOG_DEFAULT_LEVEL

const static char *TAG = "CoAP_server";

static char espressif_data[100];
static int espressif_data_len = 0;

#ifdef CONFIG_COAP_MBEDTLS_PKI
/* CA cert, taken from coap_ca.pem
   Server cert, taken from coap_server.crt
   Server key, taken from coap_server.key

   The PEM, CRT and KEY file are examples taken from
   https://github.com/eclipse/californium/tree/master/demo-certs/src/main/resources
   as the Certificate test (by default) for the coap_client is against the
   californium server.

   To embed it in the app binary, the PEM, CRT and KEY file is named
   in the CMakeLists.txt EMBED_TXTFILES definition.
 */
extern uint8_t ca_pem_start[] asm("_binary_coap_ca_pem_start");
extern uint8_t ca_pem_end[]   asm("_binary_coap_ca_pem_end");
extern uint8_t server_crt_start[] asm("_binary_coap_server_crt_start");
extern uint8_t server_crt_end[]   asm("_binary_coap_server_crt_end");
extern uint8_t server_key_start[] asm("_binary_coap_server_key_start");
extern uint8_t server_key_end[]   asm("_binary_coap_server_key_end");
#endif /* CONFIG_COAP_MBEDTLS_PKI */

#ifdef CONFIG_COAP_OSCORE_SUPPORT
extern uint8_t oscore_conf_start[] asm("_binary_coap_oscore_conf_start");
extern uint8_t oscore_conf_end[]   asm("_binary_coap_oscore_conf_end");
#endif /* CONFIG_COAP_OSCORE_SUPPORT */

#define INITIAL_DATA "Hello World!"

/*
 * The resource handler
 */
static void
hnd_espressif_get(coap_resource_t *resource,
                  coap_session_t *session,
                  const coap_pdu_t *request,
                  const coap_string_t *query,
                  coap_pdu_t *response)
{
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);
    coap_add_data_large_response(resource, session, request, response,
                                 query, COAP_MEDIATYPE_TEXT_PLAIN, 60, 0,
                                 (size_t)espressif_data_len,
                                 (const u_char *)espressif_data,
                                 NULL, NULL);
}

static void
hnd_espressif_put(coap_resource_t *resource,
                  coap_session_t *session,
                  const coap_pdu_t *request,
                  const coap_string_t *query,
                  coap_pdu_t *response)
{
    size_t size;
    size_t offset;
    size_t total;
    const unsigned char *data;

    coap_resource_notify_observers(resource, NULL);

    if (strcmp (espressif_data, INITIAL_DATA) == 0) {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_CREATED);
    } else {
        coap_pdu_set_code(response, COAP_RESPONSE_CODE_CHANGED);
    }

    /* coap_get_data_large() sets size to 0 on error */
    (void)coap_get_data_large(request, &size, &data, &offset, &total);

    if (size == 0) {      /* re-init */
        snprintf(espressif_data, sizeof(espressif_data), INITIAL_DATA);
        espressif_data_len = strlen(espressif_data);
    } else {
        espressif_data_len = size > sizeof (espressif_data) ? sizeof (espressif_data) : size;
        memcpy (espressif_data, data, espressif_data_len);
    }
}

static void
hnd_espressif_delete(coap_resource_t *resource,
                     coap_session_t *session,
                     const coap_pdu_t *request,
                     const coap_string_t *query,
                     coap_pdu_t *response)
{
    coap_resource_notify_observers(resource, NULL);
    snprintf(espressif_data, sizeof(espressif_data), INITIAL_DATA);
    espressif_data_len = strlen(espressif_data);
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_DELETED);
}

#ifdef CONFIG_COAP_OSCORE_SUPPORT
static void
hnd_oscore_get(coap_resource_t *resource,
               coap_session_t *session,
               const coap_pdu_t *request,
               const coap_string_t *query,
               coap_pdu_t *response)
{
    coap_pdu_set_code(response, COAP_RESPONSE_CODE_CONTENT);
    coap_add_data_large_response(resource, session, request, response,
                                 query, COAP_MEDIATYPE_TEXT_PLAIN, 60, 0,
                                 sizeof("OSCORE Success!"),
                                 (const u_char *)"OSCORE Success!",
                                 NULL, NULL);
}
#endif /* CONFIG_COAP_OSCORE_SUPPORT */

#ifdef CONFIG_COAP_MBEDTLS_PKI

static int
verify_cn_callback(const char *cn,
                   const uint8_t *asn1_public_cert,
                   size_t asn1_length,
                   coap_session_t *session,
                   unsigned depth,
                   int validated,
                   void *arg
                  )
{
    coap_log_info("CN '%s' presented by server (%s)\n",
                  cn, depth ? "CA" : "Certificate");
    return 1;
}
#endif /* CONFIG_COAP_MBEDTLS_PKI */

static void
coap_log_handler (coap_log_t level, const char *message)
{
    uint32_t esp_level = ESP_LOG_INFO;
    const char *cp = strchr(message, '\n');

    while (cp) {
        ESP_LOG_LEVEL(esp_level, TAG, "%.*s", (int)(cp - message), message);
        message = cp + 1;
        cp = strchr(message, '\n');
    }
    if (message[0] != '\000') {
        ESP_LOG_LEVEL(esp_level, TAG, "%s", message);
    }
}

static void coap_example_server(void *p)
{
    coap_context_t *ctx = NULL;
    coap_resource_t *resource = NULL;
    int have_ep = 0;
    uint16_t u_s_port = atoi(CONFIG_EXAMPLE_COAP_LISTEN_PORT);
#ifdef CONFIG_EXAMPLE_COAPS_LISTEN_PORT
    uint16_t s_port = atoi(CONFIG_EXAMPLE_COAPS_LISTEN_PORT);
#else /* ! CONFIG_EXAMPLE_COAPS_LISTEN_PORT */
    uint16_t s_port = 0;
#endif /* ! CONFIG_EXAMPLE_COAPS_LISTEN_PORT */

#ifdef CONFIG_EXAMPLE_COAP_WEBSOCKET_PORT
    uint16_t ws_port = atoi(CONFIG_EXAMPLE_COAP_WEBSOCKET_PORT);
#else /* ! CONFIG_EXAMPLE_COAP_WEBSOCKET_PORT */
    uint16_t ws_port = 0;
#endif /* ! CONFIG_EXAMPLE_COAP_WEBSOCKET_PORT */

#ifdef CONFIG_EXAMPLE_COAP_WEBSOCKET_SECURE_PORT
    uint16_t ws_s_port = atoi(CONFIG_EXAMPLE_COAP_WEBSOCKET_SECURE_PORT);
#else /* ! CONFIG_EXAMPLE_COAP_WEBSOCKET_SECURE_PORT */
    uint16_t ws_s_port = 0;
#endif /* ! CONFIG_EXAMPLE_COAP_WEBSOCKET_SECURE_PORT */
    uint32_t scheme_hint_bits;
#ifdef CONFIG_COAP_OSCORE_SUPPORT
    coap_str_const_t osc_conf = { 0, 0};
    coap_oscore_conf_t *oscore_conf;
#endif /* CONFIG_COAP_OSCORE_SUPPORT */

    /* Initialize libcoap library */
    coap_startup();

    snprintf(espressif_data, sizeof(espressif_data), INITIAL_DATA);
    espressif_data_len = strlen(espressif_data);
    coap_set_log_handler(coap_log_handler);
    coap_set_log_level(EXAMPLE_COAP_LOG_DEFAULT_LEVEL);

    while (1) {
        unsigned wait_ms;
        coap_addr_info_t *info = NULL;
        coap_addr_info_t *info_list = NULL;

        ctx = coap_new_context(NULL);
        if (!ctx) {
            ESP_LOGE(TAG, "coap_new_context() failed");
            goto clean_up;
        }
        coap_context_set_block_mode(ctx,
                                    COAP_BLOCK_USE_LIBCOAP | COAP_BLOCK_SINGLE_BODY);
        coap_context_set_max_idle_sessions(ctx, 20);

#ifdef CONFIG_COAP_MBEDTLS_PSK
        /* Need PSK setup before we set up endpoints */
        coap_context_set_psk(ctx, "CoAP",
                             (const uint8_t *)EXAMPLE_COAP_PSK_KEY,
                             sizeof(EXAMPLE_COAP_PSK_KEY) - 1);
#endif /* CONFIG_COAP_MBEDTLS_PSK */

#ifdef CONFIG_COAP_MBEDTLS_PKI
        /* Need PKI setup before we set up endpoints */
        unsigned int ca_pem_bytes = ca_pem_end - ca_pem_start;
        unsigned int server_crt_bytes = server_crt_end - server_crt_start;
        unsigned int server_key_bytes = server_key_end - server_key_start;
        coap_dtls_pki_t dtls_pki;

        memset (&dtls_pki, 0, sizeof(dtls_pki));
        dtls_pki.version = COAP_DTLS_PKI_SETUP_VERSION;
        if (ca_pem_bytes) {
            /*
             * Add in additional certificate checking.
             * This list of enabled can be tuned for the specific
             * requirements - see 'man coap_encryption'.
             *
             * Note: A list of root ca file can be setup separately using
             * coap_context_set_pki_root_cas(), but the below is used to
             * define what checking actually takes place.
             */
            dtls_pki.verify_peer_cert        = 1;
            dtls_pki.check_common_ca         = 1;
            dtls_pki.allow_self_signed       = 1;
            dtls_pki.allow_expired_certs     = 1;
            dtls_pki.cert_chain_validation   = 1;
            dtls_pki.cert_chain_verify_depth = 2;
            dtls_pki.check_cert_revocation   = 1;
            dtls_pki.allow_no_crl            = 1;
            dtls_pki.allow_expired_crl       = 1;
            dtls_pki.allow_bad_md_hash       = 1;
            dtls_pki.allow_short_rsa_length  = 1;
            dtls_pki.validate_cn_call_back   = verify_cn_callback;
            dtls_pki.cn_call_back_arg        = NULL;
            dtls_pki.validate_sni_call_back  = NULL;
            dtls_pki.sni_call_back_arg       = NULL;
        }
        dtls_pki.pki_key.key_type = COAP_PKI_KEY_PEM_BUF;
        dtls_pki.pki_key.key.pem_buf.public_cert = server_crt_start;
        dtls_pki.pki_key.key.pem_buf.public_cert_len = server_crt_bytes;
        dtls_pki.pki_key.key.pem_buf.private_key = server_key_start;
        dtls_pki.pki_key.key.pem_buf.private_key_len = server_key_bytes;
        dtls_pki.pki_key.key.pem_buf.ca_cert = ca_pem_start;
        dtls_pki.pki_key.key.pem_buf.ca_cert_len = ca_pem_bytes;

        coap_context_set_pki(ctx, &dtls_pki);
#endif /* CONFIG_COAP_MBEDTLS_PKI */

#ifdef CONFIG_COAP_OSCORE_SUPPORT
        osc_conf.s = oscore_conf_start;
        osc_conf.length = oscore_conf_end - oscore_conf_start;
        oscore_conf = coap_new_oscore_conf(osc_conf,
                                           NULL,
                                           NULL, 0);
        coap_context_oscore_server(ctx, oscore_conf);
#endif /* CONFIG_COAP_OSCORE_SUPPORT */

        /* set up the CoAP server socket(s) */
        scheme_hint_bits =
            coap_get_available_scheme_hint_bits(
#if defined(CONFIG_COAP_MBEDTLS_PSK) || defined(CONFIG_COAP_MBEDTLS_PKI)
                1,
#else /* ! CONFIG_COAP_MBEDTLS_PSK) && ! CONFIG_COAP_MBEDTLS_PKI */
                0,
#endif /* ! CONFIG_COAP_MBEDTLS_PSK) && ! CONFIG_COAP_MBEDTLS_PKI */
#ifdef CONFIG_COAP_WEBSOCKETS
                1,
#else /* ! CONFIG_COAP_WEBSOCKETS */
                0,
#endif /* ! CONFIG_COAP_WEBSOCKETS */
                0);

#if LWIP_IPV6
        info_list = coap_resolve_address_info(coap_make_str_const("::"), u_s_port, s_port,
                                              ws_port, ws_s_port,
                                              0,
                                              scheme_hint_bits,
                                              COAP_RESOLVE_TYPE_LOCAL);
#else /* LWIP_IPV6 */
        info_list = coap_resolve_address_info(coap_make_str_const("0.0.0.0"), u_s_port, s_port,
                                              ws_port, ws_s_port,
                                              0,
                                              scheme_hint_bits,
                                              COAP_RESOLVE_TYPE_LOCAL);
#endif /* LWIP_IPV6 */
        if (info_list == NULL) {
            ESP_LOGE(TAG, "coap_resolve_address_info() failed");
            goto clean_up;
        }

        for (info = info_list; info != NULL; info = info->next) {
            coap_endpoint_t *ep;

            ep = coap_new_endpoint(ctx, &info->addr, info->proto);
            if (!ep) {
                ESP_LOGW(TAG, "cannot create endpoint for proto %u", info->proto);
            } else {
                have_ep = 1;
            }
        }
        coap_free_address_info(info_list);
        if (!have_ep) {
            ESP_LOGE(TAG, "No endpoints available");
            goto clean_up;
        }

        resource = coap_resource_init(coap_make_str_const("Espressif"), 0);
        if (!resource) {
            ESP_LOGE(TAG, "coap_resource_init() failed");
            goto clean_up;
        }
        coap_register_handler(resource, COAP_REQUEST_GET, hnd_espressif_get);
        coap_register_handler(resource, COAP_REQUEST_PUT, hnd_espressif_put);
        coap_register_handler(resource, COAP_REQUEST_DELETE, hnd_espressif_delete);
        /* We possibly want to Observe the GETs */
        coap_resource_set_get_observable(resource, 1);
        coap_add_resource(ctx, resource);
#ifdef CONFIG_COAP_OSCORE_SUPPORT
        resource = coap_resource_init(coap_make_str_const("oscore"), COAP_RESOURCE_FLAGS_OSCORE_ONLY);
        if (!resource) {
            ESP_LOGE(TAG, "coap_resource_init() failed");
            goto clean_up;
        }
        coap_register_handler(resource, COAP_REQUEST_GET, hnd_oscore_get);
        coap_add_resource(ctx, resource);
#endif /* CONFIG_COAP_OSCORE_SUPPORT */

#if defined(CONFIG_EXAMPLE_COAP_MCAST_IPV4) || defined(CONFIG_EXAMPLE_COAP_MCAST_IPV6)
        esp_netif_t *netif = NULL;
        for (int i = 0; i < esp_netif_get_nr_of_ifs(); ++i) {
            char buf[8];
            netif = esp_netif_next(netif);
            esp_netif_get_netif_impl_name(netif, buf);
#if defined(CONFIG_EXAMPLE_COAP_MCAST_IPV4)
            coap_join_mcast_group_intf(ctx, CONFIG_EXAMPLE_COAP_MULTICAST_IPV4_ADDR, buf);
#endif /* CONFIG_EXAMPLE_COAP_MCAST_IPV4 */
#if defined(CONFIG_EXAMPLE_COAP_MCAST_IPV6)
            /* When adding IPV6 esp-idf requires ifname param to be filled in */
            coap_join_mcast_group_intf(ctx, CONFIG_EXAMPLE_COAP_MULTICAST_IPV6_ADDR, buf);
#endif /* CONFIG_EXAMPLE_COAP_MCAST_IPV6 */
        }
#endif /* CONFIG_EXAMPLE_COAP_MCAST_IPV4 || CONFIG_EXAMPLE_COAP_MCAST_IPV6 */

        wait_ms = COAP_RESOURCE_CHECK_TIME * 1000;

        while (1) {
            int result = coap_io_process(ctx, wait_ms);
            if (result < 0) {
                break;
            } else if (result && (unsigned)result < wait_ms) {
                /* decrement if there is a result wait time returned */
                wait_ms -= result;
            }
            if (result) {
                /* result must have been >= wait_ms, so reset wait_ms */
                wait_ms = COAP_RESOURCE_CHECK_TIME * 1000;
            }
        }
    }
clean_up:
    coap_free_context(ctx);
    coap_cleanup();

    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    xTaskCreate(coap_example_server, "coap", 8 * 1024, NULL, 5, NULL);
}
