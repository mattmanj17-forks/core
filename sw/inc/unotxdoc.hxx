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
#ifndef INCLUDED_SW_INC_UNOTXDOC_HXX
#define INCLUDED_SW_INC_UNOTXDOC_HXX

#include "swdllapi.h"
#include <sfx2/sfxbasemodel.hxx>

#include <com/sun/star/style/XStyleFamiliesSupplier.hpp>
#include <com/sun/star/style/XAutoStylesSupplier.hpp>
#include <com/sun/star/document/XLinkTargetSupplier.hpp>
#include <com/sun/star/document/XRedlinesSupplier.hpp>
#include <com/sun/star/text/XNumberingRulesSupplier.hpp>
#include <com/sun/star/text/XFootnotesSupplier.hpp>
#include <com/sun/star/text/XEndnotesSupplier.hpp>
#include <com/sun/star/text/XContentControlsSupplier.hpp>
#include <com/sun/star/text/XTextSectionsSupplier.hpp>
#include <com/sun/star/text/XLineNumberingProperties.hpp>
#include <com/sun/star/text/XChapterNumberingSupplier.hpp>
#include <com/sun/star/text/XPagePrintable.hpp>
#include <com/sun/star/text/XTextFieldsSupplier.hpp>
#include <com/sun/star/text/XTextGraphicObjectsSupplier.hpp>
#include <com/sun/star/text/XTextTablesSupplier.hpp>
#include <com/sun/star/text/XDocumentIndexesSupplier.hpp>
#include <com/sun/star/text/XBookmarksSupplier.hpp>
#include <com/sun/star/text/XTextDocument.hpp>
#include <com/sun/star/text/XTextEmbeddedObjectsSupplier.hpp>
#include <com/sun/star/text/XReferenceMarksSupplier.hpp>
#include <com/sun/star/text/XTextFramesSupplier.hpp>
#include <com/sun/star/drawing/XDrawPageSupplier.hpp>
#include <com/sun/star/drawing/XDrawPagesSupplier.hpp>
#include <com/sun/star/util/XReplaceable.hpp>
#include <com/sun/star/util/XRefreshable.hpp>
#include <com/sun/star/util/XLinkUpdate.hpp>
#include <com/sun/star/view/XRenderable.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/beans/XPropertyState.hpp>
#include <com/sun/star/xforms/XFormsSupplier.hpp>
#include <com/sun/star/text/XFlatParagraphIteratorProvider.hpp>
#include <com/sun/star/document/XDocumentLanguages.hpp>
#include <com/sun/star/util/XCloneable.hpp>
#include <o3tl/deleter.hxx>
#include <rtl/ref.hxx>
#include <svx/fmdmod.hxx>
#include <editeng/UnoForbiddenCharsTable.hxx>
#include <cppuhelper/implbase.hxx>
#include <vcl/ITiledRenderable.hxx>
#include <com/sun/star/text/XPasteBroadcaster.hpp>

#include "unobaseclass.hxx"
#include "viewopt.hxx"

#include <deque>

class SwDoc;
class SwDocShell;
class SwXBodyText;
class SwFmDrawPage;
class SwUnoCursor;
class SwXDocumentPropertyHelper;
class SfxViewFrame;
class SfxViewShell;
class SwPrintUIOptions;
class SwPrintData;
class SwRenderData;
class SwViewShell;
class SfxItemPropertySet;
class SwXTextTables;
class SwXTextFrames;
class SwXTextGraphicObjects;
class SwXTextEmbeddedObjects;
class SwXTextFieldTypes;
class SwXTextFieldMasters;
class SwXTextSections;
class SwXNumberingRulesCollection;
class SwXFootnote;
class SwXFootnotes;
class SwXContentControls;
class SwXDocumentIndexes;
class SwXStyleFamilies;
class SwXStyle;
class SwXAutoStyles;
class SwXBookmarks;
class SwXChapterNumbering;
class SwXFootnoteProperties;
class SwXEndnoteProperties;
class SwXLineNumberingProperties;
class SwXReferenceMarks;
class SwXLinkTargetSupplier;
class SwXRedlines;
class SwXDocumentSettings;
class SwXTextDefaults;
class SwXBookmark;
class SwXTextSection;
class SwXTextField;
class SwXLineBreak;
class SwXTextFrame;
class SwXTextGraphicObject;
class SwXPageStyle;
class SwXContentControl;
class SwXTextEmbeddedObject;
class SvXMLEmbeddedObjectHelper;
class SwXFieldmark;
class SwXSection;
class SwXFieldMaster;
class SvNumberFormatsSupplierObj;
namespace com::sun::star::frame { class XController; }

typedef cppu::ImplInheritanceHelper
<
    SfxBaseModel,
    css::text::XTextDocument,
    css::text::XLineNumberingProperties,
    css::text::XChapterNumberingSupplier,
    css::text::XNumberingRulesSupplier,
    css::text::XFootnotesSupplier,
    css::text::XEndnotesSupplier,
    css::text::XContentControlsSupplier,
    css::util::XReplaceable,
    css::text::XPagePrintable,
    css::text::XReferenceMarksSupplier,
    css::text::XTextTablesSupplier,
    css::text::XTextFramesSupplier,
    css::text::XBookmarksSupplier,
    css::text::XTextSectionsSupplier,
    css::text::XTextGraphicObjectsSupplier,
    css::text::XTextEmbeddedObjectsSupplier,
    css::text::XTextFieldsSupplier,
    css::style::XStyleFamiliesSupplier,
    css::style::XAutoStylesSupplier,
    css::lang::XServiceInfo,
    css::drawing::XDrawPageSupplier,
    css::drawing::XDrawPagesSupplier,
    css::text::XDocumentIndexesSupplier,
    css::beans::XPropertySet,
    css::beans::XPropertyState,
    css::document::XLinkTargetSupplier,
    css::document::XRedlinesSupplier,
    css::util::XRefreshable,
    css::util::XLinkUpdate,
    css::view::XRenderable,
    css::xforms::XFormsSupplier,
    css::text::XFlatParagraphIteratorProvider,
    css::document::XDocumentLanguages,
    css::util::XCloneable,
    css::text::XPasteBroadcaster
>
SwXTextDocumentBaseClass;

class SW_DLLPUBLIC SwXTextDocument final : public SwXTextDocumentBaseClass,
    public SvxFmMSFactory,
    public vcl::ITiledRenderable
{
private:
    class Impl;
    ::sw::UnoImplPtr<Impl> m_pImpl;

    std::deque<std::unique_ptr<UnoActionContext, o3tl::default_delete<UnoActionContext>>> maActionArr;

    const SfxItemPropertySet* m_pPropSet;

    SwDocShell*             m_pDocShell;

    rtl::Reference<SwFmDrawPage>                                m_xDrawPage;

    rtl::Reference<SwXBodyText>                                 m_xBodyText;
    rtl::Reference< SvNumberFormatsSupplierObj >                m_xNumFormatAgg;

    rtl::Reference<SwXNumberingRulesCollection>                 mxXNumberingRules;
    rtl::Reference<SwXFootnotes>                                mxXFootnotes;
    rtl::Reference<SwXFootnoteProperties>                       mxXFootnoteSettings;
    rtl::Reference<SwXFootnotes>                                mxXEndnotes;
    rtl::Reference<SwXEndnoteProperties>                        mxXEndnoteSettings;
    rtl::Reference<SwXContentControls>                          mxXContentControls;
    rtl::Reference<SwXReferenceMarks>                           mxXReferenceMarks;
    rtl::Reference<SwXTextFieldTypes>                           mxXTextFieldTypes;
    rtl::Reference<SwXTextFieldMasters>                         mxXTextFieldMasters;
    rtl::Reference<SwXTextSections>                             mxXTextSections;
    rtl::Reference<SwXBookmarks>                                mxXBookmarks;
    rtl::Reference<SwXTextTables>                               mxXTextTables;
    rtl::Reference<SwXTextFrames>                               mxXTextFrames;
    rtl::Reference<SwXTextGraphicObjects>                       mxXGraphicObjects;
    rtl::Reference<SwXTextEmbeddedObjects>                      mxXEmbeddedObjects;
    rtl::Reference<SwXStyleFamilies>                            mxXStyleFamilies;
    mutable rtl::Reference<SwXAutoStyles>                       mxXAutoStyles;
    rtl::Reference<SwXChapterNumbering>                         mxXChapterNumbering;
    rtl::Reference<SwXDocumentIndexes>                          mxXDocumentIndexes;

    rtl::Reference<SwXLineNumberingProperties>                  mxXLineNumberingProperties;
    rtl::Reference<SwXLinkTargetSupplier>                       mxLinkTargetSupplier;
    rtl::Reference<SwXRedlines>                                 mxXRedlines;

    //temporary frame to enable PDF export if no valid view is available
    SfxViewFrame*                                   m_pHiddenViewFrame;
    rtl::Reference<SwXDocumentPropertyHelper>       mxPropertyHelper;

    std::unique_ptr<SwPrintUIOptions>               m_pPrintUIOptions;
    std::unique_ptr<SwRenderData>                   m_pRenderData;

    void                    GetNumberFormatter();

    css::uno::Reference<css::uno::XInterface> create(
        OUString const & rServiceName,
        css::uno::Sequence<css::uno::Any> const * arguments);

    // used for XRenderable implementation
    SfxViewShell *  GuessViewShell( /* out */ bool &rbIsSwSrcView, const css::uno::Reference< css::frame::XController >& rController = css::uno::Reference< css::frame::XController >() );
    SwDoc *         GetRenderDoc( SfxViewShell *&rpView, const css::uno::Any& rSelection, bool bIsPDFExport );
    SfxViewShell *  GetRenderView( bool &rbIsSwSrcView, const css::uno::Sequence< css::beans::PropertyValue >& rxOptions, bool bIsPDFExport );

    OUString           maBuildId;

    // boolean for XPagePrintable
    // set in XPagePrintable::printPages(..) to indicate that the PagePrintSettings
    // has to be applied in XRenderable::getRenderer(..) through which the printing
    // is implemented.
    bool m_bApplyPagePrintSettingsFromXPagePrintable;

    /** abstract SdrModel provider */
    virtual SdrModel& getSdrModelFromUnoModel() const override;

    virtual ~SwXTextDocument() override;

    void ThrowIfInvalid() const;
    SwDoc& GetDocOrThrow() const;

public:
    SwXTextDocument(SwDocShell* pShell);

    void NotifyRefreshListeners();
    virtual     css::uno::Any SAL_CALL queryInterface( const css::uno::Type& aType ) override;
    virtual void SAL_CALL acquire(  ) noexcept override;
    virtual void SAL_CALL release(  ) noexcept override;

    //XWeak
    virtual css::uno::Sequence< css::uno::Type > SAL_CALL getTypes(  ) override;

    static const css::uno::Sequence< sal_Int8 > & getUnoTunnelId();

    //XUnoTunnel
    virtual sal_Int64 SAL_CALL getSomething( const css::uno::Sequence< sal_Int8 >& aIdentifier ) override;

    //XTextDocument
    virtual css::uno::Reference< css::text::XText >  SAL_CALL getText() override;
    rtl::Reference< SwXBodyText > getBodyText();
    virtual void SAL_CALL reformat() override;

    using SfxBaseModel::addEventListener;
    using SfxBaseModel::removeEventListener;

    //XModel
    virtual sal_Bool SAL_CALL attachResource( const OUString& aURL, const css::uno::Sequence< css::beans::PropertyValue >& aArgs ) override;
    virtual OUString SAL_CALL getURL(  ) override;
    virtual css::uno::Sequence< css::beans::PropertyValue > SAL_CALL getArgs(  ) override;
    virtual void SAL_CALL connectController( const css::uno::Reference< css::frame::XController >& xController ) override;
    virtual void SAL_CALL disconnectController( const css::uno::Reference< css::frame::XController >& xController ) override;
    virtual void SAL_CALL lockControllers(  ) override;
    virtual void SAL_CALL unlockControllers(  ) override;
    virtual sal_Bool SAL_CALL hasControllersLocked(  ) override;
    virtual css::uno::Reference< css::frame::XController > SAL_CALL getCurrentController(  ) override;
    virtual void SAL_CALL setCurrentController( const css::uno::Reference< css::frame::XController >& xController ) override;
    virtual css::uno::Reference< css::uno::XInterface > SAL_CALL getCurrentSelection(  ) override;

    //XComponent
    virtual void SAL_CALL dispose() override;
    virtual void SAL_CALL addEventListener(const css::uno::Reference< css::lang::XEventListener > & aListener) override;
    virtual void SAL_CALL removeEventListener(const css::uno::Reference< css::lang::XEventListener > & aListener) override;

    //XCloseable
    virtual void SAL_CALL close( sal_Bool bDeliverOwnership ) override;

    //XLineNumberingProperties
    virtual css::uno::Reference< css::beans::XPropertySet > SAL_CALL getLineNumberingProperties() override;

    //XChapterNumberingSupplier
    virtual css::uno::Reference< css::container::XIndexReplace >  SAL_CALL getChapterNumberingRules() override;

    //XNumberingRulesSupplier
    virtual css::uno::Reference< css::container::XIndexAccess > SAL_CALL getNumberingRules() override;

    //XFootnotesSupplier
    virtual css::uno::Reference< css::container::XIndexAccess >  SAL_CALL getFootnotes() override;
    virtual css::uno::Reference< css::beans::XPropertySet >  SAL_CALL getFootnoteSettings() override;

    //XEndnotesSupplier
    virtual css::uno::Reference< css::container::XIndexAccess >  SAL_CALL getEndnotes() override;
    virtual css::uno::Reference< css::beans::XPropertySet >  SAL_CALL getEndnoteSettings() override;

    // XContentControlsSupplier
    css::uno::Reference<css::container::XIndexAccess> SAL_CALL getContentControls() override;

    //XReplaceable
    virtual css::uno::Reference< css::util::XReplaceDescriptor >  SAL_CALL createReplaceDescriptor() override;
    virtual sal_Int32 SAL_CALL replaceAll(const css::uno::Reference< css::util::XSearchDescriptor > & xDesc) override;

    //XSearchable
    virtual css::uno::Reference< css::util::XSearchDescriptor >  SAL_CALL createSearchDescriptor() override;
    virtual css::uno::Reference< css::container::XIndexAccess >  SAL_CALL findAll(const css::uno::Reference< css::util::XSearchDescriptor > & xDesc) override;
    virtual css::uno::Reference< css::uno::XInterface >  SAL_CALL findFirst(const css::uno::Reference< css::util::XSearchDescriptor > & xDesc) override;
    virtual css::uno::Reference< css::uno::XInterface >  SAL_CALL findNext(const css::uno::Reference< css::uno::XInterface > & xStartAt, const css::uno::Reference< css::util::XSearchDescriptor > & xDesc) override;

    //XPagePrintable
    virtual css::uno::Sequence< css::beans::PropertyValue > SAL_CALL getPagePrintSettings() override;
    virtual void SAL_CALL setPagePrintSettings(const css::uno::Sequence< css::beans::PropertyValue >& aSettings) override;
    virtual void SAL_CALL printPages(const css::uno::Sequence< css::beans::PropertyValue >& xOptions) override;

    //XReferenceMarksSupplier
    virtual css::uno::Reference< css::container::XNameAccess >  SAL_CALL getReferenceMarks() override;

    // css::text::XTextFieldsSupplier
    virtual css::uno::Reference< css::container::XEnumerationAccess >  SAL_CALL getTextFields() override;
    virtual css::uno::Reference< css::container::XNameAccess >  SAL_CALL getTextFieldMasters() override;

    // css::text::XTextEmbeddedObjectsSupplier
    virtual css::uno::Reference< css::container::XNameAccess >  SAL_CALL getEmbeddedObjects() override;

    // css::text::XBookmarksSupplier
    virtual css::uno::Reference< css::container::XNameAccess >  SAL_CALL getBookmarks() override;

    // css::text::XTextSectionsSupplier
    virtual css::uno::Reference< css::container::XNameAccess >  SAL_CALL getTextSections() override;

    // css::text::XTextTablesSupplier
    virtual css::uno::Reference< css::container::XNameAccess >  SAL_CALL getTextTables() override;

    // css::text::XTextGraphicObjectsSupplier
    virtual css::uno::Reference< css::container::XNameAccess >  SAL_CALL getGraphicObjects() override;

    // css::text::XTextFramesSupplier
    virtual css::uno::Reference< css::container::XNameAccess >  SAL_CALL getTextFrames() override;

    //XStyleFamiliesSupplier
    virtual css::uno::Reference< css::container::XNameAccess >  SAL_CALL getStyleFamilies() override;

    //XAutoStylesSupplier
    virtual css::uno::Reference< css::style::XAutoStyles > SAL_CALL getAutoStyles(  ) override;

    //XMultiServiceFactory
    virtual css::uno::Reference< css::uno::XInterface >  SAL_CALL createInstance(const OUString& ServiceSpecifier) override;
    virtual css::uno::Reference< css::uno::XInterface >  SAL_CALL createInstanceWithArguments(const OUString& ServiceSpecifier,
                const css::uno::Sequence< css::uno::Any >& Arguments) override;
    virtual css::uno::Sequence< OUString > SAL_CALL getAvailableServiceNames() override;

    //XServiceInfo
    virtual OUString SAL_CALL getImplementationName() override;
    virtual sal_Bool SAL_CALL supportsService(const OUString& ServiceName) override;
    virtual css::uno::Sequence< OUString > SAL_CALL getSupportedServiceNames() override;

    // css::drawing::XDrawPageSupplier
    virtual css::uno::Reference< css::drawing::XDrawPage >  SAL_CALL getDrawPage() override;

    // css::drawing::XDrawPagesSupplier
    virtual css::uno::Reference< css::drawing::XDrawPages > SAL_CALL getDrawPages() override;

    // css::text::XDocumentIndexesSupplier
    virtual css::uno::Reference< css::container::XIndexAccess >  SAL_CALL getDocumentIndexes() override;

    //XPropertySet
    virtual css::uno::Reference< css::beans::XPropertySetInfo > SAL_CALL getPropertySetInfo(  ) override;
    virtual void SAL_CALL setPropertyValue( const OUString& aPropertyName, const css::uno::Any& aValue ) override;
    virtual css::uno::Any SAL_CALL getPropertyValue( const OUString& PropertyName ) override;
    virtual void SAL_CALL addPropertyChangeListener( const OUString& aPropertyName, const css::uno::Reference< css::beans::XPropertyChangeListener >& xListener ) override;
    virtual void SAL_CALL removePropertyChangeListener( const OUString& aPropertyName, const css::uno::Reference< css::beans::XPropertyChangeListener >& aListener ) override;
    virtual void SAL_CALL addVetoableChangeListener( const OUString& PropertyName, const css::uno::Reference< css::beans::XVetoableChangeListener >& aListener ) override;
    virtual void SAL_CALL removeVetoableChangeListener( const OUString& PropertyName, const css::uno::Reference< css::beans::XVetoableChangeListener >& aListener ) override;

    //XPropertyState
    virtual css::beans::PropertyState SAL_CALL getPropertyState( const OUString& rPropertyName ) override;
    virtual css::uno::Sequence< css::beans::PropertyState > SAL_CALL getPropertyStates( const css::uno::Sequence< OUString >& rPropertyNames ) override;
    virtual void SAL_CALL setPropertyToDefault( const OUString& rPropertyName ) override;
    virtual css::uno::Any SAL_CALL getPropertyDefault( const OUString& rPropertyName ) override;

    //XLinkTargetSupplier
    virtual css::uno::Reference< css::container::XNameAccess >  SAL_CALL getLinks() override;

    //XRedlinesSupplier
    virtual css::uno::Reference< css::container::XEnumerationAccess > SAL_CALL getRedlines(  ) override;

    // css::util::XRefreshable
    virtual void SAL_CALL refresh() override;
    virtual void SAL_CALL addRefreshListener(const css::uno::Reference< css::util::XRefreshListener > & l) override;
    virtual void SAL_CALL removeRefreshListener(const css::uno::Reference< css::util::XRefreshListener > & l) override;

    // css::util::XLinkUpdate,
    virtual void SAL_CALL updateLinks(  ) override;

    // css::view::XRenderable
    virtual sal_Int32 SAL_CALL getRendererCount( const css::uno::Any& aSelection, const css::uno::Sequence< css::beans::PropertyValue >& xOptions ) override;
    virtual css::uno::Sequence< css::beans::PropertyValue > SAL_CALL getRenderer( sal_Int32 nRenderer, const css::uno::Any& aSelection, const css::uno::Sequence< css::beans::PropertyValue >& xOptions ) override;
    virtual void SAL_CALL render( sal_Int32 nRenderer, const css::uno::Any& aSelection, const css::uno::Sequence< css::beans::PropertyValue >& xOptions ) override;

    // css::xforms::XFormsSupplier
    virtual css::uno::Reference< css::container::XNameContainer > SAL_CALL getXForms(  ) override;

    // css::document::XDocumentLanguages
    virtual css::uno::Sequence< css::lang::Locale > SAL_CALL getDocumentLanguages( ::sal_Int16 nScriptTypes, ::sal_Int16 nCount ) override;

    // css::text::XFlatParagraphIteratorProvider:
    virtual css::uno::Reference< css::text::XFlatParagraphIterator > SAL_CALL getFlatParagraphIterator(::sal_Int32 nTextMarkupType, sal_Bool bAutomatic ) override;

    // css::util::XCloneable
    virtual css::uno::Reference< css::util::XCloneable > SAL_CALL createClone(  ) override;

    // css::text::XPasteBroadcaster
    void SAL_CALL addPasteEventListener(
        const ::css::uno::Reference<::css::text::XPasteListener>& xListener) override;
    void SAL_CALL removePasteEventListener(
        const ::css::uno::Reference<::css::text::XPasteListener>& xListener) override;

    /// @see vcl::ITiledRenderable::paintTile().
    virtual void paintTile( VirtualDevice &rDevice,
                            int nOutputWidth,
                            int nOutputHeight,
                            int nTilePosX,
                            int nTilePosY,
                            tools::Long nTileWidth,
                            tools::Long nTileHeight ) override;
    /// @see vcl::ITiledRenderable::getDocumentSize().
    virtual Size getDocumentSize() override;
    /// @see vcl::ITiledRenderable::setPart().
    virtual void setPart(int nPart, bool bAllowChangeFocus = true) override;
    /// @see vcl::ITiledRenderable::getParts().
    virtual int getParts() override;
    /// @see vcl::ITiledRenderable::getPart().
    virtual int getPart() override;
    /// @see vcl::ITiledRenderable::getPartName().
    virtual OUString getPartName(int nPart) override;
    /// @see vcl::ITiledRenderable::getPartHash().
    virtual OUString getPartHash(int nPart) override;
    /// @see vcl::ITiledRenderable::getDocWindow().
    virtual VclPtr<vcl::Window> getDocWindow() override;
    /// @see vcl::ITiledRenderable::initializeForTiledRendering().
    virtual void initializeForTiledRendering(const css::uno::Sequence<css::beans::PropertyValue>& rArguments) override;
    /// @see vcl::ITiledRenderable::postKeyEvent().
    virtual void postKeyEvent(int nType, int nCharCode, int nKeyCode) override;
    /// @see vcl::ITiledRenderable::postMouseEvent().
    virtual void postMouseEvent(int nType, int nX, int nY, int nCount, int nButtons, int nModifier) override;
    /// @see vcl::ITiledRenderable::setTextSelection().
    virtual void setTextSelection(int nType, int nX, int nY) override;
    /// @see vcl::ITiledRenderable::getSelection().
    virtual css::uno::Reference<css::datatransfer::XTransferable> getSelection() override;
    /// @see vcl::ITiledRenderable::setGraphicSelection().
    virtual void setGraphicSelection(int nType, int nX, int nY) override;
    /// @see vcl::ITiledRenderable::resetSelection().
    virtual void resetSelection() override;
    /// @see vcl::ITiledRenderable::getPartPageRectangles().
    virtual OUString getPartPageRectangles() override;
    /// @see vcl::ITiledRenderable::setClipboard().
    virtual void setClipboard(const css::uno::Reference<css::datatransfer::clipboard::XClipboard>& xClipboard) override;
    /// @see vcl::ITiledRenderable::isMimeTypeSupported().
    virtual bool isMimeTypeSupported() override;
    /// @see vcl::ITiledRenderable::setClientVisibleArea().
    virtual void setClientVisibleArea(const tools::Rectangle& rRectangle) override;
    /// @see vcl::ITiledRenderable::setClientZoom.
    virtual void setClientZoom(int nTilePixelWidth_, int nTilePixelHeight_, int nTileTwipWidth_, int nTileTwipHeight_) override;
    /// @see vcl::ITiledRenderable::getPointer().
    virtual PointerStyle getPointer() override;
    /// @see vcl::ITiledRenderable::getTrackedChanges().
    void getTrackedChanges(tools::JsonWriter&) override;
    /// @see vcl::ITiledRenderable::getTrackedChangeAuthors().
    void getTrackedChangeAuthors(tools::JsonWriter& rJsonWriter) override;

    void getRulerState(tools::JsonWriter& rJsonWriter) override;
    /// @see vcl::ITiledRenderable::getPostIts().
    void getPostIts(tools::JsonWriter& rJsonWriter) override;

    /// @see vcl::ITiledRenderable::executeFromFieldEvent().
    virtual void executeFromFieldEvent(const StringMap& aArguments) override;

    /// @see vcl::ITiledRenderable::getSearchResultRectangles().
    std::vector<basegfx::B2DRange> getSearchResultRectangles(const char* pPayload) override;

    /// @see vcl::ITiledRenderable::executeContentControlEvent().
    void executeContentControlEvent(const StringMap& aArguments) override;

    /// @see vcl::ITiledRenderable::getCommandValues().
    void getCommandValues(tools::JsonWriter& rJsonWriter, std::string_view rCommand) override;

    /// @see vcl::ITiledRenderable::getViewRenderState().
    OString getViewRenderState(SfxViewShell* pViewShell = nullptr) override;

    /// @see vcl::ITiledRenderable::supportsCommand().
    bool supportsCommand(std::u16string_view rCommand) override;

    void                        Invalidate();
    void                        Reactivate(SwDocShell* pNewDocShell);
    SwXDocumentPropertyHelper * GetPropertyHelper ();

    void                        InitNewDoc();

    SwUnoCursor* CreateCursorForSearch(css::uno::Reference< css::text::XTextCursor > & xCursor);
    SwUnoCursor* FindAny(const css::uno::Reference< css::util::XSearchDescriptor > & xDesc,
                                            css::uno::Reference< css::text::XTextCursor > & xCursor, bool bAll,
                                            sal_Int32& nResult,
                                            css::uno::Reference< css::uno::XInterface > const & xLastResult);

    SwDocShell*                 GetDocShell() {return m_pDocShell;}

    rtl::Reference<SwXDocumentIndexes> getSwDocumentIndexes();
    rtl::Reference<SwXTextTables> getSwTextTables();
    rtl::Reference<SwFmDrawPage> getSwDrawPage();
    rtl::Reference<SwXFootnotes> getSwXFootnotes();
    rtl::Reference<SwXFootnotes> getSwXEndnotes();
    rtl::Reference<SwXTextFieldMasters> getSwXTextFieldMasters();
    rtl::Reference<SwXFieldMaster> createFieldMaster(std::u16string_view sServiceName);
    rtl::Reference<SwXTextField> createTextField(std::u16string_view sServiceName);
    rtl::Reference<SwXFieldmark> createFieldmark(std::u16string_view sServiceName);
    /// returns either SwXDocumentIndex or SwXTextSection
    rtl::Reference<SwXSection> createSection(std::u16string_view rObjectType);
    rtl::Reference<SwXDocumentSettings> createDocumentSettings();
    rtl::Reference<SwXTextDefaults> createTextDefaults();
    rtl::Reference<SwXBookmark> createBookmark();
    rtl::Reference<SwXFieldmark> createFieldmark();
    rtl::Reference<SwXTextSection> createTextSection();
    rtl::Reference<SwXTextField> createFieldAnnotation();
    rtl::Reference<SwXLineBreak> createLineBreak();
    rtl::Reference<SwXTextFrame> createTextFrame();
    rtl::Reference<SwXTextGraphicObject> createTextGraphicObject();
    rtl::Reference<SwXStyle> createNumberingStyle();
    rtl::Reference<SwXStyle> createCharacterStyle();
    rtl::Reference<SwXStyle> createParagraphStyle();
    rtl::Reference<SwXPageStyle> createPageStyle();
    rtl::Reference<SwXContentControl> createContentControl();
    rtl::Reference<SwXFootnote> createFootnote();
    rtl::Reference<SwXFootnote> createEndnote();
    rtl::Reference<SwXTextEmbeddedObject> createTextEmbeddedObject();
    rtl::Reference<SvXMLEmbeddedObjectHelper> createEmbeddedObjectResolver();
    rtl::Reference< SwXStyleFamilies > getSwStyleFamilies();
    rtl::Reference< SwXRedlines > getSwRedlines();
    rtl::Reference<SwXTextFieldTypes> getSwTextFields();
    rtl::Reference<SwXTextFrames> getSwTextFrames();
};

class SwXLinkTargetSupplier final : public cppu::WeakImplHelper
<
    css::container::XNameAccess,
    css::lang::XServiceInfo
>
{
    SwXTextDocument* m_pxDoc;
    OUString m_sTables;
    OUString m_sFrames;
    OUString m_sGraphics;
    OUString m_sOLEs;
    OUString m_sSections;
    OUString m_sOutlines;
    OUString m_sBookmarks;
    OUString m_sDrawingObjects;

public:
    SwXLinkTargetSupplier(SwXTextDocument& rxDoc);
    virtual ~SwXLinkTargetSupplier() override;

    //XNameAccess
    virtual css::uno::Any SAL_CALL getByName(const OUString& Name) override;
    virtual css::uno::Sequence< OUString > SAL_CALL getElementNames() override;
    virtual sal_Bool SAL_CALL hasByName(const OUString& Name) override;

    //XElementAccess
    virtual css::uno::Type SAL_CALL getElementType(  ) override;
    virtual sal_Bool SAL_CALL hasElements(  ) override;

    //XServiceInfo
    virtual OUString SAL_CALL getImplementationName() override;
    virtual sal_Bool SAL_CALL supportsService(const OUString& ServiceName) override;
    virtual css::uno::Sequence< OUString > SAL_CALL getSupportedServiceNames() override;

    void    Invalidate() {m_pxDoc = nullptr;}
};

class SwXLinkNameAccessWrapper final : public cppu::WeakImplHelper
<
    css::beans::XPropertySet,
    css::container::XNameAccess,
    css::lang::XServiceInfo,
    css::document::XLinkTargetSupplier
>
{
    css::uno::Reference< css::container::XNameAccess >    m_xRealAccess;
    const SfxItemPropertySet*                             m_pPropSet;
    const OUString                                        m_sLinkSuffix;
    const OUString                                        m_sLinkDisplayName;
    SwXTextDocument*                                      m_pxDoc;

public:
    SwXLinkNameAccessWrapper(css::uno::Reference< css::container::XNameAccess >  const & xAccess,
            OUString aLinkDisplayName, OUString  sSuffix);
    SwXLinkNameAccessWrapper(SwXTextDocument& rxDoc,
            OUString aLinkDisplayName, OUString sSuffix);
    virtual ~SwXLinkNameAccessWrapper() override;

    //XNameAccess
    virtual css::uno::Any SAL_CALL getByName(const OUString& Name) override;
    virtual css::uno::Sequence< OUString > SAL_CALL getElementNames() override;
    virtual sal_Bool SAL_CALL hasByName(const OUString& Name) override;

    //XElementAccess
    virtual css::uno::Type SAL_CALL getElementType(  ) override;
    virtual sal_Bool SAL_CALL hasElements(  ) override;

    //XPropertySet
    virtual css::uno::Reference< css::beans::XPropertySetInfo > SAL_CALL getPropertySetInfo(  ) override;
    virtual void SAL_CALL setPropertyValue( const OUString& aPropertyName, const css::uno::Any& aValue ) override;
    virtual css::uno::Any SAL_CALL getPropertyValue( const OUString& PropertyName ) override;
    virtual void SAL_CALL addPropertyChangeListener( const OUString& aPropertyName, const css::uno::Reference< css::beans::XPropertyChangeListener >& xListener ) override;
    virtual void SAL_CALL removePropertyChangeListener( const OUString& aPropertyName, const css::uno::Reference< css::beans::XPropertyChangeListener >& aListener ) override;
    virtual void SAL_CALL addVetoableChangeListener( const OUString& PropertyName, const css::uno::Reference< css::beans::XVetoableChangeListener >& aListener ) override;
    virtual void SAL_CALL removeVetoableChangeListener( const OUString& PropertyName, const css::uno::Reference< css::beans::XVetoableChangeListener >& aListener ) override;

    //XLinkTargetSupplier
    virtual css::uno::Reference< css::container::XNameAccess >  SAL_CALL getLinks() override;

    //XServiceInfo
    virtual OUString SAL_CALL getImplementationName() override;
    virtual sal_Bool SAL_CALL supportsService(const OUString& ServiceName) override;
    virtual css::uno::Sequence< OUString > SAL_CALL getSupportedServiceNames() override;

};

class SwXOutlineTarget final : public cppu::WeakImplHelper
<
    css::beans::XPropertySet,
    css::lang::XServiceInfo
>
{
    const SfxItemPropertySet*   m_pPropSet;
    OUString                    m_sOutlineText;
    OUString                    m_sActualText;
    const sal_Int32             m_nOutlineLevel;

public:
    SwXOutlineTarget(OUString aOutlineText, OUString aActualText,
                     const sal_Int32 nOutlineLevel);
    virtual ~SwXOutlineTarget() override;

    //XPropertySet
    virtual css::uno::Reference< css::beans::XPropertySetInfo > SAL_CALL getPropertySetInfo(  ) override;
    virtual void SAL_CALL setPropertyValue( const OUString& aPropertyName, const css::uno::Any& aValue ) override;
    virtual css::uno::Any SAL_CALL getPropertyValue( const OUString& PropertyName ) override;
    virtual void SAL_CALL addPropertyChangeListener( const OUString& aPropertyName, const css::uno::Reference< css::beans::XPropertyChangeListener >& xListener ) override;
    virtual void SAL_CALL removePropertyChangeListener( const OUString& aPropertyName, const css::uno::Reference< css::beans::XPropertyChangeListener >& aListener ) override;
    virtual void SAL_CALL addVetoableChangeListener( const OUString& PropertyName, const css::uno::Reference< css::beans::XVetoableChangeListener >& aListener ) override;
    virtual void SAL_CALL removeVetoableChangeListener( const OUString& PropertyName, const css::uno::Reference< css::beans::XVetoableChangeListener >& aListener ) override;

    //XServiceInfo
    virtual OUString SAL_CALL getImplementationName() override;
    virtual sal_Bool SAL_CALL supportsService(const OUString& ServiceName) override;
    virtual css::uno::Sequence< OUString > SAL_CALL getSupportedServiceNames() override;
};

class SwXDrawingObjectTarget final : public cppu::WeakImplHelper
<
    css::beans::XPropertySet,
    css::lang::XServiceInfo
>
{
    const SfxItemPropertySet*   m_pPropSet;
    OUString                    m_sDrawingObjectText;

public:
    SwXDrawingObjectTarget(OUString aDrawingObjectText);
    virtual ~SwXDrawingObjectTarget() override;

    //XPropertySet
    virtual css::uno::Reference< css::beans::XPropertySetInfo > SAL_CALL getPropertySetInfo(  ) override;
    virtual void SAL_CALL setPropertyValue( const OUString& aPropertyName, const css::uno::Any& aValue ) override;
    virtual css::uno::Any SAL_CALL getPropertyValue( const OUString& PropertyName ) override;
    virtual void SAL_CALL addPropertyChangeListener( const OUString& aPropertyName, const css::uno::Reference< css::beans::XPropertyChangeListener >& xListener ) override;
    virtual void SAL_CALL removePropertyChangeListener( const OUString& aPropertyName, const css::uno::Reference< css::beans::XPropertyChangeListener >& aListener ) override;
    virtual void SAL_CALL addVetoableChangeListener( const OUString& PropertyName, const css::uno::Reference< css::beans::XVetoableChangeListener >& aListener ) override;
    virtual void SAL_CALL removeVetoableChangeListener( const OUString& PropertyName, const css::uno::Reference< css::beans::XVetoableChangeListener >& aListener ) override;

    //XServiceInfo
    virtual OUString SAL_CALL getImplementationName() override;
    virtual sal_Bool SAL_CALL supportsService(const OUString& ServiceName) override;
    virtual css::uno::Sequence< OUString > SAL_CALL getSupportedServiceNames() override;
};


enum class SwCreateDrawTable {
    Dash = 1, Gradient, Hatch, Bitmap, TransGradient, Marker, Defaults
};

class SwXDocumentPropertyHelper final : public SvxUnoForbiddenCharsTable
{
    css::uno::Reference < css::uno::XInterface > m_xDashTable;
    css::uno::Reference < css::uno::XInterface > m_xGradientTable;
    css::uno::Reference < css::uno::XInterface > m_xHatchTable;
    css::uno::Reference < css::uno::XInterface > m_xBitmapTable;
    css::uno::Reference < css::uno::XInterface > m_xTransGradientTable;
    css::uno::Reference < css::uno::XInterface > m_xMarkerTable;
    css::uno::Reference < css::uno::XInterface > m_xDrawDefaults;

    SwDoc*  m_pDoc;
public:
    SwXDocumentPropertyHelper(SwDoc& rDoc);
    virtual ~SwXDocumentPropertyHelper() override;
    css::uno::Reference<css::uno::XInterface> GetDrawTable(SwCreateDrawTable nWhich);
    void Invalidate();

    virtual void onChange() override;
};

// The class SwViewOptionAdjust_Impl is used to adjust the SwViewOption of
// the current SwViewShell so that fields are not printed as commands and
// hidden characters are always invisible. Hidden text and place holders
// should be printed according to the current print options.
// After printing the view options are restored
class SwViewOptionAdjust_Impl
{
    SwViewShell *   m_pShell;
    SwViewOption    m_aOldViewOptions;
public:
    SwViewOptionAdjust_Impl( SwViewShell& rSh, const SwViewOption &rViewOptions );
    ~SwViewOptionAdjust_Impl();
    void AdjustViewOptions( SwPrintData const* const pPrtOptions, bool setShowPlaceHoldersInPDF );
    bool checkShell( const SwViewShell& rCompare ) const
    { return &rCompare == m_pShell; }
    void DontTouchThatViewShellItSmellsFunny() { m_pShell = nullptr; }
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
