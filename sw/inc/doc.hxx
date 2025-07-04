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
#pragma once

// SwDoc interfaces
#include <o3tl/typed_flags_set.hxx>
#include <o3tl/sorted_vector.hxx>
#include <vcl/idle.hxx>
#include "swdllapi.h"
#include "swtypes.hxx"
#include "toxe.hxx"
#include "flyenum.hxx"
#include "flypos.hxx"
#include "swdbdata.hxx"
#include <sfx2/objsh.hxx>
#include <svl/style.hxx>
#include <editeng/numitem.hxx>
#include "tox.hxx"
#include "frmfmt.hxx"
#include "frameformats.hxx"
#include "charfmt.hxx"
#include "docary.hxx"
#include "charformats.hxx"
#include "pagedesc.hxx"
#include "tblenum.hxx"
#include "ndarr.hxx"
#include "ndtyp.hxx"
#include <memory>
#include <mutex>
#include <set>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace editeng { class SvxBorderLine; }

class SwExtTextInput;
class EditFieldInfo;
class Outliner;
class OutputDevice;
class Point;
class SbxArray;
class SdrObject;
class SdrUndoAction;
class SvNumberFormatter;
class SvxMacro;
class SwAutoCompleteWord;
class SwAutoCorrExceptWord;
class SwCellFrame;
class SwCellStyleTable;
class SwCursorShell;
class SwCursor;
class SwDocShell;
class SwDrawView;
class SwEditShell;
class SwFormat;
class SwFormatINetFormat;
class SwFormatRefMark;
class SwFootnoteIdxs;
class SwFootnoteInfo;
class SwEndNoteInfo;
class SwLineNumberInfo;
class SwDBManager;
class SwNodeIndex;
class SwNodeRange;
class SwNumRule;
class SwPagePreviewPrtData;
class SwRootFrame;
class SwRubyListEntry;
class SwSectionFormat;
class SwSectionData;
class SwSelBoxes;
class SwTableAutoFormatTable;
class SwTOXBaseSection;
class SwTabCols;
class SwTable;
class SwTableAutoFormat;
class SwTableBox;
class SwTableBoxFormat;
class SwTableFormat;
class SwTableLineFormat;
class SwTableNode;
class SwTextBlocks;
class SwURLStateChanged;
class SwUnoCursor;
class SwViewShell;
class SwDrawContact;
class SdrView;
class SdrMarkList;
class SwAuthEntry;
class SwLayoutCache;
class IStyleAccess;
struct SwCallMouseEvent;
struct SwDocStat;
struct SwSortOptions;
struct SwDefTOXBase_Impl;
class SwPrintUIOptions;
struct SwConversionArgs;
class SwRenderData;
class IDocumentUndoRedo;
class IDocumentSettingAccess;
class IDocumentDeviceAccess;
class IDocumentDrawModelAccess;
class IDocumentChartDataProviderAccess;
class IDocumentTimerAccess;
class IDocumentLinksAdministration;
class IDocumentListItems;
class IDocumentListsAccess;
class IDocumentOutlineNodes;
class IDocumentContentOperations;
class IDocumentRedlineAccess;
class IDocumentStatistics;
class IDocumentState;
class IDocumentLayoutAccess;
class IDocumentStylePoolAccess;
class IDocumentExternalData;
class IDocumentMarkAccess;
class SetGetExpFields;
struct SwInsertTableOptions;
class SwContentControlManager;
enum class SvMacroItemId : sal_uInt16;
enum class SvxFrameDirection;
enum class RndStdIds;
class SwMarkName;

namespace sw::mark { class MarkManager; }
namespace sw {
    enum class RedlineMode;
    enum class FieldmarkMode;
    enum class ParagraphBreakMode;
    class MetaFieldManager;
    class UndoManager;
    class IShellCursorSupplier;
    class DocumentSettingManager;
    class DocumentDeviceManager;
    class DocumentDrawModelManager;
    class DocumentChartDataProviderManager;
    class DocumentTimerManager;
    class DocumentLinksAdministrationManager;
    class DocumentListItemsManager;
    class DocumentListsManager;
    class DocumentOutlineNodesManager;
    class DocumentContentOperationsManager;
    class DocumentRedlineManager;
    class DocumentFieldsManager;
    class DocumentStatisticsManager;
    class DocumentStateManager;
    class DocumentLayoutManager;
    class DocumentStylePoolManager;
    class DocumentExternalDataManager;
    class GrammarContact;
    class OnlineAccessibilityCheck;
}

namespace com::sun::star {
    namespace container {
        class XNameContainer; //< for getXForms()/isXForms()/initXForms() methods
    }
    namespace embed { class XStorage; }
    namespace linguistic2 { class XHyphenatedWord; }
    namespace linguistic2 { class XProofreadingIterator; }
    namespace linguistic2 { class XSpellChecker1; }
    namespace script::vba { class XVBAEventProcessor; }
}

namespace ooo::vba::word {
    class XFind;
}

namespace sfx2 {
    class IXmlIdRegistry;
}

void SetAllScriptItem( SfxItemSet& rSet, const SfxPoolItem& rItem );

using SwRubyList = std::vector<std::unique_ptr<SwRubyListEntry>>;

// Represents the model of a Writer document.
class SwDoc final
{
    friend class ::sw::DocumentContentOperationsManager;

    friend void InitCore();
    friend void FinitCore();

    // private Member
    std::unique_ptr<SwNodes> m_pNodes;    //< document content (Nodes Array)
    rtl::Reference<SwAttrPool> mpAttrPool;  //< the attribute pool
    SwPageDescs              m_PageDescs; //< PageDescriptors
    Link<bool,void>          maOle2Link;  //< OLE 2.0-notification
    /* @@@MAINTAINABILITY-HORROR@@@
       Timer should not be members of the model
    */
    Idle       maOLEModifiedIdle;        //< Timer for update modified OLE-Objects
    SwDBData    maDBData;                //< database descriptor
    OUString    msTOIAutoMarkURL;        //< URL of table of index AutoMark file
    std::vector<OUString> m_PatternNames; //< Array for names of document-templates
    css::uno::Reference<css::container::XNameContainer>
        mxXForms;                        //< container with XForms models
    mutable css::uno::Reference< css::linguistic2::XProofreadingIterator > m_xGCIterator;

    const std::unique_ptr< ::sw::mark::MarkManager> mpMarkManager;
    const std::unique_ptr< ::sw::MetaFieldManager > m_pMetaFieldManager;
    const std::unique_ptr< ::SwContentControlManager > m_pContentControlManager;
    const std::unique_ptr< ::sw::DocumentDrawModelManager > m_pDocumentDrawModelManager;
    const std::unique_ptr< ::sw::DocumentRedlineManager > m_pDocumentRedlineManager;
    const std::unique_ptr< ::sw::DocumentStateManager > m_pDocumentStateManager;
    const std::unique_ptr< ::sw::UndoManager > m_pUndoManager;
    const std::unique_ptr< ::sw::DocumentSettingManager > m_pDocumentSettingManager;
    const std::unique_ptr< ::sw::DocumentChartDataProviderManager > m_pDocumentChartDataProviderManager;
    std::unique_ptr< ::sw::DocumentDeviceManager > m_pDeviceAccess;
    const std::unique_ptr< ::sw::DocumentTimerManager > m_pDocumentTimerManager;
    const std::unique_ptr< ::sw::DocumentLinksAdministrationManager > m_pDocumentLinksAdministrationManager;
    const std::unique_ptr< ::sw::DocumentListItemsManager > m_pDocumentListItemsManager;
    const std::unique_ptr< ::sw::DocumentListsManager > m_pDocumentListsManager;
    const std::unique_ptr< ::sw::DocumentOutlineNodesManager > m_pDocumentOutlineNodesManager;
    const std::unique_ptr< ::sw::DocumentContentOperationsManager > m_pDocumentContentOperationsManager;
    const std::unique_ptr< ::sw::DocumentFieldsManager > m_pDocumentFieldsManager;
    const std::unique_ptr< ::sw::DocumentStatisticsManager > m_pDocumentStatisticsManager;
    const std::unique_ptr< ::sw::DocumentLayoutManager > m_pDocumentLayoutManager;
    const std::unique_ptr< ::sw::DocumentStylePoolManager > m_pDocumentStylePoolManager;
    const std::unique_ptr< ::sw::DocumentExternalDataManager > m_pDocumentExternalDataManager;

    // Pointer
    std::unique_ptr<SwFrameFormat>     mpDfltFrameFormat;     //< Default formats.
    std::unique_ptr<SwFrameFormat>     mpEmptyPageFormat;     //< Format for the default empty page
    std::unique_ptr<SwFrameFormat>     mpColumnContFormat;    //< Format for column container
    std::unique_ptr<SwCharFormat>      mpDfltCharFormat;
    std::unique_ptr<SwTextFormatColl>  mpDfltTextFormatColl;  //< Defaultformatcollections
    std::unique_ptr<SwGrfFormatColl>   mpDfltGrfFormatColl;

    std::unique_ptr<sw::FrameFormats<SwFrameFormat*>>    mpFrameFormatTable;    //< Format table
    std::unique_ptr<SwCharFormats>     mpCharFormatTable;
    std::unique_ptr<SwCharFormats>     mpCharFormatDeletionTable;
    std::unique_ptr<sw::FrameFormats<sw::SpzFrameFormat*>>    mpSpzFrameFormatTable;
    std::unique_ptr<SwSectionFormats>  mpSectionFormatTable;
    std::unique_ptr<sw::TableFrameFormats>    mpTableFrameFormatTable; //< For tables
    std::unique_ptr<SwTextFormatColls> mpTextFormatCollTable;   //< FormatCollections
    std::unique_ptr<SwGrfFormatColls>  mpGrfFormatCollTable;

    std::unique_ptr<SwTOXTypes>        mpTOXTypes;              //< Tables/indices
    std::unique_ptr<SwDefTOXBase_Impl> mpDefTOXBases;           //< defaults of SwTOXBase's

    std::unique_ptr<SwDBManager> m_pOwnDBManager; //< own DBManager
    SwDBManager * m_pDBManager; //< DBManager for evaluation of DB-fields.

    SwNumRule       *mpOutlineRule;
    std::unique_ptr<SwFootnoteInfo>   mpFootnoteInfo;
    std::unique_ptr<SwEndNoteInfo>    mpEndNoteInfo;
    std::unique_ptr<SwLineNumberInfo> mpLineNumberInfo;
    std::unique_ptr<SwFootnoteIdxs>   mpFootnoteIdxs;

    SwDocShell      *mpDocShell;                   //< Ptr to SfxDocShell of Doc.
    SfxObjectShellLock mxTmpDocShell;              //< A temporary shell that is used to copy OLE-Nodes

    std::unique_ptr<SwAutoCorrExceptWord> mpACEWord;               /**< For the automated takeover of
                                                   auto-corrected words that are "re-corrected". */
    std::unique_ptr<SwURLStateChanged> mpURLStateChgd;             //< SfxClient for changes in INetHistory

    std::mutex mNumberFormatterMutex;
    SvNumberFormatter* mpNumberFormatter;             //< NumFormatter for tables / fields

    mutable std::unique_ptr<SwNumRuleTable> mpNumRuleTable;     //< List of all named NumRules.

    // Hash map to find numrules by name
    mutable std::unordered_map<UIName, SwNumRule *> maNumRuleMap;

    std::unique_ptr<SwPagePreviewPrtData> m_pPgPViewPrtData; //< Indenting / spacing for printing of page view.
    SwExtTextInput  *mpExtInputRing;

    std::unique_ptr<IStyleAccess>  mpStyleAccess;                //< handling of automatic styles
    std::unique_ptr<SwLayoutCache> mpLayoutCache;                /**< Layout cache to read and save with the
                                                                    document for a faster formatting */

    std::unique_ptr<sw::GrammarContact> mpGrammarContact; //< for grammar checking in paragraphs during editing
    std::unique_ptr<sw::OnlineAccessibilityCheck> mpOnlineAccessibilityCheck;

    css::uno::Reference< css::script::vba::XVBAEventProcessor > mxVbaEvents;
    css::uno::Reference< ooo::vba::word::XFind > mxVbaFind;
    css::uno::Reference<css::container::XNameContainer> m_xTemplateToProjectCache;

    /// Table styles (autoformats that are applied with table changes).
    std::unique_ptr<SwTableAutoFormatTable> m_pTableStyles;
    /// Cell Styles not assigned to a Table Style
    std::unique_ptr<SwCellStyleTable> mpCellStyles;
private:
    std::unique_ptr< ::sfx2::IXmlIdRegistry > m_pXmlIdRegistry;

    // other

    sal_uInt32  mnRsid;              //< current session ID of the document
    sal_uInt32  mnRsidRoot;          //< session ID when the document was created

    oslInterlockedCount  mReferenceCount;

    bool mbDtor                  : 1;    /**< TRUE: is in SwDoc DTOR.
                                               and unfortunately temporarily also in
                                               SwSwgReader::InLayout() when flawed
                                               frames need deletion. */
    bool mbCopyIsMove            : 1;    //< TRUE: Copy is a hidden Move.
    bool mbInReading             : 1;    //< TRUE: Document is in the process of being read.
    bool mbInWriting : 1; //< TRUE: Document is in the process of being written.
    bool mbInMailMerge           : 1;    //< TRUE: Document is in the process of being written by mail merge.
    bool mbInXMLImport           : 1;    //< TRUE: During xml import, attribute portion building is not necessary.
    bool mbInXMLImport242        : 1 = false; //< TRUE: During xml import, apply workaround for style-ref field
    bool mbInWriterfilterImport  : 1;    //< TRUE: writerfilter import (DOCX,RTF)
    bool mbUpdateTOX             : 1;    //< TRUE: After loading document, update TOX.
    bool mbInLoadAsynchron       : 1;    //< TRUE: Document is in the process of being loaded asynchronously.
    bool mbIsAutoFormatRedline   : 1;    //< TRUE: Redlines are recorded by Autoformat.
    bool mbOLEPrtNotifyPending   : 1;    /**< TRUE: Printer has changed. At creation of View
                                                notification of OLE-Objects PrtOLENotify() is required. */
    bool mbAllOLENotify          : 1;    //< True: Notification of all objects is required.
    bool mbInsOnlyTextGlssry     : 1;    //< True: insert 'only text' glossary into doc
    bool mbContains_MSVBasic     : 1;    //< True: MS-VBasic exist is in our storage
    bool mbClipBoard             : 1;    //< TRUE: this document represents the clipboard
    bool mbColumnSelection       : 1;    //< TRUE: this content has been created by a column selection (clipboard docs only)
    bool mbIsPrepareSelAll       : 1;
    bool mbDontCorrectBookmarks = false;

    enum MissingDictionary { False = -1, Undefined = 0, True = 1 };
    MissingDictionary meDictionaryMissing;

    // true: Document contains at least one anchored object, which is anchored AT_PAGE with a content position.
    //       Thus, certain adjustment needed during formatting for these kind of anchored objects.
    bool mbContainsAtPageObjWithContentAnchor : 1;

    static SwAutoCompleteWord *s_pAutoCompleteWords;  //< List of all words for AutoComplete
    /// The last, still alive SwDoc instance, for debugging.
    static SwDoc* s_pLast;

    // private methods
    SwFlyFrameFormat* MakeFlySection_( const SwPosition& rAnchPos,
                                const SwContentNode& rNode, RndStdIds eRequestId,
                                const SfxItemSet* pFlyAttrSet,
                                SwFrameFormat* );
    sal_Int8 SetFlyFrameAnchor( SwFrameFormat& rFlyFormat, SfxItemSet& rSet, bool bNewFrames );

    typedef SwFormat* (SwDoc::*FNCopyFormat)( const UIName&, SwFormat*, bool );
    SwFormat* CopyFormat( const SwFormat& rFormat, const SwFormatsBase& rFormatArr,
                        FNCopyFormat fnCopyFormat, const SwFormat& rDfltFormat );
    void CopyFormatArr( const SwFormatsBase& rSourceArr, SwFormatsBase const & rDestArr,
                        FNCopyFormat fnCopyFormat, SwFormat& rDfltFormat );
    SW_DLLPUBLIC void CopyPageDescHeaderFooterImpl( bool bCpyHeader,
                                const SwFrameFormat& rSrcFormat, SwFrameFormat& rDestFormat );

    SwDoc( const SwDoc &) = delete;

    // Database fields:
    void AddUsedDBToList( std::vector<OUString>& rDBNameList,
                          const std::vector<OUString>& rUsedDBNames );
    void AddUsedDBToList( std::vector<OUString>& rDBNameList, const OUString& rDBName );
    static bool IsNameInArray( const std::vector<OUString>& rOldNames, const OUString& rName );
    void GetAllDBNames( std::vector<OUString>& rAllDBNames );
    static OUString ReplaceUsedDBs( const std::vector<OUString>& rUsedDBNames,
                             const OUString& rNewName, const OUString& rFormula );
    static std::vector<OUString>& FindUsedDBs( const std::vector<OUString>& rAllDBNames,
                                const OUString& rFormula,
                                std::vector<OUString>& rUsedDBNames );

    SW_DLLPUBLIC void EnsureNumberFormatter(); // must be called with mNumberFormatterMutex locked

    bool UnProtectTableCells( SwTable& rTable );

    /** Create sub-documents according to the given collection.
     If no collection is given, take chapter style of the 1st level. */
    bool SplitDoc( sal_uInt16 eDocType, const OUString& rPath, bool bOutline,
                        const SwTextFormatColl* pSplitColl, int nOutlineLevel = 0 );

    // Update charts of given table.
    void UpdateCharts_( const SwTable& rTable, SwViewShell const & rVSh ) const;

    static bool SelectNextRubyChars( SwPaM& rPam, SwRubyListEntry& rRubyEntry );

    // CharTimer calls this method.
    void DoUpdateAllCharts();
    DECL_LINK( DoUpdateModifiedOLE, Timer *, void );

public:
    SW_DLLPUBLIC SwFormat *MakeCharFormat_(const UIName &, SwFormat *, bool );
    SwFormat *MakeFrameFormat_(const UIName &, SwFormat *, bool );

private:
    SwFormat *MakeTextFormatColl_(const UIName &, SwFormat *, bool );

private:
    OUString msDocAccTitle;

    void InitTOXTypes();

public:
    enum DocumentType {
        DOCTYPE_NATIVE,
        DOCTYPE_MSWORD              // This doc model comes from MS Word
        };
    DocumentType    meDocType;
    DocumentType    GetDocumentType() const { return meDocType; }
    void            SetDocumentType( DocumentType eDocType ) { meDocType = eDocType; }

    // Life cycle
    SW_DLLPUBLIC SwDoc();
    SW_DLLPUBLIC ~SwDoc();

    bool IsInDtor() const { return mbDtor; }

    /* @@@MAINTAINABILITY-HORROR@@@
       Implementation details made public.
    */
    SwNodes      & GetNodes()       { return *m_pNodes; }
    SwNodes const& GetNodes() const { return *m_pNodes; }

private:
    friend class ::rtl::Reference<SwDoc>;

    /** Acquire a reference to an instance. A caller shall release
        the instance by calling 'release' when it is no longer needed.
        'acquire' and 'release' calls need to be balanced.

        @returns
        the current reference count of the instance for debugging purposes.
    */
    SW_DLLPUBLIC sal_Int32 acquire();
    /** Releases a reference to an instance. A caller has to call
        'release' when a before acquired reference to an instance
        is no longer needed. 'acquire' and 'release' calls need to
        be balanced.

    @returns
        the current reference count of the instance for debugging purposes.
    */
    SW_DLLPUBLIC sal_Int32 release();
    /** Returns the current reference count. This method should be used for
        debugging purposes. Using it otherwise is a signal of a design flaw.
    */
public:
    sal_Int32 getReferenceCount() const;

    //MarkManager
    SW_DLLPUBLIC ::sw::mark::MarkManager& GetMarkManager();

    // IDocumentSettingAccess
    SW_DLLPUBLIC IDocumentSettingAccess const & getIDocumentSettingAccess() const; //The IDocumentSettingAccess interface
    SW_DLLPUBLIC IDocumentSettingAccess & getIDocumentSettingAccess();
    ::sw::DocumentSettingManager      & GetDocumentSettingManager(); //The implementation of the interface with some additional methods
    ::sw::DocumentSettingManager const& GetDocumentSettingManager() const;
    sal_uInt32 getRsid() const;
    void setRsid( sal_uInt32 nVal );
    sal_uInt32 getRsidRoot() const;
    void setRsidRoot( sal_uInt32 nVal );

    // IDocumentDeviceAccess
    IDocumentDeviceAccess const & getIDocumentDeviceAccess() const;
    SW_DLLPUBLIC IDocumentDeviceAccess & getIDocumentDeviceAccess();

    // IDocumentMarkAccess
    SW_DLLPUBLIC IDocumentMarkAccess* getIDocumentMarkAccess();
    SW_DLLPUBLIC const IDocumentMarkAccess* getIDocumentMarkAccess() const;

    // IDocumentRedlineAccess
    IDocumentRedlineAccess const& getIDocumentRedlineAccess() const;
    SW_DLLPUBLIC IDocumentRedlineAccess& getIDocumentRedlineAccess();

    ::sw::DocumentRedlineManager const& GetDocumentRedlineManager() const;
    SW_DLLPUBLIC ::sw::DocumentRedlineManager& GetDocumentRedlineManager();

    // IDocumentUndoRedo
    SW_DLLPUBLIC IDocumentUndoRedo      & GetIDocumentUndoRedo();
    IDocumentUndoRedo const& GetIDocumentUndoRedo() const;

    // IDocumentLinksAdministration
    IDocumentLinksAdministration const & getIDocumentLinksAdministration() const;
    SW_DLLPUBLIC IDocumentLinksAdministration & getIDocumentLinksAdministration();

    ::sw::DocumentLinksAdministrationManager const & GetDocumentLinksAdministrationManager() const;
    ::sw::DocumentLinksAdministrationManager & GetDocumentLinksAdministrationManager();

    // IDocumentFieldsAccess
    IDocumentFieldsAccess const & getIDocumentFieldsAccess() const;
    SW_DLLPUBLIC IDocumentFieldsAccess & getIDocumentFieldsAccess();

    ::sw::DocumentFieldsManager & GetDocumentFieldsManager();

    // Returns 0 if the field cannot hide para, or a positive integer indicating the field type
    // "weight" when several hiding fields' FieldHidesPara() give conflicting results
    int FieldCanHideParaWeight(SwFieldIds eFieldId) const;
    bool FieldHidesPara(const SwField& rField) const;

    // IDocumentContentOperations
    IDocumentContentOperations const & getIDocumentContentOperations() const;
    SW_DLLPUBLIC IDocumentContentOperations & getIDocumentContentOperations();
    ::sw::DocumentContentOperationsManager const & GetDocumentContentOperationsManager() const;
    ::sw::DocumentContentOperationsManager & GetDocumentContentOperationsManager();

    bool UpdateParRsid( SwTextNode *pTextNode, sal_uInt32 nVal = 0 );
    void UpdateRsid( const SwPaM &rRg, sal_Int32 nLen );

    // IDocumentStylePoolAccess
    IDocumentStylePoolAccess const & getIDocumentStylePoolAccess() const;
    SW_DLLPUBLIC IDocumentStylePoolAccess & getIDocumentStylePoolAccess();

    // SwLineNumberInfo
    SW_DLLPUBLIC const SwLineNumberInfo& GetLineNumberInfo() const;
    SW_DLLPUBLIC void SetLineNumberInfo(const SwLineNumberInfo& rInfo);

    // IDocumentStatistics
    IDocumentStatistics const & getIDocumentStatistics() const;
    SW_DLLPUBLIC IDocumentStatistics & getIDocumentStatistics();

    ::sw::DocumentStatisticsManager const & GetDocumentStatisticsManager() const;
    ::sw::DocumentStatisticsManager & GetDocumentStatisticsManager();

    // IDocumentState
    IDocumentState const & getIDocumentState() const;
    SW_DLLPUBLIC IDocumentState & getIDocumentState();

    // IDocumentDrawModelAccess
    void AddDrawUndo( std::unique_ptr<SdrUndoAction> );
    SW_DLLPUBLIC IDocumentDrawModelAccess const & getIDocumentDrawModelAccess() const;
    SW_DLLPUBLIC IDocumentDrawModelAccess & getIDocumentDrawModelAccess();

    ::sw::DocumentDrawModelManager const & GetDocumentDrawModelManager() const;
    ::sw::DocumentDrawModelManager & GetDocumentDrawModelManager();

    // IDocumentLayoutAccess
    SW_DLLPUBLIC IDocumentLayoutAccess const & getIDocumentLayoutAccess() const;
    SW_DLLPUBLIC IDocumentLayoutAccess & getIDocumentLayoutAccess();

    ::sw::DocumentLayoutManager const & GetDocumentLayoutManager() const;
    ::sw::DocumentLayoutManager & GetDocumentLayoutManager();

    // IDocumentTimerAccess
    // Our own 'IdleTimer' calls the following method
    IDocumentTimerAccess const & getIDocumentTimerAccess() const;
    IDocumentTimerAccess & getIDocumentTimerAccess();

    // IDocumentChartDataProviderAccess
    IDocumentChartDataProviderAccess const & getIDocumentChartDataProviderAccess() const;
    IDocumentChartDataProviderAccess & getIDocumentChartDataProviderAccess();

    // IDocumentListItems
    IDocumentListItems const & getIDocumentListItems() const;
    IDocumentListItems & getIDocumentListItems();

    // IDocumentOutlineNodes
    IDocumentOutlineNodes const & getIDocumentOutlineNodes() const;
    IDocumentOutlineNodes & getIDocumentOutlineNodes();

    // IDocumentListsAccess
    IDocumentListsAccess const & getIDocumentListsAccess() const;
    SW_DLLPUBLIC IDocumentListsAccess & getIDocumentListsAccess();

    //IDocumentExternalData
    IDocumentExternalData const & getIDocumentExternalData() const;
    SW_DLLPUBLIC IDocumentExternalData & getIDocumentExternalData();

    //End of Interfaces

    void setDocAccTitle( const OUString& rTitle ) { msDocAccTitle = rTitle; }
    const OUString& getDocAccTitle() const { return msDocAccTitle; }

    // INextInterface here
    DECL_LINK(CalcFieldValueHdl, EditFieldInfo*, void);

    // OLE ???
    bool IsOLEPrtNotifyPending() const  { return mbOLEPrtNotifyPending; }
    inline void SetOLEPrtNotifyPending( bool bSet = true );
    void PrtOLENotify( bool bAll ); // All or only marked

    bool IsPrepareSelAll() const { return mbIsPrepareSelAll; }
    void SetPrepareSelAll() { mbIsPrepareSelAll = true; }

    void SetContainsAtPageObjWithContentAnchor( const bool bFlag )
    {
        mbContainsAtPageObjWithContentAnchor = bFlag;
    }
    bool DoesContainAtPageObjWithContentAnchor()
    {
        return mbContainsAtPageObjWithContentAnchor;
    }

    /** Returns positions of all FlyFrames in the document.
     If a Pam-Pointer is passed the FlyFrames attached to paragraphs
     have to be surrounded completely by css::awt::Selection.
     ( Start < Pos < End ) !!!
     (Required for Writers.) */
    SW_DLLPUBLIC SwPosFlyFrames GetAllFlyFormats( const SwPaM*,
                        bool bDrawAlso,
                        bool bAsCharAlso = false ) const;

    SwFlyFrameFormat  *MakeFlyFrameFormat (const UIName &rFormatName, SwFrameFormat *pDerivedFrom);
    SwDrawFrameFormat *MakeDrawFrameFormat(const UIName &rFormatName, SwFrameFormat *pDerivedFrom);

    // From now on this interface has to be used for Flys.
    // pAnchorPos must be set, if they are not attached to pages AND
    // Anchor is not already set at valid ContentPos
    // in FlySet/FrameFormat.
    /* new parameter bCalledFromShell

       true: An existing adjust item at pAnchorPos is propagated to
       the content node of the new fly section. That propagation only
       takes place if there is no adjust item in the paragraph style
       for the new fly section.

       false: no propagation
    */
    SW_DLLPUBLIC SwFlyFrameFormat* MakeFlySection( RndStdIds eAnchorType,
                                 const SwPosition* pAnchorPos,
                                 const SfxItemSet* pSet = nullptr,
                                 SwFrameFormat *pParent = nullptr,
                                 bool bCalledFromShell = false );
    SwFlyFrameFormat* MakeFlyAndMove( const SwPaM& rPam, const SfxItemSet& rSet,
                                const SwSelBoxes* pSelBoxes,
                                SwFrameFormat *pParent );

    // Helper that checks for unique items for DrawingLayer items of type NameOrIndex
    // and evtl. corrects that items to ensure unique names for that type. This call may
    // modify/correct entries inside of the given SfxItemSet, and it will apply a name to
    // the items in question (what is essential to make the named slots associated with
    // these items work for the UNO API and thus e.g. for ODF import/export)
    void CheckForUniqueItemForLineFillNameOrIndex(SfxItemSet& rSet);

    SW_DLLPUBLIC bool SetFlyFrameAttr( SwFrameFormat& rFlyFormat, SfxItemSet& rSet );

    bool SetFrameFormatToFly( SwFrameFormat& rFlyFormat, SwFrameFormat& rNewFormat,
                        SfxItemSet* pSet = nullptr, bool bKeepOrient = false );
    void SetFlyFrameTitle( SwFlyFrameFormat& rFlyFrameFormat,
                         const OUString& sNewTitle );
    void SetFlyFrameDescription( SwFlyFrameFormat& rFlyFrameFormat,
                               const OUString& sNewDescription );
    void SetFlyFrameDecorative(SwFlyFrameFormat& rFlyFrameFormat,
                               bool isDecorative);

    // Footnotes
    // Footnote information
    const SwFootnoteInfo& GetFootnoteInfo() const         { return *mpFootnoteInfo; }
    SW_DLLPUBLIC void SetFootnoteInfo(const SwFootnoteInfo& rInfo);
    const SwEndNoteInfo& GetEndNoteInfo() const { return *mpEndNoteInfo; }
    SW_DLLPUBLIC void SetEndNoteInfo(const SwEndNoteInfo& rInfo);
          SwFootnoteIdxs& GetFootnoteIdxs()       { return *mpFootnoteIdxs; }
    const SwFootnoteIdxs& GetFootnoteIdxs() const { return *mpFootnoteIdxs; }
    /// change footnotes in range
    bool SetCurFootnote( const SwPaM& rPam, const OUString& rNumStr,
                    bool bIsEndNote );

    /** Operations on the content of the document e.g.
        spell-checking/hyphenating/word-counting
    */
    css::uno::Any
            Spell( SwPaM&, css::uno::Reference< css::linguistic2::XSpellChecker1 > const &,
                   sal_uInt16* pPageCnt, sal_uInt16* pPageSt, bool bGrammarCheck,
                   SwRootFrame const* pLayout, // for grammar-check
                   SwConversionArgs *pConvArgs = nullptr ) const;

    css::uno::Reference< css::linguistic2::XHyphenatedWord >
            Hyphenate( SwPaM *pPam, const Point &rCursorPos,
                         sal_uInt16* pPageCnt, sal_uInt16* pPageSt );

    // count words in pam
    static void CountWords( const SwPaM& rPaM, SwDocStat& rStat );

    // Glossary Document
    bool IsInsOnlyTextGlossary() const      { return mbInsOnlyTextGlssry; }

    void Summary(SwDoc& rExtDoc, sal_uInt8 nLevel, sal_uInt8 nPara, bool bImpress);

    void ChangeAuthorityData(const SwAuthEntry* pNewData);

    bool IsInHeaderFooter( const SwNode& ) const;
    SW_DLLPUBLIC SvxFrameDirection GetTextDirection( const SwPosition& rPos,
                            const Point* pPt = nullptr ) const;
    SW_DLLPUBLIC bool IsInVerticalText( const SwPosition& rPos ) const;

    // Database  and DB-Manager
    void SetDBManager( SwDBManager* pNewMgr )     { m_pDBManager = pNewMgr; }
    SwDBManager* GetDBManager() const             { return m_pDBManager; }
    void ChangeDBFields( const std::vector<OUString>& rOldNames,
                        const OUString& rNewName );
    SW_DLLPUBLIC void SetInitDBFields(bool b);

    // Find out which databases are used by fields.
    void GetAllUsedDB( std::vector<OUString>& rDBNameList,
                       const std::vector<OUString>* pAllDBNames = nullptr );

    void ChgDBData( const SwDBData& rNewData );
    SW_DLLPUBLIC SwDBData const & GetDBData();

    // Some helper functions
    UIName GetUniqueGrfName(UIName rPrefix = UIName()) const;
    UIName GetUniqueOLEName() const;
    SW_DLLPUBLIC UIName GetUniqueFrameName() const;
    UIName GetUniqueShapeName() const;
    SW_DLLPUBLIC UIName GetUniqueDrawObjectName() const;

    SW_DLLPUBLIC o3tl::sorted_vector<SwRootFrame*> GetAllLayouts();

    void SetFlyName( SwFlyFrameFormat& rFormat, const UIName& rName );
    SW_DLLPUBLIC const SwFlyFrameFormat* FindFlyByName( const UIName& rName, SwNodeType nNdTyp = SwNodeType::NONE ) const;

    static void GetGrfNms( const SwFlyFrameFormat& rFormat, OUString* pGrfName, OUString* pFltName );

    // Set a valid name for all Flys that have none (Called by Readers after reading).
    void SetAllUniqueFlyNames();

    /** Reset attributes. All TextHints and (if completely selected) all hard-
     formatted stuff (auto-formats) are removed.
     Introduce new optional parameter <bSendDataChangedEvents> in order to
     control, if the side effect "send data changed events" is triggered or not. */
    void ResetAttrs( const SwPaM &rRg,
                     bool bTextAttr = true,
                     const o3tl::sorted_vector<sal_uInt16> &rAttrs = o3tl::sorted_vector<sal_uInt16>(),
                     const bool bSendDataChangedEvents = true,
                     SwRootFrame const* pLayout = nullptr);
    void RstTextAttrs(const SwPaM &rRg, bool bInclRefToxMark = false,
            bool bExactRange = false, SwRootFrame const* pLayout = nullptr);

    /** Set attribute in given format.1y
     *  If Undo is enabled, the old values is added to the Undo history. */
    SW_DLLPUBLIC void SetAttr( const SfxPoolItem&, SwFormat& );
    /** Set attribute in given format.1y
     *  If Undo is enabled, the old values is added to the Undo history. */
    SW_DLLPUBLIC void SetAttr( const SfxItemSet&, SwFormat& );

    // method to reset a certain attribute at the given format
    void ResetAttrAtFormat( const std::vector<sal_uInt16>& rIds,
                            SwFormat& rChangedFormat );

    /** Set attribute as new default attribute in current document.
     If Undo is activated, the old one is listed in Undo-History. */
    void SetDefault( const SfxPoolItem& );
    void SetDefault( const SfxItemSet& );

    // Query default attribute in this document.
    SW_DLLPUBLIC const SfxPoolItem& GetDefault( sal_uInt16 nFormatHint ) const;
    template<class T> const T&  GetDefault( TypedWhichId<T> nWhich ) const
    {
        return static_cast<const T&>(GetDefault(sal_uInt16(nWhich)));
    }

    // Do not expand text attributes.
    bool DontExpandFormat( const SwPosition& rPos, bool bFlag = true );

    // Formats
    const sw::FrameFormats<SwFrameFormat*>* GetFrameFormats() const     { return mpFrameFormatTable.get(); }
          sw::FrameFormats<SwFrameFormat*>* GetFrameFormats()           { return mpFrameFormatTable.get(); }
    const SwCharFormats* GetCharFormats() const   { return mpCharFormatTable.get();}
          SwCharFormats* GetCharFormats()         { return mpCharFormatTable.get();}

    // LayoutFormats (frames, DrawObjects), sometimes const sometimes not
    const sw::FrameFormats<sw::SpzFrameFormat*>* GetSpzFrameFormats() const   { return mpSpzFrameFormatTable.get(); }
          sw::FrameFormats<sw::SpzFrameFormat*>* GetSpzFrameFormats()         { return mpSpzFrameFormatTable.get(); }

    const SwFrameFormat *GetDfltFrameFormat() const   { return mpDfltFrameFormat.get(); }
          SwFrameFormat *GetDfltFrameFormat()         { return mpDfltFrameFormat.get(); }
    const SwFrameFormat *GetEmptyPageFormat() const { return mpEmptyPageFormat.get(); }
          SwFrameFormat *GetEmptyPageFormat()       { return mpEmptyPageFormat.get(); }
    const SwFrameFormat *GetColumnContFormat() const{ return mpColumnContFormat.get(); }
          SwFrameFormat *GetColumnContFormat()      { return mpColumnContFormat.get(); }
    const SwCharFormat *GetDfltCharFormat() const { return mpDfltCharFormat.get();}
          SwCharFormat *GetDfltCharFormat()       { return mpDfltCharFormat.get();}

    // @return the interface of the management of (auto)styles
    IStyleAccess& GetIStyleAccess() { return *mpStyleAccess; }

    // Remove all language dependencies from all existing formats
    void RemoveAllFormatLanguageDependencies();

    SW_DLLPUBLIC SwFrameFormat* MakeFrameFormat(const UIName &rFormatName, SwFrameFormat *pDerivedFrom,
                          bool bAuto = true);
    SW_DLLPUBLIC void DelFrameFormat( SwFrameFormat *pFormat, bool bBroadcast = false );
    SwFrameFormat* FindFrameFormatByName( const UIName& rName ) const;

    SW_DLLPUBLIC SwCharFormat *MakeCharFormat(const UIName &rFormatName, SwCharFormat *pDerivedFrom);
    void       DelCharFormat(size_t nFormat, bool bBroadcast = false);
    void       DelCharFormat(SwCharFormat const * pFormat, bool bBroadcast = false);
    SwCharFormat* FindCharFormatByName( const UIName& rName ) const
        {   return mpCharFormatTable->FindFormatByName(rName); }

    // Formatcollections (styles)
    // TXT
    const SwTextFormatColl* GetDfltTextFormatColl() const { return mpDfltTextFormatColl.get(); }
    SwTextFormatColl* GetDfltTextFormatColl() { return mpDfltTextFormatColl.get(); }
    const SwTextFormatColls *GetTextFormatColls() const { return mpTextFormatCollTable.get(); }
    SwTextFormatColls *GetTextFormatColls() { return mpTextFormatCollTable.get(); }
    SW_DLLPUBLIC SwTextFormatColl *MakeTextFormatColl( const UIName &rFormatName,
                                  SwTextFormatColl *pDerivedFrom);
    SwConditionTextFormatColl* MakeCondTextFormatColl( const UIName &rFormatName,
                                               SwTextFormatColl *pDerivedFrom);
    void DelTextFormatColl(size_t nFormat, bool bBroadcast = false);
    void DelTextFormatColl( SwTextFormatColl const * pColl, bool bBroadcast = false );
    /** Add 4th optional parameter <bResetListAttrs>.
     'side effect' of <SetTextFormatColl> with <bReset = true> is that the hard
     attributes of the affected text nodes are cleared, except the break
     attribute, the page description attribute and the list style attribute.
     The new parameter <bResetListAttrs> indicates, if the list attributes
     (list style, restart at and restart with) are cleared as well in case
     that <bReset = true> and the paragraph style has a list style attribute set. */
    SW_DLLPUBLIC bool SetTextFormatColl(const SwPaM &rRg, SwTextFormatColl *pFormat,
                       const bool bReset = true,
                       const bool bResetListAttrs = false,
                       const bool bResetAllCharAttrs = false,
                       SwRootFrame const* pLayout = nullptr);
    SwTextFormatColl* FindTextFormatCollByName( const UIName& rName ) const
        {   return mpTextFormatCollTable->FindFormatByName(rName); }

    void ChkCondColls();

    const SwGrfFormatColl* GetDfltGrfFormatColl() const   { return mpDfltGrfFormatColl.get(); }
    SwGrfFormatColl* GetDfltGrfFormatColl()  { return mpDfltGrfFormatColl.get(); }
    const SwGrfFormatColls *GetGrfFormatColls() const     { return mpGrfFormatCollTable.get(); }
    SwGrfFormatColl *MakeGrfFormatColl(const UIName &rFormatName,
                                    SwGrfFormatColl *pDerivedFrom);

    // Table formatting
    const sw::TableFrameFormats* GetTableFrameFormats() const  { return mpTableFrameFormatTable.get(); }
          sw::TableFrameFormats* GetTableFrameFormats()        { return mpTableFrameFormatTable.get(); }
    SW_DLLPUBLIC size_t GetTableFrameFormatCount( bool bUsed ) const;
    SwTableFormat& GetTableFrameFormat(size_t nFormat, bool bUsed ) const;
    SwTableFormat* MakeTableFrameFormat(const UIName &rFormatName, SwFrameFormat *pDerivedFrom);
    void        DelTableFrameFormat( SwTableFormat* pFormat );
    SW_DLLPUBLIC SwTableFormat* FindTableFormatByName( const UIName& rName, bool bAll = false ) const;

    /** Access to frames.
    Iterate over Flys - for Basic-Collections. */
    SW_DLLPUBLIC size_t GetFlyCount( FlyCntType eType, bool bIgnoreTextBoxes = false ) const;
    SwFrameFormat* GetFlyNum(size_t nIdx, FlyCntType eType, bool bIgnoreTextBoxes = false );
    SW_DLLPUBLIC std::vector<SwFrameFormat const*> GetFlyFrameFormats(
            FlyCntType eType,
            bool bIgnoreTextBoxes);
    SwFrameFormat* GetFlyFrameFormatByName( const UIName& sFrameFormatName );

    // Copy formats in own arrays and return them.
    SwFrameFormat  *CopyFrameFormat ( const SwFrameFormat& );
    SwCharFormat *CopyCharFormat( const SwCharFormat& );
    SwTextFormatColl* CopyTextColl( const SwTextFormatColl& rColl );
    SwGrfFormatColl* CopyGrfColl( const SwGrfFormatColl& rColl );

    // Replace all styles with those from rSource.
    void ReplaceStyles( const SwDoc& rSource, bool bIncludePageStyles = true );

    // Replace all property defaults with those from rSource.
    SW_DLLPUBLIC void ReplaceDefaults( const SwDoc& rSource );

    // Replace all compatibility options with those from rSource.
    SW_DLLPUBLIC void ReplaceCompatibilityOptions( const SwDoc& rSource );

    /** Replace all user defined document properties with xSourceDocProps.
        Convenience function used by ReplaceDocumentProperties to skip some UNO calls.
     */
    void ReplaceUserDefinedDocumentProperties( const css::uno::Reference< css::document::XDocumentProperties >& xSourceDocProps );

    /** Replace document properties with those from rSource.

        This includes the user defined document properties!
     */
    SW_DLLPUBLIC void ReplaceDocumentProperties(const SwDoc& rSource, bool mailMerge = false);

    // Query if style (paragraph- / character- / frame- / page-) is used.
    bool IsUsed( const sw::BroadcastingModify& ) const;
    /// Query if table style is used.
    bool IsUsed( const SwTableAutoFormat& ) const;
    SW_DLLPUBLIC bool IsUsed( const SwNumRule& ) const;

    // Set name of newly loaded document template.
    size_t SetDocPattern(const OUString& rPatternName);

    // @return name of document template. Can be 0!
    const OUString* GetDocPattern(size_t nPos) const;

    // travel over PaM Ring
    bool InsertGlossary( SwTextBlocks& rBlock, const OUString& rEntry,
                        SwPaM& rPaM, SwCursorShell* pShell = nullptr);

    /** get the set of printable pages for the XRenderable API by
     evaluating the respective settings (see implementation) */
    static void CalculatePagesForPrinting( const SwRootFrame& rLayout, SwRenderData &rData, const SwPrintUIOptions &rOptions, bool bIsPDFExport,
            sal_Int32 nDocPageCount );
    static void UpdatePagesForPrintingWithPostItData( SwRenderData &rData, const SwPrintUIOptions &rOptions,
            sal_Int32 nDocPageCount );
    static void CalculatePagePairsForProspectPrinting( const SwRootFrame& rLayout, SwRenderData &rData, const SwPrintUIOptions &rOptions,
            sal_Int32 nDocPageCount );
    static void CalculateNonBlankPages( const SwRootFrame& rLayout, sal_uInt16& nDocPageCount, sal_uInt16& nActualPage );

    // PageDescriptor interface.
    size_t GetPageDescCnt() const { return m_PageDescs.size(); }
    const SwPageDesc& GetPageDesc(const size_t i) const { return *m_PageDescs[i]; }
    SwPageDesc& GetPageDesc(size_t const i) { return *m_PageDescs[i]; }
    SW_DLLPUBLIC SwPageDesc* FindPageDesc(const UIName& rName, size_t* pPos = nullptr) const;
    // Just searches the pointer in the m_PageDescs vector!
    bool        ContainsPageDesc(const SwPageDesc *pDesc, size_t* pPos) const;

    /** Copy the complete PageDesc - beyond document and "deep"!
     Optionally copying of PoolFormatId, -HlpId can be prevented. */
    SW_DLLPUBLIC void CopyPageDesc( const SwPageDesc& rSrcDesc, SwPageDesc& rDstDesc,
                        bool bCopyPoolIds = true );

    /** Copy header (with contents) from SrcFormat to DestFormat
     (can also be copied into other document). */
    void CopyHeader( const SwFrameFormat& rSrcFormat, SwFrameFormat& rDestFormat )
        { CopyPageDescHeaderFooterImpl( true, rSrcFormat, rDestFormat ); }

    /** Copy footer (with contents) from SrcFormat to DestFormat.
     (can also be copied into other document). */
    void CopyFooter( const SwFrameFormat& rSrcFormat, SwFrameFormat& rDestFormat )
        { CopyPageDescHeaderFooterImpl( false, rSrcFormat, rDestFormat ); }

    // For Reader
    SW_DLLPUBLIC void ChgPageDesc( const UIName & rName, const SwPageDesc& );
    SW_DLLPUBLIC void ChgPageDesc( size_t i, const SwPageDesc& );
    void DelPageDesc( const UIName & rName, bool bBroadcast = false);
    void DelPageDesc( size_t i, bool bBroadcast = false );
    void PreDelPageDesc(SwPageDesc const * pDel);
    SW_DLLPUBLIC SwPageDesc* MakePageDesc(const UIName &rName, const SwPageDesc* pCpy = nullptr,
                             bool bRegardLanguage = true);
    void BroadcastStyleOperation(const UIName& rName, SfxStyleFamily eFamily,
                                 SfxHintId nOp);

    /** The html import sometimes overwrites the page sizes set in
     the page descriptions. This function is used to correct this. */
    void CheckDefaultPageFormat();

    // Methods for tables/indices
    static sal_uInt16 GetCurTOXMark( const SwPosition& rPos, SwTOXMarks& );
    void DeleteTOXMark( const SwTOXMark* pTOXMark );
    SW_DLLPUBLIC const SwTOXMark& GotoTOXMark( const SwTOXMark& rCurTOXMark,
                                SwTOXSearch eDir, bool bInReadOnly );
    /// Iterate over all SwTOXMark, if the function returns false, iteration is stopped
    SW_DLLPUBLIC void ForEachTOXMark( const std::function<bool(const SwTOXMark&)>&  ) const;

    // Insert/Renew table/index
    SW_DLLPUBLIC SwTOXBaseSection* InsertTableOf( const SwPosition& rPos,
                                            const SwTOXBase& rTOX,
                                            const SfxItemSet* pSet = nullptr,
                                            bool bExpand = false,
                                    SwRootFrame const* pLayout = nullptr);
    SwTOXBaseSection* InsertTableOf( const SwPaM& aPam,
                                            const SwTOXBase& rTOX,
                                            const SfxItemSet* pSet = nullptr,
                                            bool bExpand = false,
                                    SwRootFrame const* pLayout = nullptr );
    void              InsertTableOf( SwNodeOffset nSttNd, SwNodeOffset nEndNd,
                                            const SwTOXBase& rTOX,
                                            const SfxItemSet* pSet );
    SW_DLLPUBLIC static SwTOXBase* GetCurTOX( const SwPosition& rPos );
    static const SwAttrSet& GetTOXBaseAttrSet(const SwTOXBase& rTOX);

    bool DeleteTOX( const SwTOXBase& rTOXBase, bool bDelNodes );
    OUString GetUniqueTOXBaseName( const SwTOXType& rType,
                                   const OUString& sChkStr ) const;

    bool SetTOXBaseName(const SwTOXBase& rTOXBase, const UIName& rName);

    // After reading file update all tables/indices
    void SetUpdateTOX( bool bFlag )            { mbUpdateTOX = bFlag; }
    bool IsUpdateTOX() const                   { return mbUpdateTOX; }

    const OUString& GetTOIAutoMarkURL() const {return msTOIAutoMarkURL;}
    void            SetTOIAutoMarkURL(const OUString& rSet) {msTOIAutoMarkURL = rSet;}

    bool IsInReading() const                    { return mbInReading; }
    void SetInReading( bool bNew )              { mbInReading = bNew; }

    bool IsInWriting() const { return mbInWriting; }
    void SetInWriting(bool bNew) { mbInWriting = bNew; }

    bool IsInMailMerge() const                  { return mbInMailMerge; }
    void SetInMailMerge( bool bNew )            { mbInMailMerge = bNew; }

    bool IsClipBoard() const                    { return mbClipBoard; }
    // N.B.: must be called right after constructor! (@see GetXmlIdRegistry)
    void SetClipBoard( bool bNew )              { mbClipBoard = bNew; }

    bool IsColumnSelection() const              { return mbColumnSelection; }
    void SetColumnSelection( bool bNew )        { mbColumnSelection = bNew; }

    bool IsInXMLImport() const { return mbInXMLImport; }
    void SetInXMLImport( bool bNew ) { mbInXMLImport = bNew; }
    bool IsInXMLImport242() const { return mbInXMLImport242; }
    void SetInXMLImport242(bool const bNew) { mbInXMLImport242 = bNew; }
    bool IsInWriterfilterImport() const { return mbInWriterfilterImport; }
    void SetInWriterfilterImport(bool const b) { mbInWriterfilterImport = b; }

    // Manage types of tables/indices
    SW_DLLPUBLIC sal_uInt16 GetTOXTypeCount( TOXTypes eTyp ) const;
    SW_DLLPUBLIC const SwTOXType* GetTOXType( TOXTypes eTyp, sal_uInt16 nId ) const;
    const SwTOXType* InsertTOXType( const SwTOXType& rTyp );
    const SwTOXTypes& GetTOXTypes() const { return *mpTOXTypes; }

    const SwTOXBase*    GetDefaultTOXBase( TOXTypes eTyp, bool bCreate );
    void                SetDefaultTOXBase(const SwTOXBase& rBase);

    // Key for management of index.
    void GetTOIKeys(SwTOIKeyType eTyp, std::vector<OUString>& rArr,
            SwRootFrame const& rLayout) const;

    // Sort table text.
    bool SortTable(const SwSelBoxes& rBoxes, const SwSortOptions&);
    bool SortText(const SwPaM&, const SwSortOptions&);

    // Correct the SwPosition-Objects that are registered with the document
    // e. g. Bookmarks or tables/indices.
    // If bMoveCursor is set move Cursor too.

    // Set everything in rOldNode on rNewPos + Offset.
    void CorrAbs(
        const SwNode& rOldNode,
        const SwPosition& rNewPos,
        const sal_Int32 nOffset = 0,
        bool bMoveCursor = false );

    // Set everything in the range of [rStartNode, rEndNode] to rNewPos.
    static void CorrAbs(
        const SwNodeIndex& rStartNode,
        const SwNodeIndex& rEndNode,
        const SwPosition& rNewPos,
        bool bMoveCursor = false );

    // Set everything in this range from rRange to rNewPos.
    static void CorrAbs(
        const SwPaM& rRange,
        const SwPosition& rNewPos,
        bool bMoveCursor = false );

    // Set everything in rOldNode to relative Pos.
    void CorrRel(
        const SwNode& rOldNode,
        const SwPosition& rNewPos,
        const sal_Int32 nOffset = 0,
        bool bMoveCursor = false );

    // Query / set rules for Outline.
    SwNumRule* GetOutlineNumRule() const
    {
        return mpOutlineRule;
    }
    SW_DLLPUBLIC void SetOutlineNumRule( const SwNumRule& rRule );
    SW_DLLPUBLIC void PropagateOutlineRule();

    // Outline - promote / demote.
    bool OutlineUpDown(const SwPaM& rPam, short nOffset, SwRootFrame const* pLayout = nullptr);

    /// Outline - move up / move down.
    bool MoveOutlinePara( const SwPaM& rPam,
                    SwOutlineNodes::difference_type nOffset,
                    const SwOutlineNodesInline* pOutlNdsInline = nullptr);

    SW_DLLPUBLIC bool GotoOutline(SwPosition& rPos, const OUString& rName, SwRootFrame const* = nullptr) const;

    enum class SetNumRuleMode {
        Default = 0,
        /// indicates if a new list is created by applying the given list style.
        CreateNewList = 1,
        DontSetItem = 2,
        /** If enabled, the indent attributes "before text" and
          "first line indent" are additionally reset at the provided PaM,
          if the list style makes use of the new list level attributes. */
        ResetIndentAttrs = 4,
        DontSetIfAlreadyApplied = 8
    };

    /** Set or change numbering rule on text nodes, as direct formatting.
     @param sContinuedListId If bCreateNewList is false, may contain the
      list Id of a list which has to be continued by applying the given list style

     @return the set ListId if bSetItem is true */
    OUString SetNumRule( const SwPaM&,
                     const SwNumRule&,
                     SetNumRuleMode mode,
                     SwRootFrame const* pLayout = nullptr,
                     const OUString& sContinuedListId = OUString(),
                     SvxTextLeftMarginItem const* pTextLeftMarginToPropagate = nullptr,
                     SvxFirstLineIndentItem const* pFirstLineIndentToPropagate = nullptr);
    void SetCounted(const SwPaM&, bool bCounted, SwRootFrame const* pLayout);

    void MakeUniqueNumRules(const SwPaM & rPaM);

    void SetNumRuleStart( const SwPosition& rPos, bool bFlag = true );
    void SetNodeNumStart( const SwPosition& rPos, sal_uInt16 nStt );

    // sw_redlinehide: may set rPos to different node (the one with the NumRule)
    static SwNumRule* GetNumRuleAtPos(SwPosition& rPos, SwRootFrame const* pLayout = nullptr);

    const SwNumRuleTable& GetNumRuleTable() const { return *mpNumRuleTable; }

    /**
       Add numbering rule to document.

       @param pRule    rule to add
    */
    void AddNumRule(SwNumRule * pRule);

    // add optional parameter <eDefaultNumberFormatPositionAndSpaceMode>
    SW_DLLPUBLIC sal_uInt16 MakeNumRule( const UIName &rName,
        const SwNumRule* pCpy = nullptr,
        const SvxNumberFormat::SvxNumPositionAndSpaceMode eDefaultNumberFormatPositionAndSpaceMode =
            SvxNumberFormat::LABEL_WIDTH_AND_POSITION );
    sal_uInt16 FindNumRule( const UIName& rName ) const;
    std::set<OUString> GetUsedBullets();
    SW_DLLPUBLIC SwNumRule* FindNumRulePtr( const UIName& rName ) const;

    // Deletion only possible if Rule is not used!
    bool RenameNumRule(const UIName & aOldName, const UIName & aNewName,
                           bool bBroadcast = false);
    SW_DLLPUBLIC bool DelNumRule( const UIName& rName, bool bBroadCast = false );
    SW_DLLPUBLIC UIName GetUniqueNumRuleName( const UIName* pChkStr = nullptr, bool bAutoNum = true ) const;

    void UpdateNumRule();   // Update all invalids.
    void ChgNumRuleFormats( const SwNumRule& rRule );
    void ReplaceNumRule( const SwPosition& rPos, const UIName& rOldRule,
                        const UIName& rNewRule );

    // Goto next/previous on same level.
    static bool GotoNextNum( SwPosition&, SwRootFrame const* pLayout,
                        bool bOverUpper = true,
                        sal_uInt8* pUpper = nullptr, sal_uInt8* pLower = nullptr );
    static bool GotoPrevNum( SwPosition&, SwRootFrame const* pLayout,
                        bool bOverUpper = true );

    /** Searches for a text node with a numbering rule.

       add optional parameter <bInvestigateStartNode>
       add output parameter <sListId>

       \param rPos         position to start search
       \param bForward     - true:  search forward
                           - false: search backward
       \param bNum         - true:  search for enumeration
                           - false: search for itemize
       \param bOutline     - true:  search for outline numbering rule
                           - false: search for non-outline numbering rule
       \param nNonEmptyAllowed   number of non-empty paragraphs allowed between
                                 rPos and found paragraph

        @param sListId
        output parameter - in case a list style is found, <sListId> holds the
        list id, to which the text node belongs, which applies the found list style.

        @param bInvestigateStartNode
        input parameter - boolean, indicating, if start node, determined by given
        start position has to be investigated or not.
     */
    const SwNumRule * SearchNumRule(const SwPosition & rPos,
                                    const bool bForward,
                                    const bool bNum,
                                    const bool bOutline,
                                    int nNonEmptyAllowed,
                                    OUString& sListId,
                                    SwRootFrame const* pLayout,
                                    const bool bInvestigateStartNode = false,
                                    SvxTextLeftMarginItem const** o_ppTextLeftMargin = nullptr,
                                    SvxFirstLineIndentItem const** o_ppFirstLineIndent = nullptr);

    // Paragraphs without numbering but with indents.
    bool NoNum( const SwPaM& );

    // Delete, splitting of numbering list.
    void DelNumRules(const SwPaM&, SwRootFrame const* pLayout = nullptr);

    // Invalidates all numrules
    void InvalidateNumRules();

    bool NumUpDown(const SwPaM&, bool bDown, SwRootFrame const* pLayout = nullptr);

    /** Move selected paragraphs (not only numberings)
     according to offsets. (if negative: go to doc start). */
    bool MoveParagraph(SwPaM&, SwNodeOffset nOffset, bool bIsOutlMv = false);
    bool MoveParagraphImpl(SwPaM&, SwNodeOffset nOffset, bool bIsOutlMv, SwRootFrame const*);

    bool NumOrNoNum( SwNode& rIdx, bool bDel = false);

    void StopNumRuleAnimations( const OutputDevice* );

    /** Insert new table at position @param rPos (will be inserted before Node!).
     For AutoFormat at input: columns have to be set at predefined width.
     The array holds the positions of the columns (not their widths).
     new @param bCalledFromShell:
       true: called from shell -> propagate existing adjust item at
       rPos to every new cell. A existing adjust item in the table
       heading or table contents paragraph style prevent that
       propagation.
       false: do not propagate
    */
    SW_DLLPUBLIC const SwTable* InsertTable( const SwInsertTableOptions& rInsTableOpts,  // HeadlineNoBorder
                                const SwPosition& rPos, sal_uInt16 nRows,
                                sal_uInt16 nCols, sal_Int16 eAdjust,
                                const SwTableAutoFormat* pTAFormat = nullptr,
                                const std::vector<sal_uInt16> *pColArr = nullptr,
                                bool bCalledFromShell = false,
                                bool bNewModel = true,
                                const OUString& rTableName = {} );

    // If index is in a table, return TableNode, else 0.
    static SwTableNode* IsIdxInTable( const SwNodeIndex& rIdx );
    static SwTableNode* IsInTable( const SwNode& );

    // Create a balanced table out of the selected range.
    const SwTable* TextToTable( const SwInsertTableOptions& rInsTableOpts, // HeadlineNoBorder,
                                const SwPaM& rRange, sal_Unicode cCh,
                                sal_Int16 eAdjust,
                                const SwTableAutoFormat* );

    // text to table conversion - API support
    const SwTable* TextToTable( const std::vector< std::vector<SwNodeRange> >& rTableNodes );

    bool TableToText( const SwTableNode* pTableNd, sal_Unicode cCh );

    // Create columns / rows in table.
    void InsertCol( const SwCursor& rCursor,
                    sal_uInt16 nCnt = 1, bool bBehind = true );
    bool InsertCol( const SwSelBoxes& rBoxes,
                    sal_uInt16 nCnt = 1, bool bBehind = true, bool bInsertDummy = true );
    void InsertRow( const SwCursor& rCursor,
                    sal_uInt16 nCnt = 1, bool bBehind = true );
    SW_DLLPUBLIC bool InsertRow( const SwSelBoxes& rBoxes,
                    sal_uInt16 nCnt = 1, bool bBehind = true, bool bInsertDummy = true );

    // Delete Columns/Rows in table.
    enum class RowColMode { DeleteRow = 0, DeleteColumn = 1, DeleteProtected = 2 };
    void DelTable(SwTableNode * pTable);
    bool DeleteRowCol(const SwSelBoxes& rBoxes, RowColMode eMode = RowColMode::DeleteRow);
    void DeleteRow( const SwCursor& rCursor );
    void DeleteCol( const SwCursor& rCursor );

    // Split / concatenate boxes in table.
    bool SplitTable( const SwSelBoxes& rBoxes, bool bVert,
                       sal_uInt16 nCnt, bool bSameHeight = false );

    TableMergeErr MergeTable( SwPaM& rPam );
    UIName GetUniqueTableName() const;
    bool IsInsTableFormatNum() const;
    bool IsInsTableChangeNumFormat() const;
    bool IsInsTableAlignNum() const;
    bool IsSplitVerticalByDefault() const;
    void SetSplitVerticalByDefault(bool value);

    // From FEShell (for Undo and BModified).
    static void GetTabCols( SwTabCols &rFill, const SwCellFrame* pBoxFrame );
    void SetTabCols( const SwTabCols &rNew, bool bCurRowOnly,
                    const SwCellFrame* pBoxFrame );
    static void GetTabRows( SwTabCols &rFill, const SwCellFrame* pBoxFrame );
    void SetTabRows( const SwTabCols &rNew, bool bCurColOnly,
                    const SwCellFrame* pBoxFrame );

    // Direct access for UNO.
    void SetTabCols(SwTable& rTab, const SwTabCols &rNew, const SwTabCols &rOld,
                                    const SwTableBox *pStart, bool bCurRowOnly);

    void SetRowsToRepeat( SwTable &rTable, sal_uInt16 nSet );

    /// AutoFormat for table/table selection.
    /// @param bResetDirect Reset direct formatting that might be applied to the cells.
    bool SetTableAutoFormat(const SwSelBoxes& rBoxes, const SwTableAutoFormat& rNew, bool bResetDirect = false, TableStyleName const* pStyleNameToSet = nullptr);

    // Query attributes.
    bool GetTableAutoFormat( const SwSelBoxes& rBoxes, SwTableAutoFormat& rGet );

    /// Return the available table styles.
    SW_DLLPUBLIC SwTableAutoFormatTable& GetTableStyles();
    const SwTableAutoFormatTable& GetTableStyles() const
    {
        return const_cast<SwDoc*>(this)->GetTableStyles();
    }
    /// Counts table styles without triggering lazy-load of them.
    bool HasTableStyles() const { return m_pTableStyles != nullptr; }
    // Create a new table style. Tracked by Undo.
    SW_DLLPUBLIC SwTableAutoFormat* MakeTableStyle(const TableStyleName& rName);
    // Delete table style named rName. Tracked by undo.
    SW_DLLPUBLIC std::unique_ptr<SwTableAutoFormat> DelTableStyle(const TableStyleName& rName, bool bBroadcast = false);
    // Change (replace) a table style named rName. Tracked by undo.
    SW_DLLPUBLIC void ChgTableStyle(const TableStyleName& rName, const SwTableAutoFormat& rNewFormat);

    const SwCellStyleTable& GetCellStyles() const  { return *mpCellStyles; }
          SwCellStyleTable& GetCellStyles()        { return *mpCellStyles; }

    void AppendUndoForInsertFromDB( const SwPaM& rPam, bool bIsTable );

    void SetColRowWidthHeight( SwTableBox& rCurrentBox, TableChgWidthHeightType eType,
                                SwTwips nAbsDiff, SwTwips nRelDiff );
    SwTableBoxFormat* MakeTableBoxFormat();
    SwTableLineFormat* MakeTableLineFormat();

    // helper function: cleanup before checking number value
    bool IsNumberFormat( const OUString& aString, sal_uInt32& F_Index, double& fOutNumber);
    // Check if box has numerical value. Change format of box if required.
    void ChkBoxNumFormat( SwTableBox& rCurrentBox, bool bCallUpdate );
    void SetTableBoxFormulaAttrs( SwTableBox& rBox, const SfxItemSet& rSet );
    void ClearBoxNumAttrs( SwNode& rNode );
    void ClearLineNumAttrs( SwPosition const & rPos );

    bool InsCopyOfTable( SwPosition& rInsPos, const SwSelBoxes& rBoxes,
                        const SwTable* pCpyTable, bool bCpyName = false,
                        bool bCorrPos = false, const TableStyleName& rStyleName = TableStyleName() );

    void UnProtectCells( const UIName& rTableName );
    bool UnProtectCells( const SwSelBoxes& rBoxes );
    void UnProtectTables( const SwPaM& rPam );
    bool HasTableAnyProtection( const SwPosition* pPos,
                              const UIName* pTableName,
                              bool* pFullTableProtection );

    // Split table at baseline position, i.e. create a new table.
    void SplitTable( const SwPosition& rPos, SplitTable_HeadlineOption eMode,
                        bool bCalcNewSize );

    /** And vice versa: rPos must be in the table that remains. The flag indicates
     whether the current table is merged with the one before or behind it. */
    bool MergeTable( const SwPosition& rPos, bool bWithPrev );

    // Make charts of given table update.
    void UpdateCharts( const UIName& rName ) const;

    // Update all charts, for that exists any table.
    void UpdateAllCharts()          { DoUpdateAllCharts(); }

    // Table is renamed and refreshes charts.
    void SetTableName( SwFrameFormat& rTableFormat, const UIName &rNewName );

    // @return the reference in document that is set for name.
    const SwFormatRefMark* GetRefMark( const SwMarkName& rName ) const;

    // @return RefMark via index - for UNO.
    const SwFormatRefMark* GetRefMark( sal_uInt16 nIndex ) const;

    /** @return names of all references that are set in document.
     If array pointer is 0 return only whether a RefMark is set in document. */
    SW_DLLPUBLIC sal_uInt16 GetRefMarks( std::vector<OUString>* = nullptr ) const;
    SW_DLLPUBLIC void GetRefMarks( std::vector<const SwFormatRefMark*>& ) const;
    /// Iterate over all SwFormatRefMark, if the function returns false, iteration is stopped
    SW_DLLPUBLIC void ForEachRefMark( const std::function<bool(const SwFormatRefMark&)>&  ) const;

    void DeleteFormatRefMark(const SwFormatRefMark* pFormatRefMark);

    // Insert label. If a FlyFormat is created, return it.
    SwFlyFrameFormat* InsertLabel( const SwLabelType eType, const OUString &rText, const OUString& rSeparator,
                    const OUString& rNumberingSeparator,
                    const bool bBefore, const sal_uInt16 nId, const SwNodeOffset nIdx,
                    const UIName& rCharacterStyle,
                    const bool bCpyBrd );
    SwFlyFrameFormat* InsertDrawLabel(
        const OUString &rText, const OUString& rSeparator, const OUString& rNumberSeparator,
        const sal_uInt16 nId, const UIName& rCharacterStyle, SdrObject& rObj );

    // Query attribute pool.
    const SwAttrPool& GetAttrPool() const   { return *mpAttrPool; }
          SwAttrPool& GetAttrPool()         { return *mpAttrPool; }

    // Search for an EditShell.
    SAL_RET_MAYBENULL SwEditShell const * GetEditShell() const;
    SAL_RET_MAYBENULL SW_DLLPUBLIC SwEditShell* GetEditShell();
    SAL_RET_MAYBENULL ::sw::IShellCursorSupplier * GetIShellCursorSupplier();

    // OLE 2.0-notification.
    void  SetOle2Link(const Link<bool,void>& rLink) {maOle2Link = rLink;}
    const Link<bool,void>& GetOle2Link() const {return maOle2Link;}

    // insert section (the ODF kind of section, not the nodesarray kind)
    SW_DLLPUBLIC SwSection * InsertSwSection(SwPaM const& rRange, SwSectionData &,
            std::tuple<SwTOXBase const*, sw::RedlineMode, sw::FieldmarkMode, sw::ParagraphBreakMode> const* pTOXBase,
            SfxItemSet const*const pAttr, bool const bUpdate = true);
    static sal_uInt16 IsInsRegionAvailable( const SwPaM& rRange,
                                const SwNode** ppSttNd = nullptr );
    SW_DLLPUBLIC static SwSection* GetCurrSection( const SwPosition& rPos );
    SwSectionFormats& GetSections() { return *mpSectionFormatTable; }
    const SwSectionFormats& GetSections() const { return *mpSectionFormatTable; }
    SwSectionFormat *MakeSectionFormat();
    void DelSectionFormat( SwSectionFormat *pFormat, bool bDelNodes = false );
    void UpdateSection(size_t const nSect, SwSectionData &,
            SfxItemSet const*const = nullptr, bool const bPreventLinkUpdate = false);
    SW_DLLPUBLIC OUString GetUniqueSectionName( const OUString* pChkStr = nullptr ) const;

    /* @@@MAINTAINABILITY-HORROR@@@
       The model should not have anything to do with a shell.
       Unnecessary compile/link time dependency.
    */

    // Pointer to SfxDocShell from Doc. Can be 0!!
    SAL_RET_MAYBENULL SwDocShell* GetDocShell()         { return mpDocShell; }
    SAL_RET_MAYBENULL const SwDocShell* GetDocShell() const   { return mpDocShell; }
    void SetDocShell( SwDocShell* pDSh );

    /** in case during copying of embedded object a new shell is created,
     it should be set here and cleaned later */
    void SetTmpDocShell(const SfxObjectShellLock& rLock) { mxTmpDocShell = rLock; }
    const SfxObjectShellLock& GetTmpDocShell() const   { return mxTmpDocShell; }

    // For Autotexts? (text modules) They have only one SVPersist at their disposal.
    SW_DLLPUBLIC SfxObjectShell* GetPersist() const;

    // Pointer to storage of SfxDocShells. Can be 0!!!
    SW_DLLPUBLIC css::uno::Reference< css::embed::XStorage > GetDocStorage();

    // Query / set flag indicating if document is loaded asynchronously at this moment.
    bool IsInLoadAsynchron() const             { return mbInLoadAsynchron; }
    void SetInLoadAsynchron( bool bFlag )       { mbInLoadAsynchron = bFlag; }

    // For Drag&Move: (e.g. allow "moving" of RefMarks)
    bool IsCopyIsMove() const              { return mbCopyIsMove; }
    void SetCopyIsMove( bool bFlag )        { mbCopyIsMove = bFlag; }

    SwDrawContact* GroupSelection( SdrView& );
    void UnGroupSelection( SdrView& );
    bool DeleteSelection( SwDrawView& );

    // Invalidates OnlineSpell-WrongLists.
    void SpellItAgainSam( bool bInvalid, bool bOnlyWrong, bool bSmartTags );
    void InvalidateAutoCompleteFlag();

    void SetCalcFieldValueHdl(Outliner* pOutliner);

    // Query if URL was visited.
    // Query via Doc, if only a Bookmark has been given.
    // In this case the document name has to be set in front.
    bool IsVisitedURL( std::u16string_view rURL );

    // Save current values for automatic registration of exceptions in Autocorrection.
    void SetAutoCorrExceptWord( std::unique_ptr<SwAutoCorrExceptWord> pNew );
    SwAutoCorrExceptWord* GetAutoCorrExceptWord()       { return mpACEWord.get(); }
    void DeleteAutoCorrExceptWord();

    const SwFormatINetFormat* FindINetAttr( std::u16string_view rName ) const;
    /// Iterate over all SwFormatINetFormat, if the function returns false, iteration is stopped
    SW_DLLPUBLIC void ForEachINetFormat( const std::function<bool(const SwFormatINetFormat&)>&  ) const;

    /// Iterate over all SwFormatURL, if the function returns false, iteration is stopped
    SW_DLLPUBLIC void ForEachFormatURL( const std::function<bool(const SwFormatURL&)>&  ) const;

    /// Iterate over all SvxOverlineItem, if the function returns false, iteration is stopped
    SW_DLLPUBLIC void ForEachOverlineItem( const std::function<bool(const SvxOverlineItem&)>&  ) const;

    /// Iterate over all SwFormatField, if the function returns false, iteration is stopped
    void ForEachFormatField( TypedWhichId<SwFormatField> nWhich, const std::function<bool(const SwFormatField&)>&  ) const;

    /// Iterate over all RES_CHRATR_BOX SvxBoxItem, if the function returns false, iteration is stopped
    SW_DLLPUBLIC void ForEachCharacterBoxItem(const std::function<bool(const SvxBoxItem&)>&  ) const;

    /// Iterate over all RES_CHRATR_COLOR SvxColorItem, if the function returns false, iteration is stopped
    SW_DLLPUBLIC void ForEachCharacterColorItem(const std::function<bool(const SvxColorItem&)>&  ) const;

    /// Iterate over all RES_CHRATR_UNDERLINE SvxUnderlineItem, if the function returns false, iteration is stopped
    SW_DLLPUBLIC void ForEachCharacterUnderlineItem(const std::function<bool(const SvxUnderlineItem&)>&  ) const;

    /// Iterate over all RES_CHRATR_BACKGROUND SvxBrushItem, if the function returns false, iteration is stopped
    SW_DLLPUBLIC void ForEachCharacterBrushItem(const std::function<bool(const SvxBrushItem&)>&  ) const;

    /// Iterate over all RES_CHRATR_FONT/RES_CHRATR_CJK_FONT/RES_CHRATR_CTL_FONT SvxFontItem, if the function returns false, iteration is stopped
    SW_DLLPUBLIC void ForEachCharacterFontItem(TypedWhichId<SvxFontItem> nWhich, bool bIgnoreAutoStyles, const std::function<bool(const SvxFontItem&)>&  );

    /// Iterate over all RES_TXTATR_UNKNOWN_CONTAINER SvXMLAttrContainerItem, if the function returns false, iteration is stopped
    SW_DLLPUBLIC void ForEachTxtAtrContainerItem(const std::function<bool(const SvXMLAttrContainerItem&)>&  ) const;

    /// Iterate over all RES_PARATR_TABSTOP SvxTabStopItem, if the function returns false, iteration is stopped
    SW_DLLPUBLIC void ForEachParaAtrTabStopItem(const std::function<bool(const SvxTabStopItem&)>&  );

    /// Iterate over all RES_UNKNOWNATR_CONTAINER SvXMLAttrContainerItem, if the function returns false, iteration is stopped
    SW_DLLPUBLIC void ForEachUnknownAtrContainerItem(const std::function<bool(const SvXMLAttrContainerItem&)>&  ) const;

    /// Iterate over all RES_BOX SvxBoxItem, if the function returns false, iteration is stopped
    SW_DLLPUBLIC void ForEachBoxItem(const std::function<bool(const SvxBoxItem&)>&  ) const;

    /// Iterate over all RES_SHADOW SvxShadowItem, if the function returns false, iteration is stopped
    SW_DLLPUBLIC void ForEachShadowItem(const std::function<bool(const SvxShadowItem&)>&  ) const;

    /// Iterate over all RES_BACKGROUND SvxBrushItem, if the function returns false, iteration is stopped
    SW_DLLPUBLIC void ForEachBackgroundBrushItem(const std::function<bool(const SvxBrushItem&)>&  ) const;

    // Call into intransparent Basic; expect possible Return String.
    void ExecMacro( const SvxMacro& rMacro, OUString* pRet, SbxArray* pArgs );

    // Call into intransparent Basic / JavaScript.
    sal_uInt16 CallEvent( SvMacroItemId nEvent, const SwCallMouseEvent& rCallEvent,
                        bool bChkPtr = false );

    /** Adjust left margin via object bar (similar to adjustment of numerations).
     One can either change the margin "by" adding or subtracting a given
     offset or set it "to" this position (bModulus = true). */
    void MoveLeftMargin(const SwPaM& rPam, bool bRight, bool bModulus,
            SwRootFrame const* pLayout = nullptr);

    // Query NumberFormatter.
    SvNumberFormatter* GetNumberFormatter(bool bCreate = true)
    {
        std::scoped_lock lock(mNumberFormatterMutex);
        if (bCreate)
            EnsureNumberFormatter();
        return mpNumberFormatter;
    }

    const SvNumberFormatter* GetNumberFormatter(bool bCreate = true) const
    {
        return const_cast<SwDoc*>(this)->GetNumberFormatter(bCreate);
    }

    bool HasInvisibleContent() const;
    // delete invisible content, like hidden sections and paragraphs
    SW_DLLPUBLIC bool RemoveInvisibleContent();
    // restore the invisible content if it's available on the undo stack
    bool RestoreInvisibleContent();

    // Replace fields by text - mailmerge support
    bool ConvertFieldsToText(SwRootFrame const& rLayout);
    bool ConvertFieldToText(const SwField& rField, SwRootFrame const& rLayout);

    // Create sub-documents according to given collection.
    // If no collection is given, use chapter styles for 1st level.
    bool GenerateGlobalDoc( const OUString& rPath,
                                const SwTextFormatColl* pSplitColl );
    bool GenerateGlobalDoc( const OUString& rPath, int nOutlineLevel );
    bool GenerateHTMLDoc( const OUString& rPath,
                                const SwTextFormatColl* pSplitColl );
    bool GenerateHTMLDoc( const OUString& rPath, int nOutlineLevel );

    //  Compare two documents.
    tools::Long CompareDoc( const SwDoc& rDoc );

    // Merge two documents.
    tools::Long MergeDoc( const SwDoc& rDoc );

    bool IsAutoFormatRedline() const           { return mbIsAutoFormatRedline; }
    void SetAutoFormatRedline( bool bFlag )    { mbIsAutoFormatRedline = bFlag; }

    // For AutoFormat: with Undo/Redlining.
    void SetTextFormatCollByAutoFormat( const SwPosition& rPos, sal_uInt16 nPoolId,
                                const SfxItemSet* pSet );
    void SetFormatItemByAutoFormat( const SwPaM& rPam, const SfxItemSet& );

    // Only for SW-textbloxks! Does not pay any attention to layout!
    void ClearDoc();        // Deletes all content!

    // Query /set data for PagePreview.
    const SwPagePreviewPrtData* GetPreviewPrtData() const { return m_pPgPViewPrtData.get(); }

    // If pointer == 0 destroy pointer in document.
    // Else copy object.
    // Pointer is not transferred to ownership by document!
    void SetPreviewPrtData( const SwPagePreviewPrtData* pData );

    /** update all modified OLE-Objects. The modification is called over the
     StarOne - Interface */
    void SetOLEObjModified();

    // Uno - Interfaces
    SW_DLLPUBLIC std::shared_ptr<SwUnoCursor> CreateUnoCursor( const SwPosition& rPos, bool bTableCursor = false );

    // FeShell - Interfaces
    // !!! These assume always an existing layout !!!
    bool ChgAnchor( const SdrMarkList& _rMrkList,
                        RndStdIds _eAnchorType,
                        const bool _bSameOnly,
                        const bool _bPosCorr );

    void SetRowHeight( const SwCursor& rCursor, const SwFormatFrameSize &rNew );
    static std::unique_ptr<SwFormatFrameSize> GetRowHeight( const SwCursor& rCursor );
    void SetRowSplit( const SwCursor& rCursor, const SwFormatRowSplit &rNew );
    static std::unique_ptr<SwFormatRowSplit> GetRowSplit( const SwCursor& rCursor );

    /// Adjustment of Rowheights. Determine via bTstOnly if more than one row is selected.
    /// bOptimize: distribute current table height, instead of using the largest row.
    ///   Call again without bOptimize to ensure equal height in case some row's content didn't fit.
    bool BalanceRowHeight( const SwCursor& rCursor, bool bTstOnly, const bool bOptimize );
    void SetRowBackground( const SwCursor& rCursor, const SvxBrushItem &rNew );
    static bool GetRowBackground( const SwCursor& rCursor, std::unique_ptr<SvxBrushItem>& rToFill );
    /// rNotTracked = false means that the row was deleted or inserted with its tracked cell content
    /// bAll: delete all table rows without selection
    /// bIns: insert table row
    void SetRowNotTracked( const SwCursor& rCursor,
                        const SvxPrintItem &rNotTracked, bool bAll = false, bool bIns = false );
    /// don't call SetRowNotTracked() for rows with tracked row change
    static bool HasRowNotTracked( const SwCursor& rCursor );
    void SetTabBorders( const SwCursor& rCursor, const SfxItemSet& rSet );
    void SetTabLineStyle( const SwCursor& rCursor,
                          const Color* pColor, bool bSetLine,
                          const editeng::SvxBorderLine* pBorderLine );
    static void GetTabBorders( const SwCursor& rCursor, SfxItemSet& rSet );
    void SetBoxAttr( const SwCursor& rCursor, const SfxPoolItem &rNew );
    /**
    Retrieves a box attribute from the given cursor.

    @return Whether the property is set over the current box selection.

    @remarks A property is 'set' if it's set to the same value over all boxes in the current selection.
    The property value is retrieved from the first box in the current selection. It is then compared to
    the values of the same property over any other boxes in the selection; if any value is different from
    that of the first box, the property is unset (and false is returned).
    */
    static bool GetBoxAttr( const SwCursor& rCursor, std::unique_ptr<SfxPoolItem>& rToFill );
    void SetBoxAlign( const SwCursor& rCursor, sal_uInt16 nAlign );
    static sal_uInt16 GetBoxAlign( const SwCursor& rCursor );
    /// Adjusts selected cell widths in such a way, that their content does not need to be wrapped (if possible).
    /// bBalance evenly re-distributes the available space regardless of content or wrapping.
    /// bNoShrink keeps table size the same by distributing excess space proportionately.
    void AdjustCellWidth( const SwCursor& rCursor, const bool bBalance, const bool bNoShrink );

    SW_DLLPUBLIC SwChainRet Chainable( const SwFrameFormat &rSource, const SwFrameFormat &rDest );
    SwChainRet Chain( SwFrameFormat &rSource, const SwFrameFormat &rDest );
    void Unchain( SwFrameFormat &rFormat );

    // For Copy/Move from FrameShell.
    rtl::Reference<SdrObject> CloneSdrObj( const SdrObject&, bool bMoveWithinDoc = false,
                            bool bInsInPage = true );

    // FeShell - Interface end

    // Interface for TextInputData - for text input of Chinese and Japanese.
    SwExtTextInput* CreateExtTextInput( const SwPaM& rPam );
    void DeleteExtTextInput( SwExtTextInput* pDel );
    SwExtTextInput* GetExtTextInput( const SwNode& rNd,
                                sal_Int32 nContentPos = -1) const;
    SwExtTextInput* GetExtTextInput() const;

    // Interface for access to AutoComplete-List.
    static SwAutoCompleteWord& GetAutoCompleteWords() { return *s_pAutoCompleteWords; }

    bool ContainsMSVBasic() const          { return mbContains_MSVBasic; }
    void SetContainsMSVBasic( bool bFlag )  { mbContains_MSVBasic = bFlag; }

    // Interface for the list of Ruby - texts/attributes
    static sal_uInt16 FillRubyList( const SwPaM& rPam, SwRubyList& rList );
    void SetRubyList( SwPaM& rPam, const SwRubyList& rList );

    void ReadLayoutCache( SvStream& rStream );
    void WriteLayoutCache( SvStream& rStream );
    SwLayoutCache* GetLayoutCache() const { return mpLayoutCache.get(); }

    /** Checks if any of the text node contains hidden characters.
        Used for optimization. Changing the view option 'view hidden text'
        has to trigger a reformatting only if some of the text is hidden.
    */
    bool ContainsHiddenChars() const;

    std::unique_ptr<sw::GrammarContact> const& getGrammarContact() const { return mpGrammarContact; }
    std::unique_ptr<sw::OnlineAccessibilityCheck> const& getOnlineAccessibilityCheck() const
    {
        return mpOnlineAccessibilityCheck;
    }

    /** Marks/Unmarks a list level of a certain list

        levels of a certain lists are marked now

        @param sListId    list Id of the list whose level has to be marked/unmarked
        @param nListLevel level to mark
        @param bValue     - true  mark the level
                          - false unmark the level
    */
    void MarkListLevel( const OUString& sListId,
                        const int nListLevel,
                        const bool bValue );

    // Change a format undoable.
    SW_DLLPUBLIC void ChgFormat(SwFormat & rFormat, const SfxItemSet & rSet);

    void RenameFormat(SwFormat & rFormat, const UIName & sNewName,
                   bool bBroadcast = false);

    // Change a TOX undoable.
    void ChangeTOX(SwTOXBase & rTOX, const SwTOXBase & rNew);

    /**
       Returns a textual description of a PaM.

       @param rPaM     the PaM to describe

       If rPaM only spans one paragraph the result is:

            '<text in the PaM>'

       <text in the PaM> is shortened to nUndoStringLength characters.

       If rPaM spans more than one paragraph the result is:

            paragraphs                               (STR_PARAGRAPHS)

       @return the textual description of rPaM
     */
    static OUString GetPaMDescr(const SwPaM & rPaM);

    static bool IsFirstOfNumRuleAtPos(const SwPosition & rPos, SwRootFrame const& rLayout);

    // access methods for XForms model(s)

    // access container for XForms model; will be NULL if !isXForms()
    const css::uno::Reference<css::container::XNameContainer>&
        getXForms() const { return mxXForms;}

    css::uno::Reference< css::linguistic2::XProofreadingIterator > const & GetGCIterator() const;

    // #i31958# is this an XForms document?
    bool isXForms() const;

    // #i31958# initialize XForms models; turn this into an XForms document
    void initXForms( bool bCreateDefaultModel );

    // #i113606# for disposing XForms
    void disposeXForms( );

    //Update all the page masters
    SW_DLLPUBLIC void SetDefaultPageMode(bool bSquaredPageMode);
    SW_DLLPUBLIC bool IsSquaredPageMode() const;

    const css::uno::Reference< ooo::vba::word::XFind >& getVbaFind() const { return mxVbaFind; }
    void setVbaFind( const css::uno::Reference< ooo::vba::word::XFind > &xFind) { mxVbaFind = xFind; }
    css::uno::Reference< css::script::vba::XVBAEventProcessor > const & GetVbaEventProcessor();
    SW_DLLPUBLIC void SetVbaEventProcessor();
    void SetVBATemplateToProjectCache( css::uno::Reference< css::container::XNameContainer > const & xCache ) { m_xTemplateToProjectCache = xCache; };
    const css::uno::Reference< css::container::XNameContainer >& GetVBATemplateToProjectCache() const { return m_xTemplateToProjectCache; };
    ::sfx2::IXmlIdRegistry& GetXmlIdRegistry();
    SW_DLLPUBLIC ::sw::MetaFieldManager & GetMetaFieldManager();
    SW_DLLPUBLIC ::SwContentControlManager& GetContentControlManager();
    SW_DLLPUBLIC ::sw::UndoManager      & GetUndoManager();
    ::sw::UndoManager const& GetUndoManager() const;

    rtl::Reference<SfxObjectShell> CreateCopy(bool bCallInitNew, bool bEmpty) const;
    SwNodeIndex AppendDoc(const SwDoc& rSource, sal_uInt16 nStartPageNumber,
                 bool bDeletePrevious, int physicalPageOffset,
                 const sal_uLong nDocNo);

    /**
     * Dumps the entire nodes structure to the given destination (file nodes.xml in the current directory by default)
     */
    void dumpAsXml(xmlTextWriterPtr = nullptr) const;

    std::set<Color> GetDocColors();
    std::vector< std::weak_ptr<SwUnoCursor> > mvUnoCursorTable;

    // Remove expired UnoCursor weak pointers the document keeps to notify about document death.
    void cleanupUnoCursorTable() const
    {
        auto & rTable = const_cast<SwDoc*>(this)->mvUnoCursorTable;
        // In most cases we'll remove most of the elements.
        std::erase_if(rTable, [] (std::weak_ptr<SwUnoCursor> const & x) { return x.expired(); });
    }

    /**
     * @param bSkipStart don't actually start the jobs, just check
     * @returns true if new background checking jobs were started
     */
    bool StartGrammarChecking( bool bSkipStart = false );

    /// Use to notify if the dictionary can be found for a single content portion (has to be called for all portions)
    void SetMissingDictionaries( bool bIsMissing );
    /// Returns true if no dictionary can be found for any content
    bool IsDictionaryMissing() const { return meDictionaryMissing == MissingDictionary::True; }

    void SetLanguage(const LanguageType eLang, const sal_uInt16 nId);

    static bool HasParagraphDirectFormatting(const SwPosition& rPos);

    bool SetDontCorrectBookmarks(bool val) { return std::exchange(mbDontCorrectBookmarks, val); }

private:
    // Copies master header to left / first one, if necessary - used by ChgPageDesc().
    void CopyMasterHeader(const SwPageDesc &rChged, const SwFormatHeader &rHead, SwPageDesc &pDesc, bool bLeft, bool bFirst);
    // Copies master footer to left / first one, if necessary - used by ChgPageDesc().
    void CopyMasterFooter(const SwPageDesc &rChged, const SwFormatFooter &rFoot, SwPageDesc &pDesc, bool bLeft, bool bFirst);

};

namespace o3tl {
    template<> struct typed_flags<SwDoc::RowColMode> : is_typed_flags<SwDoc::RowColMode, 3> {};
    template<> struct typed_flags<SwDoc::SetNumRuleMode> : is_typed_flags<SwDoc::SetNumRuleMode, 0x0f> {};
}

// This method is called in Dtor of SwDoc and deletes cache of ContourObjects.
void ClrContourCache();

inline void SwDoc::SetOLEPrtNotifyPending( bool bSet )
{
    mbOLEPrtNotifyPending = bSet;
    if( !bSet )
        mbAllOLENotify = false;
}

bool sw_GetPostIts(const IDocumentFieldsAccess& rIDFA, SetGetExpFields * pSrtLst);

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
