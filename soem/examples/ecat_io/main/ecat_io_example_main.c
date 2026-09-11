/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_soem.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <inttypes.h>
#include <stdbool.h>

/* Slave identification defined by ESI/SII info. */
#define EXPECTED_VENDOR_ID      0x000004D8u
#define EXPECTED_PRODUCT_ID     0x0000000Du
#define EXPECTED_REVISION       0x00000001u
#define OUTPUT_PHYS_ADDR        0x0F01u /* Store RxPDO data. */
#define INPUT_PHYS_ADDR         0x1000u /* Store TxPDO data. */

/* Slave hardware design mapping to process-data. */
#define SW1_INPUT_MASK          0x01u
#define LED3_OUTPUT_MASK        0x01u

/* EtherCAT network logical address. */
#define OUTPUT_LOG_ADDR         0x00000000u
#define INPUT_LOG_ADDR          0x00000001u

/* EtherCAT cyclic IO runtime parameters.*/
#define IO_CYCLE_PERIOD_MS      10 /* Cycle period in milliseconds. */
#define IO_CYCLE_PERIOD_US      (IO_CYCLE_PERIOD_MS * 1000)
#define IO_EXPECTED_WKC         3 /* Expected WKC count in one cycle. LRW write success +2, read +1. */

/* PHY Link-Up event bit. */
#define ETH_LINK_UP_BIT BIT0

static EventGroupHandle_t s_eth_events;
static ecx_contextt *s_ecat_context; /* SOEM allocates the context from the heap during initialization. */
static const char *TAG = "ecat_io";

static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;

    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED: {
        uint8_t mac_addr[6] = {0};

        (void)esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "MAC %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2],
                 mac_addr[3], mac_addr[4], mac_addr[5]);
        xEventGroupSetBits(s_eth_events, ETH_LINK_UP_BIT);
        break;
    }

    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Ethernet Link Down");
        xEventGroupClearBits(s_eth_events, ETH_LINK_UP_BIT);
        break;

    default:
        break;
    }
}

/* Initialize the internal EMAC with an external IP101 PHY. */
static esp_err_t example_eth_init(esp_eth_handle_t *eth_handle_out)
{
    if (eth_handle_out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    eth_esp32_emac_config_t emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    /* Use the ESP32-P4 default RMII pins and the PHY settings required by example hardware. */
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = 51;

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);
    if (mac == NULL) {
        return ESP_ERR_NO_MEM;
    }

    /* IP101 is supported by the generic IEEE 802.3 PHY driver. */
    esp_eth_phy_t *phy = esp_eth_phy_new_generic(&phy_config);
    if (phy == NULL) {
        mac->del(mac);
        return ESP_ERR_NO_MEM;
    }

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    esp_err_t ret = esp_eth_driver_install(&eth_config, &eth_handle);
    if (ret != ESP_OK) {
        mac->del(mac);
        phy->del(phy);
        return ret;
    }

    *eth_handle_out = eth_handle;
    return ESP_OK;
}

/* Uninstall Ethernet and release the MAC and PHY instances. */
static esp_err_t example_eth_deinit(esp_eth_handle_t eth_handle)
{
    if (eth_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_eth_mac_t *mac = NULL;
    esp_eth_phy_t *phy = NULL;

    esp_err_t ret = esp_eth_get_mac_instance(eth_handle, &mac);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_eth_get_phy_instance(eth_handle, &phy);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_eth_driver_uninstall(eth_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = mac->del(mac);
    if (ret != ESP_OK) {
        phy->del(phy);
        return ret;
    }

    return phy->del(phy);
}

/* Configure the fixed process-data mapping used by this example.
   Slave in example does not support CoE.
   Names follow the slave's perspective: IN/OUT focus on field-side, Rx/Tx focus on network-side. */
static int example_configure_process_data_mapping(uint16_t slave_number)
{
    ec_slavet *slave = &s_ecat_context->slavelist[slave_number];
    uint16_t config_address = slave->configadr; /* Fetch slave's configured address. Use FPWR cmd. */

    /* Initialize SM and FMMU parameters. */
    ec_smt sm_output = {
        .StartAddr = htoes(OUTPUT_PHYS_ADDR),
        .SMlength = htoes(1),
        .SMflags = htoel(0x00010044u),
    };
    ec_smt sm_input = {
        .StartAddr = htoes(INPUT_PHYS_ADDR),
        .SMlength = htoes(1),
        .SMflags = htoel(0x00010000u),
    };
    ec_fmmut fmmu_output = {
        .LogStart = htoel(OUTPUT_LOG_ADDR),
        .LogLength = htoes(1),
        .LogStartbit = 0, /* Start bit in first byte.*/
        .LogEndbit = 7, /* End bit in last byte.*/
        .PhysStart = htoes(OUTPUT_PHYS_ADDR),
        .PhysStartBit = 0,
        .FMMUtype = 2, /* Mapping direction, 2 stands for logical write. */
        .FMMUactive = 1,
    };
    ec_fmmut fmmu_input = {
        .LogStart = htoel(INPUT_LOG_ADDR),
        .LogLength = htoes(1),
        .LogStartbit = 0,
        .LogEndbit = 7,
        .PhysStart = htoes(INPUT_PHYS_ADDR),
        .PhysStartBit = 0,
        .FMMUtype = 1,
        .FMMUactive = 1,
    };

    /* Use SM0 for process-data output, SM1 for process-data input. */
    int wkc = ecx_FPWR(&s_ecat_context->port, config_address, ECT_REG_SM0,
                       sizeof(sm_output), &sm_output, EC_TIMEOUTRET3);
    if (wkc <= 0) {
        ESP_LOGE(TAG, "Failed to configure SM0: WKC=%d", wkc);
        return 0;
    }
    wkc = ecx_FPWR(&s_ecat_context->port, config_address, ECT_REG_SM1,
                   sizeof(sm_input), &sm_input, EC_TIMEOUTRET3);
    if (wkc <= 0) {
        ESP_LOGE(TAG, "Failed to configure SM1: WKC=%d", wkc);
        return 0;
    }

    /* Use FMMU0 for process-data output, FMMU1 for process-data input. */
    wkc = ecx_FPWR(&s_ecat_context->port, config_address, ECT_REG_FMMU0,
                   sizeof(fmmu_output), &fmmu_output, EC_TIMEOUTRET3);
    if (wkc <= 0) {
        ESP_LOGE(TAG, "Failed to configure FMMU0: WKC=%d", wkc);
        return 0;
    }
    wkc = ecx_FPWR(&s_ecat_context->port, config_address, ECT_REG_FMMU1,
                   sizeof(fmmu_input), &fmmu_input, EC_TIMEOUTRET3);
    if (wkc <= 0) {
        ESP_LOGE(TAG, "Failed to configure FMMU1: WKC=%d", wkc);
        return 0;
    }

    /* Keep the SOEM slave descriptor consistent with the ESC configuration. */
    slave->SM[0] = sm_output;
    slave->SM[1] = sm_input;
    slave->SMtype[0] = 3; /* 3 stands for Process-data output. */
    slave->SMtype[1] = 4; /* 4 stands for Process-data input. */
    slave->Obits = 8;
    slave->Ibits = 8;
    slave->Obytes = 1;
    slave->Ibytes = 1;
    slave->FMMU[0] = fmmu_output;
    slave->FMMU[1] = fmmu_input;
    slave->FMMUunused = 2; /* Next free FMMU index. */

    return 1;
}

/* Request a PRE-OP or SAFE-OP state transition. Repeatedly check state reached or not until timeout.
   @return 1 if slave reach the exact requested state without AL ERROR, otherwise 0. */
static int example_request_state(uint16_t slave_number, uint16_t requested_state)
{
    ec_slavet *slave = &s_ecat_context->slavelist[slave_number];

    slave->state = requested_state; /* Set target state wanted. */
    int wkc = ecx_writestate(s_ecat_context, slave_number);
    if (wkc <= 0) {
        ESP_LOGE(TAG, "Failed to request state 0x%02x (unexpected WKC): WKC=%d", requested_state, wkc);
        return 0;
    }

    int64_t deadline = esp_timer_get_time() + EC_TIMEOUTSTATE; /* Set deadline timepoint in us. */
    do {
        /* ecx_statecheck() returns only the base state without the error flag,
           but stores the complete AL Status and AL Status Code in slave descriptor. */
        (void)ecx_statecheck(s_ecat_context, slave_number, requested_state, EC_TIMEOUTRET);
        if (slave->state == requested_state) {
            return 1;
        }
        vTaskDelay(pdMS_TO_TICKS(IO_CYCLE_PERIOD_MS)); /* No real-time requirement. */
    } while (esp_timer_get_time() < deadline);
    ESP_LOGE(TAG, "Failed to request state 0x%02x (timeout): state=0x%04x, AL status=0x%04x",
             requested_state, slave->state, slave->ALstatuscode);
    return 0;
}

/* Request an OP transition while keeping process-data exchange active. */
static int example_request_state_op(uint16_t slave_number)
{
    ec_slavet *slave = &s_ecat_context->slavelist[slave_number];
    uint8_t process_image[2] = {0}; /* Output fixed at zero, input not processed. */

    /* One LRW communication before state change request. */
    int wkc = ecx_LRW(&s_ecat_context->port, OUTPUT_LOG_ADDR,
                      sizeof(process_image), process_image, EC_TIMEOUTRET3);
    if (wkc != IO_EXPECTED_WKC) {
        ESP_LOGE(TAG, "Failed to request state OP (first LRW failed): WKC=%d", wkc);
        return 0;
    }

    /* Formally request OP state. */
    slave->state = EC_STATE_OPERATIONAL;
    wkc = ecx_writestate(s_ecat_context, slave_number);
    if (wkc <= 0) {
        ESP_LOGE(TAG, "Failed to request state OP (unexpected WKC): WKC=%d", wkc);
        return 0;
    }

    /* Try to check OP state reached while maintaining process-data communication. */
    int64_t deadline = esp_timer_get_time() + EC_TIMEOUTSTATE;
    do {
        wkc = ecx_LRW(&s_ecat_context->port, OUTPUT_LOG_ADDR,
                      sizeof(process_image), process_image, EC_TIMEOUTRET3);
        if (wkc != IO_EXPECTED_WKC) {
            ESP_LOGE(TAG, "Failed to request state OP (later LRW failed): WKC=%d", wkc);
            return 0;
        }

        (void)ecx_statecheck(s_ecat_context, slave_number, EC_STATE_OPERATIONAL, EC_TIMEOUTRET);
        if (slave->state == EC_STATE_OPERATIONAL) {
            return 1;
        }
        vTaskDelay(pdMS_TO_TICKS(IO_CYCLE_PERIOD_MS));
    } while (esp_timer_get_time() < deadline);
    ESP_LOGE(TAG, "Failed to request state OP (timeout): state=0x%04x, AL status=0x%04x",
             slave->state, slave->ALstatuscode);
    return 0;
}

/* Callback function for esp_timer. */
static void example_cycle_timer_callback(void *arg)
{
    TaskHandle_t cyclic_task = (TaskHandle_t)arg;
    xTaskNotifyGive(cyclic_task);
}

/* Run cyclic LRW process-data exchange.
   process_image[0] contains the output calculated from the previous input.
   process_image[1] contains the current input.
   SW1 is active low and LED3 is active high.
   An esp_timer callback periodically notifies this task to start each cycle. */
static void example_run_cyclic_io(ecx_contextt *context, EventGroupHandle_t eth_events)
{
    uint8_t process_image[2] = {0};
    uint8_t next_output = 0;
    uint8_t previous_input = 0;
    bool first_input = true;
    uint32_t cycle = 0;

    /* Create a software timer. */
    TaskHandle_t cyclic_task = xTaskGetCurrentTaskHandle(); /* main_task handle. */
    esp_timer_handle_t cycle_timer = NULL;
    const esp_timer_create_args_t cycle_timer_args = {
        .callback = example_cycle_timer_callback,
        .arg = cyclic_task,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ecat_cycle",
    };
    ESP_ERROR_CHECK(esp_timer_create(&cycle_timer_args, &cycle_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(cycle_timer, IO_CYCLE_PERIOD_US));

    /* Start cyclic IO. */
    ESP_LOGI(TAG, "EtherCAT cyclic IO started");
    ESP_LOGI(TAG, "Press SW1: LED3 ON; release SW1: LED3 OFF");

    while (true) {
        /* Blocked wait until periodic timer notifies this task. */
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if ((xEventGroupGetBits(eth_events) & ETH_LINK_UP_BIT) == 0) {
            break;
        }

        process_image[0] = next_output;

        int wkc = ecx_LRW(&context->port, OUTPUT_LOG_ADDR,
                          sizeof(process_image), process_image, EC_TIMEOUTRET3);
        if (wkc != IO_EXPECTED_WKC) {
            ESP_LOGE(TAG, "Cycle %" PRIu32 ": unexpected WKC=%d", cycle, wkc);
            break;
        }

        uint8_t current_input = process_image[1];
        bool sw1_pressed = (current_input & SW1_INPUT_MASK) == 0;

        next_output = sw1_pressed ? LED3_OUTPUT_MASK : 0;

        if (first_input || current_input != previous_input) {
            ESP_LOGI(TAG,
                     "Cycle %" PRIu32 ": "
                     "input=0x%02x SW1=%s, "
                     "next output=0x%02x LED3=%s",
                     cycle,
                     current_input,
                     sw1_pressed ? "PRESSED" : "RELEASED",
                     next_output,
                     sw1_pressed ? "ON" : "OFF");

            previous_input = current_input;
            first_input = false;
        }

        cycle++;
    }

    ESP_ERROR_CHECK(esp_timer_stop(cycle_timer));
    ESP_ERROR_CHECK(esp_timer_delete(cycle_timer));
    ESP_LOGW(TAG, "EtherCAT cyclic IO stopped");
}

void app_main(void)
{
    /* stage1: Initialize Ethernet and wait for Link-Up. */
    esp_eth_handle_t eth_handle = NULL;
    s_eth_events = xEventGroupCreate();
    ESP_ERROR_CHECK(s_eth_events != NULL ? ESP_OK : ESP_ERR_NO_MEM);

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(
                        ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler, NULL));

    ESP_ERROR_CHECK(example_eth_init(&eth_handle));
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

    /* Wait up to 10 seconds for the physical Ethernet link. */
    EventBits_t bits = xEventGroupWaitBits(s_eth_events, ETH_LINK_UP_BIT,
                                           pdFALSE, pdTRUE, pdMS_TO_TICKS(10000));
    ESP_ERROR_CHECK((bits & ETH_LINK_UP_BIT) != 0 ? ESP_OK : ESP_ERR_TIMEOUT);

    /* stage2: Initialize SOEM over the Ethernet driver. */
    s_ecat_context = esp_soem_init(eth_handle);
    ESP_ERROR_CHECK(s_ecat_context != NULL ? ESP_OK : ESP_FAIL);
    ESP_LOGI(TAG, "SOEM initialized");

    /* stage3: Discover and initialize ECAT slaves.
       Config fixed address, read SII, and ask for entering PRE-OP(within err-acknowledge set, without check). */
    int slave_count = ecx_config_init(s_ecat_context);
    ESP_ERROR_CHECK(slave_count > 0 ? ESP_OK : ESP_FAIL);
    ESP_LOGI(TAG, "%d EtherCAT slave(s) found", slave_count);

    /* Example uses a fixed process-data mapping for one LAN9252 slave. */
    const uint16_t slave_index = 1;
    ec_slavet *slave = &s_ecat_context->slavelist[slave_index];

    /* Check slave's identity.*/
    ESP_ERROR_CHECK(slave_count == 1 ? ESP_OK : ESP_ERR_INVALID_SIZE);
    ESP_ERROR_CHECK(
        ((slave->eep_man == EXPECTED_VENDOR_ID) &&
         (slave->eep_id == EXPECTED_PRODUCT_ID) &&
         (slave->eep_rev == EXPECTED_REVISION)) ? ESP_OK : ESP_ERR_INVALID_RESPONSE);
    ESP_LOGI(
        TAG, "Slave: vendor=0x%08" PRIx32 ", product=0x%08" PRIx32 ", revision=0x%08" PRIx32,
        slave->eep_man, slave->eep_id, slave->eep_rev);

    /* stage4: Ensure the slave is in PRE-OP before configuring the ESC. */
    ESP_ERROR_CHECK(example_request_state(
                        slave_index, EC_STATE_PRE_OP) ? ESP_OK : ESP_ERR_INVALID_STATE);
    ESP_LOGI(TAG, "Slave reached PRE-OP");

    /* stage5: Configure process-data mapping. */
    ESP_ERROR_CHECK(example_configure_process_data_mapping(slave_index) ? ESP_OK : ESP_FAIL);
    ESP_LOGI(TAG, "Process-data mapping configured");

    /* stage6: Ask for entering SAFE-OP and confirm it. */
    ESP_ERROR_CHECK(example_request_state(
                        slave_index, EC_STATE_SAFE_OP) ? ESP_OK : ESP_ERR_INVALID_STATE);
    ESP_LOGI(TAG, "Slave reached SAFE-OP");

    /* stage7: Ask for entering OP state and confirm it.
       Process-data communication must remain active during such transition. */
    ESP_ERROR_CHECK(example_request_state_op(
                        slave_index) ? ESP_OK : ESP_ERR_INVALID_STATE);
    ESP_LOGI(TAG, "Slave reached OP");

    /* stage8: Run cyclic IO closed-loop. */
    example_run_cyclic_io(s_ecat_context, s_eth_events);

    /* stage9: Resource release when cyclic IO error occurs. */
    ESP_ERROR_CHECK(esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler));
    ESP_ERROR_CHECK(esp_eth_stop(eth_handle));
    esp_soem_deinit(s_ecat_context);
    s_ecat_context = NULL;
    ESP_ERROR_CHECK(example_eth_deinit(eth_handle));
    eth_handle = NULL;
    ESP_ERROR_CHECK(esp_event_loop_delete_default());
    vEventGroupDelete(s_eth_events);
    s_eth_events = NULL;
    ESP_LOGI(TAG, "EtherCAT example stopped");
}
