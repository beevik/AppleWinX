/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#pragma once

class Memory2;

class Cpu {
public:
    using FExecute = uint8_t (Cpu::*)(uint8_t src);

    struct Opcode {
        uint8_t  instruction;
        uint8_t  addrMode;
        uint8_t  accessMode;
        FExecute execute;
    };

    struct Registers {
        uint8_t     a;
        uint8_t     x;
        uint8_t     y;
        uint8_t     ps;
        uint16_t    pc;
        uint16_t    sp;
    };

private:
    ECpuType  m_type   = CPU_TYPE_6502;
    Memory2 * m_memory = nullptr;
    Registers m_regs   = { 0 };
    int64_t   m_cycle  = 0;
    bool      m_halt   = false;

    const Opcode * m_opcodeTable;

    // Addressing mode step methods
    void StepAbsolute(const Opcode & opcode);
    void StepAbsoluteIndexed(const Opcode & opcode);
    void StepImpliedAccumulator(const Opcode & opcode);
    void StepImmediate(const Opcode & opcode);
    void StepImplied(const Opcode & opcode);
    void StepIndexedIndirect(const Opcode & opcode);
    void StepIndirectIndexed(const Opcode & opcode);
    void StepRelative(const Opcode & opcode);
    void StepZeroPage(const Opcode & opcode);
    void StepZeroPageIndexed(const Opcode & opcode);

    // Helper methods
    void ResetFlag(uint8_t flag);
    void SetFlag(uint8_t flag);
    void SetFlagTo(uint8_t flag, int predicate);
    void UpdateNZ(uint8_t src);

public:
    // Public methods
    Cpu(Memory2 * memory);
    void Initialize();
    void Step();

    void AddCycles(int64_t cycles)      { m_cycle += cycles; }
    int64_t Cycle() const               { return m_cycle; }
    const Registers & Registers() const { return m_regs; }

    // Instructions
    uint8_t Adc(uint8_t src);
    uint8_t Alr(uint8_t src);
    uint8_t Anc(uint8_t src);
    uint8_t And(uint8_t src);
    uint8_t Arr(uint8_t src);
    uint8_t Asl(uint8_t src);
    uint8_t Aso(uint8_t src);
    uint8_t Axa(uint8_t src);
    uint8_t Axs(uint8_t src);
    uint8_t Bcc(uint8_t src);
    uint8_t Bcs(uint8_t src);
    uint8_t Beq(uint8_t src);
    uint8_t Bit(uint8_t src);
    uint8_t Bmi(uint8_t src);
    uint8_t Bne(uint8_t src);
    uint8_t Bpl(uint8_t src);
    uint8_t Brk(uint8_t src);
    uint8_t Bvc(uint8_t src);
    uint8_t Bvs(uint8_t src);
    uint8_t Clc(uint8_t src);
    uint8_t Cld(uint8_t src);
    uint8_t Cli(uint8_t src);
    uint8_t Clv(uint8_t src);
    uint8_t Cmp(uint8_t src);
    uint8_t Cpx(uint8_t src);
    uint8_t Cpy(uint8_t src);
    uint8_t Dcm(uint8_t src);
    uint8_t Dec(uint8_t src);
    uint8_t Dex(uint8_t src);
    uint8_t Dey(uint8_t src);
    uint8_t Eor(uint8_t src);
    uint8_t Hlt(uint8_t src);
    uint8_t Inc(uint8_t src);
    uint8_t Ins(uint8_t src);
    uint8_t Inx(uint8_t src);
    uint8_t Iny(uint8_t src);
    uint8_t Jmp(uint8_t src);
    uint8_t JmpI(uint8_t src);
    uint8_t Jsr(uint8_t src);
    uint8_t Las(uint8_t src);
    uint8_t Lax(uint8_t src);
    uint8_t Lda(uint8_t src);
    uint8_t Ldx(uint8_t src);
    uint8_t Ldy(uint8_t src);
    uint8_t Lse(uint8_t src);
    uint8_t Lsr(uint8_t src);
    uint8_t Nop(uint8_t src);
    uint8_t Oal(uint8_t src);
    uint8_t Ora(uint8_t src);
    uint8_t Pha(uint8_t src);
    uint8_t Php(uint8_t src);
    uint8_t Pla(uint8_t src);
    uint8_t Plp(uint8_t src);
    uint8_t Rla(uint8_t src);
    uint8_t Rol(uint8_t src);
    uint8_t Ror(uint8_t src);
    uint8_t Rra(uint8_t src);
    uint8_t Rti(uint8_t src);
    uint8_t Rts(uint8_t src);
    uint8_t Sax(uint8_t src);
    uint8_t Say(uint8_t src);
    uint8_t Sbc(uint8_t src);
    uint8_t Sec(uint8_t src);
    uint8_t Sed(uint8_t src);
    uint8_t Sei(uint8_t src);
    uint8_t Skb(uint8_t src);
    uint8_t Skw(uint8_t src);
    uint8_t Sta(uint8_t src);
    uint8_t Stx(uint8_t src);
    uint8_t Sty(uint8_t src);
    uint8_t Tas(uint8_t src);
    uint8_t Tax(uint8_t src);
    uint8_t Tay(uint8_t src);
    uint8_t Tsx(uint8_t src);
    uint8_t Txa(uint8_t src);
    uint8_t Txs(uint8_t src);
    uint8_t Tya(uint8_t src);
    uint8_t Xaa(uint8_t src);
    uint8_t Xas(uint8_t src);
};
