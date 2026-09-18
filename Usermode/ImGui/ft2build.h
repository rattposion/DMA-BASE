#pragma once

struct FT_LibraryRec_;
struct FT_FaceRec_;
struct FT_GlyphSlotRec_;
struct FT_SizeRec_;
struct FT_MemoryRec_;

typedef struct FT_LibraryRec_* FT_Library;
typedef struct FT_FaceRec_* FT_Face;
typedef struct FT_GlyphSlotRec_* FT_GlyphSlot;
typedef struct FT_SizeRec_* FT_Size;
typedef struct FT_MemoryRec_* FT_Memory;

typedef long FT_Long;
typedef unsigned long FT_ULong;
typedef int FT_Error;
typedef unsigned int FT_UInt;
typedef unsigned char FT_Byte;
typedef signed char FT_Char;
typedef unsigned short FT_UShort;
typedef signed short FT_Short;
typedef signed int FT_Int;
typedef signed int FT_Int32;
typedef unsigned long FT_Fixed;
typedef unsigned int FT_Glyph_Format;
typedef unsigned long FT_Tag;
typedef char* FT_String;
typedef long FT_Pos;
typedef int FT_Render_Mode;

#define FT_LOAD_DEFAULT 0x0000
#define FT_LOAD_NO_SCALE 0x0001
#define FT_LOAD_NO_HINTING 0x0002
#define FT_LOAD_RENDER 0x0004
#define FT_LOAD_NO_BITMAP 0x0008
#define FT_LOAD_VERTICAL_LAYOUT 0x0010
#define FT_LOAD_FORCE_AUTOHINT 0x0020
#define FT_LOAD_CROP_BITMAP 0x0040
#define FT_LOAD_PEDANTIC 0x0080
#define FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH 0x0100
#define FT_LOAD_NO_RECURSE 0x0200
#define FT_LOAD_IGNORE_TRANSFORM 0x0400
#define FT_LOAD_MONOCHROME 0x0800
#define FT_LOAD_LINEAR_DESIGN 0x1000
#define FT_LOAD_NO_AUTOHINT 0x2000

#define FT_LOAD_TARGET_LIGHT 0x4000
#define FT_LOAD_TARGET_MONO 0x8000
#define FT_LOAD_TARGET_NORMAL 0x10000
#define FT_LOAD_COLOR 0x20000

#define FT_MAKE_TAG(ch1, ch2, ch3, ch4) \
    ((FT_Tag)(((FT_ULong)(ch1) << 24) | ((FT_ULong)(ch2) << 16) | ((FT_ULong)(ch3) << 8) | (FT_ULong)(ch4)))

#define FT_GLYPH_FORMAT_NONE       0
#define FT_GLYPH_FORMAT_COMPOSITE  FT_MAKE_TAG('c', 'o', 'm', 'p')
#define FT_GLYPH_FORMAT_BITMAP     FT_MAKE_TAG('b', 'i', 't', 'm')
#define FT_GLYPH_FORMAT_OUTLINE    FT_MAKE_TAG('o', 'u', 't', 'l')
#define FT_GLYPH_FORMAT_PLOTTER    FT_MAKE_TAG('p', 'l', 'o', 't')
#define FT_GLYPH_FORMAT_SVG        FT_MAKE_TAG('S', 'V', 'G', ' ')

#define FREETYPE_MAJOR 2
#define FREETYPE_MINOR 10
#define FREETYPE_PATCH 0

#define FT_Err_Ok 0
#define FT_Err_Cannot_Open_Resource 1
#define FT_Err_Unknown_File_Format 2
#define FT_Err_Invalid_File_Format 3
#define FT_Err_Invalid_Version 4
#define FT_Err_Lower_Module_Version 5
#define FT_Err_Invalid_Argument 6
#define FT_Err_Unimplemented_Feature 7
#define FT_Err_Invalid_Table 8
#define FT_Err_Invalid_Offset 9
#define FT_Err_Array_Too_Large 10
#define FT_Err_Missing_Module 11
#define FT_Err_Missing_Property 12

typedef struct FT_Vector_ {
    long x;
    long y;
} FT_Vector;

typedef struct FT_BBox_ {
    long xMin, yMin;
    long xMax, yMax;
} FT_BBox;

typedef struct FT_Bitmap_ {
    unsigned int rows;
    unsigned int width;
    int pitch;
    unsigned char* buffer;
    unsigned short num_grays;
    unsigned char pixel_mode;
    unsigned char palette_mode;
    void* palette;
} FT_Bitmap;

typedef struct FT_Size_Metrics_ {
    FT_UShort x_ppem;
    FT_UShort y_ppem;
    FT_Long x_scale;
    FT_Long y_scale;
    FT_Long ascender;
    FT_Long descender;
    FT_Long height;
    FT_Long max_advance;
} FT_Size_Metrics;

typedef struct FT_Glyph_Metrics_ {
    FT_Pos width;
    FT_Pos height;
    FT_Pos horiBearingX;
    FT_Pos horiBearingY;
    FT_Pos horiAdvance;
    FT_Pos vertBearingX;
    FT_Pos vertBearingY;
    FT_Pos vertAdvance;
} FT_Glyph_Metrics;

typedef struct FT_Outline_ {
    short n_contours;
    short n_points;
    FT_Vector* points;
    char* tags;
    short* contours;
    int flags;
} FT_Outline;

#define FT_SIZE_REQUEST_TYPE_NOMINAL 0
#define FT_SIZE_REQUEST_TYPE_REAL_DIM 1
#define FT_SIZE_REQUEST_TYPE_BBOX 2
#define FT_SIZE_REQUEST_TYPE_CELL 3
#define FT_SIZE_REQUEST_TYPE_SCALES 4
#define FT_SIZE_REQUEST_TYPE_MAX 5

typedef struct FT_Size_RequestRec_ {
    int type;
    FT_Long width;
    FT_Long height;
    FT_UInt horzResolution;
    FT_UInt vertResolution;
    FT_UInt horiResolution;
} FT_Size_RequestRec, *FT_Size_Request;

#define FT_PIXEL_MODE_NONE 0
#define FT_PIXEL_MODE_MONO 1
#define FT_PIXEL_MODE_GRAY 2
#define FT_PIXEL_MODE_GRAY2 3
#define FT_PIXEL_MODE_GRAY4 4
#define FT_PIXEL_MODE_LCD 5
#define FT_PIXEL_MODE_LCD_V 6
#define FT_PIXEL_MODE_BGRA 7
#define FT_PIXEL_MODE_MAX 8

#define FT_ENCODING_NONE 0
#define FT_ENCODING_UNICODE 1

#define FT_RENDER_MODE_NORMAL 0
#define FT_RENDER_MODE_MONO 1
#define FT_RENDER_MODE_LCD 2
#define FT_RENDER_MODE_LCD_V 3

#define FT_FREETYPE_H "ft2build.h"
#define FT_MODULE_H "ft2build.h"
#define FT_GLYPH_H "ft2build.h"
#define FT_SIZES_H "ft2build.h"
#define FT_SYNTHESIS_H "ft2build.h"
#define FT_OTSVG_H "ft2build.h"
#define FT_BBOX_H "ft2build.h"

extern "C" {
    int FT_Init_FreeType(FT_Library* alibrary);
    int FT_Done_FreeType(FT_Library library);
    int FT_New_Face(FT_Library library, const char* filepathname, long face_index, FT_Face* aface);
    int FT_New_Memory_Face(FT_Library library, const FT_Byte* file_base, FT_Long file_size, FT_Long face_index, FT_Face* aface);
    int FT_Done_Face(FT_Face face);
    int FT_Set_Char_Size(FT_Face face, long char_width, long char_height, unsigned int horz_resolution, unsigned int vert_resolution);
    int FT_Load_Char(FT_Face face, unsigned long char_code, int load_flags);
    int FT_Load_Glyph(FT_Face face, FT_UInt glyph_index, int load_flags);
    int FT_Get_Glyph(FT_GlyphSlot slot, void* aglyph);
    int FT_Glyph_To_Bitmap(void* the_glyph, int render_mode, FT_Vector* origin, int destroy);
    void FT_Done_Glyph(void* glyph);
    int FT_Select_Charmap(FT_Face face, int encoding);
    int FT_New_Library(FT_Memory memory, FT_Library* alibrary);
    int FT_Add_Default_Modules(FT_Library library);
    int FT_Done_Library(FT_Library library);
    int FT_New_Size(FT_Face face, FT_Size* asize);
    int FT_Activate_Size(FT_Size size);
    int FT_Request_Size(FT_Face face, FT_Size_Request req);
    int FT_Done_Size(FT_Size size);
    FT_UInt FT_Get_Char_Index(FT_Face face, FT_ULong charcode);
    int FT_Render_Glyph(FT_GlyphSlot slot, int render_mode);
    int FT_GlyphSlot_Embolden(FT_GlyphSlot slot);
    int FT_GlyphSlot_Oblique(FT_GlyphSlot slot);
}

struct FT_LibraryRec_ {
    void* data;
};

struct FT_FaceRec_ {
    void* data;
    FT_GlyphSlot glyph;
};

struct FT_GlyphSlotRec_ {
    FT_Library library;
    FT_Face face;
    FT_GlyphSlot next;
    FT_UInt reserved;
    void* generic;
    FT_Glyph_Metrics metrics;
    FT_Fixed linearHoriAdvance;
    FT_Fixed linearVertAdvance;
    FT_Vector advance;
    FT_Glyph_Format format;
    FT_Bitmap bitmap;
    FT_Int bitmap_left;
    FT_Int bitmap_top;
    FT_Outline outline;
    FT_UInt num_subglyphs;
    void* subglyphs;
    void* control_data;
    long control_len;
    FT_Pos lsb_delta;
    FT_Pos rsb_delta;
    void* other;
    void* internal;
};

struct FT_SizeRec_ {
    FT_Face face;
    void* generic;
    FT_Size_Metrics metrics;
    void* internal;
};

struct FT_MemoryRec_ {
    void* user;
    void* (*alloc)(FT_Memory memory, long size);
    void (*free)(FT_Memory memory, void* block);
    void* (*realloc)(FT_Memory memory, long cur_size, long new_size, void* block);
};
