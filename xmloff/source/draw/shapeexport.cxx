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

#include <config_wasm_strip.h>

#include <basegfx/matrix/b2dhommatrix.hxx>
#include <basegfx/matrix/b3dhommatrix.hxx>
#include <basegfx/polygon/b2dpolypolygon.hxx>
#include <basegfx/polygon/b2dpolypolygontools.hxx>
#include <basegfx/polygon/b2dpolygontools.hxx>
#include <basegfx/polygon/b3dpolypolygon.hxx>
#include <basegfx/polygon/b3dpolypolygontools.hxx>
#include <basegfx/tuple/b2dtuple.hxx>
#include <basegfx/vector/b3dvector.hxx>

#include <com/sun/star/beans/XPropertyState.hpp>
#include <com/sun/star/beans/PropertyValues.hpp>
#include <com/sun/star/container/XChild.hpp>
#include <com/sun/star/container/XEnumerationAccess.hpp>
#include <com/sun/star/container/XIdentifierAccess.hpp>
#include <com/sun/star/container/XNamed.hpp>
#include <com/sun/star/document/XEventsSupplier.hpp>
#include <com/sun/star/drawing/CameraGeometry.hpp>
#include <com/sun/star/drawing/CircleKind.hpp>
#include <com/sun/star/drawing/ConnectorType.hpp>
#include <com/sun/star/drawing/Direction3D.hpp>
#include <com/sun/star/drawing/EscapeDirection.hpp>
#include <com/sun/star/drawing/EnhancedCustomShapeAdjustmentValue.hpp>
#include <com/sun/star/drawing/EnhancedCustomShapeGluePointType.hpp>
#include <com/sun/star/drawing/EnhancedCustomShapeParameterPair.hpp>
#include <com/sun/star/drawing/EnhancedCustomShapeParameterType.hpp>
#include <com/sun/star/drawing/EnhancedCustomShapeMetalType.hpp>
#include <com/sun/star/drawing/EnhancedCustomShapeSegment.hpp>
#include <com/sun/star/drawing/EnhancedCustomShapeSegmentCommand.hpp>
#include <com/sun/star/drawing/EnhancedCustomShapeTextFrame.hpp>
#include <com/sun/star/drawing/EnhancedCustomShapeTextPathMode.hpp>
#include <com/sun/star/drawing/GluePoint2.hpp>
#include <com/sun/star/drawing/HomogenMatrix.hpp>
#include <com/sun/star/drawing/HomogenMatrix3.hpp>
#include <com/sun/star/drawing/PolyPolygonBezierCoords.hpp>
#include <com/sun/star/drawing/PolyPolygonShape3D.hpp>
#include <com/sun/star/drawing/Position3D.hpp>
#include <com/sun/star/drawing/ProjectionMode.hpp>
#include <com/sun/star/drawing/ShadeMode.hpp>
#include <com/sun/star/drawing/XControlShape.hpp>
#include <com/sun/star/drawing/XCustomShapeEngine.hpp>
#include <com/sun/star/drawing/XGluePointsSupplier.hpp>
#include <com/sun/star/drawing/BarCode.hpp>
#include <com/sun/star/drawing/BarCodeErrorCorrection.hpp>
#include <com/sun/star/drawing/XShapes3.hpp>
#include <com/sun/star/embed/ElementModes.hpp>
#include <com/sun/star/embed/XStorage.hpp>
#include <com/sun/star/embed/XTransactedObject.hpp>
#include <com/sun/star/graphic/XGraphic.hpp>
#include <com/sun/star/graphic/GraphicProvider.hpp>
#include <com/sun/star/graphic/XGraphicProvider.hpp>
#include <com/sun/star/io/XSeekableInputStream.hpp>
#include <com/sun/star/io/XStream.hpp>
#include <com/sun/star/lang/ServiceNotRegisteredException.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/media/ZoomLevel.hpp>
#include <com/sun/star/presentation/AnimationSpeed.hpp>
#include <com/sun/star/presentation/ClickAction.hpp>
#include <com/sun/star/style/XStyle.hpp>
#include <com/sun/star/table/XColumnRowRange.hpp>
#include <com/sun/star/text/WritingMode2.hpp>
#include <com/sun/star/text/XText.hpp>

#include <comphelper/classids.hxx>
#include <comphelper/memorystream.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/propertyvalue.hxx>
#include <comphelper/sequenceashashmap.hxx>
#include <comphelper/storagehelper.hxx>
#include <officecfg/Office/Common.hxx>

#include <o3tl/any.hxx>
#include <o3tl/typed_flags_set.hxx>
#include <o3tl/string_view.hxx>

#include <rtl/math.hxx>
#include <rtl/ustrbuf.hxx>
#include <rtl/ustring.hxx>
#include <sal/log.hxx>

#include <sax/tools/converter.hxx>

#include <tools/debug.hxx>
#include <tools/globname.hxx>
#include <tools/helpers.hxx>
#include <comphelper/diagnose_ex.hxx>
#include <vcl/graph.hxx>

#include <xmloff/contextid.hxx>
#include <xmloff/families.hxx>
#include <xmloff/namespacemap.hxx>
#include <xmloff/shapeexport.hxx>
#include <xmloff/unointerfacetouniqueidentifiermapper.hxx>
#include <xmloff/xmlexp.hxx>
#include <xmloff/xmlnamespace.hxx>
#include <xmloff/xmltoken.hxx>
#include <xmloff/xmluconv.hxx>
#include <xmloff/table/XMLTableExport.hxx>
#include <xmloff/ProgressBarHelper.hxx>

#include <anim.hxx>
#include <EnhancedCustomShapeToken.hxx>
#include "sdpropls.hxx"
#include <xexptran.hxx>
#include "ximpshap.hxx"
#include <XMLBase64Export.hxx>
#include <XMLImageMapExport.hxx>
#include <memory>
#include <algorithm>

using namespace ::com::sun::star;
using namespace ::xmloff::EnhancedCustomShapeToken;
using namespace ::xmloff::token;

constexpr OUStringLiteral XML_EMBEDDEDOBJECTGRAPHIC_URL_BASE = u"vnd.sun.star.GraphicObject:";

namespace {

bool supportsText(XmlShapeType eShapeType)
{
        return eShapeType != XmlShapeType::PresChartShape &&
        eShapeType != XmlShapeType::PresOLE2Shape &&
        eShapeType != XmlShapeType::DrawSheetShape &&
        eShapeType != XmlShapeType::PresSheetShape &&
        eShapeType != XmlShapeType::Draw3DSceneObject &&
        eShapeType != XmlShapeType::Draw3DCubeObject &&
        eShapeType != XmlShapeType::Draw3DSphereObject &&
        eShapeType != XmlShapeType::Draw3DLatheObject &&
        eShapeType != XmlShapeType::Draw3DExtrudeObject &&
        eShapeType != XmlShapeType::DrawPageShape &&
        eShapeType != XmlShapeType::PresPageShape &&
        eShapeType != XmlShapeType::DrawGroupShape;

}

}

constexpr OUString gsZIndex( u"ZOrder"_ustr );
constexpr OUStringLiteral gsPrintable( u"Printable" );
constexpr OUStringLiteral gsVisible( u"Visible" );
constexpr OUString gsModel( u"Model"_ustr );
constexpr OUStringLiteral gsStartShape( u"StartShape" );
constexpr OUStringLiteral gsEndShape( u"EndShape" );
constexpr OUString gsOnClick( u"OnClick"_ustr );
constexpr OUStringLiteral gsEventType( u"EventType" );
constexpr OUStringLiteral gsPresentation( u"Presentation" );
constexpr OUStringLiteral gsMacroName( u"MacroName" );
constexpr OUString gsScript( u"Script"_ustr );
constexpr OUStringLiteral gsLibrary( u"Library" );
constexpr OUStringLiteral gsClickAction( u"ClickAction" );
constexpr OUString gsBookmark( u"Bookmark"_ustr );
constexpr OUStringLiteral gsEffect( u"Effect" );
constexpr OUStringLiteral gsPlayFull( u"PlayFull" );
constexpr OUStringLiteral gsVerb( u"Verb" );
constexpr OUStringLiteral gsSoundURL( u"SoundURL" );
constexpr OUStringLiteral gsSpeed( u"Speed" );
constexpr OUStringLiteral gsStarBasic( u"StarBasic" );
constexpr OUStringLiteral gsHyperlink( u"Hyperlink" );

XMLShapeExport::XMLShapeExport(SvXMLExport& rExp,
                                SvXMLExportPropertyMapper *pExtMapper )
:   mrExport( rExp ),
    maCurrentShapesIter(maShapesInfos.end()),
    mbExportLayer( false ),
    // #88546# init to sal_False
    mbHandleProgressBar( false )
{
    // construct PropertySetMapper
    mxPropertySetMapper = CreateShapePropMapper( mrExport );
    if( pExtMapper )
    {
        rtl::Reference < SvXMLExportPropertyMapper > xExtMapper( pExtMapper );
        mxPropertySetMapper->ChainExportMapper( xExtMapper );
    }

/*
    // chain text attributes
    xPropertySetMapper->ChainExportMapper(XMLTextParagraphExport::CreateParaExtPropMapper(rExp));
*/

    mrExport.GetAutoStylePool()->AddFamily(
        XmlStyleFamily::SD_GRAPHICS_ID,
        XML_STYLE_FAMILY_SD_GRAPHICS_NAME,
        GetPropertySetMapper(),
        XML_STYLE_FAMILY_SD_GRAPHICS_PREFIX);
    mrExport.GetAutoStylePool()->AddFamily(
        XmlStyleFamily::SD_PRESENTATION_ID,
        XML_STYLE_FAMILY_SD_PRESENTATION_NAME,
        GetPropertySetMapper(),
        XML_STYLE_FAMILY_SD_PRESENTATION_PREFIX);

    // create table export helper and let him add his families in time
    GetShapeTableExport();
}

XMLShapeExport::~XMLShapeExport()
{
}

// sj: replacing CustomShapes with standard objects that are also supported in OpenOffice.org format
uno::Reference< drawing::XShape > XMLShapeExport::checkForCustomShapeReplacement( const uno::Reference< drawing::XShape >& xShape )
{
    uno::Reference< drawing::XShape > xCustomShapeReplacement;

    if( !( GetExport().getExportFlags() & SvXMLExportFlags::OASIS ) )
    {
        OUString aType( xShape->getShapeType() );
        if( aType == "com.sun.star.drawing.CustomShape" )
        {
            uno::Reference< beans::XPropertySet > xSet( xShape, uno::UNO_QUERY );
            if( xSet.is() )
            {
                OUString aEngine;
                xSet->getPropertyValue(u"CustomShapeEngine"_ustr) >>= aEngine;
                if ( aEngine.isEmpty() )
                {
                    aEngine = "com.sun.star.drawing.EnhancedCustomShapeEngine";
                }
                const uno::Reference< uno::XComponentContext >& xContext( ::comphelper::getProcessComponentContext() );

                if ( !aEngine.isEmpty() )
                {
                    uno::Sequence< beans::PropertyValue > aPropValues{
                        comphelper::makePropertyValue(u"CustomShape"_ustr, xShape),
                        comphelper::makePropertyValue(u"ForceGroupWithText"_ustr, true)
                    };
                    uno::Sequence< uno::Any > aArgument = { uno::Any(aPropValues) };
                    uno::Reference< uno::XInterface > xInterface(
                        xContext->getServiceManager()->createInstanceWithArgumentsAndContext(aEngine, aArgument, xContext) );
                    if ( xInterface.is() )
                    {
                        uno::Reference< drawing::XCustomShapeEngine > xCustomShapeEngine(
                            uno::Reference< drawing::XCustomShapeEngine >( xInterface, uno::UNO_QUERY ) );
                        if ( xCustomShapeEngine.is() )
                            xCustomShapeReplacement = xCustomShapeEngine->render();
                    }
                }
            }
        }
    }
    return xCustomShapeReplacement;
}

// This method collects all automatic styles for the given XShape
void XMLShapeExport::collectShapeAutoStyles(const uno::Reference< drawing::XShape >& xShape )
{
    if( maCurrentShapesIter == maShapesInfos.end() )
    {
        OSL_FAIL( "XMLShapeExport::collectShapeAutoStyles(): no call to seekShapes()!" );
        return;
    }
    sal_Int32 nZIndex = 0;
    uno::Reference< beans::XPropertySet > xPropSet(xShape, uno::UNO_QUERY);
    if( xPropSet.is() )
        xPropSet->getPropertyValue(gsZIndex) >>= nZIndex;

    ImplXMLShapeExportInfoVector& aShapeInfoVector = (*maCurrentShapesIter).second;

    if( static_cast<sal_Int32>(aShapeInfoVector.size()) <= nZIndex )
    {
        OSL_FAIL( "XMLShapeExport::collectShapeAutoStyles(): no shape info allocated for a given shape" );
        return;
    }

    ImplXMLShapeExportInfo& aShapeInfo = aShapeInfoVector[nZIndex];

    uno::Reference< drawing::XShape > xCustomShapeReplacement = checkForCustomShapeReplacement( xShape );
    if ( xCustomShapeReplacement.is() )
        aShapeInfo.xCustomShapeReplacement = std::move(xCustomShapeReplacement);

    // first compute the shapes type
    ImpCalcShapeType(xShape, aShapeInfo.meShapeType);

    // #i118485# enabled XmlShapeType::DrawChartShape and XmlShapeType::DrawOLE2Shape
    // to have text
    const bool bObjSupportsText =
        supportsText(aShapeInfo.meShapeType);

    const bool bObjSupportsStyle =
        aShapeInfo.meShapeType != XmlShapeType::DrawGroupShape;

    bool bIsEmptyPresObj = false;

    if ( aShapeInfo.xCustomShapeReplacement.is() )
        xPropSet.clear();

    // prep text styles
    if( xPropSet.is() && bObjSupportsText )
    {
        uno::Reference< text::XText > xText(xShape, uno::UNO_QUERY);
        if (xText.is())
        {
            try
            {
                // tdf#153161: it seems that the call to xText->getText flushes the changes
                // for some objects, that otherwise fail to get exported correctly. Maybe at some
                // point it would make sense to find a better place for more targeted flush.
                xText = xText->getText();
            }
            catch (uno::RuntimeException const&)
            {
                // E.g., SwXTextFrame that contains only a table will throw; this is not an error
            }

            uno::Reference< beans::XPropertySetInfo > xPropSetInfo( xPropSet->getPropertySetInfo() );

            if( xPropSetInfo.is() && xPropSetInfo->hasPropertyByName(u"IsEmptyPresentationObject"_ustr) )
            {
                uno::Any aAny = xPropSet->getPropertyValue(u"IsEmptyPresentationObject"_ustr);
                aAny >>= bIsEmptyPresObj;
            }

            if(!bIsEmptyPresObj)
            {
                GetExport().GetTextParagraphExport()->collectTextAutoStyles( xText );
            }
        }
    }

    // compute the shape parent style
    if( xPropSet.is() )
    {
        uno::Reference< beans::XPropertySetInfo > xPropertySetInfo( xPropSet->getPropertySetInfo() );

        OUString aParentName;
        uno::Reference< style::XStyle > xStyle;

        if( bObjSupportsStyle )
        {
            if( xPropertySetInfo.is() && xPropertySetInfo->hasPropertyByName(u"Style"_ustr) )
                xPropSet->getPropertyValue(u"Style"_ustr) >>= xStyle;

            if(xStyle.is())
            {
                // get family ID
                uno::Reference< beans::XPropertySet > xStylePropSet(xStyle, uno::UNO_QUERY);
                SAL_WARN_IF( !xStylePropSet.is(), "xmloff", "style without a XPropertySet?" );
                try
                {
                    if(xStylePropSet.is())
                    {
                        OUString aFamilyName;
                        xStylePropSet->getPropertyValue(u"Family"_ustr) >>= aFamilyName;
                        if( !aFamilyName.isEmpty() && aFamilyName != "graphics" )
                            aShapeInfo.mnFamily = XmlStyleFamily::SD_PRESENTATION_ID;
                    }
                }
                catch(const beans::UnknownPropertyException&)
                {
                    // Ignored.
                    SAL_WARN( "xmloff",
                        "XMLShapeExport::collectShapeAutoStyles: style has no 'Family' property");
                }

                // get parent-style name
                if(XmlStyleFamily::SD_PRESENTATION_ID == aShapeInfo.mnFamily)
                {
                    aParentName = msPresentationStylePrefix;
                }

                aParentName += xStyle->getName();
            }
        }

        if (aParentName.isEmpty() && xPropertySetInfo->hasPropertyByName(u"TextBox"_ustr) && xPropSet->getPropertyValue(u"TextBox"_ustr).hasValue() && xPropSet->getPropertyValue(u"TextBox"_ustr).get<bool>())
        {
            // Shapes with a Writer TextBox always have a parent style.
            // If there would be none, then assign the default one.
            aParentName = "Frame";
        }

        // filter propset
        std::vector< XMLPropertyState > aPropStates;

        sal_Int32 nCount = 0;
        if( !bIsEmptyPresObj || (aShapeInfo.meShapeType != XmlShapeType::PresPageShape) )
        {
            aPropStates = GetPropertySetMapper()->Filter(mrExport, xPropSet);

            if (XmlShapeType::DrawControlShape == aShapeInfo.meShapeType)
            {
                // for control shapes, we additionally need the number format style (if any)
                uno::Reference< drawing::XControlShape > xControl(xShape, uno::UNO_QUERY);
                DBG_ASSERT(xControl.is(), "XMLShapeExport::collectShapeAutoStyles: ShapeType control, but no XControlShape!");
                if (xControl.is())
                {
                    uno::Reference< beans::XPropertySet > xControlModel(xControl->getControl(), uno::UNO_QUERY);
                    DBG_ASSERT(xControlModel.is(), "XMLShapeExport::collectShapeAutoStyles: no control model on the control shape!");

                    OUString sNumberStyle = mrExport.GetFormExport()->getControlNumberStyle(xControlModel);
                    if (!sNumberStyle.isEmpty())
                    {
                        sal_Int32 nIndex = GetPropertySetMapper()->getPropertySetMapper()->FindEntryIndex(CTF_SD_CONTROL_SHAPE_DATA_STYLE);
                            // TODO : this retrieval of the index could be moved into the ctor, holding the index
                            //          as member, thus saving time.
                        DBG_ASSERT(-1 != nIndex, "XMLShapeExport::collectShapeAutoStyles: could not obtain the index for our context id!");

                        XMLPropertyState aNewState(nIndex, uno::Any(sNumberStyle));
                        aPropStates.push_back(aNewState);
                    }
                }
            }

            nCount = std::count_if(aPropStates.cbegin(), aPropStates.cend(),
                [](const XMLPropertyState& rProp) { return rProp.mnIndex != -1; });
        }

        if(nCount == 0)
        {
            // no hard attributes, use parent style name for export
            aShapeInfo.msStyleName = aParentName;
        }
        else
        {
            // there are filtered properties -> hard attributes
            // try to find this style in AutoStylePool
            aShapeInfo.msStyleName = mrExport.GetAutoStylePool()->Find(aShapeInfo.mnFamily, aParentName, aPropStates);

            if(aShapeInfo.msStyleName.isEmpty())
            {
                // Style did not exist, add it to AutoStalePool
                aShapeInfo.msStyleName = mrExport.GetAutoStylePool()->Add(aShapeInfo.mnFamily, aParentName, std::move(aPropStates));
            }
        }

        // optionally generate auto style for text attributes
        if( (!bIsEmptyPresObj || (aShapeInfo.meShapeType != XmlShapeType::PresPageShape)) && bObjSupportsText )
        {
            aPropStates = GetExport().GetTextParagraphExport()->GetParagraphPropertyMapper()->Filter(mrExport, xPropSet);

            // yet more additionally, we need to care for the ParaAdjust property
            if ( XmlShapeType::DrawControlShape == aShapeInfo.meShapeType )
            {
                uno::Reference< beans::XPropertySetInfo > xPropSetInfo( xPropSet->getPropertySetInfo() );
                uno::Reference< beans::XPropertyState > xPropState( xPropSet, uno::UNO_QUERY );
                if ( xPropSetInfo.is() && xPropState.is() )
                {
                    // this is because:
                    // * if controls shapes have a ParaAdjust property, then this is the Align property of the control model
                    // * control models are allowed to have an Align of "void"
                    // * the Default for control model's Align is TextAlign_LEFT
                    // * defaults for style properties are not written, but we need to write the "left",
                    //   because we need to distinguish this "left" from the case where not align attribute
                    //   is present which means "void"
                    if  (   xPropSetInfo->hasPropertyByName( u"ParaAdjust"_ustr )
                        &&  ( beans::PropertyState_DEFAULT_VALUE == xPropState->getPropertyState( u"ParaAdjust"_ustr ) )
                        )
                    {
                        sal_Int32 nIndex = GetExport().GetTextParagraphExport()->GetParagraphPropertyMapper()->getPropertySetMapper()->FindEntryIndex( CTF_SD_SHAPE_PARA_ADJUST );
                            // TODO : this retrieval of the index should be moved into the ctor, holding the index
                            //          as member, thus saving time.
                        DBG_ASSERT(-1 != nIndex, "XMLShapeExport::collectShapeAutoStyles: could not obtain the index for the ParaAdjust context id!");

                        XMLPropertyState aAlignDefaultState(nIndex, xPropSet->getPropertyValue(u"ParaAdjust"_ustr));

                        aPropStates.push_back( aAlignDefaultState );
                    }
                }
            }

            nCount = std::count_if(aPropStates.cbegin(), aPropStates.cend(),
                [](const XMLPropertyState& rProp) { return rProp.mnIndex != -1; });

            if( nCount )
            {
                aShapeInfo.msTextStyleName = mrExport.GetAutoStylePool()->Find( XmlStyleFamily::TEXT_PARAGRAPH, u""_ustr, aPropStates );
                if(aShapeInfo.msTextStyleName.isEmpty())
                {
                    // Style did not exist, add it to AutoStalePool
                    aShapeInfo.msTextStyleName = mrExport.GetAutoStylePool()->Add(XmlStyleFamily::TEXT_PARAGRAPH, u""_ustr, std::move(aPropStates));
                }
            }
        }
    }

    // prepare animation information if needed
    if( mxAnimationsExporter.is() )
        XMLAnimationsExporter::prepare( xShape );

    // check for special shapes

    switch( aShapeInfo.meShapeType )
    {
        case XmlShapeType::DrawConnectorShape:
        {
            uno::Reference< uno::XInterface > xConnection;

            // create shape ids for export later
            xPropSet->getPropertyValue( gsStartShape ) >>= xConnection;
            if( xConnection.is() )
                mrExport.getInterfaceToIdentifierMapper().registerReference( xConnection );

            xPropSet->getPropertyValue( gsEndShape ) >>= xConnection;
            if( xConnection.is() )
                mrExport.getInterfaceToIdentifierMapper().registerReference( xConnection );
            break;
        }
        case XmlShapeType::PresTableShape:
        case XmlShapeType::DrawTableShape:
        {
            try
            {
                uno::Reference< table::XColumnRowRange > xRange( xPropSet->getPropertyValue( gsModel ), uno::UNO_QUERY_THROW );
                GetShapeTableExport()->collectTableAutoStyles( xRange );
            }
            catch(const uno::Exception&)
            {
                DBG_UNHANDLED_EXCEPTION( "xmloff", "collecting auto styles for a table" );
            }
            break;
        }
        default:
            break;
    }

    // check for shape collections (group shape or 3d scene)
    // and collect contained shapes style infos
    const uno::Reference< drawing::XShape >& xCollection = aShapeInfo.xCustomShapeReplacement.is()
                                                ? aShapeInfo.xCustomShapeReplacement : xShape;
    {
        uno::Reference< drawing::XShapes > xShapes( xCollection, uno::UNO_QUERY );
        if( xShapes.is() )
        {
            collectShapesAutoStyles( xShapes );
        }
    }
}

namespace
{
    class NewTextListsHelper
    {
        public:
            explicit NewTextListsHelper( SvXMLExport& rExp )
                : mrExport( rExp )
            {
                mrExport.GetTextParagraphExport()->PushNewTextListsHelper();
            }

            ~NewTextListsHelper()
            {
                mrExport.GetTextParagraphExport()->PopTextListsHelper();
            }

        private:
            SvXMLExport& mrExport;
    };
}
// This method exports the given XShape
void XMLShapeExport::exportShape(const uno::Reference< drawing::XShape >& xShape,
                                 XMLShapeExportFlags nFeatures /* = SEF_DEFAULT */,
                                 css::awt::Point* pRefPoint /* = NULL */,
                                 comphelper::AttributeList* pAttrList /* = NULL */ )
{
    SAL_INFO("xmloff", xShape->getShapeType());
    if( maCurrentShapesIter == maShapesInfos.end() )
    {
        SAL_WARN( "xmloff", "XMLShapeExport::exportShape(): no auto styles where collected before export" );
        return;
    }
    sal_Int32 nZIndex = 0;
    uno::Reference< beans::XPropertySet > xSet( xShape, uno::UNO_QUERY );
    OUString sHyperlink;
    try
    {
        xSet->getPropertyValue(gsHyperlink) >>= sHyperlink;
    }
    catch (beans::UnknownPropertyException)
    {
    }

    std::unique_ptr< SvXMLElementExport >  pHyperlinkElement;

    // Need to stash the attributes that are pre-loaded for the shape export
    // (otherwise they will become attributes of the draw:a element)
    uno::Reference<xml::sax::XAttributeList> xSaveAttribs(
        new comphelper::AttributeList(GetExport().GetAttrList()));
    GetExport().ClearAttrList();
    if( xSet.is() && (GetExport().GetModelType() == SvtModuleOptions::EFactory::DRAW) )
    {
        // export hyperlinks with <a><shape/></a>. Currently only in draw since draw
        // does not support document events
        try
        {
            presentation::ClickAction eAction = presentation::ClickAction_NONE;
            xSet->getPropertyValue(gsOnClick) >>= eAction;

            if( (eAction == presentation::ClickAction_DOCUMENT) ||
                (eAction == presentation::ClickAction_BOOKMARK) )
            {
                OUString sURL;
                xSet->getPropertyValue(gsBookmark) >>= sURL;

                if( !sURL.isEmpty() )
                {
                    mrExport.AddAttribute( XML_NAMESPACE_XLINK, XML_HREF, sURL );
                    mrExport.AddAttribute( XML_NAMESPACE_XLINK, XML_TYPE, XML_SIMPLE );
                    mrExport.AddAttribute( XML_NAMESPACE_XLINK, XML_SHOW, XML_EMBED );
                    pHyperlinkElement.reset( new SvXMLElementExport(mrExport, XML_NAMESPACE_DRAW, XML_A, true, true) );
                }
            }
        }
        catch(const uno::Exception&)
        {
            TOOLS_WARN_EXCEPTION("xmloff", "XMLShapeExport::exportShape(): exception during hyperlink export");
        }
    }
    else if (xSet.is() && !sHyperlink.isEmpty())
    {
        mrExport.AddAttribute( XML_NAMESPACE_XLINK, XML_HREF, sHyperlink );
        mrExport.AddAttribute( XML_NAMESPACE_XLINK, XML_TYPE, XML_SIMPLE );
        pHyperlinkElement.reset( new SvXMLElementExport(mrExport, XML_NAMESPACE_DRAW, XML_A, true, true) );
    }
    // re-add stashed attributes
    GetExport().AddAttributeList(xSaveAttribs);

    if( xSet.is() )
        xSet->getPropertyValue(gsZIndex) >>= nZIndex;

    ImplXMLShapeExportInfoVector& aShapeInfoVector = (*maCurrentShapesIter).second;

    if( static_cast<sal_Int32>(aShapeInfoVector.size()) <= nZIndex )
    {
        SAL_WARN( "xmloff", "XMLShapeExport::exportShape(): no shape info collected for a given shape" );
        return;
    }

    NewTextListsHelper aNewTextListsHelper( mrExport );

    const ImplXMLShapeExportInfo& aShapeInfo = aShapeInfoVector[nZIndex];

#ifdef DBG_UTIL
    // check if this is the correct ShapesInfo
    uno::Reference< container::XChild > xChild( xShape, uno::UNO_QUERY );
    if( xChild.is() )
    {
        uno::Reference< drawing::XShapes > xParent( xChild->getParent(), uno::UNO_QUERY );
        SAL_WARN_IF( !xParent.is() && xParent.get() == (*maCurrentShapesIter).first.get(), "xmloff", "XMLShapeExport::exportShape(): Wrong call to XMLShapeExport::seekShapes()" );
    }

    // first compute the shapes type
    {
        XmlShapeType eShapeType(XmlShapeType::NotYetSet);
        ImpCalcShapeType(xShape, eShapeType);

        SAL_WARN_IF( eShapeType != aShapeInfo.meShapeType, "xmloff", "exportShape callings do not correspond to collectShapeAutoStyles calls!: " << xShape->getShapeType() );
    }
#endif

    // collect animation information if needed
    if( mxAnimationsExporter.is() )
        mxAnimationsExporter->collect( xShape, mrExport );

    /* Export shapes name if he has one (#i51726#)
       Export of the shape name for text documents only if the OpenDocument
       file format is written - exceptions are group shapes.
       Note: Writer documents in OpenOffice.org file format doesn't contain
             any names for shapes, except for group shapes.
    */
    if ( ( GetExport().GetModelType() != SvtModuleOptions::EFactory::WRITER &&
           GetExport().GetModelType() != SvtModuleOptions::EFactory::WRITERWEB &&
           GetExport().GetModelType() != SvtModuleOptions::EFactory::WRITERGLOBAL ) ||
         ( GetExport().getExportFlags() & SvXMLExportFlags::OASIS ) ||
         aShapeInfo.meShapeType == XmlShapeType::DrawGroupShape ||
         ( aShapeInfo.meShapeType == XmlShapeType::DrawCustomShape &&
           aShapeInfo.xCustomShapeReplacement.is() ) )
    {
        uno::Reference< container::XNamed > xNamed( xShape, uno::UNO_QUERY );
        if( xNamed.is() )
        {
            const OUString aName( xNamed->getName() );
            if( !aName.isEmpty() )
                mrExport.AddAttribute(XML_NAMESPACE_DRAW, XML_NAME, aName );
        }
    }

    // export style name
    if( !aShapeInfo.msStyleName.isEmpty() )
    {
        if(XmlStyleFamily::SD_GRAPHICS_ID == aShapeInfo.mnFamily)
            mrExport.AddAttribute(XML_NAMESPACE_DRAW, XML_STYLE_NAME, mrExport.EncodeStyleName( aShapeInfo.msStyleName) );
        else
            mrExport.AddAttribute(XML_NAMESPACE_PRESENTATION, XML_STYLE_NAME, mrExport.EncodeStyleName( aShapeInfo.msStyleName) );
    }

    // export text style name
    if( !aShapeInfo.msTextStyleName.isEmpty() )
    {
        mrExport.AddAttribute(XML_NAMESPACE_DRAW, XML_TEXT_STYLE_NAME, aShapeInfo.msTextStyleName );
    }

    // export shapes id if needed
    {
        uno::Reference< uno::XInterface > xRef( xShape, uno::UNO_QUERY );
        const OUString& rShapeId = mrExport.getInterfaceToIdentifierMapper().getIdentifier( xRef );
        if( !rShapeId.isEmpty() )
        {
            mrExport.AddAttributeIdLegacy(XML_NAMESPACE_DRAW, rShapeId);
        }
    }

    // export layer information
    if( mbExportLayer )
    {
        // check for group or scene shape and not export layer if this is one
        uno::Reference< drawing::XShapes > xShapes( xShape, uno::UNO_QUERY );
        if( !xShapes.is() )
        {
            try
            {
                uno::Reference< beans::XPropertySet > xProps( xShape, uno::UNO_QUERY );
                OUString aLayerName;
                xProps->getPropertyValue(u"LayerName"_ustr) >>= aLayerName;
                mrExport.AddAttribute(XML_NAMESPACE_DRAW, XML_LAYER, aLayerName );

            }
            catch(const uno::Exception&)
            {
                DBG_UNHANDLED_EXCEPTION( "xmloff", "exporting layer name for shape" );
            }
        }
    }

    // export draw:display (do not export in ODF 1.3 or older)
    if (xSet.is() && (mrExport.getSaneDefaultVersion() & SvtSaveOptions::ODFSVER_EXTENDED))
    {
        if( aShapeInfo.meShapeType != XmlShapeType::DrawPageShape && aShapeInfo.meShapeType != XmlShapeType::PresPageShape &&
            aShapeInfo.meShapeType != XmlShapeType::HandoutShape && aShapeInfo.meShapeType != XmlShapeType::DrawChartShape )
            try
            {
                bool bVisible = true;
                bool bPrintable = true;

                xSet->getPropertyValue(gsVisible) >>= bVisible;
                xSet->getPropertyValue(gsPrintable) >>= bPrintable;

                XMLTokenEnum eDisplayToken = XML_TOKEN_INVALID;
                const unsigned short nDisplay = (bVisible ? 2 : 0) | (bPrintable ? 1 : 0);
                switch( nDisplay )
                {
                case 0: eDisplayToken = XML_NONE; break;
                case 1: eDisplayToken = XML_PRINTER; break;
                case 2: eDisplayToken = XML_SCREEN; break;
                // case 3: eDisplayToken = XML_ALWAYS break; this is the default
                }

                if( eDisplayToken != XML_TOKEN_INVALID )
                    mrExport.AddAttribute(XML_NAMESPACE_DRAW_EXT, XML_DISPLAY, eDisplayToken );
            }
            catch(const uno::Exception&)
            {
                DBG_UNHANDLED_EXCEPTION("xmloff.draw");
            }
    }

    // #82003# test export count
    // #91587# ALWAYS increment since now ALL to be exported shapes are counted.
    if(mrExport.GetShapeExport()->IsHandleProgressBarEnabled())
    {
        mrExport.GetProgressBarHelper()->Increment();
    }

    onExport( xShape );

    // export shape element
    switch(aShapeInfo.meShapeType)
    {
        case XmlShapeType::DrawRectangleShape:
        {
            ImpExportRectangleShape(xShape, nFeatures, pRefPoint );
            break;
        }
        case XmlShapeType::DrawEllipseShape:
        {
            ImpExportEllipseShape(xShape, nFeatures, pRefPoint );
            break;
        }
        case XmlShapeType::DrawLineShape:
        {
            ImpExportLineShape(xShape, nFeatures, pRefPoint );
            break;
        }
        case XmlShapeType::DrawPolyPolygonShape:  // closed PolyPolygon
        case XmlShapeType::DrawPolyLineShape:     // open PolyPolygon
        case XmlShapeType::DrawClosedBezierShape: // closed tools::PolyPolygon containing curves
        case XmlShapeType::DrawOpenBezierShape:   // open tools::PolyPolygon containing curves
        {
            ImpExportPolygonShape(xShape, aShapeInfo.meShapeType, nFeatures, pRefPoint );
            break;
        }

        case XmlShapeType::DrawTextShape:
        case XmlShapeType::PresTitleTextShape:
        case XmlShapeType::PresOutlinerShape:
        case XmlShapeType::PresSubtitleShape:
        case XmlShapeType::PresNotesShape:
        case XmlShapeType::PresHeaderShape:
        case XmlShapeType::PresFooterShape:
        case XmlShapeType::PresSlideNumberShape:
        case XmlShapeType::PresDateTimeShape:
        {
            ImpExportTextBoxShape(xShape, aShapeInfo.meShapeType, nFeatures, pRefPoint );
            break;
        }

        case XmlShapeType::DrawGraphicObjectShape:
        case XmlShapeType::PresGraphicObjectShape:
        {
            ImpExportGraphicObjectShape(xShape, aShapeInfo.meShapeType, nFeatures, pRefPoint );
            break;
        }

        case XmlShapeType::DrawChartShape:
        case XmlShapeType::PresChartShape:
        {
            ImpExportChartShape(xShape, aShapeInfo.meShapeType, nFeatures, pRefPoint, pAttrList );
            break;
        }

        case XmlShapeType::DrawControlShape:
        {
            ImpExportControlShape(xShape, nFeatures, pRefPoint );
            break;
        }

        case XmlShapeType::DrawConnectorShape:
        {
            ImpExportConnectorShape(xShape, nFeatures, pRefPoint );
            break;
        }

        case XmlShapeType::DrawMeasureShape:
        {
            ImpExportMeasureShape(xShape, nFeatures, pRefPoint );
            break;
        }

        case XmlShapeType::DrawOLE2Shape:
        case XmlShapeType::PresOLE2Shape:
        case XmlShapeType::DrawSheetShape:
        case XmlShapeType::PresSheetShape:
        {
            ImpExportOLE2Shape(xShape, aShapeInfo.meShapeType, nFeatures, pRefPoint );
            break;
        }

        case XmlShapeType::PresTableShape:
        case XmlShapeType::DrawTableShape:
        {
            ImpExportTableShape( xShape, aShapeInfo.meShapeType, nFeatures, pRefPoint );
            break;
        }

        case XmlShapeType::DrawPageShape:
        case XmlShapeType::PresPageShape:
        case XmlShapeType::HandoutShape:
        {
            ImpExportPageShape(xShape, aShapeInfo.meShapeType, nFeatures, pRefPoint );
            break;
        }

        case XmlShapeType::DrawCaptionShape:
        {
            ImpExportCaptionShape(xShape, nFeatures, pRefPoint );
            break;
        }

        case XmlShapeType::Draw3DCubeObject:
        case XmlShapeType::Draw3DSphereObject:
        case XmlShapeType::Draw3DLatheObject:
        case XmlShapeType::Draw3DExtrudeObject:
        {
            ImpExport3DShape(xShape, aShapeInfo.meShapeType);
            break;
        }

        case XmlShapeType::Draw3DSceneObject:
        {
            ImpExport3DSceneShape( xShape, nFeatures, pRefPoint );
            break;
        }

        case XmlShapeType::DrawGroupShape:
        {
            // empty group
            ImpExportGroupShape( xShape, nFeatures, pRefPoint );
            break;
        }

        case XmlShapeType::DrawFrameShape:
        {
            ImpExportFrameShape(xShape, nFeatures, pRefPoint );
            break;
        }

        case XmlShapeType::DrawAppletShape:
        {
            ImpExportAppletShape(xShape, nFeatures, pRefPoint );
            break;
        }

        case XmlShapeType::DrawPluginShape:
        {
            ImpExportPluginShape(xShape, nFeatures, pRefPoint );
            break;
        }

        case XmlShapeType::DrawCustomShape:
        {
            if ( aShapeInfo.xCustomShapeReplacement.is() )
                ImpExportGroupShape( aShapeInfo.xCustomShapeReplacement, nFeatures, pRefPoint );
            else
                ImpExportCustomShape( xShape, nFeatures, pRefPoint );
            break;
        }

        case XmlShapeType::PresMediaShape:
        case XmlShapeType::DrawMediaShape:
        {
            ImpExportMediaShape( xShape, aShapeInfo.meShapeType, nFeatures, pRefPoint );
            break;
        }

        case XmlShapeType::PresOrgChartShape:
        case XmlShapeType::Unknown:
        case XmlShapeType::NotYetSet:
        default:
        {
            // this should never happen and is an error
            OSL_FAIL("XMLEXP: WriteShape: unknown or unexpected type of shape in export!");
            break;
        }
    }

    pHyperlinkElement.reset();

    // #97489# #97111#
    // if there was an error and no element for the shape was exported
    // we need to clear the attribute list or the attributes will be
    // set on the next exported element, which can result in corrupt
    // xml files due to duplicate attributes

    mrExport.CheckAttrList();   // asserts in non pro if we have attributes left
    mrExport.ClearAttrList();   // clears the attributes
}

// This method collects all automatic styles for the shapes inside the given XShapes collection
void XMLShapeExport::collectShapesAutoStyles( const uno::Reference < drawing::XShapes >& xShapes )
{
    ShapesInfos::iterator aOldCurrentShapesIter = maCurrentShapesIter;
    seekShapes( xShapes );

    uno::Reference< drawing::XShape > xShape;
    const sal_Int32 nShapeCount(xShapes->getCount());
    for(sal_Int32 nShapeId = 0; nShapeId < nShapeCount; nShapeId++)
    {
        xShapes->getByIndex(nShapeId) >>= xShape;
        SAL_WARN_IF( !xShape.is(), "xmloff", "Shape without a XShape?" );
        if(!xShape.is())
            continue;

        collectShapeAutoStyles( xShape );
    }

    maCurrentShapesIter = aOldCurrentShapesIter;
}

// This method exports all XShape inside the given XShapes collection
void XMLShapeExport::exportShapes( const uno::Reference < drawing::XShapes >& xShapes, XMLShapeExportFlags nFeatures /* = SEF_DEFAULT */, awt::Point* pRefPoint /* = NULL */ )
{
    ShapesInfos::iterator aOldCurrentShapesIter = maCurrentShapesIter;
    seekShapes( xShapes );

    uno::Reference< drawing::XShape > xShape;
    const sal_Int32 nShapeCount(xShapes->getCount());
    for(sal_Int32 nShapeId = 0; nShapeId < nShapeCount; nShapeId++)
    {
        xShapes->getByIndex(nShapeId) >>= xShape;
        SAL_WARN_IF( !xShape.is(), "xmloff", "Shape without a XShape?" );
        if(!xShape.is())
            continue;

        exportShape( xShape, nFeatures, pRefPoint );
    }

    maCurrentShapesIter = aOldCurrentShapesIter;
}

namespace xmloff {

void FixZOrder(uno::Reference<drawing::XShapes> const& xShapes,
    std::function<unsigned int (uno::Reference<beans::XPropertySet> const&)> const& rGetLayer)
{
    uno::Reference<drawing::XShapes3> const xShapes3(xShapes, uno::UNO_QUERY);
    assert(xShapes3.is());
    if (!xShapes3.is())
    {
        return; // only SvxDrawPage implements this
    }
    struct Layer { std::vector<sal_Int32> shapes; sal_Int32 nMin = SAL_MAX_INT32; sal_Int32 nMax = 0; };
    std::vector<Layer> layers;
    // shapes are sorted by ZOrder
    sal_Int32 const nCount(xShapes->getCount());
    for (sal_Int32 i = 0; i < nCount; ++i)
    {
        uno::Reference<beans::XPropertySet> const xShape(xShapes->getByIndex(i), uno::UNO_QUERY);
        if (!xShape.is())
        {
            SAL_WARN("xmloff", "FixZOrder: null shape, cannot sort");
            return;
        }
        unsigned int const nLayer(rGetLayer(xShape));
        if (layers.size() <= nLayer)
        {
            layers.resize(nLayer + 1);
        }
        layers[nLayer].shapes.emplace_back(i);
        if (i < layers[nLayer].nMin)
        {
            layers[nLayer].nMin = i;
        }
        if (layers[nLayer].nMax < i)
        {
            layers[nLayer].nMax = i;
        }
    }
    std::erase_if(layers, [](Layer const& rLayer) { return rLayer.shapes.empty(); });
    bool isSorted(true);
    for (size_t i = 1; i < layers.size(); ++i)
    {
        assert(layers[i].nMin != layers[i-1].nMax); // unique!
        if (layers[i].nMin < layers[i-1].nMax)
        {
            isSorted = false;
            break;
        }
    }
    if (isSorted)
    {
        return; // nothing to do
    }
    uno::Sequence<sal_Int32> aNewOrder(nCount);
    auto iterInsert(aNewOrder.getArray());
    for (auto const& rLayer : layers)
    {
        assert(rLayer.nMin <= rLayer.nMax); // empty layers have been removed
        iterInsert = std::copy(rLayer.shapes.begin(), rLayer.shapes.end(), iterInsert);
    }
    try
    {
        xShapes3->sort(aNewOrder);
    }
    catch (uno::Exception const&)
    {
        SAL_WARN("xmloff", "FixZOrder: exception");
    }
}

} // namespace xmloff

void XMLShapeExport::seekShapes( const uno::Reference< drawing::XShapes >& xShapes ) noexcept
{
    if( xShapes.is() )
    {
        maCurrentShapesIter = maShapesInfos.find( xShapes );
        if( maCurrentShapesIter == maShapesInfos.end() )
        {
            auto itPair = maShapesInfos.emplace( xShapes, ImplXMLShapeExportInfoVector( static_cast<ShapesInfos::size_type>(xShapes->getCount()) ) );

            maCurrentShapesIter = itPair.first;

            SAL_WARN_IF( maCurrentShapesIter == maShapesInfos.end(), "xmloff", "XMLShapeExport::seekShapes(): insert into stl::map failed" );
        }

        SAL_WARN_IF( (*maCurrentShapesIter).second.size() != static_cast<ShapesInfos::size_type>(xShapes->getCount()), "xmloff", "XMLShapeExport::seekShapes(): XShapes size varied between calls" );

    }
    else
    {
        maCurrentShapesIter = maShapesInfos.end();
    }
}

void XMLShapeExport::exportAutoStyles()
{
    // export all autostyle infos

    // ...for graphic
    {
        GetExport().GetAutoStylePool()->exportXML( XmlStyleFamily::SD_GRAPHICS_ID );
    }

    // ...for presentation
    {
        GetExport().GetAutoStylePool()->exportXML( XmlStyleFamily::SD_PRESENTATION_ID );
    }

    if( mxShapeTableExport.is() )
        mxShapeTableExport->exportAutoStyles();
}

/// returns the export property mapper for external chaining
SvXMLExportPropertyMapper* XMLShapeExport::CreateShapePropMapper(
    SvXMLExport& rExport )
{
    rtl::Reference< XMLPropertyHandlerFactory > xFactory = new XMLSdPropHdlFactory( rExport.GetModel(), rExport );
    rtl::Reference < XMLPropertySetMapper > xMapper = new XMLShapePropertySetMapper( xFactory, true );
    rExport.GetTextParagraphExport(); // get or create text paragraph export
    SvXMLExportPropertyMapper* pResult =
        new XMLShapeExportPropertyMapper( xMapper, rExport );
    // chain text attributes
    return pResult;
}

void XMLShapeExport::ImpCalcShapeType(const uno::Reference< drawing::XShape >& xShape,
    XmlShapeType& eShapeType)
{
    // set in every case, so init here
    eShapeType = XmlShapeType::Unknown;

    if(!xShape.is())
        return;

    OUString aType(xShape->getShapeType());

    if(!aType.match("com.sun.star."))
        return;

    if(aType.match("drawing.", 13))
    {
        // drawing shapes
        if     (aType.match("Rectangle", 21)) { eShapeType = XmlShapeType::DrawRectangleShape; }

        // #i72177# Note: Correcting CustomShape, CustomShape->Custom, len from 9 (was wrong anyways) to 6.
        // As can be seen at the other compares, the appendix "Shape" is left out of the comparison.
        else if(aType.match("Custom", 21)) { eShapeType = XmlShapeType::DrawCustomShape; }

        else if(aType.match("Ellipse", 21)) { eShapeType = XmlShapeType::DrawEllipseShape; }
        else if(aType.match("Control", 21)) { eShapeType = XmlShapeType::DrawControlShape; }
        else if(aType.match("Connector", 21)) { eShapeType = XmlShapeType::DrawConnectorShape; }
        else if(aType.match("Measure", 21)) { eShapeType = XmlShapeType::DrawMeasureShape; }
        else if(aType.match("Line", 21)) { eShapeType = XmlShapeType::DrawLineShape; }

        // #i72177# Note: This covers two types by purpose, PolyPolygonShape and PolyPolygonPathShape
        else if(aType.match("PolyPolygon", 21)) { eShapeType = XmlShapeType::DrawPolyPolygonShape; }

        // #i72177# Note: This covers two types by purpose, PolyLineShape and PolyLinePathShape
        else if(aType.match("PolyLine", 21)) { eShapeType = XmlShapeType::DrawPolyLineShape; }

        else if(aType.match("OpenBezier", 21)) { eShapeType = XmlShapeType::DrawOpenBezierShape; }
        else if(aType.match("ClosedBezier", 21)) { eShapeType = XmlShapeType::DrawClosedBezierShape; }

        // #i72177# FreeHand (opened and closed) now supports the types OpenFreeHandShape and
        // ClosedFreeHandShape respectively. Represent them as bezier shapes
        else if(aType.match("OpenFreeHand", 21)) { eShapeType = XmlShapeType::DrawOpenBezierShape; }
        else if(aType.match("ClosedFreeHand", 21)) { eShapeType = XmlShapeType::DrawClosedBezierShape; }

        else if(aType.match("GraphicObject", 21)) { eShapeType = XmlShapeType::DrawGraphicObjectShape; }
        else if(aType.match("Group", 21)) { eShapeType = XmlShapeType::DrawGroupShape; }
        else if(aType.match("Text", 21)) { eShapeType = XmlShapeType::DrawTextShape; }
        else if(aType.match("OLE2", 21))
        {
            eShapeType = XmlShapeType::DrawOLE2Shape;

            // get info about presentation shape
            uno::Reference <beans::XPropertySet> xPropSet(xShape, uno::UNO_QUERY);

            if(xPropSet.is())
            {
                OUString sCLSID;
                if(xPropSet->getPropertyValue(u"CLSID"_ustr) >>= sCLSID)
                {
#if !ENABLE_WASM_STRIP_CHART
                    // WASM_CHART change
                    // TODO: With Chart extracted this cannot really happen since
                    // no Chart could've been added at all
                    if (sCLSID == mrExport.GetChartExport()->getChartCLSID() ||
#else
                    if(
#endif
                        sCLSID == SvGlobalName( SO3_RPTCH_CLASSID ).GetHexName() )
                    {
                        eShapeType = XmlShapeType::DrawChartShape;
                    }
                    else if (sCLSID == SvGlobalName( SO3_SC_CLASSID ).GetHexName() )
                    {
                        eShapeType = XmlShapeType::DrawSheetShape;
                    }
                    else
                    {
                        // general OLE2 Object
                    }
                }
            }
        }
        else if(aType.match("Page", 21)) { eShapeType = XmlShapeType::DrawPageShape; }
        else if(aType.match("Frame", 21)) { eShapeType = XmlShapeType::DrawFrameShape; }
        else if(aType.match("Caption", 21)) { eShapeType = XmlShapeType::DrawCaptionShape; }
        else if(aType.match("Plugin", 21)) { eShapeType = XmlShapeType::DrawPluginShape; }
        else if(aType.match("Applet", 21)) { eShapeType = XmlShapeType::DrawAppletShape; }
        else if(aType.match("MediaShape", 21)) { eShapeType = XmlShapeType::DrawMediaShape; }
        else if(aType.match("TableShape", 21)) { eShapeType = XmlShapeType::DrawTableShape; }

        // 3D shapes
        else if(aType.match("Scene", 21 + 7)) { eShapeType = XmlShapeType::Draw3DSceneObject; }
        else if(aType.match("Cube", 21 + 7)) { eShapeType = XmlShapeType::Draw3DCubeObject; }
        else if(aType.match("Sphere", 21 + 7)) { eShapeType = XmlShapeType::Draw3DSphereObject; }
        else if(aType.match("Lathe", 21 + 7)) { eShapeType = XmlShapeType::Draw3DLatheObject; }
        else if(aType.match("Extrude", 21 + 7)) { eShapeType = XmlShapeType::Draw3DExtrudeObject; }
    }
    else if(aType.match("presentation.", 13))
    {
        // presentation shapes
        if     (aType.match("TitleText", 26)) { eShapeType = XmlShapeType::PresTitleTextShape; }
        else if(aType.match("Outliner", 26)) { eShapeType = XmlShapeType::PresOutlinerShape;  }
        else if(aType.match("Subtitle", 26)) { eShapeType = XmlShapeType::PresSubtitleShape;  }
        else if(aType.match("GraphicObject", 26)) { eShapeType = XmlShapeType::PresGraphicObjectShape;  }
        else if(aType.match("Page", 26)) { eShapeType = XmlShapeType::PresPageShape;  }
        else if(aType.match("OLE2", 26))
        {
            eShapeType = XmlShapeType::PresOLE2Shape;

            // get info about presentation shape
            uno::Reference <beans::XPropertySet> xPropSet(xShape, uno::UNO_QUERY);

            if(xPropSet.is()) try
            {
                OUString sCLSID;
                if(xPropSet->getPropertyValue(u"CLSID"_ustr) >>= sCLSID)
                {
                    if( sCLSID == SvGlobalName( SO3_SC_CLASSID ).GetHexName() )
                    {
                        eShapeType = XmlShapeType::PresSheetShape;
                    }
                }
            }
            catch(const uno::Exception&)
            {
                SAL_WARN( "xmloff", "XMLShapeExport::ImpCalcShapeType(), expected ole shape to have the CLSID property?" );
            }
        }
        else if(aType.match("Chart", 26)) { eShapeType = XmlShapeType::PresChartShape;  }
        else if(aType.match("OrgChart", 26)) { eShapeType = XmlShapeType::PresOrgChartShape;  }
        else if(aType.match("CalcShape", 26)) { eShapeType = XmlShapeType::PresSheetShape; }
        else if(aType.match("TableShape", 26)) { eShapeType = XmlShapeType::PresTableShape; }
        else if(aType.match("Notes", 26)) { eShapeType = XmlShapeType::PresNotesShape;  }
        else if(aType.match("HandoutShape", 26)) { eShapeType = XmlShapeType::HandoutShape; }
        else if(aType.match("HeaderShape", 26)) { eShapeType = XmlShapeType::PresHeaderShape; }
        else if(aType.match("FooterShape", 26)) { eShapeType = XmlShapeType::PresFooterShape; }
        else if(aType.match("SlideNumberShape", 26)) { eShapeType = XmlShapeType::PresSlideNumberShape; }
        else if(aType.match("DateTimeShape", 26)) { eShapeType = XmlShapeType::PresDateTimeShape; }
        else if(aType.match("MediaShape", 26)) { eShapeType = XmlShapeType::PresMediaShape; }
    }
}

/** exports all user defined gluepoints */
void XMLShapeExport::ImpExportGluePoints( const uno::Reference< drawing::XShape >& xShape )
{
    uno::Reference< drawing::XGluePointsSupplier > xSupplier( xShape, uno::UNO_QUERY );
    if( !xSupplier.is() )
        return;

    uno::Reference< container::XIdentifierAccess > xGluePoints( xSupplier->getGluePoints(), uno::UNO_QUERY );
    if( !xGluePoints.is() )
        return;

    drawing::GluePoint2 aGluePoint;

    const uno::Sequence< sal_Int32 > aIdSequence( xGluePoints->getIdentifiers() );

    for( const sal_Int32 nIdentifier : aIdSequence )
    {
        if( (xGluePoints->getByIdentifier( nIdentifier ) >>= aGluePoint) && aGluePoint.IsUserDefined )
        {
            // export only user defined gluepoints

            const OUString sId( OUString::number( nIdentifier ) );
            mrExport.AddAttribute(XML_NAMESPACE_DRAW, XML_ID, sId );

            mrExport.GetMM100UnitConverter().convertMeasureToXML(msBuffer,
                    aGluePoint.Position.X);
            mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_X, msBuffer.makeStringAndClear());

            mrExport.GetMM100UnitConverter().convertMeasureToXML(msBuffer,
                    aGluePoint.Position.Y);
            mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_Y, msBuffer.makeStringAndClear());

            if( !aGluePoint.IsRelative )
            {
                SvXMLUnitConverter::convertEnum( msBuffer, aGluePoint.PositionAlignment, aXML_GlueAlignment_EnumMap );
                mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_ALIGN, msBuffer.makeStringAndClear() );
            }

            if( aGluePoint.Escape != drawing::EscapeDirection_SMART )
            {
                SvXMLUnitConverter::convertEnum( msBuffer, aGluePoint.Escape, aXML_GlueEscapeDirection_EnumMap );
                mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_ESCAPE_DIRECTION, msBuffer.makeStringAndClear() );
            }

            SvXMLElementExport aEventsElemt(mrExport, XML_NAMESPACE_DRAW, XML_GLUE_POINT, true, true);
        }
    }
}

void XMLShapeExport::ImpExportSignatureLine(const uno::Reference<drawing::XShape>& xShape)
{
    uno::Reference<beans::XPropertySet> xPropSet(xShape, uno::UNO_QUERY);

    bool bIsSignatureLine = false;
    xPropSet->getPropertyValue(u"IsSignatureLine"_ustr) >>= bIsSignatureLine;
    if (!bIsSignatureLine)
        return;

    OUString aSignatureLineId;
    xPropSet->getPropertyValue(u"SignatureLineId"_ustr) >>= aSignatureLineId;
    mrExport.AddAttribute(XML_NAMESPACE_LO_EXT, XML_ID, aSignatureLineId);

    OUString aSuggestedSignerName;
    xPropSet->getPropertyValue(u"SignatureLineSuggestedSignerName"_ustr) >>= aSuggestedSignerName;
    if (!aSuggestedSignerName.isEmpty())
        mrExport.AddAttribute(XML_NAMESPACE_LO_EXT, XML_SUGGESTED_SIGNER_NAME, aSuggestedSignerName);

    OUString aSuggestedSignerTitle;
    xPropSet->getPropertyValue(u"SignatureLineSuggestedSignerTitle"_ustr) >>= aSuggestedSignerTitle;
    if (!aSuggestedSignerTitle.isEmpty())
        mrExport.AddAttribute(XML_NAMESPACE_LO_EXT, XML_SUGGESTED_SIGNER_TITLE, aSuggestedSignerTitle);

    OUString aSuggestedSignerEmail;
    xPropSet->getPropertyValue(u"SignatureLineSuggestedSignerEmail"_ustr) >>= aSuggestedSignerEmail;
    if (!aSuggestedSignerEmail.isEmpty())
        mrExport.AddAttribute(XML_NAMESPACE_LO_EXT, XML_SUGGESTED_SIGNER_EMAIL, aSuggestedSignerEmail);

    OUString aSigningInstructions;
    xPropSet->getPropertyValue(u"SignatureLineSigningInstructions"_ustr) >>= aSigningInstructions;
    if (!aSigningInstructions.isEmpty())
        mrExport.AddAttribute(XML_NAMESPACE_LO_EXT, XML_SIGNING_INSTRUCTIONS, aSigningInstructions);

    bool bShowSignDate = false;
    xPropSet->getPropertyValue(u"SignatureLineShowSignDate"_ustr) >>= bShowSignDate;
    mrExport.AddAttribute(XML_NAMESPACE_LO_EXT, XML_SHOW_SIGN_DATE,
                          bShowSignDate ? XML_TRUE : XML_FALSE);

    bool bCanAddComment = false;
    xPropSet->getPropertyValue(u"SignatureLineCanAddComment"_ustr) >>= bCanAddComment;
    mrExport.AddAttribute(XML_NAMESPACE_LO_EXT, XML_CAN_ADD_COMMENT,
                          bCanAddComment ? XML_TRUE : XML_FALSE);

    SvXMLElementExport aSignatureLineElement(mrExport, XML_NAMESPACE_LO_EXT, XML_SIGNATURELINE, true,
                                             true);
}

void XMLShapeExport::ImpExportQRCode(const uno::Reference<drawing::XShape>& xShape)
{
    uno::Reference<beans::XPropertySet> xPropSet(xShape, uno::UNO_QUERY);

    uno::Any aAny = xPropSet->getPropertyValue(u"BarCodeProperties"_ustr);

    css::drawing::BarCode aBarCode;
    if(!(aAny >>= aBarCode))
        return;

    mrExport.AddAttribute(XML_NAMESPACE_OFFICE, XML_STRING_VALUE, aBarCode.Payload);
    /* Export QR Code as per customised schema, @see OpenDocument-schema-v1.3+libreoffice */
    OUString temp;
    switch(aBarCode.ErrorCorrection){
        case css::drawing::BarCodeErrorCorrection::LOW :
            temp = "low";
            break;
        case css::drawing::BarCodeErrorCorrection::MEDIUM:
            temp = "medium";
            break;
        case css::drawing::BarCodeErrorCorrection::QUARTILE:
            temp = "quartile";
            break;
        case css::drawing::BarCodeErrorCorrection::HIGH:
            temp = "high";
            break;
    }
    mrExport.AddAttribute(XML_NAMESPACE_LO_EXT, XML_QRCODE_ERROR_CORRECTION, temp);
    mrExport.AddAttribute(XML_NAMESPACE_LO_EXT, XML_QRCODE_BORDER, OUStringBuffer(20).append(aBarCode.Border).makeStringAndClear());
    mrExport.AddAttribute(XML_NAMESPACE_LO_EXT, XML_QRCODE_TYPE, OUStringBuffer(20).append(aBarCode.Type).makeStringAndClear());

    SvXMLElementExport aBarCodeElement(mrExport, XML_NAMESPACE_LO_EXT, XML_QRCODE, true,
                                            true);
}

void XMLShapeExport::ExportGraphicDefaults()
{
    rtl::Reference<XMLStyleExport> aStEx(new XMLStyleExport(mrExport, mrExport.GetAutoStylePool().get()));

    // construct PropertySetMapper
    rtl::Reference< SvXMLExportPropertyMapper > xPropertySetMapper( CreateShapePropMapper( mrExport ) );
    static_cast<XMLShapeExportPropertyMapper*>(xPropertySetMapper.get())->SetAutoStyles( false );

    // chain text attributes
    xPropertySetMapper->ChainExportMapper(XMLTextParagraphExport::CreateParaExtPropMapper(mrExport));

    // chain special Writer/text frame default attributes
    xPropertySetMapper->ChainExportMapper(XMLTextParagraphExport::CreateParaDefaultExtPropMapper(mrExport));

    // write graphic family default style
    uno::Reference< lang::XMultiServiceFactory > xFact( mrExport.GetModel(), uno::UNO_QUERY );
    if( !xFact.is() )
        return;

    try
    {
        uno::Reference< beans::XPropertySet > xDefaults( xFact->createInstance(u"com.sun.star.drawing.Defaults"_ustr), uno::UNO_QUERY );
        if( xDefaults.is() )
        {
            aStEx->exportDefaultStyle( xDefaults, XML_STYLE_FAMILY_SD_GRAPHICS_NAME, xPropertySetMapper );

            // write graphic styles (family name differs depending on the module)
            aStEx->exportStyleFamily(u"graphics"_ustr, XML_STYLE_FAMILY_SD_GRAPHICS_NAME, xPropertySetMapper, false, XmlStyleFamily::SD_GRAPHICS_ID);
            aStEx->exportStyleFamily(u"GraphicStyles"_ustr, XML_STYLE_FAMILY_SD_GRAPHICS_NAME, xPropertySetMapper, false, XmlStyleFamily::SD_GRAPHICS_ID);
        }
    }
    catch(const lang::ServiceNotRegisteredException&)
    {
    }
}

void XMLShapeExport::onExport( const css::uno::Reference < css::drawing::XShape >& )
{
}

const rtl::Reference< XMLTableExport >& XMLShapeExport::GetShapeTableExport()
{
    if( !mxShapeTableExport.is() )
    {
        rtl::Reference< XMLPropertyHandlerFactory > xFactory( new XMLSdPropHdlFactory( mrExport.GetModel(), mrExport ) );
        rtl::Reference < XMLPropertySetMapper > xMapper( new XMLShapePropertySetMapper( xFactory, true ) );
        mrExport.GetTextParagraphExport(); // get or create text paragraph export
        rtl::Reference< SvXMLExportPropertyMapper > xPropertySetMapper( new XMLShapeExportPropertyMapper( xMapper, mrExport ) );
        mxShapeTableExport = new XMLTableExport( mrExport, xPropertySetMapper, xFactory );
    }

    return mxShapeTableExport;
}

void XMLShapeExport::ImpExportNewTrans(const uno::Reference< beans::XPropertySet >& xPropSet,
    XMLShapeExportFlags nFeatures, awt::Point* pRefPoint)
{
    // get matrix
    ::basegfx::B2DHomMatrix aMatrix;
    ImpExportNewTrans_GetB2DHomMatrix(aMatrix, xPropSet);

    // decompose and correct about pRefPoint
    ::basegfx::B2DTuple aTRScale;
    double fTRShear(0.0);
    double fTRRotate(0.0);
    ::basegfx::B2DTuple aTRTranslate;
    ImpExportNewTrans_DecomposeAndRefPoint(aMatrix, aTRScale, fTRShear, fTRRotate, aTRTranslate, pRefPoint);

    // use features and write
    ImpExportNewTrans_FeaturesAndWrite(aTRScale, fTRShear, fTRRotate, aTRTranslate, nFeatures);
}

void XMLShapeExport::ImpExportNewTrans_GetB2DHomMatrix(::basegfx::B2DHomMatrix& rMatrix,
    const uno::Reference< beans::XPropertySet >& xPropSet)
{
    /* Get <TransformationInHoriL2R>, if it exist
       and if the document is exported into the OpenOffice.org file format.
       This property only exists at service css::text::Shape - the
       Writer UNO service for shapes.
       This code is needed, because the positioning attributes in the
       OpenOffice.org file format are given in horizontal left-to-right layout
       regardless the layout direction the shape is in. In the OASIS Open Office
       file format the positioning attributes are correctly given in the layout
       direction the shape is in. Thus, this code provides the conversion from
       the OASIS Open Office file format to the OpenOffice.org file format. (#i28749#)
    */
    uno::Any aAny;
    if ( !( GetExport().getExportFlags() & SvXMLExportFlags::OASIS ) &&
         xPropSet->getPropertySetInfo()->hasPropertyByName(u"TransformationInHoriL2R"_ustr) )
    {
        aAny = xPropSet->getPropertyValue(u"TransformationInHoriL2R"_ustr);
    }
    else
    {
        aAny = xPropSet->getPropertyValue(u"Transformation"_ustr);
    }
    drawing::HomogenMatrix3 aMatrix;
    aAny >>= aMatrix;

    rMatrix.set(0, 0, aMatrix.Line1.Column1);
    rMatrix.set(0, 1, aMatrix.Line1.Column2);
    rMatrix.set(0, 2, aMatrix.Line1.Column3);
    rMatrix.set(1, 0, aMatrix.Line2.Column1);
    rMatrix.set(1, 1, aMatrix.Line2.Column2);
    rMatrix.set(1, 2, aMatrix.Line2.Column3);
    // For this to be a valid 2D transform matrix, the last row must be [0,0,1]
    assert( aMatrix.Line3.Column1 == 0 );
    assert( aMatrix.Line3.Column2 == 0 );
    assert( aMatrix.Line3.Column3 == 1 );
}

void XMLShapeExport::ImpExportNewTrans_DecomposeAndRefPoint(const ::basegfx::B2DHomMatrix& rMatrix, ::basegfx::B2DTuple& rTRScale,
    double& fTRShear, double& fTRRotate, ::basegfx::B2DTuple& rTRTranslate, css::awt::Point* pRefPoint)
{
    // decompose matrix
    rMatrix.decompose(rTRScale, rTRTranslate, fTRRotate, fTRShear);

    // correct translation about pRefPoint
    if(pRefPoint)
    {
        rTRTranslate -= ::basegfx::B2DTuple(pRefPoint->X, pRefPoint->Y);
    }
}

void XMLShapeExport::ImpExportNewTrans_FeaturesAndWrite(::basegfx::B2DTuple const & rTRScale, double fTRShear,
    double fTRRotate, ::basegfx::B2DTuple const & rTRTranslate, const XMLShapeExportFlags nFeatures)
{
    // always write Size (rTRScale) since this statement carries the union
    // of the object
    OUString aStr;
    OUStringBuffer sStringBuffer;
    ::basegfx::B2DTuple aTRScale(rTRScale);

    // svg: width
    if(!(nFeatures & XMLShapeExportFlags::WIDTH))
    {
        aTRScale.setX(1.0);
    }
    else
    {
        if( aTRScale.getX() > 0.0 )
            aTRScale.setX(aTRScale.getX() - 1.0);
        else if( aTRScale.getX() < 0.0 )
            aTRScale.setX(aTRScale.getX() + 1.0);
    }

    mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer,
                                                         basegfx::fround(aTRScale.getX()));
    aStr = sStringBuffer.makeStringAndClear();
    mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_WIDTH, aStr);

    // svg: height
    if(!(nFeatures & XMLShapeExportFlags::HEIGHT))
    {
        aTRScale.setY(1.0);
    }
    else
    {
        if( aTRScale.getY() > 0.0 )
            aTRScale.setY(aTRScale.getY() - 1.0);
        else if( aTRScale.getY() < 0.0 )
            aTRScale.setY(aTRScale.getY() + 1.0);
    }

    mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer,
                                                         basegfx::fround(aTRScale.getY()));
    aStr = sStringBuffer.makeStringAndClear();
    mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_HEIGHT, aStr);

    // decide if transformation is necessary
    bool bTransformationIsNecessary(fTRShear != 0.0 || fTRRotate != 0.0);

    if(bTransformationIsNecessary)
    {
        // write transformation, but WITHOUT scale which is exported as size above
        SdXMLImExTransform2D aTransform;

        aTransform.AddSkewX(atan(fTRShear));

        // #i78696#
        // fTRRotate is mathematically correct, but due to the error
        // we export/import it mirrored. Since the API implementation is fixed and
        // uses the correctly oriented angle, it is necessary for compatibility to
        // mirror the angle here to stay at the old behaviour. There is a follow-up
        // task (#i78698#) to fix this in the next ODF FileFormat version
        aTransform.AddRotate(-fTRRotate);

        aTransform.AddTranslate(rTRTranslate);

        // does transformation need to be exported?
        if(aTransform.NeedsAction())
            mrExport.AddAttribute(XML_NAMESPACE_DRAW, XML_TRANSFORM, aTransform.GetExportString(mrExport.GetMM100UnitConverter()));
    }
    else
    {
        // no shear, no rotate; just add object position to export and we are done
        if(nFeatures & XMLShapeExportFlags::X)
        {
            // svg: x
            mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer,
                    basegfx::fround(rTRTranslate.getX()));
            aStr = sStringBuffer.makeStringAndClear();
            mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_X, aStr);
        }

        if(nFeatures & XMLShapeExportFlags::Y)
        {
            // svg: y
            mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer,
                    basegfx::fround(rTRTranslate.getY()));
            aStr = sStringBuffer.makeStringAndClear();
            mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_Y, aStr);
        }
    }
}

bool XMLShapeExport::ImpExportPresentationAttributes( const uno::Reference< beans::XPropertySet >& xPropSet, const OUString& rClass )
{
    bool bIsEmpty = false;

    // write presentation class entry
    mrExport.AddAttribute(XML_NAMESPACE_PRESENTATION, XML_CLASS, rClass);

    if( xPropSet.is() )
    {
        uno::Reference< beans::XPropertySetInfo > xPropSetInfo( xPropSet->getPropertySetInfo() );


        // is empty pres. shape?
        if( xPropSetInfo.is() && xPropSetInfo->hasPropertyByName(u"IsEmptyPresentationObject"_ustr))
        {
            xPropSet->getPropertyValue(u"IsEmptyPresentationObject"_ustr) >>= bIsEmpty;
            if( bIsEmpty )
                mrExport.AddAttribute(XML_NAMESPACE_PRESENTATION, XML_PLACEHOLDER, XML_TRUE);
        }

        // is user-transformed?
        if( xPropSetInfo.is() && xPropSetInfo->hasPropertyByName(u"IsPlaceholderDependent"_ustr))
        {
            bool bTemp = false;
            xPropSet->getPropertyValue(u"IsPlaceholderDependent"_ustr) >>= bTemp;
            if(!bTemp)
                mrExport.AddAttribute(XML_NAMESPACE_PRESENTATION, XML_USER_TRANSFORMED, XML_TRUE);
        }
    }

    return bIsEmpty;
}

void XMLShapeExport::ImpExportText( const uno::Reference< drawing::XShape >& xShape, TextPNS eExtensionNS )
{
    if (eExtensionNS == TextPNS::EXTENSION)
    {
        if ((mrExport.getSaneDefaultVersion() & SvtSaveOptions::ODFSVER_EXTENDED) == 0)
        {
            return; // do not export to ODF 1.1/1.2/1.3
        }
    }
    uno::Reference< text::XText > xText( xShape, uno::UNO_QUERY );
    if( xText.is() )
    {
        uno::Reference< container::XEnumerationAccess > xEnumAccess( xShape, uno::UNO_QUERY );
        if( xEnumAccess.is() && xEnumAccess->hasElements() )
            mrExport.GetTextParagraphExport()->exportText( xText, false, true, eExtensionNS );
    }
}

namespace {

enum class Found {
    NONE              = 0x0000,
    CLICKACTION       = 0x0001,
    BOOKMARK          = 0x0002,
    EFFECT            = 0x0004,
    PLAYFULL          = 0x0008,
    VERB              = 0x0010,
    SOUNDURL          = 0x0020,
    SPEED             = 0x0040,
    CLICKEVENTTYPE    = 0x0080,
    MACRO             = 0x0100,
    LIBRARY           = 0x0200,
};

}

namespace o3tl {
    template<> struct typed_flags<Found> : is_typed_flags<Found, 0x03ff> {};
}

void XMLShapeExport::ImpExportEvents( const uno::Reference< drawing::XShape >& xShape )
{
    uno::Reference< document::XEventsSupplier > xEventsSupplier( xShape, uno::UNO_QUERY );
    if( !xEventsSupplier.is() )
        return;

    uno::Reference< container::XNameAccess > xEvents = xEventsSupplier->getEvents();
    SAL_WARN_IF( !xEvents.is(), "xmloff", "XEventsSupplier::getEvents() returned NULL" );
    if( !xEvents.is() )
        return;

    Found nFound = Found::NONE;

    OUString aClickEventType;
    presentation::ClickAction eClickAction = presentation::ClickAction_NONE;
    presentation::AnimationEffect eEffect = presentation::AnimationEffect_NONE;
    presentation::AnimationSpeed eSpeed = presentation::AnimationSpeed_SLOW;
    OUString aStrSoundURL;
    bool bPlayFull = false;
    sal_Int32 nVerb = 0;
    OUString aStrMacro;
    OUString aStrLibrary;
    OUString aStrBookmark;

    uno::Sequence< beans::PropertyValue > aClickProperties;
    if( xEvents->hasByName( gsOnClick ) && (xEvents->getByName( gsOnClick ) >>= aClickProperties) )
    {
        for (const auto& rProperty : aClickProperties)
        {
            if( !( nFound & Found::CLICKEVENTTYPE ) && rProperty.Name == gsEventType )
            {
                if( rProperty.Value >>= aClickEventType )
                    nFound |= Found::CLICKEVENTTYPE;
            }
            else if( !( nFound & Found::CLICKACTION ) && rProperty.Name == gsClickAction )
            {
                if( rProperty.Value >>= eClickAction )
                    nFound |= Found::CLICKACTION;
            }
            else if( !( nFound & Found::MACRO ) && ( rProperty.Name == gsMacroName || rProperty.Name == gsScript ) )
            {
                if( rProperty.Value >>= aStrMacro )
                    nFound |= Found::MACRO;
            }
            else if( !( nFound & Found::LIBRARY ) && rProperty.Name == gsLibrary )
            {
                if( rProperty.Value >>= aStrLibrary )
                    nFound |= Found::LIBRARY;
            }
            else if( !( nFound & Found::EFFECT ) && rProperty.Name == gsEffect )
            {
                if( rProperty.Value >>= eEffect )
                    nFound |= Found::EFFECT;
            }
            else if( !( nFound & Found::BOOKMARK ) && rProperty.Name == gsBookmark )
            {
                if( rProperty.Value >>= aStrBookmark )
                    nFound |= Found::BOOKMARK;
            }
            else if( !( nFound & Found::SPEED ) && rProperty.Name == gsSpeed )
            {
                if( rProperty.Value >>= eSpeed )
                    nFound |= Found::SPEED;
            }
            else if( !( nFound & Found::SOUNDURL ) && rProperty.Name == gsSoundURL )
            {
                if( rProperty.Value >>= aStrSoundURL )
                    nFound |= Found::SOUNDURL;
            }
            else if( !( nFound & Found::PLAYFULL ) && rProperty.Name == gsPlayFull )
            {
                if( rProperty.Value >>= bPlayFull )
                    nFound |= Found::PLAYFULL;
            }
            else if( !( nFound & Found::VERB ) && rProperty.Name == gsVerb )
            {
                if( rProperty.Value >>= nVerb )
                    nFound |= Found::VERB;
            }
        }
    }

    // create the XML elements

    if( aClickEventType == gsPresentation )
    {
        if( !(nFound & Found::CLICKACTION) || (eClickAction == presentation::ClickAction_NONE) )
            return;

        SvXMLElementExport aEventsElemt(mrExport, XML_NAMESPACE_OFFICE, XML_EVENT_LISTENERS, true, true);

        enum XMLTokenEnum eStrAction;

        switch( eClickAction )
        {
            case presentation::ClickAction_PREVPAGE:        eStrAction = XML_PREVIOUS_PAGE; break;
            case presentation::ClickAction_NEXTPAGE:        eStrAction = XML_NEXT_PAGE; break;
            case presentation::ClickAction_FIRSTPAGE:       eStrAction = XML_FIRST_PAGE; break;
            case presentation::ClickAction_LASTPAGE:        eStrAction = XML_LAST_PAGE; break;
            case presentation::ClickAction_INVISIBLE:       eStrAction = XML_HIDE; break;
            case presentation::ClickAction_STOPPRESENTATION:eStrAction = XML_STOP; break;
            case presentation::ClickAction_PROGRAM:         eStrAction = XML_EXECUTE; break;
            case presentation::ClickAction_BOOKMARK:        eStrAction = XML_SHOW; break;
            case presentation::ClickAction_DOCUMENT:        eStrAction = XML_SHOW; break;
            case presentation::ClickAction_MACRO:           eStrAction = XML_EXECUTE_MACRO; break;
            case presentation::ClickAction_VERB:            eStrAction = XML_VERB; break;
            case presentation::ClickAction_VANISH:          eStrAction = XML_FADE_OUT; break;
            case presentation::ClickAction_SOUND:           eStrAction = XML_SOUND; break;
            default:
                OSL_FAIL( "unknown presentation::ClickAction found!" );
                eStrAction = XML_UNKNOWN;
        }

        OUString aEventQName(
            mrExport.GetNamespaceMap().GetQNameByKey(
                    XML_NAMESPACE_DOM, u"click"_ustr ) );
        mrExport.AddAttribute( XML_NAMESPACE_SCRIPT, XML_EVENT_NAME, aEventQName );
        mrExport.AddAttribute( XML_NAMESPACE_PRESENTATION, XML_ACTION, eStrAction );

        if( eClickAction == presentation::ClickAction_VANISH )
        {
            if( nFound & Found::EFFECT )
            {
                XMLEffect eKind;
                XMLEffectDirection eDirection;
                sal_Int16 nStartScale;
                bool bIn;

                SdXMLImplSetEffect( eEffect, eKind, eDirection, nStartScale, bIn );

                if( eKind != EK_none )
                {
                    SvXMLUnitConverter::convertEnum( msBuffer, eKind, aXML_AnimationEffect_EnumMap );
                    mrExport.AddAttribute( XML_NAMESPACE_PRESENTATION, XML_EFFECT, msBuffer.makeStringAndClear() );
                }

                if( eDirection != ED_none )
                {
                    SvXMLUnitConverter::convertEnum( msBuffer, eDirection, aXML_AnimationDirection_EnumMap );
                    mrExport.AddAttribute( XML_NAMESPACE_PRESENTATION, XML_DIRECTION, msBuffer.makeStringAndClear() );
                }

                if( nStartScale != -1 )
                {
                    ::sax::Converter::convertPercent( msBuffer, nStartScale );
                    mrExport.AddAttribute( XML_NAMESPACE_PRESENTATION, XML_START_SCALE, msBuffer.makeStringAndClear() );
                }
            }

            if( nFound & Found::SPEED && eEffect != presentation::AnimationEffect_NONE )
            {
                if( eSpeed != presentation::AnimationSpeed_MEDIUM )
                {
                    SvXMLUnitConverter::convertEnum( msBuffer, eSpeed, aXML_AnimationSpeed_EnumMap );
                    mrExport.AddAttribute( XML_NAMESPACE_PRESENTATION, XML_SPEED, msBuffer.makeStringAndClear() );
                }
            }
        }

        if( eClickAction == presentation::ClickAction_PROGRAM ||
            eClickAction == presentation::ClickAction_BOOKMARK ||
            eClickAction == presentation::ClickAction_DOCUMENT )
        {
            if( eClickAction == presentation::ClickAction_BOOKMARK )
                msBuffer.append( '#' );

            msBuffer.append( aStrBookmark );
            mrExport.AddAttribute(XML_NAMESPACE_XLINK, XML_HREF, GetExport().GetRelativeReference(msBuffer.makeStringAndClear()) );
            mrExport.AddAttribute( XML_NAMESPACE_XLINK, XML_TYPE, XML_SIMPLE );
            mrExport.AddAttribute( XML_NAMESPACE_XLINK, XML_SHOW, XML_EMBED );
            mrExport.AddAttribute( XML_NAMESPACE_XLINK, XML_ACTUATE, XML_ONREQUEST );
        }

        if( ( nFound & Found::VERB ) && eClickAction == presentation::ClickAction_VERB )
        {
            msBuffer.append( nVerb );
            mrExport.AddAttribute(XML_NAMESPACE_PRESENTATION, XML_VERB, msBuffer.makeStringAndClear());
        }

        SvXMLElementExport aEventElemt(mrExport, XML_NAMESPACE_PRESENTATION, XML_EVENT_LISTENER, true, true);

        if( eClickAction == presentation::ClickAction_VANISH || eClickAction == presentation::ClickAction_SOUND )
        {
            if( ( nFound & Found::SOUNDURL ) && !aStrSoundURL.isEmpty() )
            {
                mrExport.AddAttribute(XML_NAMESPACE_XLINK, XML_HREF, GetExport().GetRelativeReference(aStrSoundURL) );
                mrExport.AddAttribute( XML_NAMESPACE_XLINK, XML_TYPE, XML_SIMPLE );
                mrExport.AddAttribute( XML_NAMESPACE_XLINK, XML_SHOW, XML_NEW );
                mrExport.AddAttribute( XML_NAMESPACE_XLINK, XML_ACTUATE, XML_ONREQUEST );
                if( nFound & Found::PLAYFULL && bPlayFull )
                    mrExport.AddAttribute( XML_NAMESPACE_PRESENTATION, XML_PLAY_FULL, XML_TRUE );

                SvXMLElementExport aElem( mrExport, XML_NAMESPACE_PRESENTATION, XML_SOUND, true, true );
            }
        }
    }
    else if( aClickEventType == gsStarBasic )
    {
        if( nFound & Found::MACRO )
        {
            SvXMLElementExport aEventsElemt(mrExport, XML_NAMESPACE_OFFICE, XML_EVENT_LISTENERS, true, true);

            mrExport.AddAttribute( XML_NAMESPACE_SCRIPT, XML_LANGUAGE,
                        mrExport.GetNamespaceMap().GetQNameByKey(
                            XML_NAMESPACE_OOO,
                            u"starbasic"_ustr ) );
            OUString aEventQName(
                mrExport.GetNamespaceMap().GetQNameByKey(
                        XML_NAMESPACE_DOM, u"click"_ustr ) );
            mrExport.AddAttribute( XML_NAMESPACE_SCRIPT, XML_EVENT_NAME, aEventQName );

            if( nFound & Found::LIBRARY )
            {
                const OUString& sLocation( GetXMLToken(
                    (aStrLibrary.equalsIgnoreAsciiCase("StarOffice") ||
                     aStrLibrary.equalsIgnoreAsciiCase("application") ) ? XML_APPLICATION
                                                                       : XML_DOCUMENT ) );
                mrExport.AddAttribute(XML_NAMESPACE_SCRIPT, XML_MACRO_NAME,
                    sLocation + ":" + aStrMacro);
            }
            else
            {
                mrExport.AddAttribute( XML_NAMESPACE_SCRIPT, XML_MACRO_NAME, aStrMacro );
            }

            SvXMLElementExport aEventElemt(mrExport, XML_NAMESPACE_SCRIPT, XML_EVENT_LISTENER, true, true);
        }
    }
    else if( aClickEventType == gsScript )
    {
        if( nFound & Found::MACRO )
        {
            SvXMLElementExport aEventsElemt(mrExport, XML_NAMESPACE_OFFICE, XML_EVENT_LISTENERS, true, true);

            mrExport.AddAttribute( XML_NAMESPACE_SCRIPT, XML_LANGUAGE, mrExport.GetNamespaceMap().GetQNameByKey(
                     XML_NAMESPACE_OOO, GetXMLToken(XML_SCRIPT) ) );
            OUString aEventQName(
                mrExport.GetNamespaceMap().GetQNameByKey(
                        XML_NAMESPACE_DOM, u"click"_ustr ) );
            mrExport.AddAttribute( XML_NAMESPACE_SCRIPT, XML_EVENT_NAME, aEventQName );
            mrExport.AddAttribute( XML_NAMESPACE_XLINK, XML_HREF, aStrMacro );
            mrExport.AddAttribute( XML_NAMESPACE_XLINK, XML_TYPE, u"simple"_ustr );

            SvXMLElementExport aEventElemt(mrExport, XML_NAMESPACE_SCRIPT, XML_EVENT_LISTENER, true, true);
        }
    }
}

/** #i68101# export shape Title and Description */
void XMLShapeExport::ImpExportDescription( const uno::Reference< drawing::XShape >& xShape )
{
    try
    {
        OUString aTitle;
        OUString aDescription;

        uno::Reference< beans::XPropertySet > xProps( xShape, uno::UNO_QUERY_THROW );
        xProps->getPropertyValue(u"Title"_ustr) >>= aTitle;
        xProps->getPropertyValue(u"Description"_ustr) >>= aDescription;

        if(!aTitle.isEmpty())
        {
            SvXMLElementExport aEventElemt(mrExport, XML_NAMESPACE_SVG, XML_TITLE, true, false);
            mrExport.Characters( aTitle );
        }

        if(!aDescription.isEmpty())
        {
            SvXMLElementExport aEventElemt(mrExport, XML_NAMESPACE_SVG, XML_DESC, true, false );
            mrExport.Characters( aDescription );
        }
    }
    catch( uno::Exception& )
    {
        DBG_UNHANDLED_EXCEPTION( "xmloff", "exporting Title and/or Description for shape" );
    }
}

void XMLShapeExport::ImpExportGroupShape( const uno::Reference< drawing::XShape >& xShape, XMLShapeExportFlags nFeatures, awt::Point* pRefPoint)
{
    uno::Reference< drawing::XShapes > xShapes(xShape, uno::UNO_QUERY);
    if(!(xShapes.is() && xShapes->getCount()))
        return;

    // write group shape
    bool bCreateNewline( (nFeatures & XMLShapeExportFlags::NO_WS) == XMLShapeExportFlags::NONE ); // #86116#/#92210#
    SvXMLElementExport aPGR(mrExport, XML_NAMESPACE_DRAW, XML_G, bCreateNewline, true);

    ImpExportDescription( xShape ); // #i68101#
    ImpExportEvents( xShape );
    ImpExportGluePoints( xShape );

    // #89764# if export of position is suppressed for group shape,
    // positions of contained objects should be written relative to
    // the upper left edge of the group.
    awt::Point aUpperLeft;

    if(!(nFeatures & XMLShapeExportFlags::POSITION))
    {
        nFeatures |= XMLShapeExportFlags::POSITION;
        aUpperLeft = xShape->getPosition();
        pRefPoint = &aUpperLeft;
    }

    // write members
    exportShapes( xShapes, nFeatures, pRefPoint );
}

void XMLShapeExport::ImpExportTextBoxShape(
    const uno::Reference< drawing::XShape >& xShape,
    XmlShapeType eShapeType, XMLShapeExportFlags nFeatures, awt::Point* pRefPoint)
{
    const uno::Reference< beans::XPropertySet > xPropSet(xShape, uno::UNO_QUERY);
    if(!xPropSet.is())
        return;

    // presentation attribute (if presentation)
    bool bIsPresShape(false);
    bool bIsEmptyPresObj(false);
    OUString aStr;

    switch(eShapeType)
    {
        case XmlShapeType::PresSubtitleShape:
        {
            aStr = GetXMLToken(XML_SUBTITLE);
            bIsPresShape = true;
            break;
        }
        case XmlShapeType::PresTitleTextShape:
        {
            aStr = GetXMLToken(XML_TITLE);
            bIsPresShape = true;
            break;
        }
        case XmlShapeType::PresOutlinerShape:
        {
            aStr = GetXMLToken(XML_PRESENTATION_OUTLINE);
            bIsPresShape = true;
            break;
        }
        case XmlShapeType::PresNotesShape:
        {
            aStr = GetXMLToken(XML_NOTES);
            bIsPresShape = true;
            break;
        }
        case XmlShapeType::PresHeaderShape:
        {
            aStr = GetXMLToken(XML_HEADER);
            bIsPresShape = true;
            break;
        }
        case XmlShapeType::PresFooterShape:
        {
            aStr = GetXMLToken(XML_FOOTER);
            bIsPresShape = true;
            break;
        }
        case XmlShapeType::PresSlideNumberShape:
        {
            aStr = GetXMLToken(XML_PAGE_NUMBER);
            bIsPresShape = true;
            break;
        }
        case XmlShapeType::PresDateTimeShape:
        {
            aStr = GetXMLToken(XML_DATE_TIME);
            bIsPresShape = true;
            break;
        }
        default:
            break;
    }

    // Transformation
    ImpExportNewTrans(xPropSet, nFeatures, pRefPoint);

    if(bIsPresShape)
        bIsEmptyPresObj = ImpExportPresentationAttributes( xPropSet, aStr );

    bool bCreateNewline( (nFeatures & XMLShapeExportFlags::NO_WS) == XMLShapeExportFlags::NONE ); // #86116#/#92210#
    SvXMLElementExport aElem( mrExport, XML_NAMESPACE_DRAW,
                              XML_FRAME, bCreateNewline, true );

    // evtl. corner radius?
    sal_Int32 nCornerRadius(0);
    xPropSet->getPropertyValue(u"CornerRadius"_ustr) >>= nCornerRadius;
    if(nCornerRadius)
    {
        OUStringBuffer sStringBuffer;
        mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer,
                nCornerRadius);
        mrExport.AddAttribute(XML_NAMESPACE_DRAW, XML_CORNER_RADIUS, sStringBuffer.makeStringAndClear());
    }

    {
        // write text-box
        SvXMLElementExport aOBJ(mrExport, XML_NAMESPACE_DRAW, XML_TEXT_BOX, true, true);
        if(!bIsEmptyPresObj)
            ImpExportText( xShape );
    }

    ImpExportDescription( xShape ); // #i68101#
    ImpExportEvents( xShape );
    ImpExportGluePoints( xShape );

}

void XMLShapeExport::ImpExportRectangleShape(
    const uno::Reference< drawing::XShape >& xShape,
    XMLShapeExportFlags nFeatures, css::awt::Point* pRefPoint)
{
    const uno::Reference< beans::XPropertySet > xPropSet(xShape, uno::UNO_QUERY);
    if(!xPropSet.is())
        return;

    // Transformation
    ImpExportNewTrans(xPropSet, nFeatures, pRefPoint);

    // evtl. corner radius?
    sal_Int32 nCornerRadius(0);
    xPropSet->getPropertyValue(u"CornerRadius"_ustr) >>= nCornerRadius;
    if(nCornerRadius)
    {
        OUStringBuffer sStringBuffer;
        mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer,
                nCornerRadius);
        mrExport.AddAttribute(XML_NAMESPACE_DRAW, XML_CORNER_RADIUS, sStringBuffer.makeStringAndClear());
    }

    // write rectangle
    bool bCreateNewline( (nFeatures & XMLShapeExportFlags::NO_WS) == XMLShapeExportFlags::NONE ); // #86116#/#92210#
    SvXMLElementExport aOBJ(mrExport, XML_NAMESPACE_DRAW, XML_RECT, bCreateNewline, true);

    ImpExportDescription( xShape ); // #i68101#
    ImpExportEvents( xShape );
    ImpExportGluePoints( xShape );
    ImpExportText( xShape );
}

void XMLShapeExport::ImpExportLineShape(
    const uno::Reference< drawing::XShape >& xShape,
    XMLShapeExportFlags nFeatures, awt::Point* pRefPoint)
{
    const uno::Reference< beans::XPropertySet > xPropSet(xShape, uno::UNO_QUERY);
    if(!xPropSet.is())
        return;

    OUString aStr;
    OUStringBuffer sStringBuffer;
    awt::Point aStart(0,0);
    awt::Point aEnd(1,1);

    // #85920# use 'Geometry' to get the points of the line
    // since this slot take anchor pos into account.

    // get matrix
    ::basegfx::B2DHomMatrix aMatrix;
    ImpExportNewTrans_GetB2DHomMatrix(aMatrix, xPropSet);

    // decompose and correct about pRefPoint
    ::basegfx::B2DTuple aTRScale;
    double fTRShear(0.0);
    double fTRRotate(0.0);
    ::basegfx::B2DTuple aTRTranslate;
    ImpExportNewTrans_DecomposeAndRefPoint(aMatrix, aTRScale, fTRShear, fTRRotate, aTRTranslate, pRefPoint);

    // create base position
    awt::Point aBasePosition(basegfx::fround(aTRTranslate.getX()),
                             basegfx::fround(aTRTranslate.getY()));

    if (xPropSet->getPropertySetInfo()->hasPropertyByName(u"Geometry"_ustr))
    {
        // get the two points
        uno::Any aAny(xPropSet->getPropertyValue(u"Geometry"_ustr));
        if (auto pSourcePolyPolygon
                = o3tl::tryAccess<drawing::PointSequenceSequence>(aAny))
        {
            if (pSourcePolyPolygon->getLength() > 0)
            {
                const drawing::PointSequence& rInnerSequence = (*pSourcePolyPolygon)[0];
                if (rInnerSequence.hasElements())
                {
                    const awt::Point& rPoint = rInnerSequence[0];
                    aStart = awt::Point(rPoint.X + aBasePosition.X, rPoint.Y + aBasePosition.Y);
                }
                if (rInnerSequence.getLength() > 1)
                {
                    const awt::Point& rPoint = rInnerSequence[1];
                    aEnd = awt::Point(rPoint.X + aBasePosition.X, rPoint.Y + aBasePosition.Y);
                }
            }
        }
    }

    if( nFeatures & XMLShapeExportFlags::X )
    {
        // svg: x1
        mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer,
                aStart.X);
        aStr = sStringBuffer.makeStringAndClear();
        mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_X1, aStr);
    }
    else
    {
        aEnd.X -= aStart.X;
    }

    if( nFeatures & XMLShapeExportFlags::Y )
    {
        // svg: y1
        mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer,
                aStart.Y);
        aStr = sStringBuffer.makeStringAndClear();
        mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_Y1, aStr);
    }
    else
    {
        aEnd.Y -= aStart.Y;
    }

    // svg: x2
    mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer,
            aEnd.X);
    aStr = sStringBuffer.makeStringAndClear();
    mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_X2, aStr);

    // svg: y2
    mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer,
            aEnd.Y);
    aStr = sStringBuffer.makeStringAndClear();
    mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_Y2, aStr);

    // write line
    bool bCreateNewline( (nFeatures & XMLShapeExportFlags::NO_WS) == XMLShapeExportFlags::NONE ); // #86116#/#92210#
    SvXMLElementExport aOBJ(mrExport, XML_NAMESPACE_DRAW, XML_LINE, bCreateNewline, true);

    ImpExportDescription( xShape ); // #i68101#
    ImpExportEvents( xShape );
    ImpExportGluePoints( xShape );
    ImpExportText( xShape );

}

void XMLShapeExport::ImpExportEllipseShape(
    const uno::Reference< drawing::XShape >& xShape,
    XMLShapeExportFlags nFeatures, awt::Point* pRefPoint)
{
    const uno::Reference< beans::XPropertySet > xPropSet(xShape, uno::UNO_QUERY);
    if(!xPropSet.is())
        return;

    // get size to decide between Circle and Ellipse
    awt::Size aSize = xShape->getSize();
    sal_Int32 nRx((aSize.Width + 1) / 2);
    sal_Int32 nRy((aSize.Height + 1) / 2);
    bool bCircle(nRx == nRy);

    // Transformation
    ImpExportNewTrans(xPropSet, nFeatures, pRefPoint);

    drawing::CircleKind eKind = drawing::CircleKind_FULL;
    xPropSet->getPropertyValue(u"CircleKind"_ustr) >>= eKind;
    if( eKind != drawing::CircleKind_FULL )
    {
        OUStringBuffer sStringBuffer;
        sal_Int32 nStartAngle = 0;
        sal_Int32 nEndAngle = 0;
        xPropSet->getPropertyValue(u"CircleStartAngle"_ustr) >>= nStartAngle;
        xPropSet->getPropertyValue(u"CircleEndAngle"_ustr) >>= nEndAngle;

        const double dStartAngle = nStartAngle / 100.0;
        const double dEndAngle = nEndAngle / 100.0;

        // export circle kind
        SvXMLUnitConverter::convertEnum( sStringBuffer, eKind, aXML_CircleKind_EnumMap );
        mrExport.AddAttribute(XML_NAMESPACE_DRAW, XML_KIND, sStringBuffer.makeStringAndClear() );

        // export start angle
        ::sax::Converter::convertDouble( sStringBuffer, dStartAngle );
        mrExport.AddAttribute(XML_NAMESPACE_DRAW, XML_START_ANGLE, sStringBuffer.makeStringAndClear() );

        // export end angle
        ::sax::Converter::convertDouble( sStringBuffer, dEndAngle );
        mrExport.AddAttribute(XML_NAMESPACE_DRAW, XML_END_ANGLE, sStringBuffer.makeStringAndClear() );
    }

    bool bCreateNewline( (nFeatures & XMLShapeExportFlags::NO_WS) == XMLShapeExportFlags::NONE ); // #86116#/#92210#

    // write ellipse or circle
    SvXMLElementExport aOBJ(mrExport, XML_NAMESPACE_DRAW,
                            bCircle ? XML_CIRCLE : XML_ELLIPSE,
                            bCreateNewline, true);

    ImpExportDescription( xShape ); // #i68101#
    ImpExportEvents( xShape );
    ImpExportGluePoints( xShape );
    ImpExportText( xShape );

}

void XMLShapeExport::ImpExportPolygonShape(
    const uno::Reference< drawing::XShape >& xShape,
    XmlShapeType eShapeType, XMLShapeExportFlags nFeatures, awt::Point* pRefPoint)
{
    const uno::Reference< beans::XPropertySet > xPropSet(xShape, uno::UNO_QUERY);
    if(!xPropSet.is())
        return;

    bool bBezier(eShapeType == XmlShapeType::DrawClosedBezierShape
        || eShapeType == XmlShapeType::DrawOpenBezierShape);

    // get matrix
    ::basegfx::B2DHomMatrix aMatrix;
    ImpExportNewTrans_GetB2DHomMatrix(aMatrix, xPropSet);

    // decompose and correct about pRefPoint
    ::basegfx::B2DTuple aTRScale;
    double fTRShear(0.0);
    double fTRRotate(0.0);
    ::basegfx::B2DTuple aTRTranslate;
    ImpExportNewTrans_DecomposeAndRefPoint(aMatrix, aTRScale, fTRShear, fTRRotate, aTRTranslate, pRefPoint);

    // use features and write
    ImpExportNewTrans_FeaturesAndWrite(aTRScale, fTRShear, fTRRotate, aTRTranslate, nFeatures);

    // create and export ViewBox
    awt::Size aSize(basegfx::fround<tools::Long>(aTRScale.getX()),
                    basegfx::fround<tools::Long>(aTRScale.getY()));
    SdXMLImExViewBox aViewBox(0, 0, aSize.Width, aSize.Height);
    mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_VIEWBOX, aViewBox.GetExportString());

    bool bCreateNewline( (nFeatures & XMLShapeExportFlags::NO_WS) == XMLShapeExportFlags::NONE ); // #86116#/#92210#

    // prepare name (with most used)
    enum ::xmloff::token::XMLTokenEnum eName(XML_PATH);

    uno::Any aAny( xPropSet->getPropertyValue(u"Geometry"_ustr) );
    basegfx::B2DPolyPolygon aPolyPolygon;

    // tdf#145240 the Any can contain PolyPolygonBezierCoords or PointSequenceSequence
    // (see OWN_ATTR_BASE_GEOMETRY in SvxShapePolyPolygon::getPropertyValueImpl),
    // so be more flexible in interpreting it. Try to access bezier first:
    {
        auto pSourcePolyPolygon = o3tl::tryAccess<drawing::PolyPolygonBezierCoords>(aAny);

        if(pSourcePolyPolygon && pSourcePolyPolygon->Coordinates.getLength())
        {
            aPolyPolygon = basegfx::utils::UnoPolyPolygonBezierCoordsToB2DPolyPolygon(*pSourcePolyPolygon);
        }
    }

    // if received no data, try to access point sequence second:
    if(0 == aPolyPolygon.count())
    {
        auto pSourcePolyPolygon = o3tl::tryAccess<drawing::PointSequenceSequence>(aAny);

        if(pSourcePolyPolygon)
        {
            aPolyPolygon = basegfx::utils::UnoPointSequenceSequenceToB2DPolyPolygon(*pSourcePolyPolygon);
        }
    }

    if(aPolyPolygon.count())
    {
        if(!bBezier && !aPolyPolygon.areControlPointsUsed() && 1 == aPolyPolygon.count())
        {
            // simple polygon shape, can be written as svg:points sequence
            const basegfx::B2DPolygon& aPolygon(aPolyPolygon.getB2DPolygon(0));
            const OUString aPointString(basegfx::utils::exportToSvgPoints(aPolygon));

            // write point array
            mrExport.AddAttribute(XML_NAMESPACE_DRAW, XML_POINTS, aPointString);

            // set name
            eName = aPolygon.isClosed() ? XML_POLYGON : XML_POLYLINE;
        }
        else
        {
            // complex polygon shape, write as svg:d
            const OUString aPolygonString(
                basegfx::utils::exportToSvgD(
                    aPolyPolygon,
                    true,       // bUseRelativeCoordinates
                    false,      // bDetectQuadraticBeziers: not used in old, but maybe activated now
                    true));     // bHandleRelativeNextPointCompatible

            // write point array
            mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_D, aPolygonString);
        }
    }

    // write object, but after attributes are added since this call will
    // consume all of these added attributes and the destructor will close the
    // scope. Also before text is added; this may add sub-scopes as needed
    SvXMLElementExport aOBJ(
        mrExport,
        XML_NAMESPACE_DRAW,
        eName,
        bCreateNewline,
        true);

    ImpExportDescription( xShape ); // #i68101#
    ImpExportEvents( xShape );
    ImpExportGluePoints( xShape );
    ImpExportText( xShape );

}

namespace
{

OUString getNameFromStreamURL(std::u16string_view rURL)
{
    static constexpr std::u16string_view sPackageURL(u"vnd.sun.star.Package:");

    OUString sResult;

    if (o3tl::starts_with(rURL, sPackageURL))
    {
        std::u16string_view sRequestedName = rURL.substr(sPackageURL.size());
        size_t nLastIndex = sRequestedName.rfind('/');
        if (nLastIndex != std::u16string_view::npos && nLastIndex + 1 < sRequestedName.size())
            sRequestedName = sRequestedName.substr(nLastIndex + 1);
        nLastIndex = sRequestedName.rfind('.');
        if (nLastIndex != std::u16string_view::npos)
            sRequestedName = sRequestedName.substr(0, nLastIndex);
        if (!sRequestedName.empty())
            sResult = sRequestedName;
    }

    return sResult;
}

} // end anonymous namespace

void XMLShapeExport::ImpExportGraphicObjectShape(
    const uno::Reference< drawing::XShape >& xShape,
    XmlShapeType eShapeType, XMLShapeExportFlags nFeatures, awt::Point* pRefPoint)
{
    const uno::Reference< beans::XPropertySet > xPropSet(xShape, uno::UNO_QUERY);
    if(!xPropSet.is())
        return;

    bool bIsEmptyPresObj = false;

    // Transformation
    ImpExportNewTrans(xPropSet, nFeatures, pRefPoint);

    if(eShapeType == XmlShapeType::PresGraphicObjectShape)
        bIsEmptyPresObj = ImpExportPresentationAttributes( xPropSet, GetXMLToken(XML_GRAPHIC) );

    bool bCreateNewline( (nFeatures & XMLShapeExportFlags::NO_WS) == XMLShapeExportFlags::NONE ); // #86116#/#92210#
    SvXMLElementExport aElem( mrExport, XML_NAMESPACE_DRAW,
                              XML_FRAME, bCreateNewline, true );

    if (!bIsEmptyPresObj)
    {
        uno::Reference<graphic::XGraphic> xGraphic;
        OUString sOutMimeType;

        {
            OUString aStreamURL;
            xPropSet->getPropertyValue(u"GraphicStreamURL"_ustr) >>= aStreamURL;
            OUString sRequestedName = getNameFromStreamURL(aStreamURL);

            xPropSet->getPropertyValue(u"Graphic"_ustr) >>= xGraphic;

            OUString sInternalURL;

            if (xGraphic.is())
                sInternalURL = mrExport.AddEmbeddedXGraphic(xGraphic, sOutMimeType, sRequestedName);

            if (!sInternalURL.isEmpty())
            {
                // apply possible changed stream URL to embedded image object
                if (!sRequestedName.isEmpty())
                {
                    OUString newStreamURL = u"vnd.sun.star.Package:"_ustr;
                    if (sInternalURL[0] == '#')
                    {
                        newStreamURL += sInternalURL.subView(1, sInternalURL.getLength() - 1);
                    }
                    else
                    {
                        newStreamURL += sInternalURL;
                    }

                    if (newStreamURL != aStreamURL)
                    {
                        xPropSet->setPropertyValue(u"GraphicStreamURL"_ustr, uno::Any(newStreamURL));
                    }
                }

                mrExport.AddAttribute(XML_NAMESPACE_XLINK, XML_HREF, sInternalURL);
                mrExport.AddAttribute(XML_NAMESPACE_XLINK, XML_TYPE, XML_SIMPLE);
                mrExport.AddAttribute(XML_NAMESPACE_XLINK, XML_SHOW, XML_EMBED);
                mrExport.AddAttribute(XML_NAMESPACE_XLINK, XML_ACTUATE, XML_ONLOAD);
            }
        }

        {
            if (sOutMimeType.isEmpty())
            {
                GetExport().GetGraphicMimeTypeFromStream(xGraphic, sOutMimeType);
            }
            if (GetExport().getSaneDefaultVersion() > SvtSaveOptions::ODFSVER_012)
            {
                if (!sOutMimeType.isEmpty())
                {   // ODF 1.3 OFFICE-3943
                    GetExport().AddAttribute(
                        SvtSaveOptions::ODFSVER_013 <= GetExport().getSaneDefaultVersion()
                            ? XML_NAMESPACE_DRAW
                            : XML_NAMESPACE_LO_EXT,
                        u"mime-type"_ustr, sOutMimeType);
                }
            }

            if (sOutMimeType == "application/pdf")
            {
                Graphic aGraphic(xGraphic);
                sal_Int32 nPage = aGraphic.getPageNumber();
                if (nPage >= 0)
                    GetExport().AddAttribute(XML_NAMESPACE_LO_EXT, XML_PAGE_NUMBER,
                                             OUString::number(nPage));
            }

            SvXMLElementExport aElement(mrExport, XML_NAMESPACE_DRAW, XML_IMAGE, true, true);

            // optional office:binary-data
            if (xGraphic.is())
            {
                mrExport.AddEmbeddedXGraphicAsBase64(xGraphic);
            }
            if (!bIsEmptyPresObj)
                ImpExportText(xShape);
        }

        //Resolves: fdo#62461 put preferred image first above, followed by
        //fallback here
        if (!bIsEmptyPresObj
            && officecfg::Office::Common::Save::Graphic::AddReplacementImages::get())
        {
            uno::Reference<graphic::XGraphic> xReplacementGraphic;
            xPropSet->getPropertyValue(u"ReplacementGraphic"_ustr) >>= xReplacementGraphic;

            // If there is no url, then the graphic is empty
            if (xReplacementGraphic.is())
            {
                OUString aMimeType;
                const OUString aHref = mrExport.AddEmbeddedXGraphic(xReplacementGraphic, aMimeType);

                if (aMimeType.isEmpty())
                    mrExport.GetGraphicMimeTypeFromStream(xReplacementGraphic, aMimeType);

                if (!aHref.isEmpty())
                {
                    mrExport.AddAttribute(XML_NAMESPACE_XLINK, XML_HREF, aHref);
                    mrExport.AddAttribute(XML_NAMESPACE_XLINK, XML_TYPE, XML_SIMPLE );
                    mrExport.AddAttribute(XML_NAMESPACE_XLINK, XML_SHOW, XML_EMBED );
                    mrExport.AddAttribute(XML_NAMESPACE_XLINK, XML_ACTUATE, XML_ONLOAD );
                }

                if (!aMimeType.isEmpty() && GetExport().getSaneDefaultVersion() > SvtSaveOptions::ODFSVER_012)
                {   // ODF 1.3 OFFICE-3943
                    mrExport.AddAttribute(
                        SvtSaveOptions::ODFSVER_013 <= GetExport().getSaneDefaultVersion()
                            ? XML_NAMESPACE_DRAW
                            : XML_NAMESPACE_LO_EXT,
                        u"mime-type"_ustr, aMimeType);
                }

                SvXMLElementExport aElement(mrExport, XML_NAMESPACE_DRAW, XML_IMAGE, true, true);

                // optional office:binary-data
                mrExport.AddEmbeddedXGraphicAsBase64(xReplacementGraphic);
            }
        }
    }

    ImpExportEvents( xShape );
    ImpExportGluePoints( xShape );

    // image map
    GetExport().GetImageMapExport().Export( xPropSet );
    ImpExportDescription( xShape ); // #i68101#

    // Signature Line, QR Code - needs to be after the images!
    if (GetExport().getSaneDefaultVersion() & SvtSaveOptions::ODFSVER_EXTENDED)
    {
        ImpExportSignatureLine(xShape);
        ImpExportQRCode(xShape);
    }
}

void XMLShapeExport::ImpExportChartShape(
    const uno::Reference< drawing::XShape >& xShape,
    XmlShapeType eShapeType, XMLShapeExportFlags nFeatures, awt::Point* pRefPoint,
    comphelper::AttributeList* pAttrList )
{
    ImpExportOLE2Shape( xShape, eShapeType, nFeatures, pRefPoint, pAttrList );
}

void XMLShapeExport::ImpExportControlShape(
    const uno::Reference< drawing::XShape >& xShape,
    XMLShapeExportFlags nFeatures, awt::Point* pRefPoint)
{
    const uno::Reference< beans::XPropertySet > xPropSet(xShape, uno::UNO_QUERY);
    if(xPropSet.is())
    {
        // Transformation
        ImpExportNewTrans(xPropSet, nFeatures, pRefPoint);
    }

    uno::Reference< drawing::XControlShape > xControl( xShape, uno::UNO_QUERY );
    SAL_WARN_IF( !xControl.is(), "xmloff", "Control shape is not supporting XControlShape" );
    if( xControl.is() )
    {
        uno::Reference< beans::XPropertySet > xControlModel( xControl->getControl(), uno::UNO_QUERY );
        SAL_WARN_IF( !xControlModel.is(), "xmloff", "Control shape has not XControlModel" );
        if( xControlModel.is() )
        {
            OUString sControlId = mrExport.GetFormExport()->getControlId( xControlModel );
            mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_CONTROL, sControlId );
        }
    }

    bool bCreateNewline( (nFeatures & XMLShapeExportFlags::NO_WS) == XMLShapeExportFlags::NONE ); // #86116#/#92210#
    SvXMLElementExport aOBJ(mrExport, XML_NAMESPACE_DRAW, XML_CONTROL, bCreateNewline, true);

    ImpExportDescription( xShape ); // #i68101#
}

void XMLShapeExport::ImpExportConnectorShape(
    const uno::Reference< drawing::XShape >& xShape,
    XMLShapeExportFlags nFeatures /* = SEF_DEFAULT */, awt::Point* pRefPoint /* = NULL */)
{
    uno::Reference< beans::XPropertySet > xProps( xShape, uno::UNO_QUERY );

    OUString aStr;
    OUStringBuffer sStringBuffer;

    // export connection kind
    drawing::ConnectorType eType = drawing::ConnectorType_STANDARD;
    uno::Any aAny = xProps->getPropertyValue(u"EdgeKind"_ustr);
    aAny >>= eType;

    if( eType != drawing::ConnectorType_STANDARD )
    {
        SvXMLUnitConverter::convertEnum( sStringBuffer, eType, aXML_ConnectionKind_EnumMap );
        aStr = sStringBuffer.makeStringAndClear();
        mrExport.AddAttribute(XML_NAMESPACE_DRAW, XML_TYPE, aStr);
    }

    // export line skew
    sal_Int32 nDelta1 = 0, nDelta2 = 0, nDelta3 = 0;

    aAny = xProps->getPropertyValue(u"EdgeLine1Delta"_ustr);
    aAny >>= nDelta1;
    aAny = xProps->getPropertyValue(u"EdgeLine2Delta"_ustr);
    aAny >>= nDelta2;
    aAny = xProps->getPropertyValue(u"EdgeLine3Delta"_ustr);
    aAny >>= nDelta3;

    if( nDelta1 != 0 || nDelta2 != 0 || nDelta3 != 0 )
    {
        mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer,
                nDelta1);
        if( nDelta2 != 0 || nDelta3 != 0 )
        {
            sStringBuffer.append( ' ' );
            mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer,
                    nDelta2);
            if( nDelta3 != 0 )
            {
                sStringBuffer.append( ' ' );
                mrExport.GetMM100UnitConverter().convertMeasureToXML(
                        sStringBuffer, nDelta3);
            }
        }

        aStr = sStringBuffer.makeStringAndClear();
        mrExport.AddAttribute(XML_NAMESPACE_DRAW, XML_LINE_SKEW, aStr);
    }

    // export start and end point
    awt::Point aStart(0,0);
    awt::Point aEnd(1,1);

    /* Get <StartPositionInHoriL2R> and
       <EndPositionInHoriL2R>, if they exist and if the document is exported
       into the OpenOffice.org file format.
       These properties only exist at service css::text::Shape - the
       Writer UNO service for shapes.
       This code is needed, because the positioning attributes in the
       OpenOffice.org file format are given in horizontal left-to-right layout
       regardless the layout direction the shape is in. In the OASIS Open Office
       file format the positioning attributes are correctly given in the layout
       direction the shape is in. Thus, this code provides the conversion from
       the OASIS Open Office file format to the OpenOffice.org file format. (#i36248#)
    */
    if ( !( GetExport().getExportFlags() & SvXMLExportFlags::OASIS ) &&
         xProps->getPropertySetInfo()->hasPropertyByName(u"StartPositionInHoriL2R"_ustr) &&
         xProps->getPropertySetInfo()->hasPropertyByName(u"EndPositionInHoriL2R"_ustr) )
    {
        xProps->getPropertyValue(u"StartPositionInHoriL2R"_ustr) >>= aStart;
        xProps->getPropertyValue(u"EndPositionInHoriL2R"_ustr) >>= aEnd;
    }
    else
    {
        xProps->getPropertyValue(u"StartPosition"_ustr) >>= aStart;
        xProps->getPropertyValue(u"EndPosition"_ustr) >>= aEnd;
    }

    if( pRefPoint )
    {
        aStart.X -= pRefPoint->X;
        aStart.Y -= pRefPoint->Y;
        aEnd.X -= pRefPoint->X;
        aEnd.Y -= pRefPoint->Y;
    }

    if( nFeatures & XMLShapeExportFlags::X )
    {
        // svg: x1
        mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer,
                aStart.X);
        aStr = sStringBuffer.makeStringAndClear();
        mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_X1, aStr);
    }
    else
    {
        aEnd.X -= aStart.X;
    }

    if( nFeatures & XMLShapeExportFlags::Y )
    {
        // svg: y1
        mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer,
                aStart.Y);
        aStr = sStringBuffer.makeStringAndClear();
        mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_Y1, aStr);
    }
    else
    {
        aEnd.Y -= aStart.Y;
    }

    // svg: x2
    mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer, aEnd.X);
    aStr = sStringBuffer.makeStringAndClear();
    mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_X2, aStr);

    // svg: y2
    mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer, aEnd.Y);
    aStr = sStringBuffer.makeStringAndClear();
    mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_Y2, aStr);

    // #i39320#
    uno::Reference< uno::XInterface > xRefS;
    uno::Reference< uno::XInterface > xRefE;

    // export start connection
    xProps->getPropertyValue(u"StartShape"_ustr) >>= xRefS;
    if( xRefS.is() )
    {
        const OUString& rShapeId = mrExport.getInterfaceToIdentifierMapper().getIdentifier( xRefS );
        mrExport.AddAttribute(XML_NAMESPACE_DRAW, XML_START_SHAPE, rShapeId);

        aAny = xProps->getPropertyValue(u"StartGluePointIndex"_ustr);
        sal_Int32 nGluePointId = 0;
        if( aAny >>= nGluePointId )
        {
            if( nGluePointId != -1 )
            {
                mrExport.AddAttribute(XML_NAMESPACE_DRAW, XML_START_GLUE_POINT, OUString::number( nGluePointId ));
            }
        }
    }

    // export end connection
    xProps->getPropertyValue(u"EndShape"_ustr) >>= xRefE;
    if( xRefE.is() )
    {
        const OUString& rShapeId = mrExport.getInterfaceToIdentifierMapper().getIdentifier( xRefE );
        mrExport.AddAttribute(XML_NAMESPACE_DRAW, XML_END_SHAPE, rShapeId);

        aAny = xProps->getPropertyValue(u"EndGluePointIndex"_ustr);
        sal_Int32 nGluePointId = 0;
        if( aAny >>= nGluePointId )
        {
            if( nGluePointId != -1 )
            {
                mrExport.AddAttribute(XML_NAMESPACE_DRAW, XML_END_GLUE_POINT, OUString::number( nGluePointId ));
            }
        }
    }

    // get PolygonBezier
    aAny = xProps->getPropertyValue(u"PolyPolygonBezier"_ustr);
    auto pSourcePolyPolygon = o3tl::tryAccess<drawing::PolyPolygonBezierCoords>(aAny);
    if(pSourcePolyPolygon && pSourcePolyPolygon->Coordinates.getLength())
    {
        const basegfx::B2DPolyPolygon aPolyPolygon(
            basegfx::utils::UnoPolyPolygonBezierCoordsToB2DPolyPolygon(
                *pSourcePolyPolygon));
        const OUString aPolygonString(
            basegfx::utils::exportToSvgD(
                aPolyPolygon,
                true,           // bUseRelativeCoordinates
                false,          // bDetectQuadraticBeziers: not used in old, but maybe activated now
                true));         // bHandleRelativeNextPointCompatible

        // write point array
        mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_D, aPolygonString);
    }

    // get matrix
    ::basegfx::B2DHomMatrix aMatrix;
    ImpExportNewTrans_GetB2DHomMatrix(aMatrix, xProps);

    // decompose and correct about pRefPoint
    ::basegfx::B2DTuple aTRScale;
    double fTRShear(0.0);
    double fTRRotate(0.0);
    ::basegfx::B2DTuple aTRTranslate;
    ImpExportNewTrans_DecomposeAndRefPoint(aMatrix, aTRScale, fTRShear,
            fTRRotate, aTRTranslate, pRefPoint);

    // fdo#49678: create and export ViewBox
    awt::Size aSize(basegfx::fround<tools::Long>(aTRScale.getX()),
                    basegfx::fround<tools::Long>(aTRScale.getY()));
    SdXMLImExViewBox aViewBox(0, 0, aSize.Width, aSize.Height);
    mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_VIEWBOX, aViewBox.GetExportString());

    // write connector shape. Add Export later.
    bool bCreateNewline( (nFeatures & XMLShapeExportFlags::NO_WS) == XMLShapeExportFlags::NONE ); // #86116#/#92210#
    SvXMLElementExport aOBJ(mrExport, XML_NAMESPACE_DRAW, XML_CONNECTOR, bCreateNewline, true);

    ImpExportDescription( xShape ); // #i68101#
    ImpExportEvents( xShape );
    ImpExportGluePoints( xShape );
    ImpExportText( xShape );
}

void XMLShapeExport::ImpExportMeasureShape(
    const uno::Reference< drawing::XShape >& xShape,
    XMLShapeExportFlags nFeatures /* = SEF_DEFAULT */, awt::Point const * pRefPoint /* = NULL */)
{
    uno::Reference< beans::XPropertySet > xProps( xShape, uno::UNO_QUERY );

    OUString aStr;
    OUStringBuffer sStringBuffer;

    // export start and end point
    awt::Point aStart(0,0);
    awt::Point aEnd(1,1);

    /* Get <StartPositionInHoriL2R> and
       <EndPositionInHoriL2R>, if they exist and if the document is exported
       into the OpenOffice.org file format.
       These properties only exist at service css::text::Shape - the
       Writer UNO service for shapes.
       This code is needed, because the positioning attributes in the
       OpenOffice.org file format are given in horizontal left-to-right layout
       regardless the layout direction the shape is in. In the OASIS Open Office
       file format the positioning attributes are correctly given in the layout
       direction the shape is in. Thus, this code provides the conversion from
       the OASIS Open Office file format to the OpenOffice.org file format. (#i36248#)
    */
    if ( !( GetExport().getExportFlags() & SvXMLExportFlags::OASIS ) &&
         xProps->getPropertySetInfo()->hasPropertyByName(u"StartPositionInHoriL2R"_ustr) &&
         xProps->getPropertySetInfo()->hasPropertyByName(u"EndPositionInHoriL2R"_ustr) )
    {
        xProps->getPropertyValue(u"StartPositionInHoriL2R"_ustr) >>= aStart;
        xProps->getPropertyValue(u"EndPositionInHoriL2R"_ustr) >>= aEnd;
    }
    else
    {
        xProps->getPropertyValue(u"StartPosition"_ustr) >>= aStart;
        xProps->getPropertyValue(u"EndPosition"_ustr) >>= aEnd;
    }

    if( pRefPoint )
    {
        aStart.X -= pRefPoint->X;
        aStart.Y -= pRefPoint->Y;
        aEnd.X -= pRefPoint->X;
        aEnd.Y -= pRefPoint->Y;
    }

    if( nFeatures & XMLShapeExportFlags::X )
    {
        // svg: x1
        mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer,
                aStart.X);
        aStr = sStringBuffer.makeStringAndClear();
        mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_X1, aStr);
    }
    else
    {
        aEnd.X -= aStart.X;
    }

    if( nFeatures & XMLShapeExportFlags::Y )
    {
        // svg: y1
        mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer,
                aStart.Y);
        aStr = sStringBuffer.makeStringAndClear();
        mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_Y1, aStr);
    }
    else
    {
        aEnd.Y -= aStart.Y;
    }

    // svg: x2
    mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer, aEnd.X);
    aStr = sStringBuffer.makeStringAndClear();
    mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_X2, aStr);

    // svg: y2
    mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer, aEnd.Y);
    aStr = sStringBuffer.makeStringAndClear();
    mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_Y2, aStr);

    // write measure shape
    bool bCreateNewline( (nFeatures & XMLShapeExportFlags::NO_WS) == XMLShapeExportFlags::NONE ); // #86116#/#92210#
    SvXMLElementExport aOBJ(mrExport, XML_NAMESPACE_DRAW, XML_MEASURE, bCreateNewline, true);

    ImpExportDescription( xShape ); // #i68101#
    ImpExportEvents( xShape );
    ImpExportGluePoints( xShape );

    uno::Reference< text::XText > xText( xShape, uno::UNO_QUERY );
    if( xText.is() )
        mrExport.GetTextParagraphExport()->exportText( xText );
}

void XMLShapeExport::ImpExportOLE2Shape(
    const uno::Reference< drawing::XShape >& xShape,
    XmlShapeType eShapeType, XMLShapeExportFlags nFeatures /* = SEF_DEFAULT */, awt::Point* pRefPoint /* = NULL */,
    comphelper::AttributeList* pAttrList /* = NULL */ )
{
    uno::Reference< beans::XPropertySet > xPropSet(xShape, uno::UNO_QUERY);
    uno::Reference< container::XNamed > xNamed(xShape, uno::UNO_QUERY);

    SAL_WARN_IF( !xPropSet.is() || !xNamed.is(), "xmloff", "ole shape is not implementing needed interfaces");
    if(!(xPropSet.is() && xNamed.is()))
        return;

    // Transformation
    ImpExportNewTrans(xPropSet, nFeatures, pRefPoint);

    bool bIsEmptyPresObj = false;

    // presentation settings
    if(eShapeType == XmlShapeType::PresOLE2Shape)
        bIsEmptyPresObj = ImpExportPresentationAttributes( xPropSet, GetXMLToken(XML_OBJECT) );
    else if(eShapeType == XmlShapeType::PresChartShape)
        bIsEmptyPresObj = ImpExportPresentationAttributes( xPropSet, GetXMLToken(XML_CHART) );
    else if(eShapeType == XmlShapeType::PresSheetShape)
        bIsEmptyPresObj = ImpExportPresentationAttributes( xPropSet, GetXMLToken(XML_TABLE) );

    bool bCreateNewline( (nFeatures & XMLShapeExportFlags::NO_WS) == XMLShapeExportFlags::NONE ); // #86116#/#92210#
    bool bExportEmbedded(mrExport.getExportFlags() & SvXMLExportFlags::EMBEDDED);
    OUString sPersistName;
    SvXMLElementExport aElement( mrExport, XML_NAMESPACE_DRAW,
                              XML_FRAME, bCreateNewline, true );

    if (!bIsEmptyPresObj)
    {
        if (pAttrList)
        {
            mrExport.AddAttributeList(pAttrList);
        }

        OUString sClassId;
        OUString sURL;
        bool bInternal = false;
        xPropSet->getPropertyValue(u"IsInternal"_ustr) >>= bInternal;

        {

            if ( bInternal )
            {
                // OOo internal links have no storage persistence, URL is stored in the XML file
                // the result LinkURL is empty in case the object is not a link
                xPropSet->getPropertyValue(u"LinkURL"_ustr) >>= sURL;
            }

            xPropSet->getPropertyValue(u"PersistName"_ustr) >>= sPersistName;
            if ( sURL.isEmpty() )
            {
                if( !sPersistName.isEmpty() )
                {
                    sURL = "vnd.sun.star.EmbeddedObject:" + sPersistName;
                }
            }

            if( !bInternal )
                xPropSet->getPropertyValue(u"CLSID"_ustr) >>= sClassId;

            if( !sClassId.isEmpty() )
                mrExport.AddAttribute(XML_NAMESPACE_DRAW, XML_CLASS_ID, sClassId );

            if(!bExportEmbedded)
            {
                // xlink:href
                if( !sURL.isEmpty() )
                {
                    // #96717# in theorie, if we don't have a URL we shouldn't even
                    // export this OLE shape. But practically it's too risky right now
                    // to change this so we better dispose this on load
                    sURL = mrExport.AddEmbeddedObject( sURL );

                    mrExport.AddAttribute(XML_NAMESPACE_XLINK, XML_HREF, sURL );
                    mrExport.AddAttribute( XML_NAMESPACE_XLINK, XML_TYPE, XML_SIMPLE );
                    mrExport.AddAttribute( XML_NAMESPACE_XLINK, XML_SHOW, XML_EMBED );
                    mrExport.AddAttribute( XML_NAMESPACE_XLINK, XML_ACTUATE, XML_ONLOAD );
                }
                else
                {
                    // tdf#153179 Export the preview graphic of the object if the object is missing.
                    uno::Reference<graphic::XGraphic> xGraphic;
                    xPropSet->getPropertyValue(u"Graphic"_ustr) >>= xGraphic;

                    if (xGraphic.is())
                    {
                        OUString aMimeType;
                        const OUString aHref = mrExport.AddEmbeddedXGraphic(xGraphic, aMimeType);

                        if (aMimeType.isEmpty())
                            mrExport.GetGraphicMimeTypeFromStream(xGraphic, aMimeType);

                        if (!aHref.isEmpty())
                        {
                            mrExport.AddAttribute(XML_NAMESPACE_XLINK, XML_HREF, aHref);
                            mrExport.AddAttribute(XML_NAMESPACE_XLINK, XML_TYPE, XML_SIMPLE);
                            mrExport.AddAttribute(XML_NAMESPACE_XLINK, XML_SHOW, XML_EMBED);
                            mrExport.AddAttribute(XML_NAMESPACE_XLINK, XML_ACTUATE, XML_ONLOAD);
                        }

                        if (!aMimeType.isEmpty()
                            && GetExport().getSaneDefaultVersion() > SvtSaveOptions::ODFSVER_012)
                        { // ODF 1.3 OFFICE-3943
                            mrExport.AddAttribute(SvtSaveOptions::ODFSVER_013
                                                          <= GetExport().getSaneDefaultVersion()
                                                      ? XML_NAMESPACE_DRAW
                                                      : XML_NAMESPACE_LO_EXT,
                                                  u"mime-type"_ustr, aMimeType);
                        }

                        SvXMLElementExport aImageElem(mrExport, XML_NAMESPACE_DRAW, XML_IMAGE, true,
                                                      true);

                        // optional office:binary-data
                        mrExport.AddEmbeddedXGraphicAsBase64(xGraphic);

                        ImpExportEvents(xShape);
                        ImpExportGluePoints(xShape);
                        ImpExportDescription(xShape);

                        return;
                    }
                }
            }
        }

        enum XMLTokenEnum eElem = sClassId.isEmpty() ? XML_OBJECT : XML_OBJECT_OLE ;
        SvXMLElementExport aElem( mrExport, XML_NAMESPACE_DRAW, eElem, true, true );

        // tdf#112547 export text as child of draw:object, where import expects it
        if (!bIsEmptyPresObj && supportsText(eShapeType))
        {
            // #i118485# Add text export, the draw OLE shape allows text now
            ImpExportText( xShape, TextPNS::EXTENSION );
        }

        if(bExportEmbedded && !bIsEmptyPresObj)
        {
            if(bInternal)
            {
                // embedded XML
                uno::Reference< lang::XComponent > xComp;
                xPropSet->getPropertyValue(u"Model"_ustr) >>= xComp;
                SAL_WARN_IF( !xComp.is(), "xmloff", "no xModel for own OLE format" );
                mrExport.ExportEmbeddedOwnObject( xComp );
            }
            else
            {
                // embed as Base64
                // this is an alien object ( currently MSOLE is the only supported type of such objects )
                // in case it is not an OASIS format the object should be asked to store replacement image if possible

                OUString sURLRequest( sURL );
                if ( !( mrExport.getExportFlags() & SvXMLExportFlags::OASIS ) )
                    sURLRequest +=  "?oasis=false";
                mrExport.AddEmbeddedObjectAsBase64( sURLRequest );
            }
        }
    }
    if( !bIsEmptyPresObj )
    {
        OUString sURL = XML_EMBEDDEDOBJECTGRAPHIC_URL_BASE + sPersistName;
        if( !bExportEmbedded )
        {
            sURL = GetExport().AddEmbeddedObject( sURL );
            mrExport.AddAttribute(XML_NAMESPACE_XLINK, XML_HREF, sURL );
            mrExport.AddAttribute( XML_NAMESPACE_XLINK, XML_TYPE, XML_SIMPLE );
            mrExport.AddAttribute( XML_NAMESPACE_XLINK, XML_SHOW, XML_EMBED );
            mrExport.AddAttribute( XML_NAMESPACE_XLINK, XML_ACTUATE, XML_ONLOAD );
        }

        SvXMLElementExport aElem( GetExport(), XML_NAMESPACE_DRAW,
                                  XML_IMAGE, false, true );

        if( bExportEmbedded )
            GetExport().AddEmbeddedObjectAsBase64( sURL );
    }

    ImpExportEvents( xShape );
    ImpExportGluePoints( xShape );
    ImpExportDescription( xShape ); // #i68101#

}

void XMLShapeExport::ImpExportPageShape(
    const uno::Reference< drawing::XShape >& xShape,
    XmlShapeType eShapeType, XMLShapeExportFlags nFeatures /* = SEF_DEFAULT */, awt::Point* pRefPoint /* = NULL */)
{
    const uno::Reference< beans::XPropertySet > xPropSet(xShape, uno::UNO_QUERY);
    if(!xPropSet.is())
        return;

    // #86163# Transformation
    ImpExportNewTrans(xPropSet, nFeatures, pRefPoint);

    // export page number used for this page
    uno::Reference< beans::XPropertySetInfo > xPropSetInfo( xPropSet->getPropertySetInfo() );
    static constexpr OUString aPageNumberStr(u"PageNumber"_ustr);
    if( xPropSetInfo.is() && xPropSetInfo->hasPropertyByName(aPageNumberStr))
    {
        sal_Int32 nPageNumber = 0;
        xPropSet->getPropertyValue(aPageNumberStr) >>= nPageNumber;
        if( nPageNumber )
            mrExport.AddAttribute(XML_NAMESPACE_DRAW, XML_PAGE_NUMBER, OUString::number(nPageNumber));
    }

    // a presentation page shape, normally used on notes pages only. If
    // it is used not as presentation shape, it may have been created with
    // copy-paste exchange between draw and impress (this IS possible...)
    if(eShapeType == XmlShapeType::PresPageShape)
    {
        mrExport.AddAttribute(XML_NAMESPACE_PRESENTATION, XML_CLASS,
                             XML_PAGE);
    }

    // write Page shape
    bool bCreateNewline( (nFeatures & XMLShapeExportFlags::NO_WS) == XMLShapeExportFlags::NONE ); // #86116#/#92210#
    SvXMLElementExport aOBJ(mrExport, XML_NAMESPACE_DRAW, XML_PAGE_THUMBNAIL, bCreateNewline, true);
}

void XMLShapeExport::ImpExportCaptionShape(
    const uno::Reference< drawing::XShape >& xShape,
    XMLShapeExportFlags nFeatures /* = SEF_DEFAULT */, awt::Point* pRefPoint /* = NULL */)
{
    const uno::Reference< beans::XPropertySet > xPropSet(xShape, uno::UNO_QUERY);
    if(!xPropSet.is())
        return;

    // Transformation
    ImpExportNewTrans(xPropSet, nFeatures, pRefPoint);

    // evtl. corner radius?
    sal_Int32 nCornerRadius(0);
    xPropSet->getPropertyValue(u"CornerRadius"_ustr) >>= nCornerRadius;
    if(nCornerRadius)
    {
        OUStringBuffer sStringBuffer;
        mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer,
                nCornerRadius);
        mrExport.AddAttribute(XML_NAMESPACE_DRAW, XML_CORNER_RADIUS, sStringBuffer.makeStringAndClear());
    }

    awt::Point aCaptionPoint;
    xPropSet->getPropertyValue(u"CaptionPoint"_ustr) >>= aCaptionPoint;

    mrExport.GetMM100UnitConverter().convertMeasureToXML(msBuffer,
            aCaptionPoint.X);
    mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_CAPTION_POINT_X, msBuffer.makeStringAndClear() );
    mrExport.GetMM100UnitConverter().convertMeasureToXML(msBuffer,
            aCaptionPoint.Y);
    mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_CAPTION_POINT_Y, msBuffer.makeStringAndClear() );

    // write Caption shape. Add export later.
    bool bCreateNewline( (nFeatures & XMLShapeExportFlags::NO_WS) == XMLShapeExportFlags::NONE ); // #86116#/#92210#
    bool bAnnotation( (nFeatures & XMLShapeExportFlags::ANNOTATION) == XMLShapeExportFlags::ANNOTATION );

    SvXMLElementExport aObj( mrExport,
                             (bAnnotation ? XML_NAMESPACE_OFFICE
                                           : XML_NAMESPACE_DRAW),
                             (bAnnotation ? XML_ANNOTATION : XML_CAPTION),
                             bCreateNewline, true );

    ImpExportDescription( xShape ); // #i68101#
    ImpExportEvents( xShape );
    ImpExportGluePoints( xShape );
    if( bAnnotation )
        mrExport.exportAnnotationMeta( xShape );
    ImpExportText( xShape );

}

void XMLShapeExport::ImpExportFrameShape(
    const uno::Reference< drawing::XShape >& xShape,
    XMLShapeExportFlags nFeatures, css::awt::Point* pRefPoint)
{
    const uno::Reference< beans::XPropertySet > xPropSet(xShape, uno::UNO_QUERY);
    if(!xPropSet.is())
        return;

    // Transformation
    ImpExportNewTrans(xPropSet, nFeatures, pRefPoint);

    bool bCreateNewline( (nFeatures & XMLShapeExportFlags::NO_WS) == XMLShapeExportFlags::NONE ); // #86116#/#92210#
    SvXMLElementExport aElem( mrExport, XML_NAMESPACE_DRAW,
                              XML_FRAME, bCreateNewline, true );

    // export frame url
    OUString aStr;
    xPropSet->getPropertyValue(u"FrameURL"_ustr) >>= aStr;
    mrExport.AddAttribute ( XML_NAMESPACE_XLINK, XML_HREF, GetExport().GetRelativeReference(aStr) );
    mrExport.AddAttribute ( XML_NAMESPACE_XLINK, XML_TYPE, XML_SIMPLE );
    mrExport.AddAttribute ( XML_NAMESPACE_XLINK, XML_SHOW, XML_EMBED );
    mrExport.AddAttribute ( XML_NAMESPACE_XLINK, XML_ACTUATE, XML_ONLOAD );

    // export name
    xPropSet->getPropertyValue(u"FrameName"_ustr) >>= aStr;
    if( !aStr.isEmpty() )
        mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_FRAME_NAME, aStr );

    // write floating frame
    {
        SvXMLElementExport aOBJ(mrExport, XML_NAMESPACE_DRAW, XML_FLOATING_FRAME, true, true);
    }

    ImpExportDescription(xShape);
}

void XMLShapeExport::ImpExportAppletShape(
    const uno::Reference< drawing::XShape >& xShape,
    XMLShapeExportFlags nFeatures, css::awt::Point* pRefPoint)
{
    const uno::Reference< beans::XPropertySet > xPropSet(xShape, uno::UNO_QUERY);
    if(!xPropSet.is())
        return;

    // Transformation
    ImpExportNewTrans(xPropSet, nFeatures, pRefPoint);

    bool bCreateNewline( (nFeatures & XMLShapeExportFlags::NO_WS) == XMLShapeExportFlags::NONE ); // #86116#/#92210#
    SvXMLElementExport aElement( mrExport, XML_NAMESPACE_DRAW,
                              XML_FRAME, bCreateNewline, true );

    // export frame url
    OUString aStr;
    xPropSet->getPropertyValue(u"AppletCodeBase"_ustr) >>= aStr;
    mrExport.AddAttribute ( XML_NAMESPACE_XLINK, XML_HREF, GetExport().GetRelativeReference(aStr) );
    mrExport.AddAttribute ( XML_NAMESPACE_XLINK, XML_TYPE, XML_SIMPLE );
    mrExport.AddAttribute ( XML_NAMESPACE_XLINK, XML_SHOW, XML_EMBED );
    mrExport.AddAttribute ( XML_NAMESPACE_XLINK, XML_ACTUATE, XML_ONLOAD );

    // export draw:applet-name
    xPropSet->getPropertyValue(u"AppletName"_ustr) >>= aStr;
    if( !aStr.isEmpty() )
        mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_APPLET_NAME, aStr );

    // export draw:code
    xPropSet->getPropertyValue(u"AppletCode"_ustr) >>= aStr;
    mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_CODE, aStr );

    // export draw:may-script
    bool bIsScript = false;
    xPropSet->getPropertyValue(u"AppletIsScript"_ustr) >>= bIsScript;
    mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_MAY_SCRIPT, bIsScript ? XML_TRUE : XML_FALSE );

    {
        // write applet
        SvXMLElementExport aOBJ(mrExport, XML_NAMESPACE_DRAW, XML_APPLET, true, true);

        // export parameters
        uno::Sequence< beans::PropertyValue > aCommands;
        xPropSet->getPropertyValue(u"AppletCommands"_ustr) >>= aCommands;
        for (const auto& rCommand : aCommands)
        {
            rCommand.Value >>= aStr;
            mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_NAME, rCommand.Name );
            mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_VALUE, aStr );
            SvXMLElementExport aElem( mrExport, XML_NAMESPACE_DRAW, XML_PARAM, false, true );
        }
    }

    ImpExportDescription(xShape);
}

void XMLShapeExport::ImpExportPluginShape(
    const uno::Reference< drawing::XShape >& xShape,
    XMLShapeExportFlags nFeatures, css::awt::Point* pRefPoint)
{
    const uno::Reference< beans::XPropertySet > xPropSet(xShape, uno::UNO_QUERY);
    if(!xPropSet.is())
        return;

    // Transformation
    ImpExportNewTrans(xPropSet, nFeatures, pRefPoint);

    bool bCreateNewline( (nFeatures & XMLShapeExportFlags::NO_WS) == XMLShapeExportFlags::NONE ); // #86116#/#92210#
    SvXMLElementExport aElement( mrExport, XML_NAMESPACE_DRAW,
                              XML_FRAME, bCreateNewline, true );

    // export plugin url
    OUString aStr;
    xPropSet->getPropertyValue(u"PluginURL"_ustr) >>= aStr;
    mrExport.AddAttribute ( XML_NAMESPACE_XLINK, XML_HREF, GetExport().GetRelativeReference(aStr) );
    mrExport.AddAttribute ( XML_NAMESPACE_XLINK, XML_TYPE, XML_SIMPLE );
    mrExport.AddAttribute ( XML_NAMESPACE_XLINK, XML_SHOW, XML_EMBED );
    mrExport.AddAttribute ( XML_NAMESPACE_XLINK, XML_ACTUATE, XML_ONLOAD );

    // export mime-type
    xPropSet->getPropertyValue(u"PluginMimeType"_ustr) >>= aStr;
    if(!aStr.isEmpty())
        mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_MIME_TYPE, aStr );

    {
        // write plugin
        SvXMLElementExport aOBJ(mrExport, XML_NAMESPACE_DRAW, XML_PLUGIN, true, true);

        // export parameters
        uno::Sequence< beans::PropertyValue > aCommands;
        xPropSet->getPropertyValue(u"PluginCommands"_ustr) >>= aCommands;
        for (const auto& rCommand : aCommands)
        {
            rCommand.Value >>= aStr;
            mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_NAME, rCommand.Name );
            mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_VALUE, aStr );
            SvXMLElementExport aElem( mrExport, XML_NAMESPACE_DRAW, XML_PARAM, false, true );
        }
    }

    ImpExportDescription(xShape);
}

static void lcl_CopyStream(
        uno::Reference<io::XInputStream> const& xInStream,
        uno::Reference<embed::XStorage> const& xTarget,
        OUString const& rPath, const OUString& rMimeType)
{
    ::comphelper::LifecycleProxy proxy;
    uno::Reference<io::XStream> const xStream(
        ::comphelper::OStorageHelper::GetStreamAtPackageURL(xTarget, rPath,
            embed::ElementModes::WRITE | embed::ElementModes::TRUNCATE, proxy));
    uno::Reference<io::XOutputStream> const xOutStream(
            (xStream.is()) ? xStream->getOutputStream() : nullptr);
    if (!xOutStream.is())
    {
        SAL_WARN("xmloff", "no output stream");
        throw uno::Exception(u"no output stream"_ustr,nullptr);
    }
    uno::Reference< beans::XPropertySet > const xStreamProps(xStream,
        uno::UNO_QUERY);
    if (xStreamProps.is()) { // this is NOT supported in FileSystemStorage
        xStreamProps->setPropertyValue(u"MediaType"_ustr,
            uno::Any(rMimeType));
        xStreamProps->setPropertyValue( // turn off compression
            u"Compressed"_ustr,
            uno::Any(false));
    }
    ::comphelper::OStorageHelper::CopyInputToOutput(xInStream, xOutStream);
    xOutStream->closeOutput();
    proxy.commitStorages();
}

static OUString
lcl_StoreMediaAndGetURL(SvXMLExport & rExport,
    uno::Reference<beans::XPropertySet> const& xPropSet,
    OUString const& rURL, const OUString& rMimeType)
{
    OUString urlPath;
    if (rURL.startsWithIgnoreAsciiCase("vnd.sun.star.Package:", &urlPath))
    {
        try // video is embedded
        {
            uno::Reference<embed::XStorage> const xTarget(
                    rExport.GetTargetStorage(), uno::UNO_SET_THROW);
            uno::Reference<io::XInputStream> xInStream;
            xPropSet->getPropertyValue(u"PrivateStream"_ustr)
                    >>= xInStream;

            if (!xInStream.is())
            {
                SAL_WARN("xmloff", "no input stream");
                return OUString();
            }

            lcl_CopyStream(xInStream, xTarget, rURL, rMimeType);

            return urlPath;
        }
        catch (uno::Exception const&)
        {
            TOOLS_INFO_EXCEPTION("xmloff", "exception while storing embedded media");
        }
        return OUString();
    }
    else
    {
        return rExport.GetRelativeReference(rURL); // linked
    }
}

namespace
{
void ExportGraphicPreview(const uno::Reference<graphic::XGraphic>& xGraphic, SvXMLExport& rExport, std::u16string_view rPrefix, std::u16string_view rExtension, const OUString& rMimeType)
{
    const bool bExportEmbedded(rExport.getExportFlags() & SvXMLExportFlags::EMBEDDED);

    if( xGraphic.is() ) try
    {
        uno::Reference< uno::XComponentContext > xContext = rExport.getComponentContext();

        uno::Reference< embed::XStorage > xPictureStorage;
        uno::Reference< embed::XStorage > xStorage;
        uno::Reference< io::XStream > xPictureStream;

        OUString sPictureName;
        if( bExportEmbedded )
        {
            xPictureStream = new comphelper::UNOMemoryStream();
        }
        else
        {
            xStorage.set( rExport.GetTargetStorage(), uno::UNO_SET_THROW );

            xPictureStorage.set( xStorage->openStorageElement( u"Pictures"_ustr , ::embed::ElementModes::READWRITE ), uno::UNO_SET_THROW );

            sal_Int32 nIndex = 0;
            do
            {
                sPictureName = rPrefix + OUString::number( ++nIndex ) + rExtension;
            }
            while( xPictureStorage->hasByName( sPictureName ) );

            xPictureStream.set( xPictureStorage->openStreamElement( sPictureName, ::embed::ElementModes::READWRITE ), uno::UNO_SET_THROW );
        }

        uno::Reference< graphic::XGraphicProvider > xProvider( graphic::GraphicProvider::create(xContext) );
        uno::Sequence< beans::PropertyValue > aArgs{
            comphelper::makePropertyValue(u"MimeType"_ustr, rMimeType ),
                comphelper::makePropertyValue(u"OutputStream"_ustr, xPictureStream->getOutputStream())
        };
        xProvider->storeGraphic( xGraphic, aArgs );

        if( xPictureStorage.is() )
        {
            uno::Reference< embed::XTransactedObject > xTrans( xPictureStorage, uno::UNO_QUERY );
            if( xTrans.is() )
                xTrans->commit();
        }

        if( !bExportEmbedded )
        {
            OUString sURL = "Pictures/" + sPictureName;
            rExport.AddAttribute( XML_NAMESPACE_XLINK, XML_HREF, sURL );
            rExport.AddAttribute( XML_NAMESPACE_XLINK, XML_TYPE, XML_SIMPLE );
            rExport.AddAttribute( XML_NAMESPACE_XLINK, XML_SHOW, XML_EMBED );
            rExport.AddAttribute( XML_NAMESPACE_XLINK, XML_ACTUATE, XML_ONLOAD );
        }

        SvXMLElementExport aElem( rExport, XML_NAMESPACE_DRAW, XML_IMAGE, false, true );

        if( bExportEmbedded )
        {
            uno::Reference< io::XSeekableInputStream > xSeekable( xPictureStream, uno::UNO_QUERY_THROW );
            xSeekable->seek(0);

            XMLBase64Export aBase64Exp( rExport );
            aBase64Exp.exportOfficeBinaryDataElement( uno::Reference < io::XInputStream >( xPictureStream, uno::UNO_QUERY_THROW ) );
        }
    }
    catch( uno::Exception const & )
    {
        DBG_UNHANDLED_EXCEPTION("xmloff.draw");
    }
}
}

void XMLShapeExport::ImpExportMediaShape(
    const uno::Reference< drawing::XShape >& xShape,
    XmlShapeType eShapeType, XMLShapeExportFlags nFeatures, css::awt::Point* pRefPoint)
{
    const uno::Reference< beans::XPropertySet > xPropSet(xShape, uno::UNO_QUERY);
    if(!xPropSet.is())
        return;

    // Transformation
    ImpExportNewTrans(xPropSet, nFeatures, pRefPoint);

    if(eShapeType == XmlShapeType::PresMediaShape)
    {
        (void)ImpExportPresentationAttributes( xPropSet, GetXMLToken(XML_OBJECT) );
    }
    bool bCreateNewline( (nFeatures & XMLShapeExportFlags::NO_WS) == XMLShapeExportFlags::NONE ); // #86116#/#92210#
    SvXMLElementExport aElem( mrExport, XML_NAMESPACE_DRAW,
                              XML_FRAME, bCreateNewline, true );

    // export media url
    OUString aMediaURL;
    xPropSet->getPropertyValue(u"MediaURL"_ustr) >>= aMediaURL;
    OUString sMimeType;
    xPropSet->getPropertyValue(u"MediaMimeType"_ustr) >>= sMimeType;

    OUString const persistentURL =
        lcl_StoreMediaAndGetURL(GetExport(), xPropSet, aMediaURL, sMimeType);

    mrExport.AddAttribute ( XML_NAMESPACE_XLINK, XML_HREF, persistentURL );
    mrExport.AddAttribute ( XML_NAMESPACE_XLINK, XML_TYPE, XML_SIMPLE );
    mrExport.AddAttribute ( XML_NAMESPACE_XLINK, XML_SHOW, XML_EMBED );
    mrExport.AddAttribute ( XML_NAMESPACE_XLINK, XML_ACTUATE, XML_ONLOAD );

    // export mime-type
    mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_MIME_TYPE, sMimeType );

    // write plugin
    auto pPluginOBJ = std::make_unique<SvXMLElementExport>(mrExport, XML_NAMESPACE_DRAW, XML_PLUGIN, !( nFeatures & XMLShapeExportFlags::NO_WS ), true);

    // export parameters
    static constexpr OUString aFalseStr( u"false"_ustr );
    static constexpr OUString aTrueStr( u"true"_ustr );

    bool bLoop = false;
    static constexpr OUString aLoopStr(  u"Loop"_ustr  );
    xPropSet->getPropertyValue( aLoopStr ) >>= bLoop;
    mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_NAME, aLoopStr );
    mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_VALUE, bLoop ? aTrueStr : aFalseStr );
    delete new SvXMLElementExport( mrExport, XML_NAMESPACE_DRAW, XML_PARAM, false, true );

    bool bMute = false;
    static constexpr OUString aMuteStr(  u"Mute"_ustr  );
    xPropSet->getPropertyValue( aMuteStr ) >>= bMute;
    mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_NAME, aMuteStr );
    mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_VALUE, bMute ? aTrueStr : aFalseStr );
    delete new SvXMLElementExport( mrExport, XML_NAMESPACE_DRAW, XML_PARAM, false, true );

    sal_Int16 nVolumeDB = 0;
    xPropSet->getPropertyValue(u"VolumeDB"_ustr) >>= nVolumeDB;
    mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_NAME, u"VolumeDB"_ustr );
    mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_VALUE, OUString::number( nVolumeDB ) );
    delete new SvXMLElementExport( mrExport, XML_NAMESPACE_DRAW, XML_PARAM, false, true );

    media::ZoomLevel eZoom;
    OUString aZoomValue;
    xPropSet->getPropertyValue(u"Zoom"_ustr) >>= eZoom;
    switch( eZoom )
    {
        case media::ZoomLevel_ZOOM_1_TO_4  : aZoomValue = "25%"; break;
        case media::ZoomLevel_ZOOM_1_TO_2  : aZoomValue = "50%"; break;
        case media::ZoomLevel_ORIGINAL     : aZoomValue = "100%"; break;
        case media::ZoomLevel_ZOOM_2_TO_1  : aZoomValue = "200%"; break;
        case media::ZoomLevel_ZOOM_4_TO_1  : aZoomValue = "400%"; break;
        case media::ZoomLevel_FIT_TO_WINDOW: aZoomValue = "fit"; break;
        case media::ZoomLevel_FIT_TO_WINDOW_FIXED_ASPECT: aZoomValue = "fixedfit"; break;
        case media::ZoomLevel_FULLSCREEN   : aZoomValue = "fullscreen"; break;

        default:
        break;
    }

    if( !aZoomValue.isEmpty() )
    {
        mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_NAME, u"Zoom"_ustr );
        mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_VALUE, aZoomValue );
        delete new SvXMLElementExport( mrExport, XML_NAMESPACE_DRAW, XML_PARAM, false, true );
    }

    pPluginOBJ.reset();

    if (officecfg::Office::Common::Save::Graphic::AddReplacementImages::get())
    {
        uno::Reference<graphic::XGraphic> xGraphic;
        xPropSet->getPropertyValue(u"Graphic"_ustr) >>= xGraphic;
        Graphic aGraphic(xGraphic);
        if (!aGraphic.IsNone())
        {
            // The media has a preview, export it.
            ExportGraphicPreview(xGraphic, mrExport, u"MediaPreview", u".png", u"image/png"_ustr);
        }
    }

    ImpExportDescription(xShape);
}

void XMLShapeExport::ImpExport3DSceneShape( const uno::Reference< drawing::XShape >& xShape, XMLShapeExportFlags nFeatures, awt::Point* pRefPoint)
{
    uno::Reference< drawing::XShapes > xShapes(xShape, uno::UNO_QUERY);
    if(!(xShapes.is() && xShapes->getCount()))
        return;

    uno::Reference< beans::XPropertySet > xPropSet( xShape, uno::UNO_QUERY );
    SAL_WARN_IF( !xPropSet.is(), "xmloff", "XMLShapeExport::ImpExport3DSceneShape can't export a scene without a propertyset" );
    if( !xPropSet.is() )
        return;

    // Transformation
    ImpExportNewTrans(xPropSet, nFeatures, pRefPoint);

    // 3d attributes
    export3DSceneAttributes( xPropSet );

    // write 3DScene shape
    bool bCreateNewline( (nFeatures & XMLShapeExportFlags::NO_WS) == XMLShapeExportFlags::NONE ); // #86116#/#92210#
    SvXMLElementExport aOBJ( mrExport, XML_NAMESPACE_DR3D, XML_SCENE, bCreateNewline, true);

    ImpExportDescription( xShape ); // #i68101#
    ImpExportEvents( xShape );

    // write 3DSceneLights
    export3DLamps( xPropSet );

    // #89764# if export of position is suppressed for group shape,
    // positions of contained objects should be written relative to
    // the upper left edge of the group.
    awt::Point aUpperLeft;

    if(!(nFeatures & XMLShapeExportFlags::POSITION))
    {
        nFeatures |= XMLShapeExportFlags::POSITION;
        aUpperLeft = xShape->getPosition();
        pRefPoint = &aUpperLeft;
    }

    // write members
    exportShapes( xShapes, nFeatures, pRefPoint );
}

void XMLShapeExport::ImpExport3DShape(
    const uno::Reference< drawing::XShape >& xShape,
    XmlShapeType eShapeType)
{
    const uno::Reference< beans::XPropertySet > xPropSet(xShape, uno::UNO_QUERY);
    if(!xPropSet.is())
        return;

    OUString aStr;
    OUStringBuffer sStringBuffer;

    // transformation (UNO_NAME_3D_TRANSFORM_MATRIX == "D3DTransformMatrix")
    uno::Any aAny = xPropSet->getPropertyValue(u"D3DTransformMatrix"_ustr);
    drawing::HomogenMatrix aHomMat;
    aAny >>= aHomMat;
    SdXMLImExTransform3D aTransform;
    aTransform.AddHomogenMatrix(aHomMat);
    if(aTransform.NeedsAction())
        mrExport.AddAttribute(XML_NAMESPACE_DR3D, XML_TRANSFORM, aTransform.GetExportString(mrExport.GetMM100UnitConverter()));

    switch(eShapeType)
    {
        case XmlShapeType::Draw3DCubeObject:
        {
            // minEdge
            aAny = xPropSet->getPropertyValue(u"D3DPosition"_ustr);
            drawing::Position3D aPosition3D;
            aAny >>= aPosition3D;
            ::basegfx::B3DVector aPos3D(aPosition3D.PositionX, aPosition3D.PositionY, aPosition3D.PositionZ);

            // maxEdge
            aAny = xPropSet->getPropertyValue(u"D3DSize"_ustr);
            drawing::Direction3D aDirection3D;
            aAny >>= aDirection3D;
            ::basegfx::B3DVector aDir3D(aDirection3D.DirectionX, aDirection3D.DirectionY, aDirection3D.DirectionZ);

            // transform maxEdge from distance to pos
            aDir3D = aPos3D + aDir3D;

            // write minEdge
            if(aPos3D != ::basegfx::B3DVector(-2500.0, -2500.0, -2500.0)) // write only when not default
            {
                SvXMLUnitConverter::convertB3DVector(sStringBuffer, aPos3D);
                aStr = sStringBuffer.makeStringAndClear();
                mrExport.AddAttribute(XML_NAMESPACE_DR3D, XML_MIN_EDGE, aStr);
            }

            // write maxEdge
            if(aDir3D != ::basegfx::B3DVector(2500.0, 2500.0, 2500.0)) // write only when not default
            {
                SvXMLUnitConverter::convertB3DVector(sStringBuffer, aDir3D);
                aStr = sStringBuffer.makeStringAndClear();
                mrExport.AddAttribute(XML_NAMESPACE_DR3D, XML_MAX_EDGE, aStr);
            }

            // write 3DCube shape
            // #i123542# Do this *after* the attributes are added, else these will be lost since opening
            // the scope will clear the global attribute list at the exporter
            SvXMLElementExport aOBJ(mrExport, XML_NAMESPACE_DR3D, XML_CUBE, true, true);

            break;
        }
        case XmlShapeType::Draw3DSphereObject:
        {
            // Center
            aAny = xPropSet->getPropertyValue(u"D3DPosition"_ustr);
            drawing::Position3D aPosition3D;
            aAny >>= aPosition3D;
            ::basegfx::B3DVector aPos3D(aPosition3D.PositionX, aPosition3D.PositionY, aPosition3D.PositionZ);

            // Size
            aAny = xPropSet->getPropertyValue(u"D3DSize"_ustr);
            drawing::Direction3D aDirection3D;
            aAny >>= aDirection3D;
            ::basegfx::B3DVector aDir3D(aDirection3D.DirectionX, aDirection3D.DirectionY, aDirection3D.DirectionZ);

            // write Center
            if(aPos3D != ::basegfx::B3DVector(0.0, 0.0, 0.0)) // write only when not default
            {
                SvXMLUnitConverter::convertB3DVector(sStringBuffer, aPos3D);
                aStr = sStringBuffer.makeStringAndClear();
                mrExport.AddAttribute(XML_NAMESPACE_DR3D, XML_CENTER, aStr);
            }

            // write Size
            if(aDir3D != ::basegfx::B3DVector(5000.0, 5000.0, 5000.0)) // write only when not default
            {
                SvXMLUnitConverter::convertB3DVector(sStringBuffer, aDir3D);
                aStr = sStringBuffer.makeStringAndClear();
                mrExport.AddAttribute(XML_NAMESPACE_DR3D, XML_SIZE, aStr);
            }

            // write 3DSphere shape
            // #i123542# Do this *after* the attributes are added, else these will be lost since opening
            // the scope will clear the global attribute list at the exporter
            SvXMLElementExport aOBJ(mrExport, XML_NAMESPACE_DR3D, XML_SPHERE, true, true);

            break;
        }
        case XmlShapeType::Draw3DLatheObject:
        case XmlShapeType::Draw3DExtrudeObject:
        {
            // write special 3DLathe/3DExtrude attributes, get 3D tools::PolyPolygon as drawing::PolyPolygonShape3D
            aAny = xPropSet->getPropertyValue(u"D3DPolyPolygon3D"_ustr);
            drawing::PolyPolygonShape3D aUnoPolyPolygon3D;
            aAny >>= aUnoPolyPolygon3D;

            // convert to 3D PolyPolygon
            const basegfx::B3DPolyPolygon aPolyPolygon3D(
                basegfx::utils::UnoPolyPolygonShape3DToB3DPolyPolygon(
                    aUnoPolyPolygon3D));

            // convert to 2D tools::PolyPolygon using identity 3D transformation (just grep X and Y)
            const basegfx::B3DHomMatrix aB3DHomMatrixFor2DConversion;
            const basegfx::B2DPolyPolygon aPolyPolygon(
                basegfx::utils::createB2DPolyPolygonFromB3DPolyPolygon(
                    aPolyPolygon3D,
                    aB3DHomMatrixFor2DConversion));

            // get 2D range of it
            const basegfx::B2DRange aPolyPolygonRange(aPolyPolygon.getB2DRange());

            // export ViewBox
            SdXMLImExViewBox aViewBox(
                aPolyPolygonRange.getMinX(),
                aPolyPolygonRange.getMinY(),
                aPolyPolygonRange.getWidth(),
                aPolyPolygonRange.getHeight());

            mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_VIEWBOX, aViewBox.GetExportString());

            // prepare svg:d string
            const OUString aPolygonString(
                basegfx::utils::exportToSvgD(
                    aPolyPolygon,
                    true,           // bUseRelativeCoordinates
                    false,          // bDetectQuadraticBeziers TTTT: not used in old, but maybe activated now
                    true));         // bHandleRelativeNextPointCompatible

            // write point array
            mrExport.AddAttribute(XML_NAMESPACE_SVG, XML_D, aPolygonString);

            if(eShapeType == XmlShapeType::Draw3DLatheObject)
            {
                // write 3DLathe shape
                SvXMLElementExport aOBJ(mrExport, XML_NAMESPACE_DR3D, XML_ROTATE, true, true);
            }
            else
            {
                // write 3DExtrude shape
                SvXMLElementExport aOBJ(mrExport, XML_NAMESPACE_DR3D, XML_EXTRUDE, true, true);
            }
            break;
        }
        default:
            break;
    }
}

/** helper for chart that adds all attributes of a 3d scene element to the export */
void XMLShapeExport::export3DSceneAttributes( const css::uno::Reference< css::beans::XPropertySet >& xPropSet )
{
    OUString aStr;
    OUStringBuffer sStringBuffer;

    // world transformation (UNO_NAME_3D_TRANSFORM_MATRIX == "D3DTransformMatrix")
    uno::Any aAny = xPropSet->getPropertyValue(u"D3DTransformMatrix"_ustr);
    drawing::HomogenMatrix aHomMat;
    aAny >>= aHomMat;
    SdXMLImExTransform3D aTransform;
    aTransform.AddHomogenMatrix(aHomMat);
    if(aTransform.NeedsAction())
        mrExport.AddAttribute(XML_NAMESPACE_DR3D, XML_TRANSFORM, aTransform.GetExportString(mrExport.GetMM100UnitConverter()));

    // VRP, VPN, VUP
    aAny = xPropSet->getPropertyValue(u"D3DCameraGeometry"_ustr);
    drawing::CameraGeometry aCamGeo;
    aAny >>= aCamGeo;

    ::basegfx::B3DVector aVRP(aCamGeo.vrp.PositionX, aCamGeo.vrp.PositionY, aCamGeo.vrp.PositionZ);
    if(aVRP != ::basegfx::B3DVector(0.0, 0.0, 1.0)) // write only when not default
    {
        SvXMLUnitConverter::convertB3DVector(sStringBuffer, aVRP);
        aStr = sStringBuffer.makeStringAndClear();
        mrExport.AddAttribute(XML_NAMESPACE_DR3D, XML_VRP, aStr);
    }

    ::basegfx::B3DVector aVPN(aCamGeo.vpn.DirectionX, aCamGeo.vpn.DirectionY, aCamGeo.vpn.DirectionZ);
    if(aVPN != ::basegfx::B3DVector(0.0, 0.0, 1.0)) // write only when not default
    {
        SvXMLUnitConverter::convertB3DVector(sStringBuffer, aVPN);
        aStr = sStringBuffer.makeStringAndClear();
        mrExport.AddAttribute(XML_NAMESPACE_DR3D, XML_VPN, aStr);
    }

    ::basegfx::B3DVector aVUP(aCamGeo.vup.DirectionX, aCamGeo.vup.DirectionY, aCamGeo.vup.DirectionZ);
    if(aVUP != ::basegfx::B3DVector(0.0, 1.0, 0.0)) // write only when not default
    {
        SvXMLUnitConverter::convertB3DVector(sStringBuffer, aVUP);
        aStr = sStringBuffer.makeStringAndClear();
        mrExport.AddAttribute(XML_NAMESPACE_DR3D, XML_VUP, aStr);
    }

    // projection "D3DScenePerspective" drawing::ProjectionMode
    aAny = xPropSet->getPropertyValue(u"D3DScenePerspective"_ustr);
    drawing::ProjectionMode aPrjMode;
    aAny >>= aPrjMode;
    if(aPrjMode == drawing::ProjectionMode_PARALLEL)
        aStr = GetXMLToken(XML_PARALLEL);
    else
        aStr = GetXMLToken(XML_PERSPECTIVE);
    mrExport.AddAttribute(XML_NAMESPACE_DR3D, XML_PROJECTION, aStr);

    // distance
    aAny = xPropSet->getPropertyValue(u"D3DSceneDistance"_ustr);
    sal_Int32 nDistance = 0;
    aAny >>= nDistance;
    mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer,
            nDistance);
    aStr = sStringBuffer.makeStringAndClear();
    mrExport.AddAttribute(XML_NAMESPACE_DR3D, XML_DISTANCE, aStr);

    // focalLength
    aAny = xPropSet->getPropertyValue(u"D3DSceneFocalLength"_ustr);
    sal_Int32 nFocalLength = 0;
    aAny >>= nFocalLength;
    mrExport.GetMM100UnitConverter().convertMeasureToXML(sStringBuffer,
            nFocalLength);
    aStr = sStringBuffer.makeStringAndClear();
    mrExport.AddAttribute(XML_NAMESPACE_DR3D, XML_FOCAL_LENGTH, aStr);

    // shadowSlant
    aAny = xPropSet->getPropertyValue(u"D3DSceneShadowSlant"_ustr);
    sal_Int16 nShadowSlant = 0;
    aAny >>= nShadowSlant;
    mrExport.AddAttribute(XML_NAMESPACE_DR3D, XML_SHADOW_SLANT, OUString::number(static_cast<sal_Int32>(nShadowSlant)));

    // shadeMode
    aAny = xPropSet->getPropertyValue(u"D3DSceneShadeMode"_ustr);
    drawing::ShadeMode aShadeMode;
    if(aAny >>= aShadeMode)
    {
        if(aShadeMode == drawing::ShadeMode_FLAT)
            aStr = GetXMLToken(XML_FLAT);
        else if(aShadeMode == drawing::ShadeMode_PHONG)
            aStr = GetXMLToken(XML_PHONG);
        else if(aShadeMode == drawing::ShadeMode_SMOOTH)
            aStr = GetXMLToken(XML_GOURAUD);
        else
            aStr = GetXMLToken(XML_DRAFT);
    }
    else
    {
        // ShadeMode enum not there, write default
        aStr = GetXMLToken(XML_GOURAUD);
    }
    mrExport.AddAttribute(XML_NAMESPACE_DR3D, XML_SHADE_MODE, aStr);

    // ambientColor
    aAny = xPropSet->getPropertyValue(u"D3DSceneAmbientColor"_ustr);
    sal_Int32 nAmbientColor = 0;
    aAny >>= nAmbientColor;
    ::sax::Converter::convertColor(sStringBuffer, nAmbientColor);
    aStr = sStringBuffer.makeStringAndClear();
    mrExport.AddAttribute(XML_NAMESPACE_DR3D, XML_AMBIENT_COLOR, aStr);

    // lightingMode
    aAny = xPropSet->getPropertyValue(u"D3DSceneTwoSidedLighting"_ustr);
    bool bTwoSidedLighting = false;
    aAny >>= bTwoSidedLighting;
    ::sax::Converter::convertBool(sStringBuffer, bTwoSidedLighting);
    aStr = sStringBuffer.makeStringAndClear();
    mrExport.AddAttribute(XML_NAMESPACE_DR3D, XML_LIGHTING_MODE, aStr);
}

/** helper for chart that exports all lamps from the propertyset */
void XMLShapeExport::export3DLamps( const css::uno::Reference< css::beans::XPropertySet >& xPropSet )
{
    // write lamps 1..8 as content
    OUString aStr;
    OUStringBuffer sStringBuffer;

    static constexpr OUStringLiteral aColorPropName(u"D3DSceneLightColor");
    static constexpr OUStringLiteral aDirectionPropName(u"D3DSceneLightDirection");
    static constexpr OUStringLiteral aLightOnPropName(u"D3DSceneLightOn");

    ::basegfx::B3DVector aLightDirection;
    drawing::Direction3D aLightDir;
    bool bLightOnOff = false;
    for(sal_Int32 nLamp = 1; nLamp <= 8; nLamp++)
    {
        OUString aIndexStr = OUString::number( nLamp );

        // lightcolor
        OUString aPropName = aColorPropName + aIndexStr;
        sal_Int32 nLightColor = 0;
        xPropSet->getPropertyValue( aPropName ) >>= nLightColor;
        ::sax::Converter::convertColor(sStringBuffer, nLightColor);
        aStr = sStringBuffer.makeStringAndClear();
        mrExport.AddAttribute(XML_NAMESPACE_DR3D, XML_DIFFUSE_COLOR, aStr);

        // lightdirection
        aPropName = aDirectionPropName + aIndexStr;
        xPropSet->getPropertyValue(aPropName) >>= aLightDir;
        aLightDirection = ::basegfx::B3DVector(aLightDir.DirectionX, aLightDir.DirectionY, aLightDir.DirectionZ);
        SvXMLUnitConverter::convertB3DVector(sStringBuffer, aLightDirection);
        aStr = sStringBuffer.makeStringAndClear();
        mrExport.AddAttribute(XML_NAMESPACE_DR3D, XML_DIRECTION, aStr);

        // lighton
        aPropName = aLightOnPropName + aIndexStr;
        xPropSet->getPropertyValue(aPropName) >>= bLightOnOff;
        ::sax::Converter::convertBool(sStringBuffer, bLightOnOff);
        aStr = sStringBuffer.makeStringAndClear();
        mrExport.AddAttribute(XML_NAMESPACE_DR3D, XML_ENABLED, aStr);

        // specular
        mrExport.AddAttribute(XML_NAMESPACE_DR3D, XML_SPECULAR,
            nLamp == 1 ? XML_TRUE : XML_FALSE);

        // write light entry
        SvXMLElementExport aOBJ(mrExport, XML_NAMESPACE_DR3D, XML_LIGHT, true, true);
    }
}


// using namespace css::io;
// using namespace ::xmloff::EnhancedCustomShapeToken;


static void ExportParameter( OUStringBuffer& rStrBuffer, const css::drawing::EnhancedCustomShapeParameter& rParameter )
{
    if ( !rStrBuffer.isEmpty() )
        rStrBuffer.append( ' ' );
    if ( rParameter.Value.getValueTypeClass() == uno::TypeClass_DOUBLE )
    {
        double fNumber = 0.0;
        rParameter.Value >>= fNumber;
        ::rtl::math::doubleToUStringBuffer( rStrBuffer, fNumber, rtl_math_StringFormat_Automatic, rtl_math_DecimalPlaces_Max, '.', true );
    }
    else
    {
        sal_Int32 nValue = 0;
        rParameter.Value >>= nValue;

        switch( rParameter.Type )
        {
            case css::drawing::EnhancedCustomShapeParameterType::EQUATION :
            {
                rStrBuffer.append( "?f" + OUString::number( nValue ) );
            }
            break;

            case css::drawing::EnhancedCustomShapeParameterType::ADJUSTMENT :
            {
                rStrBuffer.append( '$' );
                rStrBuffer.append( nValue );
            }
            break;

            case css::drawing::EnhancedCustomShapeParameterType::BOTTOM :
                rStrBuffer.append( GetXMLToken( XML_BOTTOM ) ); break;
            case css::drawing::EnhancedCustomShapeParameterType::RIGHT :
                rStrBuffer.append( GetXMLToken( XML_RIGHT ) ); break;
            case css::drawing::EnhancedCustomShapeParameterType::TOP :
                rStrBuffer.append( GetXMLToken( XML_TOP ) ); break;
            case css::drawing::EnhancedCustomShapeParameterType::LEFT :
                rStrBuffer.append( GetXMLToken( XML_LEFT ) ); break;
            case css::drawing::EnhancedCustomShapeParameterType::XSTRETCH :
                rStrBuffer.append( GetXMLToken( XML_XSTRETCH ) ); break;
            case css::drawing::EnhancedCustomShapeParameterType::YSTRETCH :
                rStrBuffer.append( GetXMLToken( XML_YSTRETCH ) ); break;
            case css::drawing::EnhancedCustomShapeParameterType::HASSTROKE :
                rStrBuffer.append( GetXMLToken( XML_HASSTROKE ) ); break;
            case css::drawing::EnhancedCustomShapeParameterType::HASFILL :
                rStrBuffer.append( GetXMLToken( XML_HASFILL ) ); break;
            case css::drawing::EnhancedCustomShapeParameterType::WIDTH :
                rStrBuffer.append( GetXMLToken( XML_WIDTH ) ); break;
            case css::drawing::EnhancedCustomShapeParameterType::HEIGHT :
                rStrBuffer.append( GetXMLToken( XML_HEIGHT ) ); break;
            case css::drawing::EnhancedCustomShapeParameterType::LOGWIDTH :
                rStrBuffer.append( GetXMLToken( XML_LOGWIDTH ) ); break;
            case css::drawing::EnhancedCustomShapeParameterType::LOGHEIGHT :
                rStrBuffer.append( GetXMLToken( XML_LOGHEIGHT ) ); break;
            default :
                rStrBuffer.append( nValue );
        }
    }
}

static void ImpExportEquations( SvXMLExport& rExport, const uno::Sequence< OUString >& rEquations )
{
    sal_Int32 i;
    for ( i = 0; i < rEquations.getLength(); i++ )
    {
        OUString aStr= "f" + OUString::number( i );
        rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_NAME, aStr );

        aStr = rEquations[ i ];
        sal_Int32 nIndex = 0;
        do
        {
            nIndex = aStr.indexOf( '?', nIndex );
            if ( nIndex != -1 )
            {
                aStr = OUString::Concat(aStr.subView(0, nIndex + 1)) + "f"
                    + aStr.subView(nIndex + 1, aStr.getLength() - nIndex - 1);
                nIndex++;
            }
        } while( nIndex != -1 );
        rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_FORMULA, aStr );
        SvXMLElementExport aOBJ( rExport, XML_NAMESPACE_DRAW, XML_EQUATION, true, true );
    }
}

static void ImpExportHandles( SvXMLExport& rExport, const uno::Sequence< beans::PropertyValues >& rHandles )
{
    if ( !rHandles.hasElements() )
        return;

    OUString       aStr;
    OUStringBuffer aStrBuffer;

    for ( const uno::Sequence< beans::PropertyValue >& rPropSeq : rHandles )
    {
        bool bPosition = false;
        for ( const beans::PropertyValue& rPropVal : rPropSeq )
        {
            switch( EASGet( rPropVal.Name ) )
            {
                case EAS_Position :
                {
                    css::drawing::EnhancedCustomShapeParameterPair aPosition;
                    if ( rPropVal.Value >>= aPosition )
                    {
                        ExportParameter( aStrBuffer, aPosition.First );
                        ExportParameter( aStrBuffer, aPosition.Second );
                        aStr = aStrBuffer.makeStringAndClear();

                        // Keep it for backward compatibility
                        rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_HANDLE_POSITION, aStr );

                        SvtSaveOptions::ODFSaneDefaultVersion eVersion = rExport.getSaneDefaultVersion();
                        if (eVersion >= SvtSaveOptions::ODFSVER_014)
                        {
                            comphelper::SequenceAsHashMap aPropSeqMap(rPropSeq);
                            if (aPropSeqMap.contains(u"Polar"_ustr))
                            {
                                ExportParameter( aStrBuffer, aPosition.First );
                                aStr = aStrBuffer.makeStringAndClear();
                                rExport.AddAttribute(XML_NAMESPACE_DRAW, XML_HANDLE_POLAR_RADIUS, aStr);

                                ExportParameter( aStrBuffer, aPosition.Second );
                                aStr = aStrBuffer.makeStringAndClear();
                                rExport.AddAttribute(XML_NAMESPACE_DRAW, XML_HANDLE_POLAR_ANGLE, aStr);
                            }
                            else
                            {
                                ExportParameter( aStrBuffer, aPosition.First );
                                aStr = aStrBuffer.makeStringAndClear();
                                rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_HANDLE_POSITION_X, aStr );

                                ExportParameter( aStrBuffer, aPosition.Second );
                                aStr = aStrBuffer.makeStringAndClear();
                                rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_HANDLE_POSITION_Y, aStr );
                            }
                        }

                        bPosition = true;
                    }
                }
                break;
                case EAS_MirroredX :
                {
                    bool bMirroredX;
                    if ( rPropVal.Value >>= bMirroredX )
                        rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_HANDLE_MIRROR_HORIZONTAL,
                            bMirroredX ? GetXMLToken( XML_TRUE ) : GetXMLToken( XML_FALSE ) );
                }
                break;
                case EAS_MirroredY :
                {
                    bool bMirroredY;
                    if ( rPropVal.Value >>= bMirroredY )
                        rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_HANDLE_MIRROR_VERTICAL,
                            bMirroredY ? GetXMLToken( XML_TRUE ) : GetXMLToken( XML_FALSE ) );
                }
                break;
                case EAS_Switched :
                {
                    bool bSwitched;
                    if ( rPropVal.Value >>= bSwitched )
                        rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_HANDLE_SWITCHED,
                            bSwitched ? GetXMLToken( XML_TRUE ) : GetXMLToken( XML_FALSE ) );
                }
                break;
                case EAS_Polar :
                {
                    css::drawing::EnhancedCustomShapeParameterPair aPolar;
                    if ( rPropVal.Value >>= aPolar )
                    {
                        ExportParameter( aStrBuffer, aPolar.First );
                        ExportParameter( aStrBuffer, aPolar.Second );
                        aStr = aStrBuffer.makeStringAndClear();
                        // Keep it for backward compatibility
                        rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_HANDLE_POLAR, aStr );

                        SvtSaveOptions::ODFSaneDefaultVersion eVersion = rExport.getSaneDefaultVersion();
                        if (eVersion >= SvtSaveOptions::ODFSVER_014)
                        {
                            ExportParameter( aStrBuffer, aPolar.First );
                            aStr = aStrBuffer.makeStringAndClear();
                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_HANDLE_POLAR_POLE_X, aStr );

                            ExportParameter( aStrBuffer, aPolar.Second );
                            aStr = aStrBuffer.makeStringAndClear();
                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_HANDLE_POLAR_POLE_Y, aStr );
                        }
                    }
                }
                break;
                case EAS_RadiusRangeMinimum :
                {
                    css::drawing::EnhancedCustomShapeParameter aRadiusRangeMinimum;
                    if ( rPropVal.Value >>= aRadiusRangeMinimum )
                    {
                        ExportParameter( aStrBuffer, aRadiusRangeMinimum );
                        aStr = aStrBuffer.makeStringAndClear();
                        rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_HANDLE_RADIUS_RANGE_MINIMUM, aStr );
                    }
                }
                break;
                case EAS_RadiusRangeMaximum :
                {
                    css::drawing::EnhancedCustomShapeParameter aRadiusRangeMaximum;
                    if ( rPropVal.Value >>= aRadiusRangeMaximum )
                    {
                        ExportParameter( aStrBuffer, aRadiusRangeMaximum );
                        aStr = aStrBuffer.makeStringAndClear();
                        rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_HANDLE_RADIUS_RANGE_MAXIMUM, aStr );
                    }
                }
                break;
                case EAS_RangeXMinimum :
                {
                    css::drawing::EnhancedCustomShapeParameter aXRangeMinimum;
                    if ( rPropVal.Value >>= aXRangeMinimum )
                    {
                        ExportParameter( aStrBuffer, aXRangeMinimum );
                        aStr = aStrBuffer.makeStringAndClear();
                        rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_HANDLE_RANGE_X_MINIMUM, aStr );
                    }
                }
                break;
                case EAS_RangeXMaximum :
                {
                    css::drawing::EnhancedCustomShapeParameter aXRangeMaximum;
                    if ( rPropVal.Value >>= aXRangeMaximum )
                    {
                        ExportParameter( aStrBuffer, aXRangeMaximum );
                        aStr = aStrBuffer.makeStringAndClear();
                        rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_HANDLE_RANGE_X_MAXIMUM, aStr );
                    }
                }
                break;
                case EAS_RangeYMinimum :
                {
                    css::drawing::EnhancedCustomShapeParameter aYRangeMinimum;
                    if ( rPropVal.Value >>= aYRangeMinimum )
                    {
                        ExportParameter( aStrBuffer, aYRangeMinimum );
                        aStr = aStrBuffer.makeStringAndClear();
                        rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_HANDLE_RANGE_Y_MINIMUM, aStr );
                    }
                }
                break;
                case EAS_RangeYMaximum :
                {
                    css::drawing::EnhancedCustomShapeParameter aYRangeMaximum;
                    if ( rPropVal.Value >>= aYRangeMaximum )
                    {
                        ExportParameter( aStrBuffer, aYRangeMaximum );
                        aStr = aStrBuffer.makeStringAndClear();
                        rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_HANDLE_RANGE_Y_MAXIMUM, aStr );
                    }
                }
                break;
                default:
                    break;
            }
        }
        if ( bPosition )
            SvXMLElementExport aOBJ( rExport, XML_NAMESPACE_DRAW, XML_HANDLE, true, true );
        else
            rExport.ClearAttrList();
    }
}

static void ImpExportEnhancedPath( SvXMLExport& rExport,
                            const uno::Sequence< css::drawing::EnhancedCustomShapeParameterPair >& rCoordinates,
                            const uno::Sequence< css::drawing::EnhancedCustomShapeSegment >& rSegments,
                            bool bExtended = false )
{

    OUString       aStr;
    OUStringBuffer aStrBuffer;
    bool bNeedExtended = false;

    sal_Int32 i, j, k, l;

    sal_Int32 nCoords = rCoordinates.getLength();
    sal_Int32 nSegments = rSegments.getLength();
    bool bSimpleSegments = nSegments == 0;
    if ( bSimpleSegments )
        nSegments = 4;
    for ( j = i = 0; j < nSegments; j++ )
    {
        css::drawing::EnhancedCustomShapeSegment aSegment;
        if ( bSimpleSegments )
        {
            // if there are not enough segments we will default them
            switch( j )
            {
                case 0 :
                {
                    aSegment.Count = 1;
                    aSegment.Command = css::drawing::EnhancedCustomShapeSegmentCommand::MOVETO;
                }
                break;
                case 1 :
                {
                    aSegment.Count = static_cast<sal_Int16>(std::min( nCoords - 1, sal_Int32(32767) ));
                    aSegment.Command = css::drawing::EnhancedCustomShapeSegmentCommand::LINETO;
                }
                break;
                case 2 :
                {
                    aSegment.Count = 1;
                    aSegment.Command = css::drawing::EnhancedCustomShapeSegmentCommand::CLOSESUBPATH;
                }
                break;
                case 3 :
                {
                    aSegment.Count = 1;
                    aSegment.Command = css::drawing::EnhancedCustomShapeSegmentCommand::ENDSUBPATH;
                }
                break;
            }
        }
        else
            aSegment = rSegments[ j ];

        if ( !aStrBuffer.isEmpty() )
            aStrBuffer.append( ' ' );

        sal_Int32 nParameter = 0;
        switch( aSegment.Command )
        {
            case css::drawing::EnhancedCustomShapeSegmentCommand::CLOSESUBPATH :
                aStrBuffer.append( 'Z' ); break;
            case css::drawing::EnhancedCustomShapeSegmentCommand::ENDSUBPATH :
                aStrBuffer.append( 'N' ); break;
            case css::drawing::EnhancedCustomShapeSegmentCommand::NOFILL :
                aStrBuffer.append( 'F' ); break;
            case css::drawing::EnhancedCustomShapeSegmentCommand::NOSTROKE :
                aStrBuffer.append( 'S' ); break;

            case css::drawing::EnhancedCustomShapeSegmentCommand::MOVETO :
                aStrBuffer.append( 'M' ); nParameter = 1; break;
            case css::drawing::EnhancedCustomShapeSegmentCommand::LINETO :
                aStrBuffer.append( 'L' ); nParameter = 1; break;
            case css::drawing::EnhancedCustomShapeSegmentCommand::CURVETO :
                aStrBuffer.append( 'C' ); nParameter = 3; break;
            case css::drawing::EnhancedCustomShapeSegmentCommand::ANGLEELLIPSETO :
                aStrBuffer.append( 'T' ); nParameter = 3; break;
            case css::drawing::EnhancedCustomShapeSegmentCommand::ANGLEELLIPSE :
                aStrBuffer.append( 'U' ); nParameter = 3; break;
            case css::drawing::EnhancedCustomShapeSegmentCommand::ARCTO :
                aStrBuffer.append( 'A' ); nParameter = 4; break;
            case css::drawing::EnhancedCustomShapeSegmentCommand::ARC :
                aStrBuffer.append( 'B' ); nParameter = 4; break;
            case css::drawing::EnhancedCustomShapeSegmentCommand::CLOCKWISEARCTO :
                aStrBuffer.append( 'W' ); nParameter = 4; break;
            case css::drawing::EnhancedCustomShapeSegmentCommand::CLOCKWISEARC :
                aStrBuffer.append( 'V' ); nParameter = 4; break;
            case css::drawing::EnhancedCustomShapeSegmentCommand::ELLIPTICALQUADRANTX :
                aStrBuffer.append( 'X' ); nParameter = 1; break;
            case css::drawing::EnhancedCustomShapeSegmentCommand::ELLIPTICALQUADRANTY :
                aStrBuffer.append( 'Y' ); nParameter = 1; break;
            case css::drawing::EnhancedCustomShapeSegmentCommand::QUADRATICCURVETO :
                aStrBuffer.append( 'Q' ); nParameter = 2; break;
            case css::drawing::EnhancedCustomShapeSegmentCommand::ARCANGLETO :
                if ( bExtended ) {
                    aStrBuffer.append( 'G' );
                    nParameter = 2;
                } else {
                    aStrBuffer.setLength( aStrBuffer.getLength() - 1);
                    bNeedExtended = true;
                    i += 2;
                }
                break;
            case css::drawing::EnhancedCustomShapeSegmentCommand::DARKEN :
                if ( bExtended )
                    aStrBuffer.append( 'H' );
                else
                    bNeedExtended = true;
                break;
            case css::drawing::EnhancedCustomShapeSegmentCommand::DARKENLESS :
                if ( bExtended )
                    aStrBuffer.append( 'I' );
                else
                    bNeedExtended = true;
                break;
            case css::drawing::EnhancedCustomShapeSegmentCommand::LIGHTEN :
                if ( bExtended )
                    aStrBuffer.append( 'J' );
                else
                    bNeedExtended = true;
                break;
            case css::drawing::EnhancedCustomShapeSegmentCommand::LIGHTENLESS :
                if ( bExtended )
                    aStrBuffer.append( 'K' );
                else
                    bNeedExtended = true;
                break;
            default : // ups, seems to be something wrong
            {
                aSegment.Count = 1;
                aSegment.Command = css::drawing::EnhancedCustomShapeSegmentCommand::LINETO;
            }
            break;
        }
        if ( nParameter )
        {
            for ( k = 0; k < aSegment.Count; k++ )
            {
                if ( ( i + nParameter ) <= nCoords )
                {
                    for ( l = 0; l < nParameter; l++ )
                    {
                        ExportParameter( aStrBuffer, rCoordinates[ i ].First );
                        ExportParameter( aStrBuffer, rCoordinates[ i++ ].Second );
                    }
                }
                else
                {
                    j = nSegments;  // error -> exiting
                    break;
                }
            }
        }
    }
    aStr = aStrBuffer.makeStringAndClear();
    rExport.AddAttribute( bExtended ? XML_NAMESPACE_DRAW_EXT : XML_NAMESPACE_DRAW, XML_ENHANCED_PATH, aStr );
    if (!bExtended && bNeedExtended && (rExport.getSaneDefaultVersion() & SvtSaveOptions::ODFSVER_EXTENDED))
        ImpExportEnhancedPath( rExport, rCoordinates, rSegments, true );
}

static void ImpExportEnhancedGeometry( SvXMLExport& rExport, const uno::Reference< beans::XPropertySet >& xPropSet )
{
    bool bEquations = false;
    uno::Sequence< OUString > aEquations;

    bool bHandles = false;
    uno::Sequence< beans::PropertyValues > aHandles;

    uno::Sequence< css::drawing::EnhancedCustomShapeSegment > aSegments;
    uno::Sequence< css::drawing::EnhancedCustomShapeParameterPair > aCoordinates;

    uno::Sequence< css::drawing::EnhancedCustomShapeAdjustmentValue > aAdjustmentValues;

    OUString       aStr;
    OUStringBuffer aStrBuffer;
    double fTextRotateAngle(0.0);
    double fTextPreRotateAngle(0.0); // will be consolidated with fTextRotateAngle at the end
    SvXMLUnitConverter& rUnitConverter = rExport.GetMM100UnitConverter();

    uno::Reference< beans::XPropertySetInfo > xPropSetInfo( xPropSet->getPropertySetInfo() );

    // geometry
    static constexpr OUString sCustomShapeGeometry( u"CustomShapeGeometry"_ustr );
    if ( xPropSetInfo.is() && xPropSetInfo->hasPropertyByName( sCustomShapeGeometry ) )
    {
        uno::Any aGeoPropSet( xPropSet->getPropertyValue( sCustomShapeGeometry ) );
        uno::Sequence< beans::PropertyValue > aGeoPropSeq;

        if ( aGeoPropSet >>= aGeoPropSeq )
        {
            bool bCoordinates = false;
            OUString aCustomShapeType( u"non-primitive"_ustr );

            for (const beans::PropertyValue& rGeoProp : aGeoPropSeq)
            {
                switch( EASGet( rGeoProp.Name ) )
                {
                    case EAS_Type :
                    {
                        rGeoProp.Value >>= aCustomShapeType;
                    }
                    break;
                    case EAS_MirroredX :
                    {
                        bool bMirroredX;
                        if ( rGeoProp.Value >>= bMirroredX )
                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_MIRROR_HORIZONTAL,
                                bMirroredX ? GetXMLToken( XML_TRUE ) : GetXMLToken( XML_FALSE ) );
                    }
                    break;
                    case EAS_MirroredY :
                    {
                        bool bMirroredY;
                        if ( rGeoProp.Value >>= bMirroredY )
                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_MIRROR_VERTICAL,
                                bMirroredY ? GetXMLToken( XML_TRUE ) : GetXMLToken( XML_FALSE ) );
                    }
                    break;
                    case EAS_ViewBox :
                    {
                        awt::Rectangle aRect;
                        if ( rGeoProp.Value >>= aRect )
                        {
                            SdXMLImExViewBox aViewBox( aRect.X, aRect.Y, aRect.Width, aRect.Height );
                            rExport.AddAttribute( XML_NAMESPACE_SVG, XML_VIEWBOX, aViewBox.GetExportString() );
                        }
                    }
                    break;
                    case EAS_TextPreRotateAngle :
                    {
                            rGeoProp.Value >>= fTextPreRotateAngle;
                    }
                    break;
                    case EAS_TextRotateAngle :
                    {
                        rGeoProp.Value >>= fTextRotateAngle;
                    }
                    break;
                    case EAS_Extrusion :
                    {
                        uno::Sequence< beans::PropertyValue > aExtrusionPropSeq;
                        if ( rGeoProp.Value >>= aExtrusionPropSeq )
                        {
                            bool bSkewValuesProvided = false;
                            for (const beans::PropertyValue& rProp : aExtrusionPropSeq)
                            {
                                switch( EASGet( rProp.Name ) )
                                {
                                    case EAS_Extrusion :
                                    {
                                        bool bExtrusionOn;
                                        if ( rProp.Value >>= bExtrusionOn )
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION,
                                                bExtrusionOn ? GetXMLToken( XML_TRUE ) : GetXMLToken( XML_FALSE ) );
                                    }
                                    break;
                                    case EAS_Brightness :
                                    {
                                        double fExtrusionBrightness = 0;
                                        if ( rProp.Value >>= fExtrusionBrightness )
                                        {
                                            ::sax::Converter::convertDouble(
                                                aStrBuffer,
                                                fExtrusionBrightness,
                                                false,
                                                util::MeasureUnit::PERCENT,
                                                util::MeasureUnit::PERCENT);
                                            aStrBuffer.append( '%' );
                                            aStr = aStrBuffer.makeStringAndClear();
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION_BRIGHTNESS, aStr );
                                        }
                                    }
                                    break;
                                    case EAS_Depth :
                                    {
                                        css::drawing::EnhancedCustomShapeParameterPair aDepthParaPair;
                                        if ( rProp.Value >>= aDepthParaPair )
                                        {
                                            double fDepth = 0;
                                            if ( aDepthParaPair.First.Value >>= fDepth )
                                            {
                                                rExport.GetMM100UnitConverter().convertDouble( aStrBuffer, fDepth );
                                                ExportParameter( aStrBuffer, aDepthParaPair.Second );
                                                aStr = aStrBuffer.makeStringAndClear();
                                                rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION_DEPTH, aStr );
                                            }
                                        }
                                    }
                                    break;
                                    case EAS_Diffusion :
                                    {
                                        double fExtrusionDiffusion = 0;
                                        if ( rProp.Value >>= fExtrusionDiffusion )
                                        {
                                            ::sax::Converter::convertDouble(
                                                aStrBuffer,
                                                fExtrusionDiffusion,
                                                false,
                                                util::MeasureUnit::PERCENT,
                                                util::MeasureUnit::PERCENT);
                                            aStrBuffer.append( '%' );
                                            aStr = aStrBuffer.makeStringAndClear();
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION_DIFFUSION, aStr );
                                        }
                                    }
                                    break;
                                    case EAS_NumberOfLineSegments :
                                    {
                                        sal_Int32 nExtrusionNumberOfLineSegments = 0;
                                        if ( rProp.Value >>= nExtrusionNumberOfLineSegments )
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION_NUMBER_OF_LINE_SEGMENTS, OUString::number( nExtrusionNumberOfLineSegments ) );
                                    }
                                    break;
                                    case EAS_LightFace :
                                    {
                                        bool bExtrusionLightFace;
                                        if ( rProp.Value >>= bExtrusionLightFace )
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION_LIGHT_FACE,
                                                bExtrusionLightFace ? GetXMLToken( XML_TRUE ) : GetXMLToken( XML_FALSE ) );
                                    }
                                    break;
                                    case EAS_FirstLightHarsh :
                                    {
                                        bool bExtrusionFirstLightHarsh;
                                        if ( rProp.Value >>= bExtrusionFirstLightHarsh )
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION_FIRST_LIGHT_HARSH,
                                                bExtrusionFirstLightHarsh ? GetXMLToken( XML_TRUE ) : GetXMLToken( XML_FALSE ) );
                                    }
                                    break;
                                    case EAS_SecondLightHarsh :
                                    {
                                        bool bExtrusionSecondLightHarsh;
                                        if ( rProp.Value >>= bExtrusionSecondLightHarsh )
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION_SECOND_LIGHT_HARSH,
                                                bExtrusionSecondLightHarsh ? GetXMLToken( XML_TRUE ) : GetXMLToken( XML_FALSE ) );
                                    }
                                    break;
                                    case EAS_FirstLightLevel :
                                    {
                                        double fExtrusionFirstLightLevel = 0;
                                        if ( rProp.Value >>= fExtrusionFirstLightLevel )
                                        {
                                            fExtrusionFirstLightLevel =
                                                std::clamp(fExtrusionFirstLightLevel, 0.0, 100.0);
                                            ::sax::Converter::convertDouble(
                                                aStrBuffer,
                                                fExtrusionFirstLightLevel,
                                                false,
                                                util::MeasureUnit::PERCENT,
                                                util::MeasureUnit::PERCENT);
                                            aStrBuffer.append( '%' );
                                            aStr = aStrBuffer.makeStringAndClear();
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION_FIRST_LIGHT_LEVEL, aStr );
                                        }
                                    }
                                    break;
                                    case EAS_SecondLightLevel :
                                    {
                                        double fExtrusionSecondLightLevel = 0;
                                        if ( rProp.Value >>= fExtrusionSecondLightLevel )
                                        {
                                            fExtrusionSecondLightLevel =
                                                std::clamp(fExtrusionSecondLightLevel, 0.0, 100.0);
                                            ::sax::Converter::convertDouble(
                                                aStrBuffer,
                                                fExtrusionSecondLightLevel,
                                                false,
                                                util::MeasureUnit::PERCENT,
                                                util::MeasureUnit::PERCENT);
                                            aStrBuffer.append( '%' );
                                            aStr = aStrBuffer.makeStringAndClear();
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION_SECOND_LIGHT_LEVEL, aStr );
                                        }
                                    }
                                    break;
                                    case EAS_FirstLightDirection :
                                    {
                                        drawing::Direction3D aExtrusionFirstLightDirection;
                                        if ( rProp.Value >>= aExtrusionFirstLightDirection )
                                        {
                                            ::basegfx::B3DVector aVec3D( aExtrusionFirstLightDirection.DirectionX, aExtrusionFirstLightDirection.DirectionY,
                                                aExtrusionFirstLightDirection.DirectionZ );
                                            SvXMLUnitConverter::convertB3DVector( aStrBuffer, aVec3D );
                                            aStr = aStrBuffer.makeStringAndClear();
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION_FIRST_LIGHT_DIRECTION, aStr );
                                        }
                                    }
                                    break;
                                    case EAS_SecondLightDirection :
                                    {
                                        drawing::Direction3D aExtrusionSecondLightDirection;
                                        if ( rProp.Value >>= aExtrusionSecondLightDirection )
                                        {
                                            ::basegfx::B3DVector aVec3D( aExtrusionSecondLightDirection.DirectionX, aExtrusionSecondLightDirection.DirectionY,
                                                aExtrusionSecondLightDirection.DirectionZ );
                                            SvXMLUnitConverter::convertB3DVector( aStrBuffer, aVec3D );
                                            aStr = aStrBuffer.makeStringAndClear();
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION_SECOND_LIGHT_DIRECTION, aStr );
                                        }
                                    }
                                    break;
                                    case EAS_Metal :
                                    {
                                        bool bExtrusionMetal;
                                        if ( rProp.Value >>= bExtrusionMetal )
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION_METAL,
                                                bExtrusionMetal ? GetXMLToken( XML_TRUE ) : GetXMLToken( XML_FALSE ) );
                                    }
                                    break;
                                    case EAS_MetalType :
                                    {
                                        // In ODF since ODF 1.4. Before 1.4, export only in extended.
                                        sal_Int16 eMetalType;
                                        if (rProp.Value >>= eMetalType)
                                        {
                                            // LibreOffice had used the same values as later specified in ODF 1.4
                                            if (eMetalType == drawing::EnhancedCustomShapeMetalType::MetalMSCompatible)
                                                aStr = "loext:MetalMSCompatible";
                                            else
                                                aStr = "draw:MetalODF";

                                            SvtSaveOptions::ODFSaneDefaultVersion eVersion = rExport.getSaneDefaultVersion();
                                            if (eVersion >= SvtSaveOptions::ODFSVER_014)
                                            {
                                                if (!(eVersion & SvtSaveOptions::ODFSVER_EXTENDED)
                                                    && eMetalType == drawing::EnhancedCustomShapeMetalType::MetalMSCompatible)
                                                    rExport.AddAttribute(XML_NAMESPACE_XMLNS, XML_NP_LO_EXT, XML_N_LO_EXT);
                                                rExport.AddAttribute(XML_NAMESPACE_DRAW, XML_EXTRUSION_METAL_TYPE, aStr);
                                            }
                                            else if (eVersion & SvtSaveOptions::ODFSVER_EXTENDED)
                                            {
                                                rExport.AddAttribute(XML_NAMESPACE_LO_EXT, XML_EXTRUSION_METAL_TYPE, aStr);
                                            }
                                        }
                                    }
                                    break;
                                    case EAS_ShadeMode :
                                    {
                                        // shadeMode
                                        drawing::ShadeMode eShadeMode;
                                        if( rProp.Value >>= eShadeMode )
                                        {
                                            if( eShadeMode == drawing::ShadeMode_FLAT )
                                                aStr = GetXMLToken( XML_FLAT );
                                            else if( eShadeMode == drawing::ShadeMode_PHONG )
                                                aStr = GetXMLToken( XML_PHONG );
                                            else if( eShadeMode == drawing::ShadeMode_SMOOTH )
                                                aStr = GetXMLToken( XML_GOURAUD );
                                            else
                                                aStr = GetXMLToken( XML_DRAFT );
                                        }
                                        else
                                        {
                                            // ShadeMode enum not there, write default
                                            aStr = GetXMLToken( XML_FLAT);
                                        }
                                        rExport.AddAttribute( XML_NAMESPACE_DR3D, XML_SHADE_MODE, aStr );
                                    }
                                    break;
                                    case EAS_RotateAngle :
                                    {
                                        css::drawing::EnhancedCustomShapeParameterPair aRotateAngleParaPair;
                                        if ( rProp.Value >>= aRotateAngleParaPair )
                                        {
                                            ExportParameter( aStrBuffer, aRotateAngleParaPair.First );
                                            ExportParameter( aStrBuffer, aRotateAngleParaPair.Second );
                                            aStr = aStrBuffer.makeStringAndClear();
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION_ROTATION_ANGLE, aStr );
                                        }
                                    }
                                    break;
                                    case EAS_RotationCenter :
                                    {
                                        drawing::Direction3D aExtrusionRotationCenter;
                                        if ( rProp.Value >>= aExtrusionRotationCenter )
                                        {
                                            ::basegfx::B3DVector aVec3D( aExtrusionRotationCenter.DirectionX, aExtrusionRotationCenter.DirectionY,
                                                aExtrusionRotationCenter.DirectionZ );
                                            SvXMLUnitConverter::convertB3DVector( aStrBuffer, aVec3D );
                                            aStr = aStrBuffer.makeStringAndClear();
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION_ROTATION_CENTER, aStr );
                                        }
                                    }
                                    break;
                                    case EAS_Shininess :
                                    {
                                        double fExtrusionShininess = 0;
                                        if ( rProp.Value >>= fExtrusionShininess )
                                        {
                                            ::sax::Converter::convertDouble(
                                                aStrBuffer,
                                                fExtrusionShininess,
                                                false,
                                                util::MeasureUnit::PERCENT,
                                                util::MeasureUnit::PERCENT);
                                            aStrBuffer.append( '%' );
                                            aStr = aStrBuffer.makeStringAndClear();
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION_SHININESS, aStr );
                                        }
                                    }
                                    break;
                                    case EAS_Skew :
                                    {
                                        css::drawing::EnhancedCustomShapeParameterPair aSkewParaPair;
                                        if ( rProp.Value >>= aSkewParaPair )
                                        {
                                            bSkewValuesProvided = true;
                                            ExportParameter( aStrBuffer, aSkewParaPair.First );
                                            ExportParameter( aStrBuffer, aSkewParaPair.Second );
                                            aStr = aStrBuffer.makeStringAndClear();
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION_SKEW, aStr );
                                        }
                                    }
                                    break;
                                    case EAS_Specularity :
                                    {
                                        double fExtrusionSpecularity = 0;
                                        if ( rProp.Value >>= fExtrusionSpecularity )
                                        {
                                            // ODF 1.0/1.1 allow arbitrary percent, ODF 1.2/1.3 restrict it to 0%-100%,
                                            // ODF 1.4 restricts it to non negative percent.
                                            SvtSaveOptions::ODFSaneDefaultVersion eVersion = rExport.getSaneDefaultVersion();
                                            if (fExtrusionSpecularity > 100.0 && eVersion & SvtSaveOptions::ODFSVER_EXTENDED
                                                && eVersion >= SvtSaveOptions::ODFSVER_012 && eVersion < SvtSaveOptions::ODFSVER_014)
                                            {
                                                // tdf#147580 write values > 100% in loext
                                                ::sax::Converter::convertDouble(
                                                    aStrBuffer,
                                                    fExtrusionSpecularity,
                                                    false,
                                                    util::MeasureUnit::PERCENT,
                                                    util::MeasureUnit::PERCENT);
                                                aStrBuffer.append( '%' );
                                                aStr = aStrBuffer.makeStringAndClear();
                                                rExport.AddAttribute( XML_NAMESPACE_LO_EXT, XML_EXTRUSION_SPECULARITY_LOEXT, aStr );
                                            }
                                            // clamp fExtrusionSpecularity to allowed values
                                            if (eVersion >= SvtSaveOptions::ODFSVER_012 && eVersion < SvtSaveOptions::ODFSVER_014)
                                                fExtrusionSpecularity = std::clamp<double>(fExtrusionSpecularity, 0.0, 100.0);
                                            else if (eVersion >= SvtSaveOptions::ODFSVER_014)
                                                fExtrusionSpecularity = std::max<double>(0.0, fExtrusionSpecularity);
                                            // write percent
                                            ::sax::Converter::convertDouble(
                                                aStrBuffer,
                                                fExtrusionSpecularity,
                                                false,
                                                util::MeasureUnit::PERCENT,
                                                util::MeasureUnit::PERCENT);
                                            aStrBuffer.append( '%' );
                                            aStr = aStrBuffer.makeStringAndClear();
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION_SPECULARITY, aStr );
                                        }
                                    }
                                    break;
                                    case EAS_ProjectionMode :
                                    {
                                        drawing::ProjectionMode eProjectionMode;
                                        if ( rProp.Value >>= eProjectionMode )
                                            rExport.AddAttribute( XML_NAMESPACE_DR3D, XML_PROJECTION,
                                                eProjectionMode == drawing::ProjectionMode_PARALLEL ? GetXMLToken( XML_PARALLEL ) : GetXMLToken( XML_PERSPECTIVE ) );
                                    }
                                    break;
                                    case EAS_ViewPoint :
                                    {
                                        drawing::Position3D aExtrusionViewPoint;
                                        if ( rProp.Value >>= aExtrusionViewPoint )
                                        {
                                            rUnitConverter.convertPosition3D( aStrBuffer, aExtrusionViewPoint );
                                            aStr = aStrBuffer.makeStringAndClear();
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION_VIEWPOINT, aStr );
                                        }
                                    }
                                    break;
                                    case EAS_Origin :
                                    {
                                        css::drawing::EnhancedCustomShapeParameterPair aOriginParaPair;
                                        if ( rProp.Value >>= aOriginParaPair )
                                        {
                                            ExportParameter( aStrBuffer, aOriginParaPair.First );
                                            ExportParameter( aStrBuffer, aOriginParaPair.Second );
                                            aStr = aStrBuffer.makeStringAndClear();
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION_ORIGIN, aStr );
                                        }
                                    }
                                    break;
                                    case EAS_Color :
                                    {
                                        bool bExtrusionColor;
                                        if ( rProp.Value >>= bExtrusionColor )
                                        {
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION_COLOR,
                                                bExtrusionColor ? GetXMLToken( XML_TRUE ) : GetXMLToken( XML_FALSE ) );
                                        }
                                    }
                                    break;
                                    default:
                                        break;
                                }
                            }
                            // tdf#141301: no specific skew values provided
                            if (!bSkewValuesProvided)
                            {
                                // so we need to export default values explicitly
                                rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION_SKEW, u"50 -135"_ustr);
                            }
                        }
                    }
                    break;
                    case EAS_TextPath :
                    {
                        uno::Sequence< beans::PropertyValue > aTextPathPropSeq;
                        if ( rGeoProp.Value >>= aTextPathPropSeq )
                        {
                            for (const beans::PropertyValue& rProp : aTextPathPropSeq)
                            {
                                switch( EASGet( rProp.Name ) )
                                {
                                    case EAS_TextPath :
                                    {
                                        bool bTextPathOn;
                                        if ( rProp.Value >>= bTextPathOn )
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_TEXT_PATH,
                                                bTextPathOn ? GetXMLToken( XML_TRUE ) : GetXMLToken( XML_FALSE ) );
                                    }
                                    break;
                                    case EAS_TextPathMode :
                                    {
                                        css::drawing::EnhancedCustomShapeTextPathMode eTextPathMode;
                                        if ( rProp.Value >>= eTextPathMode )
                                        {
                                            switch ( eTextPathMode )
                                            {
                                                case css::drawing::EnhancedCustomShapeTextPathMode_NORMAL: aStr = GetXMLToken( XML_NORMAL ); break;
                                                case css::drawing::EnhancedCustomShapeTextPathMode_PATH  : aStr = GetXMLToken( XML_PATH );   break;
                                                case css::drawing::EnhancedCustomShapeTextPathMode_SHAPE : aStr = GetXMLToken( XML_SHAPE );  break;
                                                default:
                                                    break;
                                            }
                                            if ( !aStr.isEmpty() )
                                                rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_TEXT_PATH_MODE, aStr );
                                        }
                                    }
                                    break;
                                    case EAS_ScaleX :
                                    {
                                        bool bScaleX;
                                        if ( rProp.Value >>= bScaleX )
                                        {
                                            aStr = bScaleX ? GetXMLToken( XML_SHAPE ) : GetXMLToken( XML_PATH );
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_TEXT_PATH_SCALE, aStr );
                                        }
                                    }
                                    break;
                                    case EAS_SameLetterHeights :
                                    {
                                        bool bSameLetterHeights;
                                        if ( rProp.Value >>= bSameLetterHeights )
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_TEXT_PATH_SAME_LETTER_HEIGHTS,
                                                bSameLetterHeights ? GetXMLToken( XML_TRUE ) : GetXMLToken( XML_FALSE ) );
                                    }
                                    break;
                                    default:
                                        break;
                                }
                            }
                        }
                    }
                    break;
                    case EAS_Path :
                    {
                        uno::Sequence< beans::PropertyValue > aPathPropSeq;
                        if ( rGeoProp.Value >>= aPathPropSeq )
                        {
                            for (const beans::PropertyValue& rProp : aPathPropSeq)
                            {
                                switch( EASGet( rProp.Name ) )
                                {
                                    case EAS_SubViewSize:
                                    {
                                        // export draw:sub-view-size (do not export in ODF 1.3 or older)
                                        if ((rExport.getSaneDefaultVersion() & SvtSaveOptions::ODFSVER_EXTENDED) == 0)
                                        {
                                            continue;
                                        }
                                        uno::Sequence< awt::Size > aSubViewSizes;
                                        rProp.Value >>= aSubViewSizes;

                                        for ( int nIdx = 0; nIdx < aSubViewSizes.getLength(); nIdx++ )
                                        {
                                            if ( nIdx )
                                                aStrBuffer.append(' ');
                                            aStrBuffer.append( aSubViewSizes[nIdx].Width );
                                            aStrBuffer.append(' ');
                                            aStrBuffer.append( aSubViewSizes[nIdx].Height );
                                        }
                                        aStr = aStrBuffer.makeStringAndClear();
                                        rExport.AddAttribute( XML_NAMESPACE_DRAW_EXT, XML_SUB_VIEW_SIZE, aStr );
                                    }
                                    break;
                                    case EAS_ExtrusionAllowed :
                                    {
                                        bool bExtrusionAllowed;
                                        if ( rProp.Value >>= bExtrusionAllowed )
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_EXTRUSION_ALLOWED,
                                                bExtrusionAllowed ? GetXMLToken( XML_TRUE ) : GetXMLToken( XML_FALSE ) );
                                    }
                                    break;
                                    case EAS_ConcentricGradientFillAllowed :
                                    {
                                        bool bConcentricGradientFillAllowed;
                                        if ( rProp.Value >>= bConcentricGradientFillAllowed )
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_CONCENTRIC_GRADIENT_FILL_ALLOWED,
                                                bConcentricGradientFillAllowed ? GetXMLToken( XML_TRUE ) : GetXMLToken( XML_FALSE ) );
                                    }
                                    break;
                                    case EAS_TextPathAllowed  :
                                    {
                                        bool bTextPathAllowed;
                                        if ( rProp.Value >>= bTextPathAllowed )
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_TEXT_PATH_ALLOWED,
                                                bTextPathAllowed ? GetXMLToken( XML_TRUE ) : GetXMLToken( XML_FALSE ) );
                                    }
                                    break;
                                    case EAS_GluePoints :
                                    {
                                        css::uno::Sequence< css::drawing::EnhancedCustomShapeParameterPair> aGluePoints;
                                        if ( rProp.Value >>= aGluePoints )
                                        {
                                            if ( aGluePoints.hasElements() )
                                            {
                                                for (const auto& rGluePoint : aGluePoints)
                                                {
                                                    ExportParameter( aStrBuffer, rGluePoint.First );
                                                    ExportParameter( aStrBuffer, rGluePoint.Second );
                                                }
                                                aStr = aStrBuffer.makeStringAndClear();
                                            }
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_GLUE_POINTS, aStr );
                                        }
                                    }
                                    break;
                                    case EAS_GluePointType :
                                    {
                                        sal_Int16 nGluePointType = sal_Int16();
                                        if ( rProp.Value >>= nGluePointType )
                                        {
                                            switch ( nGluePointType )
                                            {
                                                case css::drawing::EnhancedCustomShapeGluePointType::NONE     : aStr = GetXMLToken( XML_NONE );    break;
                                                case css::drawing::EnhancedCustomShapeGluePointType::SEGMENTS : aStr = GetXMLToken( XML_SEGMENTS ); break;
                                                case css::drawing::EnhancedCustomShapeGluePointType::RECT     : aStr = GetXMLToken( XML_RECTANGLE ); break;
                                            }
                                            if ( !aStr.isEmpty() )
                                                rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_GLUE_POINT_TYPE, aStr );
                                        }
                                    }
                                    break;
                                    case EAS_Coordinates :
                                    {
                                        bCoordinates = ( rProp.Value >>= aCoordinates );
                                    }
                                    break;
                                    case EAS_Segments :
                                    {
                                        rProp.Value >>= aSegments;
                                    }
                                    break;
                                    case EAS_StretchX :
                                    {
                                        sal_Int32 nStretchPoint = 0;
                                        if ( rProp.Value >>= nStretchPoint )
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_PATH_STRETCHPOINT_X, OUString::number( nStretchPoint ) );
                                    }
                                    break;
                                    case EAS_StretchY :
                                    {
                                        sal_Int32 nStretchPoint = 0;
                                        if ( rProp.Value >>= nStretchPoint )
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_PATH_STRETCHPOINT_Y, OUString::number( nStretchPoint ) );
                                    }
                                    break;
                                    case EAS_TextFrames :
                                    {
                                        css::uno::Sequence< css::drawing::EnhancedCustomShapeTextFrame > aPathTextFrames;
                                        if ( rProp.Value >>= aPathTextFrames )
                                        {
                                            if ( aPathTextFrames.hasElements() )
                                            {
                                                for (const auto& rPathTextFrame : aPathTextFrames)
                                                {
                                                    ExportParameter( aStrBuffer, rPathTextFrame.TopLeft.First );
                                                    ExportParameter( aStrBuffer, rPathTextFrame.TopLeft.Second );
                                                    ExportParameter( aStrBuffer, rPathTextFrame.BottomRight.First );
                                                    ExportParameter( aStrBuffer, rPathTextFrame.BottomRight.Second );
                                                }
                                                aStr = aStrBuffer.makeStringAndClear();
                                            }
                                            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_TEXT_AREAS, aStr );
                                        }
                                    }
                                    break;
                                    default:
                                        break;
                                }
                            }
                        }
                    }
                    break;
                    case EAS_Equations :
                    {
                        bEquations = ( rGeoProp.Value >>= aEquations );
                    }
                    break;
                    case EAS_Handles :
                    {
                        bHandles = ( rGeoProp.Value >>= aHandles );
                    }
                    break;
                    case EAS_AdjustmentValues :
                    {
                        rGeoProp.Value >>= aAdjustmentValues;
                    }
                    break;
                    default:
                        break;
                }
            }   // for

            // ToDo: Where is TextPreRotateAngle still used? We cannot save it in ODF.
            fTextRotateAngle += fTextPreRotateAngle;
            // Workaround for writing-mode bt-lr and tb-rl90 in ODF strict,
            // otherwise loext:writing-mode is used in style export.
            if (!(rExport.getSaneDefaultVersion() & SvtSaveOptions::ODFSVER_EXTENDED))
            {
                if (xPropSetInfo->hasPropertyByName(u"WritingMode"_ustr))
                {
                    sal_Int16 nDirection = -1;
                    xPropSet->getPropertyValue(u"WritingMode"_ustr) >>= nDirection;
                    if (nDirection == text::WritingMode2::TB_RL90)
                        fTextRotateAngle -= 90;
                    else if (nDirection == text::WritingMode2::BT_LR)
                        fTextRotateAngle -= 270;
                }
            }
            if (fTextRotateAngle != 0)
            {
                ::sax::Converter::convertDouble( aStrBuffer, fTextRotateAngle );
                aStr = aStrBuffer.makeStringAndClear();
                rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_TEXT_ROTATE_ANGLE, aStr );
            }

            rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_TYPE, aCustomShapeType );

            // adjustments
            sal_Int32 nAdjustmentValues = aAdjustmentValues.getLength();
            if ( nAdjustmentValues )
            {
                sal_Int32 i, nValue = 0;
                for ( i = 0; i < nAdjustmentValues; i++ )
                {
                    if ( i )
                        aStrBuffer.append( ' ' );

                    const css::drawing::EnhancedCustomShapeAdjustmentValue& rAdj = aAdjustmentValues[ i ];
                    if ( rAdj.State == beans::PropertyState_DIRECT_VALUE )
                    {
                        if ( rAdj.Value.getValueTypeClass() == uno::TypeClass_DOUBLE )
                        {
                            double fValue = 0.0;
                            rAdj.Value >>= fValue;
                            ::sax::Converter::convertDouble(aStrBuffer, fValue);
                        }
                        else
                        {
                            rAdj.Value >>= nValue;
                            aStrBuffer.append(nValue);
                        }
                    }
                    else
                    {
                        // this should not be, but better than setting nothing
                        aStrBuffer.append("0");
                    }
                }
                aStr = aStrBuffer.makeStringAndClear();
                rExport.AddAttribute( XML_NAMESPACE_DRAW, XML_MODIFIERS, aStr );
            }
            if ( bCoordinates )
                ImpExportEnhancedPath( rExport, aCoordinates, aSegments );
        }
    }
    SvXMLElementExport aOBJ( rExport, XML_NAMESPACE_DRAW, XML_ENHANCED_GEOMETRY, true, true );
    if ( bEquations )
        ImpExportEquations( rExport, aEquations );
    if ( bHandles )
        ImpExportHandles( rExport, aHandles );
}

void XMLShapeExport::ImpExportCustomShape(
    const uno::Reference< drawing::XShape >& xShape,
    XMLShapeExportFlags nFeatures, css::awt::Point* pRefPoint )
{
    const uno::Reference< beans::XPropertySet > xPropSet(xShape, uno::UNO_QUERY);
    if ( !xPropSet.is() )
        return;

    uno::Reference< beans::XPropertySetInfo > xPropSetInfo( xPropSet->getPropertySetInfo() );

    // Transformation
    ImpExportNewTrans( xPropSet, nFeatures, pRefPoint );

    if ( xPropSetInfo.is() )
    {
        OUString aStr;
        if ( xPropSetInfo->hasPropertyByName( u"CustomShapeEngine"_ustr ) )
        {
            uno::Any aEngine( xPropSet->getPropertyValue( u"CustomShapeEngine"_ustr ) );
            if ( ( aEngine >>= aStr ) && !aStr.isEmpty() )
                mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_ENGINE, aStr );
        }
        if ( xPropSetInfo->hasPropertyByName( u"CustomShapeData"_ustr ) )
        {
            uno::Any aData( xPropSet->getPropertyValue( u"CustomShapeData"_ustr ) );
            if ( ( aData >>= aStr ) && !aStr.isEmpty() )
                mrExport.AddAttribute( XML_NAMESPACE_DRAW, XML_DATA, aStr );
        }
    }
    bool bCreateNewline( (nFeatures & XMLShapeExportFlags::NO_WS) == XMLShapeExportFlags::NONE ); // #86116#/#92210#
    SvXMLElementExport aOBJ( mrExport, XML_NAMESPACE_DRAW, XML_CUSTOM_SHAPE, bCreateNewline, true );
    ImpExportDescription( xShape ); // #i68101#
    ImpExportEvents( xShape );
    ImpExportGluePoints( xShape );
    ImpExportText( xShape );
    ImpExportEnhancedGeometry( mrExport, xPropSet );

}

void XMLShapeExport::ImpExportTableShape( const uno::Reference< drawing::XShape >& xShape, XmlShapeType eShapeType, XMLShapeExportFlags nFeatures, css::awt::Point* pRefPoint )
{
    uno::Reference< beans::XPropertySet > xPropSet(xShape, uno::UNO_QUERY);
    uno::Reference< container::XNamed > xNamed(xShape, uno::UNO_QUERY);

    SAL_WARN_IF( !xPropSet.is() || !xNamed.is(), "xmloff", "xmloff::XMLShapeExport::ImpExportTableShape(), table shape is not implementing needed interfaces");
    if(!(xPropSet.is() && xNamed.is()))
        return;

    try
    {
        // Transformation
        ImpExportNewTrans(xPropSet, nFeatures, pRefPoint);

        bool bIsEmptyPresObj = false;

        // presentation settings
        if(eShapeType == XmlShapeType::PresTableShape)
            bIsEmptyPresObj = ImpExportPresentationAttributes( xPropSet, GetXMLToken(XML_TABLE) );

        const bool bCreateNewline( (nFeatures & XMLShapeExportFlags::NO_WS) == XMLShapeExportFlags::NONE );

        SvXMLElementExport aElement( mrExport, XML_NAMESPACE_DRAW, XML_FRAME, bCreateNewline, true );

        // do not export in ODF 1.1 or older
        if (mrExport.getSaneDefaultVersion() >= SvtSaveOptions::ODFSVER_012)
        {
            if( !bIsEmptyPresObj )
            {
                uno::Reference< container::XNamed > xTemplate( xPropSet->getPropertyValue(u"TableTemplate"_ustr), uno::UNO_QUERY );
                if( xTemplate.is() )
                {
                    const OUString sTemplate( xTemplate->getName() );
                    if( !sTemplate.isEmpty() )
                    {
                        mrExport.AddAttribute(XML_NAMESPACE_TABLE, XML_TEMPLATE_NAME, sTemplate );

                        for( const XMLPropertyMapEntry* pEntry = &aXMLTableShapeAttributes[0]; !pEntry->IsEnd(); pEntry++ )
                        {
                            try
                            {
                                bool bBool = false;
                                xPropSet->getPropertyValue( pEntry->getApiName() ) >>= bBool;
                                if( bBool )
                                    mrExport.AddAttribute(pEntry->mnNameSpace, pEntry->meXMLName, XML_TRUE );
                            }
                            catch( uno::Exception& )
                            {
                                DBG_UNHANDLED_EXCEPTION("xmloff.draw");
                            }
                        }
                    }
                }

                uno::Reference< table::XColumnRowRange > xRange( xPropSet->getPropertyValue( gsModel ), uno::UNO_QUERY_THROW );
                GetShapeTableExport()->exportTable( xRange );
            }
        }

        if (!bIsEmptyPresObj
            && officecfg::Office::Common::Save::Graphic::AddReplacementImages::get())
        {
            uno::Reference< graphic::XGraphic > xGraphic( xPropSet->getPropertyValue(u"ReplacementGraphic"_ustr), uno::UNO_QUERY );
            ExportGraphicPreview(xGraphic, mrExport, u"TablePreview", u".svm", u"image/x-vclgraphic"_ustr);
        }

        ImpExportEvents( xShape );
        ImpExportGluePoints( xShape );
        ImpExportDescription( xShape ); // #i68101#
    }
    catch( uno::Exception const & )
    {
        DBG_UNHANDLED_EXCEPTION("xmloff.draw");
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
