// Released under the MIT License. See LICENSE for details.

#include "ballistica/base/graphics/texture/ktx.h"

#include "ballistica/core/core.h"
#include "ballistica/core/platform/core_platform.h"
#include "ballistica/shared/foundation/exception.h"

namespace ballistica::base {

// We don't want this to be dependent on GL so
// lets just define the few bits we need here
#ifndef GL_COMPRESSED_RGB8_ETC2
#define GL_COMPRESSED_RGB8_ETC2 0x9274
#endif
#ifndef GL_COMPRESSED_RGBA8_ETC2_EAC
#define GL_COMPRESSED_RGBA8_ETC2_EAC 0x9278
#endif
#ifndef GL_ETC1_RGB8_OES
#define GL_ETC1_RGB8_OES 0x8D64
#endif
#ifndef GL_RGB
#define GL_RGB 0x1907
#endif
#ifndef GL_RGBA
#define GL_RGBA 0x1908
#endif
#ifndef GL_UNSIGNED_BYTE
#define GL_UNSIGNED_BYTE 0x1401
#endif
typedef uint8_t GLubyte;
typedef unsigned int GLenum;
typedef int GLint;

#define KTX_IDENTIFIER_REF \
  {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A}
#define KTX_ENDIAN_REF (0x04030201)
#define KTX_ENDIAN_REF_REV (0x01020304)
#define KTX_HEADER_SIZE (64)

typedef struct KTX_header_t {
  uint8_t identifier[12];
  uint32_t endianness;
  uint32_t glType;
  uint32_t glTypeSize;
  uint32_t glFormat;
  uint32_t glInternalFormat;
  uint32_t glBaseInternalFormat;
  uint32_t pixelWidth;
  uint32_t pixelHeight;
  uint32_t pixelDepth;
  uint32_t numberOfArrayElements;
  uint32_t numberOfFaces;
  uint32_t numberOfMipmapLevels;
  uint32_t bytesOfKeyValueData;
} KTX_header;

void LoadKTX(const std::string& file_name, unsigned char** buffers, int* widths,
             int* heights, TextureFormat* formats, size_t* sizes,
             TextureQuality texture_quality, int min_quality, int* base_level) {
  FILE* f = g_core->platform->FOpen(file_name.c_str(), "rb");
  if (!f) throw Exception("can't open file: \"" + file_name + "\"");

  KTX_header_t header{};
  static_assert(sizeof(header) == KTX_HEADER_SIZE);
  BA_PRECONDITION(fread(&header, sizeof(header), 1, f) == 1);

  // Make some assumptions; we don't support arrays, more than 1 face, or kv
  // data of any form.
  BA_PRECONDITION(header.endianness == KTX_ENDIAN_REF);
  BA_PRECONDITION(header.numberOfArrayElements == 0);
  BA_PRECONDITION(header.numberOfFaces == 1);
  BA_PRECONDITION(header.bytesOfKeyValueData == 0);
  BA_PRECONDITION(header.pixelWidth > 0 && header.pixelHeight > 0
                  && header.pixelDepth == 0);

  TextureFormat internal_format;

  if (header.glInternalFormat == GL_COMPRESSED_RGB8_ETC2) {
    internal_format = TextureFormat::kETC2_RGB;
    // NOLINTNEXTLINE(bugprone-branch-clone)
  } else if (header.glInternalFormat == GL_COMPRESSED_RGBA8_ETC2_EAC) {
    internal_format = TextureFormat::kETC2_RGBA;
  } else if (header.glInternalFormat == GL_COMPRESSED_RGBA8_ETC2_EAC) {
    internal_format = TextureFormat::kETC2_RGBA;
  } else if (header.glInternalFormat == GL_ETC1_RGB8_OES) {
    internal_format = TextureFormat::kETC1;
  } else {
    throw Exception();
  }

  uint32_t size = 0;
  uint32_t sizeRounded = 0;

  int x = header.pixelWidth;
  int y = header.pixelHeight;

  (*base_level) = 0;

  // Try dropping a level for med/low quality.
  if ((texture_quality == TextureQuality::kLow
       || texture_quality == TextureQuality::kMedium)
      && (min_quality < 2)
      && static_cast_check_fit<int>(header.numberOfMipmapLevels)
             >= (*base_level) + 1) {
    (*base_level)++;
  }

  // And one more for low in some cases.
  if (texture_quality == TextureQuality::kLow && (min_quality < 1)
      && (header.pixelWidth > 128) && (header.pixelHeight > 128)
      && static_cast_check_fit<int>(header.numberOfMipmapLevels)
             >= (*base_level) + 1) {
    (*base_level)++;
  }

  for (uint32_t level = 0; level < header.numberOfMipmapLevels; ++level) {
    if (fread(&size, sizeof(size), 1, f) != 1)
      throw Exception("Error reading texture: '" + file_name + "'");
    sizeRounded = (size + 3) & ~(uint32_t)3;
    BA_PRECONDITION(
        size == sizeRounded);  // Not currently handling this. Is it necessary?

    if ((*base_level) <= static_cast_check_fit<int>(level)) {
      sizes[level] = size;
      buffers[level] = (unsigned char*)malloc(size);
      BA_PRECONDITION(buffers[level]);
      widths[level] = static_cast<uint32_t>(x);
      heights[level] = static_cast<uint32_t>(y);
      formats[level] = internal_format;
      BA_PRECONDITION(fread(buffers[level], size, 1, f) == 1);
    } else {
      buffers[level] = nullptr;
      BA_PRECONDITION(fseek(f, static_cast_check_fit<long>(size), SEEK_CUR)
                      == 0);
    }
    x = (x + 1) >> 1;
    y = (y + 1) >> 1;
  }
  fclose(f);
}

/**

   @~English
   @page licensing Licensing

   @section etcdec etcdec.cxx License

   etcdec.cxx is made available under the terms and conditions of the following
   License Agreement.

   Software License Agreement

   PLEASE REVIEW THE FOLLOWING TERMS AND CONDITIONS PRIOR TO USING THE
   ERICSSON TEXTURE COMPRESSION CODEC SOFTWARE (THE "SOFTWARE"). THE USE
   OF THE SOFTWARE IS SUBJECT TO THE TERMS AND CONDITIONS OF THE
   FOLLOWING SOFTWARE LICENSE AGREEMENT (THE "SLA"). IF YOU DO NOT ACCEPT
   SUCH TERMS AND CONDITIONS YOU MAY NOT USE THE SOFTWARE.

   Subject to the terms and conditions of the SLA, the licensee of the
   Software (the "Licensee") hereby, receives a non-exclusive,
   non-transferable, limited, free-of-charge, perpetual and worldwide
   license, to copy, use, distribute and modify the Software, but only
   for the purpose of developing, manufacturing, selling, using and
   distributing products including the Software in binary form, which
   products are used for compression and/or decompression according to
   the Khronos standard specifications OpenGL, OpenGL ES and
   WebGL. Notwithstanding anything of the above, Licensee may distribute
   [etcdec.cxx] in source code form provided (i) it is in unmodified
   form; and (ii) it is included in software owned by Licensee.

   If Licensee institutes, or threatens to institute, patent litigation
   against Ericsson or Ericsson's affiliates for using the Software for
   developing, having developed, manufacturing, having manufactured,
   selling, offer for sale, importing, using, leasing, operating,
   repairing and/or distributing products (i) within the scope of the
   Khronos framework; or (ii) using software or other intellectual
   property rights owned by Ericsson or its affiliates and provided under
   the Khronos framework, Ericsson shall have the right to terminate this
   SLA with immediate effect. Moreover, if Licensee institutes, or
   threatens to institute, patent litigation against any other licensee
   of the Software for using the Software in products within the scope of
   the Khronos framework, Ericsson shall have the right to terminate this
   SLA with immediate effect. However, should Licensee institute, or
   threaten to institute, patent litigation against any other licensee of
   the Software based on such other licensee's use of any other software
   together with the Software, then Ericsson shall have no right to
   terminate this SLA.

   This SLA does not transfer to Licensee any ownership to any Ericsson
   or third party intellectual property rights. All rights not expressly
   granted by Ericsson under this SLA are hereby expressly
   reserved. Furthermore, nothing in this SLA shall be construed as a
   right to use or sell products in a manner which conveys or purports to
   convey whether explicitly, by principles of implied license, or
   otherwise, any rights to any third party, under any patent of Ericsson
   or of Ericsson's affiliates covering or relating to any combination of
   the Software with any other software or product (not licensed
   hereunder) where the right applies specifically to the combination and
   not to the software or product itself.

   THE SOFTWARE IS PROVIDED "AS IS". ERICSSON MAKES NO REPRESENTATIONS OF
   ANY KIND, EXTENDS NO WARRANTIES OR CONDITIONS OF ANY KIND, EITHER
   EXPRESS, IMPLIED OR STATUTORY; INCLUDING, BUT NOT LIMITED TO, EXPRESS,
   IMPLIED OR STATUTORY WARRANTIES OR CONDITIONS OF TITLE,
   MERCHANTABILITY, SATISFACTORY QUALITY, SUITABILITY, AND FITNESS FOR A
   PARTICULAR PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE
   OF THE SOFTWARE IS WITH THE LICENSEE. SHOULD THE SOFTWARE PROVE
   DEFECTIVE, THE LICENSEE ASSUMES THE COST OF ALL NECESSARY SERVICING,
   REPAIR OR CORRECTION. ERICSSON MAKES NO WARRANTY THAT THE MANUFACTURE,
   SALE, OFFERING FOR SALE, DISTRIBUTION, LEASE, USE OR IMPORTATION UNDER
   THE SLA WILL BE FREE FROM INFRINGEMENT OF PATENTS, COPYRIGHTS OR OTHER
   INTELLECTUAL PROPERTY RIGHTS OF OTHERS, AND THE VALIDITY OF THE
   LICENSE AND THE SLA ARE SUBJECT TO LICENSEE'S SOLE RESPONSIBILITY TO
   MAKE SUCH DETERMINATION AND ACQUIRE SUCH LICENSES AS MAY BE NECESSARY
   WITH RESPECT TO PATENTS, COPYRIGHT AND OTHER INTELLECTUAL PROPERTY OF
   THIRD PARTIES.

   THE LICENSEE ACKNOWLEDGES AND ACCEPTS THAT THE SOFTWARE (I) IS NOT
   LICENSED FOR; (II) IS NOT DESIGNED FOR OR INTENDED FOR; AND (III) MAY
   NOT BE USED FOR; ANY MISSION CRITICAL APPLICATIONS SUCH AS, BUT NOT
   LIMITED TO OPERATION OF NUCLEAR OR HEALTHCARE COMPUTER SYSTEMS AND/OR
   NETWORKS, AIRCRAFT OR TRAIN CONTROL AND/OR COMMUNICATION SYSTEMS OR
   ANY OTHER COMPUTER SYSTEMS AND/OR NETWORKS OR CONTROL AND/OR
   COMMUNICATION SYSTEMS ALL IN WHICH CASE THE FAILURE OF THE SOFTWARE
   COULD LEAD TO DEATH, PERSONAL INJURY, OR SEVERE PHYSICAL, MATERIAL OR
   ENVIRONMENTAL DAMAGE. LICENSEE'S RIGHTS UNDER THIS LICENSE WILL
   TERMINATE AUTOMATICALLY AND IMMEDIATELY WITHOUT NOTICE IF LICENSEE
   FAILS TO COMPLY WITH THIS PARAGRAPH.

   IN NO EVENT SHALL ERICSSON BE LIABLE FOR ANY DAMAGES WHATSOEVER,
   INCLUDING BUT NOT LIMITED TO PERSONAL INJURY, ANY GENERAL, SPECIAL,
   INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES, ARISING OUT OF OR IN
   CONNECTION WITH THE USE OR INABILITY TO USE THE SOFTWARE (INCLUDING
   BUT NOT LIMITED TO LOSS OF PROFITS, BUSINESS INTERUPTIONS, OR ANY
   OTHER COMMERCIAL DAMAGES OR LOSSES, LOSS OF DATA OR DATA BEING
   RENDERED INACCURATE OR LOSSES SUSTAINED BY THE LICENSEE OR THIRD
   PARTIES OR A FAILURE OF THE SOFTWARE TO OPERATE WITH ANY OTHER
   SOFTWARE) REGARDLESS OF THE THEORY OF LIABILITY (CONTRACT, TORT, OR
   OTHERWISE), EVEN IF THE LICENSEE OR ANY OTHER PARTY HAS BEEN ADVISED
   OF THE POSSIBILITY OF SUCH DAMAGES.

   Licensee acknowledges that "ERICSSON ///" is the corporate trademark
   of Telefonaktiebolaget LM Ericsson and that both "Ericsson" and the
   figure "///" are important features of the trade names of
   Telefonaktiebolaget LM Ericsson. Nothing contained in these terms and
   conditions shall be deemed to grant Licensee any right, title or
   interest in the word "Ericsson" or the figure "///". No delay or
   omission by Ericsson to exercise any right or power shall impair any
   such right or power to be construed to be a waiver thereof. Consent by
   Ericsson to, or waiver of, a breach by the Licensee shall not
   constitute consent to, waiver of, or excuse for any other different or
   subsequent breach.

   This SLA shall be governed by the substantive law of Sweden. Any
   dispute, controversy or claim arising out of or in connection with
   this SLA, or the breach, termination or invalidity thereof, shall be
   submitted to the exclusive jurisdiction of the Swedish Courts.

*/

//// etcpack v2.74
////
//// NO WARRANTY
////
//// BECAUSE THE PROGRAM IS LICENSED FREE OF CHARGE THE PROGRAM IS PROVIDED
//// "AS IS". ERICSSON MAKES NO REPRESENTATIONS OF ANY KIND, EXTENDS NO
//// WARRANTIES OR CONDITIONS OF ANY KIND; EITHER EXPRESS, IMPLIED OR
//// STATUTORY; INCLUDING, BUT NOT LIMITED TO, EXPRESS, IMPLIED OR
//// STATUTORY WARRANTIES OR CONDITIONS OF TITLE, MERCHANTABILITY,
//// SATISFACTORY QUALITY, SUITABILITY AND FITNESS FOR A PARTICULAR
//// PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE
//// PROGRAM IS WITH YOU. SHOULD THE PROGRAM PROVE DEFECTIVE, YOU ASSUME
//// THE COST OF ALL NECESSARY SERVICING, REPAIR OR CORRECTION. ERICSSON
//// MAKES NO WARRANTY THAT THE MANUFACTURE, SALE, OFFERING FOR SALE,
//// DISTRIBUTION, LEASE, USE OR IMPORTATION UNDER THE LICENSE WILL BE FREE
//// FROM INFRINGEMENT OF PATENTS, COPYRIGHTS OR OTHER INTELLECTUAL
//// PROPERTY RIGHTS OF OTHERS, AND THE VALIDITY OF THE LICENSE IS SUBJECT
//// TO YOUR SOLE RESPONSIBILITY TO MAKE SUCH DETERMINATION AND ACQUIRE
//// SUCH LICENSES AS MAY BE NECESSARY WITH RESPECT TO PATENTS, COPYRIGHT
//// AND OTHER INTELLECTUAL PROPERTY OF THIRD PARTIES.
////
//// FOR THE AVOIDANCE OF DOUBT THE PROGRAM (I) IS NOT LICENSED FOR; (II)
//// IS NOT DESIGNED FOR OR INTENDED FOR; AND (III) MAY NOT BE USED FOR;
//// ANY MISSION CRITICAL APPLICATIONS SUCH AS, BUT NOT LIMITED TO
//// OPERATION OF NUCLEAR OR HEALTHCARE COMPUTER SYSTEMS AND/OR NETWORKS,
//// AIRCRAFT OR TRAIN CONTROL AND/OR COMMUNICATION SYSTEMS OR ANY OTHER
//// COMPUTER SYSTEMS AND/OR NETWORKS OR CONTROL AND/OR COMMUNICATION
//// SYSTEMS ALL IN WHICH CASE THE FAILURE OF THE PROGRAM COULD LEAD TO
//// DEATH, PERSONAL INJURY, OR SEVERE PHYSICAL, MATERIAL OR ENVIRONMENTAL
//// DAMAGE. YOUR RIGHTS UNDER THIS LICENSE WILL TERMINATE AUTOMATICALLY
//// AND IMMEDIATELY WITHOUT NOTICE IF YOU FAIL TO COMPLY WITH THIS
//// PARAGRAPH.
////
//// IN NO EVENT WILL ERICSSON, BE LIABLE FOR ANY DAMAGES WHATSOEVER,
//// INCLUDING BUT NOT LIMITED TO PERSONAL INJURY, ANY GENERAL, SPECIAL,
//// INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN
//// CONNECTION WITH THE USE OR INABILITY TO USE THE PROGRAM (INCLUDING BUT
//// NOT LIMITED TO LOSS OF PROFITS, BUSINESS INTERUPTIONS, OR ANY OTHER
//// COMMERCIAL DAMAGES OR LOSSES, LOSS OF DATA OR DATA BEING RENDERED
//// INACCURATE OR LOSSES SUSTAINED BY YOU OR THIRD PARTIES OR A FAILURE OF
//// THE PROGRAM TO OPERATE WITH ANY OTHER PROGRAMS) REGARDLESS OF THE
//// THEORY OF LIABILITY (CONTRACT, TORT OR OTHERWISE), EVEN IF SUCH HOLDER
//// OR OTHER PARTY HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
////
//// (C) Ericsson AB 2013. All Rights Reserved.
////

#include <cstdio>
#include <cstdlib>

// Typedefs
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef short int16;

// Macros to help with bit extraction/insertion
#define SHIFT(size, startpos) ((startpos) - (size) + 1)
#define MASK(size, startpos) (((2 << (size - 1)) - 1) << SHIFT(size, startpos))
#define PUTBITS(dest, data, size, startpos) \
  dest = ((dest & ~MASK(size, startpos))    \
          | ((data << SHIFT(size, startpos)) & MASK(size, startpos)))
#define SHIFTHIGH(size, startpos) (((startpos) - 32) - (size) + 1)
#define MASKHIGH(size, startpos) \
  (((1 << (size)) - 1) << SHIFTHIGH(size, startpos))
#define PUTBITSHIGH(dest, data, size, startpos) \
  dest = ((dest & ~MASKHIGH(size, startpos))    \
          | ((data << SHIFTHIGH(size, startpos)) & MASKHIGH(size, startpos)))
#define GETBITS(source, size, startpos) \
  (((source) >> ((startpos) - (size) + 1)) & ((1 << (size)) - 1))
#define GETBITSHIGH(source, size, startpos) \
  (((source) >> (((startpos) - 32) - (size) + 1)) & ((1 << (size)) - 1))
#ifndef PGMOUT
#define PGMOUT 0
#endif
// Thumb macros and definitions
#define R_BITS59T 4
#define G_BITS59T 4
#define B_BITS59T 4
#define R_BITS58H 4
#define G_BITS58H 4
#define B_BITS58H 4
#define MAXIMUM_ERROR (255 * 255 * 16 * 1000)
#define R 0
#define G 1
#define B 2
#define BLOCKHEIGHT 4
#define BLOCKWIDTH 4
#define BINPOW(power) (1 << (power))
#define TABLE_BITS_59T 3
#define TABLE_BITS_58H 3

// Helper Macros
#define CLAMP(ll, x, ul) (((x) < (ll)) ? (ll) : (((x) > (ul)) ? (ul) : (x)))
#define JAS_ROUND(x) (((x) < 0.0) ? ((int)((x) - 0.5)) : ((int)((x) + 0.5)))

#define RED_CHANNEL(img, width, x, y, channels) \
  img[channels * (y * width + x) + 0]
#define GREEN_CHANNEL(img, width, x, y, channels) \
  img[channels * (y * width + x) + 1]
#define BLUE_CHANNEL(img, width, x, y, channels) \
  img[channels * (y * width + x) + 2]
#define ALPHA_CHANNEL(img, width, x, y, channels) \
  img[channels * (y * width + x) + 3]

// Global tables
static uint8 table59T[8] = {3,  6,  11, 16, 23,
                            32, 41, 64};  // 3-bit table for the 59 bit T-mode
static uint8 table58H[8] = {3,  6,  11, 16, 23,
                            32, 41, 64};  // 3-bit table for the 58 bit H-mode
static int compressParams[16][4] = {
    {-8, -2, 2, 8},       {-8, -2, 2, 8},       {-17, -5, 5, 17},
    {-17, -5, 5, 17},     {-29, -9, 9, 29},     {-29, -9, 9, 29},
    {-42, -13, 13, 42},   {-42, -13, 13, 42},   {-60, -18, 18, 60},
    {-60, -18, 18, 60},   {-80, -24, 24, 80},   {-80, -24, 24, 80},
    {-106, -33, 33, 106}, {-106, -33, 33, 106}, {-183, -47, 47, 183},
    {-183, -47, 47, 183}};
static int unscramble[4] = {2, 3, 1, 0};
int alphaTableInitialized = 0;
int alphaTable[256][8];
int alphaBase[16][4] = {
    {-15, -9, -6, -3}, {-13, -10, -7, -3}, {-13, -8, -5, -2}, {-13, -6, -4, -2},
    {-12, -8, -6, -3}, {-11, -9, -7, -3},  {-11, -8, -7, -4}, {-11, -8, -5, -3},
    {-10, -8, -6, -2}, {-10, -8, -5, -2},  {-10, -8, -4, -2}, {-10, -7, -5, -2},
    {-10, -7, -4, -3}, {-10, -3, -2, -1},  {-9, -8, -6, -4},  {-9, -7, -5, -3}};

// Global variables
int formatSigned = 0;

// Enums
enum { PATTERN_H = 0, PATTERN_T = 1 };

// Code used to create the valtab
// NO WARRANTY --- SEE STATEMENT IN TOP OF FILE (C) Ericsson AB 2013. All Rights
// Reserved.
void setupAlphaTable() {
  if (alphaTableInitialized) return;
  alphaTableInitialized = 1;

  // read table used for alpha compression
  int buf;
  for (int i = 16; i < 32; i++) {
    for (int j = 0; j < 8; j++) {
      buf = alphaBase[i - 16][3 - j % 4];
      if (j < 4)
        alphaTable[i][j] = buf;
      else
        alphaTable[i][j] = (-buf - 1);
    }
  }

  // beyond the first 16 values, the rest of the table is implicit.. so
  // calculate that!
  for (int i = 0; i < 256; i++) {
    // fill remaining slots in table with multiples of the first ones.
    int mul = i / 16;
    int old = 16 + i % 16;
    for (int j = 0; j < 8; j++) {
      alphaTable[i][j] = alphaTable[old][j] * mul;
      // note: we don't do clamping here, though we could, because we'll be
      // clamped afterwards anyway.
    }
  }
}

// Read a word in big endian style
// NO WARRANTY --- SEE STATEMENT IN TOP OF FILE (C) Ericsson AB 2013. All Rights
// Reserved.
// void read_big_endian_2byte_word(unsigned short* blockadr, FILE* f) {
//   uint8 bytes[2];
//   unsigned short block;

//   fread(&bytes[0], 1, 1, f);
//   fread(&bytes[1], 1, 1, f);

//   block = 0;
//   block |= bytes[0];
//   block = block << 8;
//   block |= bytes[1];

//   blockadr[0] = block;
// }

// Read a word in big endian style
// NO WARRANTY --- SEE STATEMENT IN TOP OF FILE (C) Ericsson AB 2013. All Rights
// Reserved.
// void read_big_endian_4byte_word(unsigned int* blockadr, FILE* f) {
//   uint8 bytes[4];
//   unsigned int block;

//   fread(&bytes[0], 1, 1, f);
//   fread(&bytes[1], 1, 1, f);
//   fread(&bytes[2], 1, 1, f);
//   fread(&bytes[3], 1, 1, f);

//   block = 0;
//   block |= bytes[0];
//   block = block << 8;
//   block |= bytes[1];
//   block = block << 8;
//   block |= bytes[2];
//   block = block << 8;
//   block |= bytes[3];

//   blockadr[0] = block;
// }

// The format stores the bits for the three extra modes in a roundabout way to
// be able to fit them without increasing the bit rate. This function converts
// them into something that is easier to work with. NO WARRANTY --- SEE
// STATEMENT IN TOP OF FILE (C) Ericsson AB 2013. All Rights Reserved.
void unstuff57bits(unsigned int planar_word1, unsigned int planar_word2,
                   unsigned int& planar57_word1, unsigned int& planar57_word2) {
  // Get bits from twotimer configuration for 57 bits
  //
  // Go to this bit layout:
  //
  //      63 62 61 60 59 58 57 56 55 54 53 52 51 50 49 48 47 46 45 44 43 42 41
  //      40 39 38 37 36 35 34 33 32
  //      -----------------------------------------------------------------------------------------------
  //     |R0               |G01G02              |B01B02  ;B03     |RH1 |RH2|GH |
  //      -----------------------------------------------------------------------------------------------
  //
  //      31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9
  //      8  7  6  5  4  3  2  1  0
  //      -----------------------------------------------------------------------------------------------
  //     |BH               |RV               |GV                  |BV | not used
  //     |
  //      -----------------------------------------------------------------------------------------------
  //
  //  From this:
  //
  //      63 62 61 60 59 58 57 56 55 54 53 52 51 50 49 48 47 46 45 44 43 42 41
  //      40 39 38 37 36 35 34 33 32
  //      ------------------------------------------------------------------------------------------------
  //     |//|R0               |G01|/|G02              |B01|/ // //|B02  |//|B03
  //     |RH1           |df|RH2|
  //      ------------------------------------------------------------------------------------------------
  //
  //      31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9
  //      8  7  6  5  4  3  2  1  0
  //      -----------------------------------------------------------------------------------------------
  //     |GH                  |BH               |RV               |GV |BV |
  //      -----------------------------------------------------------------------------------------------
  //
  //      63 62 61 60 59 58 57 56 55 54 53 52 51 50 49 48 47 46 45 44 43 42 41
  //      40 39 38 37 36 35 34  33  32
  //      ---------------------------------------------------------------------------------------------------
  //     | base col1    | dcol 2 | base col1    | dcol 2 | base col 1   | dcol 2
  //     | table  | table  |diff|flip| | R1' (5 bits) | dR2    | G1' (5 bits) |
  //     dG2    | B1' (5 bits) | dB2    | cw 1   | cw 2   |bit |bit |
  //      ---------------------------------------------------------------------------------------------------

  uint8 RO, GO1, GO2, BO1, BO2, BO3, RH1, RH2, GH, BH, RV, GV, BV;

  RO = static_cast<uint8>(GETBITSHIGH(planar_word1, 6, 62));
  GO1 = static_cast<uint8>(GETBITSHIGH(planar_word1, 1, 56));
  GO2 = static_cast<uint8>(GETBITSHIGH(planar_word1, 6, 54));
  BO1 = static_cast<uint8>(GETBITSHIGH(planar_word1, 1, 48));
  BO2 = static_cast<uint8>(GETBITSHIGH(planar_word1, 2, 44));
  BO3 = static_cast<uint8>(GETBITSHIGH(planar_word1, 3, 41));
  RH1 = static_cast<uint8>(GETBITSHIGH(planar_word1, 5, 38));
  RH2 = static_cast<uint8>(GETBITSHIGH(planar_word1, 1, 32));
  GH = static_cast<uint8>(GETBITS(planar_word2, 7, 31));
  BH = static_cast<uint8>(GETBITS(planar_word2, 6, 24));
  RV = static_cast<uint8>(GETBITS(planar_word2, 6, 18));
  GV = static_cast<uint8>(GETBITS(planar_word2, 7, 12));
  BV = static_cast<uint8>(GETBITS(planar_word2, 6, 5));

  planar57_word1 = 0;
  planar57_word2 = 0;
  PUTBITSHIGH(planar57_word1, RO, 6, 63);
  PUTBITSHIGH(planar57_word1, GO1, 1, 57);
  PUTBITSHIGH(planar57_word1, GO2, 6, 56);
  PUTBITSHIGH(planar57_word1, BO1, 1, 50);
  PUTBITSHIGH(planar57_word1, BO2, 2, 49);
  PUTBITSHIGH(planar57_word1, BO3, 3, 47);
  PUTBITSHIGH(planar57_word1, RH1, 5, 44);
  PUTBITSHIGH(planar57_word1, RH2, 1, 39);
  PUTBITSHIGH(planar57_word1, GH, 7, 38);
  PUTBITS(planar57_word2, BH, 6, 31);
  PUTBITS(planar57_word2, RV, 6, 25);
  PUTBITS(planar57_word2, GV, 7, 19);
  PUTBITS(planar57_word2, BV, 6, 12);
}

// The format stores the bits for the three extra modes in a roundabout way to
// be able to fit them without increasing the bit rate. This function converts
// them into something that is easier to work with. NO WARRANTY --- SEE
// STATEMENT IN TOP OF FILE (C) Ericsson AB 2013. All Rights Reserved.
void unstuff58bits(unsigned int thumbH_word1, unsigned int thumbH_word2,
                   unsigned int& thumbH58_word1, unsigned int& thumbH58_word2) {
  // Go to this layout:
  //
  //     |63 62 61 60 59 58|57 56 55 54 53 52 51|50 49|48 47 46 45 44 43 42 41
  //     40 39 38 37 36 35 34 33|32   |
  //     |-------empty-----|part0---------------|part1|part2------------------------------------------|part3|
  //
  //  from this:
  //
  //      63 62 61 60 59 58 57 56 55 54 53 52 51 50 49 48 47 46 45 44 43 42 41
  //      40 39 38 37 36 35 34 33 32
  //      --------------------------------------------------------------------------------------------------|
  //     |//|part0               |// // //|part1|//|part2 |df|part3|
  //      --------------------------------------------------------------------------------------------------|

  unsigned int part0, part1, part2, part3;

  // move parts
  part0 = GETBITSHIGH(thumbH_word1, 7, 62);
  part1 = GETBITSHIGH(thumbH_word1, 2, 52);
  part2 = GETBITSHIGH(thumbH_word1, 16, 49);
  part3 = GETBITSHIGH(thumbH_word1, 1, 32);
  thumbH58_word1 = 0;
  PUTBITSHIGH(thumbH58_word1, part0, 7, 57);
  PUTBITSHIGH(thumbH58_word1, part1, 2, 50);
  PUTBITSHIGH(thumbH58_word1, part2, 16, 48);
  PUTBITSHIGH(thumbH58_word1, part3, 1, 32);

  thumbH58_word2 = thumbH_word2;
}

// The format stores the bits for the three extra modes in a roundabout way to
// be able to fit them without increasing the bit rate. This function converts
// them into something that is easier to work with. NO WARRANTY --- SEE
// STATEMENT IN TOP OF FILE (C) Ericsson AB 2013. All Rights Reserved.
void unstuff59bits(unsigned int thumbT_word1, unsigned int thumbT_word2,
                   unsigned int& thumbT59_word1, unsigned int& thumbT59_word2) {
  // Get bits from twotimer configuration 59 bits.
  //
  // Go to this bit layout:
  //
  //     |63 62 61 60 59|58 57 56 55|54 53 52 51|50 49 48 47|46 45 44 43|42 41
  //     40 39|38 37 36 35|34 33 32|
  //     |----empty-----|---red 0---|--green 0--|--blue 0---|---red 1---|--green
  //     1--|--blue 1---|--dist--|
  //
  //     |31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 09
  //     08 07 06 05 04 03 02 01 00|
  //     |----------------------------------------index
  //     bits---------------------------------------------|
  //
  //
  //  From this:
  //
  //      63 62 61 60 59 58 57 56 55 54 53 52 51 50 49 48 47 46 45 44 43 42 41
  //      40 39 38 37 36 35 34 33 32
  //      -----------------------------------------------------------------------------------------------
  //     |// // //|R0a  |//|R0b  |G0         |B0         |R1         |G1 |B1 |da
  //     |df|db|
  //      -----------------------------------------------------------------------------------------------
  //
  //     |31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 09
  //     08 07 06 05 04 03 02 01 00|
  //     |----------------------------------------index
  //     bits---------------------------------------------|
  //
  //      63 62 61 60 59 58 57 56 55 54 53 52 51 50 49 48 47 46 45 44 43 42 41
  //      40 39 38 37 36 35 34 33 32
  //      -----------------------------------------------------------------------------------------------
  //     | base col1    | dcol 2 | base col1    | dcol 2 | base col 1   | dcol 2
  //     | table  | table  |df|fp| | R1' (5 bits) | dR2    | G1' (5 bits) | dG2
  //     | B1' (5 bits) | dB2    | cw 1   | cw 2   |bt|bt|
  //      ------------------------------------------------------------------------------------------------

  uint8 R0a;

  // Fix middle part
  thumbT59_word1 = thumbT_word1 >> 1;
  // Fix db (lowest bit of d)
  PUTBITSHIGH(thumbT59_word1, thumbT_word1, 1, 32);
  // Fix R0a (top two bits of R0)
  R0a = static_cast<uint8>(GETBITSHIGH(thumbT_word1, 2, 60));
  PUTBITSHIGH(thumbT59_word1, R0a, 2, 58);

  // Zero top part (not needed)
  PUTBITSHIGH(thumbT59_word1, 0, 5, 63);

  thumbT59_word2 = thumbT_word2;
}

// The color bits are expanded to the full color
// NO WARRANTY --- SEE STATEMENT IN TOP OF FILE (C) Ericsson AB 2013. All Rights
// Reserved.
void decompressColor(int R_B, int G_B, int B_B, uint8(colors_RGB444)[2][3],
                     uint8(colors)[2][3]) {
  // The color should be retrieved as:
  //
  // c = round(255/(r_bits^2-1))*comp_color
  //
  // This is similar to bit replication
  //
  // Note -- this code only work for bit replication from 4 bits and up --- 3
  // bits needs two copy operations.

  colors[0][R] = (colors_RGB444[0][R] << (8 - R_B))
                 | (colors_RGB444[0][R] >> (R_B - (8 - R_B)));
  colors[0][G] = (colors_RGB444[0][G] << (8 - G_B))
                 | (colors_RGB444[0][G] >> (G_B - (8 - G_B)));
  colors[0][B] = (colors_RGB444[0][B] << (8 - B_B))
                 | (colors_RGB444[0][B] >> (B_B - (8 - B_B)));
  colors[1][R] = (colors_RGB444[1][R] << (8 - R_B))
                 | (colors_RGB444[1][R] >> (R_B - (8 - R_B)));
  colors[1][G] = (colors_RGB444[1][G] << (8 - G_B))
                 | (colors_RGB444[1][G] >> (G_B - (8 - G_B)));
  colors[1][B] = (colors_RGB444[1][B] << (8 - B_B))
                 | (colors_RGB444[1][B] >> (B_B - (8 - B_B)));
}

void calculatePaintColors59T(uint8 d, uint8 p, uint8(colors)[2][3],
                             uint8(possible_colors)[4][3]) {
  //////////////////////////////////////////////
  //
  //		C3      C1		C4----C1---C2
  //		|		|			  |
  //		|		|			  |
  //		|-------|			  |
  //		|		|			  |
  //		|		|			  |
  //		C4      C2			  C3
  //
  //////////////////////////////////////////////

  // C4
  possible_colors[3][R] =
      static_cast<uint8>(CLAMP(0, colors[1][R] - table59T[d], 255));
  possible_colors[3][G] =
      static_cast<uint8>(CLAMP(0, colors[1][G] - table59T[d], 255));
  possible_colors[3][B] =
      static_cast<uint8>(CLAMP(0, colors[1][B] - table59T[d], 255));

  if (p == PATTERN_T) {
    // C3
    possible_colors[0][R] = colors[0][R];
    possible_colors[0][G] = colors[0][G];
    possible_colors[0][B] = colors[0][B];
    // C2
    possible_colors[1][R] =
        static_cast<uint8>(CLAMP(0, colors[1][R] + table59T[d], 255));
    possible_colors[1][G] =
        static_cast<uint8>(CLAMP(0, colors[1][G] + table59T[d], 255));
    possible_colors[1][B] =
        static_cast<uint8>(CLAMP(0, colors[1][B] + table59T[d], 255));
    // C1
    possible_colors[2][R] = colors[1][R];
    possible_colors[2][G] = colors[1][G];
    possible_colors[2][B] = colors[1][B];

  } else {
    printf("Invalid pattern. Terminating");
    exit(1);
  }
}
// Decompress a T-mode block (simple packing)
// Simple 59T packing:
//|63 62 61 60 59|58 57 56 55|54 53 52 51|50 49 48 47|46 45 44 43|42 41 40 39|38
// 37 36 35|34 33 32|
//|----empty-----|---red 0---|--green 0--|--blue 0---|---red 1---|--green
// 1--|--blue 1---|--dist--|
//
//|31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 09 08 07 06
// 05 04 03 02 01 00|
//|----------------------------------------index
// bits---------------------------------------------|
// NO WARRANTY --- SEE STATEMENT IN TOP OF FILE (C) Ericsson AB 2013. All Rights
// Reserved.
void decompressBlockTHUMB59Tc(unsigned int block_part1,
                              unsigned int block_part2, uint8* img, int width,
                              int height, int startx, int starty,
                              int channels) {
  uint8 colorsRGB444[2][3];
  uint8 colors[2][3];
  uint8 paint_colors[4][3];
  uint8 distance;
  uint8 block_mask[4][4];

  // First decode left part of block.
  colorsRGB444[0][R] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 58));
  colorsRGB444[0][G] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 54));
  colorsRGB444[0][B] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 50));

  colorsRGB444[1][R] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 46));
  colorsRGB444[1][G] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 42));
  colorsRGB444[1][B] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 38));

  distance = static_cast<uint8>(GETBITSHIGH(block_part1, TABLE_BITS_59T, 34));

  // Extend the two colors to RGB888
  decompressColor(R_BITS59T, G_BITS59T, B_BITS59T, colorsRGB444, colors);
  calculatePaintColors59T(distance, PATTERN_T, colors, paint_colors);

  // Choose one of the four paint colors for each texel
  for (uint8 x = 0; x < BLOCKWIDTH; ++x) {
    for (uint8 y = 0; y < BLOCKHEIGHT; ++y) {
      // block_mask[x][y] = GETBITS(block_part2,2,31-(y*4+x)*2);
      block_mask[x][y] =
          static_cast<uint8>(GETBITS(block_part2, 1, (y + x * 4) + 16) << 1);
      block_mask[x][y] |= GETBITS(block_part2, 1, (y + x * 4));
      img[channels * ((starty + y) * width + startx + x) + R] =
          static_cast<uint8>(
              CLAMP(0, paint_colors[block_mask[x][y]][R], 255));  // RED
      img[channels * ((starty + y) * width + startx + x) + G] =
          static_cast<uint8>(
              CLAMP(0, paint_colors[block_mask[x][y]][G], 255));  // GREEN
      img[channels * ((starty + y) * width + startx + x) + B] =
          static_cast<uint8>(
              CLAMP(0, paint_colors[block_mask[x][y]][B], 255));  // BLUE
    }
  }
}

void decompressBlockTHUMB59T(unsigned int block_part1, unsigned int block_part2,
                             uint8* img, int width, int height, int startx,
                             int starty) {
  decompressBlockTHUMB59Tc(block_part1, block_part2, img, width, height, startx,
                           starty, 3);
}

// Calculate the paint colors from the block colors
// using a distance d and one of the H- or T-patterns.
// NO WARRANTY --- SEE STATEMENT IN TOP OF FILE (C) Ericsson AB 2013. All Rights
// Reserved.
void calculatePaintColors58H(uint8 d, uint8 p, uint8(colors)[2][3],
                             uint8(possible_colors)[4][3]) {
  //////////////////////////////////////////////
  //
  //		C3      C1		C4----C1---C2
  //		|		|			  |
  //		|		|			  |
  //		|-------|			  |
  //		|		|			  |
  //		|		|			  |
  //		C4      C2			  C3
  //
  //////////////////////////////////////////////

  // C4
  possible_colors[3][R] =
      static_cast<uint8>(CLAMP(0, colors[1][R] - table58H[d], 255));
  possible_colors[3][G] =
      static_cast<uint8>(CLAMP(0, colors[1][G] - table58H[d], 255));
  possible_colors[3][B] =
      static_cast<uint8>(CLAMP(0, colors[1][B] - table58H[d], 255));

  if (p == PATTERN_H) {
    // C1
    possible_colors[0][R] =
        static_cast<uint8>(CLAMP(0, colors[0][R] + table58H[d], 255));
    possible_colors[0][G] =
        static_cast<uint8>(CLAMP(0, colors[0][G] + table58H[d], 255));
    possible_colors[0][B] =
        static_cast<uint8>(CLAMP(0, colors[0][B] + table58H[d], 255));
    // C2
    possible_colors[1][R] =
        static_cast<uint8>(CLAMP(0, colors[0][R] - table58H[d], 255));
    possible_colors[1][G] =
        static_cast<uint8>(CLAMP(0, colors[0][G] - table58H[d], 255));
    possible_colors[1][B] =
        static_cast<uint8>(CLAMP(0, colors[0][B] - table58H[d], 255));
    // C3
    possible_colors[2][R] =
        static_cast<uint8>(CLAMP(0, colors[1][R] + table58H[d], 255));
    possible_colors[2][G] =
        static_cast<uint8>(CLAMP(0, colors[1][G] + table58H[d], 255));
    possible_colors[2][B] =
        static_cast<uint8>(CLAMP(0, colors[1][B] + table58H[d], 255));
  } else {
    printf("Invalid pattern. Terminating");
    exit(1);
  }
}

// Decompress an H-mode block
// NO WARRANTY --- SEE STATEMENT IN TOP OF FILE (C) Ericsson AB 2013. All Rights
// Reserved.
void decompressBlockTHUMB58Hc(unsigned int block_part1,
                              unsigned int block_part2, uint8* img, int width,
                              int height, int startx, int starty,
                              int channels) {
  unsigned int col0, col1;
  uint8 colors[2][3];
  uint8 colorsRGB444[2][3];
  uint8 paint_colors[4][3];
  uint8 distance;
  uint8 block_mask[4][4];

  // First decode left part of block.
  colorsRGB444[0][R] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 57));
  colorsRGB444[0][G] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 53));
  colorsRGB444[0][B] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 49));

  colorsRGB444[1][R] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 45));
  colorsRGB444[1][G] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 41));
  colorsRGB444[1][B] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 37));

  distance = 0;
  distance = static_cast<uint8>((GETBITSHIGH(block_part1, 2, 33)) << 1);

  col0 = GETBITSHIGH(block_part1, 12, 57);
  col1 = GETBITSHIGH(block_part1, 12, 45);

  if (col0 >= col1) {
    distance |= 1;
  }

  // Extend the two colors to RGB888
  decompressColor(R_BITS58H, G_BITS58H, B_BITS58H, colorsRGB444, colors);

  calculatePaintColors58H(distance, PATTERN_H, colors, paint_colors);

  // Choose one of the four paint colors for each texel
  for (uint8 x = 0; x < BLOCKWIDTH; ++x) {
    for (uint8 y = 0; y < BLOCKHEIGHT; ++y) {
      // block_mask[x][y] = GETBITS(block_part2,2,31-(y*4+x)*2);
      block_mask[x][y] =
          static_cast<uint8>(GETBITS(block_part2, 1, (y + x * 4) + 16) << 1);
      block_mask[x][y] |= GETBITS(block_part2, 1, (y + x * 4));
      img[channels * ((starty + y) * width + startx + x) + R] =
          static_cast<uint8>(
              CLAMP(0, paint_colors[block_mask[x][y]][R], 255));  // RED
      img[channels * ((starty + y) * width + startx + x) + G] =
          static_cast<uint8>(
              CLAMP(0, paint_colors[block_mask[x][y]][G], 255));  // GREEN
      img[channels * ((starty + y) * width + startx + x) + B] =
          static_cast<uint8>(
              CLAMP(0, paint_colors[block_mask[x][y]][B], 255));  // BLUE
    }
  }
}
void decompressBlockTHUMB58H(unsigned int block_part1, unsigned int block_part2,
                             uint8* img, int width, int height, int startx,
                             int starty) {
  decompressBlockTHUMB58Hc(block_part1, block_part2, img, width, height, startx,
                           starty, 3);
}

// Decompress the planar mode.
// NO WARRANTY --- SEE STATEMENT IN TOP OF FILE (C) Ericsson AB 2013. All Rights
// Reserved.
void decompressBlockPlanar57c(unsigned int compressed57_1,
                              unsigned int compressed57_2, uint8* img,
                              int width, int height, int startx, int starty,
                              int channels) {
  uint8 colorO[3], colorH[3], colorV[3];

  colorO[0] = static_cast<uint8>(GETBITSHIGH(compressed57_1, 6, 63));
  colorO[1] = static_cast<uint8>(GETBITSHIGH(compressed57_1, 7, 57));
  colorO[2] = static_cast<uint8>(GETBITSHIGH(compressed57_1, 6, 50));
  colorH[0] = static_cast<uint8>(GETBITSHIGH(compressed57_1, 6, 44));
  colorH[1] = static_cast<uint8>(GETBITSHIGH(compressed57_1, 7, 38));
  colorH[2] = static_cast<uint8>(GETBITS(compressed57_2, 6, 31));
  colorV[0] = static_cast<uint8>(GETBITS(compressed57_2, 6, 25));
  colorV[1] = static_cast<uint8>(GETBITS(compressed57_2, 7, 19));
  colorV[2] = static_cast<uint8>(GETBITS(compressed57_2, 6, 12));

  colorO[0] = (colorO[0] << 2) | (colorO[0] >> 4);
  colorO[1] = (colorO[1] << 1) | (colorO[1] >> 6);
  colorO[2] = (colorO[2] << 2) | (colorO[2] >> 4);

  colorH[0] = (colorH[0] << 2) | (colorH[0] >> 4);
  colorH[1] = (colorH[1] << 1) | (colorH[1] >> 6);
  colorH[2] = (colorH[2] << 2) | (colorH[2] >> 4);

  colorV[0] = (colorV[0] << 2) | (colorV[0] >> 4);
  colorV[1] = (colorV[1] << 1) | (colorV[1] >> 6);
  colorV[2] = (colorV[2] << 2) | (colorV[2] >> 4);

  int xx, yy;

  for (xx = 0; xx < 4; xx++) {
    for (yy = 0; yy < 4; yy++) {
      img[channels * width * (starty + yy) + channels * (startx + xx) + 0] =
          static_cast<uint8>(
              CLAMP(0,
                    ((xx * (colorH[0] - colorO[0])
                      + yy * (colorV[0] - colorO[0]) + 4 * colorO[0] + 2)
                     >> 2),
                    255));
      img[channels * width * (starty + yy) + channels * (startx + xx) + 1] =
          static_cast<uint8>(
              CLAMP(0,
                    ((xx * (colorH[1] - colorO[1])
                      + yy * (colorV[1] - colorO[1]) + 4 * colorO[1] + 2)
                     >> 2),
                    255));
      img[channels * width * (starty + yy) + channels * (startx + xx) + 2] =
          static_cast<uint8>(
              CLAMP(0,
                    ((xx * (colorH[2] - colorO[2])
                      + yy * (colorV[2] - colorO[2]) + 4 * colorO[2] + 2)
                     >> 2),
                    255));

      // Equivalent method
      /*img[channels*width*(starty+yy) + channels*(startx+xx) + 0] =
(int)CLAMP(0, JAS_ROUND((xx*(colorH[0]-colorO[0])/4.0f +
yy*(colorV[0]-colorO[0])/4.0f + colorO[0])), 255);
img[channels*width*(starty+yy)
+ channels*(startx+xx) + 1] = (int)CLAMP(0,
JAS_ROUND((xx*(colorH[1]-colorO[1])/4.0f + yy*(colorV[1]-colorO[1])/4.0f +
colorO[1])), 255);
img[channels*width*(starty+yy) + channels*(startx+xx) + 2] = (int)CLAMP(0,
JAS_ROUND((xx*(colorH[2]-colorO[2])/4.0f + yy*(colorV[2]-colorO[2])/4.0f +
colorO[2])), 255);*/
    }
  }
}
void decompressBlockPlanar57(unsigned int compressed57_1,
                             unsigned int compressed57_2, uint8* img, int width,
                             int height, int startx, int starty) {
  decompressBlockPlanar57c(compressed57_1, compressed57_2, img, width, height,
                           startx, starty, 3);
}
// Decompress an ETC1 block (or ETC2 using individual or differential mode).
// NO WARRANTY --- SEE STATEMENT IN TOP OF FILE (C) Ericsson AB 2013. All Rights
// Reserved.
void decompressBlockDiffFlipC(unsigned int block_part1,
                              unsigned int block_part2, uint8* img, int width,
                              int height, int startx, int starty,
                              int channels) {
  uint8 avg_color[3], enc_color1[3], enc_color2[3];
  signed char diff[3];
  int table;
  int index, shift;
  int r, g, b;
  int diffbit;
  int flipbit;

  diffbit = (GETBITSHIGH(block_part1, 1, 33));
  flipbit = (GETBITSHIGH(block_part1, 1, 32));

  if (!diffbit) {
    // We have diffbit = 0.

    // First decode left part of block.
    avg_color[0] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 63));
    avg_color[1] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 55));
    avg_color[2] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 47));

    // Here, we should really multiply by 17 instead of 16. This can
    // be done by just copying the four lower bits to the upper ones
    // while keeping the lower bits.
    avg_color[0] |= (avg_color[0] << 4);
    avg_color[1] |= (avg_color[1] << 4);
    avg_color[2] |= (avg_color[2] << 4);

    table = GETBITSHIGH(block_part1, 3, 39) << 1;

    unsigned int pixel_indices_MSB, pixel_indices_LSB;

    pixel_indices_MSB = GETBITS(block_part2, 16, 31);
    pixel_indices_LSB = GETBITS(block_part2, 16, 15);

    if ((flipbit) == 0) {
      // We should not flip
      shift = 0;
      for (int x = startx; x < startx + 2; x++) {
        for (int y = starty; y < starty + 4; y++) {
          index = ((pixel_indices_MSB >> shift) & 1) << 1;
          index |= ((pixel_indices_LSB >> shift) & 1);
          shift++;
          index = unscramble[index];

          r = RED_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[0] + compressParams[table][index], 255));
          g = GREEN_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[1] + compressParams[table][index], 255));
          b = BLUE_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[2] + compressParams[table][index], 255));
        }
      }
    } else {
      // We should flip
      shift = 0;
      for (int x = startx; x < startx + 4; x++) {
        for (int y = starty; y < starty + 2; y++) {
          index = ((pixel_indices_MSB >> shift) & 1) << 1;
          index |= ((pixel_indices_LSB >> shift) & 1);
          shift++;
          index = unscramble[index];

          r = RED_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[0] + compressParams[table][index], 255));
          g = GREEN_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[1] + compressParams[table][index], 255));
          b = BLUE_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[2] + compressParams[table][index], 255));
        }
        shift += 2;
      }
    }

    // Now decode other part of block.
    avg_color[0] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 59));
    avg_color[1] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 51));
    avg_color[2] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 43));

    // Here, we should really multiply by 17 instead of 16. This can
    // be done by just copying the four lower bits to the upper ones
    // while keeping the lower bits.
    avg_color[0] |= (avg_color[0] << 4);
    avg_color[1] |= (avg_color[1] << 4);
    avg_color[2] |= (avg_color[2] << 4);

    table = GETBITSHIGH(block_part1, 3, 36) << 1;
    pixel_indices_MSB = GETBITS(block_part2, 16, 31);
    pixel_indices_LSB = GETBITS(block_part2, 16, 15);

    if ((flipbit) == 0) {
      // We should not flip
      shift = 8;
      for (int x = startx + 2; x < startx + 4; x++) {
        for (int y = starty; y < starty + 4; y++) {
          index = ((pixel_indices_MSB >> shift) & 1) << 1;
          index |= ((pixel_indices_LSB >> shift) & 1);
          shift++;
          index = unscramble[index];

          r = RED_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[0] + compressParams[table][index], 255));
          g = GREEN_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[1] + compressParams[table][index], 255));
          b = BLUE_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[2] + compressParams[table][index], 255));
        }
      }
    } else {
      // We should flip
      shift = 2;
      for (int x = startx; x < startx + 4; x++) {
        for (int y = starty + 2; y < starty + 4; y++) {
          index = ((pixel_indices_MSB >> shift) & 1) << 1;
          index |= ((pixel_indices_LSB >> shift) & 1);
          shift++;
          index = unscramble[index];

          r = RED_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[0] + compressParams[table][index], 255));
          g = GREEN_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[1] + compressParams[table][index], 255));
          b = BLUE_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[2] + compressParams[table][index], 255));
        }
        shift += 2;
      }
    }
  } else {
    // We have diffbit = 1.

    //      63 62 61 60 59 58 57 56 55 54 53 52 51 50 49 48 47 46 45 44 43 42 41
    //      40 39 38 37 36 35 34  33  32
    //      ---------------------------------------------------------------------------------------------------
    //     | base col1    | dcol 2 | base col1    | dcol 2 | base col 1   | dcol
    //     2 | table  | table  |diff|flip| | R1' (5 bits) | dR2    | G1' (5
    //     bits) | dG2    | B1' (5 bits) | dB2    | cw 1   | cw 2   |bit |bit |
    //      ---------------------------------------------------------------------------------------------------
    //
    //
    //     c) bit layout in bits 31 through 0 (in both cases)
    //
    //      31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9
    //      8  7  6  5  4  3   2   1  0
    //      --------------------------------------------------------------------------------------------------
    //     |       most significant pixel index bits       |         least
    //     significant pixel index bits       | | p| o| n| m| l| k| j| i| h| g|
    //     f| e| d| c| b| a| p| o| n| m| l| k| j| i| h| g| f| e| d| c | b | a |
    //      --------------------------------------------------------------------------------------------------

    // First decode left part of block.
    enc_color1[0] = static_cast<uint8>(GETBITSHIGH(block_part1, 5, 63));
    enc_color1[1] = static_cast<uint8>(GETBITSHIGH(block_part1, 5, 55));
    enc_color1[2] = static_cast<uint8>(GETBITSHIGH(block_part1, 5, 47));

    // Expand from 5 to 8 bits
    avg_color[0] = (enc_color1[0] << 3) | (enc_color1[0] >> 2);
    avg_color[1] = (enc_color1[1] << 3) | (enc_color1[1] >> 2);
    avg_color[2] = (enc_color1[2] << 3) | (enc_color1[2] >> 2);

    table = GETBITSHIGH(block_part1, 3, 39) << 1;

    unsigned int pixel_indices_MSB, pixel_indices_LSB;

    pixel_indices_MSB = GETBITS(block_part2, 16, 31);
    pixel_indices_LSB = GETBITS(block_part2, 16, 15);

    if ((flipbit) == 0) {
      // We should not flip
      shift = 0;
      for (int x = startx; x < startx + 2; x++) {
        for (int y = starty; y < starty + 4; y++) {
          index = ((pixel_indices_MSB >> shift) & 1) << 1;
          index |= ((pixel_indices_LSB >> shift) & 1);
          shift++;
          index = unscramble[index];

          r = RED_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[0] + compressParams[table][index], 255));
          g = GREEN_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[1] + compressParams[table][index], 255));
          b = BLUE_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[2] + compressParams[table][index], 255));
        }
      }
    } else {
      // We should flip
      shift = 0;
      for (int x = startx; x < startx + 4; x++) {
        for (int y = starty; y < starty + 2; y++) {
          index = ((pixel_indices_MSB >> shift) & 1) << 1;
          index |= ((pixel_indices_LSB >> shift) & 1);
          shift++;
          index = unscramble[index];

          r = RED_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[0] + compressParams[table][index], 255));
          g = GREEN_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[1] + compressParams[table][index], 255));
          b = BLUE_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[2] + compressParams[table][index], 255));
        }
        shift += 2;
      }
    }

    // Now decode right part of block.
    diff[0] = static_cast<signed char>(GETBITSHIGH(block_part1, 3, 58));
    diff[1] = static_cast<signed char>(GETBITSHIGH(block_part1, 3, 50));
    diff[2] = static_cast<signed char>(GETBITSHIGH(block_part1, 3, 42));

    // Extend sign bit to entire byte.
    diff[0] = (diff[0] << 5);
    diff[1] = (diff[1] << 5);
    diff[2] = (diff[2] << 5);
    diff[0] = diff[0] >> 5;
    diff[1] = diff[1] >> 5;
    diff[2] = diff[2] >> 5;

    //  Calculate second color
    enc_color2[0] = enc_color1[0] + diff[0];
    enc_color2[1] = enc_color1[1] + diff[1];
    enc_color2[2] = enc_color1[2] + diff[2];

    // Expand from 5 to 8 bits
    avg_color[0] = (enc_color2[0] << 3) | (enc_color2[0] >> 2);
    avg_color[1] = (enc_color2[1] << 3) | (enc_color2[1] >> 2);
    avg_color[2] = (enc_color2[2] << 3) | (enc_color2[2] >> 2);

    table = GETBITSHIGH(block_part1, 3, 36) << 1;
    pixel_indices_MSB = GETBITS(block_part2, 16, 31);
    pixel_indices_LSB = GETBITS(block_part2, 16, 15);

    if ((flipbit) == 0) {
      // We should not flip
      shift = 8;
      for (int x = startx + 2; x < startx + 4; x++) {
        for (int y = starty; y < starty + 4; y++) {
          index = ((pixel_indices_MSB >> shift) & 1) << 1;
          index |= ((pixel_indices_LSB >> shift) & 1);
          shift++;
          index = unscramble[index];

          r = RED_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[0] + compressParams[table][index], 255));
          g = GREEN_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[1] + compressParams[table][index], 255));
          b = BLUE_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[2] + compressParams[table][index], 255));
        }
      }
    } else {
      // We should flip
      shift = 2;
      for (int x = startx; x < startx + 4; x++) {
        for (int y = starty + 2; y < starty + 4; y++) {
          index = ((pixel_indices_MSB >> shift) & 1) << 1;
          index |= ((pixel_indices_LSB >> shift) & 1);
          shift++;
          index = unscramble[index];

          r = RED_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[0] + compressParams[table][index], 255));
          g = GREEN_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[1] + compressParams[table][index], 255));
          b = BLUE_CHANNEL(img, width, x, y, channels) = static_cast<uint8>(
              CLAMP(0, avg_color[2] + compressParams[table][index], 255));
        }
        shift += 2;
      }
    }
  }
}
void decompressBlockDiffFlip(unsigned int block_part1, unsigned int block_part2,
                             uint8* img, int width, int height, int startx,
                             int starty) {
  decompressBlockDiffFlipC(block_part1, block_part2, img, width, height, startx,
                           starty, 3);
}

// Decompress an ETC2 RGB block
// NO WARRANTY --- SEE STATEMENT IN TOP OF FILE (C) Ericsson AB 2013. All Rights
// Reserved.
void decompressBlockETC2c(unsigned int block_part1, unsigned int block_part2,
                          uint8* img, int width, int height, int startx,
                          int starty, int channels) {
  int diffbit;
  signed char color1[3];
  signed char diff[3];
  signed char red, green, blue;

  diffbit = (GETBITSHIGH(block_part1, 1, 33));

  if (diffbit) {
    // We have diffbit = 1;

    // Base color
    color1[0] = static_cast<signed char>(GETBITSHIGH(block_part1, 5, 63));
    color1[1] = static_cast<signed char>(GETBITSHIGH(block_part1, 5, 55));
    color1[2] = static_cast<signed char>(GETBITSHIGH(block_part1, 5, 47));

    // Diff color
    diff[0] = static_cast<signed char>(GETBITSHIGH(block_part1, 3, 58));
    diff[1] = static_cast<signed char>(GETBITSHIGH(block_part1, 3, 50));
    diff[2] = static_cast<signed char>(GETBITSHIGH(block_part1, 3, 42));

    // Extend sign bit to entire byte.
    diff[0] = (diff[0] << 5);
    diff[1] = (diff[1] << 5);
    diff[2] = (diff[2] << 5);
    diff[0] = diff[0] >> 5;
    diff[1] = diff[1] >> 5;
    diff[2] = diff[2] >> 5;

    red = color1[0] + diff[0];
    green = color1[1] + diff[1];
    blue = color1[2] + diff[2];

    if (red < 0 || red > 31) {
      unsigned int block59_part1, block59_part2;
      unstuff59bits(block_part1, block_part2, block59_part1, block59_part2);
      decompressBlockTHUMB59Tc(block59_part1, block59_part2, img, width, height,
                               startx, starty, channels);
    } else if (green < 0 || green > 31) {
      unsigned int block58_part1, block58_part2;
      unstuff58bits(block_part1, block_part2, block58_part1, block58_part2);
      decompressBlockTHUMB58Hc(block58_part1, block58_part2, img, width, height,
                               startx, starty, channels);
    } else if (blue < 0 || blue > 31) {
      unsigned int block57_part1, block57_part2;

      unstuff57bits(block_part1, block_part2, block57_part1, block57_part2);
      decompressBlockPlanar57c(block57_part1, block57_part2, img, width, height,
                               startx, starty, channels);
    } else {
      decompressBlockDiffFlipC(block_part1, block_part2, img, width, height,
                               startx, starty, channels);
    }
  } else {
    // We have diffbit = 0;
    decompressBlockDiffFlipC(block_part1, block_part2, img, width, height,
                             startx, starty, channels);
  }
}
void decompressBlockETC2(unsigned int block_part1, unsigned int block_part2,
                         uint8* img, int width, int height, int startx,
                         int starty) {
  decompressBlockETC2c(block_part1, block_part2, img, width, height, startx,
                       starty, 3);
}
// Decompress an ETC2 block with punchthrough alpha
// NO WARRANTY --- SEE STATEMENT IN TOP OF FILE (C) Ericsson AB 2013. All Rights
// Reserved.
void decompressBlockDifferentialWithAlphaC(unsigned int block_part1,
                                           unsigned int block_part2, uint8* img,
                                           uint8* alpha, int width, int height,
                                           int startx, int starty,
                                           int channelsRGB) {
  uint8 avg_color[3], enc_color1[3], enc_color2[3];
  signed char diff[3];
  int table;
  int index, shift;
  int r, g, b;
  int diffbit;
  int flipbit;
  int channelsA;

  if (channelsRGB == 3) {
    // We will decode the alpha data to a separate memory area.
    channelsA = 1;
  } else {
    // We will decode the RGB data and the alpha data to the same memory area,
    // interleaved as RGBA.
    channelsA = 4;
    alpha = &img[0 + 3];
  }

  // the diffbit now encodes whether or not the entire alpha channel is 255.
  diffbit = (GETBITSHIGH(block_part1, 1, 33));
  flipbit = (GETBITSHIGH(block_part1, 1, 32));

  // First decode left part of block.
  enc_color1[0] = static_cast<uint8>(GETBITSHIGH(block_part1, 5, 63));
  enc_color1[1] = static_cast<uint8>(GETBITSHIGH(block_part1, 5, 55));
  enc_color1[2] = static_cast<uint8>(GETBITSHIGH(block_part1, 5, 47));

  // Expand from 5 to 8 bits
  avg_color[0] = (enc_color1[0] << 3) | (enc_color1[0] >> 2);
  avg_color[1] = (enc_color1[1] << 3) | (enc_color1[1] >> 2);
  avg_color[2] = (enc_color1[2] << 3) | (enc_color1[2] >> 2);

  table = GETBITSHIGH(block_part1, 3, 39) << 1;

  unsigned int pixel_indices_MSB, pixel_indices_LSB;

  pixel_indices_MSB = GETBITS(block_part2, 16, 31);
  pixel_indices_LSB = GETBITS(block_part2, 16, 15);

  if ((flipbit) == 0) {
    // We should not flip
    shift = 0;
    for (int x = startx; x < startx + 2; x++) {
      for (int y = starty; y < starty + 4; y++) {
        index = ((pixel_indices_MSB >> shift) & 1) << 1;
        index |= ((pixel_indices_LSB >> shift) & 1);
        shift++;
        index = unscramble[index];

        int mod = compressParams[table][index];
        if (diffbit == 0 && (index == 1 || index == 2)) {
          mod = 0;
        }

        r = RED_CHANNEL(img, width, x, y, channelsRGB) =
            static_cast<uint8>(CLAMP(0, avg_color[0] + mod, 255));
        g = GREEN_CHANNEL(img, width, x, y, channelsRGB) =
            static_cast<uint8>(CLAMP(0, avg_color[1] + mod, 255));
        b = BLUE_CHANNEL(img, width, x, y, channelsRGB) =
            static_cast<uint8>(CLAMP(0, avg_color[2] + mod, 255));
        if (diffbit == 0 && index == 1) {
          alpha[(y * width + x) * channelsA] = 0;
          r = RED_CHANNEL(img, width, x, y, channelsRGB) = 0;
          g = GREEN_CHANNEL(img, width, x, y, channelsRGB) = 0;
          b = BLUE_CHANNEL(img, width, x, y, channelsRGB) = 0;
        } else {
          alpha[(y * width + x) * channelsA] = 255;
        }
      }
    }
  } else {
    // We should flip
    shift = 0;
    for (int x = startx; x < startx + 4; x++) {
      for (int y = starty; y < starty + 2; y++) {
        index = ((pixel_indices_MSB >> shift) & 1) << 1;
        index |= ((pixel_indices_LSB >> shift) & 1);
        shift++;
        index = unscramble[index];
        int mod = compressParams[table][index];
        if (diffbit == 0 && (index == 1 || index == 2)) {
          mod = 0;
        }
        r = RED_CHANNEL(img, width, x, y, channelsRGB) =
            static_cast<uint8>(CLAMP(0, avg_color[0] + mod, 255));
        g = GREEN_CHANNEL(img, width, x, y, channelsRGB) =
            static_cast<uint8>(CLAMP(0, avg_color[1] + mod, 255));
        b = BLUE_CHANNEL(img, width, x, y, channelsRGB) =
            static_cast<uint8>(CLAMP(0, avg_color[2] + mod, 255));
        if (diffbit == 0 && index == 1) {
          alpha[(y * width + x) * channelsA] = 0;
          r = RED_CHANNEL(img, width, x, y, channelsRGB) = 0;
          g = GREEN_CHANNEL(img, width, x, y, channelsRGB) = 0;
          b = BLUE_CHANNEL(img, width, x, y, channelsRGB) = 0;
        } else {
          alpha[(y * width + x) * channelsA] = 255;
        }
      }
      shift += 2;
    }
  }
  // Now decode right part of block.
  diff[0] = static_cast<signed char>(GETBITSHIGH(block_part1, 3, 58));
  diff[1] = static_cast<signed char>(GETBITSHIGH(block_part1, 3, 50));
  diff[2] = static_cast<signed char>(GETBITSHIGH(block_part1, 3, 42));

  // Extend sign bit to entire byte.
  diff[0] = (diff[0] << 5);
  diff[1] = (diff[1] << 5);
  diff[2] = (diff[2] << 5);
  diff[0] = diff[0] >> 5;
  diff[1] = diff[1] >> 5;
  diff[2] = diff[2] >> 5;

  //  Calculate second color
  enc_color2[0] = enc_color1[0] + diff[0];
  enc_color2[1] = enc_color1[1] + diff[1];
  enc_color2[2] = enc_color1[2] + diff[2];

  // Expand from 5 to 8 bits
  avg_color[0] = (enc_color2[0] << 3) | (enc_color2[0] >> 2);
  avg_color[1] = (enc_color2[1] << 3) | (enc_color2[1] >> 2);
  avg_color[2] = (enc_color2[2] << 3) | (enc_color2[2] >> 2);

  table = GETBITSHIGH(block_part1, 3, 36) << 1;
  pixel_indices_MSB = GETBITS(block_part2, 16, 31);
  pixel_indices_LSB = GETBITS(block_part2, 16, 15);

  if ((flipbit) == 0) {
    // We should not flip
    shift = 8;
    for (int x = startx + 2; x < startx + 4; x++) {
      for (int y = starty; y < starty + 4; y++) {
        index = ((pixel_indices_MSB >> shift) & 1) << 1;
        index |= ((pixel_indices_LSB >> shift) & 1);
        shift++;
        index = unscramble[index];
        int mod = compressParams[table][index];
        if (diffbit == 0 && (index == 1 || index == 2)) {
          mod = 0;
        }

        r = RED_CHANNEL(img, width, x, y, channelsRGB) =
            static_cast<uint8>(CLAMP(0, avg_color[0] + mod, 255));
        g = GREEN_CHANNEL(img, width, x, y, channelsRGB) =
            static_cast<uint8>(CLAMP(0, avg_color[1] + mod, 255));
        b = BLUE_CHANNEL(img, width, x, y, channelsRGB) =
            static_cast<uint8>(CLAMP(0, avg_color[2] + mod, 255));
        if (diffbit == 0 && index == 1) {
          alpha[(y * width + x) * channelsA] = 0;
          r = RED_CHANNEL(img, width, x, y, channelsRGB) = 0;
          g = GREEN_CHANNEL(img, width, x, y, channelsRGB) = 0;
          b = BLUE_CHANNEL(img, width, x, y, channelsRGB) = 0;
        } else {
          alpha[(y * width + x) * channelsA] = 255;
        }
      }
    }
  } else {
    // We should flip
    shift = 2;
    for (int x = startx; x < startx + 4; x++) {
      for (int y = starty + 2; y < starty + 4; y++) {
        index = ((pixel_indices_MSB >> shift) & 1) << 1;
        index |= ((pixel_indices_LSB >> shift) & 1);
        shift++;
        index = unscramble[index];
        int mod = compressParams[table][index];
        if (diffbit == 0 && (index == 1 || index == 2)) {
          mod = 0;
        }

        r = RED_CHANNEL(img, width, x, y, channelsRGB) =
            static_cast<uint8>(CLAMP(0, avg_color[0] + mod, 255));
        g = GREEN_CHANNEL(img, width, x, y, channelsRGB) =
            static_cast<uint8>(CLAMP(0, avg_color[1] + mod, 255));
        b = BLUE_CHANNEL(img, width, x, y, channelsRGB) =
            static_cast<uint8>(CLAMP(0, avg_color[2] + mod, 255));
        if (diffbit == 0 && index == 1) {
          alpha[(y * width + x) * channelsA] = 0;
          r = RED_CHANNEL(img, width, x, y, channelsRGB) = 0;
          g = GREEN_CHANNEL(img, width, x, y, channelsRGB) = 0;
          b = BLUE_CHANNEL(img, width, x, y, channelsRGB) = 0;
        } else {
          alpha[(y * width + x) * channelsA] = 255;
        }
      }
      shift += 2;
    }
  }
}
void decompressBlockDifferentialWithAlpha(unsigned int block_part1,
                                          unsigned int block_part2, uint8* img,
                                          uint8* alpha, int width, int height,
                                          int startx, int starty) {
  decompressBlockDifferentialWithAlphaC(block_part1, block_part2, img, alpha,
                                        width, height, startx, starty, 3);
}

// similar to regular decompression, but alpha channel is set to 0 if pixel
// index is 2, otherwise 255. NO WARRANTY --- SEE STATEMENT IN TOP OF FILE (C)
// Ericsson AB 2013. All Rights Reserved.
void decompressBlockTHUMB59TAlphaC(unsigned int block_part1,
                                   unsigned int block_part2, uint8* img,
                                   uint8* alpha, int width, int height,
                                   int startx, int starty, int channelsRGB) {
  uint8 colorsRGB444[2][3];
  uint8 colors[2][3];
  uint8 paint_colors[4][3];
  uint8 distance;
  uint8 block_mask[4][4];
  int channelsA;

  if (channelsRGB == 3) {
    // We will decode the alpha data to a separate memory area.
    channelsA = 1;
  } else {
    // We will decode the RGB data and the alpha data to the same memory area,
    // interleaved as RGBA.
    channelsA = 4;
    alpha = &img[0 + 3];
  }

  // First decode left part of block.
  colorsRGB444[0][R] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 58));
  colorsRGB444[0][G] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 54));
  colorsRGB444[0][B] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 50));

  colorsRGB444[1][R] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 46));
  colorsRGB444[1][G] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 42));
  colorsRGB444[1][B] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 38));

  distance = static_cast<uint8>(GETBITSHIGH(block_part1, TABLE_BITS_59T, 34));

  // Extend the two colors to RGB888
  decompressColor(R_BITS59T, G_BITS59T, B_BITS59T, colorsRGB444, colors);
  calculatePaintColors59T(distance, PATTERN_T, colors, paint_colors);

  // Choose one of the four paint colors for each texel
  for (uint8 x = 0; x < BLOCKWIDTH; ++x) {
    for (uint8 y = 0; y < BLOCKHEIGHT; ++y) {
      // block_mask[x][y] = GETBITS(block_part2,2,31-(y*4+x)*2);
      block_mask[x][y] =
          static_cast<uint8>(GETBITS(block_part2, 1, (y + x * 4) + 16) << 1);
      block_mask[x][y] |= GETBITS(block_part2, 1, (y + x * 4));
      img[channelsRGB * ((starty + y) * width + startx + x) + R] =
          static_cast<uint8>(
              CLAMP(0, paint_colors[block_mask[x][y]][R], 255));  // RED
      img[channelsRGB * ((starty + y) * width + startx + x) + G] =
          static_cast<uint8>(
              CLAMP(0, paint_colors[block_mask[x][y]][G], 255));  // GREEN
      img[channelsRGB * ((starty + y) * width + startx + x) + B] =
          static_cast<uint8>(
              CLAMP(0, paint_colors[block_mask[x][y]][B], 255));  // BLUE
      if (block_mask[x][y] == 2) {
        alpha[channelsA * (x + startx + (y + starty) * width)] = 0;
        img[channelsRGB * ((starty + y) * width + startx + x) + R] = 0;
        img[channelsRGB * ((starty + y) * width + startx + x) + G] = 0;
        img[channelsRGB * ((starty + y) * width + startx + x) + B] = 0;
      } else
        alpha[channelsA * (x + startx + (y + starty) * width)] = 255;
    }
  }
}
void decompressBlockTHUMB59TAlpha(unsigned int block_part1,
                                  unsigned int block_part2, uint8* img,
                                  uint8* alpha, int width, int height,
                                  int startx, int starty) {
  decompressBlockTHUMB59TAlphaC(block_part1, block_part2, img, alpha, width,
                                height, startx, starty, 3);
}

// Decompress an H-mode block with alpha
// NO WARRANTY --- SEE STATEMENT IN TOP OF FILE (C) Ericsson AB 2013. All Rights
// Reserved.
void decompressBlockTHUMB58HAlphaC(unsigned int block_part1,
                                   unsigned int block_part2, uint8* img,
                                   uint8* alpha, int width, int height,
                                   int startx, int starty, int channelsRGB) {
  unsigned int col0, col1;
  uint8 colors[2][3];
  uint8 colorsRGB444[2][3];
  uint8 paint_colors[4][3];
  uint8 distance;
  uint8 block_mask[4][4];
  int channelsA;

  if (channelsRGB == 3) {
    // We will decode the alpha data to a separate memory area.
    channelsA = 1;
  } else {
    // We will decode the RGB data and the alpha data to the same memory area,
    // interleaved as RGBA.
    channelsA = 4;
    alpha = &img[0 + 3];
  }

  // First decode left part of block.
  colorsRGB444[0][R] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 57));
  colorsRGB444[0][G] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 53));
  colorsRGB444[0][B] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 49));

  colorsRGB444[1][R] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 45));
  colorsRGB444[1][G] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 41));
  colorsRGB444[1][B] = static_cast<uint8>(GETBITSHIGH(block_part1, 4, 37));

  distance = 0;
  distance = static_cast<uint8>((GETBITSHIGH(block_part1, 2, 33)) << 1);

  col0 = GETBITSHIGH(block_part1, 12, 57);
  col1 = GETBITSHIGH(block_part1, 12, 45);

  if (col0 >= col1) {
    distance |= 1;
  }

  // Extend the two colors to RGB888
  decompressColor(R_BITS58H, G_BITS58H, B_BITS58H, colorsRGB444, colors);

  calculatePaintColors58H(distance, PATTERN_H, colors, paint_colors);

  // Choose one of the four paint colors for each texel
  for (uint8 x = 0; x < BLOCKWIDTH; ++x) {
    for (uint8 y = 0; y < BLOCKHEIGHT; ++y) {
      // block_mask[x][y] = GETBITS(block_part2,2,31-(y*4+x)*2);
      block_mask[x][y] =
          static_cast<uint8>(GETBITS(block_part2, 1, (y + x * 4) + 16) << 1);
      block_mask[x][y] |= GETBITS(block_part2, 1, (y + x * 4));
      img[channelsRGB * ((starty + y) * width + startx + x) + R] =
          static_cast<uint8>(
              CLAMP(0, paint_colors[block_mask[x][y]][R], 255));  // RED
      img[channelsRGB * ((starty + y) * width + startx + x) + G] =
          static_cast<uint8>(
              CLAMP(0, paint_colors[block_mask[x][y]][G], 255));  // GREEN
      img[channelsRGB * ((starty + y) * width + startx + x) + B] =
          static_cast<uint8>(
              CLAMP(0, paint_colors[block_mask[x][y]][B], 255));  // BLUE

      if (block_mask[x][y] == 2) {
        alpha[channelsA * (x + startx + (y + starty) * width)] = 0;
        img[channelsRGB * ((starty + y) * width + startx + x) + R] = 0;
        img[channelsRGB * ((starty + y) * width + startx + x) + G] = 0;
        img[channelsRGB * ((starty + y) * width + startx + x) + B] = 0;
      } else
        alpha[channelsA * (x + startx + (y + starty) * width)] = 255;
    }
  }
}
void decompressBlockTHUMB58HAlpha(unsigned int block_part1,
                                  unsigned int block_part2, uint8* img,
                                  uint8* alpha, int width, int height,
                                  int startx, int starty) {
  decompressBlockTHUMB58HAlphaC(block_part1, block_part2, img, alpha, width,
                                height, startx, starty, 3);
}
// Decompression function for ETC2_RGBA1 format.
// NO WARRANTY --- SEE STATEMENT IN TOP OF FILE (C) Ericsson AB 2013. All Rights
// Reserved.
void decompressBlockETC21BitAlphaC(unsigned int block_part1,
                                   unsigned int block_part2, uint8* img,
                                   uint8* alphaimg, int width, int height,
                                   int startx, int starty, int channelsRGB) {
  int diffbit;
  signed char color1[3];
  signed char diff[3];
  signed char red, green, blue;
  int channelsA;

  if (channelsRGB == 3) {
    // We will decode the alpha data to a separate memory area.
    channelsA = 1;
  } else {
    // We will decode the RGB data and the alpha data to the same memory area,
    // interleaved as RGBA.
    channelsA = 4;
    alphaimg = &img[0 + 3];
  }

  diffbit = (GETBITSHIGH(block_part1, 1, 33));

  if (diffbit) {
    // We have diffbit = 1, meaning no transparent pixels. regular
    // decompression.

    // Base color
    color1[0] = static_cast<signed char>(GETBITSHIGH(block_part1, 5, 63));
    color1[1] = static_cast<signed char>(GETBITSHIGH(block_part1, 5, 55));
    color1[2] = static_cast<signed char>(GETBITSHIGH(block_part1, 5, 47));

    // Diff color
    diff[0] = static_cast<signed char>(GETBITSHIGH(block_part1, 3, 58));
    diff[1] = static_cast<signed char>(GETBITSHIGH(block_part1, 3, 50));
    diff[2] = static_cast<signed char>(GETBITSHIGH(block_part1, 3, 42));

    // Extend sign bit to entire byte.
    diff[0] = (diff[0] << 5);
    diff[1] = (diff[1] << 5);
    diff[2] = (diff[2] << 5);
    diff[0] = diff[0] >> 5;
    diff[1] = diff[1] >> 5;
    diff[2] = diff[2] >> 5;

    red = color1[0] + diff[0];
    green = color1[1] + diff[1];
    blue = color1[2] + diff[2];

    if (red < 0 || red > 31) {
      unsigned int block59_part1, block59_part2;
      unstuff59bits(block_part1, block_part2, block59_part1, block59_part2);
      decompressBlockTHUMB59Tc(block59_part1, block59_part2, img, width, height,
                               startx, starty, channelsRGB);
    } else if (green < 0 || green > 31) {
      unsigned int block58_part1, block58_part2;
      unstuff58bits(block_part1, block_part2, block58_part1, block58_part2);
      decompressBlockTHUMB58Hc(block58_part1, block58_part2, img, width, height,
                               startx, starty, channelsRGB);
    } else if (blue < 0 || blue > 31) {
      unsigned int block57_part1, block57_part2;

      unstuff57bits(block_part1, block_part2, block57_part1, block57_part2);
      decompressBlockPlanar57c(block57_part1, block57_part2, img, width, height,
                               startx, starty, channelsRGB);
    } else {
      decompressBlockDifferentialWithAlphaC(block_part1, block_part2, img,
                                            alphaimg, width, height, startx,
                                            starty, channelsRGB);
    }
    for (int x = startx; x < startx + 4; x++) {
      for (int y = starty; y < starty + 4; y++) {
        alphaimg[channelsA * (x + y * width)] = 255;
      }
    }
  } else {
    // We have diffbit = 0, transparent pixels. Only T-, H- or regular diff-mode
    // possible.

    // Base color
    color1[0] = static_cast<signed char>(GETBITSHIGH(block_part1, 5, 63));
    color1[1] = static_cast<signed char>(GETBITSHIGH(block_part1, 5, 55));
    color1[2] = static_cast<signed char>(GETBITSHIGH(block_part1, 5, 47));

    // Diff color
    diff[0] = static_cast<signed char>(GETBITSHIGH(block_part1, 3, 58));
    diff[1] = static_cast<signed char>(GETBITSHIGH(block_part1, 3, 50));
    diff[2] = static_cast<signed char>(GETBITSHIGH(block_part1, 3, 42));

    // Extend sign bit to entire byte.
    diff[0] = (diff[0] << 5);
    diff[1] = (diff[1] << 5);
    diff[2] = (diff[2] << 5);
    diff[0] = diff[0] >> 5;
    diff[1] = diff[1] >> 5;
    diff[2] = diff[2] >> 5;

    red = color1[0] + diff[0];
    green = color1[1] + diff[1];
    blue = color1[2] + diff[2];
    if (red < 0 || red > 31) {
      unsigned int block59_part1, block59_part2;
      unstuff59bits(block_part1, block_part2, block59_part1, block59_part2);
      decompressBlockTHUMB59TAlphaC(block59_part1, block59_part2, img, alphaimg,
                                    width, height, startx, starty, channelsRGB);
    } else if (green < 0 || green > 31) {
      unsigned int block58_part1, block58_part2;
      unstuff58bits(block_part1, block_part2, block58_part1, block58_part2);
      decompressBlockTHUMB58HAlphaC(block58_part1, block58_part2, img, alphaimg,
                                    width, height, startx, starty, channelsRGB);
    } else if (blue < 0 || blue > 31) {
      unsigned int block57_part1, block57_part2;

      unstuff57bits(block_part1, block_part2, block57_part1, block57_part2);
      decompressBlockPlanar57c(block57_part1, block57_part2, img, width, height,
                               startx, starty, channelsRGB);
      for (int x = startx; x < startx + 4; x++) {
        for (int y = starty; y < starty + 4; y++) {
          alphaimg[channelsA * (x + y * width)] = 255;
        }
      }
    } else
      decompressBlockDifferentialWithAlphaC(block_part1, block_part2, img,
                                            alphaimg, width, height, startx,
                                            starty, channelsRGB);
  }
}
void decompressBlockETC21BitAlpha(unsigned int block_part1,
                                  unsigned int block_part2, uint8* img,
                                  uint8* alphaimg, int width, int height,
                                  int startx, int starty) {
  decompressBlockETC21BitAlphaC(block_part1, block_part2, img, alphaimg, width,
                                height, startx, starty, 3);
}
//
//	Utility functions used for alpha compression
//

// bit number frompos is extracted from input, and moved to bit number topos in
// the return value. NO WARRANTY --- SEE STATEMENT IN TOP OF FILE (C) Ericsson
// AB 2013. All Rights Reserved.
auto getbit(uint8 input, int frompos, int topos) -> uint8 {
  // uint8 output=0;
  if (frompos > topos)
    return static_cast<uint8>(((1 << frompos) & input) >> (frompos - topos));
  return static_cast<uint8>(((1 << frompos) & input) << (topos - frompos));
}

// takes as input a value, returns the value clamped to the interval [0,255].
// NO WARRANTY --- SEE STATEMENT IN TOP OF FILE (C) Ericsson AB 2013. All Rights
// Reserved.
auto clamp(int val) -> int {
  if (val < 0) val = 0;
  if (val > 255) val = 255;
  return val;
}

// Decodes tha alpha component in a block coded with
// GL_COMPRESSED_RGBA8_ETC2_EAC. Note that this decoding is slightly different
// from that of GL_COMPRESSED_R11_EAC. However, a hardware decoder can share
// gates between the two formats as explained in the specification under
// GL_COMPRESSED_R11_EAC. NO WARRANTY --- SEE STATEMENT IN TOP OF FILE (C)
// Ericsson AB 2013. All Rights Reserved.
void decompressBlockAlphaC(uint8* data, uint8* img, int width, int height,
                           int ix, int iy, int channels) {
  int alpha = data[0];
  int table = data[1];

  int bit = 0;
  int byte = 2;
  // extract an alpha value for each pixel.
  for (int x = 0; x < 4; x++) {
    for (int y = 0; y < 4; y++) {
      // Extract table index
      int index = 0;
      for (int bitpos = 0; bitpos < 3; bitpos++) {
        index |= getbit(data[byte], 7 - bit, 2 - bitpos);
        bit++;
        if (bit > 7) {
          bit = 0;
          byte++;
        }
      }
      img[(ix + x + (iy + y) * width) * channels] =
          static_cast<uint8>(clamp(alpha + alphaTable[table][index]));
    }
  }
}
void decompressBlockAlpha(uint8* data, uint8* img, int width, int height,
                          int ix, int iy) {
  decompressBlockAlphaC(data, img, width, height, ix, iy, 1);
}

// Does decompression and then immediately converts from 11 bit signed to a
// 16-bit format.
//
// NO WARRANTY --- SEE STATEMENT IN TOP OF FILE (C) Ericsson AB 2013. All Rights
// Reserved.
auto get16bits11signed(int base, int table, int mul, int index) -> int16 {
  int elevenbase = base - 128;
  if (elevenbase == -128) elevenbase = -127;
  elevenbase *= 8;
  // i want the positive value here
  int tabVal = -alphaBase[table][3 - index % 4] - 1;
  // and the sign, please
  int sign = 1 - (index / 4);

  if (sign) tabVal = tabVal + 1;
  int elevenTabVal = tabVal * 8;

  if (mul != 0)
    elevenTabVal *= mul;
  else
    elevenTabVal /= 8;

  if (sign) elevenTabVal = -elevenTabVal;

  // calculate sum
  int elevenbits = elevenbase + elevenTabVal;

  // clamp..
  if (elevenbits >= 1024)
    elevenbits = 1023;
  else if (elevenbits < -1023)
    elevenbits = -1023;
  // this is the value we would actually output..
  // but there aren't any good 11-bit file or uncompressed GL formats
  // so we extend to 15 bits signed.
  sign = elevenbits < 0;
  elevenbits = abs(elevenbits);
  auto fifteenbits = static_cast<int16>((elevenbits << 5) + (elevenbits >> 5));
  auto sixteenbits = static_cast<int16>(fifteenbits);

  if (sign) sixteenbits = -sixteenbits;

  return sixteenbits;
}

// Does decompression and then immediately converts from 11 bit signed to a
// 16-bit format Calculates the 11 bit value represented by base, table, mul and
// index, and extends it to 16 bits. NO WARRANTY --- SEE STATEMENT IN TOP OF
// FILE (C) Ericsson AB 2013. All Rights Reserved.
auto get16bits11bits(int base, int table, int mul, int index) -> uint16 {
  int elevenbase = base * 8 + 4;

  // i want the positive value here
  int tabVal = -alphaBase[table][3 - index % 4] - 1;
  // and the sign, please
  int sign = 1 - (index / 4);

  if (sign) tabVal = tabVal + 1;
  int elevenTabVal = tabVal * 8;

  if (mul != 0)
    elevenTabVal *= mul;
  else
    elevenTabVal /= 8;

  if (sign) elevenTabVal = -elevenTabVal;

  // calculate sum
  int elevenbits = elevenbase + elevenTabVal;

  // clamp..
  if (elevenbits >= 256 * 8)
    elevenbits = 256 * 8 - 1;
  else if (elevenbits < 0)
    elevenbits = 0;
  // elevenbits now contains the 11 bit alpha value as defined in the spec.

  // extend to 16 bits before returning, since we don't have any good 11-bit
  // file formats.
  auto sixteenbits = static_cast<uint16>((elevenbits << 5) + (elevenbits >> 6));

  return sixteenbits;
}

// Decompresses a block using one of the GL_COMPRESSED_R11_EAC or
// GL_COMPRESSED_SIGNED_R11_EAC-formats NO WARRANTY --- SEE STATEMENT IN TOP OF
// FILE (C) Ericsson AB 2013. All Rights Reserved.
void decompressBlockAlpha16bitC(uint8* data, uint8* img, int width, int height,
                                int ix, int iy, int channels) {
  int alpha = data[0];
  int table = data[1];

  if (formatSigned) {
    // if we have a signed format, the base value is given as a signed byte. We
    // convert it to (0-255) here, so more code can be shared with the unsigned
    // mode.
    alpha = *((signed char*)(&data[0]));
    alpha = alpha + 128;
  }

  int bit = 0;
  int byte = 2;
  // extract an alpha value for each pixel.
  for (int x = 0; x < 4; x++) {
    for (int y = 0; y < 4; y++) {
      // Extract table index
      int index = 0;
      for (int bitpos = 0; bitpos < 3; bitpos++) {
        index |= getbit(data[byte], 7 - bit, 2 - bitpos);
        bit++;
        if (bit > 7) {
          bit = 0;
          byte++;
        }
      }
      int windex = channels * (2 * (ix + x + (iy + y) * width));
#if !PGMOUT
      if (formatSigned) {
        *(int16*)&img[windex] =
            get16bits11signed(alpha, (table % 16), (table / 16), index);
      } else {
        *(uint16*)&img[windex] =
            get16bits11bits(alpha, (table % 16), (table / 16), index);
      }
#else
      // make data compatible with the .pgm format. See the comment in
      // compressBlockAlpha16() for details.
      uint16 uSixteen;
      if (formatSigned) {
        // the pgm-format only allows unsigned images,
        // so we add 2^15 to get a 16-bit value.
        uSixteen = get16bits11signed(alpha, (table % 16), (table / 16), index)
                   + 256 * 128;
      } else {
        uSixteen = get16bits11bits(alpha, (table % 16), (table / 16), index);
      }
      // byte swap for pgm
      img[windex] = uSixteen / 256;
      img[windex + 1] = uSixteen % 256;
#endif
    }
  }
}

void decompressBlockAlpha16bit(uint8* data, uint8* img, int width, int height,
                               int ix, int iy) {
  decompressBlockAlpha16bitC(data, img, width, height, ix, iy, 1);
}

static void readBigEndian4byteWord(uint32_t* pBlock, const GLubyte* s) {
  *pBlock = (s[0] << 24) | (s[1] << 16) | (s[2] << 8) | s[3];
}

void KTXUnpackETC(const GLubyte* srcETC, const GLenum srcFormat,
                  uint32_t activeWidth, uint32_t activeHeight,
                  GLubyte** dstImage, GLenum* format, GLenum* internal_format,
                  GLenum* type, GLint R16Formats, bool supportsSRGB) {
  unsigned int width, height;
  unsigned int block_part1, block_part2;
  unsigned int x, y;
  /*const*/ auto* src = (GLubyte*)srcETC;
  // AF_11BIT is used to compress R11 & RG11 though its not alpha data.
  enum { AF_NONE, AF_1BIT, AF_8BIT, AF_11BIT } alphaFormat = AF_NONE;
  int dstChannels, dstChannelBytes;

  switch (srcFormat) {
      // case GL_COMPRESSED_SIGNED_R11_EAC:
      //   if (R16Formats & _KTX_R16_FORMATS_SNORM) {
      //   	dstChannelBytes = sizeof(GLshort);
      //   	dstChannels = 1;
      //   	formatSigned = GL_TRUE;
      //   	*internal_format = GL_R16_SNORM;
      //   	*format = GL_RED;
      //   	*type = GL_SHORT;
      //   	alphaFormat = AF_11BIT;
      //   } else
      //   	return KTX_UNSUPPORTED_TEXTURE_TYPE;
      //   break;

      // case GL_COMPRESSED_R11_EAC:
      //   if (R16Formats & _KTX_R16_FORMATS_NORM) {
      //   	dstChannelBytes = sizeof(GLshort);
      //   	dstChannels = 1;
      //   	formatSigned = GL_FALSE;
      //   	*internal_format = GL_R16;
      //   	*format = GL_RED;
      //   	*type = GL_UNSIGNED_SHORT;
      //   	alphaFormat = AF_11BIT;
      //   } else
      //   	return KTX_UNSUPPORTED_TEXTURE_TYPE;
      //   break;

      // case GL_COMPRESSED_SIGNED_RG11_EAC:
      //   if (R16Formats & _KTX_R16_FORMATS_SNORM) {
      //   	dstChannelBytes = sizeof(GLshort);
      //   	dstChannels = 2;
      //   	formatSigned = GL_TRUE;
      //   	*internal_format = GL_RG16_SNORM;
      //   	*format = GL_RG;
      //   	*type = GL_SHORT;
      //   	alphaFormat = AF_11BIT;
      //   } else
      //   	return KTX_UNSUPPORTED_TEXTURE_TYPE;
      //   break;

      // case GL_COMPRESSED_RG11_EAC:
      //   if (R16Formats & _KTX_R16_FORMATS_NORM) {
      //   	dstChannelBytes = sizeof(GLshort);
      //   	dstChannels = 2;
      //   	formatSigned = GL_FALSE;
      //   	*internal_format = GL_RG16;
      //   	*format = GL_RG;
      //   	*type = GL_UNSIGNED_SHORT;
      //   	alphaFormat = AF_11BIT;
      //   } else
      //   	return KTX_UNSUPPORTED_TEXTURE_TYPE;
      //   break;

    case GL_ETC1_RGB8_OES:
    case GL_COMPRESSED_RGB8_ETC2:
      dstChannelBytes = sizeof(GLubyte);
      dstChannels = 3;
      //*internal_format = GL_RGB8;
      *format = GL_RGB;
      *type = GL_UNSIGNED_BYTE;
      break;

    case GL_COMPRESSED_RGBA8_ETC2_EAC:
      dstChannelBytes = sizeof(GLubyte);
      dstChannels = 4;
      //*internal_format = GL_RGBA8;
      *format = GL_RGBA;
      *type = GL_UNSIGNED_BYTE;
      alphaFormat = AF_8BIT;
      break;

      // case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
      //   dstChannelBytes = sizeof(GLubyte);
      //   dstChannels = 4;
      //   *internal_format = GL_RGBA8;
      //   *format = GL_RGBA;
      //   *type = GL_UNSIGNED_BYTE;
      //   alphaFormat = AF_1BIT;
      //   break;

      // case GL_COMPRESSED_SRGB8_ETC2:
      //   if (supportsSRGB) {
      //   	dstChannelBytes = sizeof(GLubyte);
      //   	dstChannels = 3;
      //   	*internal_format = GL_SRGB8;
      //   	*format = GL_RGB;
      //   	*type = GL_UNSIGNED_BYTE;
      //   } else
      //   	return KTX_UNSUPPORTED_TEXTURE_TYPE;
      //   break;

      // case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
      //   if (supportsSRGB) {
      //   	dstChannelBytes = sizeof(GLubyte);
      //   	dstChannels = 4;
      //   	*internal_format = GL_SRGB8_ALPHA8;
      //   	*format = GL_RGBA;
      //   	*type = GL_UNSIGNED_BYTE;
      //   	alphaFormat = AF_8BIT;
      //   } else
      //   	return KTX_UNSUPPORTED_TEXTURE_TYPE;
      //   break;

      // case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
      //   if (supportsSRGB) {
      //   	dstChannelBytes = sizeof(GLubyte);
      //   	dstChannels = 4;
      //   	*internal_format = GL_SRGB8_ALPHA8;
      //   	*format = GL_RGBA;
      //   	*type = GL_UNSIGNED_BYTE;
      //   	alphaFormat = AF_1BIT;
      //   } else
      //   	return KTX_UNSUPPORTED_TEXTURE_TYPE;
      //   break;

    default:
      throw Exception();
      // assert(0);  // Upper levels should be passing only one of the above
      // srcFormats.
  }

  /* active_{width,height} show how many pixels contain active data,
   * (the rest are just for making sure we have a 2*a x 4*b size).
   */

  /* Compute the full width & height. */
  width = ((activeWidth + 3) / 4) * 4;
  height = ((activeHeight + 3) / 4) * 4;

  /* printf("Width = %d, Height = %d\n", width, height); */
  /* printf("active pixel area: top left %d x %d area.\n", activeWidth,
   * activeHeight); */

  *dstImage = (GLubyte*)malloc(dstChannels * dstChannelBytes * width * height);
  if (!*dstImage) {
    throw Exception();
    // return KTX_OUT_OF_MEMORY;
  }

  if (alphaFormat != AF_NONE) setupAlphaTable();

#pragma clang diagnostic push
#pragma ide diagnostic ignored "ConstantConditionsOC"
#pragma ide diagnostic ignored "UnreachableCode"

  // NOTE: none of the decompress functions actually use the <height> parameter
  if (alphaFormat == AF_11BIT) {
    throw Exception();
    //    // One or two 11-bit alpha channels for R or RG.
    //    for (y = 0; y < height / 4; y++) {
    //      for (x = 0; x < width / 4; x++) {
    //        decompressBlockAlpha16bitC(src, *dstImage, width, height, 4 * x, 4
    //        * y,
    //                                   dstChannels);
    //        src += 8;
    //        // if (srcFormat == GL_COMPRESSED_RG11_EAC || srcFormat ==
    //        // GL_COMPRESSED_SIGNED_RG11_EAC) {
    //        decompressBlockAlpha16bitC(src,
    //        // *dstImage + dstChannelBytes, width, height, 4*x, 4*y,
    //        dstChannels);
    //        //   src += 8;
    //        // }
    //      }
    //    }
  } else {
    for (y = 0; y < height / 4; y++) {
      for (x = 0; x < width / 4; x++) {
        // Decode alpha channel for RGBA
        if (alphaFormat == AF_8BIT) {
          decompressBlockAlphaC(src, *dstImage + 3, width, height, 4 * x, 4 * y,
                                dstChannels);
          src += 8;
        }
        // Decode color dstChannels
        readBigEndian4byteWord(&block_part1, src);
        src += 4;
        readBigEndian4byteWord(&block_part2, src);
        src += 4;
        if (alphaFormat == AF_1BIT)
          decompressBlockETC21BitAlphaC(block_part1, block_part2, *dstImage,
                                        nullptr, width, height, 4 * x, 4 * y,
                                        dstChannels);
        else
          decompressBlockETC2c(block_part1, block_part2, *dstImage, width,
                               height, 4 * x, 4 * y, dstChannels);
      }
    }
  }

#pragma clang diagnostic pop

  /* Ok, now write out the active pixels to the destination image.
   * (But only if the active pixels differ from the total pixels)
   */

  if (!(height == activeHeight && width == activeWidth)) {
    int dstPixelBytes = dstChannels * dstChannelBytes;
    int dstRowBytes = dstPixelBytes * width;
    int activeRowBytes = activeWidth * dstPixelBytes;
    auto* newimg = (GLubyte*)malloc(dstPixelBytes * activeWidth * activeHeight);
    unsigned int xx, yy;
    int zz;

    if (!newimg) {
      free(*dstImage);
      // return KTX_OUT_OF_MEMORY;
      throw Exception();
    }

    /* Convert from total area to active area: */

    for (yy = 0; yy < activeHeight; yy++) {
      for (xx = 0; xx < activeWidth; xx++) {
        for (zz = 0; zz < dstPixelBytes; zz++) {
          // NOLINTNEXTLINE
          newimg[yy * activeRowBytes + xx * dstPixelBytes + zz] =
              (*dstImage)[yy * dstRowBytes + xx * dstPixelBytes + zz];
        }
      }
    }

    free(*dstImage);
    *dstImage = newimg;
  }
}

}  // namespace ballistica::base
