#include <stdint.h>

#include "dma.h"
#include "inp.h"

dmaPorts dmaPorts[] = {
    { 0x00, 0x01, 0x87, 0x09, 0x0A, 0x0B, 0x0C, 0 }, // 0
    { 0x02, 0x03, 0x83, 0x09, 0x0A, 0x0B, 0x0C, 0 }, // 1
    { 0x04, 0x05, 0x81, 0x09, 0x0A, 0x0B, 0x0C, 0 }, // 2
    { 0x06, 0x07, 0x82, 0x09, 0x0A, 0x0B, 0x0C, 0 }, // 3
    
    { 0x00, 0x00, 0x00, 0xD2, 0xD4, 0xD6, 0xD8, 0 }, // 4 (unapplicable)
    { 0xC4, 0xC6, 0x8B, 0xD2, 0xD4, 0xD6, 0xD8, 0 }, // 5
    { 0xC8, 0xCA, 0x89, 0xD2, 0xD4, 0xD6, 0xD8, 0 }, // 6
    { 0xCC, 0xCE, 0x8A, 0xD2, 0xD4, 0xD6, 0xD8, 0 }, // 7
};

bool dmaSetup(uint32_t chan, void *block, uint32_t len, uint32_t mode) {
    unsigned char rawchan = chan & 3;
    
    // mask channel
    outp(dmaPorts[chan].mask,    0x04 | rawchan);
    
    // clear flip-flop
    inp(dmaPorts[chan].clear);
    
    // set mode
    outp(dmaPorts[chan].mode,    mode | rawchan);
    
    // apparently high DMAs require some shifting to make it work
    uint32_t rawptr = (uint32_t)block >> (chan >= 4 ? 1 : 0);
    uint32_t rawlen = (len            >> (chan >= 4 ? 1 : 0)) - 1;
    uint32_t rawpage =(uint32_t)block >> 16;
    
    // set offset
    outp(dmaPorts[chan].address,  rawptr        & 0xFF);
    outp(dmaPorts[chan].address, (rawptr >>  8) & 0xFF);
    outp(dmaPorts[chan].page,     rawpage       & 0xFF);
    
    // clear flip-flop again
    inp(dmaPorts[chan].clear);
    
    // set length
    outp(dmaPorts[chan].count,    rawlen       & 0xFF);
    outp(dmaPorts[chan].count,   (rawlen >> 8) & 0xFF);

    return true;
}

bool dmaPause(uint32_t chan) {    
    // mask channel
    outp(dmaPorts[chan].mask,    0x04 | (chan & 3));
    
    return true;
};

bool dmaStart(uint32_t chan) {
    // unmask channel
    outp(dmaPorts[chan].mask,    0x00 | (chan & 3));
    
    return true;
};

bool dmaStop(uint32_t chan) {
    // mask channel
    outp(dmaPorts[chan].mask,    0x04 | (chan & 3));
    
    // clear channel
    outp(dmaPorts[chan].clear,   0x00);
    
    return true;
};

uint16_t dmaRead(uint16_t port); 
#pragma aux dmaRead = "in  al, dx" "mov  bl, al" "in  al, dx" "mov  bh, al" parm [dx] value [bx] modify [ax]

// read DMA controller 16-bit register
static uint32_t dmaRead16bit(uint32_t reg) {
    // i8237 does not feature latching current address/count, so high byte can change while reading low byte!
    // so read it until retrieving correct value
    uint32_t timeout = 20; // try at least 20 times
    volatile uint16_t oldpos, pos = dmaRead(reg);
    
    do {
        oldpos = pos;
        pos = dmaRead(reg);
        
        if ((oldpos & 0xFF00) == (pos & 0xFF00)) break;
    } while (--timeout);

    return pos;
}

// return current address (A[15..0] for 8bit channels, A[16..1] for 16bit)
uint32_t dmaGetCurrentAddress(uint32_t chan) {
    // clear flip-flop
    inp(dmaPorts[chan].clear);
    return dmaRead16bit(dmaPorts[chan].address);
}

// return current count register value
uint32_t dmaGetCurrentCount(uint32_t chan) {
    // clear flip-flop
    inp(dmaPorts[chan].clear);
    return dmaRead16bit(dmaPorts[chan].count);
}