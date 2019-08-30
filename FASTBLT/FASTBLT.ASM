;
;  APPLE //E EMULATOR FOR WINDOWS
;
;  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
;

              .386
              .model small

              assume cs:FLAT,ds:FLAT,es:FLAT,ss:FLAT

public        _FastBlt104@24
public        _FastBlt108@24
public        _FastBlt108d@24
public        _FastBlt110d@24
public        _FastBlt118d@24
public        _FastBlt120d@24
public        _FastBlt401@24
public        _FastBlt401b@24
public        _FastBltInit@16
public        _DllMain@12

BSWAP_EAX     equ       <dw 0C80Fh>
BSWAP_EBX     equ       <dw 0CB0Fh>

DESTCX        equ       560
DESTCY        equ       384
SOURCECX      equ       544

PRM_DESTX     equ       <DWORD PTR [esp+14h]>
PRM_DESTY     equ       <DWORD PTR [esp+18h]>
PRM_XSIZE     equ       <DWORD PTR [esp+1Ch]>
PRM_YSIZE     equ       <DWORD PTR [esp+20h]>
PRM_SOURCEX   equ       <DWORD PTR [esp+24h]>
PRM_SOURCEY   equ       <DWORD PTR [esp+28h]>

              .data

destbits      dd        0
desttable     dd        0
sourcebits    dd        0
sourcetable   dd        0

              .code

ADJUST104     MACRO
              shr       ebx,1
              ENDM

ADJUST110     MACRO
              shl       ebx,1
              ENDM

ADJUST118     MACRO
              mov       ecx,ebx
              shl       ebx,1
              add       ebx,ecx
              ENDM

ADJUST120     MACRO
              shl       ebx,2
              ENDM

ADJUST401     MACRO
              shr       ebx,3
              ENDM

SETPOINTERS   MACRO ADJUSTFUNC
              mov       eax,[sourcetable]
              mov       ebx,PRM_SOURCEY
              mov       eax,[eax+ebx*4]
              mov       ebx,PRM_SOURCEX
              ADJUSTFUNC
              add       eax,ebx
              mov       esi,eax

              mov       eax,[desttable]
              mov       ebx,PRM_DESTY
              mov       eax,[eax+ebx*4]
              mov       ebx,PRM_DESTX
              ADJUSTFUNC
              add       eax,ebx
              mov       edi,eax
              ENDM

              align     4
_FastBlt104@24 PROC
              push      esi
              push      edi
              push      ebx
              push      ecx

              SETPOINTERS ADJUST104

              mov       ecx,PRM_XSIZE
              cmp       ecx,8
              mov       ecx,PRM_YSIZE
              jle       f104x7det
              cmp       ecx,2
              je        f104x14x2
              jmp       f104x14
f104x7det:    cmp       ecx,2
              je        f104x8x2

; ---------------------------------------------------------------------------
; FASTBLT104 7
;
; Assumptions:
; 1. xsize == 7
; ---------------------------------------------------------------------------

              align     4
f104x7:       mov       eax,PRM_DESTX
              test      eax,1
              jnz       f104x7unext

f104x7next:   mov       eax,[esi]
              mov       ebx,[edi]
              and       eax,0F0FFFFFFh
              and       ebx,00F000000h
              or        eax,ebx
              mov       [edi],eax
              add       esi,SOURCECX/2
              add       edi,DESTCX/2
              dec       ecx
              jnz       f104x7next
              jmp       f104return

f104x7unext:  mov       eax,[esi]
              mov       ebx,[edi]
              BSWAP_EAX
              shr       eax,4
              BSWAP_EAX
              and       ebx,0000000F0h
              or        eax,ebx
              mov       [edi],eax
              add       esi,SOURCECX/2
              add       edi,DESTCX/2
              dec       ecx
              jnz       f104x7unext
              jmp       f104return

; ---------------------------------------------------------------------------
; FASTBLT104 8x2
;
; Assumptions:
; 1. xsize == 8
; 2. ysize == 2
; ---------------------------------------------------------------------------

              align     4
f104x8x2:     mov       eax,[esi]
              mov       [edi],eax
              mov       eax,[esi+SOURCECX/2]
              mov       [edi+DESTCX/2],eax
              jmp       f104return

; ---------------------------------------------------------------------------
; FASTBLT104 14
;
; Assumptions:
; 1. xsize == 14
; ---------------------------------------------------------------------------

              align     4
f104x14:      mov       eax,[esi]
              mov       [edi],eax
              mov       ax,[esi+4]
              mov       [edi+4],ax
              mov       al,[esi+6]
              mov       [edi+6],al
              add       esi,SOURCECX/2
              add       edi,DESTCX/2
              dec       ecx
              jnz       f104x14
              jmp       f104return

; ---------------------------------------------------------------------------
; FASTBLT104 14x2
;
; Assumptions:
; 1. xsize == 14
; 2. ysize == 2
; ---------------------------------------------------------------------------

              align     4
f104x14x2:    mov       eax,[esi]
              mov       [edi],eax
              mov       ax,[esi+4]
              mov       [edi+4],ax
              mov       al,[esi+6]
              mov       [edi+6],al

              mov       eax,[esi+SOURCECX/2]
              mov       [edi+DESTCX/2],eax
              mov       ax,[esi+SOURCECX/2+4]
              mov       [edi+DESTCX/2+4],ax
              mov       al,[esi+SOURCECX/2+6]
              mov       [edi+DESTCX/2+6],al

f104return:   pop       ecx
              pop       ebx
              pop       edi
              pop       esi
              retn      18h
_FastBlt104@24 ENDP

              align     4
_FastBlt108@24 PROC
              push      esi
              push      edi
              push      ebx
              push      ecx

              SETPOINTERS

              mov       ebx,PRM_XSIZE
              mov       ecx,PRM_YSIZE
              cmp       ebx,8
              jle       f108x7det
              cmp       ecx,2
              je        f108x14x2
              jmp       f108x14
f108x7det:    cmp       ecx,2
              je        f108x8x2

; ---------------------------------------------------------------------------
; FASTBLT108 7
;
; Assumptions:
; 1. xsize == 7
; ---------------------------------------------------------------------------

              align     4
f108x7:       mov       eax,[esi]
              mov       bx,[esi+4]
              mov       dl,[esi+6]
              mov       [edi],eax
              mov       [edi+4],bx
              mov       [edi+6],dl
              add       esi,SOURCECX
              add       edi,DESTCX
              dec       ecx
              jnz       f108x7
              jmp       f108return

; ---------------------------------------------------------------------------
; FASTBLT108 8x2
;
; Assumptions:
; 1. xsize == 8
; 2. ysize == 2
; ---------------------------------------------------------------------------

              align     4
f108x8x2:     mov       eax,[esi]
              mov       ebx,[esi+4]
              mov       ecx,[esi+SOURCECX]
              mov       edx,[esi+SOURCECX+4]
              mov       [edi],eax
              mov       [edi+4],ebx
              mov       [edi+DESTCX],ecx
              mov       [edi+DESTCX+4],edx
              jmp       f108return

; ---------------------------------------------------------------------------
; FASTBLT108 14
;
; Assumptions:
; 1. xsize == 14
; ---------------------------------------------------------------------------

              align     4
f108x14:      mov       eax,[esi]
              mov       ebx,[esi+4]
              mov       edx,[esi+8]
              mov       [edi],eax
              mov       [edi+4],ebx
              mov       ax,[esi+12]
              mov       [edi+8],edx
              mov       [edi+12],ax
              add       esi,SOURCECX
              add       edi,DESTCX
              dec       ecx
              jnz       f108x14
              jmp       f108return

; ---------------------------------------------------------------------------
; FASTBLT108 14x2
;
; Assumptions:
; 1. xsize == 14
; 2. ysize == 2
; ---------------------------------------------------------------------------

              align     4
f108x14x2:    mov       eax,[esi]
              mov       ebx,[esi+4]
              mov       ecx,[esi+8]
              mov       dx,[esi+12]
              mov       [edi],eax
              mov       [edi+4],ebx
              mov       [edi+8],ecx
              mov       [edi+12],dx

              mov       eax,[esi+SOURCECX]
              mov       ebx,[esi+SOURCECX+4]
              mov       ecx,[esi+SOURCECX+8]
              mov       dx,[esi+SOURCECX+12]
              mov       [edi+DESTCX],eax
              mov       [edi+DESTCX+4],ebx
              mov       [edi+DESTCX+8],ecx
              mov       [edi+DESTCX+12],dx

f108return:   pop       ecx
              pop       ebx
              pop       edi
              pop       esi
              retn      18h
_FastBlt108@24 ENDP

              align     4
_FastBlt108d@24 PROC
              push      esi
              push      edi
              push      ebx
              push      ecx

              SETPOINTERS

              mov       ecx,PRM_XSIZE
              cmp       ecx,8
              mov       ecx,PRM_YSIZE
              jle       f108dx7det
              cmp       ecx,2
              je        f108dx14x2
              jmp       f108dx14
f108dx7det:   cmp       ecx,2
              je        f108dx8x2

; ---------------------------------------------------------------------------
; FASTBLT108D 7
;
; Assumptions:
; 1. xsize == 7
; ---------------------------------------------------------------------------

              align     4
f108dx7:      mov       eax,[esi]
              mov       bx,[esi+4]
              mov       dl,[esi+6]
              mov       [edi],eax
              mov       [edi+4],bx
              mov       [edi+6],dl
              add       esi,SOURCECX
              sub       edi,DESTCX
              dec       ecx
              jnz       f108dx7
              jmp       f108dreturn

; ---------------------------------------------------------------------------
; FASTBLT108D 8x2
;
; Assumptions:
; 1. xsize == 8
; 2. ysize == 2
; ---------------------------------------------------------------------------

              align     4
f108dx8x2:    mov       eax,[esi]
              mov       ebx,[esi+4]
              mov       ecx,[esi+SOURCECX]
              mov       edx,[esi+SOURCECX+4]
              mov       [edi],eax
              mov       [edi+4],ebx
              mov       [edi-DESTCX],ecx
              mov       [edi-DESTCX+4],edx
              jmp       f108dreturn

; ---------------------------------------------------------------------------
; FASTBLT108D 14
;
; Assumptions:
; 1. xsize == 14
; ---------------------------------------------------------------------------

              align     4
f108dx14:     mov       eax,[esi]
              mov       ebx,[esi+4]
              mov       edx,[esi+8]
              mov       [edi],eax
              mov       [edi+4],ebx
              mov       ax,[esi+12]
              mov       [edi+8],edx
              mov       [edi+12],ax
              add       esi,SOURCECX
              sub       edi,DESTCX
              dec       ecx
              jnz       f108dx14
              jmp       f108dreturn

; ---------------------------------------------------------------------------
; FASTBLT108D 14x2
;
; Assumptions:
; 1. xsize == 14
; 2. ysize == 2
; ---------------------------------------------------------------------------

              align     4
f108dx14x2:   mov       eax,[esi]
              mov       ebx,[esi+4]
              mov       ecx,[esi+8]
              mov       dx,[esi+12]
              mov       [edi],eax
              mov       [edi+4],ebx
              mov       [edi+8],ecx
              mov       [edi+12],dx

              mov       eax,[esi+SOURCECX]
              mov       ebx,[esi+SOURCECX+4]
              mov       ecx,[esi+SOURCECX+8]
              mov       dx,[esi+SOURCECX+12]
              mov       [edi-DESTCX],eax
              mov       [edi-DESTCX+4],ebx
              mov       [edi-DESTCX+8],ecx
              mov       [edi-DESTCX+12],dx

f108dreturn:  pop       ecx
              pop       ebx
              pop       edi
              pop       esi
              retn      18h
_FastBlt108d@24 ENDP

              align     4
_FastBlt110d@24 PROC
              push      esi
              push      edi
              push      ebx
              push      ecx

              SETPOINTERS ADJUST110

              mov       ebx,PRM_XSIZE
              mov       ecx,PRM_YSIZE
              cmp       ebx,8
              je        f110dx8x2
              jg        f110dx14

; ---------------------------------------------------------------------------
; FASTBLT110D 7
;
; Assumptions:
; 1. xsize == 7
; ---------------------------------------------------------------------------

              align     4
f110dx7:      mov       eax,[esi]
              mov       ebx,[esi+4]
              mov       edx,[esi+8]
              mov       [edi],eax
              mov       [edi+4],ebx
              mov       ax,[esi+12]
              mov       [edi+8],edx
              mov       [edi+12],ax
              add       esi,SOURCECX*2
              sub       edi,DESTCX*2
              dec       ecx
              jnz       f110dx7
              jmp       f110dreturn

; ---------------------------------------------------------------------------
; FASTBLT110D 8x2
;
; Assumptions:
; 1. xsize == 8
; 2. ysize == 2
; ---------------------------------------------------------------------------

              align     4
f110dx8x2:    mov       eax,[esi]
              mov       ebx,[esi+4]
              mov       ecx,[esi+8]
              mov       edx,[esi+12]
              mov       [edi],eax
              mov       [edi+4],ebx
              mov       [edi+8],ecx
              mov       [edi+12],edx

              mov       eax,[esi+SOURCECX*2]
              mov       ebx,[esi+SOURCECX*2+4]
              mov       ecx,[esi+SOURCECX*2+8]
              mov       edx,[esi+SOURCECX*2+12]
              mov       [edi-DESTCX*2],eax
              mov       [edi-DESTCX*2+4],ebx
              mov       [edi-DESTCX*2+8],ecx
              mov       [edi-DESTCX*2+12],edx
              jmp       f110dreturn

; ---------------------------------------------------------------------------
; FASTBLT110D 14
;
; Assumptions:
; 1. xsize == 14
; ---------------------------------------------------------------------------

              align     4
f110dx14:     mov       eax,[esi]
              mov       ebx,[esi+4]
              mov       edx,[esi+8]
              mov       [edi],eax
              mov       [edi+4],ebx
              mov       [edi+8],edx
              mov       eax,[esi+12]
              mov       ebx,[esi+16]
              mov       edx,[esi+20]
              mov       [edi+12],eax
              mov       [edi+16],ebx
              mov       eax,[esi+24]
              mov       [edi+20],edx
              mov       [edi+24],eax
              add       esi,SOURCECX*2
              sub       edi,DESTCX*2
              dec       ecx
              jnz       f110dx14

f110dreturn:  pop       ecx
              pop       ebx
              pop       edi
              pop       esi
              retn      18h
_FastBlt110d@24 ENDP

              align     4
_FastBlt118d@24 PROC
              push      esi
              push      edi
              push      ebx
              push      ecx

              SETPOINTERS ADJUST118

              mov       ebx,PRM_XSIZE
              mov       ecx,PRM_YSIZE
              cmp       ebx,8
              je        f118dx8x2
              jg        f118dx14

; ---------------------------------------------------------------------------
; FASTBLT118D 7
;
; Assumptions:
; 1. xsize == 7
; ---------------------------------------------------------------------------

              align     4
f118dx7:      mov       eax,[esi]
              mov       ebx,[esi+4]
              mov       edx,[esi+8]
              mov       [edi],eax
              mov       [edi+4],ebx
              mov       [edi+8],edx
              mov       eax,[esi+12]
              mov       ebx,[esi+16]
              mov       dl,[esi+20]
              mov       [edi+12],eax
              mov       [edi+16],ebx
              mov       [edi+20],dl
              add       esi,SOURCECX*3
              sub       edi,DESTCX*3
              dec       ecx
              jnz       f118dx7
              jmp       f118dreturn

; ---------------------------------------------------------------------------
; FASTBLT118D 8x2
;
; Assumptions:
; 1. xsize == 8
; 2. ysize == 2
; ---------------------------------------------------------------------------

              align     4
f118dx8x2:    mov       eax,[esi]
              mov       ebx,[esi+4]
              mov       ecx,[esi+8]
              mov       edx,[esi+12]
              mov       [edi],eax
              mov       [edi+4],ebx
              mov       eax,[esi+16]
              mov       [edi+8],ecx
              mov       ebx,[esi+20]
              mov       [edi+12],edx
              mov       [edi+16],eax
              mov       [edi+20],ebx

              mov       eax,[esi+SOURCECX*3]
              mov       ebx,[esi+SOURCECX*3+4]
              mov       ecx,[esi+SOURCECX*3+8]
              mov       edx,[esi+SOURCECX*3+12]
              mov       [edi-DESTCX*3],eax
              mov       [edi-DESTCX*3+4],ebx
              mov       eax,[esi+SOURCECX*3+16]
              mov       [edi-DESTCX*3+8],ecx
              mov       ebx,[esi+SOURCECX*3+20]
              mov       [edi-DESTCX*3+12],edx
              mov       [edi-DESTCX*3+16],eax
              mov       [edi-DESTCX*3+20],ebx
              jmp       f118dreturn

; ---------------------------------------------------------------------------
; FASTBLT118D 14
;
; Assumptions:
; 1. xsize == 14
; ---------------------------------------------------------------------------

              align     4
f118dx14:     mov       eax,[esi]
              mov       ebx,[esi+4]
              mov       edx,[esi+8]
              mov       [edi],eax
              mov       [edi+4],ebx
              mov       [edi+8],edx
              mov       eax,[esi+12]
              mov       ebx,[esi+16]
              mov       edx,[esi+20]
              mov       [edi+12],eax
              mov       [edi+16],ebx
              mov       [edi+20],edx
              mov       eax,[esi+24]
              mov       ebx,[esi+28]
              mov       edx,[esi+32]
              mov       [edi+24],eax
              mov       [edi+28],ebx
              mov       eax,[esi+36]
              mov       bx,[esi+40]
              mov       [edi+32],edx
              mov       [edi+36],eax
              mov       [edi+40],bx
              add       esi,SOURCECX*3
              sub       edi,DESTCX*3
              dec       ecx
              jnz       f118dx14

f118dreturn:  pop       ecx
              pop       ebx
              pop       edi
              pop       esi
              retn      18h
_FastBlt118d@24 ENDP

              align     4
_FastBlt120d@24 PROC
              push      esi
              push      edi
              push      ebx
              push      ecx

              SETPOINTERS ADJUST120

              mov       ebx,PRM_XSIZE
              mov       ecx,PRM_YSIZE
              cmp       ebx,8
              je        f120dx8x2
              jg        f120dx14

; ---------------------------------------------------------------------------
; FASTBLT120D 7
;
; Assumptions:
; 1. xsize == 7
; ---------------------------------------------------------------------------

              align     4
f120dx7:      mov       eax,[esi]
              mov       ebx,[esi+4]
              mov       edx,[esi+8]
              mov       [edi],eax
              mov       [edi+4],ebx
              mov       [edi+8],edx
              mov       eax,[esi+12]
              mov       ebx,[esi+16]
              mov       edx,[esi+20]
              mov       [edi+12],eax
              mov       [edi+16],ebx
              mov       eax,[esi+24]
              mov       [edi+20],edx
              mov       [edi+24],eax
              add       esi,SOURCECX*4
              sub       edi,DESTCX*4
              dec       ecx
              jnz       f120dx7
              jmp       f120dreturn

; ---------------------------------------------------------------------------
; FASTBLT120D 8x2
;
; Assumptions:
; 1. xsize == 8
; 2. ysize == 2
; ---------------------------------------------------------------------------

              align     4
f120dx8x2:    mov       eax,[esi]
              mov       ebx,[esi+4]
              mov       ecx,[esi+8]
              mov       edx,[esi+12]
              mov       [edi],eax
              mov       [edi+4],ebx
              mov       [edi+8],ecx
              mov       [edi+12],edx
              mov       eax,[esi+16]
              mov       ebx,[esi+20]
              mov       ecx,[esi+24]
              mov       edx,[esi+28]
              mov       [edi+16],eax
              mov       [edi+20],ebx
              mov       [edi+24],ecx
              mov       [edi+28],edx

              mov       eax,[esi+SOURCECX*4]
              mov       ebx,[esi+SOURCECX*4+4]
              mov       ecx,[esi+SOURCECX*4+8]
              mov       edx,[esi+SOURCECX*4+12]
              mov       [edi-DESTCX*4],eax
              mov       [edi-DESTCX*4+4],ebx
              mov       [edi-DESTCX*4+8],ecx
              mov       [edi-DESTCX*4+12],edx
              mov       eax,[esi+SOURCECX*4+16]
              mov       ebx,[esi+SOURCECX*4+20]
              mov       ecx,[esi+SOURCECX*4+24]
              mov       edx,[esi+SOURCECX*4+28]
              mov       [edi-DESTCX*4+16],eax
              mov       [edi-DESTCX*4+20],ebx
              mov       [edi-DESTCX*4+24],ecx
              mov       [edi-DESTCX*4+28],edx
              jmp       f120dreturn

; ---------------------------------------------------------------------------
; FASTBLT120D 14
;
; Assumptions:
; 1. xsize == 14
; ---------------------------------------------------------------------------

              align     4
f120dx14:     mov       eax,[esi]
              mov       ebx,[esi+4]
              mov       edx,[esi+8]
              mov       [edi],eax
              mov       [edi+4],ebx
              mov       [edi+8],edx
              mov       eax,[esi+12]
              mov       ebx,[esi+16]
              mov       edx,[esi+20]
              mov       [edi+12],eax
              mov       [edi+16],ebx
              mov       [edi+20],edx
              mov       eax,[esi+24]
              mov       ebx,[esi+28]
              mov       edx,[esi+32]
              mov       [edi+24],eax
              mov       [edi+28],ebx
              mov       [edi+32],edx
              mov       eax,[esi+36]
              mov       ebx,[esi+40]
              mov       edx,[esi+44]
              mov       [edi+36],eax
              mov       [edi+40],ebx
              mov       eax,[esi+48]
              mov       ebx,[esi+52]
              mov       [edi+44],edx
              mov       [edi+48],eax
              mov       [edi+52],ebx
              add       esi,SOURCECX*4
              sub       edi,DESTCX*4
              dec       ecx
              jnz       f120dx14

f120dreturn:  pop       ecx
              pop       ebx
              pop       edi
              pop       esi
              retn      18h
_FastBlt120d@24 ENDP

              align     4
_FastBlt401@24 PROC
              push      esi
              push      edi
              push      ebx
              push      ecx

              SETPOINTERS ADJUST401

              mov       ebx,PRM_DESTX
              and       ebx,7
              mov       eax,PRM_XSIZE
              jnz       f401u
              cmp       eax,14
              jne       f401u

; ---------------------------------------------------------------------------
; FASTBLT401 ALIGNED 14
;
; Assumptions:
; 1. ((sourcex & 7) == 0)
; 2. ((destx & 7) == 0)
; 3. xsize == 14
; ---------------------------------------------------------------------------

              align     4
              xor       eax,eax
              xor       ebx,ebx
              mov       ecx,PRM_YSIZE
              shl       ecx,2
              align     4
f401a14next:  mov       ax,[esi]
              mov       bx,[edi]
              and       eax,0FCFFh
              and       ebx,0300h
              or        eax,ebx
              mov       [edi],ax
              add       esi,(SOURCECX/8)
              add       edi,(DESTCX/8)
              dec       ecx
              jnz       f401a14next
              jmp       f401return

; ---------------------------------------------------------------------------
; FASTBLT401 UNALIGNED
;
; Assumptions:
; 1. ((sourcex & 7) == 0)
; 2. xsize >= 7
; 3. xsize <= 31
; ---------------------------------------------------------------------------

              align     4
f401u:        mov       edx,0FFFFFFFFh
              mov       cl,al
              not       cl
              and       cl,1Fh
              inc       cl
              shr       edx,cl
              sub       cl,bl
              shl       edx,cl
              mov       ecx,PRM_YSIZE
              shl       ecx,10
              mov       cl,bl
              align     4
f401unext:    mov       eax,[esi]
              mov       ebx,[edi]
              BSWAP_EAX
              BSWAP_EBX
              shr       eax,cl
              and       eax,edx
              not       edx
              and       ebx,edx
              not       edx
              or        eax,ebx
              BSWAP_EAX
              mov       [edi],eax
              add       esi,(SOURCECX/8)
              add       edi,(DESTCX/8)
              dec       ch
              jnz       f401unext

f401return:   pop       ecx
              pop       ebx
              pop       edi
              pop       esi
              retn      18h
_FastBlt401@24 ENDP

              align     4
_FastBlt401b@24 PROC
              push      esi
              push      edi
              push      ebx
              push      ecx

              SETPOINTERS ADJUST401

              mov       ebx,PRM_DESTX
              and       ebx,7
              mov       eax,PRM_XSIZE
              jnz       f401bu
              cmp       eax,14
              jne       f401bu

; ---------------------------------------------------------------------------
; FASTBLT401B ALIGNED 14
;
; Assumptions:
; 1. ((sourcex & 7) == 0)
; 2. ((destx & 7) == 0)
; 3. xsize == 14
; ---------------------------------------------------------------------------

              align     4
              xor       eax,eax
              xor       ebx,ebx
              mov       ecx,PRM_YSIZE
              xor       edx,edx
              shl       ecx,2
              align     4
f401ba14next: mov       ax,[esi]
              mov       bx,[edi]
              and       eax,0FCFFh
              and       ebx,0300h
              or        eax,ebx
              mov       [edi],ax
              add       esi,(SOURCECX/8)
              inc       edx
              and       edx,3
              jz        f401ba14back
              add       edi,(DESTCX*DESTCY/8)
              jmp       f401ba14dec
f401ba14back: sub       edi,((3*DESTCX*DESTCY/8)-(DESTCX/8))
f401ba14dec:  dec       ecx
              jnz       f401ba14next
              jmp       f401breturn

; ---------------------------------------------------------------------------
; FASTBLT401B UNALIGNED
;
; Assumptions:
; 1. ((sourcex & 7) == 0)
; 2. xsize >= 7
; 3. xsize <= 31
; ---------------------------------------------------------------------------

              align     4
f401bu:       mov       edx,0FFFFFFFFh
              mov       cl,al
              not       cl
              and       cl,1Fh
              inc       cl
              shr       edx,cl
              sub       cl,bl
              shl       edx,cl
              mov       ecx,PRM_YSIZE
              shl       ecx,18
              sub       ecx,10000h
              mov       cl,bl
              align     4
f401bunext:   mov       eax,[esi]
              mov       ebx,[edi]
              BSWAP_EAX
              BSWAP_EBX
              shr       eax,cl
              and       eax,edx
              not       edx
              and       ebx,edx
              not       edx
              or        eax,ebx
              BSWAP_EAX
              mov       [edi],eax
              add       esi,(SOURCECX/8)
              inc       ch
              and       ch,3
              jz        f401buback
              add       edi,(DESTCX*DESTCY/8)
              jmp       f401busub
f401buback:   sub       edi,((3*DESTCX*DESTCY/8)-(DESTCX/8))
f401busub:    sub       ecx,10000h
              jnc       f401bunext

f401breturn:  pop       ecx
              pop       ebx
              pop       edi
              pop       esi
              retn      18h
_FastBlt401b@24 ENDP

_FastBltInit@16 PROC
              mov       eax,[esp+04h]  
              mov       sourcebits,eax
              mov       eax,[esp+08h]
              mov       destbits,eax
              mov       eax,[esp+0Ch]
              mov       sourcetable,eax
              mov       eax,[esp+10h]
              mov       desttable,eax
              retn      10h
_FastBltInit@16 ENDP

_DllMain@12   PROC
              mov       eax,1
              retn      0Ch
_DllMain@12   ENDP

              end
