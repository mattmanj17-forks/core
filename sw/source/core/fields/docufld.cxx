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

#include <config_features.h>
#include <config_fuzzers.h>

#include <textapi.hxx>

#include <hintids.hxx>
#include <com/sun/star/frame/XModel.hpp>
#include <com/sun/star/text/XText.hpp>
#include <com/sun/star/script/Converter.hpp>
#include <com/sun/star/text/PlaceholderType.hpp>
#include <com/sun/star/text/TemplateDisplayFormat.hpp>
#include <com/sun/star/text/PageNumberType.hpp>
#include <com/sun/star/text/FilenameDisplayFormat.hpp>
#include <com/sun/star/document/XDocumentPropertiesSupplier.hpp>
#include <com/sun/star/document/XDocumentProperties.hpp>
#include <com/sun/star/util/Date.hpp>
#include <com/sun/star/util/Duration.hpp>
#include <o3tl/any.hxx>
#include <o3tl/string_view.hxx>
#include <unotools/localedatawrapper.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/string.hxx>
#include <tools/urlobj.hxx>
#include <svl/numformat.hxx>
#include <svl/urihelper.hxx>
#include <unotools/useroptions.hxx>
#include <unotools/syslocale.hxx>
#include <libxml/xmlstring.h>
#include <libxml/xmlwriter.h>

#include <tools/time.hxx>
#include <tools/datetime.hxx>

#include <com/sun/star/util/DateTime.hpp>

#include <swmodule.hxx>
#include <sfx2/docfile.hxx>
#include <sfx2/doctempl.hxx>
#include <fmtfld.hxx>
#include <txtfld.hxx>
#include <charfmt.hxx>
#include <docstat.hxx>
#include <pagedesc.hxx>
#include <fmtpdsc.hxx>
#include <doc.hxx>
#include <IDocumentFieldsAccess.hxx>
#include <IDocumentStatistics.hxx>
#include <IDocumentStylePoolAccess.hxx>
#include <IDocumentLayoutAccess.hxx>
#include <rootfrm.hxx>
#include <pagefrm.hxx>
#include <cntfrm.hxx>
#include <pam.hxx>
#include <utility>
#include <viewsh.hxx>
#include <dbmgr.hxx>
#include <shellres.hxx>
#include <docufld.hxx>
#include <flddat.hxx>
#include <docfld.hxx>
#include <ndtxt.hxx>
#include <expfld.hxx>
#include <poolfmt.hxx>
#include <docsh.hxx>
#include <unofldmid.h>
#include <swunohelper.hxx>
#include <strings.hrc>

#include <editeng/outlobj.hxx>
#include <calbck.hxx>
#include <hints.hxx>

#define URL_DECODE  INetURLObject::DecodeMechanism::Unambiguous

using namespace ::com::sun::star;
using namespace ::com::sun::star::uno;

SwPageNumberFieldType::SwPageNumberFieldType()
    : SwFieldType( SwFieldIds::PageNumber ),
    m_nNumberingType( SVX_NUM_ARABIC ),
    m_bVirtual( false )
{
}

OUString SwPageNumberFieldType::Expand( SvxNumType nFormat, short nOff,
         sal_uInt16 const nPageNumber, sal_uInt16 const nMaxPage,
         const OUString& rUserStr, LanguageType nLang ) const
{
    SvxNumType nTmpFormat = (SVX_NUM_PAGEDESC == nFormat) ? m_nNumberingType : nFormat;
    int const nTmp = nPageNumber + nOff;

    if (0 > nTmp || SVX_NUM_NUMBER_NONE == nTmpFormat || (!m_bVirtual && nTmp > nMaxPage))
        return OUString();

    if( SVX_NUM_CHAR_SPECIAL == nTmpFormat )
        return rUserStr;

    return FormatNumber( nTmp, nTmpFormat, nLang );
}

std::unique_ptr<SwFieldType> SwPageNumberFieldType::Copy() const
{
    std::unique_ptr<SwPageNumberFieldType> pTmp(new SwPageNumberFieldType());

    pTmp->m_nNumberingType = m_nNumberingType;
    pTmp->m_bVirtual  = m_bVirtual;

    return pTmp;
}

void SwPageNumberFieldType::ChangeExpansion( SwDoc* pDoc,
                                            bool bVirt,
                                            const SvxNumType* pNumFormat )
{
    if( pNumFormat )
        m_nNumberingType = *pNumFormat;

    m_bVirtual = false;
    if (!(bVirt && pDoc))
        return;

    // check the flag since the layout NEVER sets it back
    for (SwRootFrame* pRootFrame : pDoc->GetAllLayouts())
    {
        const SwPageFrame* pPageFrameIter = pRootFrame->GetLastPage();
        while (pPageFrameIter)
        {
            const SwContentFrame* pContentFrame = pPageFrameIter->FindFirstBodyContent();
            if (pContentFrame)
            {
                const SwFormatPageDesc& rFormatPageDesc = pContentFrame->GetPageDescItem();
                if ( rFormatPageDesc.GetNumOffset() && rFormatPageDesc.GetDefinedIn() )
                {
                    const SwContentNode* pNd = dynamic_cast<const SwContentNode*>( rFormatPageDesc.GetDefinedIn()  );
                    if( pNd )
                    {
                        if (SwIterator<SwFrame, SwContentNode, sw::IteratorMode::UnwrapMulti>(*pNd).First())
                        // sw_redlinehide: not sure if this should happen only if
                        // it's the first node, because that's where RES_PAGEDESC
                        // is effective?
                            m_bVirtual = true;
                    }
                    else if( dynamic_cast< const SwFormat* >(rFormatPageDesc.GetDefinedIn()) !=  nullptr)
                    {
                        m_bVirtual = false;
                        sw::AutoFormatUsedHint aHint(m_bVirtual, pDoc->GetNodes());
                        rFormatPageDesc.GetDefinedIn()->CallSwClientNotify(aHint);
                        break;
                    }
                }
            }
            pPageFrameIter = static_cast<const SwPageFrame*>(pPageFrameIter->GetPrev());
        }
    }
}

SwPageNumberField::SwPageNumberField(SwPageNumberFieldType* pTyp,
          SwPageNumSubType nSub, SvxNumType nFormat, short nOff,
          sal_uInt16 const nPageNumber, sal_uInt16 const nMaxPage)
    : SwField(pTyp), m_nSubType(nSub), m_nOffset(nOff)
    , m_nPageNumber(nPageNumber)
    , m_nMaxPage(nMaxPage)
    , m_nFormat(nFormat)
{
}

void SwPageNumberField::ChangeExpansion(sal_uInt16 const nPageNumber,
        sal_uInt16 const nMaxPage)
{
    m_nPageNumber = nPageNumber;
    m_nMaxPage = nMaxPage;
}

OUString SwPageNumberField::ExpandImpl(SwRootFrame const*const) const
{
    OUString sRet;
    SwPageNumberFieldType* pFieldType = static_cast<SwPageNumberFieldType*>(GetTyp());

    if( SwPageNumSubType::Next == m_nSubType && 1 != m_nOffset )
    {
        sRet = pFieldType->Expand(GetFormat(), 1, m_nPageNumber, m_nMaxPage, m_sUserStr, GetLanguage());
        if (!sRet.isEmpty())
        {
            sRet = pFieldType->Expand(GetFormat(), m_nOffset, m_nPageNumber, m_nMaxPage, m_sUserStr, GetLanguage());
        }
    }
    else if( SwPageNumSubType::Previous == m_nSubType && -1 != m_nOffset )
    {
        sRet = pFieldType->Expand(GetFormat(), -1, m_nPageNumber, m_nMaxPage, m_sUserStr, GetLanguage());
        if (!sRet.isEmpty())
        {
            sRet = pFieldType->Expand(GetFormat(), m_nOffset, m_nPageNumber, m_nMaxPage, m_sUserStr, GetLanguage());
        }
    }
    else
        sRet = pFieldType->Expand(GetFormat(), m_nOffset, m_nPageNumber, m_nMaxPage, m_sUserStr, GetLanguage());
    return sRet;
}

std::unique_ptr<SwField> SwPageNumberField::Copy() const
{
    std::unique_ptr<SwPageNumberField> pTmp(new SwPageNumberField(
                static_cast<SwPageNumberFieldType*>(GetTyp()), m_nSubType,
                GetFormat(), m_nOffset, m_nPageNumber, m_nMaxPage));
    pTmp->SetLanguage( GetLanguage() );
    pTmp->SetUserString( m_sUserStr );
    return std::unique_ptr<SwField>(pTmp.release());
}

OUString SwPageNumberField::GetPar2() const
{
    return OUString::number(m_nOffset);
}

void SwPageNumberField::SetPar2(const OUString& rStr)
{
    m_nOffset = static_cast<short>(rStr.toInt32());
}

SwPageNumSubType SwPageNumberField::GetSubType() const
{
    return m_nSubType;
}

bool SwPageNumberField::QueryValue( uno::Any& rAny, sal_uInt16 nWhichId ) const
{
    switch( nWhichId )
    {
    case FIELD_PROP_FORMAT:
        rAny <<= static_cast<sal_Int16>(GetFormat());
        break;
    case FIELD_PROP_USHORT1:
        rAny <<= m_nOffset;
        break;
    case FIELD_PROP_SUBTYPE:
        {
            text::PageNumberType eType;
            eType = text::PageNumberType_CURRENT;
            if(m_nSubType == SwPageNumSubType::Previous)
                eType = text::PageNumberType_PREV;
            else if(m_nSubType == SwPageNumSubType::Next)
                eType = text::PageNumberType_NEXT;
            rAny <<= eType;
        }
        break;
    case FIELD_PROP_PAR1:
        rAny <<= m_sUserStr;
        break;
    case FIELD_PROP_TITLE:
        break;

    default:
        assert(false);
    }
    return true;
}

bool SwPageNumberField::PutValue( const uno::Any& rAny, sal_uInt16 nWhichId )
{
    bool bRet = true;
    sal_Int16 nSet = 0;
    switch( nWhichId )
    {
    case FIELD_PROP_FORMAT:
        rAny >>= nSet;

        // TODO: where do the defines come from?
        if(nSet <= SVX_NUM_PAGEDESC )
            m_nFormat = static_cast<SvxNumType>(nSet);
        break;
    case FIELD_PROP_USHORT1:
        rAny >>= nSet;
        m_nOffset = nSet;
        break;
    case FIELD_PROP_SUBTYPE:
        switch( static_cast<text::PageNumberType>(SWUnoHelper::GetEnumAsInt32( rAny )) )
        {
            case text::PageNumberType_CURRENT:
                m_nSubType = SwPageNumSubType::Random;
            break;
            case text::PageNumberType_PREV:
                m_nSubType = SwPageNumSubType::Previous;
            break;
            case text::PageNumberType_NEXT:
                m_nSubType = SwPageNumSubType::Next;
            break;
            default:
                bRet = false;
        }
        break;
    case FIELD_PROP_PAR1:
        rAny >>= m_sUserStr;
        break;

    default:
        assert(false);
    }
    return bRet;
}

SwAuthorFieldType::SwAuthorFieldType()
    : SwFieldType( SwFieldIds::Author )
{
}

OUString SwAuthorFieldType::Expand(SwAuthorFormat nFormat)
{
    SwModule* mod = SwModule::get();
    SvtUserOptions&  rOpt = mod->GetUserOptions();
    if((nFormat & SwAuthorFormat::Mask) == SwAuthorFormat::Name)
    {
        // Prefer the view's redline author name.
        // (set in SwXTextDocument::initializeForTiledRendering)
        std::size_t nAuthor = mod->GetRedlineAuthor();
        OUString sAuthor = mod->GetRedlineAuthor(nAuthor);
        if (sAuthor.isEmpty())
            return rOpt.GetFullName();

        return sAuthor;
    }

    return rOpt.GetID();
}

std::unique_ptr<SwFieldType> SwAuthorFieldType::Copy() const
{
    return std::make_unique<SwAuthorFieldType>();
}

SwAuthorField::SwAuthorField(SwAuthorFieldType* pTyp, SwAuthorFormat nFormat)
    : SwField(pTyp),
      m_nFormat(nFormat)
{
    m_aContent = SwAuthorFieldType::Expand(GetFormat());
}

OUString SwAuthorField::ExpandImpl(SwRootFrame const*const) const
{
    if (!IsFixed())
        const_cast<SwAuthorField*>(this)->m_aContent =
                    SwAuthorFieldType::Expand(GetFormat());

    return m_aContent;
}

std::unique_ptr<SwField> SwAuthorField::Copy() const
{
    std::unique_ptr<SwAuthorField> pTmp(new SwAuthorField( static_cast<SwAuthorFieldType*>(GetTyp()),
                                                GetFormat()));
    pTmp->SetExpansion(m_aContent);
    return std::unique_ptr<SwField>(pTmp.release());
}

bool SwAuthorField::QueryValue( uno::Any& rAny, sal_uInt16 nWhichId ) const
{
    switch( nWhichId )
    {
    case FIELD_PROP_BOOL1:
        rAny <<= (GetFormat() & SwAuthorFormat::Mask) == SwAuthorFormat::Name;
        break;

    case FIELD_PROP_BOOL2:
        rAny <<= IsFixed();
        break;

    case FIELD_PROP_PAR1:
        rAny <<= m_aContent;
        break;

    case FIELD_PROP_TITLE:
        break;

    default:
        assert(false);
    }
    return true;
}

bool SwAuthorField::PutValue( const uno::Any& rAny, sal_uInt16 nWhichId )
{
    switch( nWhichId )
    {
    case FIELD_PROP_BOOL1:
        m_nFormat = *o3tl::doAccess<bool>(rAny) ? SwAuthorFormat::Name : SwAuthorFormat::Shortcut;
        break;

    case FIELD_PROP_BOOL2:
        if( *o3tl::doAccess<bool>(rAny) )
            m_nFormat |= SwAuthorFormat::Fixed;
        else
            m_nFormat &= ~SwAuthorFormat::Fixed;
        break;

    case FIELD_PROP_PAR1:
        rAny >>= m_aContent;
        break;

    case FIELD_PROP_TITLE:
        break;

    default:
        assert(false);
    }
    return true;
}

SwFileNameFieldType::SwFileNameFieldType(SwDoc& rDocument)
    : SwFieldType( SwFieldIds::Filename )
    , m_rDoc(rDocument)
{
}

OUString SwFileNameFieldType::Expand(SwFileNameFormat nFormat) const
{
    OUString aRet;
    const SwDocShell* pDShell = m_rDoc.GetDocShell();
    if( pDShell && pDShell->HasName() )
    {
        const INetURLObject& rURLObj = pDShell->GetMedium()->GetURLObject();
        switch( nFormat & ~SwFileNameFormat::Fixed )
        {
            case SwFileNameFormat::Path:
                {
                    if( INetProtocol::File == rURLObj.GetProtocol() )
                    {
                        INetURLObject aTemp(rURLObj);
                        aTemp.removeSegment();
                        // last slash should belong to the pathname
                        aRet = aTemp.PathToFileName();
                    }
                    else
                    {
                        aRet = URIHelper::removePassword(
                                    rURLObj.GetMainURL( INetURLObject::DecodeMechanism::NONE ),
                                    INetURLObject::EncodeMechanism::WasEncoded, URL_DECODE );
                        const sal_Int32 nPos = aRet.indexOf(rURLObj.GetLastName( URL_DECODE ));
                        if (nPos>=0)
                        {
                            aRet = aRet.copy(0, nPos);
                        }
                    }
                }
                break;

            case SwFileNameFormat::Name:
                aRet = rURLObj.GetLastName( INetURLObject::DecodeMechanism::WithCharset );
                break;

            case SwFileNameFormat::NameNoExt:
                aRet = rURLObj.GetBase();
                break;

            default:
                if( INetProtocol::File == rURLObj.GetProtocol() )
                    aRet = rURLObj.GetFull();
                else
                    aRet = URIHelper::removePassword(
                                    rURLObj.GetMainURL( INetURLObject::DecodeMechanism::NONE ),
                                    INetURLObject::EncodeMechanism::WasEncoded, URL_DECODE );
        }
    }
    return aRet;
}

std::unique_ptr<SwFieldType> SwFileNameFieldType::Copy() const
{
    return std::make_unique<SwFileNameFieldType>(m_rDoc);
}

SwFileNameField::SwFileNameField(SwFileNameFieldType* pTyp, SwFileNameFormat nFormat)
    : SwField(pTyp),
      m_nFormat(nFormat)
{
    m_aContent = static_cast<SwFileNameFieldType*>(GetTyp())->Expand(GetFormat());
}

OUString SwFileNameField::ExpandImpl(SwRootFrame const*const) const
{
    if (!IsFixed())
        const_cast<SwFileNameField*>(this)->m_aContent = static_cast<SwFileNameFieldType*>(GetTyp())->Expand(GetFormat());

    return m_aContent;
}

std::unique_ptr<SwField> SwFileNameField::Copy() const
{
    std::unique_ptr<SwFileNameField> pTmp(
        new SwFileNameField(static_cast<SwFileNameFieldType*>(GetTyp()), GetFormat()));
    pTmp->SetExpansion(m_aContent);

    return std::unique_ptr<SwField>(pTmp.release());
}

bool SwFileNameField::QueryValue( uno::Any& rAny, sal_uInt16 nWhichId ) const
{
    switch( nWhichId )
    {
    case FIELD_PROP_FORMAT:
        {
            sal_Int16 nRet;
            switch( GetFormat() & (~SwFileNameFormat::Fixed) )
            {
                case SwFileNameFormat::Path:
                    nRet = text::FilenameDisplayFormat::PATH;
                break;
                case SwFileNameFormat::NameNoExt:
                    nRet = text::FilenameDisplayFormat::NAME;
                break;
                case SwFileNameFormat::Name:
                    nRet = text::FilenameDisplayFormat::NAME_AND_EXT;
                break;
                default:    nRet = text::FilenameDisplayFormat::FULL;
            }
            rAny <<= nRet;
        }
        break;

    case FIELD_PROP_BOOL2:
        rAny <<= IsFixed();
        break;

    case FIELD_PROP_PAR3:
        rAny <<= m_aContent;
        break;

    default:
        assert(false);
    }
    return true;
}

bool SwFileNameField::PutValue( const uno::Any& rAny, sal_uInt16 nWhichId )
{
    switch( nWhichId )
    {
    case FIELD_PROP_FORMAT:
        {
            //JP 24.10.2001: int32 because in UnoField.cxx a putvalue is
            //              called with a int32 value! But normally we need
            //              here only a int16
            sal_Int32 nType = 0;
            rAny >>= nType;
            bool bFixed = IsFixed();
            switch( nType )
            {
                case text::FilenameDisplayFormat::PATH:
                    m_nFormat = SwFileNameFormat::Path;
                break;
                case text::FilenameDisplayFormat::NAME:
                    m_nFormat = SwFileNameFormat::NameNoExt;
                break;
                case text::FilenameDisplayFormat::NAME_AND_EXT:
                    m_nFormat = SwFileNameFormat::Name;
                break;
                default:
                    m_nFormat = SwFileNameFormat::PathName;
            }
            if(bFixed)
                m_nFormat |= SwFileNameFormat::Fixed;
        }
        break;

    case FIELD_PROP_BOOL2:
        if( *o3tl::doAccess<bool>(rAny) )
            m_nFormat |= SwFileNameFormat::Fixed;
        else
            m_nFormat &= ~SwFileNameFormat::Fixed;
        break;

    case FIELD_PROP_PAR3:
        rAny >>= m_aContent;
        break;

    default:
        assert(false);
    }
    return true;
}

SwTemplNameFieldType::SwTemplNameFieldType(SwDoc& rDocument)
    : SwFieldType( SwFieldIds::TemplateName )
    , m_rDoc(rDocument)
{
}

OUString SwTemplNameFieldType::Expand(SwFileNameFormat nFormat) const
{
    OSL_ENSURE( nFormat < SwFileNameFormat::End, "Expand: no valid Format!" );

    OUString aRet;
    SwDocShell *pDocShell(m_rDoc.GetDocShell());
    OSL_ENSURE(pDocShell, "no SwDocShell");
    if (pDocShell) {
        uno::Reference<document::XDocumentPropertiesSupplier> xDPS(
            pDocShell->GetModel(), uno::UNO_QUERY_THROW);
        uno::Reference<document::XDocumentProperties> xDocProps(
            xDPS->getDocumentProperties());
        OSL_ENSURE(xDocProps.is(), "Doc has no DocumentProperties");

        if( SwFileNameFormat::UIName == nFormat )
            aRet = xDocProps->getTemplateName();
        else if( !xDocProps->getTemplateURL().isEmpty() )
        {
            if( SwFileNameFormat::UIRange == nFormat )
            {
                // for getting region names!
                SfxDocumentTemplates aFac;
                OUString sTmp;
                OUString sRegion;
                aFac.GetLogicNames( xDocProps->getTemplateURL(), sRegion, sTmp );
                aRet = sRegion;
            }
            else
            {
                INetURLObject aPathName( xDocProps->getTemplateURL() );
                if( SwFileNameFormat::Name == nFormat )
                    aRet = aPathName.GetLastName(URL_DECODE);
                else if( SwFileNameFormat::NameNoExt == nFormat )
                    aRet = aPathName.GetBase();
                else
                {
                    if( SwFileNameFormat::Path == nFormat )
                    {
                        aPathName.removeSegment();
                        aRet = aPathName.GetFull();
                    }
                    else
                        aRet = aPathName.GetFull();
                }
            }
        }
    }
    return aRet;
}

std::unique_ptr<SwFieldType> SwTemplNameFieldType::Copy() const
{
    return std::make_unique<SwTemplNameFieldType>(m_rDoc);
}

SwTemplNameField::SwTemplNameField(SwTemplNameFieldType* pTyp, SwFileNameFormat nFormat)
    : SwField(pTyp), m_nFormat(nFormat)
{}

OUString SwTemplNameField::ExpandImpl(SwRootFrame const*const) const
{
    return static_cast<SwTemplNameFieldType*>(GetTyp())->Expand(GetFormat());
}

std::unique_ptr<SwField> SwTemplNameField::Copy() const
{
    return std::make_unique<SwTemplNameField>(static_cast<SwTemplNameFieldType*>(GetTyp()), GetFormat());
}

bool SwTemplNameField::QueryValue( uno::Any& rAny, sal_uInt16 nWhichId ) const
{
    switch ( nWhichId )
    {
    case FIELD_PROP_FORMAT:
        {
            sal_Int16 nRet;
            switch( GetFormat() )
            {
                case SwFileNameFormat::Path:       nRet = text::FilenameDisplayFormat::PATH; break;
                case SwFileNameFormat::NameNoExt: nRet = text::FilenameDisplayFormat::NAME; break;
                case SwFileNameFormat::Name:       nRet = text::FilenameDisplayFormat::NAME_AND_EXT; break;
                case SwFileNameFormat::UIRange:   nRet = text::TemplateDisplayFormat::AREA; break;
                case SwFileNameFormat::UIName:    nRet = text::TemplateDisplayFormat::TITLE;  break;
                default:    nRet = text::FilenameDisplayFormat::FULL;

            }
            rAny <<= nRet;
        }
        break;

    default:
        assert(false);
    }
    return true;
}

bool SwTemplNameField::PutValue( const uno::Any& rAny, sal_uInt16 nWhichId )
{
    switch ( nWhichId )
    {
    case FIELD_PROP_FORMAT:
        {
            //JP 24.10.2001: int32 because in UnoField.cxx a putvalue is
            //              called with a int32 value! But normally we need
            //              here only a int16
            sal_Int32 nType = 0;
            rAny >>= nType;
            switch( nType )
            {
            case text::FilenameDisplayFormat::PATH:
                m_nFormat = SwFileNameFormat::Path;
            break;
            case text::FilenameDisplayFormat::NAME:
                m_nFormat = SwFileNameFormat::NameNoExt;
            break;
            case text::FilenameDisplayFormat::NAME_AND_EXT:
                m_nFormat = SwFileNameFormat::Name;
            break;
            case text::TemplateDisplayFormat::AREA  :
                m_nFormat = SwFileNameFormat::UIRange;
            break;
            case text::TemplateDisplayFormat::TITLE  :
                m_nFormat = SwFileNameFormat::UIName;
            break;
            default:
                m_nFormat = SwFileNameFormat::PathName;
            }
        }
        break;

    default:
        assert(false);
    }
    return true;
}

SwDocStatFieldType::SwDocStatFieldType(SwDoc& rDocument)
    : SwFieldType(SwFieldIds::DocStat)
    , m_rDoc(rDocument)
    , m_nNumberingType(SVX_NUM_ARABIC)
{
}

OUString SwDocStatFieldType::Expand(SwDocStatSubType nSubType,
    SvxNumType nFormat, sal_uInt16 nVirtPageCount) const
{
    sal_uInt32 nVal = 0;
    const SwDocStat& rDStat = m_rDoc.getIDocumentStatistics().GetDocStat();
    switch( nSubType )
    {
        case SwDocStatSubType::Table:  nVal = rDStat.nTable;   break;
        case SwDocStatSubType::Graphic:  nVal = rDStat.nGrf;   break;
        case SwDocStatSubType::OLE:  nVal = rDStat.nOLE;   break;
        case SwDocStatSubType::Paragraph: nVal = rDStat.nPara;  break;
        case SwDocStatSubType::Word: nVal = rDStat.nWord;  break;
        case SwDocStatSubType::Character: nVal = rDStat.nChar;  break;
        case SwDocStatSubType::Page:
            if( m_rDoc.getIDocumentLayoutAccess().GetCurrentLayout() )
                const_cast<SwDocStat &>(rDStat).nPage = m_rDoc.getIDocumentLayoutAccess().GetCurrentLayout()->GetPageNum();
            nVal = rDStat.nPage;
            if( SVX_NUM_PAGEDESC == nFormat )
                nFormat = m_nNumberingType;
            break;
        case SwDocStatSubType::PageRange:
            nVal = nVirtPageCount;
            if( SVX_NUM_PAGEDESC == nFormat )
                nFormat = m_nNumberingType;
            break;
        default:
            OSL_FAIL( "SwDocStatFieldType::Expand: unknown SubType" );
    }

    if( nVal <= SHRT_MAX )
        return FormatNumber( nVal, nFormat );

    return OUString::number( nVal );
}

std::unique_ptr<SwFieldType> SwDocStatFieldType::Copy() const
{
    return std::make_unique<SwDocStatFieldType>(m_rDoc);
}
void SwDocStatFieldType::UpdateRangeFields(SwRootFrame const*const pLayout)
{
    std::vector<SwFormatField*> vFields;
    GatherFields(vFields);
    for(auto pFormatField: vFields)
    {
        SwDocStatField* pDocStatField = static_cast<SwDocStatField*>(pFormatField->GetField());
        if (pDocStatField->GetSubType() == SwDocStatSubType::PageRange)
        {
            SwTextField* pTField = pFormatField->GetTextField();
            const SwTextNode& rTextNd = pTField->GetTextNode();

            // Always the first! (in Tab-Headline, header/footer )
            Point aPt;
            std::pair<Point, bool> const tmp(aPt, false);
            const SwContentFrame *const pFrame = rTextNd.getLayoutFrame(
                pLayout, nullptr, &tmp);

            if (pFrame &&
                pFrame->IsInDocBody() &&
                pFrame->FindPageFrame())
            {
                pDocStatField->ChangeExpansion(pFrame, pFrame->GetVirtPageCount());
            }
        }
    }
}
/**
 * @param pTyp
 * @param nSub SubType
 * @param nFormat
 */
SwDocStatField::SwDocStatField(SwDocStatFieldType* pTyp, SwDocStatSubType nSub,
    SvxNumType nFormat, sal_uInt16 nVirtPageCount)
    : SwField(pTyp),
    m_nSubType(nSub),
    m_nVirtPageCount(nVirtPageCount),
    m_nFormat(nFormat)
{
}

OUString SwDocStatField::ExpandImpl(SwRootFrame const*const) const
{
    return static_cast<SwDocStatFieldType*>(GetTyp())
        ->Expand(m_nSubType, m_nFormat, m_nVirtPageCount);
}

std::unique_ptr<SwField> SwDocStatField::Copy() const
{
    return std::make_unique<SwDocStatField>(
        static_cast<SwDocStatFieldType*>(GetTyp()), m_nSubType, m_nFormat, m_nVirtPageCount );
}

SwDocStatSubType SwDocStatField::GetSubType() const
{
    return m_nSubType;
}

void SwDocStatField::SetSubType(SwDocStatSubType nSub)
{
    m_nSubType = nSub;
}

void SwDocStatField::ChangeExpansion(const SwFrame* pFrame, sal_uInt16 nVirtPageCount)
{
    if( SwDocStatSubType::Page == m_nSubType && SVX_NUM_PAGEDESC == m_nFormat )
        static_cast<SwDocStatFieldType*>(GetTyp())->SetNumFormat(
                pFrame->FindPageFrame()->GetPageDesc()->GetNumType().GetNumberingType() );
    else if (nVirtPageCount && SwDocStatSubType::PageRange == m_nSubType)
        m_nVirtPageCount = nVirtPageCount;
}

bool SwDocStatField::QueryValue( uno::Any& rAny, sal_uInt16 nWhichId ) const
{
    switch ( nWhichId )
    {
    case FIELD_PROP_USHORT2:
        rAny <<= static_cast<sal_Int16>(m_nFormat);
        break;
    case FIELD_PROP_USHORT1:
        rAny <<= static_cast<sal_Int32>(m_nVirtPageCount);
        break;

    default:
        assert(false);
    }
    return true;
}

bool SwDocStatField::PutValue( const uno::Any& rAny, sal_uInt16 nWhichId )
{
    bool bRet = false;
    switch ( nWhichId )
    {
    case FIELD_PROP_USHORT2:
        {
            sal_Int16 nSet = 0;
            rAny >>= nSet;
            if(nSet <= SVX_NUM_CHARS_LOWER_LETTER_N &&
                nSet != SVX_NUM_CHAR_SPECIAL &&
                    nSet != SVX_NUM_BITMAP)
            {
                m_nFormat = static_cast<SvxNumType>(nSet);
                bRet = true;
            }
        }
        break;
    case FIELD_PROP_USHORT1:
        {
            sal_Int32 nSet = 0;
            rAny >>= nSet;
            if (nSet >= 0 && nSet < USHRT_MAX)
            {
                m_nVirtPageCount = static_cast<sal_uInt16>(nSet);
                bRet = true;
            }
        }
        break;

    default:
        assert(false);
    }
    return bRet;
}

// Document info field type

SwDocInfoFieldType::SwDocInfoFieldType(SwDoc* pDc)
    : SwValueFieldType( pDc, SwFieldIds::DocInfo )
{
}

std::unique_ptr<SwFieldType> SwDocInfoFieldType::Copy() const
{
    return std::make_unique<SwDocInfoFieldType>(GetDoc());
}

static void lcl_GetLocalDataWrapper( LanguageType nLang,
                              const LocaleDataWrapper **ppAppLocalData,
                              const LocaleDataWrapper **ppLocalData )
{
    SvtSysLocale aLocale;
    *ppAppLocalData = &aLocale.GetLocaleData();
    *ppLocalData = *ppAppLocalData;
    if( nLang != (*ppLocalData)->getLanguageTag().getLanguageType() )
        *ppLocalData = new LocaleDataWrapper(LanguageTag( nLang ));
}

OUString SwDocInfoFieldType::Expand( SwDocInfoSubType nSub, sal_uInt32 nFormat,
                                    LanguageType nLang, const OUString& rName ) const
{
    const LocaleDataWrapper *pAppLocalData = nullptr, *pLocalData = nullptr;
    SwDocShell *pDocShell(GetDoc()->GetDocShell());
    OSL_ENSURE(pDocShell, "no SwDocShell");
    if (!pDocShell) { return OUString(); }

    uno::Reference<document::XDocumentPropertiesSupplier> xDPS(
        pDocShell->GetModel(), uno::UNO_QUERY_THROW);
    uno::Reference<document::XDocumentProperties> xDocProps(
        xDPS->getDocumentProperties());
    OSL_ENSURE(xDocProps.is(), "Doc has no DocumentProperties");

    SwDocInfoSubType nExtSub = nSub & SwDocInfoSubType::UpperMask;
    nSub &= SwDocInfoSubType::LowerMask;   // do not consider extended SubTypes

    OUString aStr;
    switch(nSub)
    {
    case SwDocInfoSubType::Title:  aStr = xDocProps->getTitle();       break;
    case SwDocInfoSubType::Subject:aStr = xDocProps->getSubject();     break;
    case SwDocInfoSubType::Keys:   aStr = ::comphelper::string::convertCommaSeparated(
                                xDocProps->getKeywords());
                    break;
    case SwDocInfoSubType::Comment:aStr = xDocProps->getDescription(); break;
    case SwDocInfoSubType::DocNo:  aStr = OUString::number(
                                        xDocProps->getEditingCycles() );
                    break;
    case SwDocInfoSubType::Edit:
        if ( !nFormat )
        {
            lcl_GetLocalDataWrapper( nLang, &pAppLocalData, &pLocalData );
            sal_Int32 dur = xDocProps->getEditingDuration();
            // If Seconds > 0 then bSec should be TRUE otherwise Seconds
            // information will be lost if file has EditTime in Seconds format.
            aStr = pLocalData->getTime( tools::Time(dur/3600, (dur%3600)/60, dur%60),
                                        dur%60 > 0);
        }
        else
        {
            double fVal = xDocProps->getEditingDuration() / double(tools::Time::secondPerDay);
            aStr = ExpandValue(fVal, nFormat, nLang);
        }
        break;
    case SwDocInfoSubType::Custom:
        {
            OUString sVal;
            try
            {
                uno::Any aAny;
                uno::Reference < beans::XPropertySet > xSet(
                    xDocProps->getUserDefinedProperties(),
                    uno::UNO_QUERY_THROW);
                aAny = xSet->getPropertyValue( rName );

                uno::Reference < script::XTypeConverter > xConverter( script::Converter::create(comphelper::getProcessComponentContext()) );
                uno::Any aNew = xConverter->convertToSimpleType( aAny, uno::TypeClass_STRING );
                aNew >>= sVal;
            }
            catch (uno::Exception&) {}
            return sVal;
        }

    default:
        {
            OUString aName( xDocProps->getAuthor() );
            util::DateTime uDT( xDocProps->getCreationDate() );
            DateTime aDate(uDT);
            if( nSub == SwDocInfoSubType::Create )
                ;       // that's it !!
            else if( nSub == SwDocInfoSubType::Change )
            {
                aName = xDocProps->getModifiedBy();
                uDT = xDocProps->getModificationDate();
                aDate = DateTime(uDT);
            }
            else if( nSub == SwDocInfoSubType::Print )
            {
                aName = xDocProps->getPrintedBy();
                if ( !std::getenv("STABLE_FIELDS_HACK") )
                {
                    uDT = xDocProps->getPrintDate();
                    aDate = DateTime(uDT);
                }
            }
            else
                break;

            if (aDate.IsValidAndGregorian())
            {
                switch (nExtSub & ~SwDocInfoSubType::SubFixed)
                {
                case SwDocInfoSubType::SubAuthor:
                    aStr = aName;
                    break;

                case SwDocInfoSubType::SubTime:
                    if (!nFormat)
                    {
                        lcl_GetLocalDataWrapper( nLang, &pAppLocalData,
                                                        &pLocalData );
                        aStr = pLocalData->getTime( aDate,
                                                    false);
                    }
                    else
                    {
                        // start the number formatter
                        double fVal = SwDateTimeField::GetDateTime( *GetDoc(),
                                                    aDate);
                        aStr = ExpandValue(fVal, nFormat, nLang);
                    }
                    break;

                case SwDocInfoSubType::SubDate:
                    if (!nFormat)
                    {
                        lcl_GetLocalDataWrapper( nLang, &pAppLocalData,
                                                 &pLocalData );
                        aStr = pLocalData->getDate( aDate );
                    }
                    else
                    {
                        // start the number formatter
                        double fVal = SwDateTimeField::GetDateTime( *GetDoc(),
                                                    aDate);
                        aStr = ExpandValue(fVal, nFormat, nLang);
                    }
                    break;
                default: break;
                }
            }
        }
        break;
    }

    if( pAppLocalData != pLocalData )
        delete pLocalData;

    return aStr;
}

// document info field

SwDocInfoField::SwDocInfoField(SwDocInfoFieldType* pTyp, SwDocInfoSubType nSub, const OUString& rName, sal_uInt32 nFormat) :
    SwValueField(pTyp, nFormat), m_nSubType(nSub)
{
    m_aName = rName;
    m_aContent = static_cast<SwDocInfoFieldType*>(GetTyp())->Expand(m_nSubType, nFormat, GetLanguage(), m_aName);
}

SwDocInfoField::SwDocInfoField(SwDocInfoFieldType* pTyp, SwDocInfoSubType nSub, const OUString& rName, const OUString& rValue, sal_uInt32 nFormat) :
    SwValueField(pTyp, nFormat), m_nSubType(nSub)
{
    m_aName = rName;
    m_aContent = rValue;
}

template <class T> static double lcl_TimeToDays(const T& rTime)
{
    constexpr double fNanoSecondsPerDay = tools::Time::nanoSecPerDay;
    return (  (rTime.Hours   * tools::Time::nanoSecPerHour)
            + (rTime.Minutes * tools::Time::nanoSecPerMinute)
            + (rTime.Seconds * tools::Time::nanoSecPerSec)
            + (rTime.NanoSeconds))
        / fNanoSecondsPerDay;
}

template<class D> static double lcl_DateToDays(const D& rDate, const SwDocShell* pDocShell)
{
    const SvNumberFormatter* pFormatter = pDocShell->GetDoc()->GetNumberFormatter();
    sal_Int64 nDate = Date::DateToDays(rDate.Day, rDate.Month, rDate.Year);
    sal_Int64 nNullDate = pFormatter->GetNullDate().GetAsNormalizedDays();
    return double( nDate - nNullDate );
}

OUString SwDocInfoField::ExpandImpl(SwRootFrame const*const) const
{
    // if the field is "fixed" we don't update it from the property
    if (!IsFixed())
    {
        if ( ( m_nSubType & SwDocInfoSubType::LowerMask ) == SwDocInfoSubType::Custom )
        {
            // custom properties currently need special treatment
            // We don't have a secure way to detect "real" custom properties in Word import of text
            // fields, so we treat *every* unknown property as a custom property, even the "built-in"
            // section in Word's document summary information stream as these properties have not been
            // inserted when the document summary information was imported, we do it here.
            // This approach is still a lot better than the old one to import such fields as
            // "user fields" and simple text
            SwDocShell* pDocShell = GetDoc()->GetDocShell();
            if( !pDocShell )
                return m_aContent;
            try
            {
                uno::Reference<document::XDocumentPropertiesSupplier> xDPS( pDocShell->GetModel(), uno::UNO_QUERY_THROW);
                uno::Reference<document::XDocumentProperties> xDocProps( xDPS->getDocumentProperties());
                uno::Reference < beans::XPropertySet > xSet( xDocProps->getUserDefinedProperties(), uno::UNO_QUERY_THROW);
                uno::Reference < beans::XPropertySetInfo > xSetInfo = xSet->getPropertySetInfo();

                uno::Any aAny;
                if( xSetInfo->hasPropertyByName( m_aName ) )
                    aAny = xSet->getPropertyValue( m_aName );
                if (aAny.hasValue())
                {
                    // "void" type means that the property has not been inserted until now
                    OUString sVal;
                    if (util::Date aDate; aAny >>= aDate)
                    {
                        sVal = ExpandValue(lcl_DateToDays(aDate, pDocShell), GetFormat(), GetLanguage());
                    }
                    else if (util::DateTime aDateTime; aAny >>= aDateTime)
                    {
                        double fDateTime = lcl_TimeToDays(aDateTime) + lcl_DateToDays(aDateTime, pDocShell);
                        sVal = ExpandValue( fDateTime, GetFormat(), GetLanguage());
                    }
                    else if (util::Duration aDuration; aAny >>= aDuration)
                    {
                        sVal = OUStringChar(aDuration.Negative ? '-' : '+')
                             + SwViewShell::GetShellRes()->sDurationFormat;
                        sVal = sVal.replaceFirst("%1", OUString::number( aDuration.Years  ) );
                        sVal = sVal.replaceFirst("%2", OUString::number( aDuration.Months ) );
                        sVal = sVal.replaceFirst("%3", OUString::number( aDuration.Days   ) );
                        sVal = sVal.replaceFirst("%4", OUString::number( aDuration.Hours  ) );
                        sVal = sVal.replaceFirst("%5", OUString::number( aDuration.Minutes) );
                        sVal = sVal.replaceFirst("%6", OUString::number( aDuration.Seconds) );
                    }
                    else
                    {
                        uno::Reference < script::XTypeConverter > xConverter( script::Converter::create(comphelper::getProcessComponentContext()) );
                        uno::Any aNew = xConverter->convertToSimpleType( aAny, uno::TypeClass_STRING );
                        aNew >>= sVal;
                    }
                    const_cast<SwDocInfoField*>(this)->m_aContent = sVal;
                }
            }
            catch (uno::Exception&) {}
        }
        else
            const_cast<SwDocInfoField*>(this)->m_aContent = static_cast<SwDocInfoFieldType*>(GetTyp())->Expand(m_nSubType, GetFormat(), GetLanguage(), m_aName);
    }

    return m_aContent;
}

OUString SwDocInfoField::GetFieldName() const
{
    OUString aStr(SwFieldType::GetTypeStr(GetTypeId()) + ":");

    SwDocInfoSubType const nSub = m_nSubType & SwDocInfoSubType::LowerMask;

    switch (nSub)
    {
        case SwDocInfoSubType::Custom:
            aStr += m_aName;
            break;

        default:
            aStr += SwViewShell::GetShellRes()
                     ->aDocInfoLst[ static_cast<sal_uInt16>(nSub) - static_cast<sal_uInt16>(SwDocInfoSubType::SubtypeBegin) ];
            break;
    }
    if (IsFixed())
    {
        aStr += " " + SwViewShell::GetShellRes()->aFixedStr;
    }
    return aStr;
}

std::unique_ptr<SwField> SwDocInfoField::Copy() const
{
    std::unique_ptr<SwDocInfoField> pField(new SwDocInfoField(static_cast<SwDocInfoFieldType*>(GetTyp()), m_nSubType, m_aName, GetFormat()));
    pField->SetAutomaticLanguage(IsAutomaticLanguage());
    pField->m_aContent = m_aContent;

    return std::unique_ptr<SwField>(pField.release());
}

SwDocInfoSubType SwDocInfoField::GetSubType() const
{
    return m_nSubType;
}

void SwDocInfoField::SetSubType(SwDocInfoSubType nSub)
{
    m_nSubType = nSub;
}

void SwDocInfoField::SetLanguage(LanguageType nLng)
{
    if (!GetFormat())
        SwField::SetLanguage(nLng);
    else
        SwValueField::SetLanguage(nLng);
}

bool SwDocInfoField::QueryValue( uno::Any& rAny, sal_uInt16 nWhichId ) const
{
    switch( nWhichId )
    {
    case FIELD_PROP_PAR1:
        rAny <<= m_aContent;
        break;

    case FIELD_PROP_PAR4:
        rAny <<= m_aName;
        break;

    case FIELD_PROP_USHORT1:
        rAny  <<= static_cast<sal_Int16>(m_aContent.toInt32());
        break;

    case FIELD_PROP_BOOL1:
        rAny <<= bool(m_nSubType & SwDocInfoSubType::SubFixed);
        break;

    case FIELD_PROP_FORMAT:
        rAny  <<= static_cast<sal_Int32>(GetFormat());
        break;

    case FIELD_PROP_DOUBLE:
        {
            double fVal = GetValue();
            rAny <<= fVal;
        }
        break;
    case FIELD_PROP_PAR3:
        rAny <<= ExpandImpl(nullptr);
        break;
    case FIELD_PROP_BOOL2:
        {
            SwDocInfoSubType nExtSub = (m_nSubType & SwDocInfoSubType::UpperMask) & ~SwDocInfoSubType::SubFixed;
            rAny <<= nExtSub == SwDocInfoSubType::SubDate;
        }
        break;
    default:
        return SwField::QueryValue(rAny, nWhichId);
    }
    return true;
}

bool SwDocInfoField::PutValue( const uno::Any& rAny, sal_uInt16 nWhichId )
{
    sal_Int32 nValue = 0;
    switch( nWhichId )
    {
    case FIELD_PROP_PAR1:
        if( m_nSubType & SwDocInfoSubType::SubFixed )
            rAny >>= m_aContent;
        break;

    case FIELD_PROP_USHORT1:
        if( m_nSubType & SwDocInfoSubType::SubFixed )
        {
            rAny >>= nValue;
            m_aContent = OUString::number(nValue);
        }
        break;

    case FIELD_PROP_BOOL1:
        if(*o3tl::doAccess<bool>(rAny))
            m_nSubType |= SwDocInfoSubType::SubFixed;
        else
            m_nSubType &= ~SwDocInfoSubType::SubFixed;
        break;
    case FIELD_PROP_FORMAT:
        {
            rAny >>= nValue;
            if( nValue >= 0)
                SetFormat(nValue);
        }
        break;

    case FIELD_PROP_PAR3:
        rAny >>= m_aContent;
        break;
    case FIELD_PROP_BOOL2:
        m_nSubType &= ~SwDocInfoSubType::SubMask;
        if(*o3tl::doAccess<bool>(rAny))
            m_nSubType |= SwDocInfoSubType::SubDate;
        else
            m_nSubType |= SwDocInfoSubType::SubTime;
        break;
    default:
        return SwField::PutValue(rAny, nWhichId);
    }
    return true;
}

SwHiddenTextFieldType::SwHiddenTextFieldType( bool bSetHidden )
    : SwFieldType( SwFieldIds::HiddenText ), m_bHidden( bSetHidden )
{
}

std::unique_ptr<SwFieldType> SwHiddenTextFieldType::Copy() const
{
    return std::make_unique<SwHiddenTextFieldType>( m_bHidden );
}

void SwHiddenTextFieldType::SetHiddenFlag( bool bSetHidden )
{
    if( m_bHidden != bSetHidden )
    {
        m_bHidden = bSetHidden;
        UpdateFields();       // notify all HiddenTexts
    }
}

SwHiddenTextField::SwHiddenTextField( SwHiddenTextFieldType* pFieldType,
                                    bool    bConditional,
                                    OUString aCond,
                                    const OUString& rStr,
                                    bool    bHidden,
                                    SwFieldTypesEnum  nSub) :
    SwField( pFieldType ), m_aCond(std::move(aCond)), m_nSubType(nSub),
    m_bCanToggle(bConditional), m_bIsHidden(bHidden), m_bValid(false)
{
    if(m_nSubType == SwFieldTypesEnum::ConditionalText)
    {
        sal_Int32 nPos = 0;
        m_aTRUEText = rStr.getToken(0, '|', nPos);

        if(nPos != -1)
        {
            m_aFALSEText = rStr.getToken(0, '|', nPos);
            if(nPos != -1)
            {
                m_aContent = rStr.getToken(0, '|', nPos);
                m_bValid = true;
            }
        }
    }
    else
        m_aTRUEText = rStr;
}

SwHiddenTextField::SwHiddenTextField( SwHiddenTextFieldType* pFieldType,
                                    OUString aCond,
                                    OUString aTrue,
                                    OUString aFalse,
                                    SwFieldTypesEnum nSub)
    : SwField( pFieldType ), m_aTRUEText(std::move(aTrue)), m_aFALSEText(std::move(aFalse)), m_aCond(std::move(aCond)), m_nSubType(nSub),
      m_bIsHidden(true), m_bValid(false)
{
    m_bCanToggle = !m_aCond.isEmpty();
}

OUString SwHiddenTextField::ExpandImpl(SwRootFrame const*const) const
{
    // Type: !Hidden  -> show always
    //        Hide    -> evaluate condition

    if( SwFieldTypesEnum::ConditionalText == m_nSubType )
    {
        if( m_bValid )
            return m_aContent;

        if( m_bCanToggle && !m_bIsHidden )
            return m_aTRUEText;
    }
    else if( !static_cast<SwHiddenTextFieldType*>(GetTyp())->GetHiddenFlag() ||
        ( m_bCanToggle && m_bIsHidden ))
        return m_aTRUEText;

    return m_aFALSEText;
}

/// get current field value and cache it
void SwHiddenTextField::Evaluate(SwDoc& rDoc)
{
    if( SwFieldTypesEnum::ConditionalText != m_nSubType )
        return;

#if !HAVE_FEATURE_DBCONNECTIVITY || ENABLE_FUZZERS
    (void) rDoc;
#else
    SwDBManager* pMgr = rDoc.GetDBManager();
#endif
    m_bValid = false;
    OUString sTmpName = (m_bCanToggle && !m_bIsHidden) ? m_aTRUEText : m_aFALSEText;

    // Database expressions need to be different from normal text. Therefore, normal text is set
    // in quotes. If the latter exist they will be removed. If not, check if potential DB name.
    // Only if there are two or more dots and no quotes, we assume a database.
    if (sTmpName.getLength()>1 &&
        sTmpName.startsWith("\"") &&
        sTmpName.endsWith("\""))
    {
        m_aContent = sTmpName.copy(1, sTmpName.getLength() - 2);
        m_bValid = true;
    }
    else if(sTmpName.indexOf('\"')<0 &&
        comphelper::string::getTokenCount(sTmpName, '.') > 2)
    {
        sTmpName = ::ReplacePoint(sTmpName);
        if(sTmpName.startsWith("[") && sTmpName.endsWith("]"))
        {   // remove brackets
            sTmpName = sTmpName.copy(1, sTmpName.getLength() - 2);
        }
#if HAVE_FEATURE_DBCONNECTIVITY && !ENABLE_FUZZERS
        if( pMgr)
        {
            sal_Int32 nIdx{ 0 };
            OUString sDBName( GetDBName( sTmpName, rDoc ));
            OUString sDataSource(sDBName.getToken(0, DB_DELIM, nIdx));
            OUString sDataTableOrQuery(sDBName.getToken(0, DB_DELIM, nIdx));
            if( pMgr->IsInMerge() && !sDBName.isEmpty() &&
                pMgr->IsDataSourceOpen( sDataSource,
                                            sDataTableOrQuery, false))
            {
                double fNumber;
                pMgr->GetMergeColumnCnt(GetColumnName( sTmpName ),
                    GetLanguage(), m_aContent, &fNumber );
                m_bValid = true;
            }
        }
#endif
    }
}

OUString SwHiddenTextField::GetFieldName() const
{
    OUString aStr = SwFieldType::GetTypeStr(m_nSubType) +
        " " + m_aCond + " " + m_aTRUEText;

    if (m_nSubType == SwFieldTypesEnum::ConditionalText)
    {
        aStr += " : " + m_aFALSEText;
    }
    return aStr;
}

std::unique_ptr<SwField> SwHiddenTextField::Copy() const
{
    std::unique_ptr<SwHiddenTextField> pField(
        new SwHiddenTextField(static_cast<SwHiddenTextFieldType*>(GetTyp()), m_aCond,
                              m_aTRUEText, m_aFALSEText));
    pField->m_bIsHidden = m_bIsHidden;
    pField->m_bValid    = m_bValid;
    pField->m_aContent  = m_aContent;
    pField->m_nSubType  = m_nSubType;
    return std::unique_ptr<SwField>(pField.release());
}

/// set condition
void SwHiddenTextField::SetPar1(const OUString& rStr)
{
    m_aCond = rStr;
    m_bCanToggle = !m_aCond.isEmpty();
}

OUString SwHiddenTextField::GetPar1() const
{
    return m_aCond;
}

/// set True/False text
void SwHiddenTextField::SetPar2(const OUString& rStr)
{
    if (m_nSubType == SwFieldTypesEnum::ConditionalText)
    {
        sal_Int32 nPos = rStr.indexOf('|');
        if (nPos == -1)
            m_aTRUEText = rStr;
        else
        {
            m_aTRUEText = rStr.copy(0, nPos);
            m_aFALSEText = rStr.copy(nPos + 1);
        }
    }
    else
        m_aTRUEText = rStr;
}

/// get True/False text
OUString SwHiddenTextField::GetPar2() const
{
    if(m_nSubType != SwFieldTypesEnum::ConditionalText)
    {
        return m_aTRUEText;
    }
    return m_aTRUEText + "|" + m_aFALSEText;
}

SwFieldTypesEnum SwHiddenTextField::GetSubType() const
{
    return m_nSubType;
}

bool SwHiddenTextField::QueryValue( uno::Any& rAny, sal_uInt16 nWhichId ) const
{
    switch( nWhichId )
    {
    case FIELD_PROP_PAR1:
        rAny <<= m_aCond;
        break;
    case FIELD_PROP_PAR2:
        rAny <<= m_aTRUEText;
        break;
    case FIELD_PROP_PAR3:
        rAny <<= m_aFALSEText;
        break;
    case FIELD_PROP_PAR4 :
        rAny <<= m_aContent;
        break;
    case FIELD_PROP_BOOL1:
        rAny <<= m_bIsHidden;
        break;
    default:
        assert(false);
    }
    return true;
}

bool SwHiddenTextField::PutValue( const uno::Any& rAny, sal_uInt16 nWhichId )
{
    switch( nWhichId )
    {
    case FIELD_PROP_PAR1:
        {
            OUString sVal;
            rAny >>= sVal;
            SetPar1(sVal);
        }
        break;
    case FIELD_PROP_PAR2:
        rAny >>= m_aTRUEText;
        break;
    case FIELD_PROP_PAR3:
        rAny >>= m_aFALSEText;
        break;
    case FIELD_PROP_BOOL1:
        m_bIsHidden = *o3tl::doAccess<bool>(rAny);
        break;
    case FIELD_PROP_PAR4:
        rAny >>= m_aContent;
        m_bValid = true;
        break;
    default:
        assert(false);
    }
    return true;
}

OUString SwHiddenTextField::GetColumnName(const OUString& rName)
{
    sal_Int32 nPos = rName.indexOf(DB_DELIM);
    if( nPos>=0 )
    {
        nPos = rName.indexOf(DB_DELIM, nPos + 1);

        if( nPos>=0 )
            return rName.copy(nPos + 1);
    }
    return rName;
}

OUString SwHiddenTextField::GetDBName(std::u16string_view rName, SwDoc& rDoc)
{
    size_t nPos = rName.find(DB_DELIM);
    if( nPos != std::u16string_view::npos )
    {
        nPos = rName.find(DB_DELIM, nPos + 1);

        if( nPos != std::u16string_view::npos )
            return OUString(rName.substr(0, nPos));
    }

    SwDBData aData = rDoc.GetDBData();
    return aData.sDataSource + OUStringChar(DB_DELIM) + aData.sCommand;
}

// [aFieldDefinition] value sample : " IF A == B \"TrueText\" \"FalseText\""
void SwHiddenTextField::ParseIfFieldDefinition(std::u16string_view aFieldDefinition,
                                               OUString& rCondition,
                                               OUString& rTrue,
                                               OUString& rFalse)
{
    // get all positions inside the input string where words are started
    //
    // In: " IF A == B \"TrueText\" \"FalseText\""
    //      0         1           2          3
    //      01234567890 123456789 01 2345678901 2
    //
    // result:
    //      [1, 4, 6, 9, 11, 22]
    std::vector<sal_Int32> wordPosition;
    {
        bool quoted = false;
        bool insideWord = false;
        for (size_t i = 0; i < aFieldDefinition.size(); i++)
        {
            if (quoted)
            {
                if (aFieldDefinition[i] == '\"')
                {
                    quoted = false;
                    insideWord = false;
                }
            }
            else
            {
                if (aFieldDefinition[i] == ' ')
                {
                    // word delimiter
                    insideWord = false;
                }
                else
                {
                    if (insideWord)
                    {
                        quoted = (aFieldDefinition[i] == '\"');
                    }
                    else
                    {
                        insideWord = true;
                        wordPosition.push_back(i);
                        quoted = (aFieldDefinition[i] == '\"');
                    }
                }
            }
        }
    }

    // first word is always "IF"
    // last two words are: true-case and false-case,
    // everything before is treated as condition expression
    // => we need at least 4 words to be inside the input string
    if (wordPosition.size() < 4)
    {
        return;
    }


    const sal_Int32 conditionBegin = wordPosition[1];
    const sal_Int32 trueBegin      = wordPosition[wordPosition.size() - 2];
    const sal_Int32 falseBegin     = wordPosition[wordPosition.size() - 1];

    const sal_Int32 conditionLength = trueBegin - conditionBegin;
    const sal_Int32 trueLength      = falseBegin - trueBegin;

    // Syntax
    // OUString::copy( sal_Int32 beginIndex, sal_Int32 count )
    rCondition = o3tl::trim(aFieldDefinition.substr(conditionBegin, conditionLength));
    rTrue = o3tl::trim(aFieldDefinition.substr(trueBegin, trueLength));
    rFalse = o3tl::trim(aFieldDefinition.substr(falseBegin));

    // remove quotes
    if (rCondition.getLength() >= 2)
    {
        if (rCondition[0] == '\"' && rCondition[rCondition.getLength() - 1] == '\"')
            rCondition = rCondition.copy(1, rCondition.getLength() - 2);
    }
    if (rTrue.getLength() >= 2)
    {
        if (rTrue[0] == '\"' && rTrue[rTrue.getLength() - 1] == '\"')
            rTrue = rTrue.copy(1, rTrue.getLength() - 2);
    }
    if (rFalse.getLength() >= 2)
    {
        if (rFalse[0] == '\"' && rFalse[rFalse.getLength() - 1] == '\"')
            rFalse = rFalse.copy(1, rFalse.getLength() - 2);
    }

    // Note: do not make trim once again, while this is a user defined data
}

// field type for line height 0

SwHiddenParaFieldType::SwHiddenParaFieldType()
    : SwFieldType( SwFieldIds::HiddenPara )
{
}

std::unique_ptr<SwFieldType> SwHiddenParaFieldType::Copy() const
{
    return std::make_unique<SwHiddenParaFieldType>();
}

// field for line height 0

SwHiddenParaField::SwHiddenParaField(SwHiddenParaFieldType* pTyp, OUString aStr)
    : SwField(pTyp), m_aCond(std::move(aStr))
{
    m_bIsHidden = false;
}

OUString SwHiddenParaField::ExpandImpl(SwRootFrame const*const) const
{
    return OUString();
}

std::unique_ptr<SwField> SwHiddenParaField::Copy() const
{
    std::unique_ptr<SwHiddenParaField> pField(new SwHiddenParaField(static_cast<SwHiddenParaFieldType*>(GetTyp()), m_aCond));
    pField->m_bIsHidden = m_bIsHidden;
    return std::unique_ptr<SwField>(pField.release());
}

bool SwHiddenParaField::QueryValue( uno::Any& rAny, sal_uInt16 nWhichId ) const
{
    switch ( nWhichId )
    {
    case FIELD_PROP_PAR1:
        rAny <<= m_aCond;
        break;
    case  FIELD_PROP_BOOL1:
        rAny <<= m_bIsHidden;
        break;

    default:
        assert(false);
    }
    return true;
}

bool SwHiddenParaField::PutValue( const uno::Any& rAny, sal_uInt16 nWhichId )
{
    switch ( nWhichId )
    {
    case FIELD_PROP_PAR1:
        rAny >>= m_aCond;
        break;
    case FIELD_PROP_BOOL1:
        m_bIsHidden = *o3tl::doAccess<bool>(rAny);
        break;

    default:
        assert(false);
    }
    return true;
}

/// set condition
void SwHiddenParaField::SetPar1(const OUString& rStr)
{
    m_aCond = rStr;
}

OUString SwHiddenParaField::GetPar1() const
{
    return m_aCond;
}

// PostIt field type

SwPostItFieldType::SwPostItFieldType(SwDoc& rDoc)
    : SwFieldType( SwFieldIds::Postit )
    , mrDoc(rDoc)
{}

std::unique_ptr<SwFieldType> SwPostItFieldType::Copy() const
{
    return std::make_unique<SwPostItFieldType>(mrDoc);
}

// PostIt field

sal_uInt32 SwPostItField::s_nLastPostItId = 1;

SwPostItField::SwPostItField( SwPostItFieldType* pT,
        OUString aAuthor,
        OUString aText,
        OUString aInitials,
        SwMarkName aName,
        const DateTime& rDateTime,
        const bool bResolved,
        const sal_uInt32 nPostItId,
        const sal_uInt32 nParentId,
        const sal_uInt32 nParaId,
        const sal_uInt32 nParentPostItId,
        SwMarkName aParentName
)
    : SwField( pT )
    , m_sText( std::move(aText) )
    , m_sAuthor( std::move(aAuthor) )
    , m_sInitials( std::move(aInitials) )
    , m_sName( std::move(aName) )
    , m_aDateTime( rDateTime )
    , m_bResolved( bResolved )
    , m_nParentId( nParentId )
    , m_nParaId( nParaId )
    , m_nParentPostItId ( nParentPostItId )
    , m_sParentName( std::move(aParentName) )
{
    m_nPostItId = nPostItId == 0 ? s_nLastPostItId++ : nPostItId;
}

SwPostItField::~SwPostItField()
{
    if ( m_xTextObject.is() )
    {
        m_xTextObject->DisposeEditSource();
    }

    mpText.reset();
}

OUString SwPostItField::ExpandImpl(SwRootFrame const*const) const
{
    return OUString();
}

OUString SwPostItField::GetDescription() const
{
    return SwResId(STR_NOTE);
}

void SwPostItField::SetResolved(bool bNewState)
{
    m_bResolved = bNewState;
}

void SwPostItField::ToggleResolved()
{
    m_bResolved = !m_bResolved;
}

bool SwPostItField::GetResolved() const
{
    return m_bResolved;
}

std::unique_ptr<SwField> SwPostItField::Copy() const
{
    std::unique_ptr<SwPostItField> pRet(new SwPostItField( static_cast<SwPostItFieldType*>(GetTyp()), m_sAuthor, m_sText, m_sInitials, m_sName,
                                                           m_aDateTime, m_bResolved, m_nPostItId, m_nParentId, m_nParaId, m_nParentPostItId, m_sParentName));
    if (mpText)
        pRet->SetTextObject( *mpText );

#if ENABLE_YRS
    pRet->SetYrsCommentId(m_CommentId);
#endif

    // Note: member <m_xTextObject> not copied.

    return std::unique_ptr<SwField>(pRet.release());
}

/// set author
void SwPostItField::SetPar1(const OUString& rStr)
{
    m_sAuthor = rStr;
}

/// get author
OUString SwPostItField::GetPar1() const
{
    return m_sAuthor;
}

/// set the PostIt's text
void SwPostItField::SetPar2(const OUString& rStr)
{
    m_sText = rStr;
}

/// get the PostIt's text
OUString SwPostItField::GetPar2() const
{
    return m_sText;
}


void SwPostItField::SetName(const SwMarkName& rName)
{
    m_sName = rName;
}

void SwPostItField::SetParentName(const SwMarkName& rName)
{
    m_sParentName = rName;
}

void SwPostItField::SetTextObject( std::optional<OutlinerParaObject> pText )
{
    mpText = std::move(pText);
}

sal_Int32 SwPostItField::GetNumberOfParagraphs() const
{
    return mpText ? mpText->Count() : 1;
}

void SwPostItField::ChangeStyleSheetName(std::u16string_view rOldName, const SfxStyleSheetBase* pStyleSheet)
{
    if (mpText && pStyleSheet)
        mpText->ChangeStyleSheetName(pStyleSheet->GetFamily(), rOldName, pStyleSheet->GetName());
}

void SwPostItField::SetPostItId(const sal_uInt32 nPostItId)
{
    m_nPostItId = nPostItId == 0 ? s_nLastPostItId++ : nPostItId;
}

void SwPostItField::SetParentPostItId(const sal_uInt32 nParentPostItId)
{
    m_nParentPostItId = nParentPostItId;
}

void SwPostItField::SetParentId(const sal_uInt32 nParentId)
{
    m_nParentId = nParentId;
}

void SwPostItField::SetParaId(const sal_uInt32 nParaId)
{
    m_nParaId = nParaId;
}

bool SwPostItField::QueryValue( uno::Any& rAny, sal_uInt16 nWhichId ) const
{
    switch( nWhichId )
    {
    case FIELD_PROP_PAR1:
        rAny <<= m_sAuthor;
        break;
    case FIELD_PROP_PAR2:
        {
        rAny <<= m_sText;
        break;
        }
    case FIELD_PROP_PAR3:
        rAny <<= m_sInitials;
        break;
    case FIELD_PROP_PAR4:
        rAny <<= m_sName.toString();
        break;
    case FIELD_PROP_PAR7: // PAR5 (Parent Para Id) and PAR6 (Para Id) are skipped - they are not written into xml. Used for file conversion.
        rAny <<= m_sParentName.toString();
        break;
    case FIELD_PROP_BOOL1:
        rAny <<= m_bResolved;
        break;
    case FIELD_PROP_TEXT:
        {
            if ( !m_xTextObject.is() )
            {
                SwPostItFieldType* pGetType = static_cast<SwPostItFieldType*>(GetTyp());
                SwDoc& rDoc = pGetType->GetDoc();
                auto pObj = std::make_unique<SwTextAPIEditSource>( &rDoc );
                const_cast <SwPostItField*> (this)->m_xTextObject = new SwTextAPIObject( std::move(pObj) );
            }

            if ( mpText )
                m_xTextObject->SetText( *mpText );
            else
                m_xTextObject->SetString( m_sText );

            rAny <<= uno::Reference < text::XText >( m_xTextObject );
            break;
        }
    case FIELD_PROP_DATE:
        {
            rAny <<= m_aDateTime.GetUNODate();
        }
        break;
    case FIELD_PROP_DATE_TIME:
        {
            rAny <<= m_aDateTime.GetUNODateTime();
        }
        break;
    case FIELD_PROP_PAR5:
        {
            rAny <<= OUString(OUString::number(m_nParentId, 16).toAsciiUpperCase());
        }
        break;
    case FIELD_PROP_PAR6:
        {
            rAny <<= OUString(OUString::number(m_nPostItId, 16).toAsciiUpperCase());
        }
        break;
    case FIELD_PROP_TITLE:
        break;
    default:
        assert(false);
    }
    return true;
}

bool SwPostItField::PutValue( const uno::Any& rAny, sal_uInt16 nWhichId )
{
    switch( nWhichId )
    {
    case FIELD_PROP_PAR1:
        rAny >>= m_sAuthor;
        break;
    case FIELD_PROP_PAR2:
        rAny >>= m_sText;
        //#i100374# new string via api, delete complex text object so SwPostItNote picks up the new string
        mpText.reset();
        break;
    case FIELD_PROP_PAR3:
        rAny >>= m_sInitials;
        break;
    case FIELD_PROP_PAR4:
        {
            OUString tmp;
            if (rAny >>= tmp)
                m_sName = SwMarkName(tmp);
        }
        break;
    case FIELD_PROP_PAR7: // PAR5 (Parent Para Id) and PAR6 (Para Id) are skipped - they are not written into xml. Used for file conversion.
        {
            OUString tmp;
            if (rAny >>= tmp)
                m_sParentName = SwMarkName(tmp);
        }
        break;
    case FIELD_PROP_BOOL1:
        rAny >>= m_bResolved;
        break;
    case FIELD_PROP_TEXT:
        OSL_FAIL("Not implemented!");
        break;
    case FIELD_PROP_DATE:
        if( auto aSetDate = o3tl::tryAccess<util::Date>(rAny) )
        {
            m_aDateTime = DateTime( Date(aSetDate->Day, aSetDate->Month, aSetDate->Year) );
        }
        break;
    case FIELD_PROP_DATE_TIME:
    {
        util::DateTime aDateTimeValue;
        if(!(rAny >>= aDateTimeValue))
            return false;
        m_aDateTime = DateTime(aDateTimeValue);
    }
    break;
    case FIELD_PROP_PAR5:
    {
        OUString sTemp;
        rAny >>= sTemp;
        m_nParentId = sTemp.toInt32(16);
    }
    break;
    case FIELD_PROP_PAR6:
    {
        OUString sTemp;
        rAny >>= sTemp;
        m_nPostItId = sTemp.toInt32(16);
    }
    break;
    default:
        assert(false);
    }
    return true;
}

void SwPostItField::dumpAsXml(xmlTextWriterPtr pWriter) const
{
    (void)xmlTextWriterStartElement(pWriter, BAD_CAST("SwPostItField"));
    (void)xmlTextWriterWriteAttribute(pWriter, BAD_CAST("name"), BAD_CAST(GetName().toString().toUtf8().getStr()));

    SwField::dumpAsXml(pWriter);

    (void)xmlTextWriterStartElement(pWriter, BAD_CAST("mpText"));
    (void)xmlTextWriterWriteFormatAttribute(pWriter, BAD_CAST("ptr"), "%p", mpText ? &*mpText : nullptr);
    if (mpText)
        mpText->dumpAsXml(pWriter);
    (void)xmlTextWriterEndElement(pWriter);

    (void)xmlTextWriterEndElement(pWriter);
}

// extended user information field type

SwExtUserFieldType::SwExtUserFieldType()
    : SwFieldType( SwFieldIds::ExtUser )
{
}

std::unique_ptr<SwFieldType> SwExtUserFieldType::Copy() const
{
    return std::make_unique<SwExtUserFieldType>();
}

OUString SwExtUserFieldType::Expand(SwExtUserSubType nSub )
{
    UserOptToken nRet = static_cast<UserOptToken>(USHRT_MAX);
    switch(nSub)
    {
    case SwExtUserSubType::Firstname:      nRet = UserOptToken::FirstName; break;
    case SwExtUserSubType::Name:           nRet = UserOptToken::LastName;  break;
    case SwExtUserSubType::Shortcut:       nRet = UserOptToken::ID; break;

    case SwExtUserSubType::Company:        nRet = UserOptToken::Company;        break;
    case SwExtUserSubType::Street:         nRet = UserOptToken::Street;         break;
    case SwExtUserSubType::Title:          nRet = UserOptToken::Title;          break;
    case SwExtUserSubType::Position:       nRet = UserOptToken::Position;       break;
    case SwExtUserSubType::PhonePrivate:  nRet = UserOptToken::TelephoneHome;    break;
    case SwExtUserSubType::PhoneCompany:  nRet = UserOptToken::TelephoneWork;    break;
    case SwExtUserSubType::Fax:            nRet = UserOptToken::Fax;            break;
    case SwExtUserSubType::Email:          nRet = UserOptToken::Email;          break;
    case SwExtUserSubType::Country:        nRet = UserOptToken::Country;        break;
    case SwExtUserSubType::Zip:            nRet = UserOptToken::Zip;            break;
    case SwExtUserSubType::City:           nRet = UserOptToken::City;           break;
    case SwExtUserSubType::State:          nRet = UserOptToken::State;          break;
    case SwExtUserSubType::FathersName:    nRet = UserOptToken::FathersName;    break;
    case SwExtUserSubType::Apartment:      nRet = UserOptToken::Apartment;      break;
    default:             OSL_ENSURE( false, "Field unknown");
    }
    if( static_cast<UserOptToken>(USHRT_MAX) != nRet )
    {
        SvtUserOptions& rUserOpt = SwModule::get()->GetUserOptions();
        return rUserOpt.GetToken( nRet );
    }
    return OUString();
}

// extended user information field

SwExtUserField::SwExtUserField(SwExtUserFieldType* pTyp, SwExtUserSubType nSubTyp, SwAuthorFormat nFormat) :
    SwField(pTyp), m_nType(nSubTyp), m_nFormat(nFormat)
{
    m_aContent = SwExtUserFieldType::Expand(m_nType);
}

OUString SwExtUserField::ExpandImpl(SwRootFrame const*const) const
{
    if (!IsFixed())
        const_cast<SwExtUserField*>(this)->m_aContent = SwExtUserFieldType::Expand(m_nType);

    return m_aContent;
}

std::unique_ptr<SwField> SwExtUserField::Copy() const
{
    std::unique_ptr<SwExtUserField> pField(new SwExtUserField(static_cast<SwExtUserFieldType*>(GetTyp()), m_nType, GetFormat()));
    pField->SetExpansion(m_aContent);

    return std::unique_ptr<SwField>(pField.release());
}

SwExtUserSubType SwExtUserField::GetSubType() const
{
    return m_nType;
}

void SwExtUserField::SetSubType(SwExtUserSubType nSub)
{
    m_nType = nSub;
}

bool SwExtUserField::QueryValue( uno::Any& rAny, sal_uInt16 nWhichId ) const
{
    switch( nWhichId )
    {
    case FIELD_PROP_PAR1:
        rAny <<= m_aContent;
        break;

    case FIELD_PROP_USHORT1:
        {
            sal_Int16 nTmp = static_cast<sal_uInt16>(m_nType);
            rAny <<= nTmp;
        }
        break;
    case FIELD_PROP_BOOL1:
        rAny <<= IsFixed();
        break;
    default:
        assert(false);
    }
    return true;
}

bool SwExtUserField::PutValue( const uno::Any& rAny, sal_uInt16 nWhichId )
{
    switch( nWhichId )
    {
    case FIELD_PROP_PAR1:
        rAny >>= m_aContent;
        break;

    case FIELD_PROP_USHORT1:
        {
            sal_Int16 nTmp = 0;
            rAny >>= nTmp;
            m_nType = static_cast<SwExtUserSubType>(nTmp);
        }
        break;
    case FIELD_PROP_BOOL1:
        if( *o3tl::doAccess<bool>(rAny) )
            m_nFormat |= SwAuthorFormat::Fixed;
        else
            m_nFormat &= ~SwAuthorFormat::Fixed;
        break;
    default:
        assert(false);
    }
    return true;
}

// field type for relative page numbers

SwRefPageSetFieldType::SwRefPageSetFieldType()
    : SwFieldType( SwFieldIds::RefPageSet )
{
}

std::unique_ptr<SwFieldType> SwRefPageSetFieldType::Copy() const
{
    return std::make_unique<SwRefPageSetFieldType>();
}

// overridden since there is nothing to update
void SwRefPageSetFieldType::SwClientNotify(const SwModify&, const SfxHint&)
{
}

// field for relative page numbers

SwRefPageSetField::SwRefPageSetField( SwRefPageSetFieldType* pTyp,
                    short nOff, bool bFlag )
    : SwField( pTyp ), m_nOffset( nOff ), m_bOn( bFlag )
{
}

OUString SwRefPageSetField::ExpandImpl(SwRootFrame const*const) const
{
    return OUString();
}

std::unique_ptr<SwField> SwRefPageSetField::Copy() const
{
    return std::make_unique<SwRefPageSetField>( static_cast<SwRefPageSetFieldType*>(GetTyp()), m_nOffset, m_bOn );
}

OUString SwRefPageSetField::GetPar2() const
{
    return OUString::number(GetOffset());
}

void SwRefPageSetField::SetPar2(const OUString& rStr)
{
    SetOffset( static_cast<short>(rStr.toInt32()) );
}

bool SwRefPageSetField::QueryValue( uno::Any& rAny, sal_uInt16 nWhichId ) const
{
    switch( nWhichId )
    {
    case FIELD_PROP_BOOL1:
        rAny <<= m_bOn;
        break;
    case FIELD_PROP_USHORT1:
        rAny <<= static_cast<sal_Int16>(m_nOffset);
        break;
    default:
        assert(false);
    }
    return true;
}

bool SwRefPageSetField::PutValue( const uno::Any& rAny, sal_uInt16 nWhichId )
{
    switch( nWhichId )
    {
    case FIELD_PROP_BOOL1:
        m_bOn = *o3tl::doAccess<bool>(rAny);
        break;
    case FIELD_PROP_USHORT1:
        rAny >>=m_nOffset;
        break;
    default:
        assert(false);
    }
    return true;
}

// relative page numbers - query field

SwRefPageGetFieldType::SwRefPageGetFieldType( SwDoc& rDc )
    : SwFieldType( SwFieldIds::RefPageGet ), m_rDoc( rDc ), m_nNumberingType( SVX_NUM_ARABIC )
{
}

std::unique_ptr<SwFieldType> SwRefPageGetFieldType::Copy() const
{
    std::unique_ptr<SwRefPageGetFieldType> pNew(new SwRefPageGetFieldType( m_rDoc ));
    pNew->m_nNumberingType = m_nNumberingType;
    return pNew;
}

void SwRefPageGetFieldType::SwClientNotify(const SwModify&, const SfxHint& rHint)
{
    if (rHint.GetId() == SfxHintId::SwFormatChange
        || rHint.GetId() == SfxHintId::SwObjectDying
        || rHint.GetId() == SfxHintId::SwUpdateAttr)
    {
        // forward to text fields, they "expand" the text
        CallSwClientNotify(rHint);
        return;
    }
    if (rHint.GetId() != SfxHintId::SwLegacyModify && rHint.GetId() != SfxHintId::SwAttrSetChange)
        return;
    const sw::LegacyModifyHint* pLegacy = nullptr;
    const sw::AttrSetChangeHint* pChangeHint = nullptr;
    if (rHint.GetId() == SfxHintId::SwLegacyModify)
        pLegacy = static_cast<const sw::LegacyModifyHint*>(&rHint);
    else // rHint.GetId() == SfxHintId::SwAttrSetChange
        pChangeHint = static_cast<const sw::AttrSetChangeHint*>(&rHint);
    auto const ModifyImpl = [this](SwRootFrame const*const pLayout)
    {
        // first collect all SetPageRefFields
        SetGetExpFields aTmpLst;
        if (MakeSetList(aTmpLst, pLayout))
        {
            std::vector<SwFormatField*> vFields;
            GatherFields(vFields);
            for(auto pFormatField: vFields)
                UpdateField(pFormatField->GetTextField(), aTmpLst, pLayout);
        }
    };

    // update all GetReference fields
    if( (pLegacy && !pLegacy->m_pNew && !pLegacy->m_pOld && HasWriterListeners())
        || (pChangeHint && !pChangeHint->m_pNew && !pChangeHint->m_pOld && HasWriterListeners()))
    {
        SwRootFrame const* pLayout(nullptr);
        SwRootFrame const* pLayoutRLHidden(nullptr);
        for (SwRootFrame const*const pLay : m_rDoc.GetAllLayouts())
        {
            if (pLay->IsHideRedlines())
            {
                pLayoutRLHidden = pLay;
            }
            else
            {
                pLayout = pLay;
            }
        }
        ModifyImpl(pLayout);
        if (pLayoutRLHidden)
        {
            ModifyImpl(pLayoutRLHidden);
        }
    }

    // forward to text fields, they "expand" the text
    CallSwClientNotify(rHint);
}

bool SwRefPageGetFieldType::MakeSetList(SetGetExpFields& rTmpLst,
        SwRootFrame const*const pLayout)
{
    IDocumentRedlineAccess const& rIDRA(m_rDoc.getIDocumentRedlineAccess());
    std::vector<SwFormatField*> vFields;
    m_rDoc.getIDocumentFieldsAccess().GetSysFieldType(SwFieldIds::RefPageSet)->GatherFields(vFields);
    for(auto pFormatField: vFields)
    {
        // update only the GetRef fields
        const SwTextField* pTField = pFormatField->GetTextField();
        if (!pLayout || !pLayout->IsHideRedlines() || !sw::IsFieldDeletedInModel(rIDRA, *pTField))
        {
            const SwTextNode& rTextNd = pTField->GetTextNode();

            // Always the first! (in Tab-Headline, header/footer )
            Point aPt;
            std::pair<Point, bool> const tmp(aPt, false);
            const SwContentFrame *const pFrame = rTextNd.getLayoutFrame(
                pLayout, nullptr, &tmp);

            std::unique_ptr<SetGetExpField> pNew;

            if( !pFrame ||
                 pFrame->IsInDocBody() ||
                // #i31868#
                // Check if pFrame is not yet connected to the layout.
                !pFrame->FindPageFrame() )
            {
                pNew.reset( new SetGetExpField( rTextNd, pTField ) );
            }
            else
            {
                //  create index for determination of the TextNode
                SwPosition aPos( m_rDoc.GetNodes().GetEndOfPostIts() );
                bool const bResult = GetBodyTextNode( m_rDoc, aPos, *pFrame );
                OSL_ENSURE(bResult, "where is the Field?");
                pNew.reset( new SetGetExpField( aPos.GetNode(), pTField,
                                            aPos.GetContentIndex() ) );
            }

            rTmpLst.insert( std::move(pNew) );
        }
    }
    return !rTmpLst.empty();
}

void SwRefPageGetFieldType::UpdateField( SwTextField const * pTextField,
                                        SetGetExpFields const & rSetList,
                                        SwRootFrame const*const pLayout)
{
    SwRefPageGetField* pGetField = const_cast<SwRefPageGetField*>(static_cast<const SwRefPageGetField*>(pTextField->GetFormatField().GetField()));
    pGetField->SetText( OUString(), pLayout );

    // then search the correct RefPageSet field
    SwTextNode* pTextNode = &pTextField->GetTextNode();
    if( pTextNode->StartOfSectionIndex() >
        m_rDoc.GetNodes().GetEndOfExtras().GetIndex() )
    {
        SetGetExpField aEndField( *pTextNode, pTextField );

        SetGetExpFields::const_iterator itLast = rSetList.lower_bound( &aEndField );

        if( itLast != rSetList.begin() )
        {
            --itLast;
            const SwTextField* pRefTextField = (*itLast)->GetTextField();
            const SwRefPageSetField* pSetField =
                        static_cast<const SwRefPageSetField*>(pRefTextField->GetFormatField().GetField());
            if( pSetField->IsOn() )
            {
                // determine the correct offset
                Point aPt;
                std::pair<Point, bool> const tmp(aPt, false);
                const SwContentFrame *const pFrame = pTextNode->getLayoutFrame(
                    pLayout, nullptr, &tmp);
                const SwContentFrame *const pRefFrame = pRefTextField->GetTextNode().getLayoutFrame(
                    pLayout, nullptr, &tmp);
                const SwPageFrame* pPgFrame = nullptr;
                short nDiff = 1;
                if ( pFrame && pRefFrame )
                {
                    pPgFrame = pFrame->FindPageFrame();
                    nDiff = pPgFrame->GetPhyPageNum() -
                            pRefFrame->FindPageFrame()->GetPhyPageNum() + 1;
                }

                SvxNumType nTmpFormat = SVX_NUM_PAGEDESC == pGetField->GetFormat()
                        ? ( !pPgFrame
                                ? SVX_NUM_ARABIC
                                : pPgFrame->GetPageDesc()->GetNumType().GetNumberingType() )
                        : pGetField->GetFormat();
                const short nPageNum = std::max<short>(0, pSetField->GetOffset() + nDiff);
                pGetField->SetText(FormatNumber(nPageNum, nTmpFormat), pLayout);
            }
        }
    }
    // start formatting
    const_cast<SwFormatField&>(pTextField->GetFormatField()).ForceUpdateTextNode();
}

// queries for relative page numbering

SwRefPageGetField::SwRefPageGetField( SwRefPageGetFieldType* pTyp,
                                    SvxNumType nFormat )
    : SwField( pTyp ), m_nFormat(nFormat)
{
}

void SwRefPageGetField::SetText(const OUString& rText,
        SwRootFrame const*const pLayout)
{
    if (!pLayout || !pLayout->IsHideRedlines())
    {
        m_sText = rText;
    }
    if (!pLayout || pLayout->IsHideRedlines())
    {
        m_sTextRLHidden = rText;
    }
}

OUString SwRefPageGetField::ExpandImpl(SwRootFrame const*const pLayout) const
{
    return pLayout && pLayout->IsHideRedlines() ? m_sTextRLHidden : m_sText;
}

std::unique_ptr<SwField> SwRefPageGetField::Copy() const
{
    std::unique_ptr<SwRefPageGetField> pCpy(new SwRefPageGetField(
                        static_cast<SwRefPageGetFieldType*>(GetTyp()), GetFormat() ));
    pCpy->m_sText = m_sText;
    pCpy->m_sTextRLHidden = m_sTextRLHidden;
    return std::unique_ptr<SwField>(pCpy.release());
}

void SwRefPageGetField::ChangeExpansion(const SwFrame& rFrame,
                                        const SwTextField* pField )
{
    // only fields in Footer, Header, FootNote, Flys
    SwRefPageGetFieldType* pGetType = static_cast<SwRefPageGetFieldType*>(GetTyp());
    SwDoc& rDoc = pGetType->GetDoc();
    if( pField->GetTextNode().StartOfSectionIndex() >
        rDoc.GetNodes().GetEndOfExtras().GetIndex() )
        return;

    SwRootFrame const& rLayout(*rFrame.getRootFrame());
    OUString & rText(rLayout.IsHideRedlines() ? m_sTextRLHidden : m_sText);
    rText.clear();

    OSL_ENSURE(!rFrame.IsInDocBody(), "Flag incorrect, frame is in DocBody");

    // collect all SetPageRefFields
    SetGetExpFields aTmpLst;
    if (!pGetType->MakeSetList(aTmpLst, &rLayout))
        return ;

    //  create index for determination of the TextNode
    SwPosition aPos( rDoc.GetNodes() );

    // If no layout exists, ChangeExpansion is called for header and
    // footer lines via layout formatting without existing TextNode.
    if(!GetBodyTextNode(rDoc, aPos, rFrame))
        return;

    SetGetExpField aEndField( aPos.GetNode(), pField, aPos.GetContentIndex() );

    SetGetExpFields::const_iterator itLast = aTmpLst.lower_bound( &aEndField );

    if( itLast == aTmpLst.begin() )
        return;        // there is no corresponding set-field in front
    --itLast;

    const SwTextField* pRefTextField = (*itLast)->GetTextField();
    const SwRefPageSetField* pSetField =
                        static_cast<const SwRefPageSetField*>(pRefTextField->GetFormatField().GetField());
    Point aPt;
    std::pair<Point, bool> const tmp(aPt, false);
    const SwContentFrame *const pRefFrame = pRefTextField->GetTextNode().getLayoutFrame(
            &rLayout, nullptr, &tmp);
    if( !(pSetField->IsOn() && pRefFrame) )
        return;

    // determine the correct offset
    const SwPageFrame* pPgFrame = rFrame.FindPageFrame();
    const short nDiff = pPgFrame->GetPhyPageNum() -
                        pRefFrame->FindPageFrame()->GetPhyPageNum() + 1;

    SwRefPageGetField* pGetField = const_cast<SwRefPageGetField*>(static_cast<const SwRefPageGetField*>(pField->GetFormatField().GetField()));
    SvxNumType nTmpFormat = SVX_NUM_PAGEDESC == pGetField->GetFormat()
                        ? pPgFrame->GetPageDesc()->GetNumType().GetNumberingType()
                        : pGetField->GetFormat();
    const short nPageNum = std::max<short>(0, pSetField->GetOffset() + nDiff);
    pGetField->SetText(FormatNumber(nPageNum, nTmpFormat), &rLayout);
}

bool SwRefPageGetField::QueryValue( uno::Any& rAny, sal_uInt16 nWhichId ) const
{
    switch( nWhichId )
    {
        case FIELD_PROP_USHORT1:
            rAny <<= static_cast<sal_Int16>(GetFormat());
        break;
        case FIELD_PROP_PAR1:
            rAny <<= m_sText;
        break;
        default:
            assert(false);
    }
    return true;
}

bool SwRefPageGetField::PutValue( const uno::Any& rAny, sal_uInt16 nWhichId )
{
    switch( nWhichId )
    {
        case FIELD_PROP_USHORT1:
        {
            sal_Int16 nSet = 0;
            rAny >>= nSet;
            if(nSet <= SVX_NUM_PAGEDESC )
                m_nFormat = static_cast<SvxNumType>(nSet);
        }
        break;
        case FIELD_PROP_PAR1:
            rAny >>= m_sText;
            m_sTextRLHidden = m_sText;
        break;
    default:
        assert(false);
    }
    return true;
}

// field type to jump to and edit

SwJumpEditFieldType::SwJumpEditFieldType( SwDoc& rD )
    : SwFieldType( SwFieldIds::JumpEdit ), m_rDoc( rD ), m_aDep( *this )
{
}

std::unique_ptr<SwFieldType> SwJumpEditFieldType::Copy() const
{
    return std::make_unique<SwJumpEditFieldType>( m_rDoc );
}

SwCharFormat* SwJumpEditFieldType::GetCharFormat()
{
    SwCharFormat* pFormat = m_rDoc.getIDocumentStylePoolAccess().GetCharFormatFromPool( RES_POOLCHR_JUMPEDIT );
    m_aDep.StartListening(pFormat);
    return pFormat;
}

SwJumpEditField::SwJumpEditField( SwJumpEditFieldType* pTyp, SwJumpEditFormat nFormat,
                                OUString aText, OUString aHelp )
    : SwField( pTyp), m_sText( std::move(aText) ), m_sHelp( std::move(aHelp) ), m_nFormat(nFormat)
{
}

OUString SwJumpEditField::ExpandImpl(SwRootFrame const*const) const
{
    return "<" + m_sText + ">";
}

std::unique_ptr<SwField> SwJumpEditField::Copy() const
{
    return std::make_unique<SwJumpEditField>( static_cast<SwJumpEditFieldType*>(GetTyp()), GetFormat(),
                                m_sText, m_sHelp );
}

/// get place holder text
OUString SwJumpEditField::GetPar1() const
{
    return m_sText;
}

/// set place holder text
void SwJumpEditField::SetPar1(const OUString& rStr)
{
    m_sText = rStr;
}

/// get hint text
OUString SwJumpEditField::GetPar2() const
{
    return m_sHelp;
}

/// set hint text
void SwJumpEditField::SetPar2(const OUString& rStr)
{
    m_sHelp = rStr;
}

bool SwJumpEditField::QueryValue( uno::Any& rAny, sal_uInt16 nWhichId ) const
{
    switch( nWhichId )
    {
    case FIELD_PROP_USHORT1:
        {
            sal_Int16 nRet;
            switch( GetFormat() )
            {
            case SwJumpEditFormat::Table:  nRet = text::PlaceholderType::TABLE; break;
            case SwJumpEditFormat::Frame:  nRet = text::PlaceholderType::TEXTFRAME; break;
            case SwJumpEditFormat::Graphic:nRet = text::PlaceholderType::GRAPHIC; break;
            case SwJumpEditFormat::OLE:    nRet = text::PlaceholderType::OBJECT; break;
            default:
                nRet = text::PlaceholderType::TEXT; break;
            }
            rAny <<= nRet;
        }
        break;
    case FIELD_PROP_PAR1 :
        rAny <<= m_sHelp;
        break;
    case FIELD_PROP_PAR2 :
         rAny <<= m_sText;
         break;
    default:
        assert(false);
    }
    return true;
}

bool SwJumpEditField::PutValue( const uno::Any& rAny, sal_uInt16 nWhichId )
{
    switch( nWhichId )
    {
    case FIELD_PROP_USHORT1:
        {
            //JP 24.10.2001: int32 because in UnoField.cxx a putvalue is
            //              called with a int32 value! But normally we need
            //              here only a int16
            sal_Int32 nSet = 0;
            rAny >>= nSet;
            switch( nSet )
            {
                case text::PlaceholderType::TEXT     : m_nFormat = SwJumpEditFormat::Text; break;
                case text::PlaceholderType::TABLE    : m_nFormat = SwJumpEditFormat::Table; break;
                case text::PlaceholderType::TEXTFRAME: m_nFormat = SwJumpEditFormat::Frame; break;
                case text::PlaceholderType::GRAPHIC  : m_nFormat = SwJumpEditFormat::Graphic; break;
                case text::PlaceholderType::OBJECT   : m_nFormat = SwJumpEditFormat::OLE; break;
            }
        }
        break;
    case FIELD_PROP_PAR1 :
        rAny >>= m_sHelp;
        break;
    case FIELD_PROP_PAR2 :
         rAny >>= m_sText;
         break;
    default:
        assert(false);
    }
    return true;
}

// combined character field type

SwCombinedCharFieldType::SwCombinedCharFieldType()
    : SwFieldType( SwFieldIds::CombinedChars )
{
}

std::unique_ptr<SwFieldType> SwCombinedCharFieldType::Copy() const
{
    return std::make_unique<SwCombinedCharFieldType>();
}

// combined character field

SwCombinedCharField::SwCombinedCharField( SwCombinedCharFieldType* pFTyp,
                                            const OUString& rChars )
    : SwField( pFTyp ),
    m_sCharacters( rChars.copy( 0, std::min<sal_Int32>(rChars.getLength(), MAX_COMBINED_CHARACTERS) ))
{
}

OUString SwCombinedCharField::ExpandImpl(SwRootFrame const*const) const
{
    return m_sCharacters;
}

std::unique_ptr<SwField> SwCombinedCharField::Copy() const
{
    return std::make_unique<SwCombinedCharField>( static_cast<SwCombinedCharFieldType*>(GetTyp()),
                                        m_sCharacters );
}

OUString SwCombinedCharField::GetPar1() const
{
    return m_sCharacters;
}

void SwCombinedCharField::SetPar1(const OUString& rStr)
{
    m_sCharacters = rStr.copy(0, std::min<sal_Int32>(rStr.getLength(), MAX_COMBINED_CHARACTERS));
}

bool SwCombinedCharField::QueryValue( uno::Any& rAny,
                                        sal_uInt16 nWhichId ) const
{
    switch( nWhichId )
    {
    case FIELD_PROP_PAR1:
        rAny <<= m_sCharacters;
        break;
    default:
        assert(false);
    }
    return true;
}

bool SwCombinedCharField::PutValue( const uno::Any& rAny,
                                        sal_uInt16 nWhichId )
{
    switch( nWhichId )
    {
        case FIELD_PROP_PAR1:
        {
            OUString sTmp;
            rAny >>= sTmp;
            SetPar1(sTmp);
        }
        break;
        default:
            assert(false);
    }
    return true;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
