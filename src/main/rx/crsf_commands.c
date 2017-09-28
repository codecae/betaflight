#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include "build/build_config.h"
#include "build/debug.h"

#include "common/color.h"

#include "io/ledstrip.h"

#include "rx/crsf.h"
#include "rx/crsf_commands.h"

bool updateLedColor(crsfLedParam_t *params) {
    static bool overrideActive[LED_CONFIGURABLE_COLOR_COUNT];
    static hsvColor_t defaultColors[LED_CONFIGURABLE_COLOR_COUNT];

    if (params) {
        for (int i = 0; i < LED_CONFIGURABLE_COLOR_COUNT; i++) {
            if (!overrideActive[i]) {
                const hsvColor_t *defaultColor = &ledStripConfig()->colors[i];
                defaultColors[i].h = defaultColor->h;
                defaultColors[i].s = defaultColor->s;
                defaultColors[i].v = defaultColor->v;
            }
            hsvColor_t *color = &ledStripConfigMutable()->colors[i];
            color->h = (params->hs1 << 1) | (params->hs2 >> 7);
            color->s = ceil(((100-(params->hs2 & 0x7F))/100)*255);
            color->v = ceil((params->v/100)*255);
            overrideActive[i] = true;
        }
        return true;
    } else {
        for (int i = 0; i < LED_CONFIGURABLE_COLOR_COUNT; i++) {
            if (overrideActive[i]) {
                hsvColor_t *defaultColor = &ledStripConfigMutable()->colors[i];
                defaultColor->h = defaultColors[i].h;
                defaultColor->s = defaultColors[i].s;
                defaultColor->v = defaultColors[i].v;
                overrideActive[i] = false;
            }
        }
        return true;
    }
    return false;
}

bool handleCrsfCommand(crsfFrame_t *frame, const uint8_t *cmdCrc) {
    uint16_t cmd = (frame->cmdFrame.command << 8) | frame->cmdFrame.subCommand;
    uint8_t plCmdCrc;
    switch (cmd) {
        case CRSF_COMMAND_LED_DEFAULT: ;
            plCmdCrc = frame->frame.payload[CRSF_COMMAND_LENGTH_LED_DEFAULT-1];
            if (*cmdCrc != plCmdCrc) {
                return false;
            }
            return updateLedColor(NULL);
            break;
        case CRSF_COMMAND_LED_OVERRIDE: ;
            plCmdCrc = frame->frame.payload[CRSF_COMMAND_LENGTH_LED_OVERRIDE-1];
            if (*cmdCrc != plCmdCrc) {
                return false;
            }
            crsfLedParam_t *params = (crsfLedParam_t *)&frame->cmdFrame.payload;
            return updateLedColor(params);
            break;
        default:
            break;
    }
    return false;
}
