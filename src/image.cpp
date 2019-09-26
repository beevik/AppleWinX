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

struct imagespec {
    const char * createExts;
    const char * rejectExts;
    fdetect      detect;
    fboot        boot;
    freadinfo    read;
    fwriteinfo   write;
};

constexpr int IMAGETYPES = 7;

static const imagespec spec[IMAGETYPES] = {
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

static const BYTE diskByte[0x40] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};

static BYTE sectorNumber[3][16] = {
    { 0x00, 0x08, 0x01, 0x09, 0x02, 0x0A, 0x03, 0x0B, 0x04, 0x0C, 0x05, 0x0D, 0x06, 0x0E, 0x07, 0x0F },
    { 0x00, 0x07, 0x0E, 0x06, 0x0D, 0x05, 0x0C, 0x04, 0x0B, 0x03, 0x0A, 0x02, 0x09, 0x01, 0x08, 0x0F },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
};

static LPBYTE workBuffer = NULL;


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
        LPBYTE sectorbase = workBuffer + (sector << 8);
        LPBYTE resultptr = workBuffer + 0x1000;
        BYTE   offset = 0xAC;
        while (offset != 0x02) {
            BYTE value = 0;
#define ADDVALUE(a) value = (value << 2) |        \
                            (((a) & 0x01) << 1) | \
                            (((a) & 0x02) >> 1)
            ADDVALUE(*(sectorbase + offset));  offset -= 0x56;
            ADDVALUE(*(sectorbase + offset));  offset -= 0x56;
            ADDVALUE(*(sectorbase + offset));  offset -= 0x53;
#undef ADDVALUE
            *resultptr++ = value << 2;
        }
        resultptr[-2] &= 0x3F;
        resultptr[-1] &= 0x3F;
        offset = 0;
        do
            *resultptr++ = sectorbase[offset++];
        while (offset);
    }

    // EXCLUSIVE-OR THE ENTIRE DATA BLOCK WITH ITSELF OFFSET BY ONE BYTE,
    // CREATING A 343RD BYTE WHICH IS USED AS A CHECKSUM.  STORE THE NEW
    // BLOCK OF 343 BYTES STARTING AT 5K INTO THE WORK BUFFER.
    {
        BYTE   savedval = 0;
        LPBYTE sourceptr = workBuffer + 0x1000;
        LPBYTE resultptr = workBuffer + 0x1400;
        int    loop = 342;
        while (loop--) {
            *(resultptr++) = savedval ^ *sourceptr;
            savedval = *sourceptr++;
        }
        *resultptr = savedval;
    }

    // USING A LOOKUP TABLE, CONVERT THE 6-BIT BYTES INTO DISK BYTES.  A VALID
    // DISK BYTE IS A BYTE THAT HAS THE HIGH BIT SET, AT LEAST TWO ADJACENT BITS
    // SET (EXCLUDING THE HIGH BIT), AND AT MOST ONE PAIR OF CONSECUTIVE ZERO
    // BITS.  THE CONVERTED BLOCK OF 343 BYTES IS STORED STARTING AT 4K INTO THE
    // WORK BUFFER.
    {
        LPBYTE sourceptr = workBuffer + 0x1400;
        LPBYTE resultptr = workBuffer + 0x1000;
        int    loop = 343;
        while (loop--)
            *resultptr++ = diskByte[*sourceptr++ >> 2];
    }

    return workBuffer + 0x1000;
}

//===========================================================================
static void Decode62(LPBYTE imagePtr) {

    // IF WE HAVEN'T ALREADY DONE SO, GENERATE A TABLE FOR CONVERTING
    // DISK BYTES BACK INTO 6-BIT BYTES
    static BOOL tableGenerated = 0;
    static BYTE sixBitByte[0x80];
    if (!tableGenerated) {
        ZeroMemory(sixBitByte, 0x80);
        int loop = 0;
        while (loop < 0x40) {
            sixBitByte[diskByte[loop] - 0x80] = loop << 2;
            loop++;
        }
        tableGenerated = 1;
    }

    // USING OUR TABLE, CONVERT THE DISK BYTES BACK INTO 6-BIT BYTES
    {
        LPBYTE sourceptr = workBuffer + 0x1000;
        LPBYTE resultptr = workBuffer + 0x1400;
        int    loop = 343;
        while (loop--)
            * (resultptr++) = sixBitByte[*(sourceptr++) & 0x7F];
    }

    // EXCLUSIVE-OR THE ENTIRE DATA BLOCK WITH ITSELF OFFSET BY ONE BYTE
    // TO UNDO THE EFFECTS OF THE CHECKSUMMING PROCESS
    {
        BYTE   savedval = 0;
        LPBYTE sourceptr = workBuffer + 0x1400;
        LPBYTE resultptr = workBuffer + 0x1000;
        int    loop = 342;
        while (loop--) {
            *resultptr = savedval ^ *(sourceptr++);
            savedval = *(resultptr++);
        }
    }

    // CONVERT THE 342 6-BIT BYTES INTO 256 8-BIT BYTES
    {
        LPBYTE lowbitsptr = workBuffer + 0x1000;
        LPBYTE sectorbase = workBuffer + 0x1056;
        BYTE   offset = 0xAC;
        while (offset != 0x02) {
            if (offset >= 0xAC)
                * (imagePtr + offset) = (*(sectorbase + offset) & 0xFC)
                | (((*lowbitsptr) & 0x80) >> 7)
                | (((*lowbitsptr) & 0x40) >> 5);
            offset -= 0x56;
            *(imagePtr + offset) = (*(sectorbase + offset) & 0xFC)
                | (((*lowbitsptr) & 0x20) >> 5)
                | (((*lowbitsptr) & 0x10) >> 3);
            offset -= 0x56;
            *(imagePtr + offset) = (*(sectorbase + offset) & 0xFC)
                | (((*lowbitsptr) & 0x08) >> 3)
                | (((*lowbitsptr) & 0x04) >> 1);
            offset -= 0x53;
            lowbitsptr++;
        }
    }

}

//===========================================================================
static void DenibblizeTrack(LPBYTE trackimage, BOOL dosorder, int nibbles) {
    ZeroMemory(workBuffer, 0x1000);

    // SEARCH THROUGH THE TRACK IMAGE FOR EACH SECTOR.  FOR EVERY SECTOR
    // WE FIND, COPY THE NIBBLIZED DATA FOR THAT SECTOR INTO THE WORK
    // BUFFER AT OFFSET 4K.  THEN CALL DECODE62() TO DENIBBLIZE THE DATA
    // IN THE BUFFER AND WRITE IT INTO THE FIRST PART OF THE WORK BUFFER
    // OFFSET BY THE SECTOR NUMBER.
    int offset = 0;
    int partsleft = 33;
    int sector = 0;
    while (partsleft--) {
        BYTE byteval[3] = { 0,0,0 };
        int  bytenum = 0;
        int  loop = nibbles;
        while ((loop--) && (bytenum < 3)) {
            if (bytenum)
                byteval[bytenum++] = *(trackimage + offset++);
            else if (*(trackimage + offset++) == 0xD5)
                bytenum = 1;
            if (offset >= nibbles)
                offset = 0;
        }
        if ((bytenum == 3) && (byteval[1] == 0xAA)) {
            int loop = 0;
            int tempoffset = offset;
            while (loop < 384) {
                *(workBuffer + 0x1000 + loop++) = *(trackimage + tempoffset++);
                if (tempoffset >= nibbles)
                    tempoffset = 0;
            }
            if (byteval[2] == 0x96)
                sector = ((*(workBuffer + 0x1004) & 0x55) << 1)
                | (*(workBuffer + 0x1005) & 0x55);
            else if (byteval[2] == 0xAD) {
                Decode62(workBuffer + (sectorNumber[dosorder][sector] << 8));
                sector = 0;
            }
        }
    }
}

//===========================================================================
static DWORD NibblizeTrack(LPBYTE trackImageBuffer, BOOL dosorder, int track) {
    ZeroMemory(workBuffer + 4096, 4096);
    LPBYTE imagePtr = trackImageBuffer;
    BYTE   sector = 0;

    // WRITE GAP ONE, WHICH CONTAINS 48 SELF-SYNC BYTES
    int loop;
    for (loop = 0; loop < 48; loop++)
        *imagePtr++ = 0xFF;

    while (sector < 16) {
        // WRITE THE ADDRESS FIELD, WHICH CONTAINS:
        //   - PROLOGUE (D5AA96)
        //   - VOLUME NUMBER ("4 AND 4" ENCODED)
        //   - TRACK NUMBER ("4 AND 4" ENCODED)
        //   - SECTOR NUMBER ("4 AND 4" ENCODED)
        //   - CHECKSUM ("4 AND 4" ENCODED)
        //   - EPILOGUE (DEAAEB)
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
        for (loop = 0; loop < 6; loop++)
            *imagePtr++ = 0xFF;

        // WRITE THE DATA FIELD, WHICH CONTAINS:
        //   - PROLOGUE (D5AAAD)
        //   - 343 6-BIT BYTES OF NIBBLIZED DATA, INCLUDING A 6-BIT CHECKSUM
        //   - EPILOGUE (DEAAEB)
        *imagePtr++ = 0xD5;
        *imagePtr++ = 0xAA;
        *imagePtr++ = 0xAD;
        CopyMemory(imagePtr, Code62(sectorNumber[dosorder][sector]), 343);
        imagePtr += 343;
        *imagePtr++ = 0xDE;
        *imagePtr++ = 0xAA;
        *imagePtr++ = 0xEB;

        // WRITE GAP THREE, WHICH CONTAINS 27 SELF-SYNC BYTES
        for (loop = 0; loop < 27; loop++)
            *imagePtr++ = 0xFF;

        sector++;
    }
    return (DWORD)(imagePtr - trackImageBuffer);
}

//===========================================================================
static void SkewTrack(int track, int nibbles, LPBYTE trackImageBuffer) {
    int skewBytes = (track * 768) % nibbles;
    CopyMemory(workBuffer, trackImageBuffer, nibbles);
    CopyMemory(trackImageBuffer, workBuffer + skewBytes, nibbles - skewBytes);
    CopyMemory(trackImageBuffer + nibbles - skewBytes, workBuffer, skewBytes);
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

    if (fread(g_mem + address, length, 1, ptr->file) != 1)
        return FALSE;

    for (int loop = 0; loop < 192; ++loop)
        g_memDirty[loop] = 0xFF;

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

    ZeroMemory(workBuffer, 4096);
    if (fread(workBuffer, 4096, 1, ptr->file) != 1)
        return;

    *nibbles = NibblizeTrack(trackImageBuffer, 1, track);
    if (!optEnhancedDisk)
        SkewTrack(track, *nibbles, trackImageBuffer);
}

//===========================================================================
static void DoWrite(imageInfo * ptr, int track, int quarterTrack, LPBYTE trackimage, int nibbles) {
    ZeroMemory(workBuffer, 4096);
    DenibblizeTrack(trackimage, 1, nibbles);

    fseek(ptr->file, ptr->offset + (track << 12), SEEK_SET);
    fwrite(workBuffer, 4096, 1, ptr->file);
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
        sectorNumber[2][loop] = found;
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
        ZeroMemory(ptr->header, 88);
        fseek(ptr->file, 0, SEEK_SET);
        if (fread(ptr->header, ARRSIZE(ptr->header), 1, ptr->file) != 1)
            return;
    }

    // IF THIS IMAGE CONTAINS USER DATA, READ THE TRACK AND NIBBLIZE IT
    if (ptr->header[13] <= 2) {
        IieConvertSectorOrder(ptr->header + 14);
        fseek(ptr->file, (track << 12) + 30, SEEK_SET);
        ZeroMemory(workBuffer, 4096);
        if (fread(workBuffer, 4096, 1, ptr->file) != 1)
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
        ZeroMemory(trackImageBuffer, *nibbles);
        if (fread(trackImageBuffer, *nibbles, 1, ptr->file) != 1)
            return;
    }

}

//===========================================================================
static void IieWrite(imageInfo * ptr, int track, int quarterTrack, LPBYTE trackimage, int nibbles) {
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
static void Nib1Write(imageInfo * ptr, int track, int quarterTrack, LPBYTE trackimage, int nibbles) {
    fseek(ptr->file, ptr->offset + track * 6656, SEEK_SET);
    fwrite(trackimage, nibbles, 1, ptr->file);
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
static void Nib2Write(imageInfo * ptr, int track, int quarterTrack, LPBYTE trackimage, int nibbles) {
    fseek(ptr->file, ptr->offset + track * 6384, SEEK_SET);
    fwrite(trackimage, nibbles, 1, ptr->file);
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
    ZeroMemory(workBuffer, 4096);
    if (fread(workBuffer, 4096, 1, ptr->file) != 1)
        return;
    *nibbles = NibblizeTrack(trackImageBuffer, 0, track);
    if (!optEnhancedDisk)
        SkewTrack(track, *nibbles, trackImageBuffer);
}

//===========================================================================
static void PoWrite(imageInfo * ptr, int track, int quarterTrack, LPBYTE trackimage, int nibbles) {
    ZeroMemory(workBuffer, 4096);
    DenibblizeTrack(trackimage, 0, nibbles);
    fseek(ptr->file, ptr->offset + (track << 12), SEEK_SET);
    fwrite(workBuffer, 4096, 1, ptr->file);
}


/****************************************************************************
*
*  PRODOS PROGRAM IMAGE (PRG) FORMAT IMPLEMENTATION
*
***/

//===========================================================================
static BOOL PrgBoot(imageInfo * ptr) {
    fseek(ptr->file, 5, SEEK_SET);

    WORD address = 0;
    if (fread(&address, sizeof(WORD), 1, ptr->file) != 1)
        return FALSE;

    WORD length = 0;
    if (fread(&length, sizeof(WORD), 1, ptr->file) != 1)
        return FALSE;

    length <<= 1;
    if (address + length <= address || address >= 0xC000 || address + length - 1 >= 0xC000)
        return FALSE;

    fseek(ptr->file, 128, SEEK_SET);
    if (fread(g_mem + address, length, 1, ptr->file) != 1)
        return FALSE;

    for (int loop = 0; loop < 192; ++loop)
        g_memDirty[loop] = 0xFF;

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
    BOOL result = 0;
    if (spec[ptr->format].boot)
        result = spec[ptr->format].boot(ptr);
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
    delete[] workBuffer;
    workBuffer = NULL;
}

//===========================================================================
void ImageInitialize() {
    workBuffer = new BYTE[0x2000];
    ZeroMemory(workBuffer, 0x2000);
}

//===========================================================================
BOOL ImageOpen (
    const char *    imageFilename,
    HIMAGE *        imageHandle,
    BOOL *          writeProtected,
    BOOL            createIfNecessary
) {
    if (imageHandle)
        *imageHandle = (HIMAGE)NULL;
    if (writeProtected)
        *writeProtected = FALSE;
    if (!(imageFilename && imageHandle && writeProtected && workBuffer))
        return FALSE;

    // TRY TO OPEN THE IMAGE FILE
    BOOL readonly = FALSE;
    FILE * file = fopen(imageFilename, "rb+");
    if (!file) {
        readonly = TRUE;
        file = fopen(imageFilename, "rb");
    }

    // IF WE ARE ABLE TO OPEN THE FILE, MAP IT INTO MEMORY FOR USE BY THE
    // DETECTION FUNCTIONS
    if (file) {
        fseek(file, 0, SEEK_END);
        fpos_t pos;
        if (fgetpos(file, &pos) != 0) {
            fclose(file);
            return FALSE;
        }

        fseek(file, 0, SEEK_SET);
        int size = (int)pos;
        LPBYTE buf = new BYTE[size];
        if (fread(buf, size, 1, file) != 1) {
            fclose(file);
            return FALSE;
        }

        LPBYTE imagePtr = buf;
        DWORD  format = 0xFFFFFFFF;
        if (imagePtr) {

            // DETERMINE WHETHER THE FILE HAS A 128-BYTE MACBINARY HEADER
            if ((size > 128) &&
                (!*imagePtr) &&
                (*(imagePtr + 1) < 120) &&
                (!*(imagePtr + *(imagePtr + 1) + 2)) &&
                (*(imagePtr + 0x7A) == 0x81) &&
                (*(imagePtr + 0x7B) == 0x81))
            {
                imagePtr += 128;
                size -= 128;
            }

            // DETERMINE THE FILE'S EXTENSION
            const char * ext = imageFilename;
            while (StrChr(ext, '\\'))
                ext = StrChr(ext, '\\') + 1;
            while (StrChr(ext + 1, '.'))
                ext = StrChr(ext + 1, '.');

            // CALL THE DETECTION FUNCTIONS IN ORDER, LOOKING FOR A MATCH
            DWORD possibleformat = 0xFFFFFFFF;
            int   loop = 0;
            while ((loop < IMAGETYPES) && (format == 0xFFFFFFFF)) {
                BOOL reject = 0;
                if (ext && *ext) {
                    const char * rejectexts = spec[loop].rejectExts;
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
                    DWORD result = spec[loop].detect(imagePtr, size);
                    if (result == 2)
                        format = loop;
                    else if ((result == 1) && (possibleformat == 0xFFFFFFFF))
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

        // IF THE FILE DOES NOT MATCH ANY KNOWN FORMAT, CLOSE IT AND RETURN
        if (format == 0xFFFFFFFF) {
            fclose(file);
            return FALSE;
        }

        // OTHERWISE, CREATE A RECORD FOR THE FILE, AND RETURN AN IMAGE HANDLE
        else {
            imageInfo * ptr = new imageInfo;
            if (ptr) {
                ZeroMemory(ptr, sizeof(imageInfo));
                ptr->format         = format;
                ptr->file           = file;
                ptr->offset         = offset;
                ptr->writeProtected = readonly;
                if (imageHandle)
                    *imageHandle = (HIMAGE)ptr;
                if (writeProtected)
                    *writeProtected = readonly;
                return TRUE;
            }
            else {
                fclose(file);
                return FALSE;
            }
        }
    }

    return FALSE;
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
    if (spec[ptr->format].read)
        spec[ptr->format].read(ptr, track, quarterTrack, trackImageBuffer, nibbles);
}

//===========================================================================
void ImageWriteTrack (
    HIMAGE  imageHandle,
    int     track,
    int     quarterTrack,
    LPBYTE  trackimage,
    int     nibbles
) {
    imageInfo * ptr = (imageInfo *)imageHandle;
    if (spec[ptr->format].write && !ptr->writeProtected)
        spec[ptr->format].write(ptr, track, quarterTrack, trackimage, nibbles);
}
