/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

// CPU status bits
constexpr uint8_t PS_CARRY     = 1 << 0;
constexpr uint8_t PS_ZERO      = 1 << 1;
constexpr uint8_t PS_INTERRUPT = 1 << 2;
constexpr uint8_t PS_DECIMAL   = 1 << 3;
constexpr uint8_t PS_BREAK     = 1 << 4;
constexpr uint8_t PS_RESERVED  = 1 << 5;
constexpr uint8_t PS_OVERFLOW  = 1 << 6;
constexpr uint8_t PS_SIGN      = 1 << 7;

// Addressing modes
constexpr uint8_t AM_ABS = 0;       // absolute             abs
constexpr uint8_t AM_ABX = 1;       // indexed,X            abs,X
constexpr uint8_t AM_ABY = 2;       // indexed,Y            abs,Y
constexpr uint8_t AM_ACC = 3;       // implied (acc)
constexpr uint8_t AM_IMM = 4;       // immediate            #val
constexpr uint8_t AM_IMP = 5;       // implied
constexpr uint8_t AM_IND = 6;       // indirect             (abs)
constexpr uint8_t AM_IZX = 7;       // indirect,X           (zp,X)
constexpr uint8_t AM_IZY = 8;       // indirect,Y           (zp),Y
constexpr uint8_t AM_REL = 9;       // relative
constexpr uint8_t AM_ZPG = 10;      // zeropage             zp
constexpr uint8_t AM_ZPX = 11;      // zeropage indexed,X   zp,x
constexpr uint8_t AM_ZPY = 12;      // zeropage indexed,Y   zp,y

// Operation access modes
constexpr uint8_t AC_R  = 1 << 0;   // read
constexpr uint8_t AC_W  = 1 << 1;   // write
constexpr uint8_t AC_RW = 1 << 2;   // read-modify-write
constexpr uint8_t AC_C  = 1 << 3;   // custom

// Instructions
enum : uint8_t {
    IN_ADC,
    IN_ALR,
    IN_ANC,
    IN_AND,
    IN_ARR,
    IN_ASL,
    IN_ASO,
    IN_AXA,
    IN_AXS,
    IN_BCC,
    IN_BCS,
    IN_BEQ,
    IN_BIT,
    IN_BMI,
    IN_BNE,
    IN_BPL,
    IN_BRK,
    IN_BVC,
    IN_BVS,
    IN_CLC,
    IN_CLD,
    IN_CLI,
    IN_CLV,
    IN_CMP,
    IN_CPX,
    IN_CPY,
    IN_DCM,
    IN_DEC,
    IN_DEX,
    IN_DEY,
    IN_EOR,
    IN_HLT,
    IN_INC,
    IN_INS,
    IN_INX,
    IN_INY,
    IN_JMP,
    IN_JSR,
    IN_LAS,
    IN_LAX,
    IN_LDA,
    IN_LDX,
    IN_LDY,
    IN_LSE,
    IN_LSR,
    IN_NOP,
    IN_OAL,
    IN_ORA,
    IN_PHA,
    IN_PHP,
    IN_PLA,
    IN_PLP,
    IN_RLA,
    IN_ROL,
    IN_ROR,
    IN_RRA,
    IN_RTI,
    IN_RTS,
    IN_SAX,
    IN_SAY,
    IN_SBC,
    IN_SEC,
    IN_SED,
    IN_SEI,
    IN_SKB,
    IN_SKW,
    IN_STA,
    IN_STX,
    IN_STY,
    IN_TAX,
    IN_TAS,
    IN_TAY,
    IN_TRB,
    IN_TSB,
    IN_TSX,
    IN_TXA,
    IN_TXS,
    IN_TYA,
    IN_XAA,
    IN_XAS,
};

const Cpu::Opcode s_opcodeTable65c02[] = {
    { IN_BRK, AM_IMP, AC_C,  &Cpu::Brk  }, // 0x00
    { IN_ORA, AM_IZX, AC_R,  &Cpu::Ora  }, // 0x01
};

const Cpu::Opcode s_opcodeTable6502[] = {
    { IN_BRK, AM_IMP, AC_C,  &Cpu::Brk  }, // 0x00
    { IN_ORA, AM_IZX, AC_R,  &Cpu::Ora  }, // 0x01
    { IN_HLT, AM_IMP, 0,     &Cpu::Hlt  }, // 0x02
    { IN_ASO, AM_IZX, AC_RW, &Cpu::Aso  }, // 0x03
    { IN_SKB, AM_ZPG, AC_R,  &Cpu::Skb  }, // 0x04
    { IN_ORA, AM_ZPG, AC_R,  &Cpu::Ora  }, // 0x05
    { IN_ASL, AM_ZPG, AC_RW, &Cpu::Asl  }, // 0x06
    { IN_ASO, AM_ZPG, AC_RW, &Cpu::Aso  }, // 0x07
    { IN_PHP, AM_IMP, 0,     &Cpu::Php  }, // 0x08
    { IN_ORA, AM_IMM, AC_R,  &Cpu::Ora  }, // 0x09
    { IN_ASL, AM_ACC, 0,     &Cpu::Asl  }, // 0x0a
    { IN_ANC, AM_IMM, AC_R,  &Cpu::Anc  }, // 0x0b
    { IN_SKW, AM_ABS, AC_R,  &Cpu::Skw  }, // 0x0c
    { IN_ORA, AM_ABS, AC_R,  &Cpu::Ora  }, // 0x0d
    { IN_ASL, AM_ABS, AC_RW, &Cpu::Asl  }, // 0x0e
    { IN_ASO, AM_ABS, AC_RW, &Cpu::Aso  }, // 0x0f
    { IN_BPL, AM_REL, AC_R,  &Cpu::Bpl  }, // 0x10
    { IN_ORA, AM_IZY, AC_R,  &Cpu::Ora  }, // 0x11
    { IN_HLT, AM_IMP, 0,     &Cpu::Hlt  }, // 0x12
    { IN_ASO, AM_IZY, AC_RW, &Cpu::Aso  }, // 0x13
    { IN_SKB, AM_ZPX, AC_R,  &Cpu::Skb  }, // 0x14
    { IN_ORA, AM_ZPX, AC_R,  &Cpu::Ora  }, // 0x15
    { IN_ASL, AM_ZPX, AC_RW, &Cpu::Asl  }, // 0x16
    { IN_ASO, AM_ZPX, AC_RW, &Cpu::Aso  }, // 0x17
    { IN_CLC, AM_IMP, 0,     &Cpu::Clc  }, // 0x18
    { IN_ORA, AM_ABY, AC_R,  &Cpu::Ora  }, // 0x19
    { IN_NOP, AM_IMP, 0,     &Cpu::Nop  }, // 0x1a
    { IN_ASO, AM_ABY, AC_RW, &Cpu::Aso  }, // 0x1b
    { IN_SKW, AM_ABX, AC_R,  &Cpu::Skw  }, // 0x1c
    { IN_ORA, AM_ABX, AC_R,  &Cpu::Ora  }, // 0x1d
    { IN_ASL, AM_ABX, AC_RW, &Cpu::Asl  }, // 0x1e
    { IN_ASO, AM_ABX, AC_RW, &Cpu::Aso  }, // 0x1f
    { IN_JSR, AM_ABS, AC_C,  &Cpu::Jsr  }, // 0x20
    { IN_AND, AM_IZX, AC_R,  &Cpu::And  }, // 0x21
    { IN_HLT, AM_IMP, 0,     &Cpu::Hlt  }, // 0x22
    { IN_RLA, AM_IZX, AC_RW, &Cpu::Rla  }, // 0x23
    { IN_BIT, AM_ZPG, AC_R,  &Cpu::Bit  }, // 0x24
    { IN_AND, AM_ZPG, AC_R,  &Cpu::And  }, // 0x25
    { IN_ROL, AM_ZPG, AC_RW, &Cpu::Rol  }, // 0x26
    { IN_RLA, AM_ZPG, AC_RW, &Cpu::Rla  }, // 0x27
    { IN_PLP, AM_IMP, 0,     &Cpu::Plp  }, // 0x28
    { IN_AND, AM_IMM, AC_R,  &Cpu::And  }, // 0x29
    { IN_ROL, AM_ACC, 0,     &Cpu::Rol  }, // 0x2a
    { IN_ANC, AM_IMM, AC_R,  &Cpu::Anc  }, // 0x2b
    { IN_BIT, AM_ABS, AC_R,  &Cpu::Bit  }, // 0x2c
    { IN_AND, AM_ABS, AC_R,  &Cpu::And  }, // 0x2d
    { IN_ROR, AM_ABS, AC_RW, &Cpu::Ror  }, // 0x2e
    { IN_RLA, AM_ABS, AC_RW, &Cpu::Rla  }, // 0x2f
    { IN_BMI, AM_REL, AC_R,  &Cpu::Bmi  }, // 0x30
    { IN_AND, AM_IZY, AC_R,  &Cpu::And  }, // 0x31
    { IN_HLT, AM_IMP, 0,     &Cpu::Hlt  }, // 0x32
    { IN_RLA, AM_IZY, AC_RW, &Cpu::Rla  }, // 0x33
    { IN_SKB, AM_ZPX, AC_R,  &Cpu::Skb  }, // 0x34
    { IN_AND, AM_ZPX, AC_R,  &Cpu::And  }, // 0x35
    { IN_ROL, AM_ZPX, AC_RW, &Cpu::Rol  }, // 0x36
    { IN_RLA, AM_ZPX, AC_RW, &Cpu::Rla  }, // 0x37
    { IN_SEC, AM_IMP, 0,     &Cpu::Sec  }, // 0x38
    { IN_AND, AM_ABY, AC_R,  &Cpu::And  }, // 0x39
    { IN_NOP, AM_IMP, 0,     &Cpu::Nop  }, // 0x3a
    { IN_RLA, AM_ABY, AC_RW, &Cpu::Rla  }, // 0x3b
    { IN_SKW, AM_ABX, AC_R,  &Cpu::Skw  }, // 0x3c
    { IN_AND, AM_ABX, AC_R,  &Cpu::And  }, // 0x3d
    { IN_ROL, AM_ABX, AC_RW, &Cpu::Rol  }, // 0x3e
    { IN_RLA, AM_ABX, AC_RW, &Cpu::Rla  }, // 0x3f
    { IN_RTI, AM_IMP, AC_C,  &Cpu::Rti  }, // 0x40
    { IN_EOR, AM_IZX, AC_R,  &Cpu::Eor  }, // 0x41
    { IN_HLT, AM_IMP, 0,     &Cpu::Hlt  }, // 0x42
    { IN_LSE, AM_IZX, AC_RW, &Cpu::Lse  }, // 0x43
    { IN_SKB, AM_ZPG, AC_R,  &Cpu::Skb  }, // 0x44
    { IN_EOR, AM_ZPG, AC_R,  &Cpu::Eor  }, // 0x45
    { IN_LSR, AM_ZPG, AC_RW, &Cpu::Lsr  }, // 0x46
    { IN_LSE, AM_ZPG, AC_RW, &Cpu::Lse  }, // 0x47
    { IN_PHA, AM_IMP, 0,     &Cpu::Pha  }, // 0x48
    { IN_EOR, AM_IMM, AC_R,  &Cpu::Eor  }, // 0x49
    { IN_LSR, AM_ACC, 0,     &Cpu::Lsr  }, // 0x4a
    { IN_ALR, AM_IMM, AC_R,  &Cpu::Alr  }, // 0x4b
    { IN_JMP, AM_ABS, AC_C,  &Cpu::Jmp  }, // 0x4c
    { IN_EOR, AM_ABS, AC_R,  &Cpu::Eor  }, // 0x4d
    { IN_LSR, AM_ABS, AC_RW, &Cpu::Lsr  }, // 0x4e
    { IN_LSE, AM_ABS, AC_RW, &Cpu::Lse  }, // 0x4f
    { IN_BVC, AM_REL, AC_R,  &Cpu::Bvc  }, // 0x50
    { IN_EOR, AM_IZY, AC_R,  &Cpu::Eor  }, // 0x51
    { IN_HLT, AM_IMP, 0,     &Cpu::Hlt  }, // 0x52
    { IN_LSE, AM_IZY, AC_RW, &Cpu::Lse  }, // 0x53
    { IN_SKB, AM_ZPX, AC_R,  &Cpu::Skb  }, // 0x54
    { IN_EOR, AM_ZPX, AC_R,  &Cpu::Eor  }, // 0x55
    { IN_LSR, AM_ZPX, AC_RW, &Cpu::Lsr  }, // 0x56
    { IN_LSE, AM_ZPX, AC_RW, &Cpu::Lse  }, // 0x57
    { IN_CLI, AM_IMP, 0,     &Cpu::Cli  }, // 0x58
    { IN_EOR, AM_ABY, AC_R,  &Cpu::Eor  }, // 0x59
    { IN_NOP, AM_IMP, 0,     &Cpu::Nop  }, // 0x5a
    { IN_LSE, AM_ABY, AC_RW, &Cpu::Lse  }, // 0x5b
    { IN_SKW, AM_ABX, AC_R,  &Cpu::Skw  }, // 0x5c
    { IN_EOR, AM_ABX, AC_R,  &Cpu::Eor  }, // 0x5d
    { IN_LSR, AM_ABX, AC_RW, &Cpu::Lsr  }, // 0x5e
    { IN_LSE, AM_ABX, AC_RW, &Cpu::Lse  }, // 0x5f
    { IN_RTS, AM_IMP, AC_C,  &Cpu::Rts  }, // 0x60
    { IN_ADC, AM_IZX, AC_R,  &Cpu::Adc  }, // 0x61
    { IN_HLT, AM_IMP, 0,     &Cpu::Hlt  }, // 0x62
    { IN_RRA, AM_IZX, AC_RW, &Cpu::Rra  }, // 0x63
    { IN_SKB, AM_ZPG, AC_R,  &Cpu::Skb  }, // 0x64
    { IN_ADC, AM_ZPG, AC_R,  &Cpu::Adc  }, // 0x65
    { IN_ROR, AM_ZPG, AC_RW, &Cpu::Ror  }, // 0x66
    { IN_RRA, AM_ZPG, AC_RW, &Cpu::Rra  }, // 0x67
    { IN_PLA, AM_IMP, 0,     &Cpu::Pla  }, // 0x68
    { IN_ADC, AM_IMM, AC_R,  &Cpu::Adc  }, // 0x69
    { IN_ROR, AM_ACC, 0,     &Cpu::Ror  }, // 0x6a
    { IN_ARR, AM_IMM, AC_R,  &Cpu::Arr  }, // 0x6b
    { IN_JMP, AM_IND, AC_C,  &Cpu::JmpI }, // 0x6c
    { IN_ADC, AM_ABS, AC_R,  &Cpu::Adc  }, // 0x6d
    { IN_ROR, AM_ABS, AC_RW, &Cpu::Ror  }, // 0x6e
    { IN_RRA, AM_ABS, AC_RW, &Cpu::Rra  }, // 0x6f
    { IN_BVS, AM_REL, AC_R,  &Cpu::Bvs  }, // 0x70
    { IN_ADC, AM_IZY, AC_R,  &Cpu::Adc  }, // 0x71
    { IN_HLT, AM_IMP, 0,     &Cpu::Hlt  }, // 0x72
    { IN_RRA, AM_IZY, AC_RW, &Cpu::Rra  }, // 0x73
    { IN_SKB, AM_ZPX, AC_R,  &Cpu::Skb  }, // 0x74
    { IN_ADC, AM_ZPX, AC_R,  &Cpu::Adc  }, // 0x75
    { IN_ROR, AM_ZPX, AC_RW, &Cpu::Ror  }, // 0x76
    { IN_RRA, AM_ZPX, AC_RW, &Cpu::Rra  }, // 0x77
    { IN_SEI, AM_IMP, 0,     &Cpu::Sei  }, // 0x78
    { IN_ADC, AM_ABY, AC_R,  &Cpu::Adc  }, // 0x79
    { IN_NOP, AM_IMP, 0,     &Cpu::Nop  }, // 0x7a
    { IN_RRA, AM_ABY, AC_RW, &Cpu::Rra  }, // 0x7b
    { IN_SKW, AM_ABX, AC_R,  &Cpu::Skw  }, // 0x7c
    { IN_ADC, AM_ABX, AC_R,  &Cpu::Adc  }, // 0x7d
    { IN_ROR, AM_ABX, AC_RW, &Cpu::Ror  }, // 0x7e
    { IN_RRA, AM_ABX, AC_RW, &Cpu::Rra  }, // 0x7f
    { IN_SKB, AM_IMM, AC_R,  &Cpu::Skb  }, // 0x80
    { IN_STA, AM_IZX, AC_W,  &Cpu::Sta  }, // 0x81
    { IN_SKB, AM_IMM, AC_R,  &Cpu::Skb  }, // 0x82
    { IN_AXS, AM_IZX, AC_RW, &Cpu::Axs  }, // 0x83
    { IN_STY, AM_ZPG, AC_W,  &Cpu::Sty  }, // 0x84
    { IN_STA, AM_ZPG, AC_W,  &Cpu::Sta  }, // 0x85
    { IN_STX, AM_ZPG, AC_W,  &Cpu::Stx  }, // 0x86
    { IN_AXS, AM_ZPG, AC_RW, &Cpu::Axs  }, // 0x87
    { IN_DEY, AM_IMP, 0,     &Cpu::Dey  }, // 0x88
    { IN_SKB, AM_IMM, AC_R,  &Cpu::Skb  }, // 0x89
    { IN_TXA, AM_IMP, 0,     &Cpu::Txa  }, // 0x8a
    { IN_XAA, AM_IMM, AC_R,  &Cpu::Xaa  }, // 0x8b
    { IN_STY, AM_ABS, AC_W,  &Cpu::Sty  }, // 0x8c
    { IN_STA, AM_ABS, AC_W,  &Cpu::Sta  }, // 0x8d
    { IN_STX, AM_ABS, AC_W,  &Cpu::Stx  }, // 0x8e
    { IN_AXS, AM_ABS, AC_RW, &Cpu::Axs  }, // 0x8f
    { IN_BCC, AM_REL, AC_R,  &Cpu::Bcc  }, // 0x90
    { IN_STA, AM_IZY, AC_W,  &Cpu::Sta  }, // 0x91
    { IN_HLT, AM_IMP, 0,     &Cpu::Hlt  }, // 0x92
    { IN_AXA, AM_IZY, AC_W,  &Cpu::Axa  }, // 0x93
    { IN_STY, AM_ZPX, AC_W,  &Cpu::Sty  }, // 0x94
    { IN_STA, AM_ZPX, AC_W,  &Cpu::Sta  }, // 0x95
    { IN_STX, AM_ZPY, AC_W,  &Cpu::Stx  }, // 0x96
    { IN_AXS, AM_ZPY, AC_RW, &Cpu::Axs  }, // 0x97
    { IN_TYA, AM_IMP, 0,     &Cpu::Tya  }, // 0x98
    { IN_STA, AM_ABY, AC_W,  &Cpu::Sta  }, // 0x99
    { IN_TXS, AM_IMP, 0,     &Cpu::Txs  }, // 0x9a
    { IN_TAS, AM_ABY, AC_W,  &Cpu::Tas  }, // 0x9b
    { IN_SAY, AM_ABX, AC_W,  &Cpu::Say  }, // 0x9c
    { IN_STA, AM_ABX, AC_W,  &Cpu::Sta  }, // 0x9d
    { IN_XAS, AM_ABX, AC_W,  &Cpu::Xas  }, // 0x9e
    { IN_AXA, AM_ABX, AC_W,  &Cpu::Axa  }, // 0x9f
    { IN_LDY, AM_IMM, AC_R,  &Cpu::Ldy  }, // 0xa0
    { IN_LDA, AM_IZX, AC_R,  &Cpu::Lda  }, // 0xa1
    { IN_LDX, AM_IMM, AC_R,  &Cpu::Ldx  }, // 0xa2
    { IN_LAX, AM_IZX, AC_R,  &Cpu::Lax  }, // 0xa3
    { IN_LDY, AM_ZPG, AC_R,  &Cpu::Ldy  }, // 0xa4
    { IN_LDA, AM_ZPG, AC_R,  &Cpu::Lda  }, // 0xa5
    { IN_LDX, AM_ZPG, AC_R,  &Cpu::Ldx  }, // 0xa6
    { IN_LAX, AM_ZPG, AC_R,  &Cpu::Lax  }, // 0xa7
    { IN_TAY, AM_IMP, 0,     &Cpu::Tay  }, // 0xa8
    { IN_LDA, AM_IMM, AC_R,  &Cpu::Lda  }, // 0xa9
    { IN_TAX, AM_IMP, 0,     &Cpu::Tax  }, // 0xaa
    { IN_OAL, AM_IMM, AC_R,  &Cpu::Oal  }, // 0xab
    { IN_LDY, AM_ABS, AC_R,  &Cpu::Ldy  }, // 0xac
    { IN_LDA, AM_ABS, AC_R,  &Cpu::Lda  }, // 0xad
    { IN_LDX, AM_ABS, AC_R,  &Cpu::Ldx  }, // 0xae
    { IN_LAX, AM_ABS, AC_R,  &Cpu::Lax  }, // 0xaf
    { IN_BCS, AM_REL, AC_R,  &Cpu::Bcs  }, // 0xb0
    { IN_LDA, AM_IZY, AC_R,  &Cpu::Lda  }, // 0xb1
    { IN_HLT, AM_IMP, 0,     &Cpu::Hlt  }, // 0xb2
    { IN_LAX, AM_IZY, AC_R,  &Cpu::Lax  }, // 0xb3
    { IN_LDY, AM_ZPX, AC_R,  &Cpu::Ldy  }, // 0xb4
    { IN_LDA, AM_ZPX, AC_R,  &Cpu::Lda  }, // 0xb5
    { IN_LDX, AM_ZPY, AC_R,  &Cpu::Ldx  }, // 0xb6
    { IN_LAX, AM_ZPY, AC_R,  &Cpu::Lax  }, // 0xb7
    { IN_CLV, AM_IMP, 0,     &Cpu::Clv  }, // 0xb8
    { IN_LDA, AM_ABY, AC_R,  &Cpu::Lda  }, // 0xb9
    { IN_TSX, AM_IMP, 0,     &Cpu::Tsx  }, // 0xba
    { IN_LAS, AM_ABY, AC_R,  &Cpu::Las  }, // 0xbb
    { IN_LDY, AM_ABX, AC_R,  &Cpu::Ldy  }, // 0xbc
    { IN_LDA, AM_ABX, AC_R,  &Cpu::Lda  }, // 0xbd
    { IN_LDX, AM_ABY, AC_R,  &Cpu::Ldx  }, // 0xbe
    { IN_LAX, AM_ABY, AC_R,  &Cpu::Lax  }, // 0xbf
    { IN_CPY, AM_IMM, AC_R,  &Cpu::Cpy  }, // 0xc0
    { IN_CMP, AM_IZX, AC_R,  &Cpu::Cmp  }, // 0xc1
    { IN_SKB, AM_IMM, AC_R,  &Cpu::Skb  }, // 0xc2
    { IN_DCM, AM_IZX, AC_RW, &Cpu::Dcm  }, // 0xc3
    { IN_CPY, AM_ZPG, AC_R,  &Cpu::Cpy  }, // 0xc4
    { IN_CMP, AM_ZPG, AC_R,  &Cpu::Cmp  }, // 0xc5
    { IN_DEC, AM_ZPG, AC_RW, &Cpu::Dec  }, // 0xc6
    { IN_DCM, AM_ZPG, AC_RW, &Cpu::Dcm  }, // 0xc7
    { IN_INY, AM_IMP, 0,     &Cpu::Iny  }, // 0xc8
    { IN_CMP, AM_IMM, AC_R,  &Cpu::Cmp  }, // 0xc9
    { IN_DEX, AM_IMP, 0,     &Cpu::Dex  }, // 0xca
    { IN_SAX, AM_IMM, AC_R,  &Cpu::Sax  }, // 0xcb
    { IN_CPY, AM_ABS, AC_R,  &Cpu::Cpy  }, // 0xcc
    { IN_CMP, AM_ABS, AC_R,  &Cpu::Cmp  }, // 0xcd
    { IN_DEC, AM_ABS, AC_RW, &Cpu::Dec  }, // 0xce
    { IN_DCM, AM_ABS, AC_RW, &Cpu::Dcm  }, // 0xcf
    { IN_BNE, AM_REL, AC_R,  &Cpu::Bne  }, // 0xd0
    { IN_CMP, AM_IZY, AC_R,  &Cpu::Cmp  }, // 0xd1
    { IN_HLT, AM_IMP, 0,     &Cpu::Hlt  }, // 0xd2
    { IN_DCM, AM_IZY, AC_RW, &Cpu::Dcm  }, // 0xd3
    { IN_SKB, AM_ZPX, AC_R,  &Cpu::Skb  }, // 0xd4
    { IN_CMP, AM_ZPX, AC_R,  &Cpu::Cmp  }, // 0xd5
    { IN_DEC, AM_ZPX, AC_RW, &Cpu::Dec  }, // 0xd6
    { IN_DCM, AM_ZPX, AC_RW, &Cpu::Dcm  }, // 0xd7
    { IN_CLD, AM_IMP, 0,     &Cpu::Cld  }, // 0xd8
    { IN_CMP, AM_ABY, AC_R,  &Cpu::Cmp  }, // 0xd9
    { IN_NOP, AM_IMP, 0,     &Cpu::Nop  }, // 0xda
    { IN_DCM, AM_ABY, AC_RW, &Cpu::Dcm  }, // 0xdb
    { IN_SKW, AM_ABX, AC_R,  &Cpu::Skw  }, // 0xdc
    { IN_CMP, AM_ABX, AC_R,  &Cpu::Cmp  }, // 0xdd
    { IN_DEC, AM_ABX, AC_RW, &Cpu::Dec  }, // 0xde
    { IN_DCM, AM_ABX, AC_RW, &Cpu::Dcm  }, // 0xdf
    { IN_CPX, AM_IMM, AC_R,  &Cpu::Cpx  }, // 0xe0
    { IN_SBC, AM_IZX, AC_R,  &Cpu::Sbc  }, // 0xe1
    { IN_SKB, AM_IMM, AC_R,  &Cpu::Skb  }, // 0xe2
    { IN_INS, AM_IZX, AC_RW, &Cpu::Ins  }, // 0xe3
    { IN_CPX, AM_ZPG, AC_R,  &Cpu::Cpx  }, // 0xe4
    { IN_SBC, AM_ZPG, AC_R,  &Cpu::Sbc  }, // 0xe5
    { IN_INC, AM_ZPG, AC_RW, &Cpu::Inc  }, // 0xe6
    { IN_INS, AM_ZPG, AC_RW, &Cpu::Ins  }, // 0xe7
    { IN_INX, AM_IMP, 0,     &Cpu::Inx  }, // 0xe8
    { IN_SBC, AM_IMM, AC_R,  &Cpu::Sbc  }, // 0xe9
    { IN_NOP, AM_IMP, 0,     &Cpu::Nop  }, // 0xea
    { IN_SBC, AM_IMM, AC_R,  &Cpu::Sbc  }, // 0xeb
    { IN_CPX, AM_ABS, AC_R,  &Cpu::Cpx  }, // 0xec
    { IN_SBC, AM_ABS, AC_R,  &Cpu::Sbc  }, // 0xed
    { IN_INC, AM_ABS, AC_RW, &Cpu::Inc  }, // 0xee
    { IN_INS, AM_ABS, AC_RW, &Cpu::Ins  }, // 0xef
    { IN_BEQ, AM_REL, AC_R,  &Cpu::Beq  }, // 0xf0
    { IN_SBC, AM_IZY, AC_R,  &Cpu::Sbc  }, // 0xf1
    { IN_HLT, AM_IMP, 0,     &Cpu::Hlt  }, // 0xf2
    { IN_INS, AM_IZY, AC_RW, &Cpu::Ins  }, // 0xf3
    { IN_SKB, AM_ZPX, AC_R,  &Cpu::Skb  }, // 0xf4
    { IN_SBC, AM_ZPX, AC_R,  &Cpu::Sbc  }, // 0xf5
    { IN_INC, AM_ZPX, AC_RW, &Cpu::Inc  }, // 0xf6
    { IN_INS, AM_ZPX, AC_RW, &Cpu::Ins  }, // 0xf7
    { IN_SED, AM_IMP, 0,     &Cpu::Sed  }, // 0xf8
    { IN_SBC, AM_ABY, AC_R,  &Cpu::Sbc  }, // 0xf9
    { IN_NOP, AM_IMP, 0,     &Cpu::Nop  }, // 0xfa
    { IN_INS, AM_ABY, AC_RW, &Cpu::Ins  }, // 0xfb
    { IN_SKW, AM_ABX, AC_R,  &Cpu::Skw  }, // 0xfc
    { IN_SBC, AM_ABX, AC_R,  &Cpu::Sbc  }, // 0xfd
    { IN_INC, AM_ABX, AC_RW, &Cpu::Inc  }, // 0xfe
    { IN_INS, AM_ABX, AC_RW, &Cpu::Ins  }, // 0xff
};


/****************************************************************************
*
*   Public functions
*
***/

//===========================================================================
Cpu::Cpu(Memory * memory) : m_memory(memory) {
}

//===========================================================================
void Cpu::Initialize() {
    m_regs.a  = 0;
    m_regs.x  = 0;
    m_regs.y  = 0;
    m_regs.ps = PS_RESERVED;
    m_regs.pc = m_memory->Read16(0xfffc);
    m_regs.sp = 0x01ff;
    m_cycle   = 0;
    m_halt    = false;
    SetType(CPU_TYPE_6502);
}

//===========================================================================
void Cpu::SetType(ECpuType type) {
    m_type = type;
    m_opcodeTable = s_opcodeTable6502;
}

//===========================================================================
void Cpu::Step() {
    // Cycle 1: Fetch the opcode.
    uint8_t val = m_memory->Read8(m_regs.pc);
    auto & opcode = m_opcodeTable[val];
    ++m_regs.pc;
    ++m_cycle;

    if (opcode.accessMode & AC_C) {
        (this->*(opcode.execute))(0);
    }
    else {
        switch (opcode.addrMode) {
            case AM_ABS:
                StepAbsolute(opcode);
                break;
            case AM_ABX:
            case AM_ABY:
                StepAbsoluteIndexed(opcode);
                break;
            case AM_ACC:
                StepImpliedAccumulator(opcode);
                break;
            case AM_IMM:
                StepImmediate(opcode);
                break;
            case AM_IMP:
                StepImplied(opcode);
                break;
            case AM_IZX:
                StepIndexedIndirect(opcode);
                break;
            case AM_IZY:
                StepIndirectIndexed(opcode);
                break;
            case AM_REL:
                StepRelative(opcode);
                break;
            case AM_ZPG:
                StepZeroPage(opcode);
                break;
            case AM_ZPX:
            case AM_ZPY:
                StepZeroPageIndexed(opcode);
                break;
        }
    }
}


/****************************************************************************
*
*   Helper methods
*
***/

//===========================================================================
inline void Cpu::ResetFlag(uint8_t flag) {
    m_regs.ps &= ~flag;
}

//===========================================================================
inline void Cpu::SetFlag(uint8_t flag) {
    m_regs.ps |= flag;
}

//===========================================================================
inline void Cpu::SetFlagTo(uint8_t flag, int predicate) {
    if (predicate)
        m_regs.ps |= flag;
    else
        m_regs.ps &= ~flag;
}

//===========================================================================
inline void Cpu::UpdateNZ(uint8_t src) {
    SetFlagTo(PS_SIGN, src & 0x80);
    SetFlagTo(PS_ZERO, src == 0);
}


/****************************************************************************
*
*   Addressing mode step functions
*
***/

//===========================================================================
void Cpu::StepAbsolute(const Opcode & opcode) {
    // Cycle 2: Fetch operand(lo).
    uint16_t addrLo = m_memory->Read8(m_regs.pc);
    ++m_regs.pc;
    ++m_cycle;

    // Cycle 3: Fetch operand(hi).
    uint16_t addrHi = m_memory->Read8(m_regs.pc);
    uint16_t addr = addrHi << 8 | addrLo;
    ++m_regs.pc;
    ++m_cycle;

    if (opcode.accessMode & AC_R) {
        // Cycle 4: Fetch value at address and execute operation.
        uint8_t src = m_memory->Read8(addr);
        (this->*(opcode.execute))(src);
        ++m_cycle;
    }
    else if (opcode.accessMode & AC_W) {
        // Cycle 4: Execute operation and write value to address.
        uint8_t src = (this->*(opcode.execute))((uint8_t)addrHi);
        m_memory->Write8(addr, src);
        ++m_cycle;
    }
    else {
        // Cycle 4: Fetch value at address.
        uint8_t src = m_memory->Read8(addr);
        ++m_cycle;

        // Cycle 5: Write value to address.
        m_memory->Write8(addr, src);
        ++m_cycle;

        // Cycle 6: Execute operation and write updated value to address.
        uint8_t val = (this->*(opcode.execute))(src);
        m_memory->Write8(addr, val);
        ++m_cycle;
    }
}

//===========================================================================
void Cpu::StepAbsoluteIndexed(const Opcode & opcode) {
    // Cycle 2: Fetch operand(lo) and adjust by offset.
    uint8_t offset = opcode.addrMode == AM_ABX ? m_regs.x : m_regs.y;
    uint16_t addrLo = m_memory->Read8(m_regs.pc) + offset;
    bool pageCrossed = (addrLo & 0x0100) != 0;
    ++m_regs.pc;
    ++m_cycle;

    // Cycle 3: Fetch operand(hi).
    uint16_t addrHi = m_memory->Read8(m_regs.pc);
    ++m_regs.pc;
    ++m_cycle;

    // Cycle 4: Fetch value at unfixed address, then fix up the address.
    uint16_t addr = (addrHi << 8) | (addrLo & 0xff);
    uint8_t  src  = m_memory->Read8(addr);
    if (pageCrossed)
        addr += 0x0100;
    ++m_cycle;

    if (opcode.accessMode & AC_R) {
        if (pageCrossed) {
            // Cycle 5: Fetch value from fixed-up address.
            src = m_memory->Read8(addr);
            ++m_cycle;
        }

        (this->*(opcode.execute))(src);
    }
    else if (opcode.accessMode & AC_W) {
        // Cycle 5: Execute the operation and write the value to the
        // fixed-up address.
        uint8_t val = (this->*(opcode.execute))((uint8_t)(addr >> 8));
        m_memory->Write8(addr, val);
        ++m_cycle;
    }
    else {
        // Cycle 5: Fetch value from fixed-up address.
        src = m_memory->Read8(addr);
        ++m_cycle;

        // Cycle 6: Write the value to the fixed-up address.
        m_memory->Write8(addr, src);
        ++m_cycle;

        // Cycle 7: Execute the operation and write the modified value to the
        // fixed-up address.
        uint8_t val = (this->*(opcode.execute))(src);
        m_memory->Write8(addr, val);
        ++m_cycle;
    }
}

//===========================================================================
void Cpu::StepImmediate(const Opcode & opcode) {
    // Cycle 2: Fetch immediate value.
    uint8_t src = m_memory->Read8(m_regs.pc);
    (this->*(opcode.execute))(src);
    ++m_regs.pc;
    ++m_cycle;
}

//===========================================================================
void Cpu::StepImplied(const Opcode & opcode) {
    // Cycle 2: Fetch next value and ignore it.
    m_memory->Read8(m_regs.pc);
    (this->*(opcode.execute))(0);
    ++m_cycle;
}

//===========================================================================
void Cpu::StepImpliedAccumulator(const Opcode & opcode) {
    // Cycle 2: Fetch next value and ignore it.
    m_memory->Read8(m_regs.pc);
    m_regs.a = (this->*(opcode.execute))(m_regs.a);
    ++m_cycle;
}

//===========================================================================
void Cpu::StepIndexedIndirect(const Opcode & opcode) {
    // Cycle 2: Fetch operand, which is a zero page address.
    uint16_t zpAddr = m_memory->Read8(m_regs.pc);
    ++m_regs.pc;
    ++m_cycle;

    // Cycle 3: Read from unadjusted zero page address, then add X to the
    // zero page address.
    m_memory->Read8(zpAddr);
    zpAddr = (zpAddr + m_regs.x) & 0xff;
    ++m_cycle;

    // Cycle 4: Fetch target address (lo) from adjusted zero page address.
    uint16_t targetLo = m_memory->Read8(zpAddr);
    ++m_cycle;

    // Cycle 5: Fetch target address (hi) from adjusted zero page address.
    uint16_t targetHi = m_memory->Read8((zpAddr + 1) & 0xff);
    uint16_t target   = targetHi << 8 | targetLo;
    ++m_cycle;

    if (opcode.accessMode & AC_R) {
        // Cycle 6: Read from target address and execute the operation on it.
        uint8_t src = m_memory->Read8(target);
        (this->*(opcode.execute))(src);
        ++m_cycle;
    }
    else if (opcode.accessMode & AC_W) {
        // Cycle 6: Execute the operation and write result to target address.
        uint8_t val = (this->*(opcode.execute))((uint8_t)targetHi);
        m_memory->Write8(target, val);
        ++m_cycle;
    }
    else {
        // Cycle 6: Read from the target address.
        uint8_t src = m_memory->Read8(target);
        ++m_cycle;

        // Cycle 7: Write unmodified value back to the target address.
        m_memory->Write8(target, src);
        ++m_cycle;

        // Cycle 8: Execute operation on the value and write the modified
        // value back to the target address.
        uint8_t val = (this->*(opcode.execute))(src);
        m_memory->Write8(target, val);
        ++m_cycle;
    }
}

//===========================================================================
void Cpu::StepIndirectIndexed(const Opcode & opcode) {
    // Cycle 2: Fetch operand, which is a zero page address.
    uint16_t zpAddr = m_memory->Read8(m_regs.pc);
    ++m_regs.pc;
    ++m_cycle;

    // Cycle 3: Fetch target address (lo). Add Y.
    uint16_t targetLo = m_memory->Read8(zpAddr) + m_regs.y;
    bool pageCrossed = (targetLo & 0x0100) != 0;
    ++m_cycle;

    // Cycle 4: Fetch target address (hi).
    uint16_t targetHi = m_memory->Read8((zpAddr + 1) & 0xff);
    ++m_cycle;

    // Cycle 5 : Read from unfixed target address, and fix up target address.
    uint16_t target = (targetHi << 8) | (targetLo & 0xff);
    uint8_t src = m_memory->Read8(target);
    if (pageCrossed)
        target += 0x100;
    ++m_cycle;

    if (opcode.accessMode & AC_R) {
        if (pageCrossed) {
            // Cycle 6: Read from fixed-up target, and execute operation.
            src = m_memory->Read8(target);
            ++m_cycle;
        }
        (this->*(opcode.execute))(src);
    }
    else if (opcode.accessMode & AC_W) {
        // Cycle 6: Execute the operaton and write result to fixed-up target.
        uint8_t val = (this->*(opcode.execute))((uint8_t)(target >> 8));
        m_memory->Write8(target, val);
        ++m_cycle;
    }
    else {
        // Cycle 6: Read from fixed-up target, and execute operation.
        uint8_t src = m_memory->Read8(target);
        ++m_cycle;

        // Cycle 7: Write unmodified value back to the target address.
        m_memory->Write8(target, src);
        ++m_cycle;

        // Cycle 8: Execute operation on the value and write the modified
        // value back to the target address.
        uint8_t val = (this->*(opcode.execute))(src);
        m_memory->Write8(target, val);
    }
}

//===========================================================================
void Cpu::StepRelative(const Opcode & opcode) {
    // Cycle 2: Fetch branch offset operand and execute branch instruction
    uint8_t offset = m_memory->Read8(m_regs.pc);
    uint8_t branchTaken = (this->*(opcode.execute))(0);
    m_regs.pc++;
    m_cycle++;

    if (branchTaken) {
        // Cycle 3: Fetch next opcode and ignore it.
        m_memory->Read8(m_regs.pc);
        ++m_cycle;

        // Adjust and fix up PC.
        uint16_t pcTmp = (m_regs.pc & 0xff00) | ((m_regs.pc + offset) & 0xff);
        m_regs.pc += (int8_t)offset;
        if (m_regs.pc != pcTmp) {
            // Cycle 4: If a page boundary was crossed, read from the
            // unadjusted PC address and ignore the value.
            m_memory->Read8(pcTmp);
            ++m_cycle;
        }
    }
}

//===========================================================================
void Cpu::StepZeroPage(const Opcode & opcode) {
    // Cycle 2: Fetch operand.
    uint16_t addr = m_memory->Read8(m_regs.pc);
    ++m_regs.pc;
    ++m_cycle;

    if (opcode.accessMode & AC_R) {
        // Cycle 3: Fetch from address and execute operation.
        uint8_t src = m_memory->Read8(addr);
        (this->*(opcode.execute))(src);
        ++m_cycle;
    }
    else if (opcode.accessMode & AC_W) {
        // Cycle 3: Execute operation and write value to address.
        uint8_t src = (this->*(opcode.execute))(0);
        m_memory->Write8(addr, src);
        ++m_cycle;
    }
    else {
        // Cycle 3: Fetch value at address.
        uint8_t src = m_memory->Read8(addr);
        ++m_cycle;

        // Cycle 4: Write value to address.
        m_memory->Write8(addr, src);
        ++m_cycle;

        // Cycle 5: Execute operation and write updated value to address.
        uint8_t val = (this->*(opcode.execute))(src);
        m_memory->Write8(addr, val);
        ++m_cycle;
    }
}

//===========================================================================
void Cpu::StepZeroPageIndexed(const Opcode & opcode) {
    // Cycle 2: Fetch operand and adjust by offset.
    uint16_t addr = m_memory->Read8(m_regs.pc);
    ++m_regs.pc;
    ++m_cycle;

    // Cycle 3: Fetch value at unadjusted address and fix up the address
    // using the index register.
    m_memory->Read8(addr);
    uint8_t offset = opcode.addrMode == AM_ZPX ? m_regs.x : m_regs.y;
    addr = (addr + offset) & 0xff;
    ++m_cycle;

    if (opcode.accessMode & AC_R) {
        // Cycle 4: Fetch value at the fixed-up address and execute operation
        // on it.
        uint8_t src = m_memory->Read8(addr);
        (this->*(opcode.execute))(src);
        ++m_cycle;
    }
    else if (opcode.accessMode & AC_W) {
        // Cycle 5: Execute the operation and write the value to the
        // fixed-up address.
        uint8_t val = (this->*(opcode.execute))(0);
        m_memory->Write8(addr, val);
        ++m_cycle;
    }
    else {
        // Cycle 4: Fetch value at adjusted address.
        uint8_t src = m_memory->Read8(addr);
        ++m_cycle;

        // Cycle 5: Write the value to the fixed-up address.
        m_memory->Write8(addr, src);
        ++m_cycle;

        // Cycle 6: Execute the operation and write the modified value to the
        // fixed-up address.
        uint8_t val = (this->*(opcode.execute))(src);
        m_memory->Write8(addr, val);
        ++m_cycle;
    }
}


/****************************************************************************
*
*   Instructions
*
***/

//===========================================================================
uint8_t Cpu::Adc(uint8_t src) {
    uint32_t acc   = (uint32_t)m_regs.a;
    uint32_t add   = (uint32_t)src;
    uint32_t carry = (uint32_t)(m_regs.ps & PS_CARRY);
    uint32_t val;
    if (m_regs.ps & PS_DECIMAL) {
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

        val = hi | lo;
        SetFlagTo(PS_OVERFLOW, ((acc ^ val) & 0x80) && !((acc ^ add) & 0x80));
    }
    else {
        val = acc + add + carry;
        SetFlagTo(PS_CARRY, val & 0x100);
        SetFlagTo(PS_OVERFLOW, (acc & 0x80) == (add & 0x80) && (acc & 0x80) != (val & 0x80));
    }

    m_regs.a = uint8_t(val);
    UpdateNZ(m_regs.a);
    return 0;
}

//===========================================================================
uint8_t Cpu::Alr(uint8_t src) {
    m_regs.a &= src;
    SetFlagTo(PS_CARRY, m_regs.a & 1);
    m_regs.a >>= 1;
    ResetFlag(PS_SIGN);
    SetFlagTo(PS_ZERO, !m_regs.a);
    return 0;
}

//===========================================================================
uint8_t Cpu::Anc(uint8_t src) {
    m_regs.a &= src;
    SetFlagTo(PS_CARRY, m_regs.a & 0x80);
    UpdateNZ(m_regs.a);
    return m_regs.a;
    return 0;
}

//===========================================================================
uint8_t Cpu::And(uint8_t src) {
    m_regs.a &= src;
    UpdateNZ(m_regs.a);
    return m_regs.a;
}

//===========================================================================
uint8_t Cpu::Arr(uint8_t src) {
    m_regs.a &= src;
    uint8_t loBit = m_regs.a & 1;
    m_regs.a = (m_regs.a >> 1) | ((m_regs.ps & PS_CARRY) << 7);
    SetFlagTo(PS_CARRY, loBit);
    UpdateNZ(m_regs.a);
    return 0;
}

//===========================================================================
uint8_t Cpu::Asl(uint8_t src) {
    SetFlagTo(PS_CARRY, src & 0x80);
    src <<= 1;
    UpdateNZ(src);
    return src;
}

//===========================================================================
uint8_t Cpu::Aso(uint8_t src) {
    SetFlagTo(PS_CARRY, src & 0x80);
    src <<= 1;
    src |= m_regs.a;
    UpdateNZ(src);
    return src;
}

//===========================================================================
uint8_t Cpu::Axa(uint8_t src) {
    return m_regs.a & m_regs.x & (src + 1);
}

//===========================================================================
uint8_t Cpu::Axs(uint8_t) {
    return m_regs.a & m_regs.x;
}

//===========================================================================
uint8_t Cpu::Bit(uint8_t src) {
    SetFlagTo(PS_ZERO, !(m_regs.a & src));
    SetFlagTo(PS_SIGN, src & 0x80);
    SetFlagTo(PS_OVERFLOW, src & 0x40);
    return 0;
}

//===========================================================================
uint8_t Cpu::Bcc(uint8_t) {
    return (m_regs.ps & PS_CARRY) ? 0 : 1;
}

//===========================================================================
uint8_t Cpu::Bcs(uint8_t) {
    return (m_regs.ps & PS_CARRY) ? 1 : 0;
}

//===========================================================================
uint8_t Cpu::Beq(uint8_t) {
    return (m_regs.ps & PS_ZERO) ? 1 : 0;
}

//===========================================================================
uint8_t Cpu::Bmi(uint8_t src) {
    return (m_regs.ps & PS_SIGN) ? 1 : 0;
}

//===========================================================================
uint8_t Cpu::Bne(uint8_t src) {
    return (m_regs.ps & PS_ZERO) ? 0 : 1;
}

//===========================================================================
uint8_t Cpu::Bpl(uint8_t src) {
    return (m_regs.ps & PS_SIGN) ? 0 : 1;
}

//===========================================================================
uint8_t Cpu::Bvc(uint8_t src) {
    return (m_regs.ps & PS_OVERFLOW) ? 0 : 1;
}

//===========================================================================
uint8_t Cpu::Bvs(uint8_t src) {
    return (m_regs.ps & PS_OVERFLOW) ? 1 : 0;
}

//===========================================================================
uint8_t Cpu::Brk(uint8_t src) {
    // Cycle 2: Fetch next byte and ignore it.
    m_memory->Read8(m_regs.pc);
    m_regs.pc++;
    m_cycle++;

    // Cycle 3: Push PC(hi) on stack.
    m_memory->Write8(m_regs.sp, (m_regs.pc >> 8));
    m_regs.sp = ((m_regs.sp - 1) & 0xff) | 0x100;
    ++m_cycle;

    // Cycle 4: Push PC(lo) on stack.
    m_memory->Write8(m_regs.sp, (m_regs.pc & 0xff));
    m_regs.sp = ((m_regs.sp - 1) & 0xff) | 0x100;
    ++m_cycle;

    // Cycle 5: Push PS on stack (with B set).
    m_memory->Write8(m_regs.sp, (m_regs.ps | PS_BREAK));
    m_regs.sp = ((m_regs.sp - 1) & 0xff) | 0x100;
    ++m_cycle;

    // Cycle 6: Fetch PC(lo) from 0xfffe
    uint16_t addrLo = m_memory->Read8(0xfffe);
    ++m_cycle;

    // Cycle 7: Fetch PC(hi) from 0xfffe and set the interrupt flag.
    uint16_t addrHi = m_memory->Read8(0xffff);
    SetFlag(PS_INTERRUPT);
    m_regs.pc = addrHi << 8 | addrLo;
    ++m_cycle;

    return 0;
}

//===========================================================================
uint8_t Cpu::Clc(uint8_t) {
    ResetFlag(PS_CARRY);
    return 0;
}

//===========================================================================
uint8_t Cpu::Cld(uint8_t) {
    ResetFlag(PS_DECIMAL);
    return 0;
}

//===========================================================================
uint8_t Cpu::Cli(uint8_t) {
    ResetFlag(PS_INTERRUPT);
    return 0;
}

//===========================================================================
uint8_t Cpu::Clv(uint8_t) {
    ResetFlag(PS_OVERFLOW);
    return 0;
}

//===========================================================================
uint8_t Cpu::Cmp(uint8_t src) {
    SetFlagTo(PS_CARRY, m_regs.a >= src);
    UpdateNZ(m_regs.a - src);
    return 0;
}

//===========================================================================
uint8_t Cpu::Cpx(uint8_t src) {
    SetFlagTo(PS_CARRY, m_regs.x >= src);
    UpdateNZ(m_regs.x - src);
    return 0;
}

//===========================================================================
uint8_t Cpu::Cpy(uint8_t src) {
    SetFlagTo(PS_CARRY, m_regs.y >= src);
    UpdateNZ(m_regs.y - src);
    return 0;
}

//===========================================================================
uint8_t Cpu::Dcm(uint8_t src) {
    src--;
    SetFlagTo(PS_CARRY, m_regs.a >= src);
    UpdateNZ(m_regs.a - src);
    return src;
}

//===========================================================================
uint8_t Cpu::Dec(uint8_t src) {
    --src;
    UpdateNZ(src);
    return src;
}

//===========================================================================
uint8_t Cpu::Dex(uint8_t) {
    --m_regs.x;
    UpdateNZ(m_regs.x);
    return 0;
}

//===========================================================================
uint8_t Cpu::Dey(uint8_t) {
    --m_regs.y;
    UpdateNZ(m_regs.y);
    return 0;
}

//===========================================================================
uint8_t Cpu::Eor(uint8_t src) {
    m_regs.a ^= src;
    UpdateNZ(m_regs.a);
    return m_regs.a;
}

//===========================================================================
uint8_t Cpu::Hlt(uint8_t) {
    m_halt = true;
    return 0;
}

//===========================================================================
uint8_t Cpu::Inc(uint8_t src) {
    ++src;
    UpdateNZ(src);
    return src;
}

//===========================================================================
uint8_t Cpu::Ins(uint8_t src) {
    src = m_regs.a - (src + 1);
    UpdateNZ(src);
    return src;
}

//===========================================================================
uint8_t Cpu::Inx(uint8_t) {
    ++m_regs.x;
    UpdateNZ(m_regs.x);
    return 0;
}

//===========================================================================
uint8_t Cpu::Iny(uint8_t) {
    ++m_regs.y;
    UpdateNZ(m_regs.y);
    return 0;
}

//===========================================================================
uint8_t Cpu::Jmp(uint8_t) {
    // Cycle 2: Fetch operand low byte.
    uint16_t addrLo = m_memory->Read8(m_regs.pc);
    ++m_regs.pc;
    ++m_cycle;

    // Cycle 3: Fetch operand hi byte.
    uint16_t addrHi = m_memory->Read8(m_regs.pc);
    m_regs.pc = addrHi << 8 | addrLo;
    ++m_cycle;

    return 0;
}

//===========================================================================
uint8_t Cpu::JmpI(uint8_t) {
    // Cycle 2: Fetch operand low byte.
    uint16_t addrLo = m_memory->Read8(m_regs.pc);
    ++m_regs.pc;
    ++m_cycle;

    // Cycle 3: Fetch operand hi byte.
    uint16_t addrHi = m_memory->Read8(m_regs.pc);
    ++m_cycle;

    // Cycle 4: Fetch target address (lo).
    uint16_t addr = addrHi << 8 | addrLo;
    uint16_t targetLo = m_memory->Read8(addr);
    ++m_cycle;

    // Cycle 5: Fetch target address (hi).
    addr = (addrHi << 8) | ((addrLo + 1) & 0xff);
    uint16_t targetHi = m_memory->Read8(addr);
    m_regs.pc = targetHi << 8 | targetLo;
    ++m_cycle;

    return 0;
}

//===========================================================================
uint8_t Cpu::Jsr(uint8_t src) {
    // Get a pointer to the absolute address operand of JSR.
    uint16_t abs = m_regs.pc;

    // cycle 2: Fetch operand low byte.
    uint16_t addrLo = m_memory->Read8(abs);
    ++m_regs.pc;
    ++m_cycle;

    // Cycle 3: Read stack and ignore.
    m_memory->Read8(m_regs.sp);
    ++m_cycle;

    // Cycle 4: Push PC(hi) on stack.
    m_memory->Write8(m_regs.sp, (m_regs.pc >> 8));
    m_regs.sp = ((m_regs.sp - 1) & 0xff) | 0x100;
    ++m_cycle;

    // Cycle 5: Push PC(lo) on stack.
    m_memory->Write8(m_regs.sp, (m_regs.pc & 0xff));
    m_regs.sp = ((m_regs.sp - 1) & 0xff) | 0x100;
    ++m_cycle;

    // Cycle 6: Fetch operand high byte and update PC.
    uint16_t addrHi = m_memory->Read8(abs + 1);
    m_regs.pc = addrHi << 8 | addrLo;
    ++m_cycle;

    return 0;
}

//===========================================================================
uint8_t Cpu::Las(uint8_t src) {
    uint8_t val = src & (uint8_t)m_regs.sp;
    m_regs.a = m_regs.x = val;
    m_regs.sp = val | 0x0100;
    UpdateNZ(m_regs.a);
    return 0;
}

//===========================================================================
uint8_t Cpu::Lax(uint8_t src) {
    m_regs.a = m_regs.x = src;
    UpdateNZ(m_regs.a);
    return 0;
}

//===========================================================================
uint8_t Cpu::Lda(uint8_t src) {
    m_regs.a = src;
    UpdateNZ(m_regs.a);
    return 0;
}

//===========================================================================
uint8_t Cpu::Ldx(uint8_t src) {
    m_regs.x = src;
    UpdateNZ(m_regs.x);
    return 0;
}

//===========================================================================
uint8_t Cpu::Ldy(uint8_t src) {
    m_regs.y = src;
    UpdateNZ(m_regs.y);
    return 0;
}

//===========================================================================
uint8_t Cpu::Nop(uint8_t) {
    return 0;
}

//===========================================================================
uint8_t Cpu::Oal(uint8_t src) {
    m_regs.a = m_regs.x = (m_regs.a | 0xee) & src;
    UpdateNZ(m_regs.a);
    return src;
}

//===========================================================================
uint8_t Cpu::Ora(uint8_t src) {
    m_regs.a |= src;
    UpdateNZ(m_regs.a);
    return src;
}

//===========================================================================
uint8_t Cpu::Pha(uint8_t) {
    m_memory->Write8(m_regs.sp, m_regs.a);
    m_regs.sp = ((m_regs.sp - 1) & 0xff) | 0x100;
    m_cycle++;
    return 0;
}

//===========================================================================
uint8_t Cpu::Php(uint8_t) {
    SetFlag(PS_RESERVED);
    m_memory->Write8(m_regs.sp, m_regs.ps);
    m_regs.sp = ((m_regs.sp - 1) & 0xff) | 0x100;
    m_cycle++;
    return 0;
}

//===========================================================================
uint8_t Cpu::Pla(uint8_t) {
    // Cycle 3: Increment stack pointer.
    m_regs.sp = ((m_regs.sp + 1) & 0xff) | 0x100;
    m_cycle++;

    // Cycle 4: Fetch value at stack head into accumulator.
    m_regs.a = m_memory->Read8(m_regs.sp);
    UpdateNZ(m_regs.a);
    m_cycle++;
    return 0;
}

//===========================================================================
uint8_t Cpu::Plp(uint8_t) {
    // Cycle 3: Increment stack pointer.
    m_regs.sp = ((m_regs.sp + 1) & 0xff) | 0x100;
    m_cycle++;

    // Cycle 4: Fetch value at stack head into status byte.
    m_regs.ps = m_memory->Read8(m_regs.sp);
    m_cycle++;
    return 0;
}

//===========================================================================
uint8_t Cpu::Lse(uint8_t src) {
    SetFlagTo(PS_CARRY, src & 1);
    ResetFlag(PS_SIGN);
    src >>= 1;
    src ^= m_regs.a;
    SetFlagTo(PS_ZERO, !src);
    return src;
}

//===========================================================================
uint8_t Cpu::Lsr(uint8_t src) {
    SetFlagTo(PS_CARRY, src & 1);
    ResetFlag(PS_SIGN);
    src >>= 1;
    SetFlagTo(PS_ZERO, !src);
    return src;
}

//===========================================================================
uint8_t Cpu::Rla(uint8_t src) {
    uint8_t hiBit = src >> 7;
    src = src << 1 | (m_regs.ps & PS_CARRY);
    src &= m_regs.a;
    SetFlagTo(PS_CARRY, hiBit);
    UpdateNZ(src);
    return src;
}

//===========================================================================
uint8_t Cpu::Rol(uint8_t src) {
    uint8_t hiBit = src >> 7;
    src = src << 1 | (m_regs.ps & PS_CARRY);
    SetFlagTo(PS_CARRY, hiBit);
    UpdateNZ(src);
    return src;
}

//===========================================================================
uint8_t Cpu::Ror(uint8_t src) {
    uint8_t loBit = src & 1;
    src = src >> 1 | ((m_regs.ps & PS_CARRY) << 7);
    SetFlagTo(PS_CARRY, loBit);
    UpdateNZ(src);
    return src;
}

//===========================================================================
uint8_t Cpu::Rra(uint8_t src) {
    uint8_t loBit = src & 1;
    src = src >> 1 | ((m_regs.ps & PS_CARRY) << 7);
    src &= m_regs.a;
    SetFlagTo(PS_CARRY, loBit);
    UpdateNZ(src);
    return src;
}

//===========================================================================
uint8_t Cpu::Rti(uint8_t) {
    // Cycle 2: Fetch next byte and ignore it.
    m_memory->Read8(m_regs.pc);
    ++m_cycle;

    // Cycle 3: Increment stack pointer.
    m_regs.sp = ((m_regs.sp + 1) & 0xff) | 0x100;
    ++m_cycle;

    // Cycle 4: Pop PS from stack and increment stack pointer.
    m_regs.ps = m_memory->Read8(m_regs.sp);
    m_regs.sp = ((m_regs.sp + 1) & 0xff) | 0x100;
    ++m_cycle;

    // Cycle 5: Pop PC(lo) from stack and increment stack pointer.
    uint16_t addrLo = m_memory->Read8(m_regs.sp);
    m_regs.sp = ((m_regs.sp + 1) & 0xff) | 0x100;
    ++m_cycle;

    // Cycle 6: Pop PC(hi) from stack and increment stack pointer.
    uint16_t addrHi = m_memory->Read8(m_regs.sp);
    m_regs.pc = addrHi << 8 | addrLo;
    ++m_cycle;

    return 0;
}

//===========================================================================
uint8_t Cpu::Rts(uint8_t) {
    // Cycle 2: Fetch next byte and ignore it.
    m_memory->Read8(m_regs.pc);
    ++m_cycle;

    // Cycle 3: Increment stack pointer
    m_regs.sp = ((m_regs.sp + 1) & 0xff) | 0x100;
    ++m_cycle;

    // Cycle 4: Pop PC(lo) from stack and increment stack pointer.
    uint16_t addrLo = m_memory->Read8(m_regs.sp);
    m_regs.sp = ((m_regs.sp + 1) & 0xff) | 0x100;
    ++m_cycle;

    // Cycle 5: Pop PC(hi) from stack.
    uint16_t addrHi = m_memory->Read8(m_regs.sp);
    m_regs.pc = addrHi << 8 | addrLo;
    ++m_cycle;

    // Cycle 6: Increment PC
    ++m_regs.pc;
    ++m_cycle;

    return 0;
}

//===========================================================================
uint8_t Cpu::Sax(uint8_t src) {
    m_regs.x = (m_regs.a & m_regs.x) - src;
    SetFlagTo(PS_CARRY, m_regs.x >= src);
    UpdateNZ(m_regs.x - src);
    return 0;
}

//===========================================================================
uint8_t Cpu::Say(uint8_t src) {
    return m_regs.y & (src + 1);
}

//===========================================================================
uint8_t Cpu::Sbc(uint8_t src) {
    uint32_t acc   = uint32_t(m_regs.a);
    uint32_t sub   = uint32_t(src);
    uint32_t carry = uint32_t(m_regs.ps & PS_CARRY);
    uint32_t val;
    if (m_regs.ps & PS_DECIMAL) {
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

        val = hi | lo;

        SetFlagTo(PS_OVERFLOW, ((acc ^ val) & 0x80) && ((acc ^ sub) & 0x80));
    }
    else {
        val = 0xff + acc - sub + carry;
        SetFlagTo(PS_CARRY, val >= 0x100);
        SetFlagTo(PS_OVERFLOW, ((acc * 0x80) != (sub & 0x80)) && ((acc & 0x80) != (val & 0x80)));
    }

    m_regs.a = uint8_t(val);
    UpdateNZ(m_regs.a);
    return 0;
}

//===========================================================================
uint8_t Cpu::Sec(uint8_t) {
    SetFlag(PS_CARRY);
    return 0;
}

//===========================================================================
uint8_t Cpu::Sed(uint8_t) {
    SetFlag(PS_DECIMAL);
    return 0;
}

//===========================================================================
uint8_t Cpu::Sei(uint8_t) {
    SetFlag(PS_INTERRUPT);
    return 0;
}

//===========================================================================
uint8_t Cpu::Skb(uint8_t) {
    return 0;
}

//===========================================================================
uint8_t Cpu::Skw(uint8_t) {
    return 0;
}

//===========================================================================
uint8_t Cpu::Sta(uint8_t) {
    return m_regs.a;
}

//===========================================================================
uint8_t Cpu::Stx(uint8_t) {
    return m_regs.x;
}

//===========================================================================
uint8_t Cpu::Sty(uint8_t) {
    return m_regs.y;
}

//===========================================================================
uint8_t Cpu::Tas(uint8_t src) {
    uint8_t val = m_regs.a & m_regs.x;
    m_regs.sp = 0x0100 | val;
    return val & (src + 1);
}

//===========================================================================
uint8_t Cpu::Tax(uint8_t) {
    m_regs.x = m_regs.a;
    UpdateNZ(m_regs.x);
    return 0;
}

//===========================================================================
uint8_t Cpu::Tay(uint8_t) {
    m_regs.y = m_regs.a;
    UpdateNZ(m_regs.y);
    return 0;
}

//===========================================================================
uint8_t Cpu::Tsx(uint8_t) {
    m_regs.x = (uint8_t)m_regs.sp;
    UpdateNZ(m_regs.x);
    return 0;
}

//===========================================================================
uint8_t Cpu::Txa(uint8_t) {
    m_regs.a = m_regs.x;
    UpdateNZ(m_regs.a);
    return 0;
}

//===========================================================================
uint8_t Cpu::Txs(uint8_t) {
    m_regs.sp = 0x0100 | m_regs.x;
    return 0;
}

//===========================================================================
uint8_t Cpu::Tya(uint8_t) {
    m_regs.a = m_regs.y;
    UpdateNZ(m_regs.a);
    return 0;
}

//===========================================================================
uint8_t Cpu::Xaa(uint8_t src) {
    m_regs.a = (m_regs.x & src);
    UpdateNZ(m_regs.a);
    return 0;
}

//===========================================================================
uint8_t Cpu::Xas(uint8_t src) {
    return m_regs.x & src;
}
