/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

constexpr uint8_t PS_CARRY     = 1 << 0;
constexpr uint8_t PS_ZERO      = 1 << 1;
constexpr uint8_t PS_INTERRUPT = 1 << 2;
constexpr uint8_t PS_DECIMAL   = 1 << 3;
constexpr uint8_t PS_BREAK     = 1 << 4;
constexpr uint8_t PS_RESERVED  = 1 << 5;
constexpr uint8_t PS_OVERFLOW  = 1 << 6;
constexpr uint8_t PS_SIGN      = 1 << 7;

CpuRegisters regs = { 0 };

static ECpuType s_cpuType;


/****************************************************************************
*
*   Addressing mode helpers
*
***/

//===========================================================================
static inline uint16_t Absolute() {
    uint16_t addr = *(uint16_t *)(g_pageRead[regs.pc >> 8] + (regs.pc & 0xff));
    regs.pc += 2;
    return addr;
}

//===========================================================================
static inline uint16_t AbsoluteX() {
    uint16_t addr = *(uint16_t *)(g_pageRead[regs.pc >> 8] + (regs.pc & 0xff)) + uint16_t(regs.x);
    regs.pc += 2;
    return addr;
}

//===========================================================================
static inline uint16_t AbsoluteX(int * cycles) {
    uint16_t tmp  = *(uint16_t *)(g_pageRead[regs.pc >> 8] + (regs.pc & 0xff));
    uint16_t addr = tmp + uint16_t(regs.x);
    if ((tmp ^ addr) & 0xff00)
        ++*cycles;
    regs.pc += 2;
    return addr;
}

//===========================================================================
static inline uint16_t AbsoluteY() {
    uint16_t addr = *(uint16_t *)(g_pageRead[regs.pc >> 8] + (regs.pc & 0xff)) + uint16_t(regs.y);
    regs.pc += 2;
    return addr;
}

//===========================================================================
static inline uint16_t AbsoluteY(int * cycles) {
    uint16_t tmp  = *(uint16_t *)(g_pageRead[regs.pc >> 8] + (regs.pc & 0xff));
    uint16_t addr = tmp + uint16_t(regs.y);
    if ((tmp ^ addr) & 0xff00)
        ++*cycles;
    regs.pc += 2;
    return addr;
}

//===========================================================================
static inline uint16_t Immediate() {
    return regs.pc++;
}

//===========================================================================
static inline uint16_t Indirect() {
    uint16_t offset = *(uint16_t *)(g_pageRead[regs.pc >> 8] + (regs.pc & 0xff));
    uint16_t addr   = *(uint16_t *)(g_pageRead[offset >> 8] + (offset & 0xff));
    regs.pc += 2;
    return addr;
}

//===========================================================================
static inline uint16_t Indirect6502() {
    uint16_t offset = *(uint16_t *)(g_pageRead[regs.pc >> 8] + (regs.pc & 0xff));
    uint16_t addr;
    if ((offset & 0xff) == 0xff) {
        uint16_t addr0 = g_pageRead[offset >> 8][0xff];
        uint16_t addr1 = g_pageRead[offset >> 8][0x00];
        addr = addr0 | addr1 << 8;
    }
    else {
        addr = *(uint16_t *)(g_pageRead[offset >> 8] + (offset & 0xff));
    }
    regs.pc += 2;
    return addr;
}

//===========================================================================
static inline uint16_t IndirectX() {
    uint8_t  offset = g_pageRead[regs.pc >> 8][regs.pc & 0xff] + regs.x;
    uint16_t addr   = *(uint16_t *)(g_pageRead[0] + offset);
    ++regs.pc;
    return addr;
}

//===========================================================================
static inline uint16_t IndirectY() {
    uint8_t  offset = g_pageRead[regs.pc >> 8][regs.pc & 0xff];
    uint16_t addr   = *(uint16_t *)(g_pageRead[0] + offset) + (uint16_t)regs.y;
    ++regs.pc;
    return addr;
}

//===========================================================================
static inline uint16_t IndirectY(int * cycles) {
    uint8_t  offset = g_pageRead[regs.pc >> 8][regs.pc & 0xff];
    uint16_t tmp    = *(uint16_t *)(g_pageRead[0] + offset);
    uint16_t addr   = tmp + uint16_t(regs.y);
    if ((tmp ^ addr) & 0xff00)
        ++*cycles;
    ++regs.pc;
    return addr;
}

//===========================================================================
static inline uint16_t IndirectZeroPage() {
    uint8_t  offset = g_pageRead[regs.pc >> 8][regs.pc & 0xff];
    uint16_t addr   = *(uint16_t *)(g_pageRead[0] + offset);
    ++regs.pc;
    return addr;
}

//===========================================================================
static inline uint16_t Relative() {
    int8_t offset = (int8_t)g_pageRead[regs.pc >> 8][regs.pc & 0xff];
    ++regs.pc;
    return regs.pc + offset;
}

//===========================================================================
static inline uint16_t ZeroPage() {
    uint16_t addr = g_pageRead[regs.pc >> 8][regs.pc & 0xff];
    ++regs.pc;
    return addr;
}

//===========================================================================
static inline uint16_t ZeroPageX() {
    uint16_t addr = uint8_t(g_pageRead[regs.pc >> 8][regs.pc & 0xff] + regs.x);
    ++regs.pc;
    return addr;
}

//===========================================================================
static inline uint16_t ZeroPageY() {
    uint16_t addr = uint8_t(g_pageRead[regs.pc >> 8][regs.pc & 0xff] + regs.y);
    ++regs.pc;
    return addr;
}


/****************************************************************************
*
*  Memory access helpers
*
***/

//===========================================================================
static inline void Branch(uint8_t condition, uint16_t addr, int * cycles) {
    if (condition) {
        uint16_t oldaddr = regs.pc;
        regs.pc = addr;
        *cycles += 1 + ((oldaddr ^ addr) ^ 0xff00) ? 1 : 0;
    }
}

//===========================================================================
static inline uint8_t Pop8() {
    regs.sp = ((regs.sp + 1) & 0xff) | 0x100;
    return g_pageRead[regs.sp >> 8][regs.sp & 0xff];
}

//===========================================================================
static inline uint16_t Pop16() {
    uint16_t addr0 = Pop8();
    uint16_t addr1 = Pop8();
    return addr0 | addr1 << 8;
}

//===========================================================================
static inline void Push8(uint8_t value) {
    g_pageWrite[regs.sp >> 8][regs.sp & 0xff] = value;
    regs.sp = (regs.sp - 1) | 0x100;
}

//===========================================================================
static inline uint8_t Read8(uint16_t addr) {
    if ((addr >> 8) == 0xc0)
        return MemIoRead(addr);
    else
        return g_pageRead[addr >> 8][addr & 0xff];
}

//===========================================================================
static inline uint16_t Read16(uint16_t addr) {
    return *(uint16_t *)(g_pageRead[addr >> 8] + (addr & 0xff));
}

//===========================================================================
static inline void Write8(uint16_t addr, uint8_t value) {
    if (addr >> 8 == 0xc0)
        MemIoWrite(addr, value);
    else
        g_pageWrite[addr >> 8][addr & 0xff] = value;
}


/****************************************************************************
*
*   Status bit helpers
*
***/

//===========================================================================
static inline void ResetFlag(uint8_t flag) {
    regs.ps &= ~flag;
}

//===========================================================================
static inline void SetFlag(uint8_t flag) {
    regs.ps |= flag;
}

//===========================================================================
static inline void SetFlagTo(uint8_t flag, int predicate) {
    if (predicate)
        regs.ps |= flag;
    else
        regs.ps &= ~flag;
}

//===========================================================================
static inline void UpdateNZ(uint8_t value) {
    SetFlagTo(PS_SIGN, value & 0x80);
    SetFlagTo(PS_ZERO, value == 0);
}


/****************************************************************************
*
*   Instruction implementations
*
***/

//===========================================================================
static inline void Adc6502(uint8_t val8) {
    uint32_t acc   = uint32_t(regs.a);
    uint32_t add   = uint32_t(val8);
    uint32_t carry = uint32_t(regs.ps & PS_CARRY);
    uint32_t newval;
    if (regs.ps & PS_DECIMAL) {
        uint32_t lo = (acc & 0x0f) + (add & 0x0f) + carry;
        uint32_t carrylo = 0;
        if (lo >= 0x0a) {
            carrylo = 0x10;
            lo -= 0x0a;
        }

        uint32_t hi = (acc & 0xf0) + (add & 0xf0) + carrylo;
        if (hi >= 0xa0) {
            SetFlag(PS_CARRY);
            hi -= 0xa0;
        }
        else
            ResetFlag(PS_CARRY);

        newval = hi | lo;
        SetFlagTo(PS_OVERFLOW, ((acc ^ newval) & 0x80) && !((acc ^ add) & 0x80));
    }
    else {
        newval = acc + add + carry;
        SetFlagTo(PS_CARRY, newval & 0x100);
        SetFlagTo(PS_OVERFLOW, (acc & 0x80) == (add & 0x80) && (acc & 0x80) != (newval & 0x80));
    }

    regs.a = uint8_t(newval);
    UpdateNZ(regs.a);
}

//===========================================================================
static inline void Alr(uint8_t val8) {
    regs.a &= val8;
    SetFlagTo(PS_CARRY, regs.a & 1);
    regs.a >>= 1;
    ResetFlag(PS_SIGN);
    SetFlagTo(PS_ZERO, !regs.a);
}

//===========================================================================
static inline void Anc(uint8_t val8) {
    regs.a &= val8;
    SetFlagTo(PS_CARRY, regs.a & 0x80);
    UpdateNZ(regs.a);
}

//===========================================================================
static inline void And(uint8_t val8) {
    regs.a &= val8;
    UpdateNZ(regs.a);
}

//===========================================================================
static inline void Arr(uint8_t val8) {
    regs.a &= val8;
    uint8_t loBit = regs.a & 1;
    regs.a = regs.a >> 1 | ((regs.ps & PS_CARRY) << 7);
    SetFlagTo(PS_CARRY, loBit);
    UpdateNZ(regs.a);
}

//===========================================================================
static inline uint8_t Asl(uint8_t val8) {
    SetFlagTo(PS_CARRY, val8 & 0x80);
    val8 <<= 1;
    UpdateNZ(val8);
    return val8;
}

//===========================================================================
static inline uint8_t Aso(uint8_t val8) {
    SetFlagTo(PS_CARRY, val8 & 0x80);
    val8 <<= 1;
    val8 |= regs.a;
    UpdateNZ(val8);
    return val8;
}

//===========================================================================
static inline uint8_t Axs(uint8_t val8) {
    return regs.a & regs.x;
}

//===========================================================================
static inline void Bit(uint8_t val8) {
    SetFlagTo(PS_ZERO, !(regs.a & val8));
    SetFlagTo(PS_SIGN, val8 & 0x80);
    SetFlagTo(PS_OVERFLOW, val8 & 0x40);
}

//===========================================================================
static inline void Brk() {
    Push8(regs.pc >> 8);
    Push8(regs.pc & 0xff);
    SetFlag(PS_BREAK);
    Push8(regs.ps);
    SetFlag(PS_INTERRUPT);
    regs.pc = Read16(0xfffe);
}

//===========================================================================
static inline void Cmp(uint8_t val8) {
    SetFlagTo(PS_CARRY, regs.a >= val8);
    UpdateNZ(regs.a - val8);
}

//===========================================================================
static inline void Cpx(uint8_t val8) {
    SetFlagTo(PS_CARRY, regs.x >= val8);
    UpdateNZ(regs.x - val8);
}

//===========================================================================
static inline void Cpy(uint8_t val8) {
    SetFlagTo(PS_CARRY, regs.y >= val8);
    UpdateNZ(regs.y - val8);
}

//===========================================================================
static inline uint8_t Dcm(uint8_t val8) {
    val8--;
    SetFlagTo(PS_CARRY, regs.a >= val8);
    UpdateNZ(regs.a - val8);
    return val8;
}

//===========================================================================
static inline uint8_t Dec(uint8_t val8) {
    val8--;
    UpdateNZ(val8);
    return val8;
}

//===========================================================================
static inline void Dex() {
    --regs.x;
    UpdateNZ(regs.x);
}

//===========================================================================
static inline void Dey() {
    --regs.y;
    UpdateNZ(regs.y);
}

//===========================================================================
static inline void Eor(uint8_t val8) {
    regs.a ^= val8;
    UpdateNZ(regs.a);
}

//===========================================================================
static inline uint8_t Inc(uint8_t val8) {
    ++val8;
    UpdateNZ(val8);
    return val8;
}

//===========================================================================
static inline uint8_t Ins(uint8_t val8) {
    val8 = regs.a - (val8 + 1);
    UpdateNZ(val8);
    return val8;
}

//===========================================================================
static inline void Inx() {
    ++regs.x;
    UpdateNZ(regs.x);
}

//===========================================================================
static inline void Iny() {
    ++regs.y;
    UpdateNZ(regs.y);
}

//===========================================================================
static inline void Jmp(uint16_t addr) {
    regs.pc = addr;
}

//===========================================================================
static inline void Jsr(uint16_t addr) {
    --regs.pc;
    Push8(regs.pc >> 8);
    Push8(regs.pc & 0xff);
    regs.pc = addr;
}

//===========================================================================
static inline void Lax(uint8_t val8) {
    regs.a = regs.x = val8;
    UpdateNZ(regs.a);
}

//===========================================================================
static inline void Lda(uint8_t val8) {
    regs.a = val8;
    UpdateNZ(regs.a);
}

//===========================================================================
static inline void Ldx(uint8_t val8) {
    regs.x = val8;
    UpdateNZ(regs.x);
}

//===========================================================================
static inline void Ldy(uint8_t val8) {
    regs.y = val8;
    UpdateNZ(regs.y);
}

//===========================================================================
static inline uint8_t Lse(uint8_t val8) {
    SetFlagTo(PS_CARRY, val8 & 1);
    ResetFlag(PS_SIGN);
    val8 >>= 1;
    val8 ^= regs.a;
    SetFlagTo(PS_ZERO, !val8);
    return val8;
}

//===========================================================================
static inline uint8_t Lsr(uint8_t val8) {
    SetFlagTo(PS_CARRY, val8 & 1);
    ResetFlag(PS_SIGN);
    val8 >>= 1;
    SetFlagTo(PS_ZERO, !val8);
    return val8;
}

//===========================================================================
static inline void Oal(uint8_t val8) {
    regs.a = regs.x = (regs.a | 0xee) & val8;
    UpdateNZ(regs.a);
}

//===========================================================================
static inline void Ora(uint8_t val8) {
    regs.a |= val8;
    UpdateNZ(regs.a);
}

//===========================================================================
static inline void Php() {
    SetFlag(PS_RESERVED);
    Push8(regs.ps);
}

//===========================================================================
static inline void Pla() {
    regs.a = Pop8();
    UpdateNZ(regs.a);
}

//===========================================================================
static inline uint8_t Rla(uint8_t val8) {
    uint8_t hiBit = val8 >> 7;
    val8 = val8 << 1 | (regs.ps & PS_CARRY);
    val8 &= regs.a;
    SetFlagTo(PS_CARRY, hiBit);
    UpdateNZ(val8);
    return val8;
}

//===========================================================================
static inline uint8_t Rol(uint8_t val8) {
    uint8_t hiBit = val8 >> 7;
    val8 = val8 << 1 | (regs.ps & PS_CARRY);
    SetFlagTo(PS_CARRY, hiBit);
    UpdateNZ(val8);
    return val8;
}

//===========================================================================
static inline uint8_t Ror(uint8_t val8) {
    uint8_t loBit = val8 & 1;
    val8 = val8 >> 1 | ((regs.ps & PS_CARRY) << 7);
    SetFlagTo(PS_CARRY, loBit);
    UpdateNZ(val8);
    return val8;
}

//===========================================================================
static inline uint8_t Rra(uint8_t val8) {
    uint8_t loBit = val8 & 1;
    val8 = val8 >> 1 | ((regs.ps & PS_CARRY) << 7);
    val8 &= regs.a;
    SetFlagTo(PS_CARRY, loBit);
    UpdateNZ(val8);
    return val8;
}

//===========================================================================
static inline void Rti() {
    regs.ps = Pop8();
    regs.pc = Pop16();
}

//===========================================================================
static inline void Rts() {
    regs.pc = Pop16() + 1;
}

//===========================================================================
static inline void Sax(uint8_t val8) {
    regs.x = (regs.a & regs.x) - val8;
    SetFlagTo(PS_CARRY, regs.x >= val8);
    UpdateNZ(regs.x - val8);
}

//===========================================================================
static inline void Sbc6502(uint8_t val8) {
    uint32_t acc   = uint32_t(regs.a);
    uint32_t sub   = uint32_t(val8);
    uint32_t carry = uint32_t(regs.ps & PS_CARRY);
    uint32_t newval;
    if (regs.ps & PS_DECIMAL) {
        uint32_t lo = 0x0f + (acc & 0x0f) - (sub & 0x0f) + carry;
        uint32_t carrylo;
        if (lo < 0x10) {
            lo -= 0x06;
            carrylo = 0;
        }
        else {
            lo -= 0x10;
            carrylo = 0x10;
        }

        uint32_t hi = 0xf0 + (acc & 0xf0) - (sub & 0xf0) + carrylo;
        if (hi < 0x100) {
            ResetFlag(PS_CARRY);
            hi -= 0x60;
        }
        else {
            SetFlag(PS_CARRY);
            hi -= 0x100;
        }

        newval = hi | lo;

        SetFlagTo(PS_OVERFLOW, ((acc ^ newval) & 0x80) && ((acc ^ sub) & 0x80));
    }
    else {
        newval = 0xff + acc - sub + carry;
        SetFlagTo(PS_CARRY, newval >= 0x100);
        SetFlagTo(PS_OVERFLOW, ((acc * 0x80) != (sub & 0x80)) && ((acc & 0x80) != (newval & 0x80)));
    }

    regs.a = uint8_t(newval);
    UpdateNZ(regs.a);
}

//===========================================================================
static inline void Tax() {
    regs.x = regs.a;
    UpdateNZ(regs.x);
}

//===========================================================================
static inline void Tay() {
    regs.y = regs.a;
    UpdateNZ(regs.y);
}

//===========================================================================
static inline uint8_t Trb(uint8_t val8) {
    SetFlagTo(PS_ZERO, !(regs.a & val8));
    val8 &= ~regs.a;
    return val8;
}

//===========================================================================
static inline uint8_t Tsb(uint8_t val8) {
    SetFlagTo(PS_ZERO, !(val8 & regs.a));
    val8 |= regs.a;
    return val8;
}

//===========================================================================
static inline void Tsx() {
    regs.x = uint8_t(regs.sp & 0xff);
    UpdateNZ(regs.x);
}

//===========================================================================
static inline void Txa() {
    regs.a = regs.x;
    UpdateNZ(regs.a);
}

//===========================================================================
static inline void Txs() {
    regs.sp = regs.x | 0x100;
}

//===========================================================================
static inline void Tya() {
    regs.a = regs.y;
    UpdateNZ(regs.a);
}

//===========================================================================
static inline void Xaa(uint8_t val8) {
    regs.a = (regs.x & val8);
    UpdateNZ(regs.a);
}


/****************************************************************************
*
*   CPU step functions
*
***/

//===========================================================================
static int CpuStep6502() {
    int      cycles = 0;
    uint16_t addr;
    uint8_t  val8;

    if (regs.halt)
        return 1000;

#if 0
    constexpr uint16_t DEBUGADDR = 0x1000;
    static bool s_logging = false;
    if (regs.pc >= DEBUGADDR && regs.pc <= DEBUGADDR + 0x100) {
        s_logging = true;
    }
    if (s_logging) {
        static int s_count;
        ++s_count;
        if (s_count == 2232) {
            int foo = 0;
        }
        static bool s_dumped = false;
        if (!s_dumped) {
            DebugDumpMemory("c:\\Temp\\Mem1000.bin", 0x1000, 0x0100);
            s_dumped = true;
        }
        DebugPrintf(
            "%8d: PC=%04X Op=%02X A=%02X X=%02X Y=%02X\n",
            s_count,
            regs.pc,
            g_pageRead[regs.pc >> 8][regs.pc & 0xff],
            regs.a,
            regs.x,
            regs.y
        );
    }
#endif

    uint8_t opcode = g_pageRead[regs.pc >> 8][regs.pc & 0xff];
    ++regs.pc;
    switch (opcode) {
        case 0x00: // BRK
            Brk();
            cycles += 7;
            break;
        case 0x01: // ORA (izx)
            addr = IndirectX();
            val8 = Read8(addr);
            Ora(val8);
            cycles += 6;
            break;
        case 0x02: // *HLT
            regs.halt = true;
            break;
        case 0x03: // *ASO (izx)
            addr = IndirectX();
            val8 = Read8(addr);
            val8 = Aso(val8);
            Write8(addr, val8);
            cycles += 8;
            break;
        case 0x04: // *SKB (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            cycles += 3;
            break;
        case 0x05: // ORA (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            Ora(val8);
            cycles += 3;
            break;
        case 0x06: // ASL (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            val8 = Asl(val8);
            Write8(addr, val8);
            cycles += 5;
            break;
        case 0x07: // *ASO (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            val8 = Aso(val8);
            Write8(addr, val8);
            cycles += 5;
            break;
        case 0x08: // PHP
            Php();
            cycles += 3;
            break;
        case 0x09: // ORA (imm)
            addr = Immediate();
            val8 = Read8(addr);
            Ora(val8);
            cycles += 2;
            break;
        case 0x0a: // ASL (acc)
            regs.a = Asl(regs.a);
            cycles += 2;
            break;
        case 0x0b: // *ANC (imm)
            addr = Immediate();
            val8 = Read8(addr);
            Anc(val8);
            cycles += 2;
            break;
        case 0x0c: // *SKW (abs)
            addr = Absolute();
            cycles += 3;
            val8 = Read8(addr);
            cycles += 1;
            break;
        case 0x0d: // ORA (abs)
            addr = Absolute();
            cycles += 3;
            val8 = Read8(addr);
            Ora(val8);
            cycles += 1;
            break;
        case 0x0e: // ASL (abs)
            addr = Absolute();
            cycles += 3;
            val8 = Read8(addr);
            cycles += 1;
            Write8(addr, val8);
            cycles += 1;
            val8 = Asl(val8);
            Write8(addr, val8);
            cycles += 1;
            break;
        case 0x0f: // *ASO (abs)
            addr = Absolute();
            val8 = Read8(addr);
            val8 = Aso(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0x10: // BPL
            addr = Relative();
            Branch(!(regs.ps & PS_SIGN), addr, &cycles);
            cycles += 2;
            break;
        case 0x11: // ORA (izy)
            addr = IndirectY(&cycles);
            val8 = Read8(addr);
            Ora(val8);
            cycles += 5;
            break;
        case 0x12: // *HLT
            regs.halt = true;
            break;
        case 0x13: // *ASO (izy)
            addr = IndirectY();
            val8 = Read8(addr);
            val8 = Aso(val8);
            Write8(addr, val8);
            cycles += 8;
            break;
        case 0x14: // *SKB (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            cycles += 4;
            break;
        case 0x15: // ORA (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            Ora(val8);
            cycles += 4;
            break;
        case 0x16: // ASL (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            val8 = Asl(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0x17: // *ASO (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            val8 = Aso(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0x18: // CLC
            ResetFlag(PS_CARRY);
            cycles += 1;
            break;
        case 0x19: // ORA (aby)
            addr = AbsoluteY(&cycles);
            val8 = Read8(addr);
            Ora(val8);
            cycles += 4;
            break;
        case 0x1a: // *NOP
            cycles += 2;
            break;
        case 0x1b: // *ASO (aby)
            addr = AbsoluteY();
            val8 = Read8(addr);
            val8 = Aso(val8);
            Write8(addr, val8);
            cycles += 7;
            break;
        case 0x1c: // *SKW (abx)
            addr = AbsoluteX(&cycles);
            val8 = Read8(addr);
            cycles += 4;
            break;
        case 0x1d: // ORA (abx)
            addr = AbsoluteX(&cycles);
            val8 = Read8(addr);
            Ora(val8);
            cycles += 4;
            break;
        case 0x1e: // ASL (abx)
            addr = AbsoluteX();
            val8 = Read8(addr);
            val8 = Asl(val8);
            Write8(addr, val8);
            cycles += 7;
            break;
        case 0x1f: // *ASO (abx)
            addr = AbsoluteX();
            val8 = Read8(addr);
            val8 = Aso(val8);
            Write8(addr, val8);
            cycles += 7;
            break;
        case 0x20: // JSR (abs)
            addr = Absolute();
            Jsr(addr);
            cycles += 6;
            break;
        case 0x21: // AND (izx)
            addr = IndirectX();
            val8 = Read8(addr);
            And(val8);
            cycles += 6;
            break;
        case 0x22: // *HLT
            regs.halt = true;
            break;
        case 0x23: // *RLA (izx)
            addr = IndirectX();
            val8 = Read8(addr);
            val8 = Rla(val8);
            Write8(addr, val8);
            cycles += 8;
            break;
        case 0x24: // BIT (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            Bit(val8);
            cycles += 3;
            break;
        case 0x25: // AND (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            And(val8);
            cycles += 3;
            break;
        case 0x26: // ROL (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            val8 = Rol(val8);
            Write8(addr, val8);
            cycles += 5;
            break;
        case 0x27: // *RLA (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            val8 = Rla(val8);
            Write8(addr, val8);
            cycles += 5;
            break;
        case 0x28: // PLP
            regs.ps = Pop8();
            cycles += 4;
            break;
        case 0x29: // AND (imm)
            addr = Immediate();
            val8 = Read8(addr);
            And(val8);
            cycles += 2;
            break;
        case 0x2a: // ROL (acc)
            regs.a = Rol(regs.a);
            cycles += 2;
            break;
        case 0x2b: // *ANC (imm)
            addr = Immediate();
            val8 = Read8(addr);
            Anc(val8);
            cycles += 2;
            break;
        case 0x2c: // BIT (abs)
            addr = Absolute();
            val8 = Read8(addr);
            Bit(val8);
            cycles += 4;
            break;
        case 0x2d: // AND (abs)
            addr = Absolute();
            val8 = Read8(addr);
            And(val8);
            cycles += 4;
            break;
        case 0x2e: // ROL (abs)
            addr = Absolute();
            val8 = Read8(addr);
            val8 = Rol(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0x2f: // *RLA (abs)
            addr = Absolute();
            val8 = Read8(addr);
            val8 = Rla(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0x30: // BMI
            addr = Relative();
            Branch(regs.ps & PS_SIGN, addr, &cycles);
            cycles += 2;
            break;
        case 0x31: // AND (izy)
            addr = IndirectY(&cycles);
            val8 = Read8(addr);
            And(val8);
            cycles += 5;
            break;
        case 0x32: // *HLT
            regs.halt = true;
            break;
        case 0x33: // *RLA (izy)
            addr = IndirectY();
            val8 = Read8(addr);
            val8 = Rla(val8);
            Write8(addr, val8);
            cycles += 8;
            break;
        case 0x34: // *SKB (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            cycles += 4;
            break;
        case 0x35: // AND (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            And(val8);
            cycles += 4;
            break;
        case 0x36: // ROL (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            val8 = Rol(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0x37: // *RLA (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            val8 = Rla(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0x38: // SEC
            SetFlag(PS_CARRY);
            cycles += 2;
            break;
        case 0x39: // AND (aby)
            addr = AbsoluteY(&cycles);
            val8 = Read8(addr);
            And(val8);
            cycles += 4;
            break;
        case 0x3a: // *NOP
            cycles += 2;
            break;
        case 0x3b: // *RLA (aby)
            addr = AbsoluteY();
            val8 = Read8(addr);
            val8 = Rla(val8);
            Write8(addr, val8);
            cycles += 7;
            break;
        case 0x3c: // *SKW (abx)
            addr = AbsoluteX(&cycles);
            val8 = Read8(addr);
            cycles += 4;
            break;
        case 0x3d: // AND (abx)
            addr = AbsoluteX(&cycles);
            val8 = Read8(addr);
            And(val8);
            cycles += 4;
            break;
        case 0x3e: // ROL (abx)
            addr = AbsoluteX();
            val8 = Read8(addr);
            val8 = Rol(val8);
            Write8(addr, val8);
            cycles += 7;
            break;
        case 0x3f: // *RLA (abx)
            addr = AbsoluteX();
            val8 = Read8(addr);
            val8 = Rla(val8);
            Write8(addr, val8);
            cycles += 7;
            break;
        case 0x40: // RTI
            Rti();
            cycles += 6;
            break;
        case 0x41: // EOR (izx)
            addr = IndirectX();
            val8 = Read8(addr);
            Eor(val8);
            cycles += 6;
            break;
        case 0x42: // *HLT
            regs.halt = true;
            break;
        case 0x43: // *LSE (izx)
            addr = IndirectX();
            val8 = Read8(addr);
            val8 = Lse(val8);
            Write8(addr, val8);
            cycles += 8;
            break;
        case 0x44: // *SKB (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            cycles += 3;
            break;
        case 0x45: // EOR (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            Eor(val8);
            cycles += 3;
            break;
        case 0x46: // LSR (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            val8 = Lsr(val8);
            Write8(addr, val8);
            cycles += 5;
            break;
        case 0x47: // *LSE (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            val8 = Lse(val8);
            Write8(addr, val8);
            cycles += 5;
            break;
        case 0x48: // PHA
            Push8(regs.a);
            cycles += 3;
            break;
        case 0x49: // EOR (imm)
            addr = Immediate();
            val8 = Read8(addr);
            Eor(val8);
            cycles += 2;
            break;
        case 0x4a: // LSR (acc)
            regs.a = Lsr(regs.a);
            cycles += 2;
            break;
        case 0x4b: // *ALR (imm)
            addr = Immediate();
            val8 = Read8(addr);
            Alr(val8);
            cycles += 2;
            break;
        case 0x4c: // JMP (abs)
            addr = Absolute();
            Jmp(addr);
            cycles += 3;
            break;
        case 0x4d: // EOR (abs)
            addr = Absolute();
            val8 = Read8(addr);
            Eor(val8);
            cycles += 4;
            break;
        case 0x4e: // LSR (abs)
            addr = Absolute();
            val8 = Read8(addr);
            val8 = Lsr(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0x4f: // *LSE (abs)
            addr = Absolute();
            val8 = Read8(addr);
            val8 = Lse(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0x50: // BVC
            addr = Relative();
            Branch(!(regs.ps & PS_OVERFLOW), addr, &cycles);
            cycles += 2;
            break;
        case 0x51: // EOR (izy)
            addr = IndirectY(&cycles);
            val8 = Read8(addr);
            Eor(val8);
            cycles += 5;
            break;
        case 0x52: // *HLT
            regs.halt = true;
            break;
        case 0x53: // *LSE (izy)
            addr = IndirectY();
            val8 = Read8(addr);
            val8 = Lse(val8);
            Write8(addr, val8);
            cycles += 8;
            break;
        case 0x54: // *SKB (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            cycles += 4;
            break;
        case 0x55: // EOR (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            Eor(val8);
            cycles += 4;
            break;
        case 0x56: // LSR (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            val8 = Lsr(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0x57: // *LSE (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            val8 = Lse(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0x58: // CLI
            ResetFlag(PS_INTERRUPT);
            cycles += 2;
            break;
        case 0x59: // EOR (aby)
            addr = AbsoluteY(&cycles);
            val8 = Read8(addr);
            Eor(val8);
            cycles += 4;
            break;
        case 0x5a: // *NOP
            cycles += 2;
            break;
        case 0x5b: // *LSE (aby)
            addr = AbsoluteY();
            val8 = Read8(addr);
            val8 = Lse(val8);
            Write8(addr, val8);
            cycles += 7;
            break;
        case 0x5c: // *SKW (abx)
            addr = AbsoluteX(&cycles);
            val8 = Read8(addr);
            cycles += 4;
            break;
        case 0x5d: // EOR (abx)
            addr = AbsoluteX(&cycles);
            val8 = Read8(addr);
            Eor(val8);
            cycles += 4;
            break;
        case 0x5e: // LSR (abx)
            addr = AbsoluteX();
            val8 = Read8(addr);
            val8 = Lsr(val8);
            Write8(addr, val8);
            cycles += 7;
            break;
        case 0x5f: // *LSE (abx)
            addr = AbsoluteX();
            val8 = Read8(addr);
            val8 = Lse(val8);
            Write8(addr, val8);
            cycles += 7;
            break;
        case 0x60: // RTS
            Rts();
            cycles += 6;
            break;
        case 0x61: // ADC (izx)
            addr = IndirectX();
            val8 = Read8(addr);
            Adc6502(val8);
            cycles += 6;
            break;
        case 0x62: // *HLT
            regs.halt = true;
            break;
        case 0x63: // *RRA (izx)
            addr = IndirectX();
            val8 = Read8(addr);
            val8 = Rra(val8);
            Write8(addr, val8);
            cycles += 8;
            break;
        case 0x64: // *SKB (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            cycles += 3;
            break;
        case 0x65: // ADC (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            Adc6502(val8);
            cycles += 3;
            break;
        case 0x66: // ROR (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            val8 = Ror(val8);
            Write8(addr, val8);
            cycles += 5;
            break;
        case 0x67: // *RRA (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            val8 = Rra(val8);
            Write8(addr, val8);
            cycles += 5;
            break;
        case 0x68: // PLA
            Pla();
            cycles += 4;
            break;
        case 0x69: // ADC (imm)
            addr = Immediate();
            val8 = Read8(addr);
            Adc6502(val8);
            cycles += 2;
            break;
        case 0x6a: // ROR (acc)
            regs.a = Ror(regs.a);
            cycles += 2;
            break;
        case 0x6b: // *ARR (imm)
            addr = Immediate();
            val8 = Read8(addr);
            Arr(val8);
            cycles += 2;
            break;
        case 0x6c: // JMP (ind)
            addr = Indirect6502();
            Jmp(addr);
            cycles += 6;
            break;
        case 0x6d: // ADC (abs)
            addr = Absolute();
            val8 = Read8(addr);
            Adc6502(val8);
            cycles += 4;
            break;
        case 0x6e: // ROR (abs)
            addr = Absolute();
            val8 = Read8(addr);
            val8 = Ror(val8);
            Write8(addr, val8);
            cycles += 7;
            break;
        case 0x6f: // *RRA (abs)
            addr = Absolute();
            val8 = Read8(addr);
            val8 = Rra(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0x70: // BVS
            addr = Relative();
            Branch(regs.ps & PS_OVERFLOW, addr, &cycles);
            cycles += 2;
            break;
        case 0x71: // ADC (izy)
            addr = IndirectY(&cycles);
            val8 = Read8(addr);
            Adc6502(val8);
            cycles += 5;
            break;
        case 0x72: // *HLT
            regs.halt = true;
            break;
        case 0x73: // *RRA (izy)
            addr = IndirectY();
            val8 = Read8(addr);
            val8 = Rra(val8);
            Write8(addr, val8);
            cycles += 8;
            break;
        case 0x74: // *SKB (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            cycles += 4;
            break;
        case 0x75: // ADC (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            Adc6502(val8);
            cycles += 4;
            break;
        case 0x76: // ROR (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            val8 = Ror(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0x77: // *RRA (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            val8 = Rra(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0x78: // SEI
            SetFlag(PS_INTERRUPT);
            cycles += 2;
            break;
        case 0x79: // ADC (aby)
            addr = AbsoluteY(&cycles);
            val8 = Read8(addr);
            Adc6502(val8);
            cycles += 4;
            break;
        case 0x7a: // *NOP
            cycles += 2;
            break;
        case 0x7b: // *RRA (aby)
            addr = AbsoluteY();
            val8 = Read8(addr);
            val8 = Rra(val8);
            Write8(addr, val8);
            cycles += 7;
            break;
        case 0x7c: // *SKW (abx)
            addr = AbsoluteX(&cycles);
            val8 = Read8(addr);
            cycles += 4;
            break;
        case 0x7d: // ADC (abx)
            addr = AbsoluteX(&cycles);
            val8 = Read8(addr);
            Adc6502(val8);
            cycles += 4;
            break;
        case 0x7e: // ROR (abx)
            addr = AbsoluteX();
            val8 = Read8(addr);
            val8 = Ror(val8);
            Write8(addr, val8);
            cycles += 7;
            break;
        case 0x7f: // *RRA (abx)
            addr = AbsoluteX();
            val8 = Read8(addr);
            val8 = Rra(val8);
            Write8(addr, val8);
            cycles += 7;
            break;
        case 0x80: // *SKB (imm)
            addr = Immediate();
            val8 = Read8(addr);
            cycles += 2;
            break;
        case 0x81: // STA (izx)
            addr = IndirectX();
            Write8(addr, regs.a);
            cycles += 6;
            break;
        case 0x82: // *SKB (imm)
            addr = Immediate();
            val8 = Read8(addr);
            cycles += 2;
            break;
        case 0x83: // *AXS (izx)
            addr = IndirectX();
            val8 = Read8(addr);
            val8 = Axs(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0x84: // STY (zp)
            addr = ZeroPage();
            Write8(addr, regs.y);
            cycles += 3;
            break;
        case 0x85: // STA (zp)
            addr = ZeroPage();
            Write8(addr, regs.a);
            cycles += 3;
            break;
        case 0x86: // STX (zp)
            addr = ZeroPage();
            Write8(addr, regs.x);
            cycles += 3;
            break;
        case 0x87: // *AXS (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            val8 = Axs(val8);
            Write8(addr, val8);
            cycles += 3;
            break;
        case 0x88: // DEY
            Dey();
            cycles += 2;
            break;
        case 0x89: // *SKB (imm)
            addr = Immediate();
            val8 = Read8(addr);
            cycles += 2;
            break;
        case 0x8a: // TXA
            Txa();
            cycles += 2;
            break;
        case 0x8b: // *XAA
            addr = Immediate();
            val8 = Read8(addr);
            Xaa(val8);
            cycles += 2;
            break;
        case 0x8c: // STY (abs)
            addr = Absolute();
            Write8(addr, regs.y);
            cycles += 4;
            break;
        case 0x8d: // STA (abs)
            addr = Absolute();
            cycles += 3;
            Write8(addr, regs.a);
            cycles += 1;
            break;
        case 0x8e: // STX (abs)
            addr = Absolute();
            Write8(addr, regs.x);
            cycles += 4;
            break;
        case 0x8f: // *AXS (abs)
            addr = Absolute();
            val8 = Read8(addr);
            val8 = Axs(val8);
            Write8(addr, val8);
            cycles += 4;
            break;
        case 0x90: // BCC
            addr = Relative();
            Branch(!(regs.ps & PS_CARRY), addr, &cycles);
            cycles += 2;
            break;
        case 0x91: // STA (izy)
            addr = IndirectY(&cycles);
            Write8(addr, regs.a);
            cycles += 6;
            break;
        case 0x92: // *HLT
            regs.halt = true;
            break;
        case 0x94: // STY (zpx)
            addr = ZeroPageX();
            Write8(addr, regs.y);
            cycles += 4;
            break;
        case 0x95: // STA (zpx)
            addr = ZeroPageX();
            Write8(addr, regs.a);
            cycles += 4;
            break;
        case 0x96: // STX (zpy)
            addr = ZeroPageY();
            Write8(addr, regs.x);
            cycles += 4;
            break;
        case 0x97: // *AXS (zpy)
            addr = ZeroPageY();
            val8 = Read8(addr);
            val8 = Axs(val8);
            Write8(addr, val8);
            cycles += 4;
            break;
        case 0x98: // TYA
            Tya();
            cycles += 2;
            break;
        case 0x99: // STA (aby)
            addr = AbsoluteY();
            Write8(addr, regs.a);
            cycles += 5;
            break;
        case 0x9a: // TXS
            Txs();
            cycles += 2;
            break;
        case 0x9b: // *TAS (aby)
            addr = AbsoluteY(&cycles);
            // @@@ WRITE ME
            cycles += 5;
            break;
        case 0x9d: // STA (abx)
            addr = AbsoluteX();
            Write8(addr, regs.a);
            cycles += 5;
            break;
        case 0xa0: // LDY (imm)
            addr = Immediate();
            val8 = Read8(addr);
            Ldy(val8);
            cycles += 2;
            break;
        case 0xa1: // LDA (izx)
            addr = IndirectX();
            val8 = Read8(addr);
            Lda(val8);
            cycles += 6;
            break;
        case 0xa2: // LDX (imm)
            addr = Immediate();
            val8 = Read8(addr);
            Ldx(val8);
            cycles += 2;
            break;
        case 0xa3: // *LAX (izx)
            addr = IndirectX();
            val8 = Read8(addr);
            Lax(val8);
            cycles += 6;
            break;
        case 0xa4: // LDY (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            Ldy(val8);
            cycles += 3;
            break;
        case 0xa5: // LDA (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            Lda(val8);
            cycles += 3;
            break;
        case 0xa6: // LDX (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            Ldx(val8);
            cycles += 3;
            break;
        case 0xa7: // *LAX (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            Lax(val8);
            cycles += 3;
            break;
        case 0xa8: // TAY
            Tay();
            cycles += 2;
            break;
        case 0xa9: // LDA (imm)
            addr = Immediate();
            val8 = Read8(addr);
            Lda(val8);
            cycles += 2;
            break;
        case 0xaa: // TAX
            Tax();
            cycles += 2;
            break;
        case 0xab: // *OAL (imm)
            addr = Immediate();
            val8 = Read8(addr);
            Oal(val8);
            cycles += 2;
            break;
        case 0xac: // LDY (abs)
            addr = Absolute();
            cycles += 3;
            val8 = Read8(addr);
            Ldy(val8);
            cycles += 1;
            break;
        case 0xad: // LDA (abs)
            addr = Absolute();
            cycles += 3;
            val8 = Read8(addr);
            Lda(val8);
            cycles += 1;
            break;
        case 0xae: // LDX (abs)
            addr = Absolute();
            val8 = Read8(addr);
            Ldx(val8);
            cycles += 4;
            break;
        case 0xaf: // *LAX (abs)
            addr = Absolute();
            val8 = Read8(addr);
            Lax(val8);
            cycles += 4;
            break;
        case 0xb0: // BCS
            addr = Relative();
            Branch(regs.ps & PS_CARRY, addr, &cycles);
            cycles += 2;
            break;
        case 0xb1: // LDA (izy)
            addr = IndirectY(&cycles);
            val8 = Read8(addr);
            Lda(val8);
            cycles += 5;
            break;
        case 0xb2: // *HLT
            regs.halt = true;
            break;
        case 0xb3: // *LAX (izy)
            addr = IndirectY(&cycles);
            val8 = Read8(addr);
            Lax(val8);
            cycles += 5;
            break;
        case 0xb4: // LDY (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            Ldy(val8);
            cycles += 4;
            break;
        case 0xb5: // LDA (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            Lda(val8);
            cycles += 4;
            break;
        case 0xb6: // LDX (zpy)
            addr = ZeroPageY();
            val8 = Read8(addr);
            Ldx(val8);
            cycles += 4;
            break;
        case 0xb7: // *LAX (zpy)
            addr = ZeroPageY();
            val8 = Read8(addr);
            Lax(val8);
            cycles += 4;
            break;
        case 0xb8: // CLV
            ResetFlag(PS_OVERFLOW);
            cycles += 2;
            break;
        case 0xb9: // LDA (aby)
            addr = AbsoluteY(&cycles);
            val8 = Read8(addr);
            Lda(val8);
            cycles += 4;
            break;
        case 0xba: // TSX
            Tsx();
            cycles += 2;
            break;
        case 0xbc: // LDY (abx)
            addr = AbsoluteX(&cycles);
            val8 = Read8(addr);
            Ldy(val8);
            cycles += 4;
            break;
        case 0xbd: // LDA (abx)
            addr = AbsoluteX(&cycles);
            val8 = Read8(addr);
            Lda(val8);
            cycles += 4;
            break;
        case 0xbe: // LDX (aby)
            addr = AbsoluteY(&cycles);
            val8 = Read8(addr);
            Ldx(val8);
            cycles += 4;
            break;
        case 0xbf: // *LAX (aby)
            addr = IndirectY(&cycles);
            val8 = Read8(addr);
            Lax(val8);
            cycles += 4;
            break;
        case 0xc0: // CPY (imm)
            addr = Immediate();
            val8 = Read8(addr);
            Cpy(val8);
            cycles += 2;
            break;
        case 0xc1: // CMP (izx)
            addr = IndirectX();
            val8 = Read8(addr);
            Cmp(val8);
            cycles += 6;
            break;
        case 0xc2: // *SKB (imm)
            addr = Immediate();
            val8 = Read8(addr);
            cycles += 2;
            break;
        case 0xc3: // *DCM (izx)
            addr = IndirectX();
            val8 = Read8(addr);
            val8 = Dcm(val8);
            Write8(addr, val8);
            cycles += 8;
            break;
        case 0xc4: // CPY (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            Cpy(val8);
            cycles += 3;
            break;
        case 0xc5: // CMP (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            Cmp(val8);
            cycles += 3;
            break;
        case 0xc6: // DEC (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            val8 = Dec(val8);
            Write8(addr, val8);
            cycles += 5;
            break;
        case 0xc7: // *DCM (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            val8 = Dcm(val8);
            Write8(addr, val8);
            cycles += 5;
            break;
        case 0xc8: // INY
            Iny();
            cycles += 2;
            break;
        case 0xc9: // CMP (imm)
            addr = Immediate();
            val8 = Read8(addr);
            Cmp(val8);
            cycles += 2;
            break;
        case 0xca: // DEX
            Dex();
            cycles += 2;
            break;
        case 0xcb: // *SAX (imm)
            addr = Immediate();
            val8 = Read8(addr);
            Sax(val8);
            cycles += 2;
            break;
        case 0xcc: // CPY (abs)
            addr = Absolute();
            val8 = Read8(addr);
            Cpy(val8);
            cycles += 4;
            break;
        case 0xcd: // CMP (abs)
            addr = Absolute();
            val8 = Read8(addr);
            Cmp(val8);
            cycles += 4;
            break;
        case 0xce: // DEC (abs)
            addr = Absolute();
            val8 = Read8(addr);
            val8 = Dec(val8);
            Write8(addr, val8);
            cycles += 5;
            break;
        case 0xcf: // *DCM (abs)
            addr = Absolute();
            val8 = Read8(addr);
            val8 = Dcm(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0xd0: // BNE
            addr = Relative();
            Branch(!(regs.ps & PS_ZERO), addr, &cycles);
            cycles += 2;
            break;
        case 0xd1: // CMP (izy)
            addr = IndirectY(&cycles);
            val8 = Read8(addr);
            Cmp(val8);
            cycles += 5;
            break;
        case 0xd2: // *HLT
            regs.halt = true;
            break;
        case 0xd3: // *DCM (izy)
            addr = IndirectY();
            val8 = Read8(addr);
            val8 = Dcm(val8);
            Write8(addr, val8);
            cycles += 8;
            break;
        case 0xd4: // *SKB (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            cycles += 4;
            break;
        case 0xd5: // CMP (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            Cmp(val8);
            cycles += 4;
            break;
        case 0xd6: // DEC (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            val8 = Dec(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0xd7: // *DCM (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            val8 = Dcm(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0xd8: // CLD
            ResetFlag(PS_DECIMAL);
            cycles += 2;
            break;
        case 0xd9: // CMP (aby)
            addr = AbsoluteY(&cycles);
            val8 = Read8(addr);
            Cmp(val8);
            cycles += 4;
            break;
        case 0xda: // *NOP
            cycles += 2;
            break;
        case 0xdb: // *DCM (aby)
            addr = AbsoluteY();
            val8 = Read8(addr);
            val8 = Dcm(val8);
            Write8(addr, val8);
            cycles += 7;
            break;
        case 0xdc: // *SKW (abx)
            addr = AbsoluteX(&cycles);
            val8 = Read8(addr);
            cycles += 4;
            break;
        case 0xdd: // CMP (abx)
            addr = AbsoluteX(&cycles);
            val8 = Read8(addr);
            Cmp(val8);
            cycles += 4;
            break;
        case 0xde: // DEC (abx)
            addr = AbsoluteX();
            val8 = Read8(addr);
            val8 = Dec(val8);
            Write8(addr, val8);
            cycles += 7;
            break;
        case 0xdf: // *DCM (abx)
            addr = AbsoluteX();
            val8 = Read8(addr);
            val8 = Dcm(val8);
            Write8(addr, val8);
            cycles += 7;
            break;
        case 0xe0: // CPX (imm)
            addr = Immediate();
            val8 = Read8(addr);
            Cpx(val8);
            cycles += 2;
            break;
        case 0xe1: // SBC (izx)
            addr = IndirectX();
            val8 = Read8(addr);
            Sbc6502(val8);
            cycles += 6;
            break;
        case 0xe2: // *SKB (imm)
            addr = Immediate();
            val8 = Read8(addr);
            cycles += 2;
            break;
        case 0xe3: // *INS (izx)
            addr = IndirectX();
            val8 = Read8(addr);
            val8 = Ins(val8);
            Write8(addr, val8);
            cycles += 8;
            break;
        case 0xe4: // CPX (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            Cpx(val8);
            cycles += 3;
            break;
        case 0xe5: // SBC (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            Sbc6502(val8);
            cycles += 3;
            break;
        case 0xe6: // INC (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            val8 = Inc(val8);
            Write8(addr, val8);
            cycles += 5;
            break;
        case 0xe7: // *INS (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            val8 = Ins(val8);
            Write8(addr, val8);
            cycles += 5;
            break;
        case 0xe8: // INX
            Inx();
            cycles += 2;
            break;
        case 0xe9: // SBC (imm)
            addr = Immediate();
            val8 = Read8(addr);
            Sbc6502(val8);
            cycles += 2;
            break;
        case 0xea: // NOP
            cycles += 2;
            break;
        case 0xec: // CPX (abs)
            addr = Absolute();
            val8 = Read8(addr);
            Cpx(val8);
            cycles += 4;
            break;
        case 0xed: // SBC (abs)
            addr = Absolute();
            val8 = Read8(addr);
            Sbc6502(val8);
            cycles += 4;
            break;
        case 0xee: // INC (abs)
            addr = Absolute();
            val8 = Read8(addr);
            val8 = Inc(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0xef: // *INS (abs)
            addr = Absolute();
            val8 = Read8(addr);
            val8 = Ins(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0xf0: // BEQ
            addr = Relative();
            Branch(regs.ps & PS_ZERO, addr, &cycles);
            cycles += 2;
            break;
        case 0xf1: // SBC (izy)
            addr = IndirectY(&cycles);
            val8 = Read8(addr);
            Sbc6502(val8);
            cycles += 5;
            break;
        case 0xf2: // *HLT
            regs.halt = true;
            break;
        case 0xf3: // *INS (izy)
            addr = IndirectY();
            val8 = Read8(addr);
            val8 = Ins(val8);
            Write8(addr, val8);
            cycles += 8;
            break;
        case 0xf4: // *SKB (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            cycles += 4;
            break;
        case 0xf5: // SBC (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            Sbc6502(val8);
            cycles += 4;
            break;
        case 0xf6: // INC (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            val8 = Inc(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0xf7: // *INS (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            val8 = Ins(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0xf8: // SED
            SetFlag(PS_DECIMAL);
            cycles += 2;
            break;
        case 0xf9: // SBC (aby)
            addr = AbsoluteY(&cycles);
            val8 = Read8(addr);
            Sbc6502(val8);
            cycles += 4;
            break;
        case 0xfa: // *NOP
            cycles += 2;
            break;
        case 0xfc: // *SKW (abx)
            addr = AbsoluteX(&cycles);
            val8 = Read8(addr);
            cycles += 4;
            break;
        case 0xfd: // SBC (abx)
            addr = AbsoluteX(&cycles);
            val8 = Read8(addr);
            Sbc6502(val8);
            cycles += 4;
            break;
        case 0xfe: // INC (abx)
            addr = AbsoluteX();
            val8 = Read8(addr);
            val8 = Inc(val8);
            Write8(addr, val8);
            cycles += 7;
            break;
        case 0xff: // *INS (abx)
            addr = AbsoluteX();
            val8 = Read8(addr);
            val8 = Ins(val8);
            Write8(addr, val8);
            cycles += 7;
            break;

        default:
            cycles++;
            break;
    }

    return cycles;
}

/****************************************************************************
*
*   Public functions
*
***/

//===========================================================================
ECpuType CpuGetType() {
    return s_cpuType;
}

//===========================================================================
void CpuInitialize() {
    regs.a  = 0;
    regs.x  = 0;
    regs.y  = 0;
    regs.ps = PS_RESERVED;
    regs.pc = *(uint16_t *)(g_pageRead[0xff] + 0xfc);
    regs.sp = 0x01ff;
}

//===========================================================================
void CpuSetType (ECpuType type) {
    s_cpuType = type;
}

//===========================================================================
int CpuStep() {
    if (s_cpuType == CPU_TYPE_6502)
        return CpuStep6502();
    else
        return CpuStep6502();   // TODO: Write CpuStep65C02
}
