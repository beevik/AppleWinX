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

constexpr int SHORTOPCODES = 22;
constexpr int BENCHOPCODES = 33;

bool      cpuKill = false;
registers regs = { 0 };

static ECpuType cpuType;

static const BYTE benchopcode[BENCHOPCODES] = {
    0x06, 0x16, 0x24, 0x45, 0x48, 0x65, 0x68, 0x76,
    0x84, 0x85, 0x86, 0x91, 0x94, 0xA4, 0xA5, 0xA6,
    0xB1, 0xB4, 0xC0, 0xC4, 0xC5, 0xE6, 0x19, 0x6D,
    0x8D, 0x99, 0x9D, 0xAD, 0xB9, 0xBD, 0xDD, 0xED,
    0xEE
};

#if 0
struct State {
    registers reg;
    uint8_t   opcode;
};

static State state[256];
static int stateindex = 0;
const int statestart = 50000;
#endif


/****************************************************************************
*
*  GENERAL PURPOSE MACROS
*
***/

#define AF_TO_EF() flagc = (regs.ps & PS_CARRY);                            \
                   flagn = (regs.ps & PS_SIGN);                             \
                   flagv = (regs.ps & PS_OVERFLOW);                         \
                   flagz = (regs.ps & PS_ZERO)
#define EF_TO_AF() regs.ps = (regs.ps & ~(PS_CARRY | PS_SIGN |              \
                                         PS_OVERFLOW | PS_ZERO))            \
                              | (flagc ? PS_CARRY    : 0)                   \
                              | (flagn ? PS_SIGN     : 0)                   \
                              | (flagv ? PS_OVERFLOW : 0)                   \
                              | (flagz ? PS_ZERO     : 0)
#define CMOS     if (EmulatorGetAppleType() != APPLE_TYPE_IIE) { ++*cyclecounter; break; }
#define CYC(a)   *cyclecounter += (a)
#define POP()    (mem[regs.sp >= 0x1FF ? (regs.sp = 0x100) : ++regs.sp])
#define PUSH(a)  mem[regs.sp--] = (a);                                      \
                 if (regs.sp < 0x100) { regs.sp = 0x1FF; }
#define READ()   ((addr & 0xFF00) == 0xC000                                 \
                    ? ioRead[addr & 0xFF](regs.pc,(BYTE)addr,0,0)           \
                    : mem[addr])
#define SETNZ(a) {                                                          \
                   flagn = ((a) & 0x80);                                    \
                   flagz = !((a) & 0xFF);                                   \
                 }
#define SETZ(a)  flagz = !((a) & 0xFF)
#define TOBCD(a) (((((a) / 10) % 10) << 4) | ((a) % 10))
#define TOBIN(a) (((a) >> 4) * 10 + ((a) & 0x0F))
#define WRITE(a) {                                                          \
                   memDirty[addr >> 8] = 0xFF;                              \
                   LPBYTE page = memWrite[0][addr >> 8];                    \
                   if (page)                                                \
                     page[addr & 0xFF] = (BYTE)(a);                         \
                   else if ((addr & 0xFF00) == 0xC000)                      \
                     ioWrite[addr & 0xFF](regs.pc,(BYTE)addr,1,(BYTE)(a));  \
                 }


/****************************************************************************
*
*  ADDRESSING MODE MACROS
*
***/

#define ABS    addr = *(LPWORD)(mem + regs.pc); regs.pc += 2;
#define ABSX   addr = *(LPWORD)(mem + regs.pc) + (WORD)regs.x; regs.pc += 2;
#define ABSY   addr = *(LPWORD)(mem + regs.pc) + (WORD)regs.y; regs.pc += 2;
#define IABS   addr = *(LPWORD)(mem + *(LPWORD)(mem + regs.pc)); regs.pc += 2;
#define IABSX  addr = *(LPWORD)(mem + *(LPWORD)(mem + regs.pc) + (WORD)regs.x); regs.pc += 2;
#define IMM    addr = regs.pc++;
#define INDX   addr = *(LPWORD)(mem + ((mem[regs.pc++] + regs.x) & 0xFF));
#define INDY   addr = *(LPWORD)(mem + mem[regs.pc++]) + (WORD)regs.y;
#define IND    addr = *(LPWORD)(mem + mem[regs.pc++]);
#define REL    addr = (signed char)mem[regs.pc++];
#define ZPG    addr = mem[regs.pc++];
#define ZPGX   addr = (mem[regs.pc++] + regs.x) & 0xFF;
#define ZPGY   addr = (mem[regs.pc++] + regs.y) & 0xFF;


/****************************************************************************
*
*  INSTRUCTION MACROS
*
***/

#define ADC      temp = READ();                                             \
                 if (regs.ps & PS_DECIMAL) {                                \
                   val    = TOBIN(regs.a)+TOBIN(temp)+(flagc != 0);         \
                   flagc  = (val > 99);                                     \
                   regs.a = TOBCD(val);                                     \
                   if (EmulatorGetAppleType() == APPLE_TYPE_IIE)            \
                     SETNZ(regs.a);                                         \
                 }                                                          \
                 else {                                                     \
                   val    = regs.a+temp+(flagc != 0);                       \
                   flagc  = (val > 0xFF);                                   \
                   flagv  = (((regs.a & 0x80) == (temp & 0x80)) &&          \
                             ((regs.a & 0x80) != (val & 0x80)));            \
                   regs.a = val & 0xFF;                                     \
                   SETNZ(regs.a);                                           \
                 }
#define AND      regs.a &= READ();                                          \
                 SETNZ(regs.a);
#define ASL      val   = READ() << 1;                                       \
                 flagc = (val > 0xFF);                                      \
                 SETNZ(val);                                                \
                 WRITE(val);
#define ASLA     val   = regs.a << 1;                                       \
                 flagc = (val > 0xFF);                                      \
                 SETNZ(val);                                                \
                 regs.a = (BYTE)val;
#define BCC      if (!flagc) regs.pc += addr;
#define BCS      if ( flagc) regs.pc += addr;
#define BEQ      if ( flagz) regs.pc += addr;
#define BIT      val   = READ();                                            \
                 flagz = !(regs.a & val);                                   \
                 flagn = val & 0x80;                                        \
                 flagv = val & 0x40;
#define BITI     flagz = !(regs.a & READ());
#define BMI      if ( flagn) regs.pc += addr;
#define BNE      if (!flagz) regs.pc += addr;
#define BPL      if (!flagn) regs.pc += addr;
#define BRA      regs.pc += addr;
#define BRK      PUSH(regs.pc >> 8)                                         \
                 PUSH(regs.pc & 0xFF)                                       \
                 EF_TO_AF();                                                \
                 regs.ps |= PS_BREAK;                                       \
                 PUSH(regs.ps)                                              \
                 regs.ps |= PS_INTERRUPT;                                   \
                 regs.pc = *(LPWORD)(mem+0xFFFE);
#define BVC      if (!flagv) regs.pc += addr;
#define BVS      if ( flagv) regs.pc += addr;
#define CLC      flagc = 0;
#define CLD      regs.ps &= ~PS_DECIMAL;
#define CLI      regs.ps &= ~PS_INTERRUPT;
#define CLV      flagv = 0;
#define CMP      val   = READ();                                            \
                 flagc = (regs.a >= val);                                   \
                 val   = regs.a-val;                                        \
                 SETNZ(val);
#define CPX      val   = READ();                                            \
                 flagc = (regs.x >= val);                                   \
                 val   = regs.x-val;                                        \
                 SETNZ(val);
#define CPY      val   = READ();                                            \
                 flagc = (regs.y >= val);                                   \
                 val   = regs.y-val;                                        \
                 SETNZ(val);
#define DEA      --regs.a;                                                  \
                 SETNZ(regs.a);
#define DEC      val = READ()-1;                                            \
                 SETNZ(val);                                                \
                 WRITE(val);
#define DEX      --regs.x;                                                  \
                 SETNZ(regs.x);
#define DEY      --regs.y;                                                  \
                 SETNZ(regs.y);
#define EOR      regs.a ^= READ();                                          \
                 SETNZ(regs.a);
#define INA      ++regs.a;                                                  \
                 SETNZ(regs.a);
#define INC      val = READ()+1;                                            \
                 SETNZ(val);                                                \
                 WRITE(val);
#define INX      ++regs.x;                                                  \
                 SETNZ(regs.x);
#define INY      ++regs.y;                                                  \
                 SETNZ(regs.y);
#define JMP      regs.pc = addr;
#define JSR      --regs.pc;                                                 \
                 PUSH(regs.pc >> 8)                                         \
                 PUSH(regs.pc & 0xFF)                                       \
                 regs.pc = addr;
#define LDA      regs.a = READ();                                           \
                 SETNZ(regs.a);
#define LDX      regs.x = READ();                                           \
                 SETNZ(regs.x);
#define LDY      regs.y = READ();                                           \
                 SETNZ(regs.y);
#define LSR      val   = READ();                                            \
                 flagc = (val & 1);                                         \
                 flagn = 0;                                                 \
                 val >>= 1;                                                 \
                 SETZ(val);                                                 \
                 WRITE(val);
#define LSRA     flagc = (regs.a & 1);                                      \
                 flagn = 0;                                                 \
                 regs.a >>= 1;                                              \
                 SETZ(regs.a);
#define NOP
#define ORA      regs.a |= READ();                                          \
                 SETNZ(regs.a);
#define PHA      PUSH(regs.a)
#define PHP      EF_TO_AF();                                                \
                 regs.ps |= PS_RESERVED;                                    \
                 PUSH(regs.ps)
#define PHX      PUSH(regs.x)
#define PHY      PUSH(regs.y)
#define PLA      regs.a = POP();                                            \
                 SETNZ(regs.a);
#define PLP      regs.ps = POP();                                           \
                 AF_TO_EF();
#define PLX      regs.x = POP();                                            \
                 SETNZ(regs.x);
#define PLY      regs.y = POP();                                            \
                 SETNZ(regs.y);
#define ROL      val   = (READ() << 1) | (flagc != 0);                      \
                 flagc = (val > 0xFF);                                      \
                 SETNZ(val);                                                \
                 WRITE(val);
#define ROLA     val    = (((WORD)regs.a) << 1) | (flagc != 0);             \
                 flagc  = (val > 0xFF);                                     \
                 regs.a = val & 0xFF;                                       \
                 SETNZ(regs.a);
#define ROR      temp  = READ();                                            \
                 val   = (temp >> 1) | (flagc ? 0x80 : 0);                  \
                 flagc = temp & 1;                                          \
                 SETNZ(val);                                                \
                 WRITE(val);
#define RORA     val    = (((WORD)regs.a) >> 1) | (flagc ? 0x80 : 0);       \
                 flagc  = regs.a & 1;                                       \
                 regs.a = val & 0xFF;                                       \
                 SETNZ(regs.a);
#define RTI      regs.ps = POP();                                           \
                 AF_TO_EF();                                                \
                 regs.pc = POP();                                           \
                 regs.pc |= (((WORD)POP()) << 8);
#define RTS      regs.pc = POP();                                           \
                 regs.pc |= (((WORD)POP()) << 8);                           \
                 ++regs.pc;
#define SBC      temp = READ();                                             \
                 if (regs.ps & PS_DECIMAL) {                                \
                   val    = TOBIN(regs.a)-TOBIN(temp)-!flagc;               \
                   flagc  = (val < 0x8000);                                 \
                   regs.a = TOBCD(val);                                     \
                   if (EmulatorGetAppleType() == APPLE_TYPE_IIE)            \
                     SETNZ(regs.a);                                         \
                 }                                                          \
                 else {                                                     \
                   val    = regs.a-temp-!flagc;                             \
                   flagc  = (val < 0x8000);                                 \
                   flagv  = (((regs.a & 0x80) != (temp & 0x80)) &&          \
                             ((regs.a & 0x80) != (val & 0x80)));            \
                   regs.a = val & 0xFF;                                     \
                   SETNZ(regs.a);                                           \
                 }
#define SEC      flagc = 1;
#define SED      regs.ps |= PS_DECIMAL;
#define SEI      regs.ps |= PS_INTERRUPT;
#define STA      WRITE(regs.a);
#define STX      WRITE(regs.x);
#define STY      WRITE(regs.y);
#define STZ      WRITE(0);
#define TAX      regs.x = regs.a;                                           \
                 SETNZ(regs.x);
#define TAY      regs.y = regs.a;                                           \
                 SETNZ(regs.y);
#define TRB      val   = READ();                                            \
                 flagz = !(regs.a & val);                                   \
                 val  &= ~regs.a;                                           \
                 WRITE(val);
#define TSB      val   = READ();                                            \
                 flagz = !(regs.a & val);                                   \
                 val   |= regs.a;                                           \
                 WRITE(val);
#define TSX      regs.x = regs.sp & 0xFF;                                   \
                 SETNZ(regs.x);
#define TXA      regs.a = regs.x;                                           \
                 SETNZ(regs.a);
#define TXS      regs.sp = 0x100 | regs.x;
#define TYA      regs.a = regs.y;                                           \
                 SETNZ(regs.a);
#define INVALID1
#define INVALID2 if (EmulatorGetAppleType() == APPLE_TYPE_IIE) ++regs.pc;
#define INVALID3 if (EmulatorGetAppleType() == APPLE_TYPE_IIE) regs.pc += 2;


/****************************************************************************
*
*   Addressing mode helpers
*
***/

//===========================================================================
static inline uint16_t Absolute() {
    uint16_t addr = *(uint16_t *)(mem + regs.pc);
    regs.pc += 2;
    return addr;
}

//===========================================================================
static inline uint16_t AbsoluteX() {
    uint16_t addr = *(uint16_t *)(mem + regs.pc) + uint16_t(regs.x);
    regs.pc += 2;
    return addr;
}

//===========================================================================
static inline uint16_t AbsoluteX(int * cycles) {
    uint16_t oldaddr = regs.pc;
    uint16_t addr = *(uint16_t *)(mem + regs.pc) + uint16_t(regs.x);
    if ((oldaddr ^ addr) & 0xff00)
        ++*cycles;
    regs.pc += 2;
    return addr;
}

//===========================================================================
static inline uint16_t AbsoluteY() {
    uint16_t addr = *(uint16_t *)(mem + regs.pc) + uint16_t(regs.y);
    regs.pc += 2;
    return addr;
}

//===========================================================================
static inline uint16_t AbsoluteY(int * cycles) {
    uint16_t oldaddr = regs.pc;
    uint16_t addr = *(uint16_t *)(mem + regs.pc) + uint16_t(regs.y);
    if ((oldaddr ^ addr) & 0xff00)
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
    uint16_t addr = *(uint16_t *)(mem + *(uint16_t *)(mem + regs.pc));
    regs.pc += 2;
    return addr;
}

//===========================================================================
static inline uint16_t Indirect6502() {
    uint16_t addr;
    if (mem[regs.pc] == 0xff) {
        uint16_t addr0 = *(uint16_t *)(mem + regs.pc);
        uint16_t addr1 = addr0 - 0xff;
        addr = uint16_t(mem[addr0]) | uint16_t(mem[addr1] << 8);
    }
    else
        addr = *(uint16_t *)(mem + *(uint16_t *)(mem + regs.pc));
    regs.pc += 2;
    return addr;
}

//===========================================================================
static inline uint16_t IndirectX() {
    uint16_t addr = *(uint16_t *)(mem + ((mem[regs.pc] + regs.x) & 0xff));
    ++regs.pc;
    return addr;
}

//===========================================================================
static inline uint16_t IndirectY() {
    uint16_t addr = *(uint16_t *)(mem + mem[regs.pc]) + (uint16_t)regs.y;
    ++regs.pc;
    return addr;
}

//===========================================================================
static inline uint16_t IndirectY(int * cycles) {
    uint16_t oldaddr = regs.pc;
    uint16_t addr = *(uint16_t *)(mem + mem[regs.pc]) + (uint16_t)regs.y;
    if ((oldaddr ^ addr) & 0xff00)
        ++*cycles;
    ++regs.pc;
    return addr;
}

//===========================================================================
static inline uint16_t IndirectZeroPage() {
    uint16_t addr = *(uint16_t *)(mem + mem[regs.pc]);
    ++regs.pc;
    return addr;
}

//===========================================================================
static inline uint16_t Relative() {
    int8_t offset = (int8_t)mem[regs.pc];
    ++regs.pc;
    return regs.pc + offset;
}

//===========================================================================
static inline uint16_t ZeroPage() {
    uint16_t addr = mem[regs.pc];
    ++regs.pc;
    return addr;
}

//===========================================================================
static inline uint16_t ZeroPageX() {
    uint16_t addr = uint8_t(mem[regs.pc] + regs.x);
    ++regs.pc;
    return addr;
}

//===========================================================================
static inline uint16_t ZeroPageY() {
    uint16_t addr = uint8_t(mem[regs.pc] + regs.y);
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
    return mem[regs.sp];
}

//===========================================================================
static inline uint16_t Pop16() {
    uint16_t addr0 = Pop8();
    uint16_t addr1 = Pop8();
    return addr0 | addr1 << 8;
}

//===========================================================================
static inline void Push8(uint8_t value) {
    mem[regs.sp] = value;
    regs.sp = (regs.sp - 1) | 0x100;
}

//===========================================================================
static inline uint8_t Read8(uint16_t addr) {
    if ((addr & 0xff00) == 0xc000)
        return ioRead[addr & 0xff](regs.pc, (uint8_t)addr, 0, 0);
    else
        return mem[addr];
}

//===========================================================================
static inline uint16_t Read16(uint16_t addr) {
    return *(uint16_t *)(mem + addr);
}

//===========================================================================
static inline void Write8(uint16_t addr, uint8_t value) {
    int       offset = addr & 0xff;
    int       pageno = addr >> 8;
    uint8_t * page   = memWrite[0][pageno];
    memDirty[pageno] = 0xff;
    if (page)
        page[offset] = value;
    else if (pageno == 0xc0)
        ioWrite[offset](regs.pc, uint8_t(offset), 1, value);
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
    uint8_t lobit = regs.a & 1;
    regs.a = regs.a >> 1 | ((regs.ps & PS_CARRY) << 7);
    SetFlagTo(PS_CARRY, lobit);
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
static inline uint8_t Lsr(uint8_t val8) {
    SetFlagTo(PS_CARRY, val8 & 1);
    ResetFlag(PS_SIGN);
    val8 >>= 1;
    SetFlagTo(PS_ZERO, !val8);
    return val8;
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
    uint8_t hibit = val8 >> 7;
    val8 = val8 << 1 | (regs.ps & PS_CARRY);
    val8 &= regs.a;
    SetFlagTo(PS_CARRY, hibit);
    UpdateNZ(val8);
    return val8;
}

//===========================================================================
static inline uint8_t Rol(uint8_t val8) {
    uint8_t hibit = val8 >> 7;
    val8 = val8 << 1 | (regs.ps & PS_CARRY);
    SetFlagTo(PS_CARRY, hibit);
    UpdateNZ(val8);
    return val8;
}

//===========================================================================
static inline uint8_t Ror(uint8_t val8) {
    uint8_t lobit = val8 & 1;
    val8 = val8 >> 1 | ((regs.ps & PS_CARRY) << 7);
    SetFlagTo(PS_CARRY, lobit);
    UpdateNZ(val8);
    return val8;
}

//===========================================================================
static inline uint8_t Rra(uint8_t val8) {
    uint8_t lobit = val8 & 1;
    val8 = val8 >> 1 | ((regs.ps & PS_CARRY) << 7);
    val8 &= regs.a;
    SetFlagTo(PS_CARRY, lobit);
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
static inline uint8_t Slo(uint8_t val8) {
    SetFlagTo(PS_CARRY, val8 & 0x80);
    val8 <<= 1;
    val8 |= regs.a;
    UpdateNZ(val8);
    return val8;
}

//===========================================================================
static inline uint8_t Sre(uint8_t val8) {
    SetFlagTo(PS_CARRY, val8 & 1);
    ResetFlag(PS_SIGN);
    val8 >>= 1;
    val8 ^= regs.a;
    SetFlagTo(PS_ZERO, !val8);
    return val8;
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


/****************************************************************************
*
*   CPU step functions
*
***/

//===========================================================================
int CpuStep6502() {
    int      cycles = 0;
    uint16_t addr;
    uint8_t  val8;

    if (cpuKill)
        return 1000;

//    char buf[32];
//    StrPrintf(buf, ARRSIZE(buf), "PC: %04X  Opcde: %02X\n", regs.pc, mem[regs.pc]);
//    OutputDebugString(buf);

#if 0
    int stateoffset = stateindex - statestart;
    if (stateoffset >= 0 && stateoffset < ARRSIZE(state)) {
        state[stateoffset].reg = regs;
        state[stateoffset].opcode = mem[regs.pc];
    }
    ++stateindex;
    if (stateoffset == ARRSIZE(state)) {
        char buf[32];
        for (int i = 0; i < ARRSIZE(state); ++i) {
            StrPrintf(buf, ARRSIZE(buf), "%02X: %02X %02X%02X%02X%02X\n", i, state[i].opcode, state[i].reg.a, state[i].reg.x, state[i].reg.y, state[i].reg.ps);
            OutputDebugString(buf);
        }
    }
#endif

    switch (mem[regs.pc++]) {
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
        case 0x02: // *KIL
            cpuKill = true;
            break;
        case 0x03: // *SLO (izx)
            addr = IndirectX();
            val8 = Read8(addr);
            val8 = Slo(val8);
            Write8(addr, val8);
            cycles += 8;
            break;
        case 0x04: // *NOP (zp)
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
        case 0x07: // *SLO (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            val8 = Slo(val8);
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
        case 0x0c: // *NOP (abs)
            addr = Absolute();
            val8 = Read8(addr);
            cycles += 3;
            break;
        case 0x0d: // ORA (abs)
            addr = Absolute();
            val8 = Read8(addr);
            Ora(val8);
            cycles += 4;
            break;
        case 0x0e: // ASL (abs)
            addr = Absolute();
            val8 = Read8(addr);
            val8 = Asl(val8);
            Write8(addr, val8);
            cycles += 6;
            break;
        case 0x0f: // *SLO (abs)
            addr = Absolute();
            val8 = Read8(addr);
            val8 = Slo(val8);
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
        case 0x12: // *KIL
            cpuKill = true;
            break;
        case 0x13: // *SLO (izy)
            addr = IndirectY();
            val8 = Read8(addr);
            val8 = Slo(val8);
            Write8(addr, val8);
            cycles += 8;
            break;
        case 0x14: // *NOP (zpx)
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
        case 0x17: // *SLO (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            val8 = Slo(val8);
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
        case 0x1b: // *SLO (aby)
            addr = AbsoluteY();
            val8 = Read8(addr);
            val8 = Slo(val8);
            Write8(addr, val8);
            cycles += 7;
            break;
        case 0x1c: // *NOP (abx)
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
        case 0x1f: // *SLO (abx)
            addr = AbsoluteX();
            val8 = Read8(addr);
            val8 = Slo(val8);
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
        case 0x22: // *KIL
            cpuKill = true;
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
        case 0x32: // *KIL
            cpuKill = true;
            break;
        case 0x33: // *RLA (izy)
            addr = IndirectY();
            val8 = Read8(addr);
            val8 = Rla(val8);
            Write8(addr, val8);
            cycles += 8;
            break;
        case 0x34: // *NOP (zpx)
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
        case 0x3c: // *NOP (abx)
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
        case 0x42: // *KIL
            cpuKill = true;
            break;
        case 0x43: // *SRE (izx)
            addr = IndirectX();
            val8 = Read8(addr);
            val8 = Sre(val8);
            Write8(addr, val8);
            cycles += 8;
            break;
        case 0x44: // *NOP (zp)
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
        case 0x47: // *SRE (zp)
            addr = ZeroPage();
            val8 = Read8(addr);
            val8 = Sre(val8);
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
        case 0x4f: // *SRE (abs)
            addr = Absolute();
            val8 = Read8(addr);
            val8 = Sre(val8);
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
        case 0x52: // *KIL
            cpuKill = true;
            break;
        case 0x53: // *SRE (izy)
            addr = IndirectY();
            val8 = Read8(addr);
            val8 = Sre(val8);
            Write8(addr, val8);
            cycles += 8;
            break;
        case 0x54: // *NOP (zpx)
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
        case 0x57: // *SRE (zpx)
            addr = ZeroPageX();
            val8 = Read8(addr);
            val8 = Sre(val8);
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
        case 0x5b: // *SRE (aby)
            addr = AbsoluteY();
            val8 = Read8(addr);
            val8 = Sre(val8);
            Write8(addr, val8);
            cycles += 7;
            break;
        case 0x5c: // *NOP (abx)
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
        case 0x5f: // *SRE (abx)
            addr = AbsoluteX();
            val8 = Read8(addr);
            val8 = Sre(val8);
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
        case 0x62: // *KIL
            cpuKill = true;
            break;
        case 0x63: // *RRA (izx)
            addr = IndirectX();
            val8 = Read8(addr);
            val8 = Rra(val8);
            Write8(addr, val8);
            cycles += 8;
            break;
        case 0x64: // *NOP (zp)
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
        case 0x72: // *KIL
            cpuKill = true;
            break;
        case 0x73: // *RRA (izy)
            addr = IndirectY();
            val8 = Read8(addr);
            val8 = Rra(val8);
            Write8(addr, val8);
            cycles += 8;
            break;
        case 0x74: // *NOP (zpx)
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
        case 0x7c: // *NOP (abx)
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
        case 0x80: // *NOP (imm)
            addr = Immediate();
            val8 = Read8(addr);
            cycles += 2;
            break;
        case 0x81: // STA (izx)
            addr = IndirectX();
            Write8(addr, regs.a);
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
        case 0x88: // DEY
            Dey();
            cycles += 2;
            break;
        case 0x8a: // TXA
            Txa();
            cycles += 2;
            break;
        case 0x8c: // STY (abs)
            addr = Absolute();
            Write8(addr, regs.y);
            cycles += 4;
            break;
        case 0x8d: // STA (abs)
            addr = Absolute();
            Write8(addr, regs.a);
            cycles += 4;
            break;
        case 0x8e: // STX (abs)
            addr = Absolute();
            Write8(addr, regs.x);
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
        case 0x98: // TYA
            Tya();
            cycles += 2;
            break;
        case 0x99: // STA (aby)
            addr = AbsoluteY(&cycles);
            Write8(addr, regs.a);
            cycles += 5;
            break;
        case 0x9a: // TXS
            Txs();
            cycles += 2;
            break;
        case 0x9d: // STA (abx)
            addr = AbsoluteX(&cycles);
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
        case 0xaa:
            Tax();
            cycles += 2;
            break;
        case 0xac: // LDY (abs)
            addr = Absolute();
            val8 = Read8(addr);
            Ldy(val8);
            cycles += 4;
            break;
        case 0xad: // LDA (abs)
            addr = Absolute();
            val8 = Read8(addr);
            Lda(val8);
            cycles += 4;
            break;
        case 0xae: // LDX (abs)
            addr = Absolute();
            val8 = Read8(addr);
            Ldx(val8);
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

        default:
            cycles++;
            break;
    }

    return cycles;
}

//===========================================================================
int CpuStepTest() {
    WORD addr;
    BOOL flagc;
    BOOL flagn;
    BOOL flagv;
    BOOL flagz;
    WORD temp;
    WORD val;
    AF_TO_EF();
    int64_t cycles = 0;
    int64_t * cyclecounter = &cycles;

#if 0
    int stateoffset = stateindex - statestart;
    if (stateoffset >= 0 && stateoffset < ARRSIZE(state)) {
        state[stateoffset].reg = regs;
        state[stateoffset].opcode = mem[regs.pc];
    }
    ++stateindex;
    if (stateoffset == ARRSIZE(state)) {
        char buf[32];
        for (int i = 0; i < ARRSIZE(state); ++i) {
            StrPrintf(buf, ARRSIZE(buf), "%02X: %02X %02X%02X%02X%02X\n", i, state[i].opcode, state[i].reg.a, state[i].reg.x, state[i].reg.y, state[i].reg.ps);
            OutputDebugString(buf);
        }
    }
#endif

    switch (mem[regs.pc++]) {
        case 0x00:       BRK           CYC(7);  break;
        case 0x01:       INDX ORA      CYC(6);  break;
        case 0x02:       INVALID2      CYC(2);  break;
        case 0x03:       INVALID1      CYC(1);  break;
        case 0x04: CMOS  ZPG TSB       CYC(5);  break;
        case 0x05:       ZPG ORA       CYC(3);  break;
        case 0x06:       ZPG ASL       CYC(5);  break;
        case 0x07:       INVALID1      CYC(1);  break;
        case 0x08:       PHP           CYC(3);  break;
        case 0x09:       IMM ORA       CYC(2);  break;
        case 0x0A:       ASLA          CYC(2);  break;
        case 0x0B:       INVALID1      CYC(1);  break;
        case 0x0C: CMOS  ABS TSB       CYC(6);  break;
        case 0x0D:       ABS ORA       CYC(4);  break;
        case 0x0E:       ABS ASL       CYC(6);  break;
        case 0x0F:       INVALID1      CYC(1);  break;
        case 0x10:       REL BPL       CYC(3);  break;
        case 0x11:       INDY ORA      CYC(5);  break;
        case 0x12: CMOS  IND ORA       CYC(5);  break;
        case 0x13:       INVALID1      CYC(1);  break;
        case 0x14: CMOS  ZPG TRB       CYC(5);  break;
        case 0x15:       ZPGX ORA      CYC(4);  break;
        case 0x16:       ZPGX ASL      CYC(6);  break;
        case 0x17:       INVALID1      CYC(1);  break;
        case 0x18:       CLC           CYC(2);  break;
        case 0x19:       ABSY ORA      CYC(4);  break;
        case 0x1A: CMOS  INA           CYC(2);  break;
        case 0x1B:       INVALID1      CYC(1);  break;
        case 0x1C: CMOS  ABS TRB       CYC(6);  break;
        case 0x1D:       ABSX ORA      CYC(4);  break;
        case 0x1E:       ABSX ASL      CYC(6);  break;
        case 0x1F:       INVALID1      CYC(1);  break;
        case 0x20:       ABS JSR       CYC(6);  break;
        case 0x21:       INDX AND      CYC(6);  break;
        case 0x22:       INVALID2      CYC(2);  break;
        case 0x23:       INVALID1      CYC(1);  break;
        case 0x24:       ZPG BIT       CYC(3);  break;
        case 0x25:       ZPG AND       CYC(3);  break;
        case 0x26:       ZPG ROL       CYC(5);  break;
        case 0x27:       INVALID1      CYC(1);  break;
        case 0x28:       PLP           CYC(4);  break;
        case 0x29:       IMM AND       CYC(2);  break;
        case 0x2A:       ROLA          CYC(2);  break;
        case 0x2B:       INVALID1      CYC(1);  break;
        case 0x2C:       ABS BIT       CYC(4);  break;
        case 0x2D:       ABS AND       CYC(4);  break;
        case 0x2E:       ABS ROL       CYC(6);  break;
        case 0x2F:       INVALID1      CYC(1);  break;
        case 0x30:       REL BMI       CYC(3);  break;
        case 0x31:       INDY AND      CYC(5);  break;
        case 0x32: CMOS  IND AND       CYC(5);  break;
        case 0x33:       INVALID1      CYC(1);  break;
        case 0x34: CMOS  ZPGX BIT      CYC(4);  break;
        case 0x35:       ZPGX AND      CYC(4);  break;
        case 0x36:       ZPGX ROL      CYC(6);  break;
        case 0x37:       INVALID1      CYC(1);  break;
        case 0x38:       SEC           CYC(2);  break;
        case 0x39:       ABSY AND      CYC(4);  break;
        case 0x3A: CMOS  DEA           CYC(2);  break;
        case 0x3B:       INVALID1      CYC(1);  break;
        case 0x3C: CMOS  ABSX BIT      CYC(4);  break;
        case 0x3D:       ABSX AND      CYC(4);  break;
        case 0x3E:       ABSX ROL      CYC(6);  break;
        case 0x3F:       INVALID1      CYC(1);  break;
        case 0x40:       RTI           CYC(6);  break;
        case 0x41:       INDX EOR      CYC(6);  break;
        case 0x42:       INVALID2      CYC(2);  break;
        case 0x43:       INVALID1      CYC(1);  break;
        case 0x44:       INVALID2      CYC(3);  break;
        case 0x45:       ZPG EOR       CYC(3);  break;
        case 0x46:       ZPG LSR       CYC(5);  break;
        case 0x47:       INVALID1      CYC(1);  break;
        case 0x48:       PHA           CYC(3);  break;
        case 0x49:       IMM EOR       CYC(2);  break;
        case 0x4A:       LSRA          CYC(2);  break;
        case 0x4B:       INVALID1      CYC(1);  break;
        case 0x4C:       ABS JMP       CYC(3);  break;
        case 0x4D:       ABS EOR       CYC(4);  break;
        case 0x4E:       ABS LSR       CYC(6);  break;
        case 0x4F:       INVALID1      CYC(1);  break;
        case 0x50:       REL BVC       CYC(3);  break;
        case 0x51:       INDY EOR      CYC(5);  break;
        case 0x52: CMOS  IND EOR       CYC(5);  break;
        case 0x53:       INVALID1      CYC(1);  break;
        case 0x54:       INVALID2      CYC(4);  break;
        case 0x55:       ZPGX EOR      CYC(4);  break;
        case 0x56:       ZPGX LSR      CYC(6);  break;
        case 0x57:       INVALID1      CYC(1);  break;
        case 0x58:       CLI           CYC(2);  break;
        case 0x59:       ABSY EOR      CYC(4);  break;
        case 0x5A: CMOS  PHY           CYC(3);  break;
        case 0x5B:       INVALID1      CYC(1);  break;
        case 0x5C:       INVALID3      CYC(8);  break;
        case 0x5D:       ABSX EOR      CYC(4);  break;
        case 0x5E:       ABSX LSR      CYC(6);  break;
        case 0x5F:       INVALID1      CYC(1);  break;
        case 0x60:       RTS           CYC(6);  break;
        case 0x61:       INDX ADC      CYC(6);  break;
        case 0x62:       INVALID2      CYC(2);  break;
        case 0x63:       INVALID1      CYC(1);  break;
        case 0x64: CMOS  ZPG STZ       CYC(3);  break;
        case 0x65:       ZPG ADC       CYC(3);  break;
        case 0x66:       ZPG ROR       CYC(5);  break;
        case 0x67:       INVALID1      CYC(1);  break;
        case 0x68:       PLA           CYC(4);  break;
        case 0x69:       IMM ADC       CYC(2);  break;
        case 0x6A:       RORA          CYC(2);  break;
        case 0x6B:       INVALID1      CYC(1);  break;
        case 0x6C:       IABS JMP      CYC(6);  break;
        case 0x6D:       ABS ADC       CYC(4);  break;
        case 0x6E:       ABS ROR       CYC(6);  break;
        case 0x6F:       INVALID1      CYC(1);  break;
        case 0x70:       REL BVS       CYC(3);  break;
        case 0x71:       INDY ADC      CYC(5);  break;
        case 0x72: CMOS  IND ADC       CYC(5);  break;
        case 0x73:       INVALID1      CYC(1);  break;
        case 0x74: CMOS  ZPGX STZ      CYC(4);  break;
        case 0x75:       ZPGX ADC      CYC(4);  break;
        case 0x76:       ZPGX ROR      CYC(6);  break;
        case 0x77:       INVALID1      CYC(1);  break;
        case 0x78:       SEI           CYC(2);  break;
        case 0x79:       ABSY ADC      CYC(4);  break;
        case 0x7A: CMOS  PLY           CYC(4);  break;
        case 0x7B:       INVALID1      CYC(1);  break;
        case 0x7C: CMOS  IABSX JMP     CYC(6);  break;
        case 0x7D:       ABSX ADC      CYC(4);  break;
        case 0x7E:       ABSX ROR      CYC(6);  break;
        case 0x7F:       INVALID1      CYC(1);  break;
        case 0x80: CMOS  REL BRA       CYC(3);  break;
        case 0x81:       INDX STA      CYC(6);  break;
        case 0x82:       INVALID2      CYC(2);  break;
        case 0x83:       INVALID1      CYC(1);  break;
        case 0x84:       ZPG STY       CYC(3);  break;
        case 0x85:       ZPG STA       CYC(3);  break;
        case 0x86:       ZPG STX       CYC(3);  break;
        case 0x87:       INVALID1      CYC(1);  break;
        case 0x88:       DEY           CYC(2);  break;
        case 0x89: CMOS  IMM BITI      CYC(2);  break;
        case 0x8A:       TXA           CYC(2);  break;
        case 0x8B:       INVALID1      CYC(1);  break;
        case 0x8C:       ABS STY       CYC(4);  break;
        case 0x8D:       ABS STA       CYC(4);  break;
        case 0x8E:       ABS STX       CYC(4);  break;
        case 0x8F:       INVALID1      CYC(1);  break;
        case 0x90:       REL BCC       CYC(3);  break;
        case 0x91:       INDY STA      CYC(6);  break;
        case 0x92: CMOS  IND STA       CYC(5);  break;
        case 0x93:       INVALID1      CYC(1);  break;
        case 0x94:       ZPGX STY      CYC(4);  break;
        case 0x95:       ZPGX STA      CYC(4);  break;
        case 0x96:       ZPGY STX      CYC(4);  break;
        case 0x97:       INVALID1      CYC(1);  break;
        case 0x98:       TYA           CYC(2);  break;
        case 0x99:       ABSY STA      CYC(5);  break;
        case 0x9A:       TXS           CYC(2);  break;
        case 0x9B:       INVALID1      CYC(1);  break;
        case 0x9C: CMOS  ABS STZ       CYC(4);  break;
        case 0x9D:       ABSX STA      CYC(5);  break;
        case 0x9E: CMOS  ABSX STZ      CYC(5);  break;
        case 0x9F:       INVALID1      CYC(1);  break;
        case 0xA0:       IMM LDY       CYC(2);  break;
        case 0xA1:       INDX LDA      CYC(6);  break;
        case 0xA2:       IMM LDX       CYC(2);  break;
        case 0xA3:       INVALID1      CYC(1);  break;
        case 0xA4:       ZPG LDY       CYC(3);  break;
        case 0xA5:       ZPG LDA       CYC(3);  break;
        case 0xA6:       ZPG LDX       CYC(3);  break;
        case 0xA7:       INVALID1      CYC(1);  break;
        case 0xA8:       TAY           CYC(2);  break;
        case 0xA9:       IMM LDA       CYC(2);  break;
        case 0xAA:       TAX           CYC(2);  break;
        case 0xAB:       INVALID1      CYC(1);  break;
        case 0xAC:       ABS LDY       CYC(4);  break;
        case 0xAD:       ABS LDA       CYC(4);  break;
        case 0xAE:       ABS LDX       CYC(4);  break;
        case 0xAF:       INVALID1      CYC(1);  break;
        case 0xB0:       REL BCS       CYC(3);  break;
        case 0xB1:       INDY LDA      CYC(5);  break;
        case 0xB2: CMOS  IND LDA       CYC(5);  break;
        case 0xB3:       INVALID1      CYC(1);  break;
        case 0xB4:       ZPGX LDY      CYC(4);  break;
        case 0xB5:       ZPGX LDA      CYC(4);  break;
        case 0xB6:       ZPGY LDX      CYC(4);  break;
        case 0xB7:       INVALID1      CYC(1);  break;
        case 0xB8:       CLV           CYC(2);  break;
        case 0xB9:       ABSY LDA      CYC(4);  break;
        case 0xBA:       TSX           CYC(2);  break;
        case 0xBB:       INVALID1      CYC(1);  break;
        case 0xBC:       ABSX LDY      CYC(4);  break;
        case 0xBD:       ABSX LDA      CYC(4);  break;
        case 0xBE:       ABSY LDX      CYC(4);  break;
        case 0xBF:       INVALID1      CYC(1);  break;
        case 0xC0:       IMM CPY       CYC(2);  break;
        case 0xC1:       INDX CMP      CYC(6);  break;
        case 0xC2:       INVALID2      CYC(2);  break;
        case 0xC3:       INVALID1      CYC(1);  break;
        case 0xC4:       ZPG CPY       CYC(3);  break;
        case 0xC5:       ZPG CMP       CYC(3);  break;
        case 0xC6:       ZPG DEC       CYC(5);  break;
        case 0xC7:       INVALID1      CYC(1);  break;
        case 0xC8:       INY           CYC(2);  break;
        case 0xC9:       IMM CMP       CYC(2);  break;
        case 0xCA:       DEX           CYC(2);  break;
        case 0xCB:       INVALID1      CYC(1);  break;
        case 0xCC:       ABS CPY       CYC(4);  break;
        case 0xCD:       ABS CMP       CYC(4);  break;
        case 0xCE:       ABS DEC       CYC(5);  break;
        case 0xCF:       INVALID1      CYC(1);  break;
        case 0xD0:       REL BNE       CYC(3);  break;
        case 0xD1:       INDY CMP      CYC(5);  break;
        case 0xD2: CMOS  IND CMP       CYC(5);  break;
        case 0xD3:       INVALID1      CYC(1);  break;
        case 0xD4:       INVALID2      CYC(4);  break;
        case 0xD5:       ZPGX CMP      CYC(4);  break;
        case 0xD6:       ZPGX DEC      CYC(6);  break;
        case 0xD7:       INVALID1      CYC(1);  break;
        case 0xD8:       CLD           CYC(2);  break;
        case 0xD9:       ABSY CMP      CYC(4);  break;
        case 0xDA: CMOS  PHX           CYC(3);  break;
        case 0xDB:       INVALID1      CYC(1);  break;
        case 0xDC:       INVALID3      CYC(4);  break;
        case 0xDD:       ABSX CMP      CYC(4);  break;
        case 0xDE:       ABSX DEC      CYC(6);  break;
        case 0xDF:       INVALID1      CYC(1);  break;
        case 0xE0:       IMM CPX       CYC(2);  break;
        case 0xE1:       INDX SBC      CYC(6);  break;
        case 0xE2:       INVALID2      CYC(2);  break;
        case 0xE3:       INVALID1      CYC(1);  break;
        case 0xE4:       ZPG CPX       CYC(3);  break;
        case 0xE5:       ZPG SBC       CYC(3);  break;
        case 0xE6:       ZPG INC       CYC(5);  break;
        case 0xE7:       INVALID1      CYC(1);  break;
        case 0xE8:       INX           CYC(2);  break;
        case 0xE9:       IMM SBC       CYC(2);  break;
        case 0xEA:       NOP           CYC(2);  break;
        case 0xEB:       INVALID1      CYC(1);  break;
        case 0xEC:       ABS CPX       CYC(4);  break;
        case 0xED:       ABS SBC       CYC(4);  break;
        case 0xEE:       ABS INC       CYC(6);  break;
        case 0xEF:       INVALID1      CYC(1);  break;
        case 0xF0:       REL BEQ       CYC(3);  break;
        case 0xF1:       INDY SBC      CYC(5);  break;
        case 0xF2: CMOS  IND SBC       CYC(5);  break;
        case 0xF3:       INVALID1      CYC(1);  break;
        case 0xF4:       INVALID2      CYC(4);  break;
        case 0xF5:       ZPGX SBC      CYC(4);  break;
        case 0xF6:       ZPGX INC      CYC(6);  break;
        case 0xF7:       INVALID1      CYC(1);  break;
        case 0xF8:       SED           CYC(2);  break;
        case 0xF9:       ABSY SBC      CYC(4);  break;
        case 0xFA: CMOS  PLX           CYC(4);  break;
        case 0xFB:       INVALID1      CYC(1);  break;
        case 0xFC:       INVALID3      CYC(4);  break;
        case 0xFD:       ABSX SBC      CYC(4);  break;
        case 0xFE:       ABSX INC      CYC(6);  break;
        case 0xFF:       INVALID1      CYC(1);  break;
    }
    EF_TO_AF();
    return int(cycles);
}

//===========================================================================
int InternalCpuExecute(DWORD attemptcycles, int64_t * cyclecounter) {
    WORD addr;
    BOOL flagc;
    BOOL flagn;
    BOOL flagv;
    BOOL flagz;
    WORD temp;
    WORD val;
    AF_TO_EF();
    int64_t startcycles = *cyclecounter;
    do {
        switch (mem[regs.pc++]) {
            case 0x00:       BRK           CYC(7);  break;
            case 0x01:       INDX ORA      CYC(6);  break;
            case 0x02:       INVALID2      CYC(2);  break;
            case 0x03:       INVALID1      CYC(1);  break;
            case 0x04: CMOS  ZPG TSB       CYC(5);  break;
            case 0x05:       ZPG ORA       CYC(3);  break;
            case 0x06:       ZPG ASL       CYC(5);  break;
            case 0x07:       INVALID1      CYC(1);  break;
            case 0x08:       PHP           CYC(3);  break;
            case 0x09:       IMM ORA       CYC(2);  break;
            case 0x0A:       ASLA          CYC(2);  break;
            case 0x0B:       INVALID1      CYC(1);  break;
            case 0x0C: CMOS  ABS TSB       CYC(6);  break;
            case 0x0D:       ABS ORA       CYC(4);  break;
            case 0x0E:       ABS ASL       CYC(6);  break;
            case 0x0F:       INVALID1      CYC(1);  break;
            case 0x10:       REL BPL       CYC(3);  break;
            case 0x11:       INDY ORA      CYC(5);  break;
            case 0x12: CMOS  IND ORA       CYC(5);  break;
            case 0x13:       INVALID1      CYC(1);  break;
            case 0x14: CMOS  ZPG TRB       CYC(5);  break;
            case 0x15:       ZPGX ORA      CYC(4);  break;
            case 0x16:       ZPGX ASL      CYC(6);  break;
            case 0x17:       INVALID1      CYC(1);  break;
            case 0x18:       CLC           CYC(2);  break;
            case 0x19:       ABSY ORA      CYC(4);  break;
            case 0x1A: CMOS  INA           CYC(2);  break;
            case 0x1B:       INVALID1      CYC(1);  break;
            case 0x1C: CMOS  ABS TRB       CYC(6);  break;
            case 0x1D:       ABSX ORA      CYC(4);  break;
            case 0x1E:       ABSX ASL      CYC(6);  break;
            case 0x1F:       INVALID1      CYC(1);  break;
            case 0x20:       ABS JSR       CYC(6);  break;
            case 0x21:       INDX AND      CYC(6);  break;
            case 0x22:       INVALID2      CYC(2);  break;
            case 0x23:       INVALID1      CYC(1);  break;
            case 0x24:       ZPG BIT       CYC(3);  break;
            case 0x25:       ZPG AND       CYC(3);  break;
            case 0x26:       ZPG ROL       CYC(5);  break;
            case 0x27:       INVALID1      CYC(1);  break;
            case 0x28:       PLP           CYC(4);  break;
            case 0x29:       IMM AND       CYC(2);  break;
            case 0x2A:       ROLA          CYC(2);  break;
            case 0x2B:       INVALID1      CYC(1);  break;
            case 0x2C:       ABS BIT       CYC(4);  break;
            case 0x2D:       ABS AND       CYC(4);  break;
            case 0x2E:       ABS ROL       CYC(6);  break;
            case 0x2F:       INVALID1      CYC(1);  break;
            case 0x30:       REL BMI       CYC(3);  break;
            case 0x31:       INDY AND      CYC(5);  break;
            case 0x32: CMOS  IND AND       CYC(5);  break;
            case 0x33:       INVALID1      CYC(1);  break;
            case 0x34: CMOS  ZPGX BIT      CYC(4);  break;
            case 0x35:       ZPGX AND      CYC(4);  break;
            case 0x36:       ZPGX ROL      CYC(6);  break;
            case 0x37:       INVALID1      CYC(1);  break;
            case 0x38:       SEC           CYC(2);  break;
            case 0x39:       ABSY AND      CYC(4);  break;
            case 0x3A: CMOS  DEA           CYC(2);  break;
            case 0x3B:       INVALID1      CYC(1);  break;
            case 0x3C: CMOS  ABSX BIT      CYC(4);  break;
            case 0x3D:       ABSX AND      CYC(4);  break;
            case 0x3E:       ABSX ROL      CYC(6);  break;
            case 0x3F:       INVALID1      CYC(1);  break;
            case 0x40:       RTI           CYC(6);  break;
            case 0x41:       INDX EOR      CYC(6);  break;
            case 0x42:       INVALID2      CYC(2);  break;
            case 0x43:       INVALID1      CYC(1);  break;
            case 0x44:       INVALID2      CYC(3);  break;
            case 0x45:       ZPG EOR       CYC(3);  break;
            case 0x46:       ZPG LSR       CYC(5);  break;
            case 0x47:       INVALID1      CYC(1);  break;
            case 0x48:       PHA           CYC(3);  break;
            case 0x49:       IMM EOR       CYC(2);  break;
            case 0x4A:       LSRA          CYC(2);  break;
            case 0x4B:       INVALID1      CYC(1);  break;
            case 0x4C:       ABS JMP       CYC(3);  break;
            case 0x4D:       ABS EOR       CYC(4);  break;
            case 0x4E:       ABS LSR       CYC(6);  break;
            case 0x4F:       INVALID1      CYC(1);  break;
            case 0x50:       REL BVC       CYC(3);  break;
            case 0x51:       INDY EOR      CYC(5);  break;
            case 0x52: CMOS  IND EOR       CYC(5);  break;
            case 0x53:       INVALID1      CYC(1);  break;
            case 0x54:       INVALID2      CYC(4);  break;
            case 0x55:       ZPGX EOR      CYC(4);  break;
            case 0x56:       ZPGX LSR      CYC(6);  break;
            case 0x57:       INVALID1      CYC(1);  break;
            case 0x58:       CLI           CYC(2);  break;
            case 0x59:       ABSY EOR      CYC(4);  break;
            case 0x5A: CMOS  PHY           CYC(3);  break;
            case 0x5B:       INVALID1      CYC(1);  break;
            case 0x5C:       INVALID3      CYC(8);  break;
            case 0x5D:       ABSX EOR      CYC(4);  break;
            case 0x5E:       ABSX LSR      CYC(6);  break;
            case 0x5F:       INVALID1      CYC(1);  break;
            case 0x60:       RTS           CYC(6);  break;
            case 0x61:       INDX ADC      CYC(6);  break;
            case 0x62:       INVALID2      CYC(2);  break;
            case 0x63:       INVALID1      CYC(1);  break;
            case 0x64: CMOS  ZPG STZ       CYC(3);  break;
            case 0x65:       ZPG ADC       CYC(3);  break;
            case 0x66:       ZPG ROR       CYC(5);  break;
            case 0x67:       INVALID1      CYC(1);  break;
            case 0x68:       PLA           CYC(4);  break;
            case 0x69:       IMM ADC       CYC(2);  break;
            case 0x6A:       RORA          CYC(2);  break;
            case 0x6B:       INVALID1      CYC(1);  break;
            case 0x6C:       IABS JMP      CYC(6);  break;
            case 0x6D:       ABS ADC       CYC(4);  break;
            case 0x6E:       ABS ROR       CYC(6);  break;
            case 0x6F:       INVALID1      CYC(1);  break;
            case 0x70:       REL BVS       CYC(3);  break;
            case 0x71:       INDY ADC      CYC(5);  break;
            case 0x72: CMOS  IND ADC       CYC(5);  break;
            case 0x73:       INVALID1      CYC(1);  break;
            case 0x74: CMOS  ZPGX STZ      CYC(4);  break;
            case 0x75:       ZPGX ADC      CYC(4);  break;
            case 0x76:       ZPGX ROR      CYC(6);  break;
            case 0x77:       INVALID1      CYC(1);  break;
            case 0x78:       SEI           CYC(2);  break;
            case 0x79:       ABSY ADC      CYC(4);  break;
            case 0x7A: CMOS  PLY           CYC(4);  break;
            case 0x7B:       INVALID1      CYC(1);  break;
            case 0x7C: CMOS  IABSX JMP     CYC(6);  break;
            case 0x7D:       ABSX ADC      CYC(4);  break;
            case 0x7E:       ABSX ROR      CYC(6);  break;
            case 0x7F:       INVALID1      CYC(1);  break;
            case 0x80: CMOS  REL BRA       CYC(3);  break;
            case 0x81:       INDX STA      CYC(6);  break;
            case 0x82:       INVALID2      CYC(2);  break;
            case 0x83:       INVALID1      CYC(1);  break;
            case 0x84:       ZPG STY       CYC(3);  break;
            case 0x85:       ZPG STA       CYC(3);  break;
            case 0x86:       ZPG STX       CYC(3);  break;
            case 0x87:       INVALID1      CYC(1);  break;
            case 0x88:       DEY           CYC(2);  break;
            case 0x89: CMOS  IMM BITI      CYC(2);  break;
            case 0x8A:       TXA           CYC(2);  break;
            case 0x8B:       INVALID1      CYC(1);  break;
            case 0x8C:       ABS STY       CYC(4);  break;
            case 0x8D:       ABS STA       CYC(4);  break;
            case 0x8E:       ABS STX       CYC(4);  break;
            case 0x8F:       INVALID1      CYC(1);  break;
            case 0x90:       REL BCC       CYC(3);  break;
            case 0x91:       INDY STA      CYC(6);  break;
            case 0x92: CMOS  IND STA       CYC(5);  break;
            case 0x93:       INVALID1      CYC(1);  break;
            case 0x94:       ZPGX STY      CYC(4);  break;
            case 0x95:       ZPGX STA      CYC(4);  break;
            case 0x96:       ZPGY STX      CYC(4);  break;
            case 0x97:       INVALID1      CYC(1);  break;
            case 0x98:       TYA           CYC(2);  break;
            case 0x99:       ABSY STA      CYC(5);  break;
            case 0x9A:       TXS           CYC(2);  break;
            case 0x9B:       INVALID1      CYC(1);  break;
            case 0x9C: CMOS  ABS STZ       CYC(4);  break;
            case 0x9D:       ABSX STA      CYC(5);  break;
            case 0x9E: CMOS  ABSX STZ      CYC(5);  break;
            case 0x9F:       INVALID1      CYC(1);  break;
            case 0xA0:       IMM LDY       CYC(2);  break;
            case 0xA1:       INDX LDA      CYC(6);  break;
            case 0xA2:       IMM LDX       CYC(2);  break;
            case 0xA3:       INVALID1      CYC(1);  break;
            case 0xA4:       ZPG LDY       CYC(3);  break;
            case 0xA5:       ZPG LDA       CYC(3);  break;
            case 0xA6:       ZPG LDX       CYC(3);  break;
            case 0xA7:       INVALID1      CYC(1);  break;
            case 0xA8:       TAY           CYC(2);  break;
            case 0xA9:       IMM LDA       CYC(2);  break;
            case 0xAA:       TAX           CYC(2);  break;
            case 0xAB:       INVALID1      CYC(1);  break;
            case 0xAC:       ABS LDY       CYC(4);  break;
            case 0xAD:       ABS LDA       CYC(4);  break;
            case 0xAE:       ABS LDX       CYC(4);  break;
            case 0xAF:       INVALID1      CYC(1);  break;
            case 0xB0:       REL BCS       CYC(3);  break;
            case 0xB1:       INDY LDA      CYC(5);  break;
            case 0xB2: CMOS  IND LDA       CYC(5);  break;
            case 0xB3:       INVALID1      CYC(1);  break;
            case 0xB4:       ZPGX LDY      CYC(4);  break;
            case 0xB5:       ZPGX LDA      CYC(4);  break;
            case 0xB6:       ZPGY LDX      CYC(4);  break;
            case 0xB7:       INVALID1      CYC(1);  break;
            case 0xB8:       CLV           CYC(2);  break;
            case 0xB9:       ABSY LDA      CYC(4);  break;
            case 0xBA:       TSX           CYC(2);  break;
            case 0xBB:       INVALID1      CYC(1);  break;
            case 0xBC:       ABSX LDY      CYC(4);  break;
            case 0xBD:       ABSX LDA      CYC(4);  break;
            case 0xBE:       ABSY LDX      CYC(4);  break;
            case 0xBF:       INVALID1      CYC(1);  break;
            case 0xC0:       IMM CPY       CYC(2);  break;
            case 0xC1:       INDX CMP      CYC(6);  break;
            case 0xC2:       INVALID2      CYC(2);  break;
            case 0xC3:       INVALID1      CYC(1);  break;
            case 0xC4:       ZPG CPY       CYC(3);  break;
            case 0xC5:       ZPG CMP       CYC(3);  break;
            case 0xC6:       ZPG DEC       CYC(5);  break;
            case 0xC7:       INVALID1      CYC(1);  break;
            case 0xC8:       INY           CYC(2);  break;
            case 0xC9:       IMM CMP       CYC(2);  break;
            case 0xCA:       DEX           CYC(2);  break;
            case 0xCB:       INVALID1      CYC(1);  break;
            case 0xCC:       ABS CPY       CYC(4);  break;
            case 0xCD:       ABS CMP       CYC(4);  break;
            case 0xCE:       ABS DEC       CYC(5);  break;
            case 0xCF:       INVALID1      CYC(1);  break;
            case 0xD0:       REL BNE       CYC(3);  break;
            case 0xD1:       INDY CMP      CYC(5);  break;
            case 0xD2: CMOS  IND CMP       CYC(5);  break;
            case 0xD3:       INVALID1      CYC(1);  break;
            case 0xD4:       INVALID2      CYC(4);  break;
            case 0xD5:       ZPGX CMP      CYC(4);  break;
            case 0xD6:       ZPGX DEC      CYC(6);  break;
            case 0xD7:       INVALID1      CYC(1);  break;
            case 0xD8:       CLD           CYC(2);  break;
            case 0xD9:       ABSY CMP      CYC(4);  break;
            case 0xDA: CMOS  PHX           CYC(3);  break;
            case 0xDB:       INVALID1      CYC(1);  break;
            case 0xDC:       INVALID3      CYC(4);  break;
            case 0xDD:       ABSX CMP      CYC(4);  break;
            case 0xDE:       ABSX DEC      CYC(6);  break;
            case 0xDF:       INVALID1      CYC(1);  break;
            case 0xE0:       IMM CPX       CYC(2);  break;
            case 0xE1:       INDX SBC      CYC(6);  break;
            case 0xE2:       INVALID2      CYC(2);  break;
            case 0xE3:       INVALID1      CYC(1);  break;
            case 0xE4:       ZPG CPX       CYC(3);  break;
            case 0xE5:       ZPG SBC       CYC(3);  break;
            case 0xE6:       ZPG INC       CYC(5);  break;
            case 0xE7:       INVALID1      CYC(1);  break;
            case 0xE8:       INX           CYC(2);  break;
            case 0xE9:       IMM SBC       CYC(2);  break;
            case 0xEA:       NOP           CYC(2);  break;
            case 0xEB:       INVALID1      CYC(1);  break;
            case 0xEC:       ABS CPX       CYC(4);  break;
            case 0xED:       ABS SBC       CYC(4);  break;
            case 0xEE:       ABS INC       CYC(6);  break;
            case 0xEF:       INVALID1      CYC(1);  break;
            case 0xF0:       REL BEQ       CYC(3);  break;
            case 0xF1:       INDY SBC      CYC(5);  break;
            case 0xF2: CMOS  IND SBC       CYC(5);  break;
            case 0xF3:       INVALID1      CYC(1);  break;
            case 0xF4:       INVALID2      CYC(4);  break;
            case 0xF5:       ZPGX SBC      CYC(4);  break;
            case 0xF6:       ZPGX INC      CYC(6);  break;
            case 0xF7:       INVALID1      CYC(1);  break;
            case 0xF8:       SED           CYC(2);  break;
            case 0xF9:       ABSY SBC      CYC(4);  break;
            case 0xFA: CMOS  PLX           CYC(4);  break;
            case 0xFB:       INVALID1      CYC(1);  break;
            case 0xFC:       INVALID3      CYC(4);  break;
            case 0xFD:       ABSX SBC      CYC(4);  break;
            case 0xFE:       ABSX INC      CYC(6);  break;
            case 0xFF:       INVALID1      CYC(1);  break;
        }
    } while (*cyclecounter - startcycles < attemptcycles);
    EF_TO_AF();
    return int(*cyclecounter - startcycles);
}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
int CpuExecute(int32_t cycles, int64_t * cyclecounter) {
    static bool laststep = false;

    if (cycles == 0) {
        laststep = true;
        return InternalCpuExecute(0, cyclecounter);
    }

    if (laststep)
        laststep = false;

    return InternalCpuExecute(cycles, cyclecounter);
}

//===========================================================================
void CpuInitialize() {
    regs.a  = 0;
    regs.x  = 0;
    regs.y  = 0;
    regs.ps = 0x20;
    regs.pc = *(uint16_t *)(mem + 0xfffc);
    regs.sp = 0x01ff;
}

//===========================================================================
void CpuSetType (ECpuType type) {
    cpuType = type;
}

//===========================================================================
void CpuSetupBenchmark() {
    regs.a  = 0;
    regs.x  = 0;
    regs.y  = 0;
    regs.pc = 0x300;
    regs.sp = 0x1FF;

    // CREATE CODE SEGMENTS CONSISTING OF GROUPS OF COMMONLY-USED OPCODES
    int addr = 0x300;
    int opcode = 0;
    do {
        mem[addr++] = benchopcode[opcode];
        mem[addr++] = benchopcode[opcode];
        if (opcode >= SHORTOPCODES)
            mem[addr++] = 0;
        if ((++opcode >= BENCHOPCODES) || ((addr & 0x0F) >= 0x0B)) {
            mem[addr++] = 0x4C;
            mem[addr++] = (opcode >= BENCHOPCODES) ? 0x00 : ((addr >> 4) + 1) << 4;
            mem[addr++] = 0x03;
            while (addr & 0x0F)
                ++addr;
        }
    } while (opcode < BENCHOPCODES);
}
