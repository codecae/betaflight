/*
 * telem_msp.h
 *
 */

#pragma once

typedef void (*sendMspTelemetryFnPtr)(uint8_t *data);

void processMspPacket(mspPacket_t *packet, mspPacket_t *replyPacket, uint8_t *txBuffer);
void generateMspErrorReply(uint8_t error, int16_t cmd, mspPacket_t *replyPacket, uint8_t *txBuffer);
bool sendTelemetryMspReply(uint8_t payloadSize, mspPacket_t *replyPacket, uint8_t *txBuffer, sendMspTelemetryFnPtr sendMspTelemetryFn);