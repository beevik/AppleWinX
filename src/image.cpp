/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

enum EImageMatch {
    IMAGEMATCH_FAIL = 0,
    IMAGEMATCH_MAYBE = 1,
    IMAGEMATCH_FOUND = 2,
};

struct Image {
    int        format;
    FILE *     file;
    int        offset;
    bool       writeProtected;
};

typedef bool        (* FBoot)(Image * info);
typedef EImageMatch (* FDetect)(uint8_t * imagePtr, int imageSize);
typedef void        (* FReadInfo)(Image * info, int track, int quarterTrack, uint8_t * buffer, int * nibbles);
typedef void        (* FWriteInfo)(Image * info, int track, int quarterTrack, uint8_t * buffer, int nibbles);

static bool        AplBoot(Image * ptr);
static EImageMatch AplDetect(uint8_t * imagePtr, int imageSize);
static EImageMatch DoDetect(uint8_t * imagePtr, int imageSize);
static void        DoRead(Image * ptr, int track, int quarterTrack, uint8_t * buffer, int * nibbles);
static void        DoWrite(Image * ptr, int track, int quarterTrack, uint8_t * buffer, int nibbles);
static EImageMatch Nib1Detect(uint8_t * imagePtr, int imageSize);
static void        Nib1Read(Image * ptr, int track, int quarterTrack, uint8_t * buffer, int * nibbles);
static void        Nib1Write(Image * ptr, int track, int quarterTrack, uint8_t * buffer, int nibbles);
static EImageMatch Nib2Detect(uint8_t * imagePtr, int imageSize);
static void        Nib2Read(Image * ptr, int track, int quarterTrack, uint8_t * buffer, int * nibbles);
static void        Nib2Write(Image * ptr, int track, int quarterTrack, uint8_t * buffer, int nibbles);
static EImageMatch PoDetect(uint8_t * imagePtr, int imageSize);
static void        PoRead(Image * ptr, int track, int quarterTrack, uint8_t * buffer, int * nibbles);
static void        PoWrite(Image * ptr, int track, int quarterTrack, uint8_t * buffer, int nibbles);
static bool        PrgBoot(Image * ptr);
static EImageMatch PrgDetect(uint8_t * imagePtr, int imageSize);

enum EImageFormat {
    IMAGEFORMAT_PRG,
    IMAGEFORMAT_DSK,
    IMAGEFORMAT_PO,
    IMAGEFORMAT_APL,
    IMAGEFORMAT_NIB1,
    IMAGEFORMAT_NIB2,
    IMAGEFORMATS,

    IMAGEFORMAT_UNKNOWN = IMAGEFORMATS
};

struct ImageFormatData {
    const char * primaryExt;
    const char * excludedExts[4];
    FDetect      detect;
    FBoot        boot;
    FReadInfo    read;
    FWriteInfo   write;
};

static const ImageFormatData s_imageFormatData[] = {
    {
        ".prg",
        { ".do", ".dsk", ".nib", ".po" },
        PrgDetect,
        PrgBoot,
        nullptr,
        nullptr
    },
    {
        ".dsk",
        { ".nib", ".po", ".prg" },
        DoDetect,
        nullptr,
        DoRead,
        DoWrite
    },
    {
        ".po",
        { ".do", ".nib", ".prg" },
        PoDetect,
        nullptr,
        PoRead,
        PoWrite
    },
    {
        ".apl",
        { ".do", ".dsk", ".nib", ".po" },
        AplDetect,
        AplBoot,
        nullptr,
        nullptr
    },
    {
        ".nib",
        { ".do", ".po", ".prg" },
        Nib1Detect,
        nullptr,
        Nib1Read,
        Nib1Write
    },
    {
        ".nb2",
        { ".do", ".po", ".prg" },
        Nib2Detect,
        nullptr,
        Nib2Read,
        Nib2Write
    },
};

static_assert(ARRSIZE(s_imageFormatData) == IMAGEFORMATS, "s_imageFormats array mismatch");

static const uint8_t s_diskByte[0x40] = {
    0x96, 0x97, 0x9a, 0x9b, 0x9d, 0x9e, 0x9f, 0xa6,
    0xa7, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb2, 0xb3,
    0xb4, 0xb5, 0xb6, 0xb7, 0xb9, 0xba, 0xbb, 0xbc,
    0xbd, 0xbe, 0xbf, 0xcb, 0xcd, 0xce, 0xcf, 0xd3,
    0xd6, 0xd7, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
    0xdf, 0xe5, 0xe6, 0xe7, 0xe9, 0xea, 0xeb, 0xec,
    0xed, 0xee, 0xef, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
    0xf7, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
};

/*
    DOS:
        logical order  0 1 2 3 4 5 6 7 8 9 A B C D E F
        physical order 0 D B 9 7 5 3 1 E C A 8 6 4 2 F

    ProDOS:
        logical order  0 E D C B A 9 8 7 6 5 4 3 2 1 F
        physical order 0 2 4 6 8 A C E 1 3 5 7 9 B D F
*/

static uint8_t s_sectorNumber[3][16] = {
    { 0x00, 0x08, 0x01, 0x09, 0x02, 0x0a, 0x03, 0x0b, 0x04, 0x0c, 0x05, 0x0d, 0x06, 0x0e, 0x07, 0x0f }, // ProDOS
    { 0x00, 0x07, 0x0e, 0x06, 0x0d, 0x05, 0x0c, 0x04, 0x0b, 0x03, 0x0a, 0x02, 0x09, 0x01, 0x08, 0x0f }, // DOS
};

static uint8_t * s_workBuffer = nullptr;


/****************************************************************************
*
*  Nibbilizing functions
*
***/

//===========================================================================
static uint8_t * Code62(int sector) {

    // Convert the 256 8-bit bytes into 342 6-bit bytes, which we store
    // starting 4k bytes into the work buffer.
    {
        uint8_t * sectorBase = s_workBuffer + ((uintptr_t)sector << 8);
        uint8_t * resultPtr = s_workBuffer + 0x1000;
        uint8_t   offset = 0xac;
        while (offset != 0x02) {
            uint8_t value = 0;
#define ADDVALUE(a) value = (value << 2) | (((a) & 0x01) << 1) | (((a) & 0x02) >> 1)
            ADDVALUE(sectorBase[offset]);  offset -= 0x56;
            ADDVALUE(sectorBase[offset]);  offset -= 0x56;
            ADDVALUE(sectorBase[offset]);  offset -= 0x53;
#undef ADDVALUE
            *resultPtr++ = value << 2;
        }
        resultPtr[-2] &= 0x3f;
        resultPtr[-1] &= 0x3f;
        offset = 0;
        do {
            *resultPtr++ = sectorBase[offset++];
        } while (offset);
    }

    // Exclusive-or the entire data block with itself offset by one byte,
    // creating a 343rd byte which is used as a checksum.  Store the new block
    // of 343 bytes starting 5k bytes into the work buffer.
    {
        uint8_t   savedVal  = 0;
        uint8_t * sourcePtr = s_workBuffer + 0x1000;
        uint8_t * resultPtr = s_workBuffer + 0x1400;
        for (int loop = 342; loop--; ) {
            *resultPtr++ = savedVal ^ *sourcePtr;
            savedVal = *sourcePtr++;
        }
        *resultPtr = savedVal;
    }

    // Using a lookup table, convert the 6-bit bytes into disk bytes.  A valid
    // disk byte is a byte that has the high bit set, at least two adjacent
    // bits set (excluding the high bit), and at most one pair of consecutive
    // zero bits.  The converted block of 343 bytes is stored starting 4k
    // bytes into the work buffer.
    {
        uint8_t * sourcePtr = s_workBuffer + 0x1400;
        uint8_t * resultPtr = s_workBuffer + 0x1000;
        for (int loop = 343; loop--; )
            *resultPtr++ = s_diskByte[*sourcePtr++ >> 2];
    }

    return s_workBuffer + 0x1000;
}

//===========================================================================
static void Decode62(uint8_t * imagePtr) {

    // If we haven't already done so, generate a table for converting
    // disk bytes back into 6-bit bytes.
    static bool tableGenerated = false;
    static uint8_t sixBitByte[0x80];
    if (!tableGenerated) {
        memset(sixBitByte, 0, ARRSIZE(sixBitByte));
        int loop = 0;
        while (loop < 0x40) {
            sixBitByte[s_diskByte[loop] - 0x80] = loop << 2;
            loop++;
        }
        tableGenerated = true;
    }

    // Using our table, convert the disk bytes back into 6-bit bytes.
    {
        uint8_t * sourcePtr = s_workBuffer + 0x1000;
        uint8_t * resultPtr = s_workBuffer + 0x1400;
        int    loop = 343;
        while (loop--)
            * (resultPtr++) = sixBitByte[*sourcePtr++ & 0x7f];
    }

    // Exclusive-or the entire data block with itself offset by one byte
    // to undo the effects of the checksumming process.
    {
        uint8_t   savedVal = 0;
        uint8_t * sourcePtr = s_workBuffer + 0x1400;
        uint8_t * resultPtr = s_workBuffer + 0x1000;
        int       loop = 342;
        while (loop--) {
            *resultPtr = savedVal ^ *sourcePtr++;
            savedVal = *resultPtr++;
        }
    }

    // Convert the 342 6-bit bytes into 256 8-bit bytes.
    {
        uint8_t * lowBitsPtr = s_workBuffer + 0x1000;
        uint8_t * sectorBase = s_workBuffer + 0x1056;
        uint8_t   offset = 0xac;
        while (offset != 0x02) {
            if (offset >= 0xac) {
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
static void DenibblizeTrack(uint8_t * trackImage, int dosOrder, int nibbles) {
    memset(s_workBuffer, 0, 0x1000);

    int offset      = 0;
    int sector      = -1;
    int firstSector = -1;
    for (int scan = 0; scan < 64; ++scan) {

        // Search for a D5 XX XX sequence.
        uint8_t sequence[3] = { 0xd5, 0x00, 0x00 };
        for (int i = 0, count = 0; i < nibbles && count < 3; i++) {
            if (count > 0)
                sequence[count++] = trackImage[offset];
            else if (trackImage[offset] == 0xd5)
                count = 1;
            offset = (offset + 1) % nibbles;
        }

        if (sequence[1] == 0xaa) {

            // D5 AA 96 indicates a sector header.
            if (sequence[2] == 0x96) {
                for (int i = 0, tmpOffset = offset; i < 6; ++i) {
                    s_workBuffer[0x1000 + i] = trackImage[tmpOffset];
                    tmpOffset = (tmpOffset + 1) % nibbles;
                }
                sector = (s_workBuffer[0x1004] & 0x55) << 1 | (s_workBuffer[0x1005] & 0x55);
                if (sector == firstSector)
                    break;
                if (firstSector == -1)
                    firstSector = sector;
            }

            // D5 AA AD indicates a sector data field.
            else if (sequence[2] == 0xad && sector >= -1) {
                for (int i = 0, tmpOffset = offset; i < 384; ++i) {
                    s_workBuffer[0x1000 + i] = trackImage[tmpOffset];
                    tmpOffset = (tmpOffset + 1) % nibbles;
                }
                Decode62(s_workBuffer + ((uintptr_t)s_sectorNumber[dosOrder][sector] << 8));
                sector = -1;
            }
        }
    }
}

//===========================================================================
static int NibblizeTrack(uint8_t * trackImageBuffer, int dosOrder, int track) {
    memset(s_workBuffer + 0x1000, 0, 0x1000);
    uint8_t * imagePtr = trackImageBuffer;

    // Write gap one, which contains 48 self-sync bytes
    for (int loop = 0; loop < 48; loop++)
        *imagePtr++ = 0xff;

    for (uint8_t sector = 0; sector < 16; ++sector) {
        // Write the address field, which contains:
        //   - prologue (d5aa96)
        //   - volume number (4-and-4 encoded)
        //   - track number (4-and-4 encoded)
        //   - sector number (4-and-4 encoded)
        //   - checksum (4-and-4 encoded)
        //   - epilogue (deaaeb)
        *imagePtr++ = 0xd5;
        *imagePtr++ = 0xaa;
        *imagePtr++ = 0x96;
        *imagePtr++ = 0xff;
        *imagePtr++ = 0xfe;
#define CODE44A(a) ((((a) >> 1) & 0x55) | 0xaa)
#define CODE44B(a) (((a) & 0x55) | 0xaa)
        *imagePtr++ = CODE44A((uint8_t)track);
        *imagePtr++ = CODE44B((uint8_t)track);
        *imagePtr++ = CODE44A(sector);
        *imagePtr++ = CODE44B(sector);
        *imagePtr++ = CODE44A(0xfe ^ ((uint8_t)track) ^ sector);
        *imagePtr++ = CODE44B(0xfe ^ ((uint8_t)track) ^ sector);
#undef CODE44A
#undef CODE44B
        *imagePtr++ = 0xde;
        *imagePtr++ = 0xaa;
        *imagePtr++ = 0xeb;

        // Write gap two, which contains six self-sync bytes
        for (int loop = 0; loop < 6; loop++)
            *imagePtr++ = 0xff;

        // Write the data field, which contains:
        //   - prologue (d5aaad)
        //   - 343 6-bit bytes of nibblized data, including a 6-bit checksum
        //   - epilogue (deaaeb)
        *imagePtr++ = 0xd5;
        *imagePtr++ = 0xaa;
        *imagePtr++ = 0xad;
        memcpy(imagePtr, Code62(s_sectorNumber[dosOrder][sector]), 343);
        imagePtr += 343;
        *imagePtr++ = 0xde;
        *imagePtr++ = 0xaa;
        *imagePtr++ = 0xeb;

        // Write gap three, which contains 27 self-sync bytes
        for (int loop = 0; loop < 27; loop++)
            *imagePtr++ = 0xff;
    }
    return (int)(imagePtr - trackImageBuffer);
}

//===========================================================================
static void SkewTrack(int track, int nibbles, uint8_t * trackImageBuffer) {
    int skewBytes = (track * 768) % nibbles;
    memcpy(s_workBuffer, trackImageBuffer, nibbles);
    memcpy(trackImageBuffer, s_workBuffer + skewBytes, nibbles - skewBytes);
    memcpy(trackImageBuffer + nibbles - skewBytes, s_workBuffer, skewBytes);
}


/****************************************************************************
*
*  Raw Apple program image (APL)
*
***/

//===========================================================================
static bool AplBoot(Image * ptr) {
    fseek(ptr->file, 0, SEEK_SET);

    uint16_t addr16 = 0;
    if (fread(&addr16, sizeof(addr16), 1, ptr->file) != 1)
        return false;
    uint32_t address = addr16;

    uint16_t length16 = 0;
    if (fread(&length16, sizeof(length16), 1, ptr->file) != 1)
        return false;
    uint32_t length = length16;

    if (address >= 0xc000 || address + length - 1 >= 0xc000)
        return false;

    uint8_t * buf = new uint8_t[length];
    if (fread(buf, length, 1, ptr->file) != 1) {
        delete[] buf;
        return false;
    }

    uint32_t a = address;
    for (int i = 0; i < (int)length; ++i, ++a)
        g_pageRead[a >> 8][a & 0xff] = buf[i];
    delete[] buf;

    regs.pc = addr16;
    return true;
}

//===========================================================================
static EImageMatch AplDetect(uint8_t * imagePtr, int imageSize) {
    int length = *(uint16_t *)(imagePtr + 2);
    if (((length + 4) == imageSize) ||
        ((length + 4 + ((256 - ((length + 4) & 255)) & 255)) == imageSize))
    {
        return IMAGEMATCH_MAYBE;
    }
    return IMAGEMATCH_FAIL;
}


/****************************************************************************
*
*  DOS order (DO)
*
***/

//===========================================================================
static EImageMatch DoDetect(uint8_t * imagePtr, int imageSize) {
    if ((imageSize < 143105 || imageSize > 143364) && imageSize != 143403 && imageSize != 143488)
        return IMAGEMATCH_FAIL;

    // Check for a DOS disk
    {
        bool mismatch = false;
        for (int loop = 1; loop < 16 && !mismatch; ++loop) {
            if (imagePtr[0x11002 + 0x100 * loop] != loop - 1)
                mismatch = true;
        }
        if (!mismatch)
            return IMAGEMATCH_FOUND;
    }

    // Check for a ProDOS disk
    {
        bool mismatch = false;
        for (int loop = 2; loop < 6 && !mismatch; ++loop) {
            if (*(uint16_t *)(imagePtr + 0x200 * (uintptr_t)loop + 0x100) != (loop == 5 ? 0 : 6 - loop) ||
                *(uint16_t *)(imagePtr + 0x200 * (uintptr_t)loop + 0x102) != (loop == 2 ? 0 : 8 - loop))
            {
                mismatch = true;
            }
        }
        if (!mismatch)
            return IMAGEMATCH_FOUND;
    }

    return IMAGEMATCH_MAYBE;
}

//===========================================================================
static void DoRead(Image * ptr, int track, int quarterTrack, uint8_t * trackImageBuffer, int * nibbles) {
    fseek(ptr->file, ptr->offset + (track << 12), SEEK_SET);

    memset(s_workBuffer, 0, 0x1000);
    if (fread(s_workBuffer, 0x1000, 1, ptr->file) != 1)
        return;

    *nibbles = NibblizeTrack(trackImageBuffer, 1, track);
    if (!g_optEnhancedDisk)
        SkewTrack(track, *nibbles, trackImageBuffer);
}

//===========================================================================
static void DoWrite(Image * ptr, int track, int quarterTrack, uint8_t * trackImage, int nibbles) {
    DenibblizeTrack(trackImage, 1, nibbles);
    fseek(ptr->file, ptr->offset + (track << 12), SEEK_SET);
    fwrite(s_workBuffer, 0x1000, 1, ptr->file);
}


/****************************************************************************
*
*  Nibblized 6656-Nibble (NIB)
*
***/

//===========================================================================
static EImageMatch Nib1Detect(uint8_t * imagePtr, int imageSize) {
    return (imageSize == 232960) ? IMAGEMATCH_FOUND : IMAGEMATCH_FAIL;
}

//===========================================================================
static void Nib1Read(Image * ptr, int track, int quarterTrack, uint8_t * trackImageBuffer, int * nibbles) {
    fseek(ptr->file, ptr->offset + track * 6656, SEEK_SET);
    *nibbles = (int)fread(trackImageBuffer, 1, 6656, ptr->file);
}

//===========================================================================
static void Nib1Write(Image * ptr, int track, int quarterTrack, uint8_t * trackImage, int nibbles) {
    fseek(ptr->file, ptr->offset + track * 6656, SEEK_SET);
    fwrite(trackImage, nibbles, 1, ptr->file);
}


/****************************************************************************
*
*  Nibblized 6384-Nibble (NB2)
*
***/

//===========================================================================
static EImageMatch Nib2Detect(uint8_t * imagePtr, int imageSize) {
    return (imageSize == 223440) ? IMAGEMATCH_FOUND : IMAGEMATCH_FAIL;
}

//===========================================================================
static void Nib2Read(Image * ptr, int track, int quarterTrack, uint8_t * trackImageBuffer, int * nibbles) {
    fseek(ptr->file, ptr->offset + track * 6384, SEEK_SET);
    *nibbles = (int)fread(trackImageBuffer, 1, 6384, ptr->file);
}

//===========================================================================
static void Nib2Write(Image * ptr, int track, int quarterTrack, uint8_t * trackImage, int nibbles) {
    fseek(ptr->file, ptr->offset + track * 6384, SEEK_SET);
    fwrite(trackImage, nibbles, 1, ptr->file);
}


/****************************************************************************
*
*  ProDOS Order (PO)
*
***/

//===========================================================================
static EImageMatch PoDetect(uint8_t * imagePtr, int imageSize) {
    if ((imageSize < 143105 || imageSize > 143364) && imageSize != 143488)
        return IMAGEMATCH_FAIL;

    // Check for a ProDOS order image of a DOS disk
    {
        bool mismatch = false;
        for (int loop = 5; loop < 14 && !mismatch; ++loop) {
            if (imagePtr[0x11002 + 0x100 * loop] != 14 - loop)
                mismatch = true;
        }
        if (!mismatch)
            return IMAGEMATCH_FOUND;
    }

    // Check for a ProDOS order image of a ProDOS disk
    {
        bool mismatch = false;
        for (int loop = 2; loop < 6 && !mismatch; ++loop) {
            if (*(uint16_t *)(imagePtr + 0x200 * (uintptr_t)loop + 0) != (loop == 2 ? 0 : loop - 1) ||
                *(uint16_t *)(imagePtr + 0x200 * (uintptr_t)loop + 2) != (loop == 5 ? 0 : loop + 1))
            {
                mismatch = true;
            }
        }
        if (!mismatch)
            return IMAGEMATCH_FOUND;
    }

    return IMAGEMATCH_MAYBE;
}

//===========================================================================
static void PoRead(Image * ptr, int track, int quarterTrack, uint8_t * trackImageBuffer, int * nibbles) {
    fseek(ptr->file, ptr->offset + (track << 12), SEEK_SET);
    memset(s_workBuffer, 0, 0x1000);
    if (fread(s_workBuffer, 0x1000, 1, ptr->file) != 1)
        return;
    *nibbles = NibblizeTrack(trackImageBuffer, 0, track);
    if (!g_optEnhancedDisk)
        SkewTrack(track, *nibbles, trackImageBuffer);
}

//===========================================================================
static void PoWrite(Image * ptr, int track, int quarterTrack, uint8_t * trackImage, int nibbles) {
    DenibblizeTrack(trackImage, 0, nibbles);
    fseek(ptr->file, ptr->offset + (track << 12), SEEK_SET);
    fwrite(s_workBuffer, 0x1000, 1, ptr->file);
}


/****************************************************************************
*
*  ProDOS program image (PRG)
*
***/

//===========================================================================
static bool PrgBoot(Image * ptr) {
    fseek(ptr->file, 5, SEEK_SET);

    uint16_t address = 0;
    if (fread(&address, sizeof(uint16_t), 1, ptr->file) != 1)
        return false;

    uint16_t length = 0;
    if (fread(&length, sizeof(uint16_t), 1, ptr->file) != 1)
        return false;

    length <<= 1;
    if (address + length <= address || address >= 0xc000 || address + length - 1 >= 0xc000)
        return false;

    fseek(ptr->file, 128, SEEK_SET);
    uint8_t * buf = new uint8_t[length];
    if (fread(buf, length, 1, ptr->file) != 1) {
        delete[] buf;
        return false;
    }

    uint16_t a = address;
    for (int i = 0; i < length; ++i, ++a)
        g_pageRead[a >> 8][a & 0xff] = buf[i];
    delete[] buf;

    regs.pc = address;
    return true;
}

//===========================================================================
static EImageMatch PrgDetect(uint8_t * imagePtr, int imageSize) {
    return (*(uint32_t *)imagePtr == 0x214c470a) ? IMAGEMATCH_FOUND : IMAGEMATCH_FAIL;
}


//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
bool ImageBoot(Image * image) {
    bool result = false;
    if (s_imageFormatData[image->format].boot)
        result = s_imageFormatData[image->format].boot(image);
    if (result)
        image->writeProtected = true;
    return result;
}

//===========================================================================
void ImageClose(Image * image) {
    if (image->file)
        fclose(image->file);
    delete image;
}

//===========================================================================
void ImageDestroy() {
    delete[] s_workBuffer;
    s_workBuffer = nullptr;
}

//===========================================================================
void ImageInitialize() {
    s_workBuffer = new uint8_t[0x2000];
    memset(s_workBuffer, 0, 0x2000);
}

//===========================================================================
bool ImageOpen(
    const char *    imageFilename,
    Image **        image,
    bool *          writeProtected
) {
    if (!imageFilename || !image || !writeProtected || !s_workBuffer)
        return false;

    *image = nullptr;
    *writeProtected = false;

    // Try to open the image file
    bool readonly = false;
    FILE * file = fopen(imageFilename, "rb+");
    if (!file) {
        readonly = true;
        file = fopen(imageFilename, "rb");
        if (!file)
            return false;
    }

    // If we are able to open the file, map it into memory for use by the
    // detection functions
    fseek(file, 0, SEEK_END);
    size_t size = (size_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    uint8_t * buf = new uint8_t[size];
    if (fread(buf, size, 1, file) != 1) {
        fclose(file);
        return false;
    }

    uint8_t * imagePtr = buf;
    EImageFormat format = IMAGEFORMAT_UNKNOWN;
    if (imagePtr) {

        // Strip off MacBinary header if it has one
        if ((size > 128) &&
            (imagePtr[0] == 0) &&
            (imagePtr[1] < 120 && imagePtr[imagePtr[1] + 2] == 0) &&
            (imagePtr[0x7a] == 0x81 && imagePtr[0x7b] == 0x81))
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
        EImageFormat possibleFormat = IMAGEFORMAT_UNKNOWN;
        EImageFormat currFormat = (EImageFormat)0;
        while (currFormat < IMAGEFORMAT_UNKNOWN) {
            bool reject = false;
            if (ext && ext[0] != '\0') {
                const char * const * excludedExts = s_imageFormatData[currFormat].excludedExts;
                for (int f = 0; f < 4 && excludedExts[f] != nullptr; ++f) {
                    if (!StrCmpI(ext, excludedExts[f])) {
                        reject = true;
                        break;
                    }
                }
            }
            if (!reject) {
                EImageMatch result = s_imageFormatData[currFormat].detect(imagePtr, (int)size);
                if (result == IMAGEMATCH_FOUND) {
                    format = currFormat;
                    break;
                }
                else if ((result == IMAGEMATCH_MAYBE) && (possibleFormat == IMAGEFORMAT_UNKNOWN))
                    possibleFormat = currFormat;
            }
            currFormat = (EImageFormat)(currFormat + 1);
        }
        if (currFormat == IMAGEFORMAT_UNKNOWN)
            format = possibleFormat;
    }

    int offset = (int)(imagePtr - buf);
    delete[] buf;

    // If the file matches a known format, create a record for the file, and return an image handle
    if (format != IMAGEFORMAT_UNKNOWN) {
        Image * ptr = new Image;
        if (ptr) {
            memset(ptr, 0, sizeof(Image));
            ptr->format         = format;
            ptr->file           = file;
            ptr->offset         = offset;
            ptr->writeProtected = readonly;
            if (image)
                *image = (Image *)ptr;
            if (writeProtected)
                *writeProtected = readonly;
            return true;
        }
    }

    fclose(file);
    return false;
}

//===========================================================================
void ImageReadTrack(
    Image *     image,
    int         track,
    int         quarterTrack,
    uint8_t *   trackImageBuffer,
    int *       nibbles
) {
    *nibbles = 0;
    if (s_imageFormatData[image->format].read)
        s_imageFormatData[image->format].read(image, track, quarterTrack, trackImageBuffer, nibbles);
}

//===========================================================================
void ImageWriteTrack(
    Image *     image,
    int         track,
    int         quarterTrack,
    uint8_t *   trackImage,
    int         nibbles
) {
    if (s_imageFormatData[image->format].write && !image->writeProtected)
        s_imageFormatData[image->format].write(image, track, quarterTrack, trackImage, nibbles);
}
