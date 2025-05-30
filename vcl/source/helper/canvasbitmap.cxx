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

#include <sal/config.h>

#include <com/sun/star/lang/IndexOutOfBoundsException.hpp>
#include <com/sun/star/util/Endianness.hpp>
#include <com/sun/star/rendering/ColorComponentTag.hpp>
#include <com/sun/star/rendering/ColorSpaceType.hpp>
#include <com/sun/star/rendering/RenderingIntent.hpp>

#include <comphelper/diagnose_ex.hxx>
#include <canvasbitmap.hxx>
#include <vcl/canvastools.hxx>
#include <vcl/BitmapReadAccess.hxx>
#include <vcl/svapp.hxx>

#include <algorithm>

using namespace vcl::unotools;
using namespace ::com::sun::star;

namespace
{
    // TODO(Q3): move to o3tl bithacks or somesuch. A similar method is in canvas/canvastools.hxx

    // Good ole HAKMEM tradition. Calc number of 1 bits in 32bit word,
    // unrolled loop. See e.g. Hackers Delight, p. 66
    sal_Int32 bitcount( sal_uInt32 val )
    {
        val = val - ((val >> 1) & 0x55555555);
        val = (val & 0x33333333) + ((val >> 2) & 0x33333333);
        val = (val + (val >> 4)) & 0x0F0F0F0F;
        val = val + (val >> 8);
        val = val + (val >> 16);
        return sal_Int32(val & 0x0000003F);
    }
}

void VclCanvasBitmap::setComponentInfo( sal_uInt32 redShift, sal_uInt32 greenShift, sal_uInt32 blueShift )
{
    // sort channels in increasing order of appearance in the pixel
    // (starting with the least significant bits)
    sal_Int8 redPos(0);
    sal_Int8 greenPos(1);
    sal_Int8 bluePos(2);

    if( redShift > greenShift )
    {
        std::swap(redPos,greenPos);
        if( redShift > blueShift )
        {
            std::swap(redPos,bluePos);
            if( greenShift > blueShift )
                std::swap(greenPos,bluePos);
        }
    }
    else
    {
        if( greenShift > blueShift )
        {
            std::swap(greenPos,bluePos);
            if( redShift > blueShift )
                std::swap(redPos,bluePos);
        }
    }

    m_aComponentTags.realloc(3);
    sal_Int8* pTags = m_aComponentTags.getArray();
    pTags[redPos]   = rendering::ColorComponentTag::RGB_RED;
    pTags[greenPos] = rendering::ColorComponentTag::RGB_GREEN;
    pTags[bluePos]  = rendering::ColorComponentTag::RGB_BLUE;

    m_aComponentBitCounts.realloc(3);
    sal_Int32* pCounts = m_aComponentBitCounts.getArray();
    pCounts[redPos]   = bitcount(redShift);
    pCounts[greenPos] = bitcount(greenShift);
    pCounts[bluePos]  = bitcount(blueShift);
}

BitmapScopedReadAccess& VclCanvasBitmap::getBitmapReadAccess()
{
    // BitmapReadAccess is more expensive than BitmapInfoAccess,
    // as the latter requires also pixels, which may need converted
    // from the system format (and even fetched). Most calls here
    // need only info access, create read access only on demand.
    if(!m_pBmpReadAcc)
        m_pBmpReadAcc.emplace(m_aBitmap);
    return *m_pBmpReadAcc;
}

BitmapScopedReadAccess& VclCanvasBitmap::getAlphaReadAccess()
{
    if(!m_pAlphaReadAcc)
        m_pAlphaReadAcc.emplace(m_aAlpha);
    return *m_pAlphaReadAcc;
}

VclCanvasBitmap::VclCanvasBitmap( const BitmapEx& rBitmap ) :
    m_aBmpEx( rBitmap ),
    m_aBitmap( rBitmap.GetBitmap() ),
    m_pBmpAcc( m_aBitmap ),
    m_nBitsPerInputPixel(0),
    m_nBitsPerOutputPixel(0),
    m_nRedIndex(-1),
    m_nGreenIndex(-1),
    m_nBlueIndex(-1),
    m_nAlphaIndex(-1),
    m_nIndexIndex(-1),
    m_bPalette(false)
{
    if( m_aBmpEx.IsAlpha() )
    {
        m_aAlpha = m_aBmpEx.GetAlphaMask().GetBitmap();
        m_pAlphaAcc = m_aAlpha;
    }

    m_aLayout.ScanLines      = 0;
    m_aLayout.ScanLineBytes  = 0;
    m_aLayout.ScanLineStride = 0;
    m_aLayout.PlaneStride    = 0;
    m_aLayout.ColorSpace.clear();
    m_aLayout.Palette.clear();
    m_aLayout.IsMsbFirst     = false;

    if( !m_pBmpAcc )
        return;

    m_aLayout.ScanLines      = m_pBmpAcc->Height();
    m_aLayout.ScanLineBytes  = (m_pBmpAcc->GetBitCount()*m_pBmpAcc->Width() + 7) / 8;
    m_aLayout.ScanLineStride = m_pBmpAcc->GetScanlineSize();
    m_aLayout.PlaneStride    = 0;

    switch( m_pBmpAcc->GetScanlineFormat() )
    {
        case ScanlineFormat::N1BitMsbPal:
            m_bPalette           = true;
            m_nBitsPerInputPixel = 1;
            m_aLayout.IsMsbFirst = true;
            break;

        case ScanlineFormat::N8BitPal:
            m_bPalette           = true;
            m_nBitsPerInputPixel = 8;
            m_aLayout.IsMsbFirst = false; // doesn't matter
            break;

        case ScanlineFormat::N24BitTcBgr:
            m_bPalette           = false;
            m_nBitsPerInputPixel = 24;
            m_aLayout.IsMsbFirst = false; // doesn't matter
            setComponentInfo( static_cast<sal_uInt32>(0xff0000UL),
                              static_cast<sal_uInt32>(0x00ff00UL),
                              static_cast<sal_uInt32>(0x0000ffUL) );
            break;

        case ScanlineFormat::N24BitTcRgb:
            m_bPalette           = false;
            m_nBitsPerInputPixel = 24;
            m_aLayout.IsMsbFirst = false; // doesn't matter
            setComponentInfo( static_cast<sal_uInt32>(0x0000ffUL),
                              static_cast<sal_uInt32>(0x00ff00UL),
                              static_cast<sal_uInt32>(0xff0000UL) );
            break;

        case ScanlineFormat::N32BitTcAbgr:
        case ScanlineFormat::N32BitTcXbgr:
        {
            m_bPalette           = false;
            m_nBitsPerInputPixel = 32;
            m_aLayout.IsMsbFirst = false; // doesn't matter

            m_aComponentTags = { /* 0 */ rendering::ColorComponentTag::ALPHA,
                                 /* 1 */ rendering::ColorComponentTag::RGB_BLUE,
                                 /* 2 */ rendering::ColorComponentTag::RGB_GREEN,
                                 /* 3 */ rendering::ColorComponentTag::RGB_RED };

            m_aComponentBitCounts = { /* 0 */ 8,
                                      /* 1 */ 8,
                                      /* 2 */ 8,
                                      /* 3 */ 8 };

            m_nRedIndex   = 3;
            m_nGreenIndex = 2;
            m_nBlueIndex  = 1;
            m_nAlphaIndex = 0;
        }
        break;

        case ScanlineFormat::N32BitTcArgb:
        case ScanlineFormat::N32BitTcXrgb:
        {
            m_bPalette           = false;
            m_nBitsPerInputPixel = 32;
            m_aLayout.IsMsbFirst = false; // doesn't matter

            m_aComponentTags = { /* 0 */ rendering::ColorComponentTag::ALPHA,
                                 /* 1 */ rendering::ColorComponentTag::RGB_RED,
                                 /* 2 */ rendering::ColorComponentTag::RGB_GREEN,
                                 /* 3 */ rendering::ColorComponentTag::RGB_BLUE };

            m_aComponentBitCounts = { /* 0 */ 8,
                                      /* 1 */ 8,
                                      /* 2 */ 8,
                                      /* 3 */ 8 };

            m_nRedIndex   = 1;
            m_nGreenIndex = 2;
            m_nBlueIndex  = 3;
            m_nAlphaIndex = 0;
        }
        break;

        case ScanlineFormat::N32BitTcBgra:
        case ScanlineFormat::N32BitTcBgrx:
        {
            m_bPalette           = false;
            m_nBitsPerInputPixel = 32;
            m_aLayout.IsMsbFirst = false; // doesn't matter

            m_aComponentTags = { /* 0 */ rendering::ColorComponentTag::RGB_BLUE,
                                 /* 1 */ rendering::ColorComponentTag::RGB_GREEN,
                                 /* 2 */ rendering::ColorComponentTag::RGB_RED,
                                 /* 3 */ rendering::ColorComponentTag::ALPHA };

            m_aComponentBitCounts = { /* 0 */ 8,
                                      /* 1 */ 8,
                                      /* 2 */ 8,
                                      /* 3 */ 8 };

            m_nRedIndex   = 2;
            m_nGreenIndex = 1;
            m_nBlueIndex  = 0;
            m_nAlphaIndex = 3;
        }
        break;

        case ScanlineFormat::N32BitTcRgba:
        case ScanlineFormat::N32BitTcRgbx:
        {
            m_bPalette           = false;
            m_nBitsPerInputPixel = 32;
            m_aLayout.IsMsbFirst = false; // doesn't matter

            m_aComponentTags = { /* 0 */ rendering::ColorComponentTag::RGB_RED,
                                 /* 1 */ rendering::ColorComponentTag::RGB_GREEN,
                                 /* 2 */ rendering::ColorComponentTag::RGB_BLUE,
                                 /* 3 */ rendering::ColorComponentTag::ALPHA };

            m_aComponentBitCounts = { /* 0 */ 8,
                                      /* 1 */ 8,
                                      /* 2 */ 8,
                                      /* 3 */ 8 };

            m_nRedIndex   = 0;
            m_nGreenIndex = 1;
            m_nBlueIndex  = 2;
            m_nAlphaIndex = 3;
        }
        break;

        default:
            OSL_FAIL( "unsupported bitmap format" );
            break;
    }

    if( m_bPalette )
    {
        m_aComponentTags = { rendering::ColorComponentTag::INDEX };

        m_aComponentBitCounts = { m_nBitsPerInputPixel };

        m_nIndexIndex = 0;
    }

    m_nBitsPerOutputPixel = m_nBitsPerInputPixel;
    if( !m_aBmpEx.IsAlpha() )
        return;

    // TODO(P1): need to interleave alpha with bitmap data -
    // won't fuss with less-than-8 bit for now
    m_nBitsPerOutputPixel = std::max(sal_Int32(8),m_nBitsPerInputPixel);

    // check whether alpha goes in front or behind the
    // bitcount sequence. If pixel format is little endian,
    // put it behind all the other channels. If it's big
    // endian, put it in front (because later, the actual data
    // always gets written after the pixel data)

    // TODO(Q1): slight catch - in the case of the
    // BMP_FORMAT_32BIT_XX_ARGB formats, duplicate alpha
    // channels might happen!
    m_aComponentTags.realloc(m_aComponentTags.getLength()+1);
    m_aComponentTags.getArray()[m_aComponentTags.getLength()-1] = rendering::ColorComponentTag::ALPHA;

    m_aComponentBitCounts.realloc(m_aComponentBitCounts.getLength()+1);
    m_aComponentBitCounts.getArray()[m_aComponentBitCounts.getLength()-1] = m_aBmpEx.IsAlpha() ? 8 : 1;

    // always add a full byte to the pixel size, otherwise
    // pixel packing hell breaks loose.
    m_nBitsPerOutputPixel += 8;

    // adapt scanline parameters
    const Size aSize = m_aBitmap.GetSizePixel();
    m_aLayout.ScanLineBytes  =
    m_aLayout.ScanLineStride = (aSize.Width()*m_nBitsPerOutputPixel + 7)/8;
}

VclCanvasBitmap::~VclCanvasBitmap()
{
}

// XBitmap
geometry::IntegerSize2D SAL_CALL VclCanvasBitmap::getSize()
{
    SolarMutexGuard aGuard;
    return integerSize2DFromSize( m_aBitmap.GetSizePixel() );
}

sal_Bool SAL_CALL VclCanvasBitmap::hasAlpha()
{
    SolarMutexGuard aGuard;
    return m_aBmpEx.IsAlpha();
}

uno::Reference< rendering::XBitmap > SAL_CALL VclCanvasBitmap::getScaledBitmap( const geometry::RealSize2D& newSize,
                                                                                sal_Bool beFast )
{
    SolarMutexGuard aGuard;

    BitmapEx aNewBmp( m_aBitmap );
    aNewBmp.Scale( sizeFromRealSize2D( newSize ), beFast ? BmpScaleFlag::Default : BmpScaleFlag::BestQuality );
    return uno::Reference<rendering::XBitmap>( new VclCanvasBitmap( aNewBmp ) );
}

// XIntegerReadOnlyBitmap
uno::Sequence< sal_Int8 > SAL_CALL VclCanvasBitmap::getData( rendering::IntegerBitmapLayout&     bitmapLayout,
                                                             const geometry::IntegerRectangle2D& rect )
{
    SolarMutexGuard aGuard;

    bitmapLayout = getMemoryLayout();

    const ::tools::Rectangle aRequestedArea( vcl::unotools::rectangleFromIntegerRectangle2D(rect) );
    if( aRequestedArea.IsEmpty() )
        return uno::Sequence< sal_Int8 >();

    // Invalid/empty bitmap: no data available
    if( !m_pBmpAcc )
        throw lang::IndexOutOfBoundsException();
    if( m_aBmpEx.IsAlpha() && !m_pAlphaAcc )
        throw lang::IndexOutOfBoundsException();

    if( aRequestedArea.Left() < 0 || aRequestedArea.Top() < 0 ||
        aRequestedArea.Right() > m_pBmpAcc->Width() ||
        aRequestedArea.Bottom() > m_pBmpAcc->Height() )
    {
        throw lang::IndexOutOfBoundsException();
    }

    uno::Sequence< sal_Int8 > aRet;
    tools::Rectangle aRequestedBytes( aRequestedArea );

    // adapt to byte boundaries
    aRequestedBytes.SetLeft( aRequestedArea.Left()*m_nBitsPerOutputPixel/8 );
    aRequestedBytes.SetRight( (aRequestedArea.Right()*m_nBitsPerOutputPixel + 7)/8 );

    // copy stuff to output sequence
    aRet.realloc(aRequestedBytes.getOpenWidth()*aRequestedBytes.getOpenHeight());
    sal_Int8* pOutBuf = aRet.getArray();

    bitmapLayout.ScanLines     = aRequestedBytes.getOpenHeight();
    bitmapLayout.ScanLineBytes =
    bitmapLayout.ScanLineStride= aRequestedBytes.getOpenWidth();

    sal_Int32 nScanlineStride=bitmapLayout.ScanLineStride;
    if (m_pBmpAcc->IsBottomUp())
    {
        pOutBuf += bitmapLayout.ScanLineStride*(aRequestedBytes.getOpenHeight()-1);
        nScanlineStride *= -1;
    }

    if( !m_aBmpEx.IsAlpha() )
    {
        BitmapScopedReadAccess& pBmpAcc = getBitmapReadAccess();
        OSL_ENSURE(pBmpAcc,"Invalid bmp read access");

        // can return bitmap data as-is
        for( tools::Long y=aRequestedBytes.Top(); y<aRequestedBytes.Bottom(); ++y )
        {
            Scanline pScan = pBmpAcc->GetScanline(y);
            memcpy(pOutBuf, pScan+aRequestedBytes.Left(), aRequestedBytes.getOpenWidth());
            pOutBuf += nScanlineStride;
        }
    }
    else
    {
        BitmapScopedReadAccess& pBmpAcc = getBitmapReadAccess();
        BitmapScopedReadAccess& pAlphaAcc = getAlphaReadAccess();
        OSL_ENSURE(pBmpAcc,"Invalid bmp read access");
        OSL_ENSURE(pAlphaAcc,"Invalid alpha read access");

        // interleave alpha with bitmap data - note, bitcount is
        // always integer multiple of 8
        OSL_ENSURE((m_nBitsPerOutputPixel & 0x07) == 0,
                   "Transparent bitmap bitcount not integer multiple of 8" );

        for( tools::Long y=aRequestedArea.Top(); y<aRequestedArea.Bottom(); ++y )
        {
            sal_Int8* pOutScan = pOutBuf;

            if( m_nBitsPerInputPixel < 8 )
            {
                // input less than a byte - copy via GetPixel()
                for( tools::Long x=aRequestedArea.Left(); x<aRequestedArea.Right(); ++x )
                {
                    *pOutScan++ = pBmpAcc->GetPixelIndex(y,x);
                    // vcl used to store transparency. Now it stores alpha. But we need the UNO
                    // interface to still preserve the old interface.
                    *pOutScan++ = 255 - pAlphaAcc->GetPixelIndex(y,x);
                }
            }
            else
            {
                const tools::Long nNonAlphaBytes( m_nBitsPerInputPixel/8 );
                const tools::Long nScanlineOffsetLeft(aRequestedArea.Left()*nNonAlphaBytes);
                Scanline  pScan = pBmpAcc->GetScanline(y) + nScanlineOffsetLeft;
                Scanline pScanlineAlpha = pAlphaAcc->GetScanline( y );

                // input integer multiple of byte - copy directly
                for( tools::Long x=aRequestedArea.Left(); x<aRequestedArea.Right(); ++x )
                {
                    for( tools::Long i=0; i<nNonAlphaBytes; ++i )
                        *pOutScan++ = *pScan++;
                    // vcl used to store transparency. Now it stores alpha. But we need the UNO
                    // interface to still preserve the old interface.
                    *pOutScan++ = 255 - pAlphaAcc->GetIndexFromData( pScanlineAlpha, x );
                }
            }

            pOutBuf += nScanlineStride;
        }
    }

    return aRet;
}

uno::Sequence< sal_Int8 > SAL_CALL VclCanvasBitmap::getPixel( rendering::IntegerBitmapLayout&   bitmapLayout,
                                                              const geometry::IntegerPoint2D&   pos )
{
    SolarMutexGuard aGuard;

    bitmapLayout = getMemoryLayout();

    // Invalid/empty bitmap: no data available
    if( !m_pBmpAcc )
        throw lang::IndexOutOfBoundsException();
    if( m_aBmpEx.IsAlpha() && !m_pAlphaAcc )
        throw lang::IndexOutOfBoundsException();

    if( pos.X < 0 || pos.Y < 0 ||
        pos.X > m_pBmpAcc->Width() || pos.Y > m_pBmpAcc->Height() )
    {
        throw lang::IndexOutOfBoundsException();
    }

    uno::Sequence< sal_Int8 > aRet((m_nBitsPerOutputPixel + 7)/8);
    sal_Int8* pOutBuf = aRet.getArray();

    // copy stuff to output sequence
    bitmapLayout.ScanLines     = 1;
    bitmapLayout.ScanLineBytes =
    bitmapLayout.ScanLineStride= aRet.getLength();

    const tools::Long nScanlineLeftOffset( pos.X*m_nBitsPerInputPixel/8 );
    if( !m_aBmpEx.IsAlpha() )
    {
        BitmapScopedReadAccess& pBmpAcc = getBitmapReadAccess();
        assert(pBmpAcc && "Invalid bmp read access");

        // can return bitmap data as-is
        Scanline pScan = pBmpAcc->GetScanline(pos.Y);
        memcpy(pOutBuf, pScan+nScanlineLeftOffset, aRet.getLength() );
    }
    else
    {
        BitmapScopedReadAccess& pBmpAcc = getBitmapReadAccess();
        BitmapScopedReadAccess& pAlphaAcc = getAlphaReadAccess();
        assert(pBmpAcc && "Invalid bmp read access");
        assert(pAlphaAcc && "Invalid alpha read access");

        // interleave alpha with bitmap data - note, bitcount is
        // always integer multiple of 8
        assert((m_nBitsPerOutputPixel & 0x07) == 0 &&
                   "Transparent bitmap bitcount not integer multiple of 8" );

        if( m_nBitsPerInputPixel < 8 )
        {
            // input less than a byte - copy via GetPixel()
            *pOutBuf++ = pBmpAcc->GetPixelIndex(pos.Y,pos.X);
            // vcl used to store transparency. Now it stores alpha. But we need the UNO
            // interface to still preserve the old interface.
            *pOutBuf   = 255 - pAlphaAcc->GetPixelIndex(pos.Y,pos.X);
        }
        else
        {
            const tools::Long nNonAlphaBytes( m_nBitsPerInputPixel/8 );
            Scanline  pScan = pBmpAcc->GetScanline(pos.Y);

            // input integer multiple of byte - copy directly
            memcpy(pOutBuf, pScan+nScanlineLeftOffset, nNonAlphaBytes );
            pOutBuf += nNonAlphaBytes;
            // vcl used to store transparency. Now it stores alpha. But we need the UNO
            // interface to still preserve the old interface.
            *pOutBuf++ = 255 - pAlphaAcc->GetPixelIndex(pos.Y,pos.X);
        }
    }

    return aRet;
}

uno::Reference< rendering::XBitmapPalette > VclCanvasBitmap::getPalette()
{
    SolarMutexGuard aGuard;

    uno::Reference< XBitmapPalette > aRet;
    if( m_bPalette )
        aRet.set(this);

    return aRet;
}

rendering::IntegerBitmapLayout SAL_CALL VclCanvasBitmap::getMemoryLayout()
{
    SolarMutexGuard aGuard;

    rendering::IntegerBitmapLayout aLayout( m_aLayout );

    // only set references to self on separate copy of
    // IntegerBitmapLayout - if we'd set that on m_aLayout, we'd have
    // a circular reference!
    if( m_bPalette )
        aLayout.Palette.set( this );

    aLayout.ColorSpace.set( this );

    return aLayout;
}

sal_Int32 SAL_CALL VclCanvasBitmap::getNumberOfEntries()
{
    SolarMutexGuard aGuard;

    if( !m_pBmpAcc )
        return 0;

    return m_pBmpAcc->HasPalette() ? m_pBmpAcc->GetPaletteEntryCount() : 0 ;
}

sal_Bool SAL_CALL VclCanvasBitmap::getIndex( uno::Sequence< double >& o_entry, sal_Int32 nIndex )
{
    SolarMutexGuard aGuard;

    const sal_uInt16 nCount( m_pBmpAcc ?
                         (m_pBmpAcc->HasPalette() ? m_pBmpAcc->GetPaletteEntryCount() : 0 ) : 0 );
    OSL_ENSURE(nIndex >= 0 && nIndex < nCount,"Palette index out of range");
    if( nIndex < 0 || nIndex >= nCount )
        throw lang::IndexOutOfBoundsException(u"Palette index out of range"_ustr,
                                              static_cast<rendering::XBitmapPalette*>(this));

    const BitmapColor aCol = m_pBmpAcc->GetPaletteColor(sal::static_int_cast<sal_uInt16>(nIndex));
    o_entry.realloc(3);
    double* pColor=o_entry.getArray();
    pColor[0] = aCol.GetRed();
    pColor[1] = aCol.GetGreen();
    pColor[2] = aCol.GetBlue();

    return true; // no palette transparency here.
}

sal_Bool SAL_CALL VclCanvasBitmap::setIndex( const uno::Sequence< double >&, sal_Bool, sal_Int32 nIndex )
{
    SolarMutexGuard aGuard;

    const sal_uInt16 nCount( m_pBmpAcc ?
                         (m_pBmpAcc->HasPalette() ? m_pBmpAcc->GetPaletteEntryCount() : 0 ) : 0 );

    OSL_ENSURE(nIndex >= 0 && nIndex < nCount,"Palette index out of range");
    if( nIndex < 0 || nIndex >= nCount )
        throw lang::IndexOutOfBoundsException(u"Palette index out of range"_ustr,
                                              static_cast<rendering::XBitmapPalette*>(this));

    return false; // read-only implementation
}

uno::Reference< rendering::XColorSpace > SAL_CALL VclCanvasBitmap::getColorSpace(  )
{
    // this is the method from XBitmapPalette. Return palette color
    // space here
    static uno::Reference<rendering::XColorSpace> gColorSpace = vcl::unotools::createStandardColorSpace();
    return gColorSpace;
}

sal_Int8 SAL_CALL VclCanvasBitmap::getType(  )
{
    return rendering::ColorSpaceType::RGB;
}

uno::Sequence< ::sal_Int8 > SAL_CALL VclCanvasBitmap::getComponentTags(  )
{
    SolarMutexGuard aGuard;
    return m_aComponentTags;
}

sal_Int8 SAL_CALL VclCanvasBitmap::getRenderingIntent(  )
{
    return rendering::RenderingIntent::PERCEPTUAL;
}

uno::Sequence< ::beans::PropertyValue > SAL_CALL VclCanvasBitmap::getProperties(  )
{
    return uno::Sequence< ::beans::PropertyValue >();
}

uno::Sequence< double > SAL_CALL VclCanvasBitmap::convertColorSpace( const uno::Sequence< double >& deviceColor,
                                                                     const uno::Reference< ::rendering::XColorSpace >& targetColorSpace )
{
    // TODO(P3): if we know anything about target
    // colorspace, this can be greatly sped up
    uno::Sequence<rendering::ARGBColor> aIntermediate(
        convertToARGB(deviceColor));
    return targetColorSpace->convertFromARGB(aIntermediate);
}

uno::Sequence<rendering::RGBColor> SAL_CALL VclCanvasBitmap::convertToRGB( const uno::Sequence< double >& deviceColor )
{
    SolarMutexGuard aGuard;

    const std::size_t nLen( deviceColor.getLength() );
    const sal_Int32 nComponentsPerPixel(m_aComponentTags.getLength());
    ENSURE_ARG_OR_THROW2(nLen%nComponentsPerPixel==0,
                         "number of channels no multiple of pixel element count",
                         static_cast<rendering::XBitmapPalette*>(this), 01);

    uno::Sequence< rendering::RGBColor > aRes(nLen/nComponentsPerPixel);
    rendering::RGBColor* pOut( aRes.getArray() );

    if( m_bPalette )
    {
        OSL_ENSURE(m_nIndexIndex != -1,
                   "Invalid color channel indices");
        ENSURE_OR_THROW(m_pBmpAcc,
                        "Unable to get BitmapAccess");

        for( std::size_t i=0; i<nLen; i+=nComponentsPerPixel )
        {
            const BitmapColor aCol = m_pBmpAcc->GetPaletteColor(
                sal::static_int_cast<sal_uInt16>(deviceColor[i+m_nIndexIndex]));

            // TODO(F3): Convert result to sRGB color space
            *pOut++ = rendering::RGBColor(toDoubleColor(aCol.GetRed()),
                                          toDoubleColor(aCol.GetGreen()),
                                          toDoubleColor(aCol.GetBlue()));
        }
    }
    else
    {
        OSL_ENSURE(m_nRedIndex != -1 && m_nGreenIndex != -1 && m_nBlueIndex != -1,
                   "Invalid color channel indices");

        for( std::size_t i=0; i<nLen; i+=nComponentsPerPixel )
        {
            // TODO(F3): Convert result to sRGB color space
            *pOut++ = rendering::RGBColor(
                deviceColor[i+m_nRedIndex],
                deviceColor[i+m_nGreenIndex],
                deviceColor[i+m_nBlueIndex]);
        }
    }

    return aRes;
}

uno::Sequence<rendering::ARGBColor> SAL_CALL VclCanvasBitmap::convertToARGB( const uno::Sequence< double >& deviceColor )
{
    SolarMutexGuard aGuard;

    const std::size_t nLen( deviceColor.getLength() );
    const sal_Int32 nComponentsPerPixel(m_aComponentTags.getLength());
    ENSURE_ARG_OR_THROW2(nLen%nComponentsPerPixel==0,
                         "number of channels no multiple of pixel element count",
                         static_cast<rendering::XBitmapPalette*>(this), 01);

    uno::Sequence< rendering::ARGBColor > aRes(nLen/nComponentsPerPixel);
    rendering::ARGBColor* pOut( aRes.getArray() );

    if( m_bPalette )
    {
        OSL_ENSURE(m_nIndexIndex != -1,
                   "Invalid color channel indices");
        ENSURE_OR_THROW(m_pBmpAcc,
                        "Unable to get BitmapAccess");

        for( std::size_t i=0; i<nLen; i+=nComponentsPerPixel )
        {
            const BitmapColor aCol = m_pBmpAcc->GetPaletteColor(
                sal::static_int_cast<sal_uInt16>(deviceColor[i+m_nIndexIndex]));

            // TODO(F3): Convert result to sRGB color space
            const double nAlpha( m_nAlphaIndex != -1 ? 1.0 - deviceColor[i+m_nAlphaIndex] : 1.0 );
            *pOut++ = rendering::ARGBColor(nAlpha,
                                           toDoubleColor(aCol.GetRed()),
                                           toDoubleColor(aCol.GetGreen()),
                                           toDoubleColor(aCol.GetBlue()));
        }
    }
    else
    {
        OSL_ENSURE(m_nRedIndex != -1 && m_nGreenIndex != -1 && m_nBlueIndex != -1,
                   "Invalid color channel indices");

        for( std::size_t i=0; i<nLen; i+=nComponentsPerPixel )
        {
            // TODO(F3): Convert result to sRGB color space
            const double nAlpha( m_nAlphaIndex != -1 ? 1.0 - deviceColor[i+m_nAlphaIndex] : 1.0 );
            *pOut++ = rendering::ARGBColor(
                nAlpha,
                deviceColor[i+m_nRedIndex],
                deviceColor[i+m_nGreenIndex],
                deviceColor[i+m_nBlueIndex]);
        }
    }

    return aRes;
}

uno::Sequence<rendering::ARGBColor> SAL_CALL VclCanvasBitmap::convertToPARGB( const uno::Sequence< double >& deviceColor )
{
    SolarMutexGuard aGuard;

    const std::size_t nLen( deviceColor.getLength() );
    const sal_Int32 nComponentsPerPixel(m_aComponentTags.getLength());
    ENSURE_ARG_OR_THROW2(nLen%nComponentsPerPixel==0,
                         "number of channels no multiple of pixel element count",
                         static_cast<rendering::XBitmapPalette*>(this), 01);

    uno::Sequence< rendering::ARGBColor > aRes(nLen/nComponentsPerPixel);
    rendering::ARGBColor* pOut( aRes.getArray() );

    if( m_bPalette )
    {
        OSL_ENSURE(m_nIndexIndex != -1,
                   "Invalid color channel indices");
        ENSURE_OR_THROW(m_pBmpAcc,
                        "Unable to get BitmapAccess");

        for( std::size_t i=0; i<nLen; i+=nComponentsPerPixel )
        {
            const BitmapColor aCol = m_pBmpAcc->GetPaletteColor(
                sal::static_int_cast<sal_uInt16>(deviceColor[i+m_nIndexIndex]));

            // TODO(F3): Convert result to sRGB color space
            const double nAlpha( m_nAlphaIndex != -1 ? 1.0 - deviceColor[i+m_nAlphaIndex] : 1.0 );
            *pOut++ = rendering::ARGBColor(nAlpha,
                                           nAlpha*toDoubleColor(aCol.GetRed()),
                                           nAlpha*toDoubleColor(aCol.GetGreen()),
                                           nAlpha*toDoubleColor(aCol.GetBlue()));
        }
    }
    else
    {
        OSL_ENSURE(m_nRedIndex != -1 && m_nGreenIndex != -1 && m_nBlueIndex != -1,
                   "Invalid color channel indices");

        for( std::size_t i=0; i<nLen; i+=nComponentsPerPixel )
        {
            // TODO(F3): Convert result to sRGB color space
            const double nAlpha( m_nAlphaIndex != -1 ? 1.0 - deviceColor[i+m_nAlphaIndex] : 1.0 );
            *pOut++ = rendering::ARGBColor(
                nAlpha,
                nAlpha*deviceColor[i+m_nRedIndex],
                nAlpha*deviceColor[i+m_nGreenIndex],
                nAlpha*deviceColor[i+m_nBlueIndex]);
        }
    }

    return aRes;
}

uno::Sequence< double > SAL_CALL VclCanvasBitmap::convertFromRGB( const uno::Sequence<rendering::RGBColor>& rgbColor )
{
    SolarMutexGuard aGuard;

    const std::size_t nLen( rgbColor.getLength() );
    const sal_Int32 nComponentsPerPixel(m_aComponentTags.getLength());

    uno::Sequence< double > aRes(nLen*nComponentsPerPixel);
    double* pColors=aRes.getArray();

    if( m_bPalette )
    {
        for( const auto& rIn : rgbColor )
        {
            pColors[m_nIndexIndex] = m_pBmpAcc->GetBestPaletteIndex(
                    BitmapColor(toByteColor(rIn.Red),
                                toByteColor(rIn.Green),
                                toByteColor(rIn.Blue)));
            if( m_nAlphaIndex != -1 )
                pColors[m_nAlphaIndex] = 1.0;

            pColors += nComponentsPerPixel;
        }
    }
    else
    {
        for( const auto& rIn : rgbColor )
        {
            pColors[m_nRedIndex]   = rIn.Red;
            pColors[m_nGreenIndex] = rIn.Green;
            pColors[m_nBlueIndex]  = rIn.Blue;
            if( m_nAlphaIndex != -1 )
                pColors[m_nAlphaIndex] = 1.0;

            pColors += nComponentsPerPixel;
        }
    }
    return aRes;
}

uno::Sequence< double > SAL_CALL VclCanvasBitmap::convertFromARGB( const uno::Sequence<rendering::ARGBColor>& rgbColor )
{
    SolarMutexGuard aGuard;

    const std::size_t nLen( rgbColor.getLength() );
    const sal_Int32 nComponentsPerPixel(m_aComponentTags.getLength());

    uno::Sequence< double > aRes(nLen*nComponentsPerPixel);
    double* pColors=aRes.getArray();

    if( m_bPalette )
    {
        for( const auto& rIn : rgbColor )
        {
            pColors[m_nIndexIndex] = m_pBmpAcc->GetBestPaletteIndex(
                    BitmapColor(toByteColor(rIn.Red),
                                toByteColor(rIn.Green),
                                toByteColor(rIn.Blue)));
            if( m_nAlphaIndex != -1 )
                pColors[m_nAlphaIndex] = rIn.Alpha;

            pColors += nComponentsPerPixel;
        }
    }
    else
    {
        for( const auto& rIn : rgbColor )
        {
            pColors[m_nRedIndex]   = rIn.Red;
            pColors[m_nGreenIndex] = rIn.Green;
            pColors[m_nBlueIndex]  = rIn.Blue;
            if( m_nAlphaIndex != -1 )
                pColors[m_nAlphaIndex] = rIn.Alpha;

            pColors += nComponentsPerPixel;
        }
    }
    return aRes;
}

uno::Sequence< double > SAL_CALL VclCanvasBitmap::convertFromPARGB( const uno::Sequence<rendering::ARGBColor>& rgbColor )
{
    SolarMutexGuard aGuard;

    const std::size_t nLen( rgbColor.getLength() );
    const sal_Int32 nComponentsPerPixel(m_aComponentTags.getLength());

    uno::Sequence< double > aRes(nLen*nComponentsPerPixel);
    double* pColors=aRes.getArray();

    if( m_bPalette )
    {
        for( const auto& rIn : rgbColor )
        {
            const double nAlpha( rIn.Alpha );
            pColors[m_nIndexIndex] = m_pBmpAcc->GetBestPaletteIndex(
                    BitmapColor(toByteColor(rIn.Red / nAlpha),
                                toByteColor(rIn.Green / nAlpha),
                                toByteColor(rIn.Blue / nAlpha)));
            if( m_nAlphaIndex != -1 )
                pColors[m_nAlphaIndex] = nAlpha;

            pColors += nComponentsPerPixel;
        }
    }
    else
    {
        for( const auto& rIn : rgbColor )
        {
            const double nAlpha( rIn.Alpha );
            pColors[m_nRedIndex]   = rIn.Red / nAlpha;
            pColors[m_nGreenIndex] = rIn.Green / nAlpha;
            pColors[m_nBlueIndex]  = rIn.Blue / nAlpha;
            if( m_nAlphaIndex != -1 )
                pColors[m_nAlphaIndex] = nAlpha;

            pColors += nComponentsPerPixel;
        }
    }
    return aRes;
}

sal_Int32 SAL_CALL VclCanvasBitmap::getBitsPerPixel(  )
{
    return m_nBitsPerOutputPixel;
}

uno::Sequence< ::sal_Int32 > SAL_CALL VclCanvasBitmap::getComponentBitCounts(  )
{
    return m_aComponentBitCounts;
}

sal_Int8 SAL_CALL VclCanvasBitmap::getEndianness(  )
{
    return util::Endianness::LITTLE;
}

uno::Sequence<double> SAL_CALL VclCanvasBitmap::convertFromIntegerColorSpace( const uno::Sequence< ::sal_Int8 >& deviceColor,
                                                                              const uno::Reference< ::rendering::XColorSpace >& targetColorSpace )
{
    if( dynamic_cast<VclCanvasBitmap*>(targetColorSpace.get()) )
    {
        SolarMutexGuard aGuard;

        const std::size_t nLen( deviceColor.getLength() );
        const sal_Int32 nComponentsPerPixel(m_aComponentTags.getLength());
        ENSURE_ARG_OR_THROW2(nLen%nComponentsPerPixel==0,
                             "number of channels no multiple of pixel element count",
                             static_cast<rendering::XBitmapPalette*>(this), 01);

        uno::Sequence<double> aRes(nLen);
        double* pOut( aRes.getArray() );

        if( m_bPalette )
        {
            OSL_ENSURE(m_nIndexIndex != -1,
                       "Invalid color channel indices");
            ENSURE_OR_THROW(m_pBmpAcc,
                            "Unable to get BitmapAccess");

            for( std::size_t i=0; i<nLen; i+=nComponentsPerPixel )
            {
                const BitmapColor aCol = m_pBmpAcc->GetPaletteColor(
                    sal::static_int_cast<sal_uInt16>(deviceColor[i+m_nIndexIndex]));

                // TODO(F3): Convert result to sRGB color space
                const double nAlpha( m_nAlphaIndex != -1 ? 1.0 - deviceColor[i+m_nAlphaIndex] : 1.0 );
                *pOut++ = toDoubleColor(aCol.GetRed());
                *pOut++ = toDoubleColor(aCol.GetGreen());
                *pOut++ = toDoubleColor(aCol.GetBlue());
                *pOut++ = nAlpha;
            }
        }
        else
        {
            OSL_ENSURE(m_nRedIndex != -1 && m_nGreenIndex != -1 && m_nBlueIndex != -1,
                       "Invalid color channel indices");

            for( std::size_t i=0; i<nLen; i+=nComponentsPerPixel )
            {
                // TODO(F3): Convert result to sRGB color space
                const double nAlpha( m_nAlphaIndex != -1 ? 1.0 - deviceColor[i+m_nAlphaIndex] : 1.0 );
                *pOut++ = deviceColor[i+m_nRedIndex];
                *pOut++ = deviceColor[i+m_nGreenIndex];
                *pOut++ = deviceColor[i+m_nBlueIndex];
                *pOut++ = nAlpha;
            }
        }

        return aRes;
    }
    else
    {
        // TODO(P3): if we know anything about target
        // colorspace, this can be greatly sped up
        uno::Sequence<rendering::ARGBColor> aIntermediate(
            convertIntegerToARGB(deviceColor));
        return targetColorSpace->convertFromARGB(aIntermediate);
    }
}

uno::Sequence< ::sal_Int8 > SAL_CALL VclCanvasBitmap::convertToIntegerColorSpace( const uno::Sequence< ::sal_Int8 >& deviceColor,
                                                                                  const uno::Reference< ::rendering::XIntegerBitmapColorSpace >& targetColorSpace )
{
    if( dynamic_cast<VclCanvasBitmap*>(targetColorSpace.get()) )
    {
        // it's us, so simply pass-through the data
        return deviceColor;
    }
    else
    {
        // TODO(P3): if we know anything about target
        // colorspace, this can be greatly sped up
        uno::Sequence<rendering::ARGBColor> aIntermediate(
            convertIntegerToARGB(deviceColor));
        return targetColorSpace->convertIntegerFromARGB(aIntermediate);
    }
}

uno::Sequence<rendering::RGBColor> SAL_CALL VclCanvasBitmap::convertIntegerToRGB( const uno::Sequence< ::sal_Int8 >& deviceColor )
{
    SolarMutexGuard aGuard;

    const sal_uInt8*     pIn( reinterpret_cast<const sal_uInt8*>(deviceColor.getConstArray()) );
    const std::size_t nLen( deviceColor.getLength() );
    const sal_Int32 nNumColors((nLen*8 + m_nBitsPerOutputPixel-1)/m_nBitsPerOutputPixel);

    uno::Sequence< rendering::RGBColor > aRes(nNumColors);
    rendering::RGBColor* pOut( aRes.getArray() );

    BitmapScopedReadAccess& pBmpAcc = getBitmapReadAccess();
    ENSURE_OR_THROW(pBmpAcc,
                    "Unable to get BitmapAccess");

    if( m_aBmpEx.IsAlpha() )
    {
        const sal_Int32 nBytesPerPixel((m_nBitsPerOutputPixel+7)/8);
        for( std::size_t i=0; i<nLen; i+=nBytesPerPixel )
        {
            // if palette, index is guaranteed to be 8 bit
            const BitmapColor aCol =
                m_bPalette ?
                pBmpAcc->GetPaletteColor(*pIn) :
                pBmpAcc->GetPixelFromData(pIn,0);

            // TODO(F3): Convert result to sRGB color space
            *pOut++ = rendering::RGBColor(toDoubleColor(aCol.GetRed()),
                                          toDoubleColor(aCol.GetGreen()),
                                          toDoubleColor(aCol.GetBlue()));
            // skips alpha
            pIn += nBytesPerPixel;
        }
    }
    else
    {
        for( sal_Int32 i=0; i<nNumColors; ++i )
        {
            const BitmapColor aCol =
                m_bPalette ?
                pBmpAcc->GetPaletteColor( pBmpAcc->GetPixelFromData( pIn, i ).GetIndex()) :
                pBmpAcc->GetPixelFromData(pIn, i);

            // TODO(F3): Convert result to sRGB color space
            *pOut++ = rendering::RGBColor(toDoubleColor(aCol.GetRed()),
                                          toDoubleColor(aCol.GetGreen()),
                                          toDoubleColor(aCol.GetBlue()));
        }
    }

    return aRes;
}

uno::Sequence<rendering::ARGBColor> SAL_CALL VclCanvasBitmap::convertIntegerToARGB( const uno::Sequence< ::sal_Int8 >& deviceColor )
{
    SolarMutexGuard aGuard;

    const sal_uInt8*     pIn( reinterpret_cast<const sal_uInt8*>(deviceColor.getConstArray()) );
    const std::size_t nLen( deviceColor.getLength() );
    const sal_Int32 nNumColors((nLen*8 + m_nBitsPerOutputPixel-1)/m_nBitsPerOutputPixel);

    uno::Sequence< rendering::ARGBColor > aRes(nNumColors);
    rendering::ARGBColor* pOut( aRes.getArray() );

    BitmapScopedReadAccess& pBmpAcc = getBitmapReadAccess();
    ENSURE_OR_THROW(pBmpAcc,
                    "Unable to get BitmapAccess");

    if( m_aBmpEx.IsAlpha() )
    {
        const tools::Long      nNonAlphaBytes( (m_nBitsPerInputPixel+7)/8 );
        const sal_Int32 nBytesPerPixel((m_nBitsPerOutputPixel+7)/8);
        for( std::size_t i=0; i<nLen; i+=nBytesPerPixel )
        {
            // if palette, index is guaranteed to be 8 bit
            const BitmapColor aCol =
                m_bPalette ?
                pBmpAcc->GetPaletteColor(*pIn) :
                pBmpAcc->GetPixelFromData(pIn,0);

            // TODO(F3): Convert result to sRGB color space
            *pOut++ = rendering::ARGBColor(1.0 - toDoubleColor(pIn[nNonAlphaBytes]),
                                           toDoubleColor(aCol.GetRed()),
                                           toDoubleColor(aCol.GetGreen()),
                                           toDoubleColor(aCol.GetBlue()));
            pIn += nBytesPerPixel;
        }
    }
    else
    {
        for( sal_Int32 i=0; i<nNumColors; ++i )
        {
            const BitmapColor aCol =
                m_bPalette ?
                pBmpAcc->GetPaletteColor( pBmpAcc->GetPixelFromData( pIn, i ).GetIndex() ) :
                pBmpAcc->GetPixelFromData(pIn, i);

            // TODO(F3): Convert result to sRGB color space
            *pOut++ = rendering::ARGBColor(1.0,
                                           toDoubleColor(aCol.GetRed()),
                                           toDoubleColor(aCol.GetGreen()),
                                           toDoubleColor(aCol.GetBlue()));
        }
    }

    return aRes;
}

uno::Sequence<rendering::ARGBColor> SAL_CALL VclCanvasBitmap::convertIntegerToPARGB( const uno::Sequence< ::sal_Int8 >& deviceColor )
{
    SolarMutexGuard aGuard;

    const sal_uInt8*     pIn( reinterpret_cast<const sal_uInt8*>(deviceColor.getConstArray()) );
    const std::size_t nLen( deviceColor.getLength() );
    const sal_Int32 nNumColors((nLen*8 + m_nBitsPerOutputPixel-1)/m_nBitsPerOutputPixel);

    uno::Sequence< rendering::ARGBColor > aRes(nNumColors);
    rendering::ARGBColor* pOut( aRes.getArray() );

    BitmapScopedReadAccess& pBmpAcc = getBitmapReadAccess();
    ENSURE_OR_THROW(pBmpAcc,
                    "Unable to get BitmapAccess");

    if( m_aBmpEx.IsAlpha() )
    {
        const tools::Long      nNonAlphaBytes( (m_nBitsPerInputPixel+7)/8 );
        const sal_Int32 nBytesPerPixel((m_nBitsPerOutputPixel+7)/8);
        for( std::size_t i=0; i<nLen; i+=nBytesPerPixel )
        {
            // if palette, index is guaranteed to be 8 bit
            const BitmapColor aCol =
                m_bPalette ?
                pBmpAcc->GetPaletteColor(*pIn) :
                pBmpAcc->GetPixelFromData(pIn,0);

            // TODO(F3): Convert result to sRGB color space
            const double nAlpha( 1.0 - toDoubleColor(pIn[nNonAlphaBytes]) );
            *pOut++ = rendering::ARGBColor(nAlpha,
                                           nAlpha*toDoubleColor(aCol.GetRed()),
                                           nAlpha*toDoubleColor(aCol.GetGreen()),
                                           nAlpha*toDoubleColor(aCol.GetBlue()));
            pIn += nBytesPerPixel;
        }
    }
    else
    {
        for( sal_Int32 i=0; i<nNumColors; ++i )
        {
            const BitmapColor aCol =
                m_bPalette ?
                pBmpAcc->GetPaletteColor( pBmpAcc->GetPixelFromData( pIn, i ).GetIndex() ) :
                pBmpAcc->GetPixelFromData(pIn, i);

            // TODO(F3): Convert result to sRGB color space
            *pOut++ = rendering::ARGBColor(1.0,
                                           toDoubleColor(aCol.GetRed()),
                                           toDoubleColor(aCol.GetGreen()),
                                           toDoubleColor(aCol.GetBlue()));
        }
    }

    return aRes;
}

uno::Sequence< ::sal_Int8 > SAL_CALL VclCanvasBitmap::convertIntegerFromRGB( const uno::Sequence<rendering::RGBColor>& rgbColor )
{
    SolarMutexGuard aGuard;

    const std::size_t nLen( rgbColor.getLength() );
    const sal_Int32 nNumBytes((nLen*m_nBitsPerOutputPixel+7)/8);

    uno::Sequence< sal_Int8 > aRes(nNumBytes);
    sal_uInt8* pColors=reinterpret_cast<sal_uInt8*>(aRes.getArray());
    BitmapScopedReadAccess& pBmpAcc = getBitmapReadAccess();

    if( m_aBmpEx.IsAlpha() )
    {
        const tools::Long nNonAlphaBytes( (m_nBitsPerInputPixel+7)/8 );
        for( std::size_t i=0; i<nLen; ++i )
        {
            const BitmapColor aCol(toByteColor(rgbColor[i].Red),
                                   toByteColor(rgbColor[i].Green),
                                   toByteColor(rgbColor[i].Blue));
            const BitmapColor aCol2 =
                m_bPalette ?
                BitmapColor(
                    sal::static_int_cast<sal_uInt8>(pBmpAcc->GetBestPaletteIndex( aCol ))) :
                aCol;

            pBmpAcc->SetPixelOnData(pColors,i,aCol2);
            pColors   += nNonAlphaBytes;
            *pColors++ = sal_uInt8(255);
        }
    }
    else
    {
        for( std::size_t i=0; i<nLen; ++i )
        {
            const BitmapColor aCol(toByteColor(rgbColor[i].Red),
                                   toByteColor(rgbColor[i].Green),
                                   toByteColor(rgbColor[i].Blue));
            const BitmapColor aCol2 =
                m_bPalette ?
                BitmapColor(
                    sal::static_int_cast<sal_uInt8>(pBmpAcc->GetBestPaletteIndex( aCol ))) :
                aCol;

            pBmpAcc->SetPixelOnData(pColors,i,aCol2);
        }
    }

    return aRes;
}

uno::Sequence< ::sal_Int8 > SAL_CALL VclCanvasBitmap::convertIntegerFromARGB( const uno::Sequence<rendering::ARGBColor>& rgbColor )
{
    SolarMutexGuard aGuard;

    const std::size_t nLen( rgbColor.getLength() );
    const sal_Int32 nNumBytes((nLen*m_nBitsPerOutputPixel+7)/8);

    uno::Sequence< sal_Int8 > aRes(nNumBytes);
    sal_uInt8* pColors=reinterpret_cast<sal_uInt8*>(aRes.getArray());
    BitmapScopedReadAccess& pBmpAcc = getBitmapReadAccess();

    if( m_aBmpEx.IsAlpha() )
    {
        const tools::Long nNonAlphaBytes( (m_nBitsPerInputPixel+7)/8 );
        for( std::size_t i=0; i<nLen; ++i )
        {
            const BitmapColor aCol(toByteColor(rgbColor[i].Red),
                                   toByteColor(rgbColor[i].Green),
                                   toByteColor(rgbColor[i].Blue));
            const BitmapColor aCol2 =
                m_bPalette ?
                BitmapColor(
                    sal::static_int_cast<sal_uInt8>(pBmpAcc->GetBestPaletteIndex( aCol ))) :
                aCol;

            pBmpAcc->SetPixelOnData(pColors,i,aCol2);
            pColors   += nNonAlphaBytes;
            *pColors++ = 255 - toByteColor(rgbColor[i].Alpha);
        }
    }
    else
    {
        for( std::size_t i=0; i<nLen; ++i )
        {
            const BitmapColor aCol(toByteColor(rgbColor[i].Red),
                                   toByteColor(rgbColor[i].Green),
                                   toByteColor(rgbColor[i].Blue));
            const BitmapColor aCol2 =
                m_bPalette ?
                BitmapColor(
                    sal::static_int_cast<sal_uInt8>(pBmpAcc->GetBestPaletteIndex( aCol ))) :
                aCol;

            pBmpAcc->SetPixelOnData(pColors,i,aCol2);
        }
    }

    return aRes;
}

uno::Sequence< ::sal_Int8 > SAL_CALL VclCanvasBitmap::convertIntegerFromPARGB( const uno::Sequence<rendering::ARGBColor>& rgbColor )
{
    SolarMutexGuard aGuard;

    const std::size_t nLen( rgbColor.getLength() );
    const sal_Int32 nNumBytes((nLen*m_nBitsPerOutputPixel+7)/8);

    uno::Sequence< sal_Int8 > aRes(nNumBytes);
    sal_uInt8* pColors=reinterpret_cast<sal_uInt8*>(aRes.getArray());
    BitmapScopedReadAccess& pBmpAcc = getBitmapReadAccess();

    if( m_aBmpEx.IsAlpha() )
    {
        const tools::Long nNonAlphaBytes( (m_nBitsPerInputPixel+7)/8 );
        for( std::size_t i=0; i<nLen; ++i )
        {
            const double nAlpha( rgbColor[i].Alpha );
            const BitmapColor aCol(toByteColor(rgbColor[i].Red / nAlpha),
                                   toByteColor(rgbColor[i].Green / nAlpha),
                                   toByteColor(rgbColor[i].Blue / nAlpha));
            const BitmapColor aCol2 =
                m_bPalette ?
                BitmapColor(
                    sal::static_int_cast<sal_uInt8>(pBmpAcc->GetBestPaletteIndex( aCol ))) :
                aCol;

            pBmpAcc->SetPixelOnData(pColors,i,aCol2);
            pColors   += nNonAlphaBytes;
            *pColors++ = 255 - toByteColor(nAlpha);
        }
    }
    else
    {
        for( std::size_t i=0; i<nLen; ++i )
        {
            const BitmapColor aCol(toByteColor(rgbColor[i].Red),
                                   toByteColor(rgbColor[i].Green),
                                   toByteColor(rgbColor[i].Blue));
            const BitmapColor aCol2 =
                m_bPalette ?
                BitmapColor(
                    sal::static_int_cast<sal_uInt8>(pBmpAcc->GetBestPaletteIndex( aCol ))) :
                aCol;

            pBmpAcc->SetPixelOnData(pColors,i,aCol2);
        }
    }

    return aRes;
}


/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
