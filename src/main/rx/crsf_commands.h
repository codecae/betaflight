
#pragma once

#include "rx/crsf.h"

typedef enum {
    CRSF_COMMAND_LED_DEFAULT = 0x0901,
    CRSF_COMMAND_LED_OVERRIDE = 0x0902
} crsfCommandType_e;

typedef enum {
    CRSF_COMMAND_LENGTH_LED_DEFAULT = 5,
    CRSF_COMMAND_LENGTH_LED_OVERRIDE = 8
} crsfCommandLength_e;

typedef struct crsfLedParam_s {
    uint8_t hs1;
    uint8_t hs2;
    uint8_t v;
} crsfLedParam_t;

bool handleCrsfCommand(crsfFrame_t *frame, const uint8_t *cmdCrc);
