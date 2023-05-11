#pragma once
#include "emu8kreg.h"

void emu8k_hwinit(uint32_t iobase);
void emu8k_write(uint32_t iobase, uint32_t reg, uint32_t data);
uint32_t emu8k_read(uint32_t iobase, uint32_t reg);
void emu8k_delay(uint32_t iobase, uint32_t samples);
void emu8k_dramEnable(uint32_t iobase, bool readWrite, uint32_t startChannel = 0, uint32_t channels = 30);
void emu8k_dramDisable(uint32_t iobase, uint32_t startChannel = 0, uint32_t channels = 30);
int emu8k_waitForWriteFlush(uint32_t iobase);
int emu8k_waitForReadReady(uint32_t iobase);
int emu8k_probe(uint32_t iobase);
uint32_t emu8k_getMemSize(uint32_t iobase);
void emu8k_uploadSample(uint32_t iobase, uint32_t start, int16_t* data, uint32_t samples);
void emu8k_setFlatEq(uint32_t iobase);
void emu8k_disableOutput(uint32_t iobase);
void emu8k_enableOutput(uint32_t iobase);
int emu8k_initChannels(uint32_t iobase, uint32_t channels, uint32_t startChannel = 0, bool initEnvelope = false);

