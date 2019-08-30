;
;  APPLE //E EMULATOR FOR WINDOWS
;
;  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
;

; EAX = Work register
;  BX = Flags
;  CL = X Index
;  CH = Y Index
;  DL = Accumulator
; EDX = Clock ticks left shl 8
; ESI = Program Counter
; EDI = Effective Address
; EBP = Stack Pointer

              .386
              .model small

              assume cs:FLAT,ds:FLAT,es:FLAT,ss:FLAT

public        _CpuExecute@4
public        _CpuGetCode@12
public        _CpuInitialize@36
public        _DllMain@12

;*****************************************************************************
;*
;*  EQUATES
;*
;****

REG_A         equ       <dl>
REG_X         equ       <cl>
REG_Y         equ       <ch>

AF_SIGN       equ       80h
AF_OVERFLOW   equ       40h
AF_RESERVED   equ       20h
AF_BREAK      equ       10h
AF_DECIMAL    equ       08h
AF_INTERRUPT  equ       04h
AF_ZERO       equ       02h
AF_CARRY      equ       01h

EF_OVERFLOW   equ       0800h
EF_SIGN       equ       0080h
EF_ZERO       equ       0040h
EF_RESERVED   equ       0020h
EF_BREAK      equ       0010h
EF_DECIMAL    equ       0008h
EF_INTERRUPT  equ       0004h
EF_CARRY      equ       0001h

IF_OVERFLOW   equ       0800h
IF_SIGN       equ       0080h
IF_ZERO       equ       0040h
IF_AUXCARRY   equ       0010h
IF_PARITY     equ       0004h
IF_CARRY      equ       0001h

;*****************************************************************************
;*
;*  OPCODE JUMP TABLE AND MISCELLANEOUS VARIABLES
;*
;****

              .data

opcodetable   dd        OFFSET Opcode_00
              dd        OFFSET Opcode_01
              dd        OFFSET Opcode_02
              dd        OFFSET Opcode_03
              dd        OFFSET Opcode_04
              dd        OFFSET Opcode_05
              dd        OFFSET Opcode_06
              dd        OFFSET Opcode_07
              dd        OFFSET Opcode_08
              dd        OFFSET Opcode_09
              dd        OFFSET Opcode_0A
              dd        OFFSET Opcode_0B
              dd        OFFSET Opcode_0C
              dd        OFFSET Opcode_0D
              dd        OFFSET Opcode_0E
              dd        OFFSET Opcode_0F
              dd        OFFSET Opcode_10
              dd        OFFSET Opcode_11
              dd        OFFSET Opcode_12
              dd        OFFSET Opcode_13
              dd        OFFSET Opcode_14
              dd        OFFSET Opcode_15
              dd        OFFSET Opcode_16
              dd        OFFSET Opcode_17
              dd        OFFSET Opcode_18
              dd        OFFSET Opcode_19
              dd        OFFSET Opcode_1A
              dd        OFFSET Opcode_1B
              dd        OFFSET Opcode_1C
              dd        OFFSET Opcode_1D
              dd        OFFSET Opcode_1E
              dd        OFFSET Opcode_1F
              dd        OFFSET Opcode_20
              dd        OFFSET Opcode_21
              dd        OFFSET Opcode_22
              dd        OFFSET Opcode_23
              dd        OFFSET Opcode_24
              dd        OFFSET Opcode_25
              dd        OFFSET Opcode_26
              dd        OFFSET Opcode_27
              dd        OFFSET Opcode_28
              dd        OFFSET Opcode_29
              dd        OFFSET Opcode_2A
              dd        OFFSET Opcode_2B
              dd        OFFSET Opcode_2C
              dd        OFFSET Opcode_2D
              dd        OFFSET Opcode_2E
              dd        OFFSET Opcode_2F
              dd        OFFSET Opcode_30
              dd        OFFSET Opcode_31
              dd        OFFSET Opcode_32
              dd        OFFSET Opcode_33
              dd        OFFSET Opcode_34
              dd        OFFSET Opcode_35
              dd        OFFSET Opcode_36
              dd        OFFSET Opcode_37
              dd        OFFSET Opcode_38
              dd        OFFSET Opcode_39
              dd        OFFSET Opcode_3A
              dd        OFFSET Opcode_3B
              dd        OFFSET Opcode_3C
              dd        OFFSET Opcode_3D
              dd        OFFSET Opcode_3E
              dd        OFFSET Opcode_3F
              dd        OFFSET Opcode_40
              dd        OFFSET Opcode_41
              dd        OFFSET Opcode_42
              dd        OFFSET Opcode_43
              dd        OFFSET Opcode_44
              dd        OFFSET Opcode_45
              dd        OFFSET Opcode_46
              dd        OFFSET Opcode_47
              dd        OFFSET Opcode_48
              dd        OFFSET Opcode_49
              dd        OFFSET Opcode_4A
              dd        OFFSET Opcode_4B
              dd        OFFSET Opcode_4C
              dd        OFFSET Opcode_4D
              dd        OFFSET Opcode_4E
              dd        OFFSET Opcode_4F
              dd        OFFSET Opcode_50
              dd        OFFSET Opcode_51
              dd        OFFSET Opcode_52
              dd        OFFSET Opcode_53
              dd        OFFSET Opcode_54
              dd        OFFSET Opcode_55
              dd        OFFSET Opcode_56
              dd        OFFSET Opcode_57
              dd        OFFSET Opcode_58
              dd        OFFSET Opcode_59
              dd        OFFSET Opcode_5A
              dd        OFFSET Opcode_5B
              dd        OFFSET Opcode_5C
              dd        OFFSET Opcode_5D
              dd        OFFSET Opcode_5E
              dd        OFFSET Opcode_5F
              dd        OFFSET Opcode_60
              dd        OFFSET Opcode_61
              dd        OFFSET Opcode_62
              dd        OFFSET Opcode_63
              dd        OFFSET Opcode_64
              dd        OFFSET Opcode_65
              dd        OFFSET Opcode_66
              dd        OFFSET Opcode_67
              dd        OFFSET Opcode_68
              dd        OFFSET Opcode_69
              dd        OFFSET Opcode_6A
              dd        OFFSET Opcode_6B
              dd        OFFSET Opcode_6C
              dd        OFFSET Opcode_6D
              dd        OFFSET Opcode_6E
              dd        OFFSET Opcode_6F
              dd        OFFSET Opcode_70
              dd        OFFSET Opcode_71
              dd        OFFSET Opcode_72
              dd        OFFSET Opcode_73
              dd        OFFSET Opcode_74
              dd        OFFSET Opcode_75
              dd        OFFSET Opcode_76
              dd        OFFSET Opcode_77
              dd        OFFSET Opcode_78
              dd        OFFSET Opcode_79
              dd        OFFSET Opcode_7A
              dd        OFFSET Opcode_7B
              dd        OFFSET Opcode_7C
              dd        OFFSET Opcode_7D
              dd        OFFSET Opcode_7E
              dd        OFFSET Opcode_7F
              dd        OFFSET Opcode_80
              dd        OFFSET Opcode_81
              dd        OFFSET Opcode_82
              dd        OFFSET Opcode_83
              dd        OFFSET Opcode_84
              dd        OFFSET Opcode_85
              dd        OFFSET Opcode_86
              dd        OFFSET Opcode_87
              dd        OFFSET Opcode_88
              dd        OFFSET Opcode_89
              dd        OFFSET Opcode_8A
              dd        OFFSET Opcode_8B
              dd        OFFSET Opcode_8C
              dd        OFFSET Opcode_8D
              dd        OFFSET Opcode_8E
              dd        OFFSET Opcode_8F
              dd        OFFSET Opcode_90
              dd        OFFSET Opcode_91
              dd        OFFSET Opcode_92
              dd        OFFSET Opcode_93
              dd        OFFSET Opcode_94
              dd        OFFSET Opcode_95
              dd        OFFSET Opcode_96
              dd        OFFSET Opcode_97
              dd        OFFSET Opcode_98
              dd        OFFSET Opcode_99
              dd        OFFSET Opcode_9A
              dd        OFFSET Opcode_9B
              dd        OFFSET Opcode_9C
              dd        OFFSET Opcode_9D
              dd        OFFSET Opcode_9E
              dd        OFFSET Opcode_9F
              dd        OFFSET Opcode_A0
              dd        OFFSET Opcode_A1
              dd        OFFSET Opcode_A2
              dd        OFFSET Opcode_A3
              dd        OFFSET Opcode_A4
              dd        OFFSET Opcode_A5
              dd        OFFSET Opcode_A6
              dd        OFFSET Opcode_A7
              dd        OFFSET Opcode_A8
              dd        OFFSET Opcode_A9
              dd        OFFSET Opcode_AA
              dd        OFFSET Opcode_AB
              dd        OFFSET Opcode_AC
              dd        OFFSET Opcode_AD
              dd        OFFSET Opcode_AE
              dd        OFFSET Opcode_AF
              dd        OFFSET Opcode_B0
              dd        OFFSET Opcode_B1
              dd        OFFSET Opcode_B2
              dd        OFFSET Opcode_B3
              dd        OFFSET Opcode_B4
              dd        OFFSET Opcode_B5
              dd        OFFSET Opcode_B6
              dd        OFFSET Opcode_B7
              dd        OFFSET Opcode_B8
              dd        OFFSET Opcode_B9
              dd        OFFSET Opcode_BA
              dd        OFFSET Opcode_BB
              dd        OFFSET Opcode_BC
              dd        OFFSET Opcode_BD
              dd        OFFSET Opcode_BE
              dd        OFFSET Opcode_BF
              dd        OFFSET Opcode_C0
              dd        OFFSET Opcode_C1
              dd        OFFSET Opcode_C2
              dd        OFFSET Opcode_C3
              dd        OFFSET Opcode_C4
              dd        OFFSET Opcode_C5
              dd        OFFSET Opcode_C6
              dd        OFFSET Opcode_C7
              dd        OFFSET Opcode_C8
              dd        OFFSET Opcode_C9
              dd        OFFSET Opcode_CA
              dd        OFFSET Opcode_CB
              dd        OFFSET Opcode_CC
              dd        OFFSET Opcode_CD
              dd        OFFSET Opcode_CE
              dd        OFFSET Opcode_CF
              dd        OFFSET Opcode_D0
              dd        OFFSET Opcode_D1
              dd        OFFSET Opcode_D2
              dd        OFFSET Opcode_D3
              dd        OFFSET Opcode_D4
              dd        OFFSET Opcode_D5
              dd        OFFSET Opcode_D6
              dd        OFFSET Opcode_D7
              dd        OFFSET Opcode_D8
              dd        OFFSET Opcode_D9
              dd        OFFSET Opcode_DA
              dd        OFFSET Opcode_DB
              dd        OFFSET Opcode_DC
              dd        OFFSET Opcode_DD
              dd        OFFSET Opcode_DE
              dd        OFFSET Opcode_DF
              dd        OFFSET Opcode_E0
              dd        OFFSET Opcode_E1
              dd        OFFSET Opcode_E2
              dd        OFFSET Opcode_E3
              dd        OFFSET Opcode_E4
              dd        OFFSET Opcode_E5
              dd        OFFSET Opcode_E6
              dd        OFFSET Opcode_E7
              dd        OFFSET Opcode_E8
              dd        OFFSET Opcode_E9
              dd        OFFSET Opcode_EA
              dd        OFFSET Opcode_EB
              dd        OFFSET Opcode_EC
              dd        OFFSET Opcode_ED
              dd        OFFSET Opcode_EE
              dd        OFFSET Opcode_EF
              dd        OFFSET Opcode_F0
              dd        OFFSET Opcode_F1
              dd        OFFSET Opcode_F2
              dd        OFFSET Opcode_F3
              dd        OFFSET Opcode_F4
              dd        OFFSET Opcode_F5
              dd        OFFSET Opcode_F6
              dd        OFFSET Opcode_F7
              dd        OFFSET Opcode_F8
              dd        OFFSET Opcode_F9
              dd        OFFSET Opcode_FA
              dd        OFFSET Opcode_FB
              dd        OFFSET Opcode_FC
              dd        OFFSET Opcode_FD
              dd        OFFSET Opcode_FE
              dd        OFFSET Opcode_FF
              dd        OFFSET OpcodeEnd

mem           dd        0
IFDEF FASTPAGING
memshadow     dd        0
ENDIF
memwrite      dd        0
IFDEF FASTPAGING
image         dd        0
lastimage     dd        0
ENDIF
regs          dd        0
ioread        dd        0
iowrite       dd        0
dirtypage     dd        0

;*****************************************************************************
;*
;*  GENERAL PURPOSE MACROS
;*
;****

              .code

AF_TO_EF      MACRO
              mov       ah,al
              xor       ebx,ebx
              and       ah,not (AF_OVERFLOW + AF_ZERO)
              or        bl,ah
              and       al,AF_OVERFLOW + AF_ZERO
              shl       eax,5
              or        ebx,eax
              ENDM

EF_TO_AF      MACRO
              mov       eax,ebx
              shr       eax,5
              and       al,AF_OVERFLOW + AF_ZERO
              mov       ah,bl
              and       ah,not (EF_ZERO + 2)
              or        al,ah
              ENDM

EXECNEXT      MACRO CYCLES
              xor       eax,eax
              sub       edx,CYCLES
              mov       al,[esi]
              jc        ReturnToCpp
              jmp       opcodetable[eax*4]
              ENDM

LOAD_MEMORY   MACRO REGISTER
              ; This macro is used only for load instructions: LDA, LDX, and
              ; LDY.  It is somewhat faster than READ_MEMORY because it
              ; avoids the intermediate transfer to al.
              LOCAL LOAD_DONE
              IF _MEM
                IFE _AXDI
                  mov   eax,edi
                ENDIF
                cmp     ah,0C0h
                IF _EAXEDI
                  mov   REGISTER,[eax]
                ELSE
                  mov   REGISTER,[edi]
                ENDIF
                jne     LOAD_DONE
                call    ReadMemory
                mov     REGISTER,al
              ELSE
                IF _EAXEDI
                  mov   REGISTER,[eax]
                ELSE
                  mov   REGISTER,[edi]
                ENDIF
              ENDIF
LOAD_DONE:
              ENDM

POP8          MACRO REGISTER
              mov       eax,ebp
              inc       al
              mov       ebp,eax
              mov       REGISTER,BYTE PTR [eax]
              ENDM

POPPC         MACRO
              push      ebx
              mov       ebx,ebp
              inc       bl
              mov       al,[ebx]
              inc       bl
              mov       ah,[ebx]
              mov       ebp,ebx
              pop       ebx
              mov       si,ax
              ENDM

PUSH8         MACRO REGISTER
IFDEF FASTPAGING
              xchg      ebp,edi
              mov       al,REGISTER
              call      WriteMemory
              mov       eax,edi
              mov       edi,ebp
ELSE
              mov       BYTE PTR [ebp],REGISTER
              mov       eax,ebp
ENDIF
              dec       al
              mov       ebp,eax
              ENDM

PUSHPC        MACRO
              push      ebx
              mov       ax,si
              mov       ebx,ebp
IFDEF FASTPAGING
              push      edi
              xchg      al,ah
              mov       edi,ebx
              call      WriteMemory
              dec       bl
              mov       ax,si
              mov       edi,ebx
              call      WriteMemory
              dec       bl
              pop       edi
ELSE
              mov       [ebx],ah
              dec       bl
              mov       [ebx],al
              dec       bl
ENDIF
              mov       ebp,ebx
              pop       ebx
              ENDM

READ_MEMORY   MACRO
              LOCAL READ_DONE
              IF _MEM
                IFE _AXDI
                  mov   eax,edi
                ENDIF
                cmp     ah,0C0h
                IF _EAXEDI
                  mov   al,[eax]
                ELSE
                  mov   al,[edi]
                ENDIF
                jne     READ_DONE
                call    ReadMemory
              ELSE
                IF _EAXEDI
                  mov   al,[eax]
                ELSE
                  mov   al,[edi]
                ENDIF
              ENDIF
READ_DONE:
              ENDM

SETFLAGS_C    MACRO
              lahf
              and       ah,IF_CARRY
              and       bl,not EF_CARRY
              or        bl,ah
              ENDM

SETFLAGS_NVZC MACRO
              pushfd
              pop       eax
              and       eax,IF_SIGN + IF_OVERFLOW + IF_ZERO + IF_CARRY
              and       ebx,not (EF_SIGN + EF_OVERFLOW + EF_ZERO + EF_CARRY)
              or        ebx,eax
              ENDM

SETFLAGS_NZ   MACRO
              lahf
              and       ah,IF_SIGN + IF_ZERO
              and       bl,not (EF_SIGN + EF_ZERO)
              or        bl,ah
              ENDM

SETFLAGS_NZC  MACRO
              lahf
              and       ah,IF_SIGN + IF_ZERO + IF_CARRY
              and       bl,not (EF_SIGN + EF_ZERO + EF_CARRY)
              or        bl,ah
              ENDM

SETFLAGS_Z    MACRO
              lahf
              and       ah,IF_ZERO
              and       bl,not EF_ZERO
              or        bl,ah
              ENDM

WRITE_MEMORY  MACRO
              LOCAL WRITE_DIRECT
              LOCAL WRITE_DONE
IFDEF FASTPAGING
              call      WriteMemory
ELSE
              IF _MEM
IFDEF MEM64K
                add     di,4000h
                js      WRITE_DIRECT
                sub     di,4000h
                call    WriteMemory
                jmp     WRITE_DONE
WRITE_DIRECT:   sub     di,4000h
                mov     [edi],al
ELSE
                call    WriteMemory
ENDIF
              ELSE
                mov     [edi],al
              ENDIF
ENDIF
WRITE_DONE:
              ENDM

STORE_MEMORY  MACRO VALUE
              LOCAL STORE_DIRECT
              LOCAL STORE_DONE
              ; This macro is used only for store instructions: STA, STX, and
              ; STY.  It is somewhat faster than WRITE_MEMORY because it
              ; avoids the intermediate transfer to al, and because it makes
              ; the assumption that if _AXDI is set, then the di register is
              ; still mirrored in ax.
IFDEF FASTPAGING
              mov       al,VALUE
              call      WriteMemory
ELSE
              IF _MEM
IFDEF MEM64K
                IFE _AXDI
                  mov   eax,edi
                ENDIF
                add     ah,40h
                js      STORE_DIRECT
                mov     al,VALUE
                call    WriteMemory
                jmp     STORE_DONE
STORE_DIRECT:
                mov     [edi],VALUE
ELSE
                mov     al,VALUE
                call    WriteMemory
ENDIF
              ELSE
                mov     [edi],VALUE
              ENDIF
ENDIF
STORE_DONE:
              ENDM

;*****************************************************************************
;*
;*  ADDRESSING MODE MACROS
;*
;****

ADDR_ABS      MACRO                    ; [abs]
              _MEM    = 0FFh           ; absolute addressing
              _AXDI   = 0
              _EAXEDI = 0
              mov       di,[esi+1]
              add       si,3
              ENDM

ADDR_ABSIINDX MACRO                    ; [abs(ind,X)]
              _MEM    = 0FFh           ; absolute indexed indirect addressing
              _AXDI   = 0FFh           ; using X index
              _EAXEDI = 0
              mov       ax,[esi+1]
              add       al,REG_X
              adc       ah,0
              mov       di,ax
              add       si,3
              mov       di,[edi]
              ENDM

ADDR_ABSX     MACRO                    ; [abs,X]
              _MEM    = 0FFh           ; absolute indexed addressing
              _AXDI   = 0FFh           ; using X index
              _EAXEDI = 0FFh
              mov       eax,edi
              mov       ax,[esi+1]
              add       al,REG_X
              adc       ah,0
              add       si,3
              mov       edi,eax
              ENDM

ADDR_ABSY     MACRO                    ; [abs,Y]
              _MEM    = 0FFh           ; absolute indexed addressing
              _AXDI   = 0FFh           ; using Y index
              _EAXEDI = 0FFh
              mov       eax,edi
              mov       ax,[esi+1]
              add       al,REG_Y
              adc       ah,0
              add       si,3
              mov       edi,eax
              ENDM

ADDR_IABS     MACRO                    ; [(abs)]
              _MEM    = 0FFh           ; absolute indirect addressing
              _AXDI   = 0
              _EAXEDI = 0
              mov       di,[esi+1]
              add       si,3
              mov       di,[edi]
              ENDM

ADDR_IMM      MACRO                    ; [imm]
              _MEM    = 0              ; immediate addressing
              _AXDI   = 0
              _EAXEDI = 0
              inc       si
              mov       di,si
              inc       si
              ENDM

ADDR_INDX     MACRO                    ; [ind,X]
              _MEM    = 0FFh           ; zero page indexed indirect addressing
              _AXDI   = 0              ; using X index
              _EAXEDI = 0
              xor       ah,ah
              mov       al,[esi+1]
              add       al,REG_X
              mov       di,ax
              add       si,2
              mov       di,[edi]
              ENDM

ADDR_INDY     MACRO                    ; [ind,Y]
              _MEM    = 0FFh           ; zero page indexed indirect addressing
              _AXDI   = 0              ; using Y index
              _EAXEDI = 0
              xor       ah,ah
              mov       al,[esi+1]
              mov       di,ax
              add       si,2
              mov       di,[edi]
              mov       al,REG_Y
              add       di,ax
              ENDM

ADDR_IZPG     MACRO                    ; [(zpg)]
              _MEM    = 0FFh           ; zero page indirect addressing
              _AXDI   = 0
              _EAXEDI = 0
              xor       ah,ah
              mov       al,[esi+1]
              mov       di,ax
              add       si,2
              mov       di,[edi]
              ENDM

ADDR_ZPG      MACRO                    ; [zpg]
              _MEM    = 0              ; zero page addressing
              _AXDI   = 0FFh
              _EAXEDI = 0
              xor       ah,ah
              mov       al,[esi+1]
              mov       di,ax
              add       si,2
              ENDM

ADDR_ZPG2     MACRO                    ; [zpg]
              _MEM    = 0              ; zero page addressing
              _AXDI   = 0FFh           ; This macro sets only eax, not edi.
              _EAXEDI = 0FFh           
              mov       eax,edi
              xor       ah,ah
              mov       al,[esi+1]
              add       si,2
              ENDM

ADDR_ZPGX     MACRO                    ; [zpg,X]
              _MEM    = 0              ; zero page indexed addressing
              _AXDI   = 0FFh           ; using X index
              _EAXEDI = 0
              xor       ah,ah
              mov       al,[esi+1]
              add       al,REG_X
              mov       di,ax
              add       si,2
              ENDM

ADDR_ZPGY     MACRO                    ; [zpg,Y]
              _MEM    = 0              ; zero page indexed addressing
              _AXDI   = 0FFh           ; using Y index
              _EAXEDI = 0
              xor       ah,ah
              mov       al,[esi+1]
              add       al,REG_Y
              mov       di,ax
              add       si,2
              ENDM

;*****************************************************************************
;*
;*  INSTRUCTION MACROS
;*
;****

INST_ADC      MACRO ADDRMODE           ; Add Memory to Accumulator with Carry
              LOCAL INST_ADC_BIN       ; (NVZC)
              LOCAL INST_ADC_SF
              LOCAL INST_ADC_END
              ADDRMODE
              READ_MEMORY
              test      ebx,EF_DECIMAL
              jz        INST_ADC_BIN
              mov       ah,bl          ; perform a decimal ADC by performing
              shr       ah,1           ; a binary ADC and then decimal
              adc       al,REG_A       ; adjusting the result
              daa
              mov       REG_A,al
IFDEF CPU6502
              SETFLAGS_C
              jmp       INST_ADC_END
ELSE
              jmp       INST_ADC_SF
ENDIF
INST_ADC_BIN: mov       ah,bl          ; perform a binary ADC by moving the
              shr       ah,1           ; emulator's carry flag into the Intel
              adc       REG_A,al       ; carry flag and then doing a real ADC
INST_ADC_SF:  SETFLAGS_NVZC
INST_ADC_END:
              ENDM

INST_AND      MACRO ADDRMODE           ; "AND" Memory with Accumulator
              ADDRMODE                 ; (NZ)
              READ_MEMORY
              and       REG_A,al
              SETFLAGS_NZ
              ENDM

INST_ASL      MACRO ADDRMODE           ; Shift Memory One Bit Left
              ADDRMODE                 ; (NZC)
              READ_MEMORY
              shl       al,1
              SETFLAGS_NZC
              WRITE_MEMORY
              ENDM

INST_ASLA     MACRO                    ; Shift Accumulator One Bit Left
              inc       si             ; (NZC)
              shl       REG_A,1
              SETFLAGS_NZC
              ENDM

INST_BCC      MACRO                    ; Branch on Carry Clear
              LOCAL INST_BCC_NO
              mov       al,[esi+1]
              add       si,2
              test      ebx,EF_CARRY
              mov       ah,al
              jnz       INST_BCC_NO
              sar       ah,7
              add       si,ax
INST_BCC_NO:
              ENDM

INST_BCS      MACRO                    ; Branch on Carry Set
              LOCAL INST_BCS_NO
              mov       al,[esi+1]
              add       si,2
              test      ebx,EF_CARRY
              mov       ah,al
              jz        INST_BCS_NO
              sar       ah,7
              add       si,ax
INST_BCS_NO:
              ENDM

INST_BEQ      MACRO                    ; Branch on Result Zero
              LOCAL INST_BEQ_NO
              mov       al,[esi+1]
              add       si,2
              test      ebx,EF_ZERO
              mov       ah,al
              jz        INST_BEQ_NO
              sar       ah,7
              add       si,ax
INST_BEQ_NO:  
              ENDM

INST_BIT      MACRO ADDRMODE           ; Test Memory Bits with Accumulator
              LOCAL INST_BIT_NZ        ; (NVZ)
              ADDRMODE
              READ_MEMORY
              and       ebx,not (EF_SIGN + EF_OVERFLOW + EF_ZERO)
              test      al,REG_A       ; set the zero flag based on whether
              jnz       INST_BIT_NZ    ; the accumulator AND memory is zero
              or        ebx,EF_ZERO
INST_BIT_NZ:  mov       ah,al          ; set the sign flag to the value of
              and       eax,4080h      ; the memory location's 7th bit
              or        bl,al
              shr       ah,3           ; set the overflow flag to the value
              or        bh,ah          ; of the memory location's 6th bit
              ENDM                     

INST_BITI     MACRO ADDRMODE           ; Test Memory Bits with Accumulator
              ADDRMODE                 ; (Z)
              READ_MEMORY
              and       ebx,not EF_ZERO
              test      al,REG_A       ; set the zero flag based on whether
              SETFLAGS_Z               ; the accumulator AND memory is zero
              ENDM

INST_BMI      MACRO                    ; Branch on Result Minus
              LOCAL INST_BMI_NO
              mov       al,[esi+1]
              add       si,2
              test      ebx,EF_SIGN
              mov       ah,al
              jz        INST_BMI_NO
              sar       ah,7
              add       si,ax
INST_BMI_NO:  
              ENDM

INST_BNE      MACRO                    ; Branch on Result Not Zero
              LOCAL INST_BNE_NO
              mov       al,[esi+1]
              add       si,2
              test      ebx,EF_ZERO
              mov       ah,al
              jnz       INST_BNE_NO
              sar       ah,7
              add       si,ax
INST_BNE_NO:
              ENDM

INST_BPL      MACRO                    ; Branch on Result Plus
              LOCAL INST_BPL_NO
              mov       al,[esi+1]
              add       si,2
              test      ebx,EF_SIGN
              mov       ah,al
              jnz       INST_BPL_NO
              sar       ah,7
              add       si,ax
INST_BPL_NO:
              ENDM

INST_BRA      MACRO                    ; Branch Always
              mov       al,[esi+1]
              add       si,2
              mov       ah,al
              sar       ah,7
              add       si,ax
              ENDM

INST_BRK      MACRO                    ; Force Break
              add       si,2           ; (BI)
              PUSHPC                   ; push the program counter plus two
              or        ebx,EF_BREAK   ; turn on the break flag
              EF_TO_AF
              PUSH8     al             ; push the flags
              or        ebx,EF_INTERRUPT; turn on the interrupt flag
              mov       ax,0FFFEh
              mov       si,ax
              mov       si,[esi]       ; jump to the address stored at 0FFFEh
              ENDM

INST_BVC      MACRO                    ; Branch on Overflow Clear
              LOCAL INST_BVC_NO
              mov       al,[esi+1]
              add       si,2
              test      ebx,EF_OVERFLOW
              mov       ah,al
              jnz       INST_BVC_NO
              sar       ah,7
              add       si,ax
INST_BVC_NO:
              ENDM

INST_BVS      MACRO                    ; Branch on Overflow Set
              LOCAL INST_BVS_NO
              mov       al,[esi+1]
              add       si,2
              test      ebx,EF_OVERFLOW
              mov       ah,al
              jz        INST_BVS_NO
              sar       ah,7
              add       si,ax
INST_BVS_NO:
              ENDM

INST_CLC      MACRO                    ; Clear Carry Flag (C)
              inc       si
              and       ebx,not EF_CARRY
              ENDM

INST_CLD      MACRO                    ; Clear Decimal Mode (D)
              inc       si
              and       ebx,not EF_DECIMAL
              ENDM

INST_CLI      MACRO                    ; Clear Interrupt Disable Bit (I)
              inc       si
              and       ebx,not EF_INTERRUPT
              ENDM

INST_CLV      MACRO                    ; Clear Overflow Flag (V)
              inc       si
              and       ebx,not EF_OVERFLOW
              ENDM

INST_CMP      MACRO ADDRMODE           ; Compare Memory and Accumulator
              ADDRMODE                 ; (NZC)
              READ_MEMORY
              cmp       REG_A,al
              cmc
              SETFLAGS_NZC
              ENDM

INST_CPX      MACRO ADDRMODE           ; Compare Memory and Index X
              ADDRMODE                 ; (NZC)
              READ_MEMORY
              cmp       REG_X,al
              cmc
              SETFLAGS_NZC
              ENDM

INST_CPY      MACRO ADDRMODE           ; Compare Memory and Index Y
              ADDRMODE                 ; (NZC)
              READ_MEMORY
              cmp       REG_Y,al
              cmc
              SETFLAGS_NZC
              ENDM

INST_DEA      MACRO                    ; Decrement Accumulator
              inc       si             ; (NZ)
              dec       REG_A
              SETFLAGS_NZ
              ENDM

INST_DEC      MACRO ADDRMODE           ; Decrement by One
              ADDRMODE                 ; (NZ)
              READ_MEMORY
              dec       al
              SETFLAGS_NZ
              WRITE_MEMORY
              ENDM

INST_DEX      MACRO                    ; Decrement Index X by One
              inc       si             ; (NZ)
              dec       REG_X
              SETFLAGS_NZ
              ENDM

INST_DEY      MACRO                    ; Decrement Index Y by One
              inc       si             ; (NZ)
              dec       REG_Y
              SETFLAGS_NZ
              ENDM

INST_EOR      MACRO ADDRMODE           ; "Exclusive-OR" Memory with Accumulator
              ADDRMODE                 ; (NZ)
              READ_MEMORY
              xor       REG_A,al
              SETFLAGS_NZ
              ENDM

INST_INA      MACRO                    ; Increment Accumulator
              inc       si             ; (NZ)
              inc       REG_A
              SETFLAGS_NZ
              ENDM

INST_INC      MACRO ADDRMODE           ; Increment by One
              ADDRMODE                 ; (NZ)
              READ_MEMORY
              inc       al
              SETFLAGS_NZ
              WRITE_MEMORY
              ENDM

INST_INX      MACRO                    ; Increment Index X by One
              inc       si             ; (NZ)
              inc       REG_X
              SETFLAGS_NZ
              ENDM

INST_INY      MACRO                    ; Increment Index Y by One
              inc       si             ; (NZ)
              inc       REG_Y
              SETFLAGS_NZ
              ENDM

INST_JMP      MACRO ADDRMODE           ; Jump to New Location
              ADDRMODE
              mov       esi,edi
              ENDM

INST_JSR      MACRO ADDRMODE           ; Jump to New Loc. Saving Return Addr.
              ADDRMODE
              dec       si
              PUSHPC
              mov       esi,edi
              ENDM

INST_LDA      MACRO ADDRMODE           ; Load Accumulator with Memory
              ADDRMODE                 ; (NZ)
              LOAD_MEMORY REG_A
              test      REG_A,REG_A
              SETFLAGS_NZ
              ENDM

INST_LDX      MACRO ADDRMODE           ; Load Index X with Memory
              ADDRMODE                 ; (NZ)
              LOAD_MEMORY REG_X
              test      REG_X,REG_X
              SETFLAGS_NZ
              ENDM

INST_LDY      MACRO ADDRMODE           ; Load Index Y with Memory
              ADDRMODE                 ; (NZ)
              LOAD_MEMORY REG_Y
              test      REG_Y,REG_Y
              SETFLAGS_NZ
              ENDM

INST_LSR      MACRO ADDRMODE           ; Shift One Bit Right
              ADDRMODE                 ; (NZC)
              READ_MEMORY
              shr       al,1
              SETFLAGS_NZC
              WRITE_MEMORY
              ENDM

INST_LSRA     MACRO                    ; Shift Accumulator One Bit Right
              inc       si             ; (NZC)
              shr       REG_A,1
              SETFLAGS_NZC
              ENDM

INST_NOP      MACRO                    ; No Operation
              inc       si
              ENDM

INST_ORA      MACRO ADDRMODE           ; "OR" Memory with Accumulator
              ADDRMODE                 ; (NZ)
              READ_MEMORY
              or        REG_A,al
              SETFLAGS_NZ
              ENDM

INST_PHA      MACRO                    ; Push Accumulator on Stack
              inc       si
              PUSH8     REG_A
              ENDM

INST_PHP      MACRO                    ; Push Processor Status on Stack
              inc       si
              EF_TO_AF
              or        al,AF_RESERVED
              PUSH8     al
              ENDM

INST_PHX      MACRO                    ; Push Index X on Stack
              inc       si
              PUSH8     REG_X
              ENDM

INST_PHY      MACRO                    ; Push Index Y on Stack
              inc       si
              PUSH8     REG_Y
              ENDM

INST_PLA      MACRO                    ; Pull Accumulator from Stack
              inc       si             ; (NZ)
              POP8      REG_A
              test      REG_A,REG_A
              SETFLAGS_NZ
              ENDM

INST_PLP      MACRO                    ; Pull Processor Status from Stack
              inc       si             ; (NVBDIZC)
              POP8      al
              AF_TO_EF
              ENDM

INST_PLX      MACRO                    ; Pull Index X from Stack
              inc       si
              POP8      REG_X
              test      REG_X,REG_X
              SETFLAGS_NZ
              ENDM

INST_PLY      MACRO                    ; Pull Index Y from Stack
              inc       si
              POP8      REG_Y
              test      REG_Y,REG_Y
              SETFLAGS_NZ
              ENDM

INST_ROL      MACRO ADDRMODE           ; Rotate One Bit Left
              ADDRMODE                 ; (NZC)
              READ_MEMORY
              mov       ah,bl          ; move emulator's carry flag into the
              shr       ah,1           ; real carry flag and then rotate
              rcl       al,1           ; left one using that flag
              rcl       ah,1           ; save the carry flag
              test      al,al          ; set the negative and zero flags
              rcr       ah,1           ; restore the carry flag
              SETFLAGS_NZC
              WRITE_MEMORY
              ENDM

INST_ROLA     MACRO                    ; Rotate Accumulator One Bit Left
              inc       si             ; (NZC)
              mov       al,bl          ; move emulator's carry flag into the
              shr       al,1           ; real carry flag and then rotate
              rcl       REG_A,1        ; the accum. left one using that flag
              rcl       ah,1           ; save the carry flag
              test      REG_A,REG_A    ; set the negative and zero flags
              rcr       ah,1           ; restore the carry flag
              SETFLAGS_NZC
              ENDM

INST_ROR      MACRO ADDRMODE           ; Rotate One Bit Right
              ADDRMODE                 ; (NZC)
              READ_MEMORY
              mov       ah,bl          ; move emulator's carry flag into the
              shr       ah,1           ; real carry flag and then rotate
              rcr       al,1           ; right one using that flag
              rcl       ah,1           ; save the carry flag
              test      al,al          ; set the negative and zero flags
              rcr       ah,1           ; restore the carry flag
              SETFLAGS_NZC
              WRITE_MEMORY
              ENDM

INST_RORA     MACRO                    ; Rotate Accumulator One Bit Right
              inc       si             ; (NZC)
              mov       al,bl          ; move emulator's carry flag into the
              shr       al,1           ; real carry flag and then rotate
              rcr       REG_A,1        ; the accum. right one using that flag
              rcl       ah,1           ; save the carry flag
              test      REG_A,REG_A    ; set the negative and zero flags
              rcr       ah,1           ; restore the carry flag
              SETFLAGS_NZC
              ENDM

INST_RTI      MACRO                    ; Return from Interrupt
              POP8      al             ; (NVBDIZC)
              AF_TO_EF
              POPPC
              ENDM

INST_RTS      MACRO                    ; Return from Subroutine
              POPPC
              inc       si
              ENDM

INST_SBC      MACRO ADDRMODE           ; Subtract Memory from Acc. with Borrow
              LOCAL INST_SBC_BIN       ; (NVZC)
              LOCAL INST_SBC_SF
              LOCAL INST_SBC_END
              ADDRMODE
              READ_MEMORY
              test      ebx,EF_DECIMAL
              jz        INST_SBC_BIN
              mov       ah,bl          ; perform a decimal SBC by performing
              shr       ah,1           ; a binary SBC and then decimal
              cmc                      ; adjusting the result
              xchg      al,REG_A
              sbb       al,REG_A
              das
              cmc
              mov       REG_A,al
IFDEF CPU6502
              SETFLAGS_C
              jmp       INST_SBC_END
ELSE
              jmp       INST_SBC_SF
ENDIF
INST_SBC_BIN: mov       ah,bl          ; perform a binary SBC by moving the
              shr       ah,1           ; emulator's carry flag into the Intel
              cmc                      ; carry flag and then doing a real SBB
              sbb       REG_A,al
              cmc
INST_SBC_SF:  SETFLAGS_NVZC
INST_SBC_END:
              ENDM

INST_SEC      MACRO                    ; Set Carry Flag (C)
              inc       si
              or        ebx,EF_CARRY
              ENDM

INST_SED      MACRO                    ; Set Decimal Mode (D)
              inc       si
              or        ebx,EF_DECIMAL
              ENDM

INST_SEI      MACRO                    ; Set Interrupt Disable Bit (I)
              inc       si
              or        ebx,EF_INTERRUPT
              ENDM

INST_STA      MACRO ADDRMODE           ; Store Accumulator in Memory
              ADDRMODE
              STORE_MEMORY REG_A
              ENDM

INST_STX      MACRO ADDRMODE           ; Store Index X in Memory
              ADDRMODE
              STORE_MEMORY REG_X
              ENDM

INST_STY      MACRO ADDRMODE           ; Store Index Y in Memory
              ADDRMODE
              STORE_MEMORY REG_Y
              ENDM

INST_STZ      MACRO ADDRMODE           ; Store Zero in Memory
              ADDRMODE
              xor       al,al
              WRITE_MEMORY
              ENDM

INST_TAX      MACRO                    ; Transfer Accumulator to Index X
              inc       si             ; (NZ)
              mov       REG_X,REG_A    
              test      REG_X,REG_X
              SETFLAGS_NZ
              ENDM

INST_TAY      MACRO                    ; Transfer Accumulator to Index Y
              inc       si             ; (NZ)
              mov       REG_Y,REG_A
              test      REG_Y,REG_Y
              SETFLAGS_NZ
              ENDM

INST_TRB      MACRO ADDRMODE           ; Test and Reset Mem. Bits with Accum.
              ADDRMODE                 ; (Z)
              READ_MEMORY
              mov       ah,al
              not       REG_A
              and       al,REG_A
              not       REG_A
              and       ah,REG_A
              SETFLAGS_Z
              WRITE_MEMORY
              ENDM

INST_TSB      MACRO ADDRMODE           ; Test and Set Mem. Bits with Accum.
              ADDRMODE                 ; (Z)
              READ_MEMORY
              mov       ah,al
              or        al,REG_A
              and       ah,REG_A
              SETFLAGS_Z
              WRITE_MEMORY
              ENDM

INST_TSX      MACRO                    ; Transfer Stack Pointer to Index X
              inc       si             ; (NZ)
              mov       eax,ebp
              mov       REG_X,al
              test      al,al
              SETFLAGS_NZ
              ENDM

INST_TXA      MACRO                    ; Transfer Index X to Accumulator
              inc       si             ; (NZ)
              mov       REG_A,REG_X    
              test      REG_A,REG_A
              SETFLAGS_NZ
              ENDM

INST_TXS      MACRO                    ; Transfer Index X to Stack Pointer
              inc       si
              mov       eax,ebp
              mov       al,REG_X
              mov       ebp,eax
              ENDM

INST_TYA      MACRO                    ; Transfer Index Y to Accumulator
              inc       si             ; (NZ)
              mov       REG_A,REG_Y    
              test      REG_A,REG_A
              SETFLAGS_NZ
              ENDM

INVALID1      MACRO                    ; Invalid Opcode (1 byte)
              inc       si
              ENDM

INVALID2      MACRO                    ; Invalid Opcode (2 bytes)
              inc       si
              ENDM

INVALID3      MACRO                    ; Invalid Opcode (3 bytes)
              inc       si
              ENDM

;*****************************************************************************
;*
;*  MOST COMMON OPCODES
;*
;****

              org 200h
Opcode_06:    INST_ASL  ADDR_ZPG
              EXECNEXT  500h
              align     4
Opcode_09:    INST_ORA  ADDR_IMM
              EXECNEXT  200h
              align     4
Opcode_10:    INST_BPL
              EXECNEXT  300h
              align     4
Opcode_16:    INST_ASL  ADDR_ZPGX
              EXECNEXT  600h
              align     4
Opcode_18:    INST_CLC
              EXECNEXT  200h
              align     4
Opcode_19:    INST_ORA  ADDR_ABSY
              EXECNEXT  400h
              align     4
Opcode_20:    INST_JSR  ADDR_ABS
              EXECNEXT  600h
              align     4
Opcode_24:    INST_BIT  ADDR_ZPG
              EXECNEXT  300h
              align     4
Opcode_29:    INST_AND  ADDR_IMM
              EXECNEXT  200h
              align     4
Opcode_2A:    INST_ROLA
              EXECNEXT  200h
              align     4
Opcode_30:    INST_BMI
              EXECNEXT  300h
              align     4
Opcode_38:    INST_SEC
              EXECNEXT  200h
              align     4
Opcode_45:    INST_EOR  ADDR_ZPG
              EXECNEXT  300h
              align     4
Opcode_48:    INST_PHA
              EXECNEXT  300h
              align     4
Opcode_49:    INST_EOR  ADDR_IMM
              EXECNEXT  200h
              align     4
Opcode_4A:    INST_LSRA
              EXECNEXT  200h
              align     4
Opcode_4C:    INST_JMP  ADDR_ABS
              EXECNEXT  300h
              align     4
Opcode_60:    INST_RTS
              EXECNEXT  600h
              align     4
Opcode_65:    INST_ADC  ADDR_ZPG
              EXECNEXT  300h
              align     4
Opcode_68:    INST_PLA
              EXECNEXT  400h
              align     4
Opcode_69:    INST_ADC  ADDR_IMM
              EXECNEXT  200h
              align     4
Opcode_6A:    INST_RORA
              EXECNEXT  200h
              align     4
Opcode_6D:    INST_ADC  ADDR_ABS
              EXECNEXT  400h
              align     4
Opcode_76:    INST_ROR  ADDR_ZPGX
              EXECNEXT  600h
              align     4
Opcode_84:    INST_STY  ADDR_ZPG
              EXECNEXT  300h
              align     4
Opcode_85:    INST_STA  ADDR_ZPG
              EXECNEXT  300h
              align     4
Opcode_86:    INST_STX  ADDR_ZPG
              EXECNEXT  300h
              align     4
Opcode_88:    INST_DEY
              EXECNEXT  200h
              align     4
Opcode_8A:    INST_TXA
              EXECNEXT  200h
              align     4
Opcode_8D:    INST_STA  ADDR_ABS
              EXECNEXT  400h
              align     4
Opcode_90:    INST_BCC
              EXECNEXT  300h
              align     4
Opcode_91:    INST_STA  ADDR_INDY
              EXECNEXT  600h
              align     4
Opcode_94:    INST_STY  ADDR_ZPGX
              EXECNEXT  400h
              align     4
Opcode_99:    INST_STA  ADDR_ABSY
              EXECNEXT  500h
              align     4
Opcode_9A:    INST_TXS
              EXECNEXT  200h
              align     4
Opcode_9D:    INST_STA  ADDR_ABSX
              EXECNEXT  500h
              align     4
Opcode_A0:    INST_LDY  ADDR_IMM
              EXECNEXT  200h
              align     4
Opcode_A2:    INST_LDX  ADDR_IMM
              EXECNEXT  200h
              align     4
Opcode_A4:    INST_LDY  ADDR_ZPG
              EXECNEXT  300h
              align     4
Opcode_A5:    INST_LDA  ADDR_ZPG2
              EXECNEXT  300h
              align     4
Opcode_A6:    INST_LDX  ADDR_ZPG
              EXECNEXT  300h
              align     4
Opcode_A8:    INST_TAY
              EXECNEXT  200h
              align     4
Opcode_A9:    INST_LDA  ADDR_IMM
              EXECNEXT  200h
              align     4
Opcode_AA:    INST_TAX
              EXECNEXT  200h
              align     4
Opcode_AD:    INST_LDA  ADDR_ABS
              EXECNEXT  400h
              align     4
Opcode_B0:    INST_BCS
              EXECNEXT  300h
              align     4
Opcode_B1:    INST_LDA  ADDR_INDY
              EXECNEXT  500h
              align     4
Opcode_B4:    INST_LDY  ADDR_ZPGX
              EXECNEXT  400h
              align     4
Opcode_B9:    INST_LDA  ADDR_ABSY
              EXECNEXT  400h
              align     4
Opcode_BA:    INST_TSX
              EXECNEXT  200h
              align     4
Opcode_BD:    INST_LDA  ADDR_ABSX
              EXECNEXT  400h
              align     4
Opcode_C0:    INST_CPY  ADDR_IMM
              EXECNEXT  200h
              align     4
Opcode_C4:    INST_CPY  ADDR_ZPG
              EXECNEXT  300h
              align     4
Opcode_C5:    INST_CMP  ADDR_ZPG
              EXECNEXT  300h
              align     4
Opcode_C8:    INST_INY
              EXECNEXT  200h
              align     4
Opcode_C9:    INST_CMP  ADDR_IMM
              EXECNEXT  200h
              align     4
Opcode_CA:    INST_DEX
              EXECNEXT  200h
              align     4
Opcode_D0:    INST_BNE
              EXECNEXT  300h
              align     4
Opcode_DD:    INST_CMP  ADDR_ABSX
              EXECNEXT  400h
              align     4
Opcode_E0:    INST_CPX  ADDR_IMM
              EXECNEXT  200h
              align     4
Opcode_E6:    INST_INC  ADDR_ZPG
              EXECNEXT  500h
              align     4
Opcode_E8:    INST_INX
              EXECNEXT  200h
              align     4
Opcode_E9:    INST_SBC  ADDR_IMM
              EXECNEXT  200h
              align     4
Opcode_EA:    INST_NOP
              EXECNEXT  200h
              align     4
Opcode_ED:    INST_SBC  ADDR_ABS
              EXECNEXT  400h
              align     4
Opcode_EE:    INST_INC  ADDR_ABS
              EXECNEXT  600h
              align     4
Opcode_F0:    INST_BEQ
              EXECNEXT  300h

;*****************************************************************************
;*
;*  LESS COMMON OPCODES
;*
;****

              align     4
Opcode_00:    INST_BRK
              EXECNEXT  700h
              align     4
Opcode_01:    INST_ORA  ADDR_INDX
              EXECNEXT  600h
Opcode_02:    INVALID2
              EXECNEXT  200h
Opcode_03:    INVALID1
              EXECNEXT  100h
IFDEF CPU6502
Opcode_04:    INVALID2
ELSE
              align     4
Opcode_04:    INST_TSB  ADDR_ZPG
ENDIF
              EXECNEXT  500h
              align     4
Opcode_05:    INST_ORA  ADDR_ZPG
              EXECNEXT  300h
Opcode_07:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_08:    INST_PHP
              EXECNEXT  300h
              align     4
Opcode_0A:    INST_ASLA
              EXECNEXT  200h
Opcode_0B:    INVALID1
              EXECNEXT  100h
IFDEF CPU6502
Opcode_0C:    INVALID3
ELSE
              align     4
Opcode_0C:    INST_TSB  ADDR_ABS
ENDIF
              EXECNEXT  600h
              align     4
Opcode_0D:    INST_ORA  ADDR_ABS
              EXECNEXT  400h
              align     4
Opcode_0E:    INST_ASL  ADDR_ABS
              EXECNEXT  600h
Opcode_0F:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_11:    INST_ORA  ADDR_INDY
              EXECNEXT  500h
IFDEF CPU6502
Opcode_12:    INVALID2
ELSE
              align     4
Opcode_12:    INST_ORA  ADDR_IZPG
ENDIF
              EXECNEXT  500h
Opcode_13:    INVALID1
              EXECNEXT  100h
IFDEF CPU6502
Opcode_14:    INVALID2
ELSE
              align     4
Opcode_14:    INST_TRB  ADDR_ZPG
ENDIF
              EXECNEXT  500h
              align     4
Opcode_15:    INST_ORA  ADDR_ZPGX
              EXECNEXT  400h
Opcode_17:    INVALID1
              EXECNEXT  100h
IFDEF CPU6502
Opcode_1A:    INVALID1
ELSE
              align     4
Opcode_1A:    INST_INA
ENDIF
              EXECNEXT  200h
Opcode_1B:    INVALID1
              EXECNEXT  100h
IFDEF CPU6502
Opcode_1C:    INVALID3
ELSE
              align     4
Opcode_1C:    INST_TRB  ADDR_ABS
ENDIF
              EXECNEXT  600h
              align     4
Opcode_1D:    INST_ORA  ADDR_ABSX
              EXECNEXT  400h
              align     4
Opcode_1E:    INST_ASL  ADDR_ABSX
IFDEF CPU6502
              EXECNEXT  700h
ELSE
              EXECNEXT  600h
ENDIF
Opcode_1F:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_21:    INST_AND  ADDR_INDX
              EXECNEXT  600h
Opcode_22:    INVALID2
              EXECNEXT  200h
Opcode_23:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_25:    INST_AND  ADDR_ZPG
              EXECNEXT  300h
              align     4
Opcode_26:    INST_ROL  ADDR_ZPG
              EXECNEXT  500h
Opcode_27:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_28:    INST_PLP
              EXECNEXT  400h
Opcode_2B:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_2C:    INST_BIT  ADDR_ABS
              EXECNEXT  400h
              align     4
Opcode_2D:    INST_AND  ADDR_ABS
              EXECNEXT  200h
              align     4
Opcode_2E:    INST_ROL  ADDR_ABS
              EXECNEXT  600h
Opcode_2F:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_31:    INST_AND  ADDR_INDY
              EXECNEXT  500h
IFDEF CPU6502
Opcode_32:    INVALID2
ELSE
              align     4
Opcode_32:    INST_AND  ADDR_IZPG
ENDIF
              EXECNEXT  500h
Opcode_33:    INVALID1
              EXECNEXT  100h
IFDEF CPU6502
Opcode_34:    INVALID2
ELSE
              align     4
Opcode_34:    INST_BIT  ADDR_ZPGX
ENDIF
              EXECNEXT  400h
              align     4
Opcode_35:    INST_AND  ADDR_ZPGX
              EXECNEXT  400h
              align     4
Opcode_36:    INST_ROL  ADDR_ZPGX
              EXECNEXT  600h
Opcode_37:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_39:    INST_AND  ADDR_ABSY
              EXECNEXT  400h
IFDEF CPU6502
Opcode_3A:    INVALID1
ELSE
              align     4
Opcode_3A:    INST_DEA
ENDIF
              EXECNEXT  200h
Opcode_3B:    INVALID1
              EXECNEXT  100h
IFDEF CPU6502
Opcode_3C:    INVALID3
ELSE
              align     4
Opcode_3C:    INST_BIT  ADDR_ABSX
ENDIF
              EXECNEXT  400h
              align     4
Opcode_3D:    INST_AND  ADDR_ABSX
              EXECNEXT  400h
              align     4
Opcode_3E:    INST_ROL  ADDR_ABSX
IFDEF CPU6502
              EXECNEXT  700h
ELSE
              EXECNEXT  600h
ENDIF
Opcode_3F:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_40:    INST_RTI
              EXECNEXT  600h
              align     4
Opcode_41:    INST_EOR  ADDR_INDX
              EXECNEXT  600h
Opcode_42:    INVALID2
              EXECNEXT  200h
Opcode_43:    INVALID1
              EXECNEXT  100h
Opcode_44:    INVALID2
              EXECNEXT  300h
              align     4
Opcode_46:    INST_LSR  ADDR_ZPG
              EXECNEXT  500h
Opcode_47:    INVALID1
              EXECNEXT  100h
Opcode_4B:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_4D:    INST_EOR  ADDR_ABS
              EXECNEXT  400h
              align     4
Opcode_4E:    INST_LSR  ADDR_ABS
              EXECNEXT  600h
Opcode_4F:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_50:    INST_BVC
              EXECNEXT  300h
              align     4
Opcode_51:    INST_EOR  ADDR_INDY
              EXECNEXT  500h
IFDEF CPU6502
Opcode_52:    INVALID2
ELSE
              align     4
Opcode_52:    INST_EOR  ADDR_IZPG
ENDIF
              EXECNEXT  500h
Opcode_53:    INVALID1
              EXECNEXT  100h
Opcode_54:    INVALID2
              EXECNEXT  400h
              align     4
Opcode_55:    INST_EOR  ADDR_ZPGX
              EXECNEXT  400h
              align     4
Opcode_56:    INST_LSR  ADDR_ZPGX
              EXECNEXT  600h
Opcode_57:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_58:    INST_CLI
              EXECNEXT  200h
              align     4
Opcode_59:    INST_EOR  ADDR_ABSY
              EXECNEXT  400h
IFDEF CPU6502
Opcode_5A:    INVALID1
ELSE
              align     4
Opcode_5A:    INST_PHY
ENDIF
              EXECNEXT  300h
Opcode_5B:    INVALID1
              EXECNEXT  100h
Opcode_5C:    INVALID3
              EXECNEXT  800h
              align     4
Opcode_5D:    INST_EOR  ADDR_ABSX
              EXECNEXT  400h
              align     4
Opcode_5E:    INST_LSR  ADDR_ABSX
IFDEF CPU6502
              EXECNEXT  700h
ELSE
              EXECNEXT  600h
ENDIF
Opcode_5F:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_61:    INST_ADC  ADDR_INDX
              EXECNEXT  600h
Opcode_62:    INVALID2
              EXECNEXT  200h
Opcode_63:    INVALID1
              EXECNEXT  100h
IFDEF CPU6502
Opcode_64:    INVALID2
ELSE
              align     4
Opcode_64:    INST_STZ  ADDR_ZPG
ENDIF
              EXECNEXT  300h
              align     4
Opcode_66:    INST_ROR  ADDR_ZPG
              EXECNEXT  500h
Opcode_67:    INVALID1
              EXECNEXT  100h
Opcode_6B:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_6C:    INST_JMP  ADDR_IABS
IFDEF CPU6502
              EXECNEXT  500h
ELSE
              EXECNEXT  600h
ENDIF
              align     4
Opcode_6E:    INST_ROR  ADDR_ABS
              EXECNEXT  600h
Opcode_6F:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_70:    INST_BVS
              EXECNEXT  300h
              align     4
Opcode_71:    INST_ADC  ADDR_INDY
              EXECNEXT  500h
IFDEF CPU6502
Opcode_72:    INVALID2
ELSE
              align     4
Opcode_72:    INST_ADC  ADDR_IZPG
ENDIF
              EXECNEXT  500h
Opcode_73:    INVALID1
              EXECNEXT  100h
IFDEF CPU6502
Opcode_74:    INVALID2
ELSE
              align     4
Opcode_74:    INST_STZ  ADDR_ZPGX
ENDIF
              EXECNEXT  400h
              align     4
Opcode_75:    INST_ADC  ADDR_ZPGX
              EXECNEXT  400h
Opcode_77:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_78:    INST_SEI
              EXECNEXT  200h
              align     4
Opcode_79:    INST_ADC  ADDR_ABSY
              EXECNEXT  400h
IFDEF CPU6502
Opcode_7A:    INVALID1
ELSE
              align     4
Opcode_7A:    INST_PLY
ENDIF
              EXECNEXT  400h
Opcode_7B:    INVALID1
              EXECNEXT  100h
IFDEF CPU6502
Opcode_7C:    INVALID3
ELSE
              align     4
Opcode_7C:    INST_JMP  ADDR_ABSIINDX
ENDIF
              EXECNEXT  600h
              align     4
Opcode_7D:    INST_ADC  ADDR_ABSX
              EXECNEXT  400h
              align     4
Opcode_7E:    INST_ROR  ADDR_ABSX
IFDEF CPU6502
              EXECNEXT  700h
ELSE
              EXECNEXT  600h
ENDIF
Opcode_7F:    INVALID1
              EXECNEXT  100h
IFDEF CPU6502
Opcode_80:    INVALID2
ELSE
              align     4
Opcode_80:    INST_BRA
ENDIF
              EXECNEXT  300h
              align     4
Opcode_81:    INST_STA  ADDR_INDX
              EXECNEXT  600h
Opcode_82:    INVALID2
              EXECNEXT  200h
Opcode_83:    INVALID1
              EXECNEXT  100h
Opcode_87:    INVALID1
              EXECNEXT  100h
IFDEF CPU6502
Opcode_89:    INVALID2
ELSE
              align     4
Opcode_89:    INST_BITI ADDR_IMM
ENDIF
              EXECNEXT  200h
Opcode_8B:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_8C:    INST_STY  ADDR_ABS
              EXECNEXT  400h
              align     4
Opcode_8E:    INST_STX  ADDR_ABS
              EXECNEXT  400h
Opcode_8F:    INVALID1
              EXECNEXT  100h
IFDEF CPU6502
Opcode_92:    INVALID2
ELSE
              align     4
Opcode_92:    INST_STA  ADDR_IZPG
ENDIF
              EXECNEXT  500h
Opcode_93:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_95:    INST_STA  ADDR_ZPGX
              EXECNEXT  400h
              align     4
Opcode_96:    INST_STX  ADDR_ZPGY
              EXECNEXT  400h
Opcode_97:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_98:    INST_TYA
              EXECNEXT  200h
Opcode_9B:    INVALID1
              EXECNEXT  100h
IFDEF CPU6502
Opcode_9C:    INVALID3
ELSE
              align     4
Opcode_9C:    INST_STZ  ADDR_ABS
ENDIF
              EXECNEXT  400h
IFDEF CPU6502
Opcode_9E:    INVALID3
ELSE
              align     4
Opcode_9E:    INST_STZ  ADDR_ABSX
ENDIF
              EXECNEXT  500h
Opcode_9F:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_A1:    INST_LDA  ADDR_INDX
              EXECNEXT  600h
Opcode_A3:    INVALID1
              EXECNEXT  100h
Opcode_A7:    INVALID1
              EXECNEXT  100h
Opcode_AB:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_AC:    INST_LDY  ADDR_ABS
              EXECNEXT  400h
              align     4
Opcode_AE:    INST_LDX  ADDR_ABS
              EXECNEXT  400h
Opcode_AF:    INVALID1
              EXECNEXT  100h
IFDEF CPU6502
Opcode_B2:    INVALID2
ELSE
              align     4
Opcode_B2:    INST_LDA  ADDR_IZPG
ENDIF
              EXECNEXT  500h
Opcode_B3:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_B5:    INST_LDA  ADDR_ZPGX
              EXECNEXT  400h
              align     4
Opcode_B6:    INST_LDX  ADDR_ZPGY
              EXECNEXT  400h
Opcode_B7:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_B8:    INST_CLV
              EXECNEXT  200h
Opcode_BB:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_BC:    INST_LDY  ADDR_ABSX
              EXECNEXT  400h
              align     4
Opcode_BE:    INST_LDX  ADDR_ABSY
              EXECNEXT  400h
Opcode_BF:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_C1:    INST_CMP  ADDR_INDX
              EXECNEXT  600h
Opcode_C2:    INVALID2
              EXECNEXT  200h
Opcode_C3:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_C6:    INST_DEC  ADDR_ZPG
              EXECNEXT  500h
Opcode_C7:    INVALID1
              EXECNEXT  100h
Opcode_CB:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_CC:    INST_CPY  ADDR_ABS
              EXECNEXT  400h
              align     4
Opcode_CD:    INST_CMP  ADDR_ABS
              EXECNEXT  400h
              align     4
Opcode_CE:    INST_DEC  ADDR_ABS
              EXECNEXT  500h
Opcode_CF:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_D1:    INST_CMP  ADDR_INDY
              EXECNEXT  500h
IFDEF CPU6502
Opcode_D2:    INVALID2
ELSE
              align     4
Opcode_D2:    INST_CMP  ADDR_IZPG
ENDIF
              EXECNEXT  500h
Opcode_D3:    INVALID1
              EXECNEXT  100h
Opcode_D4:    INVALID2
              EXECNEXT  400h
              align     4
Opcode_D5:    INST_CMP  ADDR_ZPGX
              EXECNEXT  400h
              align     4
Opcode_D6:    INST_DEC  ADDR_ZPGX
              EXECNEXT  600h
Opcode_D7:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_D8:    INST_CLD
              EXECNEXT  200h
              align     4
Opcode_D9:    INST_CMP  ADDR_ABSY
              EXECNEXT  400h
IFDEF CPU6502
Opcode_DA:    INVALID1
ELSE
              align     4
Opcode_DA:    INST_PHX
ENDIF
              EXECNEXT  300h
Opcode_DB:    INVALID1
              EXECNEXT  100h
Opcode_DC:    INVALID3
              EXECNEXT  400h
              align     4
Opcode_DE:    INST_DEC  ADDR_ABSX
IFDEF CPU6502
              EXECNEXT  700h
ELSE
              EXECNEXT  600h
ENDIF
Opcode_DF:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_E1:    INST_SBC  ADDR_INDX
              EXECNEXT  600h
Opcode_E2:    INVALID2
              EXECNEXT  200h
Opcode_E3:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_E4:    INST_CPX  ADDR_ZPG
              EXECNEXT  300h
              align     4
Opcode_E5:    INST_SBC  ADDR_ZPG
              EXECNEXT  300h
Opcode_E7:    INVALID1
              EXECNEXT  100h
Opcode_EB:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_EC:    INST_CPX  ADDR_ABS
              EXECNEXT  400h
Opcode_EF:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_F1:    INST_SBC  ADDR_INDY
              EXECNEXT  500h
IFDEF CPU6502
Opcode_F2:    INVALID2
ELSE
              align     4
Opcode_F2:    INST_SBC  ADDR_IZPG
ENDIF
              EXECNEXT  500h
Opcode_F3:    INVALID1
              EXECNEXT  100h
Opcode_F4:    INVALID2
              EXECNEXT  400h
              align     4
Opcode_F5:    INST_SBC  ADDR_ZPGX
              EXECNEXT  400h
              align     4
Opcode_F6:    INST_INC  ADDR_ZPGX
              EXECNEXT  600h
Opcode_F7:    INVALID1
              EXECNEXT  100h
              align     4
Opcode_F8:    INST_SED
              EXECNEXT  200h
              align     4
Opcode_F9:    INST_SBC  ADDR_ABSY
              EXECNEXT  400h
IFDEF CPU6502
Opcode_FA:    INVALID1
ELSE
              align     4
Opcode_FA:    INST_PLX
ENDIF
              EXECNEXT  400h
Opcode_FB:    INVALID1
              EXECNEXT  100h
Opcode_FC:    INVALID3
              EXECNEXT  400h
              align     4
Opcode_FD:    INST_SBC  ADDR_ABSX
              EXECNEXT  400h
              align     4
Opcode_FE:    INST_INC  ADDR_ABSX
IFDEF CPU6502
              EXECNEXT  700h
ELSE
              EXECNEXT  600h
ENDIF
Opcode_FF:    INVALID1
              EXECNEXT  100h
OpcodeEnd:

;*****************************************************************************
;*
;*  SUPPORT FUNCTIONS
;*
;****

              align     4
ReadMemory    PROC
              mov       eax,edi        ; look up the function that handles
              push      ebx            ; reads from this address
              and       eax,0FFh
              mov       ebx,[ioread]
              mov       eax,DWORD PTR [ebx+eax*4]
              xor       ebx,ebx
              push      ecx
              push      edx
              push      ebx            
              push      ebx
              push      edi
              push      esi
              call      eax
IFDEF FASTPAGING
              mov       ebx,ebp        ; reset the high-order word of every
              mov       ecx,esi        ; register that is used as a pointer,
              mov       edx,edi        ; in case we have switched to a
              mov       ebp,[mem]      ; different memory image
              mov       esi,ebp
              mov       edi,ebp
              mov       bp,bx
              mov       si,cx
              mov       di,dx
ENDIF
              pop       edx
              pop       ecx
              pop       ebx
              retn
ReadMemory    ENDP

RestoreRegs   PROC
              mov       REG_A,[edi]    ; restore the accumulator
              mov       REG_X,[edi+1]  ; restore index X
              mov       REG_Y,[edi+2]  ; restore index Y
              mov       al,[edi+3]     ; restore the processor status flags
              AF_TO_EF
              mov       si,[edi+4]     ; restore the program counter
              mov       ebp,esi        ; restore the stack pointer
              mov       ah,1
              mov       al,[edi+6]
              mov       bp,ax
              mov       edi,esi
              retn
RestoreRegs   ENDP

SaveRegs      PROC
              mov       [edi],REG_A    ; save the accumulator
              mov       [edi+1],REG_X  ; save index X
              mov       [edi+2],REG_Y  ; save index Y
              EF_TO_AF
              mov       [edi+3],al     ; save the processor status flags
              mov       [edi+4],si     ; save the program counter
              mov       [edi+6],bp     ; save the stack pointer
              retn
SaveRegs      ENDP

              align     4
WriteMemory   PROC
              push      ebx            ; on entry: edi = target address
              push      ecx            ;           al  = value to write
                                       ; this function wipes out ah
IFDEF FASTPAGING
              push      edx
              xor       ebx,ebx        ; determine the page number and store
              mov       edx,edi        ; it in ebx
              mov       bl,dh
              mov       ecx,[dirtypage]; mark the page dirty
              mov       BYTE PTR [ecx+ebx],0FFh
              mov       edx,[image]    ; look up the location for writes to
              mov       ecx,[memwrite] ; this page in this memory image
              shl       edx,10         
              add       ecx,edx
              mov       ecx,[ecx+ebx*4]
              test      ecx,ecx        ; if the location is null, then check
              jz        WriteMemIo     ; for an I/O write

              mov       edx,edi        ; find the memory that is ultimately
              mov       dl,cl          ; backing writes to this location,
              cmp       edx,ecx        ; and store a pointer to that memory
              jne       WriteUpdate    ; in ecx
              mov       edx,[image]
              shl       edx,10
              mov       ecx,[memshadow]
              add       ecx,edx
              mov       ecx,[ecx+ebx*4]
WriteUpdate:  mov       edx,edi        ; write to the backing memory
              and       edx,0FFh       
              mov       [ecx+edx],al

              push      esi            ; update each memory image that is
              push      edi            ; shadowing the backing memory
              mov       bh,BYTE PTR [lastimage]
              mov       esi,[memwrite] ; compute the address of the last
              mov       edx,ebx        ; memory image and place it in edi
              mov       dl,0           
              mov       edx,[esi+edx*4]
              mov       dx,di
              mov       edi,edx
              mov       esi,[memshadow]; place the address of the shadow
              jmp       WriteLoop2     ; table array in esi
              align     4
WriteLoop:    sub       edi,10000h     ; move to the next memory image
              dec       bh
              js        WriteLoop3
WriteLoop2:   mov       edx,[esi+ebx*4]; determine whether this memory image
              cmp       edx,ecx        ; shadows the backing memory
              jne       WriteLoop
              mov       [edi],al       ; if it does, then write to it

              sub       edi,10000h     ; move to the next memory image
              dec       bh
              js        WriteLoop3
              mov       edx,[esi+ebx*4]; determine whether this memory image
              cmp       edx,ecx        ; shadows the backing memory
              jne       WriteLoop
              mov       [edi],al       ; if it does, then write to it
              jmp       WriteLoop
              align     4
WriteLoop3:   pop       edi
              pop       esi

WriteMemRet:  pop       edx
              pop       ecx
              pop       ebx
              retn
ELSE
              mov       ecx,edi        ; store the page number in ebx, and
              mov       ebx,[dirtypage]; mark that page dirty
              mov       bl,ch
IFDEF MEM64K
              cmp       ch,0C0h
ENDIF
              mov       BYTE PTR [ebx],0FFh
IFDEF MEM64K
              jae       WriteMem2
              mov       [edi],al
              pop       ecx
              pop       ebx
              retn
ENDIF
WriteMem2:    and       ebx,0FFh
              mov       ecx,[memwrite] ; look up the location for writes to
              mov       ecx,[ecx+ebx*4]; this page
              test      ecx,ecx        ; if the location is null, then check
              jz        WriteMemIo     ; for an I/O write
              mov       ebx,edi        ; otherwise, compute an offset into
              mov       cl,bl          ; the page, and write there
              mov       [ecx],al
WriteMemRet:  pop       ecx
              pop       ebx
              retn
ENDIF
              align     4
WriteMemIo:   cmp       ebx,0C0h       ; check the page number in ebx to see
              jne       WriteMemRet    ; if it is the I/O page
              mov       ebx,edi        ; if so, compute an offset into the
              and       ebx,0FFh       ; page, and look up the offset in the
              mov       ecx,[iowrite]  ; I/O write table
              mov       ecx,[ecx+ebx*4]
IFNDEF FASTPAGING
              push      edx
ENDIF
              push      eax            ; call the I/O function whose pointer
              push      1              ; is stored in the I/O write table
              push      edi
              push      esi
              call      ecx
IFDEF FASTPAGING
              mov       ebx,ebp        ; reset the high-order word of every
              mov       ecx,esi        ; register that is used as a pointer,
              mov       edx,edi        ; in case we have switched to a
              mov       ebp,[mem]      ; different memory image
              mov       esi,ebp
              mov       edi,ebp
              mov       bp,bx
              mov       si,cx
              mov       di,dx
ENDIF
              pop       edx
              pop       ecx
              pop       ebx
              retn
WriteMemory   ENDP

;*****************************************************************************
;*
;*  EXPORTED FUNCTIONS
;*
;****

_CpuExecute@4 PROC
              push      esi
              push      edi
              push      ebp
              push      ebx
              push      ecx
              mov       esi,[mem]      ; get the pointer to memory
              mov       edi,[regs]     ; get the pointer to the registers
              mov       edx,[esp+18h]  ; get the number of clocks to execute
              shl       edx,8          
              call      RestoreRegs    ; restore the registers
              EXECNEXT  0              ; execute the next instruction
ReturnToCpp:: mov       edi,[regs]     ; get the pointer to the registers
              call      SaveRegs       ; save the registers
              mov       eax,[esp+18h]  ; determine the number of clocks that
              not       edx            ; were actually used
              shr       edx,8
              inc       edx
              add       eax,edx
              pop       ecx
              pop       ebx
              pop       ebp
              pop       edi
              pop       esi
              retn      4h
_CpuExecute@4 ENDP

_CpuGetCode@12 PROC
              mov       eax,[mem]      ; find the opcode at the given address
              add       ax,[esp+4]
              xor       edx,edx        ; find the address of this opcode's
              mov       dl,[eax]       ; function and the next function
              mov       eax,[opcodetable+edx*4]
              inc       dl
GetCodeCont:  mov       edx,[opcodetable+edx*4]
              sub       edx,eax        ; subtract the addresses to determine
              push      edi            ; the length of this opcode's function
              mov       edi,[esp+0Ch]
              mov       [edi],eax      ; return the function address
              mov       edi,[esp+10h]
              mov       [edi],edx      ; return the function length
              pop       edi
              retn      0Ch
_CpuGetCode@12 ENDP

_CpuInitialize@36 PROC
              mov       eax,[esp+04h]  ; save a pointer to the 65c02's
              mov       mem,eax        ; addressable memory space
IFDEF FASTPAGING
              mov       eax,[esp+08h]  ; save a pointer to the memory shadow
              mov       memshadow,eax  ; table
ENDIF
              mov       eax,[esp+0Ch]  ; save a pointer to the memory write
              mov       memwrite,eax   ; table
IFDEF FASTPAGING
              mov       eax,[esp+10h]  ; save the image number
              mov       image,eax
              mov       eax,[esp+14h]  ; save the last image number 
              mov       lastimage,eax
ENDIF
              mov       eax,[esp+18h]  ; save a pointer to the table of
              mov       regs,eax       ; registers
              mov       eax,[esp+1Ch]  ; save a pointer to the I/O read table
              mov       ioread,eax
              mov       eax,[esp+20h]  ; save a pointer to the I/O write table
              mov       iowrite,eax
              mov       eax,[esp+24h]  ; save a pointer to the dirty page
              mov       dirtypage,eax  ; table
              mov       eax,[mem]
              retn      24h
_CpuInitialize@36 ENDP

_DllMain@12   PROC
              mov       eax,1
              retn      0Ch
_DllMain@12   ENDP

              end
