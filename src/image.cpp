/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

/* DO logical order  0 1 2 3 4 5 6 7 8 9 A B C D E F */
/*    physical order 0 D B 9 7 5 3 1 E C A 8 6 4 2 F */

/* PO logical order  0 E D C B A 9 8 7 6 5 4 3 2 1 F */
/*    physical order 0 2 4 6 8 A C E 1 3 5 7 9 B D F */

struct imageInfo {
    DWORD      format;
    FILE *     file;
    DWORD      offset;
    BOOL       writeProtected;
    DWORD      headerSize;
    LPBYTE     header;
};

typedef BOOL  (* fboot)(imageInfo * info);
typedef DWORD (* fdetect)(LPBYTE imagePtr, DWORD size);
typedef void  (* freadinfo)(imageInfo * info, int track, int quarterTrack, LPBYTE buffer, int * nibbles);
typedef void  (* fwriteinfo)(imageInfo * info, int track, int quarterTrack, LPBYTE buffer, int nibbles);

static BOOL  AplBoot(imageInfo * ptr);
static DWORD AplDetect(LPBYTE imagePtr, DWORD imageSize);
static DWORD DoDetect(LPBYTE imagePtr, DWORD imageSize);
static void  DoRead(imageInfo * ptr, int track, int quarterTrack, LPBYTE buffer, int * nibbles);
static void  DoWrite(imageInfo * ptr, int track, int quarterTrack, LPBYTE buffer, int nibbles);
static DWORD IieDetect(LPBYTE imagePtr, DWORD imageSize);
static void  IieRead(imageInfo * ptr, int track, int quarterTrack, LPBYTE buffer, int * nibbles);
static void  IieWrite(imageInfo * ptr, int track, int quarterTrack, LPBYTE buffer, int nibbles);
static DWORD Nib1Detect(LPBYTE imagePtr, DWORD imageSize);
static void  Nib1Read(imageInfo * ptr, int track, int quarterTrack, LPBYTE buffer, int * nibbles);
static void  Nib1Write(imageInfo * ptr, int track, int quarterTrack, LPBYTE buffer, int nibbles);
static DWORD Nib2Detect(LPBYTE imagePtr, DWORD imageSize);
static void  Nib2Read(imageInfo * ptr, int track, int quarterTrack, LPBYTE buffer, int * nibbles);
static void  Nib2Write(imageInfo * ptr, int track, int quarterTrack, LPBYTE buffer, int nibbles);
static DWORD PoDetect(LPBYTE imagePtr, DWORD imageSize);
static void  PoRead(imageInfo * ptr, int track, int quarterTrack, LPBYTE buffer, int * nibbles);
static void  PoWrite(imageInfo * ptr, int track, int quarterTrack, LPBYTE buffer, int nibbles);
static BOOL  PrgBoot(imageInfo * ptr);
static DWORD PrgDetect(LPBYTE imagePtr, DWORD imageSize);

struct ImageSpec {
    const char * createExts;
    const char * rejectExts;
    fdetect      detect;
    fboot        boot;
    freadinfo    read;
    fwriteinfo   write;
};

constexpr int IMAGETYPES = 7;

static const ImageSpec s_spec[IMAGETYPES] = {
    {
        "",
        ".do;.dsk;.iie;.nib;.po",
        PrgDetect,
        PrgBoot,
        NULL,
        NULL
    },
    {
        ".do;.dsk",
        ".nib;.iie;.po;.prg",
        DoDetect,
        NULL,
        DoRead,
        DoWrite
    },
    {
        ".po",
        ".do;.iie;.nib;.prg",
        PoDetect,
        NULL,
        PoRead,
        PoWrite
    },
    {
        "",
        ".do;.dsk;.iie;.nib;.po",
        AplDetect,
        AplBoot,
        NULL,
        NULL
    },
    {
        "",
        ".do;.iie;.po;.prg",
        Nib1Detect,
        NULL,
        Nib1Read,
        Nib1Write
    },
    {
        ".nib",
        ".do;.iie;.po;.prg",
        Nib2Detect,
        NULL,
        Nib2Read,
        Nib2Write
    },
    {
        ".iie",
        ".do.;.nib;.po;.prg",
        IieDetect,
        NULL,
        IieRead,
        IieWrite
    }
};

static const BYTE s_diskByte[0x40] = {
    0x96, 0x97, 0x9a, 0x9b, 0x9d, 0x9e, 0x9f, 0xa6,
    0xa7, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb2, 0xb3,
    0xb4, 0xb5, 0xb6, 0xb7, 0xb9, 0xba, 0xbb, 0xbc,
    0xbd, 0xbe, 0xbf, 0xcb, 0xcd, 0xce, 0xcf, 0xd3,
    0xd6, 0xd7, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
    0xdf, 0xe5, 0xe6, 0xe7, 0xe9, 0xea, 0xeb, 0xec,
    0xed, 0xee, 0xef, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
    0xf7, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
};

static BYTE s_sectorNumber[3][16] = {
    { 0x00, 0x08, 0x01, 0x09, 0x02, 0x0a, 0x03, 0x0b, 0x04, 0x0c, 0x05, 0x0d, 0x06, 0x0e, 0x07, 0x0f },
    { 0x00, 0x07, 0x0e, 0x06, 0x0d, 0x05, 0x0c, 0x04, 0x0b, 0x03, 0x0a, 0x02, 0x09, 0x01, 0x08, 0x0f },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
};

static LPBYTE s_workBuffer = NULL;


/****************************************************************************
*
*  NIBBLIZATION FUNCTIONS
*
***/

//===========================================================================
static LPBYTE Code62(int sector) {

    // CONVERT THE 256 8-BIT BYTES INTO 342 6-BIT BYTES, WHICH WE STORE
    // STARTING AT 4K INTO THE WORK BUFFER.
    {
        LPBYTE sectorBase = s_workBuffer + (sector << 8);
        LPBYTE resultPtr = s_workBuffer + 0x1000;
        BYTE   offset = 0xAC;
        while (offset != 0x02) {
            BYTE value = 0;
#define ADDVALUE(a) value = (value << 2) | (((a) & 0x01) << 1) | (((a) & 0x02) >> 1)
            ADDVALUE(sectorBase[offset]);  offset -= 0x56;
            ADDVALUE(sectorBase[offset]);  offset -= 0x56;
            ADDVALUE(sectorBase[offset]);  offset -= 0x53;
#undef ADDVALUE
            *resultPtr++ = value << 2;
        }
        resultPtr[-2] &= 0x3F;
        resultPtr[-1] &= 0x3F;
        offset = 0;
        do {
            *resultPtr++ = sectorBase[offset++];
        }
        while (offset);
    }

    // EXCLUSIVE-OR THE ENTIRE DATA BLOCK WITH ITSELF OFFSET BY ONE BYTE,
    // CREATING A 343RD BYTE WHICH IS USED AS A CHECKSUM.  STORE THE NEW
    // BLOCK OF 343 BYTES STARTING AT 5K INTO THE WORK BUFFER.
    {
        BYTE   savedVal = 0;
        LPBYTE sourcePtr = s_workBuffer + 0x1000;
        LPBYTE resultPtr = s_workBuffer + 0x1400;
        int    loop = 342;
        while (loop--) {
            *(resultPtr++) = savedVal ^ *sourcePtr;
            savedVal = *sourcePtr++;
        }
        *resultPtr = savedVal;
    }

    // USING A LOOKUP TABLE, CONVERT THE 6-BIT BYTES INTO DISK BYTES.  A VALID
    // DISK BYTE IS A BYTE THAT HAS THE HIGH BIT SET, AT LEAST TWO ADJACENT BITS
    // SET (EXCLUDING THE HIGH BIT), AND AT MOST ONE PAIR OF CONSECUTIVE ZERO
    // BITS.  THE CONVERTED BLOCK OF 343 BYTES IS STORED STARTING AT 4K INTO THE
    // WORK BUFFER.
    {
        LPBYTE sourcePtr = s_workBuffer + 0x1400;
        LPBYTE resultPtr = s_workBuffer + 0x1000;
        int    loop = 343;
        while (loop--)
            *resultPtr++ = s_diskByte[*sourcePtr++ >> 2];
    }

    return s_workBuffer + 0x1000;
}

//===========================================================================
static void Decode62(LPBYTE imagePtr) {

    // IF WE HAVEN'T ALREADY DONE SO, GENERATE A TABLE FOR CONVERTING
    // DISK BYTES BACK INTO 6-BIT BYTES
    static BOOL tableGenerated = 0;
    static BYTE sixBitByte[0x80];
    if (!tableGenerated) {
        memset(sixBitByte, 0, ARRSIZE(sixBitByte));
        int loop = 0;
        while (loop < 0x40) {
            sixBitByte[s_diskByte[loop] - 0x80] = loop << 2;
            loop++;
        }
        tableGenerated = 1;
    }

    // USING OUR TABLE, CONVERT THE DISK BYTES BACK INTO 6-BIT BYTES
    {
        LPBYTE sourcePtr = s_workBuffer + 0x1000;
        LPBYTE resultPtr = s_workBuffer + 0x1400;
        int    loop = 343;
        while (loop--)
            * (resultPtr++) = sixBitByte[*sourcePtr++ & 0x7F];
    }

    // EXCLUSIVE-OR THE ENTIRE DATA BLOCK WITH ITSELF OFFSET BY ONE BYTE
    // TO UNDO THE EFFECTS OF THE CHECKSUMMING PROCESS
    {
        BYTE   savedVal = 0;
        LPBYTE sourcePtr = s_workBuffer + 0x1400;
        LPBYTE resultPtr = s_workBuffer + 0x1000;
        int    loop = 342;
        while (loop--) {
            *resultPtr = savedVal ^ *sourcePtr++;
            savedVal = *resultPtr++;
        }
    }

    // CONVERT THE 342 6-BIT BYTES INTO 256 8-BIT BYTES
    {
        LPBYTE lowBitsPtr = s_workBuffer + 0x1000;
        LPBYTE sectorBase = s_workBuffer + 0x1056;
        BYTE   offset = 0xAC;
        while (offset != 0x02) {
            if (offset >= 0xAC) {
                imagePtr[offset] = (sectorBase[offset] & 0xfc)
                    | ((*lowBitsPtr & 0x80) >> 7)
                    | ((*lowBitsPtr & 0x40) >> 5);
            }
            offset -= 0x56;
            imagePtr[offset] = (sectorBase[offset] & 0xfc)
                | ((*lowBitsPtr & 0x20) >> 5)
                | ((*lowBitsPtr & 0x10) >> 3);
            offset -= 0x56;
            imagePtr[offset] = (sectorBase[offset] & 0xfc)
                | ((*lowBitsPtr & 0x08) >> 3)
                | ((*lowBitsPtr & 0x04) >> 1);
            offset -= 0x53;
            lowBitsPtr++;
        }
    }

}

//===========================================================================
static void DenibblizeTrack(LPBYTE trackImage, BOOL dosOrder, int nibbles) {
    memset(s_workBuffer, 0, 0x1000);

    // SEARCH THROUGH THE TRACK IMAGE FOR EACH SECTOR.  FOR EVERY SECTOR
    // WE FIND, COPY THE NIBBLIZED DATA FOR THAT SECTOR INTO THE WORK
    // BUFFER AT OFFSET 4K.  THEN CALL DECODE62() TO DENIBBLIZE THE DATA
    // IN THE BUFFER AND WRITE IT INTO THE FIRST PART OF THE WORK BUFFER
    // OFFSET BY THE SECTOR NUMBER.
    int offset = 0;
    int sector = 0;
    for (int partsLeft = 53; partsLeft--; ) {
        BYTE byteVal[3] = { 0,0,0 };
        int  byteNum = 0;
        int  loop = nibbles;
        while ((loop--) && (byteNum < 3)) {
            if (byteNum)
                byteVal[byteNum++] = trackImage[offset++];
            else if (trackImage[offset++] == 0xd5)
                byteNum = 1;
            if (offset >= nibbles)
                offset = 0;
        }
        if ((byteNum == 3) && (byteVal[1] == 0xAA)) {
            int loop = 0;
            int tempOffset = offset;
            while (loop < 384) {
                s_workBuffer[0x1000 + loop++] = trackImage[tempOffset++];
                if (tempOffset >= nibbles)
                    tempOffset = 0;
            }
            if (byteVal[2] == 0x96)
                sector = (s_workBuffer[0x1004] & 0x55) << 1 | (s_workBuffer[0x1005] & 0x55);
            else if (byteVal[2] == 0xad) {
                Decode62(s_workBuffer + ((size_t)s_sectorNumber[dosOrder][sector] << 8));
                sector = 0;
            }
        }
    }
}

//===========================================================================
static DWORD NibblizeTrack(LPBYTE trackImageBuffer, BOOL dosOrder, int track) {
    memset(s_workBuffer + 4096, 0, 4096);
    LPBYTE imagePtr = trackImageBuffer;

    // WRITE GAP ONE, WHICH CONTAINS 48 SELF-SYNC BYTES
    for (int loop = 0; loop < 48; loop++)
        *imagePtr++ = 0xFF;

    for (BYTE sector = 0; sector < 16; ++sector) {
        // Write the address field, which contains:
        //   - prologue (d5aa96)
        //   - volume number (4-and-4 encoded)
        //   - track number (4-and-4 encoded)
        //   - sector number (4-and-4 encoded)
        //   - checksum (4-and-4 encoded)
        //   - epilogue (deaaeb)
        *imagePtr++ = 0xD5;
        *imagePtr++ = 0xAA;
        *imagePtr++ = 0x96;
        *imagePtr++ = 0xFF;
        *imagePtr++ = 0xFE;
#define CODE44A(a) ((((a) >> 1) & 0x55) | 0xAA)
#define CODE44B(a) (((a) & 0x55) | 0xAA)
        *imagePtr++ = CODE44A((BYTE)track);
        *imagePtr++ = CODE44B((BYTE)track);
        *imagePtr++ = CODE44A(sector);
        *imagePtr++ = CODE44B(sector);
        *imagePtr++ = CODE44A(0xFE ^ ((BYTE)track) ^ sector);
        *imagePtr++ = CODE44B(0xFE ^ ((BYTE)track) ^ sector);
#undef CODE44A
#undef CODE44B
        *imagePtr++ = 0xDE;
        *imagePtr++ = 0xAA;
        *imagePtr++ = 0xEB;

        // WRITE GAP TWO, WHICH CONTAINS SIX SELF-SYNC BYTES
        for (int loop = 0; loop < 6; loop++)
            *imagePtr++ = 0xFF;

        // WRITE THE DATA FIELD, WHICH CONTAINS:
        //   - PROLOGUE (D5AAAD)
        //   - 343 6-BIT BYTES OF NIBBLIZED DATA, INCLUDING A 6-BIT CHECKSUM
        //   - EPILOGUE (DEAAEB)
        *imagePtr++ = 0xD5;
        *imagePtr++ = 0xAA;
        *imagePtr++ = 0xAD;
        memcpy(imagePtr, Code62(s_sectorNumber[dosOrder][sector]), 343);
        imagePtr += 343;
        *imagePtr++ = 0xDE;
        *imagePtr++ = 0xAA;
        *imagePtr++ = 0xEB;

        // WRITE GAP THREE, WHICH CONTAINS 27 SELF-SYNC BYTES
        for (int loop = 0; loop < 27; loop++)
            *imagePtr++ = 0xFF;
    }
    return (DWORD)(imagePtr - trackImageBuffer);
}

//===========================================================================
static void SkewTrack(int track, int nibbles, LPBYTE trackImageBuffer) {
    int skewBytes = (track * 768) % nibbles;
    memcpy(s_workBuffer, trackImageBuffer, nibbles);
    memcpy(trackImageBuffer, s_workBuffer + skewBytes, nibbles - skewBytes);
    memcpy(trackImageBuffer + nibbles - skewBytes, s_workBuffer, skewBytes);
}


/****************************************************************************
*
*  RAW PROGRAM IMAGE (APL) FORMAT IMPLEMENTATION
*
***/

//===========================================================================
static BOOL AplBoot(imageInfo * ptr) {
    fseek(ptr->file, 0, SEEK_SET);

    WORD address = 0;
    if (fread(&address, sizeof(WORD), 1, ptr->file) != 1)
        return FALSE;

    WORD length = 0;
    if (fread(&length, sizeof(WORD), 1, ptr->file) != 1)
        return FALSE;

    if (address + length <= address || address >= 0xC000 || address + length - 1 >= 0xC000)
        return FALSE;

    uint8_t * buf = new uint8_t[length];
    if (fread(buf, length, 1, ptr->file) != 1) {
        delete[] buf;
        return FALSE;
    }

    uint16_t a = address;
    for (int i = 0; i < length; ++i, ++a)
        g_pageRead[a >> 8][a & 0xff] = buf[i];
    delete[] buf;

    regs.pc = address;
    return TRUE;
}

//===========================================================================
static DWORD AplDetect(LPBYTE imagePtr, DWORD imageSize) {
    DWORD length = *(LPWORD)(imagePtr + 2);
    return (((length + 4) == imageSize) ||
        ((length + 4 + ((256 - ((length + 4) & 255)) & 255)) == imageSize));
}

/****************************************************************************
*
*  DOS ORDER (DO) FORMAT IMPLEMENTATION
*
***/

//===========================================================================
static DWORD DoDetect(LPBYTE imagePtr, DWORD imageSize) {
    if (((imageSize < 143105) || (imageSize > 143364)) &&
        (imageSize != 143403) && (imageSize != 143488))
        return 0;

    // CHECK FOR A DOS ORDER IMAGE OF A DOS DISKETTE
    {
        int  loop = 0;
        BOOL mismatch = 0;
        while ((loop++ < 15) && !mismatch)
            if (*(imagePtr + 0x11002 + (loop << 8)) != loop - 1)
                mismatch = 1;
        if (!mismatch)
            return 2;
    }

    // CHECK FOR A DOS ORDER IMAGE OF A PRODOS DISKETTE
    {
        int  loop = 1;
        BOOL mismatch = 0;
        while ((loop++ < 5) && !mismatch)
            if ((*(LPWORD)(imagePtr + (loop << 9) + 0x100) != ((loop == 5) ? 0 : 6 - loop)) ||
                (*(LPWORD)(imagePtr + (loop << 9) + 0x102) != ((loop == 2) ? 0 : 8 - loop)))
                mismatch = 1;
        if (!mismatch)
            return 2;
    }

    return 1;
}

//===========================================================================
static void DoRead(imageInfo * ptr, int track, int quarterTrack, LPBYTE trackImageBuffer, int * nibbles) {
    fseek(ptr->file, ptr->offset + (track << 12), SEEK_SET);

    memset(s_workBuffer, 0, 4096);
    if (fread(s_workBuffer, 4096, 1, ptr->file) != 1)
        return;

    *nibbles = NibblizeTrack(trackImageBuffer, 1, track);
    if (!g_optEnhancedDisk)
        SkewTrack(track, *nibbles, trackImageBuffer);
}

//===========================================================================
static void DoWrite(imageInfo * ptr, int track, int quarterTrack, LPBYTE trackImage, int nibbles) {
    memset(s_workBuffer, 0, 4096);
    DenibblizeTrack(trackImage, 1, nibbles);

    fseek(ptr->file, ptr->offset + (track << 12), SEEK_SET);
    fwrite(s_workBuffer, 4096, 1, ptr->file);
}


/****************************************************************************
*
*  SIMSYSTEM IIE (IIE) FORMAT IMPLEMENTATION
*
***/

//===========================================================================
static void IieConvertSectorOrder(LPBYTE sourceOrder) {
    int loop = 16;
    while (loop--) {
        BYTE found = 0xFF;
        int  loop2 = 16;
        while (loop2-- && (found == 0xFF))
            if (*(sourceOrder + loop2) == loop)
                found = loop2;
        if (found == 0xFF)
            found = 0;
        s_sectorNumber[2][loop] = found;
    }
}

//===========================================================================
static DWORD IieDetect(LPBYTE imagePtr, DWORD imageSize) {
    if (strncmp((const char *)imagePtr, "SIMSYSTEM_IIE", 13) ||
        (*(imagePtr + 13) > 3))
        return 0;
    return 2;
}

//===========================================================================
static void IieRead(imageInfo * ptr, int track, int quarterTrack, LPBYTE trackImageBuffer, int * nibbles) {

    // IF WE HAVEN'T ALREADY DONE SO, READ THE IMAGE FILE HEADER
    if (!ptr->header) {
        ptr->header = new BYTE[88];
        if (!ptr->header)
            return;
        memset(ptr->header, 0, 88);
        fseek(ptr->file, 0, SEEK_SET);
        if (fread(ptr->header, ARRSIZE(ptr->header), 1, ptr->file) != 1)
            return;
    }

    // IF THIS IMAGE CONTAINS USER DATA, READ THE TRACK AND NIBBLIZE IT
    if (ptr->header[13] <= 2) {
        IieConvertSectorOrder(ptr->header + 14);
        fseek(ptr->file, (track << 12) + 30, SEEK_SET);
        memset(s_workBuffer, 0, 4096);
        if (fread(s_workBuffer, 4096, 1, ptr->file) != 1)
            return;
        *nibbles = NibblizeTrack(trackImageBuffer, 2, track);
    }

    // OTHERWISE, IF THIS IMAGE CONTAINS NIBBLE INFORMATION, READ IT
    // DIRECTLY INTO THE TRACK BUFFER
    else {
        *nibbles = *(LPWORD)(ptr->header + (track << 1) + 14);
        DWORD offset = 88;
        while (track--)
            offset += *(LPWORD)(ptr->header + (track << 1) + 14);
        fseek(ptr->file, offset, SEEK_SET);
        memset(trackImageBuffer, 0, *nibbles);
        if (fread(trackImageBuffer, *nibbles, 1, ptr->file) != 1)
            return;
    }

}

//===========================================================================
static void IieWrite(imageInfo * ptr, int track, int quarterTrack, LPBYTE trackImage, int nibbles) {
  // note: unimplemented
}


/****************************************************************************
*
*  NIBBLIZED 6656-NIBBLE (NIB) FORMAT IMPLEMENTATION
*
***/

//===========================================================================
static DWORD Nib1Detect(LPBYTE imagePtr, DWORD imageSize) {
    return (imageSize == 232960) ? 2 : 0;
}

//===========================================================================
static void Nib1Read(imageInfo * ptr, int track, int quarterTrack, LPBYTE trackImageBuffer, int * nibbles) {
    fseek(ptr->file, ptr->offset + track * 6656, SEEK_SET);
    *nibbles = (int)fread(trackImageBuffer, 1, 6656, ptr->file);
}

//===========================================================================
static void Nib1Write(imageInfo * ptr, int track, int quarterTrack, LPBYTE trackImage, int nibbles) {
    fseek(ptr->file, ptr->offset + track * 6656, SEEK_SET);
    fwrite(trackImage, nibbles, 1, ptr->file);
}


/****************************************************************************
*
*  NIBBLIZED 6384-NIBBLE (NB2) FORMAT IMPLEMENTATION
*
***/

//===========================================================================
static DWORD Nib2Detect(LPBYTE imagePtr, DWORD imageSize) {
    return (imageSize == 223440) ? 2 : 0;
}

//===========================================================================
static void Nib2Read(imageInfo * ptr, int track, int quarterTrack, LPBYTE trackImageBuffer, int * nibbles) {
    fseek(ptr->file, ptr->offset + track * 6384, SEEK_SET);
    *nibbles = (int)fread(trackImageBuffer, 1, 6384, NULL);
}

//===========================================================================
static void Nib2Write(imageInfo * ptr, int track, int quarterTrack, LPBYTE trackImage, int nibbles) {
    fseek(ptr->file, ptr->offset + track * 6384, SEEK_SET);
    fwrite(trackImage, nibbles, 1, ptr->file);
}


/****************************************************************************
*
*  PRODOS ORDER (PO) FORMAT IMPLEMENTATION
*
***/

//===========================================================================
static DWORD PoDetect(LPBYTE imagePtr, DWORD imageSize) {
    if (((imageSize < 143105) || (imageSize > 143364)) &&
        (imageSize != 143488))
        return 0;

    // CHECK FOR A PRODOS ORDER IMAGE OF A DOS DISKETTE
    {
        int  loop = 4;
        BOOL mismatch = 0;
        while ((loop++ < 13) && !mismatch)
            if (*(imagePtr + 0x11002 + (loop << 8)) != 14 - loop)
                mismatch = 1;
        if (!mismatch)
            return 2;
    }

    // CHECK FOR A PRODOS ORDER IMAGE OF A PRODOS DISKETTE
    {
        int  loop = 1;
        BOOL mismatch = 0;
        while ((loop++ < 5) && !mismatch)
            if ((*(LPWORD)(imagePtr + (loop << 9)) != ((loop == 2) ? 0 : loop - 1)) ||
                (*(LPWORD)(imagePtr + (loop << 9) + 2) != ((loop == 5) ? 0 : loop + 1)))
                mismatch = 1;
        if (!mismatch)
            return 2;
    }

    return 1;
}

//===========================================================================
static void PoRead(imageInfo * ptr, int track, int quarterTrack, LPBYTE trackImageBuffer, int * nibbles) {
    fseek(ptr->file, ptr->offset + (track << 12), SEEK_SET);
    memset(s_workBuffer, 0, 4096);
    if (fread(s_workBuffer, 4096, 1, ptr->file) != 1)
        return;
    *nibbles = NibblizeTrack(trackImageBuffer, 0, track);
    if (!g_optEnhancedDisk)
        SkewTrack(track, *nibbles, trackImageBuffer);
}

//===========================================================================
static void PoWrite(imageInfo * ptr, int track, int quarterTrack, LPBYTE trackImage, int nibbles) {
    memset(s_workBuffer, 0, 4096);
    DenibblizeTrack(trackImage, 0, nibbles);
    fseek(ptr->file, ptr->offset + (track << 12), SEEK_SET);
    fwrite(s_workBuffer, 4096, 1, ptr->file);
}


/****************************************************************************
*
*  PRODOS PROGRAM IMAGE (PRG) FORMAT IMPLEMENTATION
*
***/

//===========================================================================
static BOOL PrgBoot(imageInfo * ptr) {
    fseek(ptr->file, 5, SEEK_SET);

    uint16_t address = 0;
    if (fread(&address, sizeof(uint16_t), 1, ptr->file) != 1)
        return FALSE;

    uint16_t length = 0;
    if (fread(&length, sizeof(uint16_t), 1, ptr->file) != 1)
        return FALSE;

    length <<= 1;
    if (address + length <= address || address >= 0xc000 || address + length - 1 >= 0xc000)
        return FALSE;

    fseek(ptr->file, 128, SEEK_SET);
    uint8_t * buf = new uint8_t[length];
    if (fread(buf, length, 1, ptr->file) != 1) {
        delete[] buf;
        return FALSE;
    }

    uint16_t a = address;
    for (int i = 0; i < length; ++i, ++a)
        g_pageRead[a >> 8][a & 0xff] = buf[i];
    delete[] buf;

    regs.pc = address;
    return TRUE;
}

//===========================================================================
static DWORD PrgDetect(LPBYTE imagePtr, DWORD imageSize) {
    return (*(LPDWORD)imagePtr == 0x214C470A) ? 2 : 0;
}


//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
BOOL ImageBoot(HIMAGE imageHandle) {
    imageInfo * ptr = (imageInfo *)imageHandle;
    BOOL result = FALSE;
    if (s_spec[ptr->format].boot)
        result = s_spec[ptr->format].boot(ptr);
    if (result)
        ptr->writeProtected = 1;
    return result;
}

//===========================================================================
void ImageClose(HIMAGE imageHandle) {
    imageInfo * ptr = (imageInfo *)imageHandle;
    if (ptr->file)
        fclose(ptr->file);
    if (ptr->header)
        delete[] ptr->header;
    delete ptr;
}

//===========================================================================
void ImageDestroy() {
    delete[] s_workBuffer;
    s_workBuffer = NULL;
}

//===========================================================================
void ImageInitialize() {
    s_workBuffer = new BYTE[0x2000];
    memset(s_workBuffer, 0, 0x2000);
}

//===========================================================================
bool ImageOpen (
    const char *    imageFilename,
    HIMAGE *        imageHandle,
    bool *          writeProtected,
    bool            createIfNecessary
) {
    if (imageHandle)
        *imageHandle = (HIMAGE)NULL;
    if (writeProtected)
        *writeProtected = false;
    if (!(imageFilename && imageHandle && writeProtected && s_workBuffer))
        return false;

    // Try to open the image file
    bool readonly = false;
    FILE * file = fopen(imageFilename, "rb+");
    if (!file) {
        readonly = true;
        file = fopen(imageFilename, "rb");
    }

    // If we are able to open the file, map it into memory for use by the
    // detection functions
    if (file) {
        fseek(file, 0, SEEK_END);
        size_t size = (size_t)ftell(file);
        fseek(file, 0, SEEK_SET);

        BYTE * buf = new BYTE[size];
        if (fread(buf, size, 1, file) != 1) {
            fclose(file);
            return false;
        }

        BYTE * imagePtr = buf;
        DWORD  format = 0xffffffff;
        if (imagePtr) {

            // Determine whether the file has a 128-byte macbinary header
            if ((size > 128) &&
                (!*imagePtr) &&
                (*(imagePtr + 1) < 120) &&
                (!*(imagePtr + *(imagePtr + 1) + 2)) &&
                (*(imagePtr + 0x7a) == 0x81) &&
                (*(imagePtr + 0x7b) == 0x81))
            {
                imagePtr += 128;
                size -= 128;
            }

            // Determine the file's extension
            const char * ext = imageFilename;
            while (StrChr(ext, '\\'))
                ext = StrChr(ext, '\\') + 1;
            while (StrChr(ext + 1, '.'))
                ext = StrChr(ext + 1, '.');

            // Call the detection functions in order, looking for a match
            DWORD possibleformat = 0xffffffff;
            int   loop = 0;
            while ((loop < IMAGETYPES) && (format == 0xffffffff)) {
                BOOL reject = 0;
                if (ext && *ext) {
                    const char * rejectexts = s_spec[loop].rejectExts;
                    while (rejectexts && *rejectexts && !reject) {
                        if (!StrCmpLenI(ext, rejectexts, StrLen(ext)))
                            reject = 1;
                        else if (StrChr(rejectexts, ';'))
                            rejectexts = StrChr(rejectexts, ';') + 1;
                        else
                            rejectexts = NULL;
                    }
                }
                if (reject)
                    ++loop;
                else {
                    DWORD result = s_spec[loop].detect(imagePtr, (DWORD)size);
                    if (result == 2)
                        format = loop;
                    else if ((result == 1) && (possibleformat == 0xffffffff))
                        possibleformat = loop++;
                    else
                        ++loop;
                }
            }
            if (format == 0xFFFFFFFF)
                format = possibleformat;
        }
        DWORD offset = (DWORD)(imagePtr - buf);
        delete[] buf;

        // If the file does not match any known format, close it and return
        if (format == 0xffffffff) {
            fclose(file);
            return false;
        }

        // Otherwise, create a record for the file, and return an image handle
        else {
            imageInfo * ptr = new imageInfo;
            if (ptr) {
                memset(ptr, 0, sizeof(imageInfo));
                ptr->format         = format;
                ptr->file           = file;
                ptr->offset         = offset;
                ptr->writeProtected = readonly;
                if (imageHandle)
                    *imageHandle = (HIMAGE)ptr;
                if (writeProtected)
                    *writeProtected = readonly;
                return true;
            }
            else {
                fclose(file);
                return false;
            }
        }
    }

    return false;
}

//===========================================================================
void ImageReadTrack(
    HIMAGE  imageHandle,
    int     track,
    int     quarterTrack,
    LPBYTE  trackImageBuffer,
    int *   nibbles
) {
    *nibbles = 0;
    imageInfo * ptr = (imageInfo *)imageHandle;
    if (s_spec[ptr->format].read)
        s_spec[ptr->format].read(ptr, track, quarterTrack, trackImageBuffer, nibbles);
}

//===========================================================================
void ImageWriteTrack (
    HIMAGE  imageHandle,
    int     track,
    int     quarterTrack,
    LPBYTE  trackImage,
    int     nibbles
) {
    imageInfo * ptr = (imageInfo *)imageHandle;
    if (s_spec[ptr->format].write && !ptr->writeProtected)
        s_spec[ptr->format].write(ptr, track, quarterTrack, trackImage, nibbles);
}
