/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

constexpr int TRACKS = 35;

class BaseImage;

enum EImageMatch {
    IMAGEMATCH_FAIL  = 0,
    IMAGEMATCH_MAYBE = 1,
    IMAGEMATCH_FOUND = 2,
};

enum EImageFormat {
    IMAGEFORMAT_DSK,
    IMAGEFORMAT_PO,
    IMAGEFORMAT_NIB,
    IMAGEFORMATS,

    IMAGEFORMAT_UNKNOWN = IMAGEFORMATS
};

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
    ProDOS:
        logical order  0 E D C B A 9 8 7 6 5 4 3 2 1 F
        physical order 0 2 4 6 8 A C E 1 3 5 7 9 B D F
    DOS:
        logical order  0 1 2 3 4 5 6 7 8 9 A B C D E F
        physical order 0 D B 9 7 5 3 1 E C A 8 6 4 2 F
*/

static uint8_t s_sectorNumber[3][16] = {
    { 0x00, 0x08, 0x01, 0x09, 0x02, 0x0a, 0x03, 0x0b, 0x04, 0x0c, 0x05, 0x0d, 0x06, 0x0e, 0x07, 0x0f }, // ProDOS
    { 0x00, 0x07, 0x0e, 0x06, 0x0d, 0x05, 0x0c, 0x04, 0x0b, 0x03, 0x0a, 0x02, 0x09, 0x01, 0x08, 0x0f }, // DOS
};

static uint8_t * s_workBuffer = nullptr;


/****************************************************************************
*
*  Helper functions
*
***/

//===========================================================================
static uint8_t * Code62(int sector) {

    // Convert the 256 8-bit bytes into 342 6-bit bytes, which we store
    // starting 4k bytes into the work buffer.
    {
        uint8_t * sectorBase = s_workBuffer + ((uintptr_t)sector << 8);
        uint8_t * resultPtr  = s_workBuffer + 0x1000;
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
static void Decode62(uint8_t * imageData) {

    // If we haven't already done so, generate a table for converting
    // disk bytes back into 6-bit bytes.
    static bool s_tableGenerated = false;
    static uint8_t s_sixBitByte[0x80];
    if (!s_tableGenerated) {
        memset(s_sixBitByte, 0, ARRSIZE(s_sixBitByte));
        for (int loop = 0; loop < 0x40; ++loop)
            s_sixBitByte[s_diskByte[loop] - 0x80] = loop << 2;
        s_tableGenerated = true;
    }

    // Using our table, convert the disk bytes back into 6-bit bytes.
    {
        uint8_t * sourcePtr = s_workBuffer + 0x1000;
        uint8_t * resultPtr = s_workBuffer + 0x1400;
        for (int loop = 343; loop--; )
            *resultPtr++ = s_sixBitByte[*sourcePtr++ & 0x7f];
    }

    // Exclusive-or the entire data block with itself offset by one byte
    // to undo the effects of the checksumming process.
    {
        uint8_t   savedVal = 0;
        uint8_t * sourcePtr = s_workBuffer + 0x1400;
        uint8_t * resultPtr = s_workBuffer + 0x1000;
        for (int loop = 342; loop--; ) {
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
                imageData[offset] = (sectorBase[offset] & 0xfc)
                    | ((*lowBitsPtr & 0x80) >> 7)
                    | ((*lowBitsPtr & 0x40) >> 5);
            }
            offset -= 0x56;
            imageData[offset] = (sectorBase[offset] & 0xfc)
                | ((*lowBitsPtr & 0x20) >> 5)
                | ((*lowBitsPtr & 0x10) >> 3);
            offset -= 0x56;
            imageData[offset] = (sectorBase[offset] & 0xfc)
                | ((*lowBitsPtr & 0x08) >> 3)
                | ((*lowBitsPtr & 0x04) >> 1);
            offset -= 0x53;
            lowBitsPtr++;
        }
    }
}

//===========================================================================
static void DenibblizeTrack(const uint8_t * trackBuffer, int dosOrder, int nibbles) {
    memset(s_workBuffer, 0, 0x1000);

    int offset      = 0;
    int sector      = -1;
    int firstSector = -1;
    for (int scan = 0; scan < 64; ++scan) {

        // Search for a D5 XX XX sequence.
        uint8_t sequence[3] = { 0xd5, 0x00, 0x00 };
        for (int i = 0, count = 0; i < nibbles && count < 3; i++) {
            if (count > 0)
                sequence[count++] = trackBuffer[offset];
            else if (trackBuffer[offset] == 0xd5)
                count = 1;
            offset = (offset + 1) % nibbles;
        }

        if (sequence[1] == 0xaa) {

            // D5 AA 96 indicates a sector header.
            if (sequence[2] == 0x96) {
                for (int i = 0, tmpOffset = offset; i < 6; ++i) {
                    s_workBuffer[0x1000 + i] = trackBuffer[tmpOffset];
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
                    s_workBuffer[0x1000 + i] = trackBuffer[tmpOffset];
                    tmpOffset = (tmpOffset + 1) % nibbles;
                }
                Decode62(s_workBuffer + ((uintptr_t)s_sectorNumber[dosOrder][sector] << 8));
                sector = -1;
            }
        }
    }
}

//===========================================================================
static int NibblizeTrack(uint8_t * trackBuffer, int dosOrder, int track) {
    memset(s_workBuffer + 0x1000, 0, 0x1000);
    uint8_t * imageData = trackBuffer;

    // Write gap one, which contains 48 self-sync bytes
    for (int loop = 0; loop < 48; loop++)
        *imageData++ = 0xff;

    for (uint8_t sector = 0; sector < 16; ++sector) {
        // Write the address field, which contains:
        //   - prologue (d5aa96)
        //   - volume number (4-and-4 encoded)
        //   - track number (4-and-4 encoded)
        //   - sector number (4-and-4 encoded)
        //   - checksum (4-and-4 encoded)
        //   - epilogue (deaaeb)
        *imageData++ = 0xd5;
        *imageData++ = 0xaa;
        *imageData++ = 0x96;
        *imageData++ = 0xff;
        *imageData++ = 0xfe;
#define CODE44A(a) ((((a) >> 1) & 0x55) | 0xaa)
#define CODE44B(a) (((a) & 0x55) | 0xaa)
        *imageData++ = CODE44A((uint8_t)track);
        *imageData++ = CODE44B((uint8_t)track);
        *imageData++ = CODE44A(sector);
        *imageData++ = CODE44B(sector);
        *imageData++ = CODE44A(0xfe ^ ((uint8_t)track) ^ sector);
        *imageData++ = CODE44B(0xfe ^ ((uint8_t)track) ^ sector);
#undef CODE44A
#undef CODE44B
        *imageData++ = 0xde;
        *imageData++ = 0xaa;
        *imageData++ = 0xeb;

        // Write gap two, which contains six self-sync bytes
        for (int loop = 0; loop < 6; loop++)
            *imageData++ = 0xff;

        // Write the data field, which contains:
        //   - prologue (d5aaad)
        //   - 343 6-bit bytes of nibblized data, including a 6-bit checksum
        //   - epilogue (deaaeb)
        *imageData++ = 0xd5;
        *imageData++ = 0xaa;
        *imageData++ = 0xad;
        memcpy(imageData, Code62(s_sectorNumber[dosOrder][sector]), 343);
        imageData += 343;
        *imageData++ = 0xde;
        *imageData++ = 0xaa;
        *imageData++ = 0xeb;

        // Write gap three, which contains 27 self-sync bytes
        for (int loop = 0; loop < 27; loop++)
            *imageData++ = 0xff;
    }
    return (int)(imageData - trackBuffer);
}

//===========================================================================
static void SkewTrack(int track, int nibbles, uint8_t * trackBuffer) {
    int skewBytes = (track * 768) % nibbles;
    memcpy(s_workBuffer, trackBuffer, nibbles);
    memcpy(trackBuffer, s_workBuffer + skewBytes, nibbles - skewBytes);
    memcpy(trackBuffer + nibbles - skewBytes, s_workBuffer, skewBytes);
}

//===========================================================================
static void GetImageName(const char * filename, char * imageName, size_t imageNameChars) {
    const char * startPos = filename;
    {
        const char * ptr;
        while ((ptr = StrChrConst(startPos, '\\')) != nullptr)
            startPos = ptr + 1;
    }

    // Strip the directory and extension.
    char imageTitle[128];
    StrCopy(imageTitle, startPos, ARRSIZE(imageTitle));
    if (imageTitle[0]) {
        char * dot = imageTitle;
        char * ptr;
        while ((ptr = StrChr(dot + 1, '.')) != nullptr)
            dot = ptr;
        if (dot > imageTitle)
            *dot = '\0';
    }

    // Convert all-caps disk names to mixed case.
    bool isAllCaps = true;
    int indexFirstLowercase = 0;
    for (; imageTitle[indexFirstLowercase]; ++indexFirstLowercase) {
        if (imageTitle[indexFirstLowercase] >= 'a' && imageTitle[indexFirstLowercase] <= 'z') {
            isAllCaps = false;
            break;
        }
    }
    if (isAllCaps && indexFirstLowercase > 1) {
        for (int i = 1; imageTitle[i] != '\0'; ++i)
            imageTitle[i] = (imageTitle[i] >= 'A' && imageTitle[i] <= 'Z')
                ? imageTitle[i] - 'A' + 'a'
                : imageTitle[i];
    }

    StrCopy(imageName, imageTitle, imageNameChars);
}


/****************************************************************************
*
*  Image factory
*
***/

class ImageFactory {
public:
    virtual BaseImage * Create(const char * name, FILE * file, int offset, bool writeProtected) = 0;
    virtual EImageMatch Detect(uint8_t * imageData, int imageSize) = 0;
};


/****************************************************************************
*
*  Base image
*
***/

class BaseImage : public Image {
protected:
    char       m_name[64];
    FILE *     m_file;
    int        m_offset;
    bool       m_writeProtected;

public:
    BaseImage(const char * name, FILE * file, int offset, bool writeProtected);
    virtual ~BaseImage() { }

    void Close();
    const char * Name() const override { return m_name; }
};

//===========================================================================
BaseImage::BaseImage(const char * name, FILE * file, int offset, bool writeProtected)
    : m_file(file), m_offset(offset), m_writeProtected(writeProtected)
{
    StrCopy(m_name, name, ARRSIZE(m_name));
}

//===========================================================================
void BaseImage::Close() {
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
}


/****************************************************************************
*
*  DOS-order image (*.dsk, *.do)
*
***/

class DosOrderImage : public BaseImage {
public:
    DosOrderImage(const char * name, FILE * file, int offset, bool writeProtected);

    void ReadTrack(int quarterTrack, uint8_t * buffer, int * nibblesRead) override;
    void WriteTrack(int quarterTrack, const uint8_t * buffer, int nibbles) override;

    class Factory : public ImageFactory {
    public:
        BaseImage * Create(const char * name, FILE * file, int offset, bool writeProtected) override;
        EImageMatch Detect(uint8_t * imageData, int imageSize) override;
    };
};

//===========================================================================
DosOrderImage::DosOrderImage(const char * name, FILE * file, int offset, bool writeProtected)
    : BaseImage(name, file, offset, writeProtected)
{
}

//===========================================================================
void DosOrderImage::ReadTrack(int quarterTrack, uint8_t * buffer, int * nibblesRead) {
    int track = quarterTrack / 4;
    if (track >= TRACKS) {
        *nibblesRead = 0;
        return;
    }

    fseek(m_file, m_offset + (track << 12), SEEK_SET);

    memset(s_workBuffer, 0, 0x1000);
    if (fread(s_workBuffer, 0x1000, 1, m_file) != 1)
        return;

    *nibblesRead = NibblizeTrack(buffer, 1, track);
    if (!g_optEnhancedDisk)
        SkewTrack(track, *nibblesRead, buffer);
}

//===========================================================================
void DosOrderImage::WriteTrack(int quarterTrack, const uint8_t * buffer, int nibbles) {
    int track = quarterTrack / 4;
    if (track >= TRACKS)
        return;

    DenibblizeTrack(buffer, 1, nibbles);
    fseek(m_file, m_offset + (track << 12), SEEK_SET);
    fwrite(s_workBuffer, 0x1000, 1, m_file);
}

//===========================================================================
BaseImage * DosOrderImage::Factory::Create(const char * name, FILE * file, int offset, bool writeProtected) {
    return new DosOrderImage(name, file, offset, writeProtected);
}

//===========================================================================
EImageMatch DosOrderImage::Factory::Detect(uint8_t * imageData, int imageSize) {
    if ((imageSize < 143105 || imageSize > 143364) && imageSize != 143403 && imageSize != 143488)
        return IMAGEMATCH_FAIL;

    // Check for a DOS image.
    {
        bool mismatch = false;
        for (int loop = 1; loop < 16 && !mismatch; ++loop) {
            if (imageData[0x11002 + 0x100 * loop] != loop - 1)
                mismatch = true;
        }
        if (!mismatch)
            return IMAGEMATCH_FOUND;
    }

    // Check for a ProDOS image.
    {
        bool mismatch = false;
        for (int loop = 2; loop < 6 && !mismatch; ++loop) {
            if (*(uint16_t *)(imageData + 0x200 * (uintptr_t)loop + 0x100) != (loop == 5 ? 0 : 6 - loop) ||
                *(uint16_t *)(imageData + 0x200 * (uintptr_t)loop + 0x102) != (loop == 2 ? 0 : 8 - loop))
            {
                mismatch = true;
            }
        }
        if (!mismatch)
            return IMAGEMATCH_FOUND;
    }

    return IMAGEMATCH_MAYBE;
}


/****************************************************************************
*
*   ProDOS-order image (*.po)
*
***/

class ProDosOrderImage : public BaseImage {
public:
    ProDosOrderImage(const char * name, FILE * file, int offset, bool writeProtected);

    void ReadTrack(int quarterTrack, uint8_t * buffer, int * nibblesRead) override;
    void WriteTrack(int quarterTrack, const uint8_t * buffer, int nibbles) override;

    class Factory : public ImageFactory {
    public:
        BaseImage * Create(const char * name, FILE * file, int offset, bool writeProtected) override;
        EImageMatch Detect(uint8_t * imageData, int imageSize) override;
    };
};

//===========================================================================
ProDosOrderImage::ProDosOrderImage(const char * name, FILE * file, int offset, bool writeProtected)
    : BaseImage(name, file, offset, writeProtected)
{
}

//===========================================================================
void ProDosOrderImage::ReadTrack(int quarterTrack, uint8_t * buffer, int * nibblesRead) {
    int track = quarterTrack / 4;
    if (track >= TRACKS) {
        *nibblesRead = 0;
        return;
    }

    fseek(m_file, m_offset + (track << 12), SEEK_SET);
    memset(s_workBuffer, 0, 0x1000);
    if (fread(s_workBuffer, 0x1000, 1, m_file) != 1)
        return;
    *nibblesRead = NibblizeTrack(buffer, 0, track);
    if (!g_optEnhancedDisk)
        SkewTrack(track, *nibblesRead, buffer);
}

//===========================================================================
void ProDosOrderImage::WriteTrack(int quarterTrack, const uint8_t * buffer, int nibbles) {
    int track = quarterTrack / 4;
    if (track >= TRACKS)
        return;

    DenibblizeTrack(buffer, 0, nibbles);
    fseek(m_file, m_offset + (track << 12), SEEK_SET);
    fwrite(s_workBuffer, 0x1000, 1, m_file);
}

//===========================================================================
BaseImage * ProDosOrderImage::Factory::Create(const char * name, FILE * file, int offset, bool writeProtected) {
    return new ProDosOrderImage(name, file, offset, writeProtected);
}

//===========================================================================
EImageMatch ProDosOrderImage::Factory::Detect(uint8_t * imageData, int imageSize) {
    if ((imageSize < 143105 || imageSize > 143364) && imageSize != 143488)
        return IMAGEMATCH_FAIL;

    // Check for a ProDOS image.
    {
        bool mismatch = false;
        for (int loop = 2; loop < 6 && !mismatch; ++loop) {
            if (*(uint16_t *)(imageData + 0x200 * (uintptr_t)loop + 0) != (loop == 2 ? 0 : loop - 1) ||
                *(uint16_t *)(imageData + 0x200 * (uintptr_t)loop + 2) != (loop == 5 ? 0 : loop + 1))
            {
                mismatch = true;
            }
        }
        if (!mismatch)
            return IMAGEMATCH_FOUND;
    }

    // Check for a DOS image stored in a ProDOS-order image.
    {
        bool mismatch = false;
        for (int loop = 5; loop < 14 && !mismatch; ++loop) {
            if (imageData[0x11002 + 0x100 * loop] != 14 - loop)
                mismatch = true;
        }
        if (!mismatch)
            return IMAGEMATCH_FOUND;
    }

    return IMAGEMATCH_MAYBE;
}


/****************************************************************************
*
*   Nibble image (*.nib)
*
***/

class NibbleImage : public BaseImage {
public:
    NibbleImage(const char * name, FILE * file, int offset, bool writeProtected);

    void ReadTrack(int quarterTrack, uint8_t * buffer, int * nibblesRead) override;
    void WriteTrack(int quarterTrack, const uint8_t * buffer, int nibbles) override;

    class Factory : public ImageFactory {
    public:
        BaseImage * Create(const char * name, FILE * file, int offset, bool writeProtected) override;
        EImageMatch Detect(uint8_t * imageData, int imageSize) override;
    };
};

//===========================================================================
NibbleImage::NibbleImage(const char * name, FILE * file, int offset, bool writeProtected)
    : BaseImage(name, file, offset, writeProtected)
{
}

//===========================================================================
void NibbleImage::ReadTrack(int quarterTrack, uint8_t * buffer, int * nibblesRead) {
    int track = quarterTrack / 4;
    if (track >= TRACKS) {
        *nibblesRead = 0;
        return;
    }

    fseek(m_file, m_offset + track * 6656, SEEK_SET);
    *nibblesRead = (int)fread(buffer, 1, 6656, m_file);
}

//===========================================================================
void NibbleImage::WriteTrack(int quarterTrack, const uint8_t * buffer, int nibbles) {
    int track = quarterTrack / 4;
    if (track >= TRACKS)
        return;

    fseek(m_file, m_offset + track * 6656, SEEK_SET);
    fwrite(buffer, nibbles, 1, m_file);
}

//===========================================================================
BaseImage * NibbleImage::Factory::Create(const char * name, FILE * file, int offset, bool writeProtected) {
    return new NibbleImage(name, file, offset, writeProtected);
}

//===========================================================================
EImageMatch NibbleImage::Factory::Detect(uint8_t * imageData, int imageSize) {
    return (imageSize == 232960) ? IMAGEMATCH_FOUND : IMAGEMATCH_FAIL;
}


/****************************************************************************
*
*   Image factory table
*
***/

static ImageFactory * s_imageFactory[] = {
    new DosOrderImage::Factory(),
    new ProDosOrderImage::Factory(),
    new NibbleImage::Factory(),
};

static_assert(ARRSIZE(s_imageFactory) == IMAGEFORMATS, "image factory table mismatch");


/****************************************************************************
*
*   Public functions
*
***/

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
Image * ImageOpen(const char * imageFilename, bool * writeProtected) {
    if (!imageFilename || !writeProtected)
        return nullptr;

    *writeProtected = false;

    // Try to open the image file.
    bool readonly = false;
    FILE * file = fopen(imageFilename, "rb+");
    if (!file) {
        file = fopen(imageFilename, "rb");
        if (!file)
            return nullptr;
        readonly = true;
    }

    // Read the image file into memory.
    fseek(file, 0, SEEK_END);
    size_t size = (size_t)ftell(file);
    fseek(file, 0, SEEK_SET);
    uint8_t * buf = new uint8_t[size];
    if (fread(buf, size, 1, file) != 1) {
        fclose(file);
        return nullptr;
    }

    // Strip off MacBinary header.
    uint8_t * imageData = buf;
    if ((size > 128) &&
        (imageData[0] == 0) &&
        (imageData[1] < 120 && imageData[imageData[1] + 2] == 0) &&
        (imageData[0x7a] == 0x81 && imageData[0x7b] == 0x81))
    {
        imageData += 128;
        size -= 128;
    }

    // Check all known image formats for possible matches.
    ImageFactory * factory  = nullptr, * possibleFactory = nullptr;
    for (int formatIndex = 0; formatIndex < IMAGEFORMATS; ++formatIndex) {
        ImageFactory * currFactory = s_imageFactory[formatIndex];
        EImageMatch match = currFactory->Detect(imageData, (int)size);
        if (match == IMAGEMATCH_FOUND) {
            factory = currFactory;
            break;
        }
        if (match == IMAGEMATCH_MAYBE && possibleFactory == nullptr)
            possibleFactory = currFactory;
    }
    if (factory == nullptr)
        factory = possibleFactory;

    // Release image data from memory.
    int offset = (int)(imageData - buf);
    delete[] buf;

    if (!factory) {
        fclose(file);
        return nullptr;
    }

    // Create the image.
    char name[64];
    GetImageName(imageFilename, name, ARRSIZE(name));
    *writeProtected = readonly;
    return factory->Create(name, file, offset, readonly);
}

//===========================================================================
void ImageClose(Image ** image) {
    BaseImage * baseImage = static_cast<BaseImage *>(*image);
    baseImage->Close();
    delete baseImage;
    *image = nullptr;
}
