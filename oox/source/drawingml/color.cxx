/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <oox/drawingml/color.hxx>
#include <algorithm>
#include <frozen/bits/defines.h>
#include <frozen/bits/elsa_std.h>
#include <frozen/unordered_map.h>
#include <math.h>
#include <osl/diagnose.h>
#include <sal/log.hxx>
#include <oox/helper/containerhelper.hxx>
#include <oox/helper/graphichelper.hxx>
#include <oox/drawingml/drawingmltypes.hxx>
#include <oox/token/namespaces.hxx>
#include <oox/token/tokens.hxx>

namespace oox::drawingml {

namespace
{

// predefined colors in DrawingML (map XML token identifiers to RGB values)
constexpr auto constDmlColors = frozen::make_unordered_map<sal_Int32, ::Color>
({
    {XML_aliceBlue,         ::Color(0xF0F8FF)},    {XML_antiqueWhite,      ::Color(0xFAEBD7)},
    {XML_aqua,              ::Color(0x00FFFF)},    {XML_aquamarine,        ::Color(0x7FFFD4)},
    {XML_azure,             ::Color(0xF0FFFF)},    {XML_beige,             ::Color(0xF5F5DC)},
    {XML_bisque,            ::Color(0xFFE4C4)},    {XML_black,             ::Color(0x000000)},
    {XML_blanchedAlmond,    ::Color(0xFFEBCD)},    {XML_blue,              ::Color(0x0000FF)},
    {XML_blueViolet,        ::Color(0x8A2BE2)},    {XML_brown,             ::Color(0xA52A2A)},
    {XML_burlyWood,         ::Color(0xDEB887)},    {XML_cadetBlue,         ::Color(0x5F9EA0)},
    {XML_chartreuse,        ::Color(0x7FFF00)},    {XML_chocolate,         ::Color(0xD2691E)},
    {XML_coral,             ::Color(0xFF7F50)},    {XML_cornflowerBlue,    ::Color(0x6495ED)},
    {XML_cornsilk,          ::Color(0xFFF8DC)},    {XML_crimson,           ::Color(0xDC143C)},
    {XML_cyan,              ::Color(0x00FFFF)},    {XML_deepPink,          ::Color(0xFF1493)},
    {XML_deepSkyBlue,       ::Color(0x00BFFF)},    {XML_dimGray,           ::Color(0x696969)},
    {XML_dkBlue,            ::Color(0x00008B)},    {XML_dkCyan,            ::Color(0x008B8B)},
    {XML_dkGoldenrod,       ::Color(0xB8860B)},    {XML_dkGray,            ::Color(0xA9A9A9)},
    {XML_dkGreen,           ::Color(0x006400)},    {XML_dkKhaki,           ::Color(0xBDB76B)},
    {XML_dkMagenta,         ::Color(0x8B008B)},    {XML_dkOliveGreen,      ::Color(0x556B2F)},
    {XML_dkOrange,          ::Color(0xFF8C00)},    {XML_dkOrchid,          ::Color(0x9932CC)},
    {XML_dkRed,             ::Color(0x8B0000)},    {XML_dkSalmon,          ::Color(0xE9967A)},
    {XML_dkSeaGreen,        ::Color(0x8FBC8B)},    {XML_dkSlateBlue,       ::Color(0x483D8B)},
    {XML_dkSlateGray,       ::Color(0x2F4F4F)},    {XML_dkTurquoise,       ::Color(0x00CED1)},
    {XML_dkViolet,          ::Color(0x9400D3)},    {XML_dodgerBlue,        ::Color(0x1E90FF)},
    {XML_firebrick,         ::Color(0xB22222)},    {XML_floralWhite,       ::Color(0xFFFAF0)},
    {XML_forestGreen,       ::Color(0x228B22)},    {XML_fuchsia,           ::Color(0xFF00FF)},
    {XML_gainsboro,         ::Color(0xDCDCDC)},    {XML_ghostWhite,        ::Color(0xF8F8FF)},
    {XML_gold,              ::Color(0xFFD700)},    {XML_goldenrod,         ::Color(0xDAA520)},
    {XML_gray,              ::Color(0x808080)},    {XML_green,             ::Color(0x008000)},
    {XML_greenYellow,       ::Color(0xADFF2F)},    {XML_honeydew,          ::Color(0xF0FFF0)},
    {XML_hotPink,           ::Color(0xFF69B4)},    {XML_indianRed,         ::Color(0xCD5C5C)},
    {XML_indigo,            ::Color(0x4B0082)},    {XML_ivory,             ::Color(0xFFFFF0)},
    {XML_khaki,             ::Color(0xF0E68C)},    {XML_lavender,          ::Color(0xE6E6FA)},
    {XML_lavenderBlush,     ::Color(0xFFF0F5)},    {XML_lawnGreen,         ::Color(0x7CFC00)},
    {XML_lemonChiffon,      ::Color(0xFFFACD)},    {XML_lime,              ::Color(0x00FF00)},
    {XML_limeGreen,         ::Color(0x32CD32)},    {XML_linen,             ::Color(0xFAF0E6)},
    {XML_ltBlue,            ::Color(0xADD8E6)},    {XML_ltCoral,           ::Color(0xF08080)},
    {XML_ltCyan,            ::Color(0xE0FFFF)},    {XML_ltGoldenrodYellow, ::Color(0xFAFA78)},
    {XML_ltGray,            ::Color(0xD3D3D3)},    {XML_ltGreen,           ::Color(0x90EE90)},
    {XML_ltPink,            ::Color(0xFFB6C1)},    {XML_ltSalmon,          ::Color(0xFFA07A)},
    {XML_ltSeaGreen,        ::Color(0x20B2AA)},    {XML_ltSkyBlue,         ::Color(0x87CEFA)},
    {XML_ltSlateGray,       ::Color(0x778899)},    {XML_ltSteelBlue,       ::Color(0xB0C4DE)},
    {XML_ltYellow,          ::Color(0xFFFFE0)},    {XML_magenta,           ::Color(0xFF00FF)},
    {XML_maroon,            ::Color(0x800000)},    {XML_medAquamarine,     ::Color(0x66CDAA)},
    {XML_medBlue,           ::Color(0x0000CD)},    {XML_medOrchid,         ::Color(0xBA55D3)},
    {XML_medPurple,         ::Color(0x9370DB)},    {XML_medSeaGreen,       ::Color(0x3CB371)},
    {XML_medSlateBlue,      ::Color(0x7B68EE)},    {XML_medSpringGreen,    ::Color(0x00FA9A)},
    {XML_medTurquoise,      ::Color(0x48D1CC)},    {XML_medVioletRed,      ::Color(0xC71585)},
    {XML_midnightBlue,      ::Color(0x191970)},    {XML_mintCream,         ::Color(0xF5FFFA)},
    {XML_mistyRose,         ::Color(0xFFE4E1)},    {XML_moccasin,          ::Color(0xFFE4B5)},
    {XML_navajoWhite,       ::Color(0xFFDEAD)},    {XML_navy,              ::Color(0x000080)},
    {XML_oldLace,           ::Color(0xFDF5E6)},    {XML_olive,             ::Color(0x808000)},
    {XML_oliveDrab,         ::Color(0x6B8E23)},    {XML_orange,            ::Color(0xFFA500)},
    {XML_orangeRed,         ::Color(0xFF4500)},    {XML_orchid,            ::Color(0xDA70D6)},
    {XML_paleGoldenrod,     ::Color(0xEEE8AA)},    {XML_paleGreen,         ::Color(0x98FB98)},
    {XML_paleTurquoise,     ::Color(0xAFEEEE)},    {XML_paleVioletRed,     ::Color(0xDB7093)},
    {XML_papayaWhip,        ::Color(0xFFEFD5)},    {XML_peachPuff,         ::Color(0xFFDAB9)},
    {XML_peru,              ::Color(0xCD853F)},    {XML_pink,              ::Color(0xFFC0CB)},
    {XML_plum,              ::Color(0xDDA0DD)},    {XML_powderBlue,        ::Color(0xB0E0E6)},
    {XML_purple,            ::Color(0x800080)},    {XML_red,               ::Color(0xFF0000)},
    {XML_rosyBrown,         ::Color(0xBC8F8F)},    {XML_royalBlue,         ::Color(0x4169E1)},
    {XML_saddleBrown,       ::Color(0x8B4513)},    {XML_salmon,            ::Color(0xFA8072)},
    {XML_sandyBrown,        ::Color(0xF4A460)},    {XML_seaGreen,          ::Color(0x2E8B57)},
    {XML_seaShell,          ::Color(0xFFF5EE)},    {XML_sienna,            ::Color(0xA0522D)},
    {XML_silver,            ::Color(0xC0C0C0)},    {XML_skyBlue,           ::Color(0x87CEEB)},
    {XML_slateBlue,         ::Color(0x6A5ACD)},    {XML_slateGray,         ::Color(0x708090)},
    {XML_snow,              ::Color(0xFFFAFA)},    {XML_springGreen,       ::Color(0x00FF7F)},
    {XML_steelBlue,         ::Color(0x4682B4)},    {XML_tan,               ::Color(0xD2B48C)},
    {XML_teal,              ::Color(0x008080)},    {XML_thistle,           ::Color(0xD8BFD8)},
    {XML_tomato,            ::Color(0xFF6347)},    {XML_turquoise,         ::Color(0x40E0D0)},
    {XML_violet,            ::Color(0xEE82EE)},    {XML_wheat,             ::Color(0xF5DEB3)},
    {XML_white,             ::Color(0xFFFFFF)},    {XML_whiteSmoke,        ::Color(0xF5F5F5)},
    {XML_yellow,            ::Color(0xFFFF00)},    {XML_yellowGreen,       ::Color(0x9ACD32)}
});

constexpr ::Color lookupDmlColor(sal_Int32 nToken)
{
    auto iterator = constDmlColors.find(nToken);
    if (iterator == constDmlColors.end())
        return API_RGB_TRANSPARENT;
    return iterator->second;
}

constexpr auto constVmlColors = frozen::make_unordered_map<sal_Int32, ::Color>
({
    {XML_aqua,              ::Color(0x00FFFF)},    {XML_black,             ::Color(0x000000)},
    {XML_blue,              ::Color(0x0000FF)},    {XML_fuchsia,           ::Color(0xFF00FF)},
    {XML_gray,              ::Color(0x808080)},    {XML_green,             ::Color(0x008000)},
    {XML_lime,              ::Color(0x00FF00)},    {XML_maroon,            ::Color(0x800000)},
    {XML_navy,              ::Color(0x000080)},    {XML_olive,             ::Color(0x808000)},
    {XML_purple,            ::Color(0x800080)},    {XML_red,               ::Color(0xFF0000)},
    {XML_silver,            ::Color(0xC0C0C0)},    {XML_teal,              ::Color(0x008080)},
    {XML_white,             ::Color(0xFFFFFF)},    {XML_yellow,            ::Color(0xFFFF00)}
});

constexpr ::Color lookupVmlColor(sal_Int32 nToken)
{
    auto iterator = constVmlColors.find(nToken);
    if (iterator == constVmlColors.end())
        return API_RGB_TRANSPARENT;
    return iterator->second;
}

constexpr auto constHighlightColors = frozen::make_unordered_map<sal_Int32, ::Color>
({
    // tdf#131841 Predefined color for OOXML highlight.
    {XML_black,             ::Color(0x000000)},    {XML_blue,              ::Color(0x0000FF)},
    {XML_cyan,              ::Color(0x00FFFF)},    {XML_darkBlue,          ::Color(0x00008B)},
    {XML_darkCyan,          ::Color(0x008B8B)},    {XML_darkGray,          ::Color(0xA9A9A9)},
    {XML_darkGreen,         ::Color(0x006400)},    {XML_darkMagenta,       ::Color(0x800080)},
    {XML_darkRed,           ::Color(0x8B0000)},    {XML_darkYellow,        ::Color(0x808000)},
    {XML_green,             ::Color(0x00FF00)},    {XML_lightGray,         ::Color(0xD3D3D3)},
    {XML_magenta,           ::Color(0xFF00FF)},    {XML_red,               ::Color(0xFF0000)},
    {XML_white,             ::Color(0xFFFFFF)},    {XML_yellow,            ::Color(0xFFFF00)}
});

constexpr ::Color lookupHighlightColor(sal_Int32 nToken)
{
    auto iterator = constHighlightColors.find(nToken);
    if (iterator == constHighlightColors.end())
        return API_RGB_TRANSPARENT;
    return iterator->second;
}

const double DEC_GAMMA          = 2.3;
const double INC_GAMMA          = 1.0 / DEC_GAMMA;

void lclRgbToRgbComponents( sal_Int32& ornR, sal_Int32& ornG, sal_Int32& ornB, ::Color nRgb )
{
    ornR = nRgb.GetRed();
    ornG = nRgb.GetGreen();
    ornB = nRgb.GetBlue();
}

sal_Int32 lclRgbComponentsToRgb( sal_Int32 nR, sal_Int32 nG, sal_Int32 nB )
{
    return static_cast< sal_Int32 >( (nR << 16) | (nG << 8) | nB );
}

sal_Int32 lclRgbCompToCrgbComp( sal_Int32 nRgbComp )
{
    return static_cast< sal_Int32 >( nRgbComp * MAX_PERCENT / 255 );
}

sal_Int32 lclCrgbCompToRgbComp( sal_Int32 nCrgbComp )
{
    return static_cast< sal_Int32 >( nCrgbComp * 255 / MAX_PERCENT );
}

sal_Int32 lclGamma( sal_Int32 nComp, double fGamma )
{
    return static_cast< sal_Int32 >( pow( static_cast< double >( nComp ) / MAX_PERCENT, fGamma ) * MAX_PERCENT + 0.5 );
}

void lclSetValue( sal_Int32& ornValue, sal_Int32 nNew, sal_Int32 nMax = MAX_PERCENT )
{
    OSL_ENSURE( (0 <= nNew) && (nNew <= nMax), "lclSetValue - invalid value" );
    if( (0 <= nNew) && (nNew <= nMax) )
        ornValue = nNew;
}

void lclModValue( sal_Int32& ornValue, sal_Int32 nMod, sal_Int32 nMax = MAX_PERCENT )
{
    OSL_ENSURE( (0 <= nMod), "lclModValue - invalid modificator" );
    ornValue = getLimitedValue< sal_Int32, double >( static_cast< double >( ornValue ) * nMod / MAX_PERCENT, 0, nMax );
}

void lclOffValue( sal_Int32& ornValue, sal_Int32 nOff, sal_Int32 nMax = MAX_PERCENT )
{
    OSL_ENSURE( (-nMax <= nOff) && (nOff <= nMax), "lclOffValue - invalid offset" );
    ornValue = getLimitedValue< sal_Int32, sal_Int32 >( ornValue + nOff, 0, nMax );
}

constexpr auto constSchemeColorNameToIndex = frozen::make_unordered_map<std::u16string_view, model::ThemeColorType>
({
    { u"dk1", model::ThemeColorType::Dark1 },
    { u"lt1", model::ThemeColorType::Light1 },
    { u"dk2", model::ThemeColorType::Dark2 },
    { u"lt2", model::ThemeColorType::Light2 },
    { u"accent1", model::ThemeColorType::Accent1 },
    { u"accent2", model::ThemeColorType::Accent2 },
    { u"accent3", model::ThemeColorType::Accent3 },
    { u"accent4", model::ThemeColorType::Accent4 },
    { u"accent5", model::ThemeColorType::Accent5 },
    { u"accent6", model::ThemeColorType::Accent6 },
    { u"hlink", model::ThemeColorType::Hyperlink },
    { u"folHlink", model::ThemeColorType::FollowedHyperlink },
    { u"tx1", model::ThemeColorType::Dark1 },
    { u"bg1", model::ThemeColorType::Light1 },
    { u"tx2", model::ThemeColorType::Dark2 },
    { u"bg2", model::ThemeColorType::Light2 },
    { u"dark1", model::ThemeColorType::Dark1},
    { u"light1", model::ThemeColorType::Light1},
    { u"dark2", model::ThemeColorType::Dark2 },
    { u"light2", model::ThemeColorType::Light2 },
    { u"text1", model::ThemeColorType::Dark1 },
    { u"background1", model::ThemeColorType::Light1 },
    { u"text2", model::ThemeColorType::Dark2 },
    { u"background2", model::ThemeColorType::Light2 },
    { u"hyperlink", model::ThemeColorType::Hyperlink },
    { u"followedHyperlink", model::ThemeColorType::FollowedHyperlink }
});

constexpr auto constThemeColorTokenMap = frozen::make_unordered_map<sal_Int32, model::ThemeColorType>
({
    { XML_dk1, model::ThemeColorType::Dark1 },
    { XML_lt1, model::ThemeColorType::Light1 },
    { XML_dk2, model::ThemeColorType::Dark2 },
    { XML_lt2, model::ThemeColorType::Light2 },
    { XML_accent1, model::ThemeColorType::Accent1 },
    { XML_accent2, model::ThemeColorType::Accent2 },
    { XML_accent3, model::ThemeColorType::Accent3 },
    { XML_accent4, model::ThemeColorType::Accent4 },
    { XML_accent5, model::ThemeColorType::Accent5 },
    { XML_accent6, model::ThemeColorType::Accent6 },
    { XML_hlink, model::ThemeColorType::Hyperlink },
    { XML_folHlink, model::ThemeColorType::FollowedHyperlink },
    { XML_tx1, model::ThemeColorType::Dark1 },
    { XML_bg1, model::ThemeColorType::Light1 },
    { XML_tx2, model::ThemeColorType::Dark2 },
    { XML_bg2, model::ThemeColorType::Light2 },
    { XML_dark1, model::ThemeColorType::Dark1 },
    { XML_light1, model::ThemeColorType::Light1 },
    { XML_dark2, model::ThemeColorType::Dark2 },
    { XML_light2, model::ThemeColorType::Light2 },
    { XML_text1, model::ThemeColorType::Dark1 },
    { XML_background1, model::ThemeColorType::Light1 },
    { XML_text2, model::ThemeColorType::Dark2 },
    { XML_background2, model::ThemeColorType::Light2 },
    { XML_hyperlink, model::ThemeColorType::Hyperlink },
    { XML_followedHyperlink, model::ThemeColorType::FollowedHyperlink },
});

} // end anonymous namespace

model::ThemeColorType schemeNameToThemeColorType(OUString const& rSchemeName)
{
    auto aIterator = constSchemeColorNameToIndex.find(rSchemeName);
    if (aIterator == constSchemeColorNameToIndex.end())
        return model::ThemeColorType::Unknown;
    else
        return aIterator->second;
}

model::ThemeColorType schemeTokenToThemeColorType(sal_uInt32 nToken)
{
    auto aIterator = constThemeColorTokenMap.find(nToken);
    if (aIterator == constThemeColorTokenMap.end())
        return model::ThemeColorType::Unknown;
    else
        return aIterator->second;
}

Color::Color() :
    meMode( ColorMode::Unused ),
    mnC1( 0 ),
    mnC2( 0 ),
    mnC3( 0 ),
    mnAlpha( MAX_PERCENT ),
    meThemeColorType( model::ThemeColorType::Unknown )
{
}

bool Color::operator==(const Color& rhs) const
{
    if (meMode != rhs.meMode)
        return false;
    if (maTransforms != rhs.maTransforms)
        return false;
    if (mnC1 != rhs.mnC1)
        return false;
    if (mnC2 != rhs.mnC2)
        return false;
    if (mnC3 != rhs.mnC3)
        return false;
    if (mnAlpha != rhs.mnAlpha)
        return false;
    if (msSchemeName != rhs.msSchemeName)
        return false;
    if (meThemeColorType != rhs.meThemeColorType)
        return false;
    return true;
}

::Color Color::getDmlPresetColor( sal_Int32 nToken, ::Color nDefaultRgb )
{
    /*  Do not pass nDefaultRgb to ContainerHelper::getVectorElement(), to be
        able to catch the existing vector entries without corresponding XML
        token identifier. */
    ::Color nRgbValue = lookupDmlColor(nToken);
    return (sal_Int32(nRgbValue) >= 0) ? nRgbValue : nDefaultRgb;
}

::Color Color::getVmlPresetColor( sal_Int32 nToken, ::Color nDefaultRgb )
{
    /*  Do not pass nDefaultRgb to ContainerHelper::getVectorElement(), to be
        able to catch the existing vector entries without corresponding XML
        token identifier. */
    ::Color nRgbValue = lookupVmlColor(nToken);
    return (sal_Int32(nRgbValue) >= 0) ? nRgbValue : nDefaultRgb;
}

::Color Color::getHighlightColor(sal_Int32 nToken, ::Color nDefaultRgb)
{
    /*  Do not pass nDefaultRgb to ContainerHelper::getVectorElement(), to be
        able to catch the existing vector entries without corresponding XML
        token identifier. */
    ::Color nRgbValue = lookupHighlightColor(nToken);
    return (sal_Int32(nRgbValue) >= 0) ? nRgbValue : nDefaultRgb;
}

void Color::setUnused()
{
    meMode = ColorMode::Unused;
}

void Color::setSrgbClr( ::Color nRgb )
{
    setSrgbClr(sal_Int32(nRgb));
}

void Color::setSrgbClr( sal_Int32 nRgb )
{
    OSL_ENSURE( (0 <= nRgb) && (nRgb <= 0xFFFFFF), "Color::setSrgbClr - invalid RGB value" );
    meMode = ColorMode::AbsoluteRgb;
    lclRgbToRgbComponents( mnC1, mnC2, mnC3, ::Color(ColorTransparency, nRgb) );
}

void Color::setScrgbClr( sal_Int32 nR, sal_Int32 nG, sal_Int32 nB )
{
    OSL_ENSURE( (0 <= nR) && (nR <= MAX_PERCENT), "Color::setScrgbClr - invalid red value" );
    OSL_ENSURE( (0 <= nG) && (nG <= MAX_PERCENT), "Color::setScrgbClr - invalid green value" );
    OSL_ENSURE( (0 <= nB) && (nB <= MAX_PERCENT), "Color::setScrgbClr - invalid blue value" );
    meMode = ColorMode::RelativeRgb;
    mnC1 = getLimitedValue< sal_Int32, sal_Int32 >( nR, 0, MAX_PERCENT );
    mnC2 = getLimitedValue< sal_Int32, sal_Int32 >( nG, 0, MAX_PERCENT );
    mnC3 = getLimitedValue< sal_Int32, sal_Int32 >( nB, 0, MAX_PERCENT );
}

void Color::setHslClr( sal_Int32 nHue, sal_Int32 nSat, sal_Int32 nLum )
{
    OSL_ENSURE( (0 <= nHue) && (nHue <= MAX_DEGREE), "Color::setHslClr - invalid hue value" );
    OSL_ENSURE( (0 <= nSat) && (nSat <= MAX_PERCENT), "Color::setHslClr - invalid saturation value" );
    OSL_ENSURE( (0 <= nLum) && (nLum <= MAX_PERCENT), "Color::setHslClr - invalid luminance value" );
    meMode = ColorMode::Hsl;
    mnC1 = getLimitedValue< sal_Int32, sal_Int32 >( nHue, 0, MAX_DEGREE );
    mnC2 = getLimitedValue< sal_Int32, sal_Int32 >( nSat, 0, MAX_PERCENT );
    mnC3 = getLimitedValue< sal_Int32, sal_Int32 >( nLum, 0, MAX_PERCENT );
}

void Color::setPrstClr( sal_Int32 nToken )
{
    ::Color nRgbValue = getDmlPresetColor( nToken, API_RGB_TRANSPARENT );
    OSL_ENSURE( sal_Int32(nRgbValue) >= 0, "Color::setPrstClr - invalid preset color token" );
    if( sal_Int32(nRgbValue) >= 0 )
        setSrgbClr( nRgbValue );
}

void Color::setHighlight(sal_Int32 nToken)
{
    ::Color nRgbValue = getHighlightColor(nToken, API_RGB_TRANSPARENT);
    OSL_ENSURE( sal_Int32(nRgbValue) >= 0, "Color::setPrstClr - invalid preset color token" );
    if ( sal_Int32(nRgbValue) >= 0 )
        setSrgbClr( nRgbValue );
}

void Color::setSchemeClr( sal_Int32 nToken )
{
    OSL_ENSURE( nToken != XML_TOKEN_INVALID, "Color::setSchemeClr - invalid color token" );
    meMode = (nToken == XML_phClr) ? ColorMode::PlaceHolder : ColorMode::Scheme;
    mnC1 = nToken;
    if (meMode == ColorMode::Scheme)
        meThemeColorType = schemeTokenToThemeColorType(nToken);
}

void Color::setPaletteClr( sal_Int32 nPaletteIdx )
{
    OSL_ENSURE( nPaletteIdx >= 0, "Color::setPaletteClr - invalid palette index" );
    meMode = ColorMode::ApplicationPalette;
    mnC1 = nPaletteIdx;
}

void Color::setSysClr( sal_Int32 nToken, sal_Int32 nLastRgb )
{
    OSL_ENSURE( (-1 <= nLastRgb) && (nLastRgb <= 0xFFFFFF), "Color::setSysClr - invalid RGB value" );
    meMode = ColorMode::SystemPalette;
    mnC1 = nToken;
    mnC2 = nLastRgb;
}

void Color::addTransformation( sal_Int32 nElement, sal_Int32 nValue )
{
    /*  Execute alpha transformations directly, store other transformations in
        a vector, they may depend on a scheme base color which will be resolved
        in Color::getColor(). */
    sal_Int32 nToken = getBaseToken( nElement );
    switch( nToken )
    {
        case XML_alpha:     lclSetValue( mnAlpha, nValue ); break;
        case XML_alphaMod:  lclModValue( mnAlpha, nValue ); break;
        case XML_alphaOff:  lclOffValue( mnAlpha, nValue ); break;
        default:            maTransforms.emplace_back( nToken, nValue );
    }
    sal_Int32 nSize = maInteropTransformations.getLength();
    maInteropTransformations.realloc(nSize + 1);
    auto pInteropTransformations = maInteropTransformations.getArray();
    pInteropTransformations[nSize].Name = getColorTransformationName( nToken );
    pInteropTransformations[nSize].Value <<= nValue;
}

void Color::addChartTintTransformation( double fTint )
{
    sal_Int32 nValue = getLimitedValue< sal_Int32, double >( fTint * MAX_PERCENT + 0.5, -MAX_PERCENT, MAX_PERCENT );
    if( nValue < 0 )
        maTransforms.emplace_back( XML_shade, nValue + MAX_PERCENT );
    else if( nValue > 0 )
        maTransforms.emplace_back( XML_tint, MAX_PERCENT - nValue );
}

void Color::addExcelTintTransformation(double fTint)
{
    sal_Int32 nValue = std::round(std::abs(fTint) * 100'000.0);
    if (fTint > 0.0)
    {
        maTransforms.emplace_back(XML_lumMod, 100'000 - nValue);
        maTransforms.emplace_back(XML_lumOff, nValue);
    }
    else if (fTint < 0.0)
    {
        maTransforms.emplace_back(XML_lumMod, 100'000 - nValue);
    }
}

void Color::clearTransformations()
{
    maTransforms.clear();
    maInteropTransformations.realloc(0);
    clearTransparence();
}

constexpr auto constColorMapTokenMap = frozen::make_unordered_map<std::u16string_view, sal_Int32>
({
    { u"bg1", XML_bg1 },         { u"tx1", XML_tx1 },         { u"bg2", XML_bg2 },
    { u"tx2", XML_tx2 },         { u"accent1", XML_accent1 }, { u"accent2", XML_accent2 },
    { u"accent3", XML_accent3 }, { u"accent4", XML_accent4 }, { u"accent5", XML_accent5 },
    { u"accent6", XML_accent6 }, { u"hlink", XML_hlink },     { u"folHlink", XML_folHlink }
});

sal_Int32 Color::getColorMapToken(std::u16string_view sName)
{
    auto aIterator = constColorMapTokenMap.find(sName);
    if (aIterator == constColorMapTokenMap.end())
        return XML_TOKEN_INVALID;
    else
        return aIterator->second;
}

OUString Color::getColorTransformationName( sal_Int32 nElement )
{
    switch( nElement )
    {
        case XML_red:       return u"red"_ustr;
        case XML_redMod:    return u"redMod"_ustr;
        case XML_redOff:    return u"redOff"_ustr;
        case XML_green:     return u"green"_ustr;
        case XML_greenMod:  return u"greenMod"_ustr;
        case XML_greenOff:  return u"greenOff"_ustr;
        case XML_blue:      return u"blue"_ustr;
        case XML_blueMod:   return u"blueMod"_ustr;
        case XML_blueOff:   return u"blueOff"_ustr;
        case XML_alpha:     return u"alpha"_ustr;
        case XML_alphaMod:  return u"alphaMod"_ustr;
        case XML_alphaOff:  return u"alphaOff"_ustr;
        case XML_hue:       return u"hue"_ustr;
        case XML_hueMod:    return u"hueMod"_ustr;
        case XML_hueOff:    return u"hueOff"_ustr;
        case XML_sat:       return u"sat"_ustr;
        case XML_satMod:    return u"satMod"_ustr;
        case XML_satOff:    return u"satOff"_ustr;
        case XML_lum:       return u"lum"_ustr;
        case XML_lumMod:    return u"lumMod"_ustr;
        case XML_lumOff:    return u"lumOff"_ustr;
        case XML_shade:     return u"shade"_ustr;
        case XML_tint:      return u"tint"_ustr;
        case XML_gray:      return u"gray"_ustr;
        case XML_comp:      return u"comp"_ustr;
        case XML_inv:       return u"inv"_ustr;
        case XML_gamma:     return u"gamma"_ustr;
        case XML_invGamma:  return u"invGamma"_ustr;
    }
    SAL_WARN( "oox.drawingml", "Color::getColorTransformationName - unexpected transformation type" );
    return OUString();
}

sal_Int32 Color::getColorTransformationToken( std::u16string_view sName )
{
    if( sName == u"red" )
        return XML_red;
    else if( sName == u"redMod" )
        return XML_redMod;
    else if( sName == u"redOff" )
        return XML_redOff;
    else if( sName == u"green" )
        return XML_green;
    else if( sName == u"greenMod" )
        return XML_greenMod;
    else if( sName == u"greenOff" )
        return XML_greenOff;
    else if( sName == u"blue" )
        return XML_blue;
    else if( sName == u"blueMod" )
        return XML_blueMod;
    else if( sName == u"blueOff" )
        return XML_blueOff;
    else if( sName == u"alpha" )
        return XML_alpha;
    else if( sName == u"alphaMod" )
        return XML_alphaMod;
    else if( sName == u"alphaOff" )
        return XML_alphaOff;
    else if( sName == u"hue" )
        return XML_hue;
    else if( sName == u"hueMod" )
        return XML_hueMod;
    else if( sName == u"hueOff" )
        return XML_hueOff;
    else if( sName == u"sat" )
        return XML_sat;
    else if( sName == u"satMod" )
        return XML_satMod;
    else if( sName == u"satOff" )
        return XML_satOff;
    else if( sName == u"lum" )
        return XML_lum;
    else if( sName == u"lumMod" )
        return XML_lumMod;
    else if( sName == u"lumOff" )
        return XML_lumOff;
    else if( sName == u"shade" )
        return XML_shade;
    else if( sName == u"tint" )
        return XML_tint;
    else if( sName == u"gray" )
        return XML_gray;
    else if( sName == u"comp" )
        return XML_comp;
    else if( sName == u"inv" )
        return XML_inv;
    else if( sName == u"gamma" )
        return XML_gamma;
    else if( sName == u"invGamma" )
        return XML_invGamma;

    SAL_WARN( "oox.drawingml", "Color::getColorTransformationToken - unexpected transformation type" );
    return XML_TOKEN_INVALID;
}

bool Color::equals(const Color& rOther, const GraphicHelper& rGraphicHelper, ::Color nPhClr) const
{
    if (getColor(rGraphicHelper, nPhClr) != rOther.getColor(rGraphicHelper, nPhClr))
        return false;

    return getTransparency() == rOther.getTransparency();
}

void Color::clearTransparence()
{
    mnAlpha = MAX_PERCENT;
}

sal_Int16 Color::getTintOrShade() const
{
    for(auto const& aTransform : maTransforms)
    {
        switch(aTransform.mnToken)
        {
            case XML_tint:
                // from 1000th percent to 100th percent...
                return aTransform.mnValue/10;
            case XML_shade:
                // from 1000th percent to 100th percent...
                return -aTransform.mnValue/10;
        }
    }
    return 0;
}

sal_Int16 Color::getLumMod() const
{
    for (const auto& rTransform : maTransforms)
    {
        if (rTransform.mnToken != XML_lumMod)
        {
            continue;
        }

        // From 1000th percent to 100th percent.
        return rTransform.mnValue / 10;
    }

    return 10000;
}

sal_Int16 Color::getLumOff() const
{
    for (const auto& rTransform : maTransforms)
    {
        if (rTransform.mnToken != XML_lumOff)
        {
            continue;
        }

        // From 1000th percent to 100th percent.
        return rTransform.mnValue / 10;
    }

    return 0;
}

model::ComplexColor Color::getComplexColor() const
{
    auto aComplexColor = model::ComplexColor::Theme(model::convertToThemeColorType(getSchemeColorIndex()));

    if (getTintOrShade() > 0)
    {
        aComplexColor.addTransformation({model::TransformationType::Tint, getTintOrShade()});
    }
    else if (getTintOrShade() < 0)
    {
        sal_Int16 nShade = o3tl::narrowing<sal_Int16>(-getTintOrShade());
        aComplexColor.addTransformation({model::TransformationType::Shade, nShade});
    }

    if (getLumMod() != 10000)
        aComplexColor.addTransformation({model::TransformationType::LumMod, getLumMod()});

    if (getLumOff() != 0)
        aComplexColor.addTransformation({model::TransformationType::LumOff, getLumOff()});

    return aComplexColor;
}

::Color Color::getColor( const GraphicHelper& rGraphicHelper, ::Color nPhClr ) const
{
    const sal_Int32 nTempC1 = mnC1;
    const sal_Int32 nTempC2 = mnC2;
    const sal_Int32 nTempC3 = mnC3;
    const ColorMode eTempMode = meMode;

    switch( meMode )
    {
        case ColorMode::Unused:  mnC1 = sal_Int32(API_RGB_TRANSPARENT); break;

        case ColorMode::AbsoluteRgb:     break;  // nothing to do
        case ColorMode::RelativeRgb:    break;  // nothing to do
        case ColorMode::Hsl:     break;  // nothing to do

        case ColorMode::Scheme:
            setResolvedRgb( rGraphicHelper.getSchemeColor( mnC1 ) );
            break;
        case ColorMode::ApplicationPalette:
            setResolvedRgb( rGraphicHelper.getPaletteColor( mnC1 ) );
            break;
        case ColorMode::SystemPalette:
            setResolvedRgb( rGraphicHelper.getSystemColor( mnC1, ::Color(ColorTransparency, mnC2) ) );
            break;
        case ColorMode::PlaceHolder:
            setResolvedRgb( nPhClr );
            break;

        case ColorMode::Finalized:
            return ::Color(ColorTransparency, mnC1);
    }

    // if color is UNUSED or turns to UNUSED in setResolvedRgb, do not perform transformations
    if( meMode != ColorMode::Unused )
    {
        for (auto const& transform : maTransforms)
        {
            switch( transform.mnToken )
            {
                case XML_red:       toCrgb(); lclSetValue( mnC1, transform.mnValue );    break;
                case XML_redMod:    toCrgb(); lclModValue( mnC1, transform.mnValue );    break;
                case XML_redOff:    toCrgb(); lclOffValue( mnC1, transform.mnValue );    break;
                case XML_green:     toCrgb(); lclSetValue( mnC2, transform.mnValue );    break;
                case XML_greenMod:  toCrgb(); lclModValue( mnC2, transform.mnValue );    break;
                case XML_greenOff:  toCrgb(); lclOffValue( mnC2, transform.mnValue );    break;
                case XML_blue:      toCrgb(); lclSetValue( mnC3, transform.mnValue );    break;
                case XML_blueMod:   toCrgb(); lclModValue( mnC3, transform.mnValue );    break;
                case XML_blueOff:   toCrgb(); lclOffValue( mnC3, transform.mnValue );    break;

                case XML_hue:       toHsl(); lclSetValue( mnC1, transform.mnValue, MAX_DEGREE ); break;
                case XML_hueMod:    toHsl(); lclModValue( mnC1, transform.mnValue, MAX_DEGREE ); break;
                case XML_hueOff:    toHsl(); lclOffValue( mnC1, transform.mnValue, MAX_DEGREE ); break;
                case XML_sat:       toHsl(); lclSetValue( mnC2, transform.mnValue );             break;
                case XML_satMod:    toHsl(); lclModValue( mnC2, transform.mnValue );             break;
                case XML_satOff:    toHsl(); lclOffValue( mnC2, transform.mnValue );             break;

                case XML_lum:
                    toHsl();
                    lclSetValue( mnC3, transform.mnValue );
                    // if color changes to black or white, it will stay gray if luminance changes again
                    if( (mnC3 == 0) || (mnC3 == MAX_PERCENT) ) mnC2 = 0;
                break;
                case XML_lumMod:
                    toHsl();
                    lclModValue( mnC3, transform.mnValue );
                    // if color changes to black or white, it will stay gray if luminance changes again
                    if( (mnC3 == 0) || (mnC3 == MAX_PERCENT) ) mnC2 = 0;
                break;
                case XML_lumOff:
                    toHsl();
                    lclOffValue( mnC3, transform.mnValue );
                    // if color changes to black or white, it will stay gray if luminance changes again
                    if( (mnC3 == 0) || (mnC3 == MAX_PERCENT) ) mnC2 = 0;
                break;

                case XML_shade:
                    // shade: 0% = black, 100% = original color
                    toCrgb();
                    OSL_ENSURE( (0 <= transform.mnValue) && (transform.mnValue <= MAX_PERCENT), "Color::getColor - invalid shade value" );
                    if( (0 <= transform.mnValue) && (transform.mnValue <= MAX_PERCENT) )
                    {
                        double fFactor = static_cast< double >( transform.mnValue ) / MAX_PERCENT;
                        mnC1 = static_cast< sal_Int32 >( mnC1 * fFactor );
                        mnC2 = static_cast< sal_Int32 >( mnC2 * fFactor );
                        mnC3 = static_cast< sal_Int32 >( mnC3 * fFactor );
                    }
                break;
                case XML_tint:
                    // tint: 0% = white, 100% = original color
                    toCrgb();
                    OSL_ENSURE( (0 <= transform.mnValue) && (transform.mnValue <= MAX_PERCENT), "Color::getColor - invalid tint value" );
                    if( (0 <= transform.mnValue) && (transform.mnValue <= MAX_PERCENT) )
                    {
                        double fFactor = static_cast< double >( transform.mnValue ) / MAX_PERCENT;
                        mnC1 = static_cast< sal_Int32 >( MAX_PERCENT - (MAX_PERCENT - mnC1) * fFactor );
                        mnC2 = static_cast< sal_Int32 >( MAX_PERCENT - (MAX_PERCENT - mnC2) * fFactor );
                        mnC3 = static_cast< sal_Int32 >( MAX_PERCENT - (MAX_PERCENT - mnC3) * fFactor );
                    }
                break;

                case XML_gray:
                    // change color to gray, weighted RGB: 22% red, 72% green, 6% blue
                    toRgb();
                    mnC1 = mnC2 = mnC3 = (mnC1 * 22 + mnC2 * 72 + mnC3 * 6) / 100;
                break;

                case XML_comp:
                    // comp: rotate hue by 180 degrees, do not change lum/sat
                    toHsl();
                    mnC1 = (mnC1 + (180 * PER_DEGREE)) % MAX_DEGREE;
                break;
                case XML_inv:
                    // invert percentual RGB values
                    toCrgb();
                    mnC1 = MAX_PERCENT - mnC1;
                    mnC2 = MAX_PERCENT - mnC2;
                    mnC3 = MAX_PERCENT - mnC3;
                break;

                case XML_gamma:
                    // increase gamma of color
                    toCrgb();
                    mnC1 = lclGamma( mnC1, INC_GAMMA );
                    mnC2 = lclGamma( mnC2, INC_GAMMA );
                    mnC3 = lclGamma( mnC3, INC_GAMMA );
                break;
                case XML_invGamma:
                    // decrease gamma of color
                    toCrgb();
                    mnC1 = lclGamma( mnC1, DEC_GAMMA );
                    mnC2 = lclGamma( mnC2, DEC_GAMMA );
                    mnC3 = lclGamma( mnC3, DEC_GAMMA );
                break;
            }
        }

        // store resulting RGB value in mnC1
        toRgb();
        mnC1 = lclRgbComponentsToRgb( mnC1, mnC2, mnC3 );
        if (hasTransparency())
        {
            mnC1 |= static_cast<sal_Int32>(255.0 * (MAX_PERCENT - mnAlpha) / MAX_PERCENT) << 24;
        }
    }
    else // if( meMode != ColorMode::Unused )
    {
        mnC1 = sal_Int32(API_RGB_TRANSPARENT);
    }

    sal_Int32 nRet = mnC1;
    // Restore the original values when the color depends on one of the input
    // parameters (rGraphicHelper or nPhClr)
    if( eTempMode >= ColorMode::Scheme && eTempMode <= ColorMode::PlaceHolder )
    {
        mnC1 = nTempC1;
        mnC2 = nTempC2;
        mnC3 = nTempC3;
        meMode = eTempMode;
    }
    else
    {
        meMode = ColorMode::Finalized;
    }
    if( meMode == ColorMode::Finalized )
        maTransforms.clear();
    return ::Color(ColorTransparency, nRet);
}

bool Color::hasTransparency() const
{
    return mnAlpha < MAX_PERCENT;
}

sal_Int16 Color::getTransparency() const
{
    return sal_Int16(std::round( (1.0 * (MAX_PERCENT - mnAlpha)) / PER_PERCENT) );
}

sal_Int16 Color::getSchemeColorIndex() const
{
    auto eThemeType = schemeNameToThemeColorType(msSchemeName);
    return sal_Int16(eThemeType);
}

model::ComplexColor Color::createComplexColor(const GraphicHelper& /*rGraphicHelper*/, sal_Int16 nPhClrTheme) const
{
    model::ComplexColor aNewComplexColor;
    if (meMode == ColorMode::PlaceHolder)
    {
        auto eTheme = model::convertToThemeColorType(nPhClrTheme);
        aNewComplexColor.setThemeColor(eTheme);
    }
    else if (meMode == ColorMode::Scheme)
    {
        auto eTheme = getThemeColorType();
        aNewComplexColor.setThemeColor(eTheme);
    }
    else if (meMode == ColorMode::AbsoluteRgb)
    {
        ::Color aColor(ColorTransparency, lclRgbComponentsToRgb(mnC1, mnC2, mnC3));
        aNewComplexColor = model::ComplexColor::createRGB(aColor);
    }
    else
    {
        // TODO - Add other options
        return aNewComplexColor;
    }

    for(auto const& aTransform : maTransforms)
    {
        sal_Int16 nValue = sal_Int16(aTransform.mnValue / 10);

        switch (aTransform.mnToken)
        {
            case XML_lumMod:
                if (nValue != 10'000)
                    aNewComplexColor.addTransformation({model::TransformationType::LumMod, nValue});
                break;
            case XML_lumOff:
                if (nValue != 0)
                    aNewComplexColor.addTransformation({model::TransformationType::LumOff, nValue});
                break;
            case XML_tint:
                if (nValue != 0)
                    aNewComplexColor.addTransformation({model::TransformationType::Tint, nValue});
                break;
            case XML_shade:
                if (nValue != 0)
                    aNewComplexColor.addTransformation({model::TransformationType::Shade, nValue});
                break;
        }
    }

    return aNewComplexColor;
}

// private --------------------------------------------------------------------

void Color::setResolvedRgb( ::Color nRgb ) const
{
    meMode = (sal_Int32(nRgb) < 0) ? ColorMode::Unused : ColorMode::AbsoluteRgb;
    lclRgbToRgbComponents( mnC1, mnC2, mnC3, nRgb );
}

void Color::toRgb() const
{
    switch( meMode )
    {
        case ColorMode::AbsoluteRgb:
            // nothing to do
        break;
        case ColorMode::RelativeRgb:
            meMode = ColorMode::AbsoluteRgb;
            mnC1 = lclCrgbCompToRgbComp( lclGamma( mnC1, INC_GAMMA ) );
            mnC2 = lclCrgbCompToRgbComp( lclGamma( mnC2, INC_GAMMA ) );
            mnC3 = lclCrgbCompToRgbComp( lclGamma( mnC3, INC_GAMMA ) );
        break;
        case ColorMode::Hsl:
        {
            meMode = ColorMode::AbsoluteRgb;
            double fR = 0.0, fG = 0.0, fB = 0.0;
            if( (mnC2 == 0) || (mnC3 == MAX_PERCENT) )
            {
                fR = fG = fB = static_cast< double >( mnC3 ) / MAX_PERCENT;
            }
            else if( mnC3 > 0 )
            {
                // base color from hue
                double fHue = static_cast< double >( mnC1 ) / MAX_DEGREE * 6.0; // interval [0.0, 6.0)
                if( fHue <= 1.0 )       { fR = 1.0; fG = fHue; }        // red...yellow
                else if( fHue <= 2.0 )  { fR = 2.0 - fHue; fG = 1.0; }  // yellow...green
                else if( fHue <= 3.0 )  { fG = 1.0; fB = fHue - 2.0; }  // green...cyan
                else if( fHue <= 4.0 )  { fG = 4.0 - fHue; fB = 1.0; }  // cyan...blue
                else if( fHue <= 5.0 )  { fR = fHue - 4.0; fB = 1.0; }  // blue...magenta
                else                    { fR = 1.0; fB = 6.0 - fHue; }  // magenta...red

                // apply saturation
                double fSat = static_cast< double >( mnC2 ) / MAX_PERCENT;
                fR = (fR - 0.5) * fSat + 0.5;
                fG = (fG - 0.5) * fSat + 0.5;
                fB = (fB - 0.5) * fSat + 0.5;

                // apply luminance
                double fLum = 2.0 * static_cast< double >( mnC3 ) / MAX_PERCENT - 1.0;  // interval [-1.0, 1.0]
                if( fLum < 0.0 )
                {
                    double fShade = fLum + 1.0; // interval [0.0, 1.0] (black...full color)
                    fR *= fShade;
                    fG *= fShade;
                    fB *= fShade;
                }
                else if( fLum > 0.0 )
                {
                    double fTint = 1.0 - fLum;  // interval [0.0, 1.0] (white...full color)
                    fR = 1.0 - ((1.0 - fR) * fTint);
                    fG = 1.0 - ((1.0 - fG) * fTint);
                    fB = 1.0 - ((1.0 - fB) * fTint);
                }
            }
            mnC1 = static_cast< sal_Int32 >( fR * 255.0 + 0.5 );
            mnC2 = static_cast< sal_Int32 >( fG * 255.0 + 0.5 );
            mnC3 = static_cast< sal_Int32 >( fB * 255.0 + 0.5 );
        }
        break;
        default:
            OSL_FAIL( "Color::toRgb - unexpected color mode" );
    }
}

void Color::toCrgb() const
{
    switch( meMode )
    {
        case ColorMode::Hsl:
            toRgb();
            [[fallthrough]];
        case ColorMode::AbsoluteRgb:
            meMode = ColorMode::RelativeRgb;
            mnC1 = lclGamma( lclRgbCompToCrgbComp( mnC1 ), DEC_GAMMA );
            mnC2 = lclGamma( lclRgbCompToCrgbComp( mnC2 ), DEC_GAMMA );
            mnC3 = lclGamma( lclRgbCompToCrgbComp( mnC3 ), DEC_GAMMA );
        break;
        case ColorMode::RelativeRgb:
            // nothing to do
        break;
        default:
            OSL_FAIL( "Color::toCrgb - unexpected color mode" );
    }
}

void Color::toHsl() const
{
    switch( meMode )
    {
        case ColorMode::RelativeRgb:
            toRgb();
            [[fallthrough]];
        case ColorMode::AbsoluteRgb:
        {
            meMode = ColorMode::Hsl;
            double fR = static_cast< double >( mnC1 ) / 255.0;  // red [0.0, 1.0]
            double fG = static_cast< double >( mnC2 ) / 255.0;  // green [0.0, 1.0]
            double fB = static_cast< double >( mnC3 ) / 255.0;  // blue [0.0, 1.0]
            double fMin = ::std::min( ::std::min( fR, fG ), fB );
            double fMax = ::std::max( ::std::max( fR, fG ), fB );
            double fD = fMax - fMin;

            using ::rtl::math::approxEqual;

            // hue: 0deg = red, 120deg = green, 240deg = blue
            if( fD == 0.0 )         // black/gray/white
                mnC1 = 0;
            else if( approxEqual(fMax, fR, 64) )   // magenta...red...yellow
                mnC1 = static_cast< sal_Int32 >( ((fG - fB) / fD * 60.0 + 360.0) * PER_DEGREE + 0.5 ) % MAX_DEGREE;
            else if( approxEqual(fMax, fG, 64) )   // yellow...green...cyan
                mnC1 = static_cast< sal_Int32 >( ((fB - fR) / fD * 60.0 + 120.0) * PER_DEGREE + 0.5 );
            else                    // cyan...blue...magenta
                mnC1 = static_cast< sal_Int32 >( ((fR - fG) / fD * 60.0 + 240.0) * PER_DEGREE + 0.5 );

            // luminance: 0% = black, 50% = full color, 100% = white
            mnC3 = static_cast< sal_Int32 >( (fMin + fMax) / 2.0 * MAX_PERCENT + 0.5 );

            // saturation: 0% = gray, 100% = full color
            if( (mnC3 == 0) || (mnC3 == MAX_PERCENT) )  // black/white
                mnC2 = 0;
            else if( mnC3 <= 50 * PER_PERCENT )         // dark...full color
                mnC2 = static_cast< sal_Int32 >( fD / (fMin + fMax) * MAX_PERCENT + 0.5 );
            else                                        // full color...light
                mnC2 = static_cast< sal_Int32 >( fD / (2.0 - fMax - fMin) * MAX_PERCENT + 0.5 );
        }
        break;
        case ColorMode::Hsl:
            // nothing to do
        break;
        default:
            OSL_FAIL( "Color::toHsl - unexpected color mode" );
    }
}

} // namespace oox

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
