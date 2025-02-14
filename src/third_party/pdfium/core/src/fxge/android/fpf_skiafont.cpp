// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
 
// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fx_fpf.h"
#if _FX_OS_ == _FX_ANDROID_
#include "fpf_skiafont.h"
#include "fpf_skiafontmgr.h"
#define FPF_EM_ADJUST(em, a) (em == 0 ? (a) : (a) * 1000 / em)
CFPF_SkiaFont::CFPF_SkiaFont()
    : m_pFontMgr(NULL)
    , m_pFontDes(NULL)
    , m_Face(NULL)
    , m_dwStyle(0)
    , m_uCharset(0)
    , m_dwRefCount(0)
{
}
CFPF_SkiaFont::~CFPF_SkiaFont()
{
    if (m_Face) {
        FXFT_Done_Face(m_Face);
    }
}
void CFPF_SkiaFont::Release()
{
    if (--m_dwRefCount == 0) {
        delete this;
    }
}
IFPF_Font* CFPF_SkiaFont::Retain()
{
    m_dwRefCount++;
    return (IFPF_Font*)this;
}
FPF_HFONT CFPF_SkiaFont::GetHandle()
{
    return NULL;
}
CFX_ByteString CFPF_SkiaFont::GetFamilyName()
{
    if (!m_Face) {
        return CFX_ByteString();
    }
    return CFX_ByteString(FXFT_Get_Face_Family_Name(m_Face));
}
CFX_WideString CFPF_SkiaFont::GetPsName()
{
    if (!m_Face) {
        return CFX_WideString();
    }
    return CFX_WideString::FromLocal(FXFT_Get_Postscript_Name(m_Face));
}
FX_INT32 CFPF_SkiaFont::GetGlyphIndex(FX_WCHAR wUnicode)
{
    if (!m_Face) {
        return wUnicode;
    }
    if (FXFT_Select_Charmap(m_Face, FXFT_ENCODING_UNICODE)) {
        return 0;
    }
    return FXFT_Get_Char_Index(m_Face, wUnicode);
}
FX_INT32 CFPF_SkiaFont::GetGlyphWidth(FX_INT32 iGlyphIndex)
{
    if (!m_Face) {
        return 0;
    }
    if (FXFT_Load_Glyph(m_Face, iGlyphIndex, FXFT_LOAD_NO_SCALE | FXFT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH)) {
        return 0;
    }
    return FPF_EM_ADJUST(FXFT_Get_Face_UnitsPerEM(m_Face), FXFT_Get_Glyph_HoriAdvance(m_Face));
}
FX_INT32 CFPF_SkiaFont::GetAscent() const
{
    if (!m_Face) {
        return 0;
    }
    return FPF_EM_ADJUST(FXFT_Get_Face_UnitsPerEM(m_Face), FXFT_Get_Face_Ascender(m_Face));
}
FX_INT32 CFPF_SkiaFont::GetDescent() const
{
    if (!m_Face) {
        return 0;
    }
    return FPF_EM_ADJUST(FXFT_Get_Face_UnitsPerEM(m_Face), FXFT_Get_Face_Descender(m_Face));
}
FX_BOOL CFPF_SkiaFont::GetGlyphBBox(FX_INT32 iGlyphIndex, FX_RECT &rtBBox)
{
    if (!m_Face) {
        return FALSE;
    }
    if (FXFT_Is_Face_Tricky(m_Face)) {
        if (FXFT_Set_Char_Size(m_Face, 0, 1000 * 64, 72, 72)) {
            return FALSE;
        }
        if (FXFT_Load_Glyph(m_Face, iGlyphIndex, FXFT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH)) {
            FXFT_Set_Pixel_Sizes(m_Face, 0, 64);
            return FALSE;
        }
        FXFT_Glyph glyph;
        if (FXFT_Get_Glyph(m_Face->glyph, &glyph)) {
            FXFT_Set_Pixel_Sizes(m_Face, 0, 64);
            return FALSE;
        }
        FXFT_BBox cbox;
        FXFT_Glyph_Get_CBox(glyph, FXFT_GLYPH_BBOX_PIXELS, &cbox);
        FX_INT32 x_ppem = m_Face->size->metrics.x_ppem;
        FX_INT32 y_ppem = m_Face->size->metrics.y_ppem;
        rtBBox.left = FPF_EM_ADJUST(x_ppem, cbox.xMin);
        rtBBox.right = FPF_EM_ADJUST(x_ppem, cbox.xMax);
        rtBBox.top = FPF_EM_ADJUST(y_ppem, cbox.yMax);
        rtBBox.bottom = FPF_EM_ADJUST(y_ppem, cbox.yMin);
        rtBBox.top = FX_MIN(rtBBox.top, GetAscent());
        rtBBox.bottom = FX_MAX(rtBBox.bottom, GetDescent());
        FXFT_Done_Glyph(glyph);
        return FXFT_Set_Pixel_Sizes(m_Face, 0, 64) == 0;
    }
    if (FXFT_Load_Glyph(m_Face, iGlyphIndex, FXFT_LOAD_NO_SCALE | FXFT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH)) {
        return FALSE;
    }
    rtBBox.left = FPF_EM_ADJUST(FXFT_Get_Face_UnitsPerEM(m_Face), FXFT_Get_Glyph_HoriBearingX(m_Face));
    rtBBox.bottom = FPF_EM_ADJUST(FXFT_Get_Face_UnitsPerEM(m_Face), FXFT_Get_Glyph_HoriBearingY(m_Face));
    rtBBox.right = FPF_EM_ADJUST(FXFT_Get_Face_UnitsPerEM(m_Face), FXFT_Get_Glyph_HoriBearingX(m_Face) + FXFT_Get_Glyph_Width(m_Face));
    rtBBox.top = FPF_EM_ADJUST(FXFT_Get_Face_UnitsPerEM(m_Face), FXFT_Get_Glyph_HoriBearingY(m_Face) - FXFT_Get_Glyph_Height(m_Face));
    return TRUE;
}
FX_BOOL CFPF_SkiaFont::GetBBox(FX_RECT &rtBBox)
{
    if (!m_Face) {
        return FALSE;
    }
    rtBBox.left = FPF_EM_ADJUST(FXFT_Get_Face_UnitsPerEM(m_Face), FXFT_Get_Face_xMin(m_Face));
    rtBBox.top = FPF_EM_ADJUST(FXFT_Get_Face_UnitsPerEM(m_Face), FXFT_Get_Face_yMin(m_Face));
    rtBBox.right = FPF_EM_ADJUST(FXFT_Get_Face_UnitsPerEM(m_Face), FXFT_Get_Face_xMax(m_Face));
    rtBBox.bottom = FPF_EM_ADJUST(FXFT_Get_Face_UnitsPerEM(m_Face), FXFT_Get_Face_yMax(m_Face));
    return TRUE;
}
FX_INT32 CFPF_SkiaFont::GetHeight() const
{
    if (!m_Face) {
        return 0;
    }
    return	FPF_EM_ADJUST(FXFT_Get_Face_UnitsPerEM(m_Face), FXFT_Get_Face_Height(m_Face));
}
FX_INT32 CFPF_SkiaFont::GetItalicAngle() const
{
    if (!m_Face) {
        return 0;
    }
    TT_Postscript *ttInfo = (TT_Postscript*)FT_Get_Sfnt_Table(m_Face, ft_sfnt_post);
    if (ttInfo) {
        return ttInfo->italicAngle;
    }
    return 0;
}
FX_DWORD CFPF_SkiaFont::GetFontData(FX_DWORD dwTable, FX_LPBYTE pBuffer, FX_DWORD dwSize)
{
    if (!m_Face) {
        return FALSE;
    }
    if (FXFT_Load_Sfnt_Table(m_Face, dwTable, 0, pBuffer, (unsigned long*)&dwSize)) {
        return 0;
    }
    return dwSize;
}
FX_BOOL CFPF_SkiaFont::InitFont(CFPF_SkiaFontMgr *pFontMgr, CFPF_SkiaFontDescriptor *pFontDes, FX_BSTR bsFamily, FX_DWORD dwStyle, FX_BYTE uCharset)
{
    if (!pFontMgr || !pFontDes) {
        return FALSE;
    }
    switch (pFontDes->GetType()) {
        case FPF_SKIAFONTTYPE_Path: {
                CFPF_SkiaPathFont *pFont = (CFPF_SkiaPathFont*)pFontDes;
                m_Face = pFontMgr->GetFontFace(pFont->m_pPath, pFont->m_iFaceIndex);
            }
            break;
        case FPF_SKIAFONTTYPE_File: {
                CFPF_SkiaFileFont *pFont = (CFPF_SkiaFileFont*)pFontDes;
                m_Face = pFontMgr->GetFontFace(pFont->m_pFile, pFont->m_iFaceIndex);
            }
            break;
        case FPF_SKIAFONTTYPE_Buffer: {
                CFPF_SkiaBufferFont *pFont = (CFPF_SkiaBufferFont*)pFontDes;
                m_Face = pFontMgr->GetFontFace((FX_LPCBYTE)pFont->m_pBuffer, pFont->m_szBuffer, pFont->m_iFaceIndex);
            }
            break;
        default:
            return FALSE;
    }
    if (!m_Face) {
        return FALSE;
    }
    m_dwStyle = dwStyle;
    m_uCharset = uCharset;
    m_pFontMgr = pFontMgr;
    m_pFontDes = pFontDes;
    m_dwRefCount = 1;
    return TRUE;
}
#endif
