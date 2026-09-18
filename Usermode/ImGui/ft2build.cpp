#include "ft2build.h"

extern "C" {

int FT_Init_FreeType(FT_Library* alibrary) {
    if (alibrary) {
        *alibrary = new FT_LibraryRec_;
        (*alibrary)->data = nullptr;
    }
    return FT_Err_Ok;
}

int FT_Done_FreeType(FT_Library library) {
    if (library) {
        delete library;
    }
    return FT_Err_Ok;
}

int FT_New_Face(FT_Library library, const char* filepathname, long face_index, FT_Face* aface) {
    if (aface) {
        *aface = new FT_FaceRec_;
        (*aface)->data = nullptr;
        (*aface)->glyph = nullptr;
    }
    return FT_Err_Ok;
}

int FT_New_Memory_Face(FT_Library library, const FT_Byte* file_base, FT_Long file_size, FT_Long face_index, FT_Face* aface) {
    if (aface) {
        *aface = new FT_FaceRec_;
        (*aface)->data = nullptr;
        (*aface)->glyph = nullptr;
    }
    return FT_Err_Ok;
}

int FT_Done_Face(FT_Face face) {
    if (face) {
        delete face;
    }
    return FT_Err_Ok;
}

int FT_Set_Char_Size(FT_Face face, long char_width, long char_height, unsigned int horz_resolution, unsigned int vert_resolution) {
    return FT_Err_Ok;
}

int FT_Load_Char(FT_Face face, unsigned long char_code, int load_flags) {
    return FT_Err_Ok;
}

int FT_Load_Glyph(FT_Face face, FT_UInt glyph_index, int load_flags) {
    return FT_Err_Ok;
}

int FT_Get_Glyph(FT_GlyphSlot slot, void* aglyph) {
    return FT_Err_Ok;
}

int FT_Glyph_To_Bitmap(void* the_glyph, int render_mode, FT_Vector* origin, int destroy) {
    return FT_Err_Ok;
}

void FT_Done_Glyph(void* glyph) {
    // No-op
}

int FT_Select_Charmap(FT_Face face, int encoding) {
    return FT_Err_Ok;
}

int FT_New_Library(FT_Memory memory, FT_Library* alibrary) {
    if (alibrary) {
        *alibrary = new FT_LibraryRec_;
        (*alibrary)->data = nullptr;
    }
    return FT_Err_Ok;
}

int FT_Add_Default_Modules(FT_Library library) {
    return FT_Err_Ok;
}

int FT_Done_Library(FT_Library library) {
    if (library) {
        delete library;
    }
    return FT_Err_Ok;
}

int FT_New_Size(FT_Face face, FT_Size* asize) {
    if (asize) {
        *asize = new FT_SizeRec_;
        (*asize)->face = face;
        (*asize)->generic = nullptr;
        (*asize)->internal = nullptr;
    }
    return FT_Err_Ok;
}

int FT_Activate_Size(FT_Size size) {
    return FT_Err_Ok;
}

int FT_Request_Size(FT_Face face, FT_Size_Request req) {
    return FT_Err_Ok;
}

int FT_Done_Size(FT_Size size) {
    if (size) {
        delete size;
    }
    return FT_Err_Ok;
}

FT_UInt FT_Get_Char_Index(FT_Face face, FT_ULong charcode) {
    return 1; // Return a default glyph index
}

int FT_Render_Glyph(FT_GlyphSlot slot, int render_mode) {
    return FT_Err_Ok;
}

int FT_GlyphSlot_Embolden(FT_GlyphSlot slot) {
    return FT_Err_Ok;
}

int FT_GlyphSlot_Oblique(FT_GlyphSlot slot) {
    return FT_Err_Ok;
}

}


