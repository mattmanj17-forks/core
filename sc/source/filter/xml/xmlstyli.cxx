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

#include "xmlstyli.hxx"
#include <xmloff/xmlnamespace.hxx>
#include <xmloff/xmlimppr.hxx>
#include <xmloff/families.hxx>
#include <xmloff/xmlnumfi.hxx>
#include <xmloff/XMLGraphicsDefaultStyle.hxx>
#include <xmloff/xmltoken.hxx>
#include <com/sun/star/style/XStyleFamiliesSupplier.hpp>
#include <com/sun/star/table/BorderLine2.hpp>
#include <comphelper/extract.hxx>
#include <xmloff/xmlprcon.hxx>
#include <xmloff/XMLComplexColorContext.hxx>
#include "XMLTableHeaderFooterContext.hxx"
#include "XMLConverter.hxx"
#include "XMLTableShapeImportHelper.hxx"
#include <sheetdata.hxx>
#include "xmlannoi.hxx"
#include <textuno.hxx>
#include <cellsuno.hxx>
#include "xmlstyle.hxx"

#include <docuno.hxx>
#include <unonames.hxx>
#include <document.hxx>
#include <conditio.hxx>
#include <rangelst.hxx>

#include <xmloff/xmltypes.hxx>
#include <xmloff/contextid.hxx>
#include <xmloff/txtprmap.hxx>

#define XML_LINE_LEFT 0
#define XML_LINE_RIGHT 1
#define XML_LINE_TOP 2
#define XML_LINE_BOTTOM 3

#define XML_LINE_TLBR 0
#define XML_LINE_BLTR 1

using namespace ::com::sun::star;
using namespace ::com::sun::star::xml::sax;
using namespace ::com::sun::star::style;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::container;
using namespace xmloff::token;
using namespace ::formula;

using com::sun::star::uno::UNO_QUERY;
ScXMLCellImportPropertyMapper::ScXMLCellImportPropertyMapper(
        const rtl::Reference< XMLPropertySetMapper >& rMapper,
        SvXMLImport& rImportP) :
    SvXMLImportPropertyMapper( rMapper, rImportP )
{
}

ScXMLCellImportPropertyMapper::~ScXMLCellImportPropertyMapper()
{
}

void ScXMLCellImportPropertyMapper::finished(::std::vector< XMLPropertyState >& rProperties, sal_Int32 nStartIndex, sal_Int32 nEndIndex ) const
{
    static const sal_Int16 aPaddingCTF[4] = { CTF_SC_LEFTPADDING, CTF_SC_RIGHTPADDING,
                                            CTF_SC_TOPPADDING, CTF_SC_BOTTOMPADDING };
    static const sal_Int16 aBorderCTF[4] = { CTF_SC_LEFTBORDER, CTF_SC_RIGHTBORDER,
                                            CTF_SC_TOPBORDER, CTF_SC_BOTTOMBORDER };

    SvXMLImportPropertyMapper::finished(rProperties, nStartIndex, nEndIndex);
    XMLPropertyState* pAllPaddingProperty(nullptr);
    XMLPropertyState* pPadding[4] = { nullptr, nullptr, nullptr, nullptr };
    XMLPropertyState* pNewPadding[4] = { nullptr, nullptr, nullptr, nullptr };
    XMLPropertyState* pAllBorderProperty = nullptr;
    XMLPropertyState* pBorders[4] = { nullptr, nullptr, nullptr, nullptr };
    XMLPropertyState* pNewBorders[4] = { nullptr, nullptr, nullptr, nullptr };
    XMLPropertyState* pAllBorderWidthProperty = nullptr;
    XMLPropertyState* pBorderWidths[4] = { nullptr, nullptr, nullptr, nullptr };
    XMLPropertyState* pDiagBorders[2] = { nullptr };
    XMLPropertyState* pOldDiagBorderWidths[2] = { nullptr };      // old attribute names without "s"
    XMLPropertyState* pDiagBorderWidths[2] = { nullptr };

    for (auto& rProperty : rProperties)
    {
        XMLPropertyState*property = &rProperty;
        if (property->mnIndex != -1)
        {
            sal_Int16 nContextID = getPropertySetMapper()->GetEntryContextId(property->mnIndex);
            switch (nContextID)
            {
                case CTF_SC_ALLPADDING                  : pAllPaddingProperty = property; break;
                case CTF_SC_LEFTPADDING                 : pPadding[XML_LINE_LEFT] = property; break;
                case CTF_SC_RIGHTPADDING                : pPadding[XML_LINE_RIGHT] = property; break;
                case CTF_SC_TOPPADDING                  : pPadding[XML_LINE_TOP] = property; break;
                case CTF_SC_BOTTOMPADDING               : pPadding[XML_LINE_BOTTOM] = property; break;
                case CTF_SC_ALLBORDER                   : pAllBorderProperty = property; break;
                case CTF_SC_LEFTBORDER                  : pBorders[XML_LINE_LEFT] = property; break;
                case CTF_SC_RIGHTBORDER                 : pBorders[XML_LINE_RIGHT] = property; break;
                case CTF_SC_TOPBORDER                   : pBorders[XML_LINE_TOP] = property; break;
                case CTF_SC_BOTTOMBORDER                : pBorders[XML_LINE_BOTTOM] = property; break;
                case CTF_SC_ALLBORDERWIDTH              : pAllBorderWidthProperty = property; break;
                case CTF_SC_LEFTBORDERWIDTH             : pBorderWidths[XML_LINE_LEFT] = property; break;
                case CTF_SC_RIGHTBORDERWIDTH            : pBorderWidths[XML_LINE_RIGHT] = property; break;
                case CTF_SC_TOPBORDERWIDTH              : pBorderWidths[XML_LINE_TOP] = property; break;
                case CTF_SC_BOTTOMBORDERWIDTH           : pBorderWidths[XML_LINE_BOTTOM] = property; break;
                case CTF_SC_DIAGONALTLBR                : pDiagBorders[XML_LINE_TLBR] = property; break;
                case CTF_SC_DIAGONALBLTR                : pDiagBorders[XML_LINE_BLTR] = property; break;
                case CTF_SC_DIAGONALTLBRWIDTH           : pOldDiagBorderWidths[XML_LINE_TLBR] = property; break;
                case CTF_SC_DIAGONALTLBRWIDTHS          : pDiagBorderWidths[XML_LINE_TLBR] = property; break;
                case CTF_SC_DIAGONALBLTRWIDTH           : pOldDiagBorderWidths[XML_LINE_BLTR] = property; break;
                case CTF_SC_DIAGONALBLTRWIDTHS          : pDiagBorderWidths[XML_LINE_BLTR] = property; break;
            }
        }
    }
    sal_uInt16 i;

    // #i27594#; copy Value, but don't insert
    if (pAllBorderWidthProperty)
        pAllBorderWidthProperty->mnIndex = -1;
    if (pAllBorderProperty)
        pAllBorderProperty->mnIndex = -1;
    if (pAllPaddingProperty)
        pAllPaddingProperty->mnIndex = -1;

    for (i = 0; i < 4; ++i)
    {
        if (pAllPaddingProperty && !pPadding[i])
            pNewPadding[i] = new XMLPropertyState(maPropMapper->FindEntryIndex(aPaddingCTF[i]), pAllPaddingProperty->maValue);
        if (pAllBorderProperty && !pBorders[i])
        {
            pNewBorders[i] = new XMLPropertyState(maPropMapper->FindEntryIndex(aBorderCTF[i]), pAllBorderProperty->maValue);
            pBorders[i] = pNewBorders[i];
        }
        if( !pBorderWidths[i] )
            pBorderWidths[i] = pAllBorderWidthProperty;
        else
            pBorderWidths[i]->mnIndex = -1;
        if( pBorders[i] )
        {
            table::BorderLine2 aBorderLine;
            pBorders[i]->maValue >>= aBorderLine;
            if( pBorderWidths[i] )
            {
                // Merge style:border-line-width values to fo:border values. Do
                // not override fo:border line width or line style with an
                // empty value!
                table::BorderLine2 aBorderLineWidth;
                pBorderWidths[i]->maValue >>= aBorderLineWidth;
                aBorderLine.OuterLineWidth = aBorderLineWidth.OuterLineWidth;
                aBorderLine.InnerLineWidth = aBorderLineWidth.InnerLineWidth;
                aBorderLine.LineDistance = aBorderLineWidth.LineDistance;
                pBorders[i]->maValue <<= aBorderLine;
            }
        }
    }
    for( i = 0; i < 2; ++i )
    {
        if( pDiagBorders[i] && ( pDiagBorderWidths[i] || pOldDiagBorderWidths[i] ) )
        {
            table::BorderLine2 aBorderLine;
            pDiagBorders[i]->maValue >>= aBorderLine;
            table::BorderLine2 aBorderLineWidth;
            if (pDiagBorderWidths[i])
                pDiagBorderWidths[i]->maValue >>= aBorderLineWidth;     // prefer new attribute
            else
                pOldDiagBorderWidths[i]->maValue >>= aBorderLineWidth;
            aBorderLine.OuterLineWidth = aBorderLineWidth.OuterLineWidth;
            aBorderLine.InnerLineWidth = aBorderLineWidth.InnerLineWidth;
            aBorderLine.LineDistance = aBorderLineWidth.LineDistance;
            pDiagBorders[i]->maValue <<= aBorderLine;
            if (pDiagBorderWidths[i])
                pDiagBorderWidths[i]->mnIndex = -1;
            if (pOldDiagBorderWidths[i])
                pOldDiagBorderWidths[i]->mnIndex = -1;      // reset mnIndex for old and new attribute if both are present
        }
    }

    for (i = 0; i < 4; ++i)
    {
        if (pNewPadding[i])
        {
            rProperties.push_back(*pNewPadding[i]);
            delete pNewPadding[i];
        }
        if (pNewBorders[i])
        {
            rProperties.push_back(*pNewBorders[i]);
            delete pNewBorders[i];
        }
    }
}

ScXMLRowImportPropertyMapper::ScXMLRowImportPropertyMapper(
        const rtl::Reference< XMLPropertySetMapper >& rMapper,
        SvXMLImport& rImportP) :
    SvXMLImportPropertyMapper( rMapper, rImportP )
{
}

ScXMLRowImportPropertyMapper::~ScXMLRowImportPropertyMapper()
{
}

void ScXMLRowImportPropertyMapper::finished(::std::vector< XMLPropertyState >& rProperties, sal_Int32 nStartIndex, sal_Int32 nEndIndex ) const
{
    SvXMLImportPropertyMapper::finished(rProperties, nStartIndex, nEndIndex);
    XMLPropertyState* pHeight(nullptr);
    XMLPropertyState* pOptimalHeight(nullptr);
    XMLPropertyState* pPageBreak(nullptr);
    for (auto& rProperty : rProperties)
    {
        XMLPropertyState* property = &rProperty;
        if (property->mnIndex != -1)
        {
            sal_Int16 nContextID = getPropertySetMapper()->GetEntryContextId(property->mnIndex);
            switch (nContextID)
            {
                case CTF_SC_ROWHEIGHT                   : pHeight = property; break;
                case CTF_SC_ROWOPTIMALHEIGHT            : pOptimalHeight = property; break;
                case CTF_SC_ROWBREAKBEFORE              : pPageBreak = property; break;
            }
        }
    }
    if (pPageBreak)
    {
        if(!(::cppu::any2bool(pPageBreak->maValue)))
            pPageBreak->mnIndex = -1;
    }
    if (pOptimalHeight)
    {
        if (::cppu::any2bool(pOptimalHeight->maValue))
        {
            if (pHeight)
            {
                // set the stored height, but keep "optimal" flag:
                // pass the height value as OptimalHeight property (only allowed while loading!)
                pOptimalHeight->maValue = pHeight->maValue;
                pHeight->mnIndex = -1;
            }
            else
                pOptimalHeight->mnIndex = -1;
        }
    }
    else if (pHeight)
    {
        rProperties.emplace_back(maPropMapper->FindEntryIndex(CTF_SC_ROWOPTIMALHEIGHT), css::uno::Any(false));
    }
    // don't access pointers to rProperties elements after push_back!
}

namespace {

class XMLTableCellPropsContext : public SvXMLPropertySetContext
{
    public:
        XMLTableCellPropsContext(
             SvXMLImport& rImport, sal_Int32 nElement,
             const uno::Reference< xml::sax::XFastAttributeList >& xAttrList,
             sal_uInt32 nFamily,
             ::std::vector< XMLPropertyState > &rProps,
             SvXMLImportPropertyMapper* pMap);

    using SvXMLPropertySetContext::createFastChildContext;
    virtual css::uno::Reference< css::xml::sax::XFastContextHandler > createFastChildContext(
        sal_Int32 nElement,
        const css::uno::Reference< css::xml::sax::XFastAttributeList >& xAttrList,
        ::std::vector< XMLPropertyState > &rProperties,
        const XMLPropertyState& rProp ) override;
};

}

XMLTableCellPropsContext::XMLTableCellPropsContext(
             SvXMLImport& rImport, sal_Int32 nElement,
             const uno::Reference< xml::sax::XFastAttributeList >& xAttrList,
             sal_uInt32 nFamily,
             ::std::vector< XMLPropertyState > &rProps,
             SvXMLImportPropertyMapper* pMap)
          : SvXMLPropertySetContext( rImport, nElement, xAttrList, nFamily,
               rProps, pMap )
{
}

css::uno::Reference< css::xml::sax::XFastContextHandler > XMLTableCellPropsContext::createFastChildContext(
    sal_Int32 nElement,
    const css::uno::Reference< css::xml::sax::XFastAttributeList >& xAttrList,
    ::std::vector< XMLPropertyState > &rProperties,
    const XMLPropertyState& rProperty)
{
    switch (mpMapper->getPropertySetMapper()->GetEntryContextId(rProperty.mnIndex))
    {
        case CTF_COMPLEX_COLOR:
        {
            return new XMLPropertyComplexColorContext(GetImport(), nElement, xAttrList, rProperty, rProperties);
        }
        break;
        case CTF_SC_HYPERLINK:
        {
            OUString sURL;
            for (auto &aIter : sax_fastparser::castToFastAttributeList( xAttrList ))
            {
                if ( aIter.getToken() == XML_ELEMENT(XLINK, XML_HREF) )
                    sURL = aIter.toString();
                else
                    XMLOFF_WARN_UNKNOWN("sc", aIter);
            }
            if ( !sURL.isEmpty() )
            {
                XMLPropertyState aProp(rProperty);
                aProp.maValue <<=  sURL;
                rProperties.push_back( aProp );
            }
        }
        break;
        default:
            break;
    }
    return SvXMLPropertySetContext::createFastChildContext(nElement, xAttrList, rProperties, rProperty);
}

namespace {

class ScXMLMapContext : public SvXMLImportContext
{
    OUString msApplyStyle;
    OUString msCondition;
    OUString msBaseCell;

    ScXMLImport& GetScImport() { return static_cast<ScXMLImport&>(GetImport()); }
public:

    ScXMLMapContext(
            SvXMLImport& rImport, sal_Int32 nElement,
            const uno::Reference< xml::sax::XFastAttributeList > & xAttrList );

    ScCondFormatEntry* CreateConditionEntry();
};

}

ScXMLMapContext::ScXMLMapContext(SvXMLImport& rImport, sal_Int32 /*nElement*/,
            const uno::Reference< xml::sax::XFastAttributeList > & xAttrList )
    : SvXMLImportContext( rImport )
{
    for( auto &aIter : sax_fastparser::castToFastAttributeList( xAttrList ) )
    {
        OUString sValue = aIter.toString();
        switch (aIter.getToken())
        {
            case XML_ELEMENT(STYLE, XML_CONDITION):
                msCondition = sValue;
                break;
            case XML_ELEMENT(STYLE, XML_APPLY_STYLE_NAME):
                msApplyStyle = GetImport().GetStyleDisplayName( XmlStyleFamily::TABLE_CELL, sValue);
                break;
            case XML_ELEMENT(STYLE, XML_BASE_CELL_ADDRESS):
                msBaseCell = sValue;
                break;
            default:
                XMLOFF_WARN_UNKNOWN("sc", aIter);
        }
    }
}

ScCondFormatEntry* ScXMLMapContext::CreateConditionEntry()
{
    OUString aCondition, aConditionNmsp;
    FormulaGrammar::Grammar eGrammar = FormulaGrammar::GRAM_UNSPECIFIED;
    GetScImport().ExtractFormulaNamespaceGrammar( aCondition, aConditionNmsp, eGrammar, msCondition );
    bool bHasNmsp = aCondition.getLength() < msCondition.getLength();

    // parse a condition from the attribute string
    ScXMLConditionParseResult aParseResult;
    ScXMLConditionHelper::parseCondition( aParseResult, aCondition, 0 );

    if( !bHasNmsp )
    {
        // the attribute does not contain a namespace: try to find a namespace of an external grammar
        FormulaGrammar::Grammar eNewGrammar = FormulaGrammar::GRAM_UNSPECIFIED;
        GetScImport().ExtractFormulaNamespaceGrammar( aCondition, aConditionNmsp, eNewGrammar, aCondition, true );
        if( eNewGrammar != FormulaGrammar::GRAM_EXTERNAL )
            eGrammar = eNewGrammar;
    }

    ScConditionMode eMode = ScConditionEntry::GetModeFromApi(aParseResult.meOperator);
    ScDocument* pDoc = GetScImport().GetDocument();
    assert(pDoc);

    ScCondFormatEntry* pEntry =  new ScCondFormatEntry(eMode, aParseResult.maOperand1, aParseResult.maOperand2, *pDoc, ScAddress(), msApplyStyle,
                                                    OUString(), OUString(), eGrammar, eGrammar);

    pEntry->SetSrcString(msBaseCell);
    return pEntry;
}

void XMLTableStyleContext::SetAttribute( sal_Int32 nElement,
                                        const OUString& rValue )
{
    switch(nElement & TOKEN_MASK)
    {
        case XML_DATA_STYLE_NAME:
            sDataStyleName = rValue;
            break;
        case XML_MASTER_PAGE_NAME:
            sPageStyle = rValue;
            break;
        default:
            XMLPropStyleContext::SetAttribute( nElement, rValue );
    }
}


XMLTableStyleContext::XMLTableStyleContext( ScXMLImport& rImport,
        XMLTableStylesContext& rStyles, XmlStyleFamily nFamily, bool bDefaultStyle ) :
    XMLPropStyleContext( rImport, rStyles, nFamily, bDefaultStyle ),
    pStyles(&rStyles),
    nNumberFormat(-1),
    nLastSheet(-1),
    bParentSet(false),
    mpCondFormat(nullptr),
    mbDeleteCondFormat(true)
{
}

XMLTableStyleContext::~XMLTableStyleContext()
{
    if(mbDeleteCondFormat)
        delete mpCondFormat;
}

css::uno::Reference< css::xml::sax::XFastContextHandler > XMLTableStyleContext::createFastChildContext(
    sal_Int32 nElement,
    const css::uno::Reference< css::xml::sax::XFastAttributeList >& xAttrList )
{
    css::uno::Reference< css::xml::sax::XFastContextHandler > xContext;

    if( nElement == XML_ELEMENT(STYLE, XML_MAP) )
    {
        if(!mpCondFormat)
            mpCondFormat = new ScConditionalFormat( 0, *GetScImport().GetDocument() );
        ScXMLMapContext* pMapContext = new ScXMLMapContext(GetImport(), nElement, xAttrList);
        xContext = pMapContext;
        mpCondFormat->AddEntry(pMapContext->CreateConditionEntry());
    }
    else if ( nElement == XML_ELEMENT(STYLE, XML_TABLE_CELL_PROPERTIES) )
    {
        SvXMLImportPropertyMapper* pImpPrMap =
            GetStyles()->GetImportPropertyMapper(
                GetFamily() );
        if( pImpPrMap )
            xContext = new XMLTableCellPropsContext( GetImport(), nElement,
                xAttrList,
                XML_TYPE_PROP_TABLE_CELL,
                GetProperties(),
                pImpPrMap );
    }

    if (!xContext)
        xContext = XMLPropStyleContext::createFastChildContext( nElement, xAttrList );
    return xContext;
}

void XMLTableStyleContext::ApplyCondFormat( const uno::Sequence<table::CellRangeAddress>& xCellRanges )
{
    if(!mpCondFormat || GetScImport().HasNewCondFormatData())
        return;

    ScRangeList aRangeList;
    for(const table::CellRangeAddress& aAddress : xCellRanges)
    {
        ScRange aRange( aAddress.StartColumn, aAddress.StartRow, aAddress.Sheet, aAddress.EndColumn, aAddress.EndRow, aAddress.Sheet );
        aRangeList.Join( aRange );
    }

    ScDocument* pDoc = GetScImport().GetDocument();
    if (!pDoc)
        return;

    SCTAB nTab = GetScImport().GetTables().GetCurrentSheet();
    ScConditionalFormatList* pFormatList = pDoc->GetCondFormList(nTab);
    auto itr = std::find_if(pFormatList->begin(), pFormatList->end(),
        [this](const std::unique_ptr<ScConditionalFormat>& rxFormat) { return rxFormat->EqualEntries(*mpCondFormat); });
    if (itr != pFormatList->end())
    {
        ScRangeList& rRangeList = (*itr)->GetRangeList();
        sal_uInt32 nCondId = (*itr)->GetKey();
        size_t n = aRangeList.size();
        for(size_t i = 0; i < n; ++i)
        {
            const ScRange & rRange = aRangeList[i];
            rRangeList.Join(rRange);
        }

        pDoc->AddCondFormatData( aRangeList, nTab, nCondId );
        return;
    }

    if(mpCondFormat && mbDeleteCondFormat)
    {
        sal_uInt32 nIndex = pDoc->AddCondFormat(std::unique_ptr<ScConditionalFormat>(mpCondFormat), nTab );
        mpCondFormat->SetKey(nIndex);
        mpCondFormat->SetRange(aRangeList);

        pDoc->AddCondFormatData( aRangeList, nTab, nIndex );
        mbDeleteCondFormat = false;
    }

}

void XMLTableStyleContext::FillPropertySet(
    const uno::Reference< XPropertySet > & rPropSet )
{
    if (!IsDefaultStyle())
    {
        if (GetFamily() == XmlStyleFamily::TABLE_CELL)
        {
            if (!bParentSet)
            {
                AddProperty(CTF_SC_CELLSTYLE, uno::Any(GetImport().GetStyleDisplayName( XmlStyleFamily::TABLE_CELL, GetParentName() )));
                bParentSet = true;
            }
            sal_Int32 nNumFmt = GetNumberFormat();
            if (nNumFmt >= 0)
                AddProperty(CTF_SC_NUMBERFORMAT, uno::Any(nNumFmt));
        }
        else if (GetFamily() == XmlStyleFamily::TABLE_TABLE)
        {
            if (!sPageStyle.isEmpty())
                AddProperty(CTF_SC_MASTERPAGENAME, uno::Any(GetImport().GetStyleDisplayName( XmlStyleFamily::MASTER_PAGE, sPageStyle )));
        }
    }
    XMLPropStyleContext::FillPropertySet(rPropSet);
}

void XMLTableStyleContext::SetDefaults()
{
    if ((GetFamily() == XmlStyleFamily::TABLE_CELL) && GetScImport().GetScModel())
    {
        rtl::Reference<ScModelObj> xMultiServiceFactory(GetScImport().GetScModel());
        if (xMultiServiceFactory.is())
        {
            uno::Reference <beans::XPropertySet> xProperties(xMultiServiceFactory->createInstance(u"com.sun.star.sheet.Defaults"_ustr), uno::UNO_QUERY);
            if (xProperties.is())
                FillPropertySet(xProperties);
        }
    }
}

void XMLTableStyleContext::AddProperty(const sal_Int16 nContextID, const uno::Any& rValue)
{
    XMLPropertyState* property = FindProperty(nContextID);
    if (property)
        property->mnIndex = -1; // #i46996# remove old property, so it isn't double
    sal_Int32 nIndex(pStyles->GetIndex(nContextID));
    OSL_ENSURE(nIndex != -1, "Property not found in Map");
    XMLPropertyState aPropState(nIndex, rValue);
    GetProperties().push_back(aPropState); // has to be inserted in a sort order later
}

XMLPropertyState* XMLTableStyleContext::FindProperty(const sal_Int16 nContextID)
{
    XMLPropertyState* pRet = nullptr;
    rtl::Reference < XMLPropertySetMapper > xPrMap;
    SvXMLImportPropertyMapper* pImpPrMap =
        pStyles->GetImportPropertyMapper( GetFamily() );
    OSL_ENSURE( pImpPrMap, "There is the import prop mapper" );
    if( pImpPrMap )
        xPrMap = pImpPrMap->getPropertySetMapper();
    if( xPrMap.is() )
    {
        auto aIter = std::find_if(GetProperties().begin(), GetProperties().end(),
            [&xPrMap, &nContextID](const XMLPropertyState& rProp) {
                return rProp.mnIndex != -1 && xPrMap->GetEntryContextId(rProp.mnIndex) == nContextID;
            });
        if (aIter != GetProperties().end())
            pRet = &(*aIter);
    }
    return pRet;
}

sal_Int32 XMLTableStyleContext::GetNumberFormat()
{
    if (nNumberFormat < 0 && !sDataStyleName.isEmpty())
    {
        const SvXMLNumFormatContext* pStyle = static_cast<const SvXMLNumFormatContext*>(
            pStyles->FindStyleChildContext(XmlStyleFamily::DATA_STYLE, sDataStyleName, true));

        if (!pStyle)
        {
            XMLTableStylesContext* pMyStyles = static_cast<XMLTableStylesContext*>(GetScImport().GetStyles());
            if (pMyStyles)
                pStyle = static_cast<const SvXMLNumFormatContext*>(
                    pMyStyles->FindStyleChildContext(XmlStyleFamily::DATA_STYLE, sDataStyleName, true));
            else
            {
                OSL_FAIL("not possible to get style");
            }
        }
        if (pStyle)
            nNumberFormat = const_cast<SvXMLNumFormatContext*>(pStyle)->GetKey();
    }
    return nNumberFormat;
}

SvXMLStyleContext *XMLTableStylesContext::CreateStyleStyleChildContext(
        XmlStyleFamily nFamily, sal_Int32 nElement,
        const uno::Reference< xml::sax::XFastAttributeList > & xAttrList )
{
    SvXMLStyleContext *pStyle;
    // use own wrapper for text and paragraph, to record style usage
    if (nFamily == XmlStyleFamily::TEXT_PARAGRAPH || nFamily == XmlStyleFamily::TEXT_TEXT)
        pStyle = new  ScCellTextStyleContext( GetImport(),*this, nFamily );
    else if (nFamily == XmlStyleFamily::SD_GRAPHICS_ID)
        pStyle = new ScShapeStyleContext( GetImport(), *this, nFamily );
    else
        pStyle = SvXMLStylesContext::CreateStyleStyleChildContext(
                    nFamily, nElement, xAttrList );

    if (!pStyle)
    {
        switch( nFamily )
        {
        case XmlStyleFamily::TABLE_CELL:
        case XmlStyleFamily::TABLE_COLUMN:
        case XmlStyleFamily::TABLE_ROW:
        case XmlStyleFamily::TABLE_TABLE:
            pStyle = new XMLTableStyleContext( GetScImport(), *this, nFamily );
            break;
        default: break;
        }
    }

    return pStyle;
}

SvXMLStyleContext *XMLTableStylesContext::CreateDefaultStyleStyleChildContext(
        XmlStyleFamily nFamily, sal_Int32 nElement,
        const uno::Reference< xml::sax::XFastAttributeList > & xAttrList )
{
    SvXMLStyleContext *pStyle(SvXMLStylesContext::CreateDefaultStyleStyleChildContext( nFamily, nElement,
                                                            xAttrList ));
    if (!pStyle)
    {
        switch( nFamily )
        {
            case XmlStyleFamily::TABLE_CELL:
                pStyle = new XMLTableStyleContext( GetScImport(), *this, nFamily, true);
            break;
            case XmlStyleFamily::SD_GRAPHICS_ID:
                pStyle = new XMLGraphicsDefaultStyle( GetScImport(), *this);
            break;
            default: break;
        }
    }

    return pStyle;
}

constexpr OUStringLiteral gsCellStyleServiceName(u"com.sun.star.style.CellStyle");
constexpr OUStringLiteral gsGraphicStyleServiceName(u"com.sun.star.style.GraphicStyle");

XMLTableStylesContext::XMLTableStylesContext( SvXMLImport& rImport,
        const bool bTempAutoStyles )
    : SvXMLStylesContext( rImport )
    , nNumberFormatIndex(-1)
    , nConditionalFormatIndex(-1)
    , nCellStyleIndex(-1)
    , nMasterPageNameIndex(-1)
    , bAutoStyles(bTempAutoStyles)
{
}

XMLTableStylesContext::~XMLTableStylesContext()
{
}

void XMLTableStylesContext::endFastElement(sal_Int32 )
{
    if (bAutoStyles)
        GetImport().GetTextImport()->SetAutoStyles( this );
    else
        GetScImport().InsertStyles();
}

SvXMLImportPropertyMapper*
    XMLTableStylesContext::GetImportPropertyMapper(
                    XmlStyleFamily nFamily ) const
{
    SvXMLImportPropertyMapper* pMapper(SvXMLStylesContext::GetImportPropertyMapper(nFamily));

    if (!pMapper)
    {
        switch( nFamily )
        {
            case XmlStyleFamily::TABLE_CELL:
            {
                if( !xCellImpPropMapper )
                {
                    const_cast<XMLTableStylesContext *>(this)->xCellImpPropMapper =
                        std::make_unique<ScXMLCellImportPropertyMapper>( GetScImport().GetCellStylesPropertySetMapper(), const_cast<SvXMLImport&>(GetImport()) );
                    xCellImpPropMapper->ChainImportMapper(XMLTextImportHelper::CreateParaExtPropMapper(const_cast<SvXMLImport&>(GetImport())));
                }
                pMapper = xCellImpPropMapper.get();
            }
            break;
            case XmlStyleFamily::TABLE_COLUMN:
            {
                if( !xColumnImpPropMapper )
                    const_cast<XMLTableStylesContext *>(this)->xColumnImpPropMapper =
                        std::make_unique<SvXMLImportPropertyMapper>( GetScImport().GetColumnStylesPropertySetMapper(), const_cast<SvXMLImport&>(GetImport()) );
                pMapper = xColumnImpPropMapper.get();
            }
             break;
            case XmlStyleFamily::TABLE_ROW:
            {
                if( !xRowImpPropMapper )
                    const_cast<XMLTableStylesContext *>(this)->xRowImpPropMapper =
                        std::make_unique<ScXMLRowImportPropertyMapper>( GetScImport().GetRowStylesPropertySetMapper(), const_cast<SvXMLImport&>(GetImport()) );
                pMapper = xRowImpPropMapper.get();
            }
             break;
            case XmlStyleFamily::TABLE_TABLE:
            {
                if( !xTableImpPropMapper )
                    const_cast<XMLTableStylesContext *>(this)->xTableImpPropMapper =
                        std::make_unique<SvXMLImportPropertyMapper>( GetScImport().GetTableStylesPropertySetMapper(), const_cast<SvXMLImport&>(GetImport()) );
                pMapper = xTableImpPropMapper.get();
            }
             break;
            default: break;
        }
    }

    return pMapper;
}

uno::Reference < XNameContainer >
        XMLTableStylesContext::GetStylesContainer( XmlStyleFamily nFamily ) const
{
    uno::Reference < XNameContainer > xStyles(SvXMLStylesContext::GetStylesContainer(nFamily));
    if (!xStyles.is())
    {
        OUString sName;
        switch( nFamily )
        {
            case XmlStyleFamily::TABLE_TABLE:
            {
                if( xTableStyles.is() )
                    xStyles.set(xTableStyles);
                else
                    sName = "TableStyles";
            }
            break;
            case XmlStyleFamily::TABLE_CELL:
            {
                if( xCellStyles.is() )
                    xStyles.set(xCellStyles);
                else
                    sName = "CellStyles";
            }
            break;
            case XmlStyleFamily::TABLE_COLUMN:
            {
                if( xColumnStyles.is() )
                    xStyles.set(xColumnStyles);
                else
                    sName = "ColumnStyles";
            }
            break;
            case XmlStyleFamily::TABLE_ROW:
            {
                if( xRowStyles.is() )
                    xStyles.set(xRowStyles);
                else
                    sName = "RowStyles";
            }
            break;
            case XmlStyleFamily::SD_GRAPHICS_ID:
            {
                if( xGraphicStyles.is() )
                    xStyles.set(xGraphicStyles);
                else
                    sName = "GraphicStyles";
            }
            break;
            default: break;
        }
        if( !xStyles.is() && !sName.isEmpty() && GetScImport().GetModel().is() )
        {
            ScModelObj* xFamiliesSupp( GetScImport().GetScModel() );
            uno::Reference< XNameAccess > xFamilies(xFamiliesSupp->getStyleFamilies());

            try
            {
                xStyles.set(xFamilies->getByName( sName ), uno::UNO_QUERY);
            }
            catch ( uno::Exception& )
            {
                // #i97680# Named table/column/row styles aren't supported, getByName will throw an exception.
                // For better interoperability, these styles should then be handled as automatic styles.
                // For now, NULL is returned (and the style is ignored).
            }
            switch( nFamily )
            {
            case XmlStyleFamily::TABLE_TABLE:
                const_cast<XMLTableStylesContext *>(this)->xTableStyles.set(xStyles);
                break;
            case XmlStyleFamily::TABLE_CELL:
                const_cast<XMLTableStylesContext *>(this)->xCellStyles.set(xStyles);
                break;
            case XmlStyleFamily::TABLE_COLUMN:
                const_cast<XMLTableStylesContext *>(this)->xColumnStyles.set(xStyles);
                break;
            case XmlStyleFamily::TABLE_ROW:
                const_cast<XMLTableStylesContext *>(this)->xRowStyles.set(xStyles);
                break;
            case XmlStyleFamily::SD_GRAPHICS_ID:
                const_cast<XMLTableStylesContext *>(this)->xGraphicStyles.set(xStyles);
                break;
            default: break;
            }
        }
    }

    return xStyles;
}

OUString XMLTableStylesContext::GetServiceName( XmlStyleFamily nFamily ) const
{
    OUString sServiceName(SvXMLStylesContext::GetServiceName(nFamily));
    if (sServiceName.isEmpty())
    {
        switch( nFamily )
        {
        case XmlStyleFamily::TABLE_COLUMN:
            sServiceName = XML_STYLE_FAMILY_TABLE_COLUMN_STYLES_NAME;
            break;
        case XmlStyleFamily::TABLE_ROW:
            sServiceName = XML_STYLE_FAMILY_TABLE_ROW_STYLES_NAME;
            break;
        case XmlStyleFamily::TABLE_CELL:
            sServiceName = gsCellStyleServiceName;
            break;
        case XmlStyleFamily::TABLE_TABLE:
            sServiceName = XML_STYLE_FAMILY_TABLE_TABLE_STYLES_NAME;
            break;
        case XmlStyleFamily::SD_GRAPHICS_ID:
            sServiceName = gsGraphicStyleServiceName;
            break;
        default: break;
        }
    }
    return sServiceName;
}

sal_Int32 XMLTableStylesContext::GetIndex(const sal_Int16 nContextID)
{
    if (nContextID == CTF_SC_CELLSTYLE)
    {
        if (nCellStyleIndex == -1)
            nCellStyleIndex =
                GetImportPropertyMapper(XmlStyleFamily::TABLE_CELL)->getPropertySetMapper()->FindEntryIndex(nContextID);
        return nCellStyleIndex;
    }
    else if (nContextID == CTF_SC_NUMBERFORMAT)
    {
        if (nNumberFormatIndex == -1)
            nNumberFormatIndex =
                GetImportPropertyMapper(XmlStyleFamily::TABLE_CELL)->getPropertySetMapper()->FindEntryIndex(nContextID);
        return nNumberFormatIndex;
    }
    else if (nContextID == CTF_SC_IMPORT_MAP)
    {
        if (nConditionalFormatIndex == -1)
            nConditionalFormatIndex =
                GetImportPropertyMapper(XmlStyleFamily::TABLE_CELL)->getPropertySetMapper()->FindEntryIndex(nContextID);
        return nConditionalFormatIndex;
    }
    else if (nContextID == CTF_SC_MASTERPAGENAME)
    {
        if (nMasterPageNameIndex == -1)
                nMasterPageNameIndex =
                GetImportPropertyMapper(XmlStyleFamily::TABLE_TABLE)->getPropertySetMapper()->FindEntryIndex(nContextID);
        return nMasterPageNameIndex;
    }
    else
        return -1;
}


bool ScXMLMasterStylesContext::InsertStyleFamily( XmlStyleFamily ) const
{
    return true;
}

ScXMLMasterStylesContext::ScXMLMasterStylesContext( SvXMLImport& rImport ) :
    SvXMLStylesContext( rImport )
{
}

ScXMLMasterStylesContext::~ScXMLMasterStylesContext()
{
}

SvXMLStyleContext *ScXMLMasterStylesContext::CreateStyleChildContext(
        sal_Int32 nElement,
        const uno::Reference< XFastAttributeList > & xAttrList )
{
    SvXMLStyleContext *pContext(nullptr);

    if( nElement == XML_ELEMENT(STYLE, XML_MASTER_PAGE) &&
         InsertStyleFamily( XmlStyleFamily::MASTER_PAGE ) )
        pContext = new ScMasterPageContext(
                        GetImport(), nElement, xAttrList,
                        !GetImport().GetTextImport()->IsInsertMode() );

    // any other style will be ignored here!

    return pContext;
}

SvXMLStyleContext *ScXMLMasterStylesContext::CreateStyleStyleChildContext(
        XmlStyleFamily /* nFamily */,
        sal_Int32 /* nElement */,
        const uno::Reference< XFastAttributeList > & /* xAttrList */ )
{
    return nullptr;
}

void ScXMLMasterStylesContext::endFastElement(sal_Int32 )
{
    FinishStyles(true);
}


ScMasterPageContext::ScMasterPageContext( SvXMLImport& rImport,
        sal_Int32 nElement,
        const uno::Reference< XFastAttributeList > & xAttrList,
        bool bOverwrite ) :
    XMLTextMasterPageContext( rImport, nElement, xAttrList, bOverwrite ),
    bContainsRightHeader(false),
    bContainsRightFooter(false)
{
}

ScMasterPageContext::~ScMasterPageContext()
{
}

SvXMLImportContext *ScMasterPageContext::CreateHeaderFooterContext(
            sal_Int32 nElement,
            const css::uno::Reference< css::xml::sax::XFastAttributeList > & xAttrList,
            const bool bFooter,
            const bool bLeft,
            const bool bFirst )
{
    if (!bLeft)
    {
        if (bFooter)
            bContainsRightFooter = true;
        else
            bContainsRightHeader = true;
    }
    if (!xPropSet.is())
        xPropSet.set(GetStyle(), UNO_QUERY );
    return new XMLTableHeaderFooterContext( GetImport(),
                                                nElement,
                                                xAttrList,
                                                xPropSet,
                                                bFooter, bLeft, bFirst );
}

void ScMasterPageContext::ClearContent(const OUString& rContent)
{
    if (!xPropSet.is())
        xPropSet.set(GetStyle(), UNO_QUERY );

    if (xPropSet.is())
    {
        uno::Reference < sheet::XHeaderFooterContent > xHeaderFooterContent(xPropSet->getPropertyValue( rContent ), uno::UNO_QUERY);
        if (xHeaderFooterContent.is())
        {
            xHeaderFooterContent->getLeftText()->setString(u""_ustr);
            xHeaderFooterContent->getCenterText()->setString(u""_ustr);
            xHeaderFooterContent->getRightText()->setString(u""_ustr);
            xPropSet->setPropertyValue( rContent, uno::Any(xHeaderFooterContent) );
        }
    }
}

void ScMasterPageContext::Finish( bool bOverwrite )
{
    XMLTextMasterPageContext::Finish(bOverwrite);
    if (!bContainsRightFooter)
        ClearContent(SC_UNO_PAGE_RIGHTFTRCON);
    if (!bContainsRightHeader)
        ClearContent(SC_UNO_PAGE_RIGHTHDRCON);
}

ScCellTextStyleContext::ScCellTextStyleContext( SvXMLImport& rImport,
            SvXMLStylesContext& rStyles, XmlStyleFamily nFamily ) :
    XMLTextStyleContext( rImport, rStyles, nFamily, false/*bDefaultStyle*/ ),
    nLastSheet(-1)
{
}

ScCellTextStyleContext::~ScCellTextStyleContext()
{
}

void ScCellTextStyleContext::FillPropertySet( const uno::Reference<beans::XPropertySet>& xPropSet )
{
    XMLTextStyleContext::FillPropertySet( xPropSet );

    ScXMLImport& rXMLImport = GetScImport();

    ScCellTextCursor* pCellImp = comphelper::getFromUnoTunnel<ScCellTextCursor>( xPropSet );
    if (pCellImp)
    {
        ScAddress aPos = pCellImp->GetCellObj().GetPosition();
        if ( aPos.Tab() != nLastSheet )
        {
            ESelection aSel = pCellImp->GetSelection();

            ScSheetSaveData* pSheetData = GetScImport().GetScModel()->GetSheetSaveData();
            pSheetData->AddTextStyle( GetName(), aPos, aSel );

            nLastSheet = aPos.Tab();
        }
    }
    else if ( rXMLImport.GetTables().GetCurrentSheet() != nLastSheet )
    {
        ScDrawTextCursor* pDrawImp = comphelper::getFromUnoTunnel<ScDrawTextCursor>( xPropSet );
        if (pDrawImp)
        {
            XMLTableShapeImportHelper* pTableShapeImport = static_cast<XMLTableShapeImportHelper*>(GetScImport().GetShapeImport().get());
            ScXMLAnnotationContext* pAnnotationContext = pTableShapeImport->GetAnnotationContext();
            if (pAnnotationContext)
            {
                pAnnotationContext->AddContentStyle( GetFamily(), GetName(), pDrawImp->GetSelection() );
                nLastSheet = rXMLImport.GetTables().GetCurrentSheet();
            }
        }

        // if it's a different shape, BlockSheet is called from XMLTableShapeImportHelper::finishShape
        // formatted text in page headers/footers can be ignored
    }
}

void ScShapeStyleContext::Finish(bool bOverwrite)
{
    // set parent styles
    XMLPropStyleContext::Finish(bOverwrite);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
