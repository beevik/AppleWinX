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

struct imageinfo {
    DWORD      format;
    FILE *     file;
    DWORD      offset;
    BOOL       writeprotected;
    DWORD      headersize;
    LPBYTE     header;
};

typedef BOOL  (* fboot)(imageinfo * info);
typedef DWORD (* fdetect)(LPBYTE imageptr, DWORD size);
typedef void  (* freadinfo)(imageinfo * info, int track, int quartertrack, LPBYTE buffer, int * nibbles);
typedef void  (* fwriteinfo)(imageinfo * info, int track, int quartertrack, LPBYTE buffer, int nibbles);

static BOOL  AplBoot(imageinfo * ptr);
static DWORD AplDetect(LPBYTE imageptr, DWORD imagesize);
static DWORD DoDetect(LPBYTE imageptr, DWORD imagesize);
static void  DoRead(imageinfo * ptr, int track, int quartertrack, LPBYTE buffer, int * nibbles);
static void  DoWrite(imageinfo * ptr, int track, int quartertrack, LPBYTE buffer, int nibbles);
static DWORD IieDetect(LPBYTE imageptr, DWORD imagesize);
static void  IieRead(imageinfo * ptr, int track, int quartertrack, LPBYTE buffer, int * nibbles);
static void  IieWrite(imageinfo * ptr, int track, int quartertrack, LPBYTE buffer, int nibbles);
static DWORD Nib1Detect(LPBYTE imageptr, DWORD imagesize);
static void  Nib1Read(imageinfo * ptr, int track, int quartertrack, LPBYTE buffer, int * nibbles);
static void  Nib1Write(imageinfo * ptr, int track, int quartertrack, LPBYTE buffer, int nibbles);
static DWORD Nib2Detect(LPBYTE imageptr, DWORD imagesize);
static void  Nib2Read(imageinfo * ptr, int track, int quartertrack, LPBYTE buffer, int * nibbles);
static void  Nib2Write(imageinfo * ptr, int track, int quartertrack, LPBYTE buffer, int nibbles);
static DWORD PoDetect(LPBYTE imageptr, DWORD imagesize);
static void  PoRead(imageinfo * ptr, int track, int quartertrack, LPBYTE buffer, int * nibbles);
static void  PoWrite(imageinfo * ptr, int track, int quartertrack, LPBYTE buffer, int nibbles);
static BOOL  PrgBoot(imageinfo * ptr);
static DWORD PrgDetect(LPBYTE imageptr, DWORD imagesize);

struct imagespec {
    const char * createexts;
    const char * rejectexts;
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

static const BYTE diskbyte[0x40] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};

static BYTE sectornumber[3][0x10] = {
    { 0x00, 0x08, 0x01, 0x09, 0x02, 0x0A, 0x03, 0x0B, 0x04, 0x0C, 0x05, 0x0D, 0x06, 0x0E, 0x07, 0x0F },
    { 0x00, 0x07, 0x0E, 0x06, 0x0D, 0x05, 0x0C, 0x04, 0x0B, 0x03, 0x0A, 0x02, 0x09, 0x01, 0x08, 0x0F },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
};

static LPBYTE workbuffer = NULL;


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
        LPBYTE sectorbase = workbuffer + (sector << 8);
        LPBYTE resultptr = workbuffer + 0x1000;
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
            * (resultptr++) = value << 2;
        }
        *(resultptr - 2) &= 0x3F;
        *(resultptr - 1) &= 0x3F;
        offset = 0;
        do
            *(resultptr++) = *(sectorbase + (offset++));
        while (offset);
    }

    // EXCLUSIVE-OR THE ENTIRE DATA BLOCK WITH ITSELF OFFSET BY ONE BYTE,
    // CREATING A 343RD BYTE WHICH IS USED AS A CHECKSUM.  STORE THE NEW
    // BLOCK OF 343 BYTES STARTING AT 5K INTO THE WORK BUFFER.
    {
        BYTE   savedval = 0;
        LPBYTE sourceptr = workbuffer + 0x1000;
        LPBYTE resultptr = workbuffer + 0x1400;
        int    loop = 342;
        while (loop--) {
            *(resultptr++) = savedval ^ *sourceptr;
            savedval = *(sourceptr++);
        }
        *resultptr = savedval;
    }

    // USING A LOOKUP TABLE, CONVERT THE 6-BIT BYTES INTO DISK BYTES.  A VALID
    // DISK BYTE IS A BYTE THAT HAS THE HIGH BIT SET, AT LEAST TWO ADJACENT
    // BITS SET (EXCLUDING THE HIGH BIT), AND AT MOST ONE PAIR OF CONSECUTIVE
    // ZERO BITS.  THE CONVERTED BLOCK OF 343 BYTES IS STORED STARTING AT 4K
    // INTO THE WORK BUFFER.
    {
        LPBYTE sourceptr = workbuffer + 0x1400;
        LPBYTE resultptr = workbuffer + 0x1000;
        int    loop = 343;
        while (loop--)
            * (resultptr++) = diskbyte[(*(sourceptr++)) >> 2];
    }

    return workbuffer + 0x1000;
}

//===========================================================================
static void Decode62(LPBYTE imageptr) {

    // IF WE HAVEN'T ALREADY DONE SO, GENERATE A TABLE FOR CONVERTING
    // DISK BYTES BACK INTO 6-BIT BYTES
    static BOOL tablegenerated = 0;
    static BYTE sixbitbyte[0x80];
    if (!tablegenerated) {
        ZeroMemory(sixbitbyte, 0x80);
        int loop = 0;
        while (loop < 0x40) {
            sixbitbyte[diskbyte[loop] - 0x80] = loop << 2;
            loop++;
        }
        tablegenerated = 1;
    }

    // USING OUR TABLE, CONVERT THE DISK BYTES BACK INTO 6-BIT BYTES
    {
        LPBYTE sourceptr = workbuffer + 0x1000;
        LPBYTE resultptr = workbuffer + 0x1400;
        int    loop = 343;
        while (loop--)
            * (resultptr++) = sixbitbyte[*(sourceptr++) & 0x7F];
    }

    // EXCLUSIVE-OR THE ENTIRE DATA BLOCK WITH ITSELF OFFSET BY ONE BYTE
    // TO UNDO THE EFFECTS OF THE CHECKSUMMING PROCESS
    {
        BYTE   savedval = 0;
        LPBYTE sourceptr = workbuffer + 0x1400;
        LPBYTE resultptr = workbuffer + 0x1000;
        int    loop = 342;
        while (loop--) {
            *resultptr = savedval ^ *(sourceptr++);
            savedval = *(resultptr++);
        }
    }

    // CONVERT THE 342 6-BIT BYTES INTO 256 8-BIT BYTES
    {
        LPBYTE lowbitsptr = workbuffer + 0x1000;
        LPBYTE sectorbase = workbuffer + 0x1056;
        BYTE   offset = 0xAC;
        while (offset != 0x02) {
            if (offset >= 0xAC)
                * (imageptr + offset) = (*(sectorbase + offset) & 0xFC)
                | (((*lowbitsptr) & 0x80) >> 7)
                | (((*lowbitsptr) & 0x40) >> 5);
            offset -= 0x56;
            *(imageptr + offset) = (*(sectorbase + offset) & 0xFC)
                | (((*lowbitsptr) & 0x20) >> 5)
                | (((*lowbitsptr) & 0x10) >> 3);
            offset -= 0x56;
            *(imageptr + offset) = (*(sectorbase + offset) & 0xFC)
                | (((*lowbitsptr) & 0x08) >> 3)
                | (((*lowbitsptr) & 0x04) >> 1);
            offset -= 0x53;
            lowbitsptr++;
        }
    }

}

//===========================================================================
static void DenibblizeTrack(LPBYTE trackimage, BOOL dosorder, int nibbles) {
    ZeroMemory(workbuffer, 0x1000);

    // SEARCH THROUGH THE TRACK IMAGE FOR EACH SECTOR.  FOR EVERY SECTOR
    // WE FIND, COPY THE NIBBLIZED DATA FOR THAT SECTOR INTO THE WORK
    // BUFFER AT OFFSET 4K.  THEN CALL DECODE62() TO DENIBBLIZE THE DATA
    // IN THE BUFFER AND WRITE IT INTO THE FIRST PART OF THE WORK BUFFER
    // OFFSET BY THE SECTOR NUMBER.
    {
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
                    *(workbuffer + 0x1000 + loop++) = *(trackimage + tempoffset++);
                    if (tempoffset >= nibbles)
                        tempoffset = 0;
                }
                if (byteval[2] == 0x96)
                    sector = ((*(workbuffer + 0x1004) & 0x55) << 1)
                    | (*(workbuffer + 0x1005) & 0x55);
                else if (byteval[2] == 0xAD) {
                    Decode62(workbuffer + (sectornumber[dosorder][sector] << 8));
                    sector = 0;
                }
            }
        }
    }
}

//===========================================================================
static DWORD NibblizeTrack(LPBYTE trackimagebuffer, BOOL dosorder, int track) {
    ZeroMemory(workbuffer + 4096, 4096);
    LPBYTE imageptr = trackimagebuffer;
    BYTE   sector = 0;

    // WRITE GAP ONE, WHICH CONTAINS 48 SELF-SYNC BYTES
    int loop;
    for (loop = 0; loop < 48; loop++)
        * (imageptr++) = 0xFF;

    while (sector < 16) {

        // WRITE THE ADDRESS FIELD, WHICH CONTAINS:
        //   - PROLOGUE (D5AA96)
        //   - VOLUME NUMBER ("4 AND 4" ENCODED)
        //   - TRACK NUMBER ("4 AND 4" ENCODED)
        //   - SECTOR NUMBER ("4 AND 4" ENCODED)
        //   - CHECKSUM ("4 AND 4" ENCODED)
        //   - EPILOGUE (DEAAEB)
        *(imageptr++) = 0xD5;
        *(imageptr++) = 0xAA;
        *(imageptr++) = 0x96;
        *(imageptr++) = 0xFF;
        *(imageptr++) = 0xFE;
#define CODE44A(a) ((((a) >> 1) & 0x55) | 0xAA)
#define CODE44B(a) (((a) & 0x55) | 0xAA)
        * (imageptr++) = CODE44A((BYTE)track);
        *(imageptr++) = CODE44B((BYTE)track);
        *(imageptr++) = CODE44A(sector);
        *(imageptr++) = CODE44B(sector);
        *(imageptr++) = CODE44A(0xFE ^ ((BYTE)track) ^ sector);
        *(imageptr++) = CODE44B(0xFE ^ ((BYTE)track) ^ sector);
#undef CODE44A
#undef CODE44B
        * (imageptr++) = 0xDE;
        *(imageptr++) = 0xAA;
        *(imageptr++) = 0xEB;

        // WRITE GAP TWO, WHICH CONTAINS SIX SELF-SYNC BYTES
        for (loop = 0; loop < 6; loop++)
            * (imageptr++) = 0xFF;

        // WRITE THE DATA FIELD, WHICH CONTAINS:
        //   - PROLOGUE (D5AAAD)
        //   - 343 6-BIT BYTES OF NIBBLIZED DATA, INCLUDING A 6-BIT CHECKSUM
        //   - EPILOGUE (DEAAEB)
        *(imageptr++) = 0xD5;
        *(imageptr++) = 0xAA;
        *(imageptr++) = 0xAD;
        CopyMemory(imageptr, Code62(sectornumber[dosorder][sector]), 343);
        imageptr += 343;
        *(imageptr++) = 0xDE;
        *(imageptr++) = 0xAA;
        *(imageptr++) = 0xEB;

        // WRITE GAP THREE, WHICH CONTAINS 27 SELF-SYNC BYTES
        for (loop = 0; loop < 27; loop++)
            * (imageptr++) = 0xFF;

        sector++;
    }
    return (DWORD)(imageptr - trackimagebuffer);
}

//===========================================================================
static void SkewTrack(int track, int nibbles, LPBYTE trackimagebuffer) {
    int skewbytes = (track * 768) % nibbles;
    CopyMemory(workbuffer, trackimagebuffer, nibbles);
    CopyMemory(trackimagebuffer, workbuffer + skewbytes, nibbles - skewbytes);
    CopyMemory(trackimagebuffer + nibbles - skewbytes, workbuffer, skewbytes);
}


/****************************************************************************
*
*  RAW PROGRAM IMAGE (APL) FORMAT IMPLEMENTATION
*
***/

//===========================================================================
static BOOL AplBoot(imageinfo * ptr) {
    fseek(ptr->file, 0, SEEK_SET);

    WORD address = 0;
    if (fread(&address, sizeof(WORD), 1, ptr->file) != 1)
        return FALSE;

    WORD length = 0;
    if (fread(&length, sizeof(WORD), 1, ptr->file) != 1)
        return FALSE;

    if (address + length <= address || address >= 0xC000 || address + length - 1 >= 0xC000)
        return FALSE;

    if (fread(mem + address, length, 1, ptr->file) != 1)
        return FALSE;

    for (int loop = 0; loop < 192; ++loop)
        memdirty[loop] = 0xFF;

    regs.pc = address;
    return TRUE;
}

//===========================================================================
static DWORD AplDetect(LPBYTE imageptr, DWORD imagesize) {
    DWORD length = *(LPWORD)(imageptr + 2);
    return (((length + 4) == imagesize) ||
        ((length + 4 + ((256 - ((length + 4) & 255)) & 255)) == imagesize));
}

/****************************************************************************
*
*  DOS ORDER (DO) FORMAT IMPLEMENTATION
*
***/

//===========================================================================
static DWORD DoDetect(LPBYTE imageptr, DWORD imagesize) {
    if (((imagesize < 143105) || (imagesize > 143364)) &&
        (imagesize != 143403) && (imagesize != 143488))
        return 0;

    // CHECK FOR A DOS ORDER IMAGE OF A DOS DISKETTE
    {
        int  loop = 0;
        BOOL mismatch = 0;
        while ((loop++ < 15) && !mismatch)
            if (*(imageptr + 0x11002 + (loop << 8)) != loop - 1)
                mismatch = 1;
        if (!mismatch)
            return 2;
    }

    // CHECK FOR A DOS ORDER IMAGE OF A PRODOS DISKETTE
    {
        int  loop = 1;
        BOOL mismatch = 0;
        while ((loop++ < 5) && !mismatch)
            if ((*(LPWORD)(imageptr + (loop << 9) + 0x100) != ((loop == 5) ? 0 : 6 - loop)) ||
                (*(LPWORD)(imageptr + (loop << 9) + 0x102) != ((loop == 2) ? 0 : 8 - loop)))
                mismatch = 1;
        if (!mismatch)
            return 2;
    }

    return 1;
}

//===========================================================================
static void DoRead(imageinfo * ptr, int track, int quartertrack, LPBYTE trackimagebuffer, int * nibbles) {
    fseek(ptr->file, ptr->offset + (track << 12), SEEK_SET);

    ZeroMemory(workbuffer, 4096);
    if (fread(workbuffer, 4096, 1, ptr->file) != 1)
        return;

    *nibbles = NibblizeTrack(trackimagebuffer, 1, track);
    if (!optenhancedisk)
        SkewTrack(track, *nibbles, trackimagebuffer);
}

//===========================================================================
static void DoWrite(imageinfo * ptr, int track, int quartertrack, LPBYTE trackimage, int nibbles) {
    ZeroMemory(workbuffer, 4096);
    DenibblizeTrack(trackimage, 1, nibbles);

    fseek(ptr->file, ptr->offset + (track << 12), SEEK_SET);
    fwrite(workbuffer, 4096, 1, ptr->file);
}


/****************************************************************************
*
*  SIMSYSTEM IIE (IIE) FORMAT IMPLEMENTATION
*
***/

//===========================================================================
static void IieConvertSectorOrder(LPBYTE sourceorder) {
    int loop = 16;
    while (loop--) {
        BYTE found = 0xFF;
        int  loop2 = 16;
        while (loop2-- && (found == 0xFF))
            if (*(sourceorder + loop2) == loop)
                found = loop2;
        if (found == 0xFF)
            found = 0;
        sectornumber[2][loop] = found;
    }
}

//===========================================================================
static DWORD IieDetect(LPBYTE imageptr, DWORD imagesize) {
    if (strncmp((const char *)imageptr, "SIMSYSTEM_IIE", 13) ||
        (*(imageptr + 13) > 3))
        return 0;
    return 2;
}

//===========================================================================
static void IieRead(imageinfo * ptr, int track, int quartertrack, LPBYTE trackimagebuffer, int * nibbles) {

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
        ZeroMemory(workbuffer, 4096);
        if (fread(workbuffer, 4096, 1, ptr->file) != 1)
            return;
        *nibbles = NibblizeTrack(trackimagebuffer, 2, track);
    }

    // OTHERWISE, IF THIS IMAGE CONTAINS NIBBLE INFORMATION, READ IT
    // DIRECTLY INTO THE TRACK BUFFER
    else {
        *nibbles = *(LPWORD)(ptr->header + (track << 1) + 14);
        DWORD offset = 88;
        while (track--)
            offset += *(LPWORD)(ptr->header + (track << 1) + 14);
        fseek(ptr->file, offset, SEEK_SET);
        ZeroMemory(trackimagebuffer, *nibbles);
        if (fread(trackimagebuffer, *nibbles, 1, ptr->file) != 1)
            return;
    }

}

//===========================================================================
static void IieWrite(imageinfo * ptr, int track, int quartertrack, LPBYTE trackimage, int nibbles) {
  // note: unimplemented
}


/****************************************************************************
*
*  NIBBLIZED 6656-NIBBLE (NIB) FORMAT IMPLEMENTATION
*
***/

//===========================================================================
static DWORD Nib1Detect(LPBYTE imageptr, DWORD imagesize) {
    return (imagesize == 232960) ? 2 : 0;
}

//===========================================================================
static void Nib1Read(imageinfo * ptr, int track, int quartertrack, LPBYTE trackimagebuffer, int * nibbles) {
    fseek(ptr->file, ptr->offset + track * 6656, SEEK_SET);
    *nibbles = (int)fread(trackimagebuffer, 1, 6656, ptr->file);
}

//===========================================================================
static void Nib1Write(imageinfo * ptr, int track, int quartertrack, LPBYTE trackimage, int nibbles) {
    fseek(ptr->file, ptr->offset + track * 6656, SEEK_SET);
    fwrite(trackimage, nibbles, 1, ptr->file);
}


/****************************************************************************
*
*  NIBBLIZED 6384-NIBBLE (NB2) FORMAT IMPLEMENTATION
*
***/

//===========================================================================
static DWORD Nib2Detect(LPBYTE imageptr, DWORD imagesize) {
    return (imagesize == 223440) ? 2 : 0;
}

//===========================================================================
static void Nib2Read(imageinfo * ptr, int track, int quartertrack, LPBYTE trackimagebuffer, int * nibbles) {
    fseek(ptr->file, ptr->offset + track * 6384, SEEK_SET);
    *nibbles = (int)fread(trackimagebuffer, 1, 6384, NULL);
}

//===========================================================================
static void Nib2Write(imageinfo * ptr, int track, int quartertrack, LPBYTE trackimage, int nibbles) {
    fseek(ptr->file, ptr->offset + track * 6384, SEEK_SET);
    fwrite(trackimage, nibbles, 1, ptr->file);
}


/****************************************************************************
*
*  PRODOS ORDER (PO) FORMAT IMPLEMENTATION
*
***/

//===========================================================================
static DWORD PoDetect(LPBYTE imageptr, DWORD imagesize) {
    if (((imagesize < 143105) || (imagesize > 143364)) &&
        (imagesize != 143488))
        return 0;

    // CHECK FOR A PRODOS ORDER IMAGE OF A DOS DISKETTE
    {
        int  loop = 4;
        BOOL mismatch = 0;
        while ((loop++ < 13) && !mismatch)
            if (*(imageptr + 0x11002 + (loop << 8)) != 14 - loop)
                mismatch = 1;
        if (!mismatch)
            return 2;
    }

    // CHECK FOR A PRODOS ORDER IMAGE OF A PRODOS DISKETTE
    {
        int  loop = 1;
        BOOL mismatch = 0;
        while ((loop++ < 5) && !mismatch)
            if ((*(LPWORD)(imageptr + (loop << 9)) != ((loop == 2) ? 0 : loop - 1)) ||
                (*(LPWORD)(imageptr + (loop << 9) + 2) != ((loop == 5) ? 0 : loop + 1)))
                mismatch = 1;
        if (!mismatch)
            return 2;
    }

    return 1;
}

//===========================================================================
static void PoRead(imageinfo * ptr, int track, int quartertrack, LPBYTE trackimagebuffer, int * nibbles) {
    fseek(ptr->file, ptr->offset + (track << 12), SEEK_SET);
    ZeroMemory(workbuffer, 4096);
    if (fread(workbuffer, 4096, 1, ptr->file) != 1)
        return;
    *nibbles = NibblizeTrack(trackimagebuffer, 0, track);
    if (!optenhancedisk)
        SkewTrack(track, *nibbles, trackimagebuffer);
}

//===========================================================================
static void PoWrite(imageinfo * ptr, int track, int quartertrack, LPBYTE trackimage, int nibbles) {
    ZeroMemory(workbuffer, 4096);
    DenibblizeTrack(trackimage, 0, nibbles);
    fseek(ptr->file, ptr->offset + (track << 12), SEEK_SET);
    fwrite(workbuffer, 4096, 1, ptr->file);
}


/****************************************************************************
*
*  PRODOS PROGRAM IMAGE (PRG) FORMAT IMPLEMENTATION
*
***/

//===========================================================================
static BOOL PrgBoot(imageinfo * ptr) {
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
    if (fread(mem + address, length, 1, ptr->file) != 1)
        return FALSE;

    for (int loop = 0; loop < 192; ++loop)
        memdirty[loop] = 0xFF;

    regs.pc = address;
    return TRUE;
}

//===========================================================================
static DWORD PrgDetect(LPBYTE imageptr, DWORD imagesize) {
    return (*(LPDWORD)imageptr == 0x214C470A) ? 2 : 0;
}


//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
BOOL ImageBoot(HIMAGE imagehandle) {
    imageinfo * ptr = (imageinfo *)imagehandle;
    BOOL result = 0;
    if (spec[ptr->format].boot)
        result = spec[ptr->format].boot(ptr);
    if (result)
        ptr->writeprotected = 1;
    return result;
}

//===========================================================================
void ImageClose(HIMAGE imagehandle) {
    imageinfo * ptr = (imageinfo *)imagehandle;
    if (ptr->file)
        fclose(ptr->file);
    if (ptr->header)
        delete[] ptr->header;
    delete ptr;
}

//===========================================================================
void ImageDestroy() {
    delete[] workbuffer;
    workbuffer = NULL;
}

//===========================================================================
void ImageInitialize() {
    workbuffer = new BYTE[0x2000];
    ZeroMemory(workbuffer, 0x2000);
}

//===========================================================================
BOOL ImageOpen (
    const char *    imagefilename,
    HIMAGE *        imagehandle,
    BOOL *          writeprotected,
    BOOL            createifnecessary
) {
    if (imagehandle)
        *imagehandle = (HIMAGE)NULL;
    if (writeprotected)
        *writeprotected = FALSE;
    if (!(imagefilename && imagehandle && writeprotected && workbuffer))
        return FALSE;

    // TRY TO OPEN THE IMAGE FILE
    BOOL readonly = FALSE;
    FILE * file = fopen(imagefilename, "rb+");
    if (!file) {
        readonly = TRUE;
        file = fopen(imagefilename, "rb");
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

        LPBYTE imageptr = buf;
        DWORD  format = 0xFFFFFFFF;
        if (imageptr) {

            // DETERMINE WHETHER THE FILE HAS A 128-BYTE MACBINARY HEADER
            if ((size > 128) &&
                (!*imageptr) &&
                (*(imageptr + 1) < 120) &&
                (!*(imageptr + *(imageptr + 1) + 2)) &&
                (*(imageptr + 0x7A) == 0x81) &&
                (*(imageptr + 0x7B) == 0x81))
            {
                imageptr += 128;
                size -= 128;
            }

            // DETERMINE THE FILE'S EXTENSION
            const char * ext = imagefilename;
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
                    const char * rejectexts = spec[loop].rejectexts;
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
                    DWORD result = spec[loop].detect(imageptr, size);
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
        DWORD offset = (DWORD)(imageptr - buf);
        delete[] buf;

        // IF THE FILE DOES NOT MATCH ANY KNOWN FORMAT, CLOSE IT AND RETURN
        if (format == 0xFFFFFFFF) {
            fclose(file);
            return FALSE;
        }

        // OTHERWISE, CREATE A RECORD FOR THE FILE, AND RETURN AN IMAGE HANDLE
        else {
            imageinfo * ptr = new imageinfo;
            if (ptr) {
                ZeroMemory(ptr, sizeof(imageinfo));
                ptr->format         = format;
                ptr->file           = file;
                ptr->offset         = offset;
                ptr->writeprotected = readonly;
                if (imagehandle)
                    *imagehandle = (HIMAGE)ptr;
                if (writeprotected)
                    *writeprotected = readonly;
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
    HIMAGE  imagehandle,
    int     track,
    int     quartertrack,
    LPBYTE  trackimagebuffer,
    int *   nibbles
) {
    *nibbles = 0;
    imageinfo * ptr = (imageinfo *)imagehandle;
    if (spec[ptr->format].read)
        spec[ptr->format].read(ptr, track, quartertrack, trackimagebuffer, nibbles);
}

//===========================================================================
void ImageWriteTrack (
    HIMAGE  imagehandle,
    int     track,
    int     quartertrack,
    LPBYTE  trackimage,
    int     nibbles
) {
    imageinfo * ptr = (imageinfo *)imagehandle;
    if (spec[ptr->format].write && !ptr->writeprotected)
        spec[ptr->format].write(ptr, track, quartertrack, trackimage, nibbles);
}
