#pragma once

#include <stdint.h>
 
struct dmaPorts {
    unsigned char address;
    unsigned char count;
    unsigned char page;
    unsigned char request;
    unsigned char mask;
    unsigned char mode;
    unsigned char clear;
    unsigned char dummy;
};

// mode consts
enum {
    dmaModeDemand       = 0x0,
    dmaModeSingle       = 0x40,
    dmaModeBlock        = 0x80,
    dmaModeCascade      = 0xC0,

    dmaModeInc          = 0x0,
    dmaModeDec          = 0x20,

    dmaModeNoAutoInit   = 0x0,
    dmaModeAutoInit     = 0x10,

    dmaModeVerify       = 0x0,
    dmaModeWrite        = 0x4,
    dmaModeRead         = 0x8,
};

// setup DMA for transfer
bool dmaSetup(uint32_t chan, void *block, uint32_t len, uint32_t mode);

// start/pause transfer
bool dmaStart(uint32_t chan);
bool dmaPause(uint32_t chan);

// stop transfer
bool dmaStop(uint32_t chan);

// get current address and count
uint32_t dmaGetCurrentAddress(uint32_t chan);
uint32_t dmaGetCurrentCount(uint32_t chan);

