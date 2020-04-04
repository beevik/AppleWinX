/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

class Memory2 {
public:
    uint8_t  Read8(uint16_t address);
    uint16_t Read16(uint16_t address);
    void     Write8(uint16_t address, uint8_t value);
};
