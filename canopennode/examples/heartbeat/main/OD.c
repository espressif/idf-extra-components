#define OD_DEFINITION
#include "301/CO_ODinterface.h"
#include "OD.h"

static uint8_t od_error_register = 0x00;
static uint8_t od_predef_err_sub0 = 0x08;
static uint32_t od_predef_err[8] = {0};
static uint32_t od_cobid_emcy = 0x00000080;
static uint16_t od_producer_hb_ms = 1000;

static OD_obj_var_t obj_1001 = {
    .dataOrig = &od_error_register,
    .attribute = ODA_SDO_R,
    .dataLength = 1
};

static OD_obj_array_t obj_1003 = {
    .dataOrig0 = &od_predef_err_sub0,
    .dataOrig = &od_predef_err[0],
    .attribute0 = ODA_SDO_R,
    .attribute = ODA_SDO_R | ODA_MB,
    .dataElementLength = 4,
    .dataElementSizeof = sizeof(uint32_t)
};

static OD_obj_var_t obj_1014 = {
    .dataOrig = &od_cobid_emcy,
    .attribute = ODA_SDO_RW | ODA_MB,
    .dataLength = 4
};

static OD_obj_var_t obj_1017 = {
    .dataOrig = &od_producer_hb_ms,
    .attribute = ODA_SDO_RW | ODA_MB,
    .dataLength = 2
};

OD_entry_t OD_ENTRY_H1001 = {0x1001, 0x01, ODT_VAR, &obj_1001, NULL};
OD_entry_t OD_ENTRY_H1003 = {0x1003, 0x09, ODT_ARR, &obj_1003, NULL};
OD_entry_t OD_ENTRY_H1014 = {0x1014, 0x01, ODT_VAR, &obj_1014, NULL};
OD_entry_t OD_ENTRY_H1017 = {0x1017, 0x01, ODT_VAR, &obj_1017, NULL};

static OD_entry_t od_list[] = {
    {0x1001, 0x01, ODT_VAR, &obj_1001, NULL},
    {0x1003, 0x09, ODT_ARR, &obj_1003, NULL},
    {0x1014, 0x01, ODT_VAR, &obj_1014, NULL},
    {0x1017, 0x01, ODT_VAR, &obj_1017, NULL},
};

static OD_t od_root = {
    .size = 4,
    .list = od_list,
};

OD_t *OD = &od_root;
