/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <sal/config.h>
#include <test/bootstrapfixture.hxx>

#include <com/sun/star/i18n/WordType.hpp>

#include <comphelper/processfactory.hxx>
#include <i18nutil/transliteration.hxx>
#include <editeng/adjustitem.hxx>
#include <editeng/boxitem.hxx>
#include <editeng/brushitem.hxx>
#include <editeng/colritem.hxx>
#include <editeng/contouritem.hxx>
#include <editeng/crossedoutitem.hxx>
#include <editeng/fhgtitem.hxx>
#include <editeng/fontitem.hxx>
#include <editeng/justifyitem.hxx>
#include <editeng/lineitem.hxx>
#include <editeng/postitem.hxx>
#include <editeng/shdditem.hxx>
#include <editeng/udlnitem.hxx>
#include <editeng/wghtitem.hxx>
#include <svl/intitem.hxx>
#include <svx/algitem.hxx>
#include <svx/rotmodit.hxx>
#include <tools/urlobj.hxx>
#include <unotools/tempfile.hxx>
#include <unotools/transliterationwrapper.hxx>

#include <editeng/langitem.hxx>
#include <editeng/charhiddenitem.hxx>

#include <sfx2/docfilt.hxx>
#include <sfx2/docfile.hxx>

#include <xmloff/odffields.hxx>

#include <breakit.hxx>
#include <doc.hxx>
#include <IDocumentRedlineAccess.hxx>
#include <IDocumentFieldsAccess.hxx>
#include <IDocumentStatistics.hxx>
#include <cellfml.hxx>
#include <docsh.hxx>
#include <docstat.hxx>
#include <docufld.hxx>
#include <fmtanchr.hxx>
#include <ndtxt.hxx>
#include <shellres.hxx>
#include <swscanner.hxx>
#include <swdll.hxx>
#include <swtypes.hxx>
#include <fmtftn.hxx>
#include <fmtrfmrk.hxx>
#include <fmtinfmt.hxx>
#include <fchrfmt.hxx>
#include <fmtfld.hxx>
#include <redline.hxx>
#include <docary.hxx>
#include <modeltoviewhelper.hxx>
#include <IMark.hxx>
#include <ring.hxx>
#include <calbck.hxx>
#include <pagedesc.hxx>
#include <calc.hxx>
#include <scriptinfo.hxx>
#include <rubylist.hxx>
#include <txatbase.hxx>

#include <tblafmt.hxx>
#include <unotbl.hxx>
#include <IDocumentMarkAccess.hxx>
#include <itabenum.hxx>

typedef rtl::Reference<SwDocShell> SwDocShellRef;

using namespace ::com::sun::star;

/* Implementation of Swdoc-Test class */

class SwDocTest : public test::BootstrapFixture
{
public:
    SwDocTest()
        : m_pDoc(nullptr)
    {
    }

    virtual void setUp() override;
    virtual void tearDown() override;

    void testTableAutoFormats();
    void testPageDescName();
    void testFileNameFields();
    void testDocStat();
    void testModelToViewHelperPassthrough();
    void testModelToViewHelperExpandFieldsExpandFootnote();
    void testModelToViewHelperExpandFieldsExpandFootnoteReplaceMode();
    void testModelToViewHelperExpandFields();
    void testModelToViewHelperExpandFieldsReplaceMode();
    void testModelToViewHelperExpandFieldsHideInvisible();
    void testModelToViewHelperExpandFieldsHideRedlined();
    void testModelToViewHelperExpandFieldsHideInvisibleExpandFootnote();
    void testModelToViewHelperExpandFieldsHideInvisibleExpandFootnoteReplaceMode();
    void testModelToViewHelperExpandFieldsHideHideRedlinedExpandFootnote();
    void testModelToViewHelperExpandFieldsHideHideRedlinedExpandFootnoteReplaceMode();
    void testModelToViewHelperHideInvisibleHideRedlined();
    void testModelToViewHelperExpandFieldsHideInvisibleHideRedlinedExpandFootnote();
    void testModelToViewHelperExpandFieldsHideInvisibleHideRedlinedExpandFootnoteReplaceMode();
    void testModelToViewHelperExpandFieldsExpandFootnote2();
    void testModelToViewHelperExpandFieldsExpandFootnoteReplaceMode2();
    void testSwScanner();
    void testUserPerceivedCharCount();
    void testMergePortionsDeleteNotSorted();
    void testGraphicAnchorDeletion();
    void testTransliterate();
    void testMarkMove();
    void testFormulas();
    void testIntrusiveRing();
    void testClientModify();
    void testBroadcastingModify();
    void testWriterMultiListener();
    void test64kPageDescs();
    void testTdf92308();
    void testTableCellComparison();
    void testFillRubyList();
    void testSetRubyList();

    CPPUNIT_TEST_SUITE(SwDocTest);

    CPPUNIT_TEST(testTransliterate);
    CPPUNIT_TEST(testTableAutoFormats);
    CPPUNIT_TEST(testPageDescName);
    CPPUNIT_TEST(testFileNameFields);
    CPPUNIT_TEST(testDocStat);
    CPPUNIT_TEST(testModelToViewHelperPassthrough);
    CPPUNIT_TEST(testModelToViewHelperExpandFieldsExpandFootnote);
    CPPUNIT_TEST(testModelToViewHelperExpandFieldsExpandFootnoteReplaceMode);
    CPPUNIT_TEST(testModelToViewHelperExpandFields);
    CPPUNIT_TEST(testModelToViewHelperExpandFieldsReplaceMode);
    CPPUNIT_TEST(testModelToViewHelperExpandFieldsHideInvisible);
    CPPUNIT_TEST(testModelToViewHelperExpandFieldsHideRedlined);
    CPPUNIT_TEST(testModelToViewHelperExpandFieldsHideInvisibleExpandFootnote);
    CPPUNIT_TEST(testModelToViewHelperExpandFieldsHideInvisibleExpandFootnoteReplaceMode);
    CPPUNIT_TEST(testModelToViewHelperExpandFieldsHideHideRedlinedExpandFootnote);
    CPPUNIT_TEST(testModelToViewHelperExpandFieldsHideHideRedlinedExpandFootnoteReplaceMode);
    CPPUNIT_TEST(testModelToViewHelperHideInvisibleHideRedlined);
    CPPUNIT_TEST(testModelToViewHelperExpandFieldsHideInvisibleHideRedlinedExpandFootnote);
    CPPUNIT_TEST(testModelToViewHelperExpandFieldsHideInvisibleHideRedlinedExpandFootnoteReplaceMode);
    CPPUNIT_TEST(testModelToViewHelperExpandFieldsExpandFootnote2);
    CPPUNIT_TEST(testModelToViewHelperExpandFieldsExpandFootnoteReplaceMode2);
    CPPUNIT_TEST(testSwScanner);
    CPPUNIT_TEST(testUserPerceivedCharCount);
    CPPUNIT_TEST(testMergePortionsDeleteNotSorted);
    CPPUNIT_TEST(testGraphicAnchorDeletion);
    CPPUNIT_TEST(testMarkMove);
    CPPUNIT_TEST(testFormulas);
    CPPUNIT_TEST(testIntrusiveRing);
    CPPUNIT_TEST(testClientModify);
    CPPUNIT_TEST(testBroadcastingModify);
    CPPUNIT_TEST(testWriterMultiListener);
    CPPUNIT_TEST(test64kPageDescs);
    CPPUNIT_TEST(testTdf92308);
    CPPUNIT_TEST(testTableCellComparison);
    CPPUNIT_TEST(testFillRubyList);
    CPPUNIT_TEST(testSetRubyList);
    CPPUNIT_TEST_SUITE_END();

private:
    SwDoc *m_pDoc;
    SwDocShellRef m_xDocShRef;
};

void SwDocTest::testPageDescName()
{
    ShellResource aShellResources;

    std::vector<OUString> aResults;

    //These names must be unique for each different combination, otherwise
    //duplicate page description names may exist, which will causes lookup
    //by name to be incorrect, and so the corresponding export to .odt
    aResults.push_back(aShellResources.GetPageDescName(1, ShellResource::NORMAL_PAGE));
    aResults.push_back(aShellResources.GetPageDescName(1, ShellResource::FIRST_PAGE));
    aResults.push_back(aShellResources.GetPageDescName(1, ShellResource::FOLLOW_PAGE));

    std::sort(aResults.begin(), aResults.end());
    aResults.erase(std::unique(aResults.begin(), aResults.end()), aResults.end());

    CPPUNIT_ASSERT_EQUAL_MESSAGE("GetPageDescName results must be unique", static_cast<size_t>(3), aResults.size());
}

//See https://bugs.libreoffice.org/show_bug.cgi?id=32463
void SwDocTest::testFileNameFields()
{
    //Here's a file name with some chars in it that will be %% encoded, when expanding
    //SwFileNameFields we want to restore the original readable filename
    utl::TempFileNamed aTempFile(u"demo [name]");
    aTempFile.EnableKillingFile();

    INetURLObject aTempFileURL(aTempFile.GetURL());
    OUString sFileURL = aTempFileURL.GetMainURL(INetURLObject::DecodeMechanism::NONE);
    SfxMedium aDstMed(sFileURL, StreamMode::STD_READWRITE);

    auto pFilter = std::make_shared<SfxFilter>(
        "Text",
        OUString(), SfxFilterFlags::NONE, SotClipboardFormatId::NONE, OUString(), OUString(),
        "TEXT", OUString() );
    aDstMed.SetFilter(pFilter);

    m_xDocShRef->DoSaveAs(aDstMed);
    m_xDocShRef->DoSaveCompleted(&aDstMed);

    const INetURLObject &rUrlObj = m_xDocShRef->GetMedium()->GetURLObject();

    SwFileNameFieldType aNameField(*m_pDoc);

    {
        OUString sResult(aNameField.Expand(SwFileNameFormat::Name));
        OUString sExpected(rUrlObj.getName(INetURLObject::LAST_SEGMENT,
            true,INetURLObject::DecodeMechanism::WithCharset));
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Expected Readable FileName", sExpected, sResult);
    }

    {
        OUString sResult(aNameField.Expand(SwFileNameFormat::PathName));
        OUString sExpected(rUrlObj.GetFull());
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Expected Readable FileName", sExpected, sResult);
    }

    {
        OUString sResult(aNameField.Expand(SwFileNameFormat::Path));
        INetURLObject aTemp(rUrlObj);
        aTemp.removeSegment();
        OUString sExpected(aTemp.PathToFileName());
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Expected Readable FileName", sExpected, sResult);
    }

    {
        OUString sResult(aNameField.Expand(SwFileNameFormat::NameNoExt));
        OUString sExpected(rUrlObj.getName(INetURLObject::LAST_SEGMENT,
            true,INetURLObject::DecodeMechanism::WithCharset));
        //Chop off .tmp
        sExpected = sExpected.copy(0, sExpected.getLength() - 4);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Expected Readable FileName", sExpected, sResult);
    }

    m_xDocShRef->DoInitNew();
}

//See http://lists.freedesktop.org/archives/libreoffice/2011-August/016666.html
//Remove unnecessary parameter to IDocumentStatistics::UpdateDocStat for
//motivation
void SwDocTest::testDocStat()
{
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Expected initial 0 count", static_cast<sal_uLong>(0), m_pDoc->getIDocumentStatistics().GetDocStat().nChar);

    SwNodeIndex aIdx(m_pDoc->GetNodes().GetEndOfContent(), -1);
    SwPaM aPaM(aIdx);

    OUString sText(u"Hello World"_ustr);
    m_pDoc->getIDocumentContentOperations().InsertString(aPaM, sText);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Should still be non-updated 0 count", static_cast<sal_uLong>(0), m_pDoc->getIDocumentStatistics().GetDocStat().nChar);

    SwDocStat aDocStat = m_pDoc->getIDocumentStatistics().GetUpdatedDocStat( false, true );
    sal_uLong nLen = static_cast<sal_uLong>(sText.getLength());

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Should now have updated count", nLen, aDocStat.nChar);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("And cache is updated too", nLen, m_pDoc->getIDocumentStatistics().GetDocStat().nChar);
}

//For UI character counts we should follow UAX#29 and display the user
//perceived characters, not the number of codepoints, nor the number of code
//units http://unicode.org/reports/tr29/
void SwDocTest::testUserPerceivedCharCount()
{
    SwBreakIt *pBreakIter = SwBreakIt::Get();

    //Grapheme example, two different unicode code-points perceived by the user as a single
    //glyph
    static constexpr OUStringLiteral sALEF_QAMATS = u"\u05D0\u05B8";
    sal_Int32 nGraphemeCount = pBreakIter->getGraphemeCount(sALEF_QAMATS);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Grapheme Count should be 1", static_cast<sal_Int32>(1), nGraphemeCount);

    //Surrogate pair example, one single unicode code-point (U+1D11E)
    //represented as two code units in UTF-16
    static constexpr OUStringLiteral sGCLEF = u"\U0001D11E";
    sal_Int32 nCount = pBreakIter->getGraphemeCount(sGCLEF);
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Surrogate Pair should be counted as single character", static_cast<sal_Int32>(1), nCount);
}

static SwTextNode* getModelToViewTestDocument(SwDoc *pDoc)
{
    SwNodeIndex aIdx(pDoc->GetNodes().GetEndOfContent(), -1);
    SwPaM aPaM(aIdx);

    SwFormatFootnote aFootnote;
    aFootnote.SetNumStr(u"foo"_ustr);

    pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());
    pDoc->getIDocumentContentOperations().InsertString(aPaM, u"AAAAA BBBBB "_ustr);
    SwTextNode* pTextNode = aPaM.GetPointNode().GetTextNode();
    sal_Int32 nPos = aPaM.GetPoint()->GetContentIndex();
    pTextNode->InsertItem(aFootnote, nPos, nPos);
    pDoc->getIDocumentContentOperations().InsertString(aPaM, u" CCCCC "_ustr);
    nPos = aPaM.GetPoint()->GetContentIndex();
    pTextNode->InsertItem(aFootnote, nPos, nPos);
    pDoc->getIDocumentContentOperations().InsertString(aPaM, u" DDDDD"_ustr);
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_Int32>((4*5) + 5 + 2), pTextNode->GetText().getLength());

    //set start of selection to first B
    aPaM.GetPoint()->nContent.Assign(aPaM.GetPointContentNode(), 6);
    aPaM.SetMark();
    //set end of selection to last C
    aPaM.GetPoint()->nContent.Assign(aPaM.GetPointContentNode(), 14);
    //set character attribute hidden on range
    SvxCharHiddenItem aHidden(true, RES_CHRATR_HIDDEN);
    pDoc->getIDocumentContentOperations().InsertPoolItem(aPaM, aHidden );
    aPaM.DeleteMark();

    //turn on red-lining and show changes
    pDoc->getIDocumentRedlineAccess().SetRedlineFlags(RedlineFlags::On | RedlineFlags::ShowDelete|RedlineFlags::ShowInsert);
    CPPUNIT_ASSERT_MESSAGE("redlining should be on", pDoc->getIDocumentRedlineAccess().IsRedlineOn());
    CPPUNIT_ASSERT_MESSAGE("redlines should be visible", IDocumentRedlineAccess::IsShowChanges(pDoc->getIDocumentRedlineAccess().GetRedlineFlags()));

    //set start of selection to last A
    aPaM.GetPoint()->nContent.Assign(aPaM.GetPointContentNode(), 4);
    aPaM.SetMark();
    //set end of selection to second last B
    aPaM.GetPoint()->nContent.Assign(aPaM.GetPointContentNode(), 9);
    pDoc->getIDocumentContentOperations().DeleteAndJoin(aPaM);    //redline-aware deletion api
    aPaM.DeleteMark();

    return pTextNode;
}

static SwTextNode* getModelToViewTestDocument2(SwDoc *pDoc)
{
    getModelToViewTestDocument(pDoc);

    SwNodeIndex aIdx(pDoc->GetNodes().GetEndOfContent(), -1);
    SwPaM aPaM(aIdx);

    pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());
    pDoc->getIDocumentContentOperations().InsertString(aPaM, u"AAAAA"_ustr);
    IDocumentMarkAccess* pMarksAccess = pDoc->getIDocumentMarkAccess();
    sw::mark::Fieldmark *pFieldmark =
            pMarksAccess->makeNoTextFieldBookmark(aPaM, SwMarkName(u"test"_ustr), ODF_FORMDROPDOWN);
    CPPUNIT_ASSERT(pFieldmark);
    uno::Sequence< OUString > vListEntries { u"BBBBB"_ustr };
    (*pFieldmark->GetParameters())[ODF_FORMDROPDOWN_LISTENTRY] <<= vListEntries;
    (*pFieldmark->GetParameters())[ODF_FORMDROPDOWN_RESULT] <<= sal_Int32(0);
    pDoc->getIDocumentContentOperations().InsertString(aPaM, u"CCCCC"_ustr);
    SwTextNode* pTextNode = aPaM.GetPointNode().GetTextNode();
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_Int32>(11),
            pTextNode->GetText().getLength());

    return pTextNode;
}

void SwDocTest::testModelToViewHelperPassthrough()
{
    SwTextNode* pTextNode = getModelToViewTestDocument(m_pDoc);

    ModelToViewHelper aModelToViewHelper(*pTextNode, nullptr, ExpandMode::PassThrough);
    OUString sViewText = aModelToViewHelper.getViewText();
    OUString sModelText = pTextNode->GetText();
    CPPUNIT_ASSERT_EQUAL(sModelText, sViewText);
}

void SwDocTest::testModelToViewHelperExpandFieldsExpandFootnote()
{
    SwTextNode* pTextNode = getModelToViewTestDocument(m_pDoc);

    ModelToViewHelper aModelToViewHelper(*pTextNode, nullptr,
            ExpandMode::ExpandFields | ExpandMode::ExpandFootnote);
    OUString sViewText = aModelToViewHelper.getViewText();
    CPPUNIT_ASSERT_EQUAL(
        u"AAAAA BBBBB foo CCCCC foo DDDDD"_ustr, sViewText);
}

void SwDocTest::testModelToViewHelperExpandFieldsExpandFootnoteReplaceMode()
{
    SwTextNode* pTextNode = getModelToViewTestDocument(m_pDoc);

    ModelToViewHelper aModelToViewHelper(*pTextNode, nullptr,
            ExpandMode::ExpandFields | ExpandMode::ExpandFootnote | ExpandMode::ReplaceMode);
    OUString sViewText = aModelToViewHelper.getViewText();
    CPPUNIT_ASSERT_EQUAL(
        OUString("AAAAA BBBBB " + OUStringChar(CHAR_ZWSP) + " CCCCC " + OUStringChar(CHAR_ZWSP) + " DDDDD"),
        sViewText);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2),
        aModelToViewHelper.getFootnotePositions().size());
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_Int32>(12),
        aModelToViewHelper.getFootnotePositions()[0]);
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_Int32>(20),
        aModelToViewHelper.getFootnotePositions()[1]);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0),
        aModelToViewHelper.getFieldPositions().size());
}

void SwDocTest::testModelToViewHelperExpandFields()
{
    SwTextNode* pTextNode = getModelToViewTestDocument(m_pDoc);

    ModelToViewHelper aModelToViewHelper(*pTextNode, nullptr, ExpandMode::ExpandFields);
    OUString sViewText = aModelToViewHelper.getViewText();
    CPPUNIT_ASSERT_EQUAL(
        u"AAAAA BBBBB  CCCCC  DDDDD"_ustr, sViewText);
}

void SwDocTest::testModelToViewHelperExpandFieldsReplaceMode()
{
    SwTextNode* pTextNode = getModelToViewTestDocument(m_pDoc);

    ModelToViewHelper aModelToViewHelper(*pTextNode, nullptr,
        ExpandMode::ExpandFields | ExpandMode::ReplaceMode);
    OUString sViewText = aModelToViewHelper.getViewText();
    CPPUNIT_ASSERT_EQUAL(u"AAAAA BBBBB  CCCCC  DDDDD"_ustr,
        sViewText);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0),
        aModelToViewHelper.getFootnotePositions().size());
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0),
        aModelToViewHelper.getFieldPositions().size());
}

void SwDocTest::testModelToViewHelperExpandFieldsHideInvisible()
{
    SwTextNode* pTextNode = getModelToViewTestDocument(m_pDoc);

    ModelToViewHelper aModelToViewHelper(*pTextNode, nullptr, ExpandMode::HideInvisible);
    OUString sViewText = aModelToViewHelper.getViewText();
    CPPUNIT_ASSERT_EQUAL(
        OUString("AAAAA CCCCC " + OUStringChar(CH_TXTATR_BREAKWORD) + " DDDDD"),
        sViewText);
}

void SwDocTest::testModelToViewHelperExpandFieldsHideRedlined()
{
    SwTextNode* pTextNode = getModelToViewTestDocument(m_pDoc);

    ModelToViewHelper aModelToViewHelper(*pTextNode, nullptr, ExpandMode::HideDeletions);
    OUString sViewText = aModelToViewHelper.getViewText();
    CPPUNIT_ASSERT_EQUAL(
        OUString("AAAABB " + OUStringChar(CH_TXTATR_BREAKWORD) + " CCCCC " + OUStringChar(CH_TXTATR_BREAKWORD) + " DDDDD"),
        sViewText);
}

void SwDocTest::testModelToViewHelperExpandFieldsHideInvisibleExpandFootnote()
{
    SwTextNode* pTextNode = getModelToViewTestDocument(m_pDoc);

    ModelToViewHelper aModelToViewHelper(*pTextNode, nullptr,
        ExpandMode::ExpandFields | ExpandMode::HideInvisible | ExpandMode::ExpandFootnote);
    OUString sViewText = aModelToViewHelper.getViewText();
    CPPUNIT_ASSERT_EQUAL(u"AAAAA CCCCC foo DDDDD"_ustr, sViewText);
}

void SwDocTest::testModelToViewHelperExpandFieldsHideInvisibleExpandFootnoteReplaceMode()
{
    SwTextNode* pTextNode = getModelToViewTestDocument(m_pDoc);

    ModelToViewHelper aModelToViewHelper(*pTextNode, nullptr,
        ExpandMode::ExpandFields | ExpandMode::HideInvisible | ExpandMode::ExpandFootnote | ExpandMode::ReplaceMode);
    OUString sViewText = aModelToViewHelper.getViewText();
    CPPUNIT_ASSERT_EQUAL(
        OUString("AAAAA CCCCC " + OUStringChar(CHAR_ZWSP) + " DDDDD"),
        sViewText);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1),
        aModelToViewHelper.getFootnotePositions().size());
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_Int32>(12),
        aModelToViewHelper.getFootnotePositions()[0]);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0),
        aModelToViewHelper.getFieldPositions().size());
}

void SwDocTest::testModelToViewHelperExpandFieldsHideHideRedlinedExpandFootnote()
{
    SwTextNode* pTextNode = getModelToViewTestDocument(m_pDoc);

    ModelToViewHelper aModelToViewHelper(*pTextNode, nullptr,
        ExpandMode::ExpandFields | ExpandMode::HideDeletions | ExpandMode::ExpandFootnote);
    OUString sViewText = aModelToViewHelper.getViewText();
    CPPUNIT_ASSERT_EQUAL(
        u"AAAABB foo CCCCC foo DDDDD"_ustr, sViewText);
}

void SwDocTest::testModelToViewHelperExpandFieldsHideHideRedlinedExpandFootnoteReplaceMode()
{
    SwTextNode* pTextNode = getModelToViewTestDocument(m_pDoc);

    ModelToViewHelper aModelToViewHelper(*pTextNode, nullptr,
        ExpandMode::ExpandFields | ExpandMode::HideDeletions | ExpandMode::ExpandFootnote | ExpandMode::ReplaceMode);
    OUString sViewText = aModelToViewHelper.getViewText();
    CPPUNIT_ASSERT_EQUAL(
       OUString("AAAABB " + OUStringChar(CHAR_ZWSP) + " CCCCC " + OUStringChar(CHAR_ZWSP) + " DDDDD"),
       sViewText);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2),
        aModelToViewHelper.getFootnotePositions().size());
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_Int32>(7),
        aModelToViewHelper.getFootnotePositions()[0]);
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_Int32>(15),
        aModelToViewHelper.getFootnotePositions()[1]);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0),
        aModelToViewHelper.getFieldPositions().size());
}

void SwDocTest::testModelToViewHelperHideInvisibleHideRedlined()
{
    SwTextNode* pTextNode = getModelToViewTestDocument(m_pDoc);

    ModelToViewHelper aModelToViewHelper(*pTextNode, nullptr,
        ExpandMode::HideInvisible | ExpandMode::HideDeletions);
    OUString sViewText = aModelToViewHelper.getViewText();
    OUString aBuffer = "AAAACCCCC " +
        OUStringChar(CH_TXTATR_BREAKWORD) +
        " DDDDD";
    CPPUNIT_ASSERT_EQUAL(aBuffer, sViewText);
}

void SwDocTest::testModelToViewHelperExpandFieldsHideInvisibleHideRedlinedExpandFootnote()
{
    SwTextNode* pTextNode = getModelToViewTestDocument(m_pDoc);

    ModelToViewHelper aModelToViewHelper(*pTextNode, nullptr,
        ExpandMode::ExpandFields | ExpandMode::HideInvisible | ExpandMode::HideDeletions | ExpandMode::ExpandFootnote);
    OUString sViewText = aModelToViewHelper.getViewText();
    CPPUNIT_ASSERT_EQUAL(u"AAAACCCCC foo DDDDD"_ustr, sViewText);
}

void SwDocTest::testModelToViewHelperExpandFieldsHideInvisibleHideRedlinedExpandFootnoteReplaceMode()
{
    SwTextNode* pTextNode = getModelToViewTestDocument(m_pDoc);

    ModelToViewHelper aModelToViewHelper(*pTextNode, nullptr,
        ExpandMode::ExpandFields | ExpandMode::HideInvisible | ExpandMode::HideDeletions | ExpandMode::ExpandFootnote | ExpandMode::ReplaceMode);
    OUString sViewText = aModelToViewHelper.getViewText();
    CPPUNIT_ASSERT_EQUAL(sViewText,
        OUString("AAAACCCCC " + OUStringChar(CHAR_ZWSP) + " DDDDD"));
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1),
        aModelToViewHelper.getFootnotePositions().size());
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_Int32>(10),
        aModelToViewHelper.getFootnotePositions()[0]);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0),
        aModelToViewHelper.getFieldPositions().size());
}

void SwDocTest::testModelToViewHelperExpandFieldsExpandFootnote2()
{
    SwTextNode* pTextNode = getModelToViewTestDocument2(m_pDoc);

    ModelToViewHelper aModelToViewHelper(*pTextNode, nullptr,
        ExpandMode::ExpandFields | ExpandMode::ExpandFootnote);
    OUString sViewText = aModelToViewHelper.getViewText();
    CPPUNIT_ASSERT_EQUAL(u"AAAAABBBBBCCCCC"_ustr, sViewText);
}

void SwDocTest::testModelToViewHelperExpandFieldsExpandFootnoteReplaceMode2()
{
    SwTextNode* pTextNode = getModelToViewTestDocument2(m_pDoc);

    ModelToViewHelper aModelToViewHelper(*pTextNode, nullptr,
        ExpandMode::ExpandFields | ExpandMode::ExpandFootnote | ExpandMode::ReplaceMode);
    OUString sViewText = aModelToViewHelper.getViewText();
    CPPUNIT_ASSERT_EQUAL(
        OUString("AAAAA" + OUStringChar(CHAR_ZWSP) + "CCCCC"),
        sViewText);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(0),
        aModelToViewHelper.getFootnotePositions().size());
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1),
        aModelToViewHelper.getFieldPositions().size());
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_Int32>(5),
        aModelToViewHelper.getFieldPositions()[0]);
}

void SwDocTest::testSwScanner()
{
    SwNodeIndex aIdx(m_pDoc->GetNodes().GetEndOfContent(), -1);
    SwPaM aPaM(aIdx);

    SwTextNode* pTextNode = aPaM.GetPointNode().GetTextNode();

    CPPUNIT_ASSERT_MESSAGE("Has Text Node", pTextNode);

    //See https://bugs.libreoffice.org/show_bug.cgi?id=40449
    //See https://bugs.libreoffice.org/show_bug.cgi?id=39365
    //Use a temporary OUString as the arg, as that's the trouble behind
    //fdo#40449 and fdo#39365
    {
        SwScanner aScanner(*pTextNode,
            u"Hello World"_ustr,
            nullptr, ModelToViewHelper(), i18n::WordType::DICTIONARY_WORD, 0,
            RTL_CONSTASCII_LENGTH("Hello World"));

        bool bFirstOk = aScanner.NextWord();
        CPPUNIT_ASSERT_MESSAGE("First Token", bFirstOk);
        const OUString &rHello = aScanner.GetWord();
        CPPUNIT_ASSERT_EQUAL(u"Hello"_ustr, rHello);

        bool bSecondOk = aScanner.NextWord();
        CPPUNIT_ASSERT_MESSAGE("Second Token", bSecondOk);
        const OUString &rWorld = aScanner.GetWord();
        CPPUNIT_ASSERT_EQUAL(u"World"_ustr, rWorld);
    }

    //See https://www.libreoffice.org/bugzilla/show_bug.cgi?id=45271
    {
        static constexpr OUString IDEOGRAPHICFULLSTOP_D = u"\u3002D"_ustr;

        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, IDEOGRAPHICFULLSTOP_D);

        SvxLanguageItem aCJKLangItem( LANGUAGE_CHINESE_SIMPLIFIED, RES_CHRATR_CJK_LANGUAGE );
        SvxLanguageItem aWestLangItem( LANGUAGE_ENGLISH_US, RES_CHRATR_LANGUAGE );
        m_pDoc->getIDocumentContentOperations().InsertPoolItem(aPaM, aCJKLangItem );
        m_pDoc->getIDocumentContentOperations().InsertPoolItem(aPaM, aWestLangItem );

        SwDocStat aDocStat;
        pTextNode = aPaM.GetPointNode().GetTextNode();
        pTextNode->CountWords(aDocStat, 0, IDEOGRAPHICFULLSTOP_D.getLength());

        CPPUNIT_ASSERT_EQUAL(static_cast<sal_uLong>(2), aDocStat.nChar);
        CPPUNIT_ASSERT_EQUAL(static_cast<sal_uLong>(2), aDocStat.nCharExcludingSpaces);
    }
    {
        static constexpr OUString test =
            u"\u3053\u306E\u65E5\u672C\u8A9E\u306F\u6B63\u3057"
            "\u304F\u6570\u3048\u3089\u308C\u308B\u3067\u3057"
            "\u3087\u3046\u304B\u3002And "
            "let's th"
            "row some"
            " English"
            " in to m"
            "ake it i"
            "nteresti"
            "ng.  \u305D\u3057\u3066"
            "\u3001\u307E\u305F\u65E5\u672C\u8A9E\u3000\u3000"
            "\u3067\u3082\u4ECA\u56DE\u306F\u7A7A\u767D\u3092"
            "\u3000\u3000\u5165\u308C\u307E\u3057\u305F\u3002"
            "  So how"
            " does th"
            "is do?  "_ustr;
        m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());
        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, test);

        SvxLanguageItem aCJKLangItem( LANGUAGE_JAPANESE, RES_CHRATR_CJK_LANGUAGE );
        SvxLanguageItem aWestLangItem( LANGUAGE_ENGLISH_US, RES_CHRATR_LANGUAGE );
        m_pDoc->getIDocumentContentOperations().InsertPoolItem(aPaM, aCJKLangItem );
        m_pDoc->getIDocumentContentOperations().InsertPoolItem(aPaM, aWestLangItem );

        SwDocStat aDocStat;
        pTextNode = aPaM.GetPointNode().GetTextNode();
        pTextNode->CountWords(aDocStat, 0, test.getLength());
        CPPUNIT_ASSERT_EQUAL_MESSAGE("words", static_cast<sal_uLong>(58), aDocStat.nWord);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("Asian characters and Korean words", static_cast<sal_uLong>(43), aDocStat.nAsianWord);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("non-whitespace chars", static_cast<sal_uLong>(105), aDocStat.nCharExcludingSpaces);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("characters", static_cast<sal_uLong>(128), aDocStat.nChar);
    }

    //See https://bz.apache.org/ooo/show_bug.cgi?id=89042
    //See https://bugs.libreoffice.org/show_bug.cgi?id=53399
    {
        SwDocStat aDocStat;

        static constexpr OUString aShouldBeThree =
            u"Should "
            "\u2018be thr"
            "ee\u2019"_ustr;

        m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());
        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, aShouldBeThree);
        pTextNode = aPaM.GetPointNode().GetTextNode();
        pTextNode->CountWords(aDocStat, 0, aShouldBeThree.getLength());
        CPPUNIT_ASSERT_EQUAL(static_cast<sal_uLong>(3), aDocStat.nWord);

        static constexpr OUString aShouldBeFive =
            u"french "
            // <<   nbsp
            "\u00AB\u00A0savoi"
            // nnbsp
            "r\u202fcalcu"
            //   idspace >>
            "ler\u3000\u00BB"_ustr;

        m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());
        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, aShouldBeFive);
        pTextNode = aPaM.GetPointNode().GetTextNode();
        aDocStat.Reset();
        pTextNode->CountWords(aDocStat, 0, aShouldBeFive.getLength());
        CPPUNIT_ASSERT_EQUAL(static_cast<sal_uLong>(5), aDocStat.nWord);
    }

    //See https://bugs.libreoffice.org/show_bug.cgi?id=49629
    {
        SwDocStat aDocStat;

        m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());
        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, u"Apple"_ustr);
        pTextNode = aPaM.GetPointNode().GetTextNode();
        sal_Int32 nPos = aPaM.GetPoint()->GetContentIndex();
        SwFormatFootnote aFootnote;
        aFootnote.SetNumStr(u"banana"_ustr);
        SwTextAttr* pTA = pTextNode->InsertItem(aFootnote, nPos, nPos);
        CPPUNIT_ASSERT(pTA);
        CPPUNIT_ASSERT_EQUAL(static_cast<sal_Int32>(6), pTextNode->Len()); //Apple + 0x02
        pTextNode->CountWords(aDocStat, 0, pTextNode->Len());
        CPPUNIT_ASSERT_EQUAL(static_cast<sal_uLong>(1), aDocStat.nWord);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("footnote should be expanded", static_cast<sal_uLong>(11), aDocStat.nChar);

        const sal_Int32 nNextPos = aPaM.GetPoint()->GetContentIndex();
        CPPUNIT_ASSERT_EQUAL(static_cast<sal_Int32>(nPos+1), nNextPos);
        SwFormatRefMark aRef(SwMarkName(u"refmark"_ustr));
        pTA = pTextNode->InsertItem(aRef, nNextPos, nNextPos);
        CPPUNIT_ASSERT(pTA);

        aDocStat.Reset();
        pTextNode->SetWordCountDirty(true);
        pTextNode->CountWords(aDocStat, 0, pTextNode->Len());
        CPPUNIT_ASSERT_EQUAL(static_cast<sal_uLong>(1), aDocStat.nWord);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("refmark anchor should not be counted", static_cast<sal_uLong>(11), aDocStat.nChar);

        m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());
        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, u"Apple"_ustr);

        DateTime aDate(DateTime::SYSTEM);
        SwPostItField aPostIt(
            static_cast<SwPostItFieldType*>(m_pDoc->getIDocumentFieldsAccess().GetSysFieldType(SwFieldIds::Postit)), u"An Author"_ustr,
            u"Some Text"_ustr, u"Initials"_ustr, SwMarkName(u"Name"_ustr), aDate );
        m_pDoc->getIDocumentContentOperations().InsertPoolItem(aPaM, SwFormatField(aPostIt));

        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, u"Apple"_ustr);
        pTextNode = aPaM.GetPointNode().GetTextNode();
        aDocStat.Reset();
        pTextNode->CountWords(aDocStat, 0, pTextNode->Len());
        CPPUNIT_ASSERT_EQUAL(static_cast<sal_uLong>(1), aDocStat.nWord);
        CPPUNIT_ASSERT_EQUAL_MESSAGE("postit anchor should effectively not exist", static_cast<sal_uLong>(10), aDocStat.nChar);
        CPPUNIT_ASSERT_EQUAL(static_cast<sal_Int32>(11), pTextNode->Len());

        aDocStat.Reset();
    }

    //See https://bugs.libreoffice.org/show_bug.cgi?id=46757
    {
        SwDocStat aDocStat;

        static constexpr OUString aString = u"Lorem ipsum"_ustr;
        m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());
        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, aString);
        pTextNode = aPaM.GetPointNode().GetTextNode();
        pTextNode->CountWords(aDocStat, 0, pTextNode->Len());
        CPPUNIT_ASSERT_EQUAL(static_cast<sal_uLong>(2), aDocStat.nWord);

        //turn on red-lining and show changes
        m_pDoc->getIDocumentRedlineAccess().SetRedlineFlags(RedlineFlags::On | RedlineFlags::ShowDelete|RedlineFlags::ShowInsert);
        CPPUNIT_ASSERT_MESSAGE("redlining should be on", m_pDoc->getIDocumentRedlineAccess().IsRedlineOn());
        CPPUNIT_ASSERT_MESSAGE("redlines should be visible", IDocumentRedlineAccess::IsShowChanges(m_pDoc->getIDocumentRedlineAccess().GetRedlineFlags()));

        //delete everything except the first word
        aPaM.SetMark(); //set start of selection to current pos
        aPaM.GetPoint()->nContent.Assign(aPaM.GetPointContentNode(), 5);   //set end of selection to fifth char of current node
        m_pDoc->getIDocumentContentOperations().DeleteAndJoin(aPaM);    //redline-aware deletion api
        //"real underlying text should be the same"
        CPPUNIT_ASSERT_EQUAL(aString, pTextNode->GetText());

        aDocStat.Reset();
        pTextNode->SetWordCountDirty(true);
        pTextNode->CountWords(aDocStat, 0, pTextNode->Len()); //but word-counting the text should only count the non-deleted text
        CPPUNIT_ASSERT_EQUAL(static_cast<sal_uLong>(1), aDocStat.nWord);

        pTextNode->SetWordCountDirty(true);

        //keep red-lining on but hide changes
        m_pDoc->getIDocumentRedlineAccess().SetRedlineFlags(RedlineFlags::On);
        CPPUNIT_ASSERT_MESSAGE("redlining should be still on", m_pDoc->getIDocumentRedlineAccess().IsRedlineOn());
        CPPUNIT_ASSERT_MESSAGE("redlines should be invisible", !IDocumentRedlineAccess::IsShowChanges(m_pDoc->getIDocumentRedlineAccess().GetRedlineFlags()));

        aDocStat.Reset();
        pTextNode->CountWords(aDocStat, 0, pTextNode->Len()); //but word-counting the text should only count the non-deleted text
        CPPUNIT_ASSERT_EQUAL(static_cast<sal_uLong>(1), aDocStat.nWord);

        OUString sLorem = pTextNode->GetText();
        CPPUNIT_ASSERT_EQUAL(u"Lorem"_ustr, sLorem);

        const SwRedlineTable& rTable = m_pDoc->getIDocumentRedlineAccess().GetRedlineTable();

        SwNodes& rNds = m_pDoc->GetNodes();
        CPPUNIT_ASSERT_EQUAL(static_cast<SwRedlineTable::size_type>(1), rTable.size());

        const SwNodeIndex* pNodeIdx = rTable[0]->GetContentIdx();
        CPPUNIT_ASSERT(pNodeIdx);

        pTextNode = rNds[ pNodeIdx->GetIndex() + 1 ]->GetTextNode();        //first deleted txtnode
        CPPUNIT_ASSERT(pTextNode);

        OUString sIpsum = pTextNode->GetText();
        CPPUNIT_ASSERT_EQUAL(u" ipsum"_ustr, sIpsum);

        aDocStat.Reset();
        pTextNode->CountWords(aDocStat, 0, pTextNode->Len()); //word-counting the text should only count the non-deleted text, and this whole chunk should be ignored
        CPPUNIT_ASSERT_EQUAL(static_cast<sal_uLong>(0), aDocStat.nWord);
        CPPUNIT_ASSERT_EQUAL(static_cast<sal_uLong>(0), aDocStat.nChar);

        // https://bugs.libreoffice.org/show_bug.cgi?id=68347 we do want to count
        // redline *added* text though
        m_pDoc->getIDocumentRedlineAccess().SetRedlineFlags(RedlineFlags::On | RedlineFlags::ShowDelete|RedlineFlags::ShowInsert);
        aPaM.DeleteMark();
        aPaM.GetPoint()->nContent.Assign(aPaM.GetPointContentNode(), 0);
        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, u"redline-new-text "_ustr);
        aDocStat.Reset();
        pTextNode = aPaM.GetPointNode().GetTextNode();
        pTextNode->SetWordCountDirty(true);
        pTextNode->CountWords(aDocStat, 0, pTextNode->Len());
        CPPUNIT_ASSERT_EQUAL(static_cast<sal_uLong>(2), aDocStat.nWord);
        //redline-new-text Lorem ipsum
        //+++++++++++++++++     ------
        //select start of original text and part of deleted text
        aDocStat.Reset();
        pTextNode->CountWords(aDocStat, 17, 25);
        CPPUNIT_ASSERT_EQUAL(static_cast<sal_uLong>(5), aDocStat.nChar);
    }

    //See https://bugs.libreoffice.org/show_bug.cgi?id=38983
    {
        SwDocStat aDocStat;

        OUString sTemplate(u"ThisXis a test."_ustr);

        m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());
        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, sTemplate.replace('X', ' '));
        pTextNode = aPaM.GetPointNode().GetTextNode();
        pTextNode->CountWords(aDocStat, 0, pTextNode->Len());
        CPPUNIT_ASSERT_EQUAL(sal_uLong(4), aDocStat.nWord);
        CPPUNIT_ASSERT_EQUAL(sal_uLong(12), aDocStat.nCharExcludingSpaces);
        CPPUNIT_ASSERT_EQUAL(sal_uLong(15), aDocStat.nChar);
        aDocStat.Reset();

        m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());
        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, sTemplate.replaceAll("X", " = "));
        pTextNode = aPaM.GetPointNode().GetTextNode();
        pTextNode->CountWords(aDocStat, 0, pTextNode->Len());
        CPPUNIT_ASSERT_EQUAL(sal_uLong(5), aDocStat.nWord);
        CPPUNIT_ASSERT_EQUAL(sal_uLong(13), aDocStat.nCharExcludingSpaces);
        CPPUNIT_ASSERT_EQUAL(sal_uLong(17), aDocStat.nChar);
        aDocStat.Reset();

        m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());
        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, sTemplate.replaceAll("X", " _ "));
        pTextNode = aPaM.GetPointNode().GetTextNode();
        pTextNode->CountWords(aDocStat, 0, pTextNode->Len());
        CPPUNIT_ASSERT_EQUAL(sal_uLong(5), aDocStat.nWord);
        CPPUNIT_ASSERT_EQUAL(sal_uLong(13), aDocStat.nCharExcludingSpaces);
        CPPUNIT_ASSERT_EQUAL(sal_uLong(17), aDocStat.nChar);
        aDocStat.Reset();

        m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());
        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, sTemplate.replaceAll("X", " -- "));
        pTextNode = aPaM.GetPointNode().GetTextNode();
        pTextNode->CountWords(aDocStat, 0, pTextNode->Len());
        CPPUNIT_ASSERT_EQUAL(sal_uLong(5), aDocStat.nWord);
        CPPUNIT_ASSERT_EQUAL(sal_uLong(14), aDocStat.nCharExcludingSpaces);
        CPPUNIT_ASSERT_EQUAL(sal_uLong(18), aDocStat.nChar);
        aDocStat.Reset();

        m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());
        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, sTemplate.replace('X', '_'));
        pTextNode = aPaM.GetPointNode().GetTextNode();
        pTextNode->CountWords(aDocStat, 0, pTextNode->Len());
        CPPUNIT_ASSERT_EQUAL(sal_uLong(3), aDocStat.nWord);
        CPPUNIT_ASSERT_EQUAL(sal_uLong(13), aDocStat.nCharExcludingSpaces);
        CPPUNIT_ASSERT_EQUAL(sal_uLong(15), aDocStat.nChar);
        aDocStat.Reset();

        m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());
        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, sTemplate.replace('X', '-'));
        pTextNode = aPaM.GetPointNode().GetTextNode();
        pTextNode->CountWords(aDocStat, 0, pTextNode->Len());
        CPPUNIT_ASSERT_EQUAL(sal_uLong(3), aDocStat.nWord);
        CPPUNIT_ASSERT_EQUAL(sal_uLong(13), aDocStat.nCharExcludingSpaces);
        CPPUNIT_ASSERT_EQUAL(sal_uLong(15), aDocStat.nChar);
        aDocStat.Reset();

        m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());
        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, sTemplate.replace('X', 0x2012));
        pTextNode = aPaM.GetPointNode().GetTextNode();
        pTextNode->CountWords(aDocStat, 0, pTextNode->Len());
        CPPUNIT_ASSERT_EQUAL(sal_uLong(3), aDocStat.nWord);
        CPPUNIT_ASSERT_EQUAL(sal_uLong(13), aDocStat.nCharExcludingSpaces);
        CPPUNIT_ASSERT_EQUAL(sal_uLong(15), aDocStat.nChar);
        aDocStat.Reset();

        m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());
        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, sTemplate.replace('X', 0x2015));
        pTextNode = aPaM.GetPointNode().GetTextNode();
        pTextNode->CountWords(aDocStat, 0, pTextNode->Len());
        CPPUNIT_ASSERT_EQUAL(sal_uLong(3), aDocStat.nWord);
        CPPUNIT_ASSERT_EQUAL(sal_uLong(13), aDocStat.nCharExcludingSpaces);
        CPPUNIT_ASSERT_EQUAL(sal_uLong(15), aDocStat.nChar);
        aDocStat.Reset();

        //But default configuration should, msword-alike treat emdash
        //and endash as word separators for word-counting
        m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());
        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, sTemplate.replace('X', 0x2013));
        pTextNode = aPaM.GetPointNode().GetTextNode();
        pTextNode->CountWords(aDocStat, 0, pTextNode->Len());
        CPPUNIT_ASSERT_EQUAL(sal_uLong(4), aDocStat.nWord);
        CPPUNIT_ASSERT_EQUAL(sal_uLong(13), aDocStat.nCharExcludingSpaces);
        CPPUNIT_ASSERT_EQUAL(sal_uLong(15), aDocStat.nChar);
        aDocStat.Reset();

        m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());
        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, sTemplate.replace('X', 0x2014));
        pTextNode = aPaM.GetPointNode().GetTextNode();
        pTextNode->CountWords(aDocStat, 0, pTextNode->Len());
        CPPUNIT_ASSERT_EQUAL(sal_uLong(4), aDocStat.nWord);
        CPPUNIT_ASSERT_EQUAL(sal_uLong(13), aDocStat.nCharExcludingSpaces);
        CPPUNIT_ASSERT_EQUAL(sal_uLong(15), aDocStat.nChar);
        aDocStat.Reset();

        static constexpr OUStringLiteral sChunk = u" \u2013 ";
        m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());
        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, sTemplate.replaceAll("X", sChunk));
        pTextNode = aPaM.GetPointNode().GetTextNode();
        pTextNode->CountWords(aDocStat, 0, pTextNode->Len());
        CPPUNIT_ASSERT_EQUAL(sal_uLong(4), aDocStat.nWord);
        CPPUNIT_ASSERT_EQUAL(sal_uLong(13), aDocStat.nCharExcludingSpaces);
        CPPUNIT_ASSERT_EQUAL(sal_uLong(17), aDocStat.nChar);
        aDocStat.Reset();
    }

    // tdf#150621 Korean words should be counted individually, rather than by syllable.
    //
    // Per i#80815, the intention for the word count feature is to emulate the behavior of MS Word.
    {
        auto fnAssertWords = [&](const OUString& aStr, sal_uLong nWords, sal_uLong nAsianWords)
        {
            m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());

            SvxLanguageItem aCJKLangItem(LANGUAGE_KOREAN, RES_CHRATR_CJK_LANGUAGE);
            SvxLanguageItem aWestLangItem(LANGUAGE_ENGLISH_US, RES_CHRATR_LANGUAGE);
            m_pDoc->getIDocumentContentOperations().InsertPoolItem(aPaM, aCJKLangItem);
            m_pDoc->getIDocumentContentOperations().InsertPoolItem(aPaM, aWestLangItem);

            m_pDoc->getIDocumentContentOperations().InsertString(aPaM, aStr);

            SwDocStat aDocStat;
            pTextNode = aPaM.GetPointNode().GetTextNode();
            pTextNode->CountWords(aDocStat, 0, aStr.getLength());
            CPPUNIT_ASSERT_EQUAL_MESSAGE("words", nWords, aDocStat.nWord);
            CPPUNIT_ASSERT_EQUAL_MESSAGE("Asian characters and Korean words", nAsianWords,
                                         aDocStat.nAsianWord);
        };

        // Basic case: Korean words are counted as space-delimited. In particular, grammatical
        // particles are treated as part of the previous word.
        fnAssertWords(u"저는 영화를 봤어요"_ustr, 3, 3);

        // Mixed script: Korean is mostly written in hangul, but hanja are still used in certain
        // situations (e.g. abbreviations in newspaper articles). For Chinese and Japanese, such
        // ideographs would be counted individually as words. In Korean, however, they are treated
        // no differently than hangul characters.
        fnAssertWords(u"尹탄핵"_ustr, 1, 1);
        fnAssertWords(u"尹 탄핵"_ustr, 2, 2);

        // These mixed-script results are anomalous, but reflect the behavior of MSW.
        fnAssertWords(u"불렀다...與"_ustr, 1, 1);
        fnAssertWords(u"불렀다 ...與"_ustr, 2, 1);
        fnAssertWords(u"불렀다 ... 與"_ustr, 3, 2);
    }
}

void SwDocTest::testMergePortionsDeleteNotSorted()
{
    SwNodeIndex aIdx(m_pDoc->GetNodes().GetEndOfContent(), -1);
    SwPaM aPaM(aIdx);
    m_pDoc->getIDocumentContentOperations().InsertString(aPaM, u"  AABBCC"_ustr);

    SwCharFormat *const pCharFormat(m_pDoc->MakeCharFormat(UIName(u"foo"_ustr), nullptr));
    SwFormatCharFormat const charFormat(pCharFormat);

    SwFormatINetFormat const inetFormat(u"http://example.com"_ustr, u""_ustr);

    IDocumentContentOperations & rIDCO(m_pDoc->getIDocumentContentOperations());
    aPaM.SetMark();
    aPaM.GetPoint()->nContent = 2;
    aPaM.GetMark()->nContent = 4;
    rIDCO.InsertPoolItem(aPaM, charFormat);
    aPaM.GetPoint()->nContent = 2;
    aPaM.GetMark()->nContent = 5;
    rIDCO.InsertPoolItem(aPaM, inetFormat);
    aPaM.GetPoint()->nContent = 6;
    aPaM.GetMark()->nContent = 8;
    rIDCO.InsertPoolItem(aPaM, charFormat);
    aPaM.GetPoint()->nContent = 4;
    aPaM.GetMark()->nContent = 6;
    // this triggered an STL assert in SwpHints::MergePortions()
    rIDCO.InsertPoolItem(aPaM, charFormat);
}

//See https://bugs.libreoffice.org/show_bug.cgi?id=40599
void SwDocTest::testGraphicAnchorDeletion()
{
    CPPUNIT_ASSERT_EQUAL_MESSAGE("Expected initial 0 count", static_cast<sal_uLong>(0), m_pDoc->getIDocumentStatistics().GetDocStat().nChar);

    SwNodeIndex aIdx(m_pDoc->GetNodes().GetEndOfContent(), -1);
    SwPaM aPaM(aIdx);

    m_pDoc->getIDocumentContentOperations().InsertString(aPaM, u"Paragraph 1"_ustr);
    m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());

    m_pDoc->getIDocumentContentOperations().InsertString(aPaM, u"graphic anchor>><<graphic anchor"_ustr);
    SwNodeIndex nPara2(aPaM.GetPoint()->GetNode());
    m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());

    m_pDoc->getIDocumentContentOperations().InsertString(aPaM, u"Paragraph 3"_ustr);

    aPaM.GetPoint()->Assign(nPara2);
    aPaM.GetPoint()->SetContent(RTL_CONSTASCII_LENGTH("graphic anchor>>"));

    //Insert a graphic at X of >>X<< in paragraph 2
    SfxItemSet aFlySet(m_pDoc->GetAttrPool(), svl::Items<RES_FRMATR_BEGIN, RES_FRMATR_END-1>);
    SwFormatAnchor aAnchor(RndStdIds::FLY_AS_CHAR);
    aAnchor.SetAnchor(aPaM.GetPoint());
    aFlySet.Put(aAnchor);
    SwFlyFrameFormat *pFrame = m_pDoc->getIDocumentContentOperations().InsertGraphic(aPaM, OUString(), OUString(), nullptr, &aFlySet, nullptr, nullptr);
    CPPUNIT_ASSERT_MESSAGE("Expected frame", pFrame != nullptr);

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Should be 1 graphic", static_cast<size_t>(1), m_pDoc->GetFlyCount(FLYCNTTYPE_GRF));

    //Delete >X<
    aPaM.GetPoint()->Assign(nPara2);
    aPaM.GetPoint()->SetContent(
        RTL_CONSTASCII_LENGTH("graphic anchor>><")+1);
    aPaM.SetMark();
    aPaM.GetPoint()->Assign(nPara2);
    aPaM.GetPoint()->SetContent(RTL_CONSTASCII_LENGTH("graphic anchor>"));
    m_pDoc->getIDocumentContentOperations().DeleteRange(aPaM);

#ifdef DEBUG_AS_HTML
    {
        SvFileStream aPasteDebug(OUString("cppunitDEBUG.html"), StreamMode::WRITE|StreamMode::TRUNC);
        WriterRef xWrt;
        GetHTMLWriter( String(), String(), xWrt );
        SwWriter aDbgWrt( aPasteDebug, *m_pDoc );
        aDbgWrt.Write( xWrt );
    }
#endif

    CPPUNIT_ASSERT_EQUAL_MESSAGE("Should be 0 graphics", static_cast<size_t>(0), m_pDoc->GetFlyCount(FLYCNTTYPE_GRF));

    //Now, if instead we swap RndStdIds::FLY_AS_CHAR (inline graphic) to RndStdIds::FLY_AT_CHAR (anchored to character)
    //and repeat the above, graphic is *not* deleted, i.e. it belongs to the paragraph, not the
    //range to which its anchored, which is annoying.
}

void SwDocTest::testTableAutoFormats()
{
    SwGlobals::ensure();

    //create new AutoFormatTable
    SwTableAutoFormatTable aTableAFT;

    //check the style size - default is expected
    CPPUNIT_ASSERT_EQUAL( size_t(1),  aTableAFT.size() );

    //create new style
    SwTableAutoFormat aTableAF( TableStyleName(u"TestItemStyle"_ustr) );

    //create new AutoFormat
    SwBoxAutoFormat aBoxAF;
    SwAutoFormatProps& rBoxFP = aBoxAF.GetProps();

    //SetFont
    SvxFontItem aFont( RES_CHRATR_FONT );
    aFont.SetFamily( FontFamily::FAMILY_DECORATIVE );
    aFont.SetPitch( FontPitch::PITCH_VARIABLE );
    aFont.SetCharSet( RTL_TEXTENCODING_MS_1251 );
    rBoxFP.SetFont( aFont );
    //SetHeight
    SvxFontHeightItem aHeight( 280, 120, RES_CHRATR_FONTSIZE );
    rBoxFP.SetHeight( aHeight );
    //SetWeight
    SvxWeightItem aWeight( FontWeight::WEIGHT_BOLD, RES_CHRATR_WEIGHT );
    rBoxFP.SetWeight( aWeight );
    //SetPosture
    SvxPostureItem aPosture( FontItalic::ITALIC_NORMAL, RES_CHRATR_POSTURE );
    rBoxFP.SetPosture( aPosture );
    //SetCJKFont
    SvxFontItem aCJKFont( RES_CHRATR_FONT );
    aCJKFont.SetFamily( FontFamily::FAMILY_MODERN );
    aCJKFont.SetPitch( FontPitch::PITCH_FIXED );
    aCJKFont.SetCharSet( RTL_TEXTENCODING_MS_1251 );
    rBoxFP.SetCJKFont( aCJKFont );
    //SetCJKHeight
    SvxFontHeightItem aCJKHeight( 230, 110, RES_CHRATR_FONTSIZE );
    rBoxFP.SetCJKHeight( aCJKHeight );
    //SetCJKWeight
    SvxWeightItem aCJKWeight( FontWeight::WEIGHT_SEMIBOLD, RES_CHRATR_WEIGHT );
    rBoxFP.SetCJKWeight( aCJKWeight );
    //SetCJKPosture
    SvxPostureItem aCJKPosture( FontItalic::ITALIC_OBLIQUE, RES_CHRATR_POSTURE );
    rBoxFP.SetCJKPosture( aCJKPosture );
    //SetCTLFont
    SvxFontItem aCTLFont( RES_CHRATR_FONT );
    aCTLFont.SetFamily( FontFamily::FAMILY_ROMAN );
    aCTLFont.SetPitch( FontPitch::PITCH_FIXED );
    aCTLFont.SetCharSet( RTL_TEXTENCODING_MS_1251 );
    rBoxFP.SetCTLFont( aCTLFont );
    //SetCTLHeight
    SvxFontHeightItem aCTLHeight( 215, 105, RES_CHRATR_FONTSIZE );
    rBoxFP.SetCTLHeight( aCTLHeight );
    //SetCTLWeight
    SvxWeightItem aCTLWeight( FontWeight::WEIGHT_ULTRABOLD, RES_CHRATR_WEIGHT );
    rBoxFP.SetCTLWeight( aCTLWeight );
    //SetCTLPosture
    SvxPostureItem aCTLPosture( FontItalic::ITALIC_OBLIQUE, RES_CHRATR_POSTURE );
    rBoxFP.SetCTLPosture( aCTLPosture );
    //SetUnderline
    SvxUnderlineItem aUnderline( FontLineStyle::LINESTYLE_DOTTED, RES_CHRATR_UNDERLINE );
    rBoxFP.SetUnderline( aUnderline );
    //SetOverline
    SvxOverlineItem aOverline( FontLineStyle::LINESTYLE_DASH, RES_CHRATR_OVERLINE );
    rBoxFP.SetOverline( aOverline );
    //SetCrossedOut
    SvxCrossedOutItem aCrossedOut( FontStrikeout::STRIKEOUT_BOLD, RES_CHRATR_CROSSEDOUT );
    rBoxFP.SetCrossedOut( aCrossedOut );
    //SetContour
    SvxContourItem aContour( true, RES_CHRATR_CONTOUR );
    rBoxFP.SetContour( aContour );
    //SetShadowed
    SvxShadowedItem aShadowed( false, RES_CHRATR_SHADOWED );
    rBoxFP.SetShadowed( aShadowed );
    //SetColor
    SvxColorItem aColor( Color(0xFF23FF), RES_CHRATR_COLOR );
    rBoxFP.SetColor( aColor );
    //SetAdjust
    SvxAdjustItem aAdjust( SvxAdjust::Center, RES_PARATR_ADJUST );
    rBoxFP.SetAdjust( aAdjust );
    //SetTextOrientation
    SvxFrameDirectionItem aTOrientation( SvxFrameDirection::Vertical_RL_TB, RES_FRAMEDIR );
    rBoxFP.SetTextOrientation( aTOrientation );
    //SetVerticalAlignment
    SwFormatVertOrient aVAlignment( 3, css::text::VertOrientation::CENTER, css::text::RelOrientation::PAGE_LEFT );
    rBoxFP.SetVerticalAlignment( aVAlignment );
    //SetBox
    SvxBoxItem aBox( RES_BOX );
    aBox.SetAllDistances( 5 );
    rBoxFP.SetBox( aBox );
    //SetBackground
    SvxBrushItem aBackground( Color(0xFF11FF), RES_BACKGROUND );
    rBoxFP.SetBackground( aBackground );
    //Set m_aTLBR
    SvxLineItem aTLBRLine(0); aTLBRLine.ScaleMetrics( 11,12 );
    rBoxFP.SetTLBR(aTLBRLine);
    //Set m_aBLTR
    SvxLineItem aBLTRLine(0); aBLTRLine.ScaleMetrics( 13,14 );
    rBoxFP.SetBLTR(aBLTRLine);
    //Set m_aHorJustify
    SvxHorJustifyItem aHJustify( SvxCellHorJustify::Center, 0 );
    rBoxFP.SetHorJustify(aHJustify);
    //Set m_aVerJustify
    SvxVerJustifyItem aVJustify( SvxCellVerJustify::Center , 0 );
    rBoxFP.SetVerJustify(aVJustify);
    //Set m_aStacked
    SfxBoolItem aStacked(0, true);
    rBoxFP.SetStacked(aStacked);
    //Set m_aMargin
    SvxMarginItem aSvxMarginItem(sal_Int16(4), sal_Int16(2), sal_Int16(3), sal_Int16(3), TypedWhichId<SvxMarginItem>(0));
    rBoxFP.SetMargin(aSvxMarginItem);
    //Set m_aLinebreak
    SfxBoolItem aLBreak(0, true);
    rBoxFP.SetLinebreak(aLBreak);
    //Set m_aRotateAngle
    SfxInt32Item aRAngle(sal_Int32(5));
    rBoxFP.SetRotateAngle(aRAngle);
    //Set m_aRotateMode
    SvxRotateModeItem aSvxRotateModeItem(SVX_ROTATE_MODE_CENTER, TypedWhichId<SvxRotateModeItem>(0));
    rBoxFP.SetRotateMode(aSvxRotateModeItem);
    //Set m_sNumFormatString
    OUString aNFString = u"UnitTestFormat"_ustr;
    rBoxFP.SetNumFormatString(aNFString);
    //Set m_eSysLanguage
    LanguageType aSLang( LANGUAGE_ENGLISH_INDIA );
    rBoxFP.SetSysLanguage(aSLang);
    //Set m_eNumFormatLanguage
    LanguageType aNFLang( LANGUAGE_GERMAN );
    rBoxFP.SetNumFormatLanguage(aNFLang);
    //Set m_aKeepWithNextPara
    SvxFormatKeepItem aKWNPara( true, 0 );
    aTableAF.SetKeepWithNextPara(aKWNPara);
    //Set m_aRepeatHeading
    sal_uInt16 aRHeading = 3;
    aTableAF.m_aRepeatHeading = aRHeading;
    //Set m_bLayoutSplit
    bool aLSplit = false;
    aTableAF.m_bLayoutSplit = aLSplit;
    //Set m_bRowSplit
    bool aRSplit = false;
    aTableAF.m_bRowSplit = aRSplit;
    //Set m_bCollapsingBorders
    bool aCBorders = false;
    aTableAF.m_bCollapsingBorders = aCBorders;
    //Set m_aShadow
    SvxShadowItem aShadow( 0, nullptr, 103, SvxShadowLocation::BottomLeft );
    aTableAF.SetShadow(aShadow);
    //Set bInclFont
    bool aIFont = false;
    aTableAF.m_bInclFont = aIFont;
    //Set bInclJustify
    bool aIJustify = false;
    aTableAF.m_bInclJustify = aIJustify;
    //Set bInclFrame
    bool aIFrame = false;
    aTableAF.m_bInclFrame = aIFrame;
    //Set bInclBackground
    bool aIBackground = false;
    aTableAF.m_bInclBackground = aIBackground;
    //Set bInclValueFormat
    bool aIVFormat = false;
    aTableAF.m_bInclValueFormat = aIVFormat;

    //set the box format to AutoFormat
    aTableAF.SetBoxFormat( aBoxAF, sal_uInt8(0) );
    //add AutoFormat to AutoFormatTable
    aTableAFT.AddAutoFormat( aTableAF );

    //check the style size
    CPPUNIT_ASSERT_EQUAL( size_t(2),  aTableAFT.size() );

    //save the bInclFontstyles
    aTableAFT.Save();

    //check the style size after save
    CPPUNIT_ASSERT_EQUAL( size_t(2),  aTableAFT.size() );

    //create new AutoFormatTable
    SwTableAutoFormatTable aLoadTAFT;

    //check the style size after load
    CPPUNIT_ASSERT_EQUAL( size_t(2),  aLoadTAFT.size() );

    //assert the values
    SwTableAutoFormat* pLoadAF = aLoadTAFT.FindAutoFormat( TableStyleName(u"TestItemStyle"_ustr) );
    CPPUNIT_ASSERT( pLoadAF );
    const SwAutoFormatProps& rLoadFP = pLoadAF->GetBoxFormat(0).GetProps();
    //GetFont
    CPPUNIT_ASSERT( bool( rLoadFP.GetFont() == aFont ) );
    //GetHeight
    CPPUNIT_ASSERT( bool( rLoadFP.GetHeight() == aHeight ) );
    //GetWeight
    CPPUNIT_ASSERT( bool( rLoadFP.GetWeight() == aWeight ) );
    //GetPosture
    CPPUNIT_ASSERT( bool( rLoadFP.GetPosture() == aPosture ) );
    //GetCJKFont
    CPPUNIT_ASSERT( bool( rLoadFP.GetCJKFont() == aCJKFont ) );
    //GetCJKHeight
    CPPUNIT_ASSERT( bool( rLoadFP.GetCJKHeight() == aCJKHeight ) );
    //GetCJKWeight
    CPPUNIT_ASSERT( bool( rLoadFP.GetCJKWeight() == aCJKWeight ) );
    //GetCJKPosture
    CPPUNIT_ASSERT( bool( rLoadFP.GetCJKPosture() == aCJKPosture ) );
    //GetCTLFont
    CPPUNIT_ASSERT( bool( rLoadFP.GetCTLFont() == aCTLFont ) );
    //GetCTLHeight
    CPPUNIT_ASSERT( bool( rLoadFP.GetCTLHeight() == aCTLHeight ) );
    //GetCTLWeight
    CPPUNIT_ASSERT( bool( rLoadFP.GetCTLWeight() == aCTLWeight ) );
    //GetCTLPosture
    CPPUNIT_ASSERT( bool( rLoadFP.GetCTLPosture() == aCTLPosture ) );
    //GetUnderline
    CPPUNIT_ASSERT( bool( rLoadFP.GetUnderline() == aUnderline ) );
    //GetOverline
    CPPUNIT_ASSERT( bool( rLoadFP.GetOverline() == aOverline ) );
    //GetCrossedOut
    CPPUNIT_ASSERT( bool( rLoadFP.GetCrossedOut() == aCrossedOut ) );
    //GetContour
    CPPUNIT_ASSERT( bool( rLoadFP.GetContour() == aContour ) );
    //GetShadowed
    CPPUNIT_ASSERT( bool( rLoadFP.GetShadowed() == aShadowed ) );
    //GetColor
    CPPUNIT_ASSERT( bool( rLoadFP.GetColor() == aColor) );
    //GetAdjust
    CPPUNIT_ASSERT( bool( rLoadFP.GetAdjust() == aAdjust ) );
    //GetTextOrientation
    CPPUNIT_ASSERT( bool( rLoadFP.GetTextOrientation() == aTOrientation ) );
    //GetVerticalAlignment
    CPPUNIT_ASSERT (bool( rLoadFP.GetVerticalAlignment() == aVAlignment ) );
    //GetBox
    CPPUNIT_ASSERT( bool( rLoadFP.GetBox() == aBox ) );
    //GetBackground
    CPPUNIT_ASSERT( bool( rLoadFP.GetBackground() == aBackground ) );
    //Get m_aTLBR
    CPPUNIT_ASSERT( bool( rLoadFP.GetTLBR() == aTLBRLine ) );
    //Get m_aBLTR
    CPPUNIT_ASSERT( bool( rLoadFP.GetBLTR() == aBLTRLine ) );
    //Get m_aHorJustify
    CPPUNIT_ASSERT( bool( rLoadFP.GetHorJustify() == aHJustify ) );
    //Get m_aVerJustify
    CPPUNIT_ASSERT( bool( rLoadFP.GetVerJustify() == aVJustify ) );
    //Get m_aStacked
    CPPUNIT_ASSERT( bool( rLoadFP.GetStacked() == aStacked ) );
    //Get m_aMargin
    CPPUNIT_ASSERT( bool( rLoadFP.GetMargin() == aSvxMarginItem ) );
    //Get m_aLinebreak
    CPPUNIT_ASSERT( bool( rLoadFP.GetLinebreak() == aLBreak ) );
    //Get m_aRotateAngle
    CPPUNIT_ASSERT( bool( rLoadFP.GetRotateAngle() == aRAngle ) );
    //Get m_aRotateMode
    //SvxRotateModeItem aRMode = rBoxFP.m_aRotateMode;GetRotateMode
    CPPUNIT_ASSERT( bool( rLoadFP.GetRotateMode() == aSvxRotateModeItem ) );
    //Get m_sNumFormatString
    CPPUNIT_ASSERT( bool( rLoadFP.GetNumFormatString() == aNFString ) );
    //Get m_eSysLanguage
    CPPUNIT_ASSERT( bool( rLoadFP.GetSysLanguage() == aSLang ) );
    //Get m_eNumFormatLanguage
    CPPUNIT_ASSERT( bool( rLoadFP.GetNumFormatLanguage() == aNFLang ) );
    //Get m_aKeepWithNextPara
    CPPUNIT_ASSERT( bool( pLoadAF->GetKeepWithNextPara() == aKWNPara ) );
    //Get m_aRepeatHeading
    CPPUNIT_ASSERT( bool( pLoadAF->m_aRepeatHeading == aRHeading ) );
    //Get m_bLayoutSplit
    CPPUNIT_ASSERT( bool( pLoadAF->m_bLayoutSplit == aLSplit ) );
    //Get m_bRowSplit
    CPPUNIT_ASSERT( bool( pLoadAF->m_bRowSplit == aRSplit ) );
    //Get m_bCollapsingBorders
    CPPUNIT_ASSERT( bool( pLoadAF->m_bCollapsingBorders == aCBorders ) );
    //Get m_aShadow
    CPPUNIT_ASSERT( bool( pLoadAF->GetShadow() == aShadow ) );
    //Get bInclFont
    CPPUNIT_ASSERT( bool( pLoadAF->m_bInclFont == aIFont ) );
    //Get bInclJustify
    CPPUNIT_ASSERT( bool( pLoadAF->m_bInclJustify == aIJustify ) );
    //Get bInclFrame
    CPPUNIT_ASSERT( bool( pLoadAF->m_bInclFrame == aIFrame ) );
    //Get bInclBackground
    CPPUNIT_ASSERT( bool( pLoadAF->m_bInclBackground == aIBackground ) );
    //Get bInclValueFormat
    CPPUNIT_ASSERT( bool( pLoadAF->m_bInclValueFormat == aIVFormat ) );
}

static OUString
translitTest(SwDoc & rDoc, const SwPaM & rPaM, TransliterationFlags const nType)
{
    utl::TransliterationWrapper aTrans(
            ::comphelper::getProcessComponentContext(), nType);
    rDoc.getIDocumentContentOperations().TransliterateText(rPaM, aTrans);
    return rPaM.GetText();
}

void SwDocTest::testTransliterate()
{
    // just some simple test to see if it's totally broken
    SwNodeIndex aIdx(m_pDoc->GetNodes().GetEndOfContent(), -1);
    SwPaM aPaM(aIdx);
    m_pDoc->getIDocumentContentOperations().InsertString(aPaM, u"foobar"_ustr);
    aPaM.SetMark();
    aPaM.GetPoint()->nContent = 0;
    CPPUNIT_ASSERT_EQUAL(u"foobar"_ustr, aPaM.GetText());

    CPPUNIT_ASSERT_EQUAL(u"FOOBAR"_ustr,
            translitTest(*m_pDoc, aPaM,
                TransliterationFlags::LOWERCASE_UPPERCASE));
    CPPUNIT_ASSERT_EQUAL(u"Foobar"_ustr,
            translitTest(*m_pDoc, aPaM,
                TransliterationFlags::TITLE_CASE));
    CPPUNIT_ASSERT_EQUAL(u"fOOBAR"_ustr,
            translitTest(*m_pDoc, aPaM,
                TransliterationFlags::TOGGLE_CASE));
    CPPUNIT_ASSERT_EQUAL(u"foobar"_ustr,
            translitTest(*m_pDoc, aPaM,
                TransliterationFlags::UPPERCASE_LOWERCASE));
    CPPUNIT_ASSERT_EQUAL(u"Foobar"_ustr,
            translitTest(*m_pDoc, aPaM,
                TransliterationFlags::SENTENCE_CASE));
    CPPUNIT_ASSERT_EQUAL(u"Foobar"_ustr,
            translitTest(*m_pDoc, aPaM,
                TransliterationFlags::HIRAGANA_KATAKANA));

    m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());
    m_pDoc->getIDocumentContentOperations().InsertString(aPaM, u"one (two) three"_ustr);
    aPaM.SetMark();
    aPaM.GetMark()->nContent = 0;
    CPPUNIT_ASSERT_EQUAL(u"One (Two) Three"_ustr,
            translitTest(*m_pDoc, aPaM,
                TransliterationFlags::TITLE_CASE));
}

namespace
{
    class SwTableFormulaTest : public SwTableFormula
    {
        SwTableNode *m_pNode;
    public:
        SwTableFormulaTest(const OUString &rStr, SwTableNode *pNode)
            : SwTableFormula(rStr)
            , m_pNode(pNode)
        {
            m_eNmType = INTRNL_NAME;
        }
        virtual const SwNode* GetNodeOfFormula() const override
        {
            return m_pNode;
        }
    };
}

//tdf#66353 Expression is faulty
void SwDocTest::testFormulas()
{
    SwNodeIndex aIdx(m_pDoc->GetNodes().GetEndOfContent(), -1);
    SwPosition aPos(aIdx);

    const SwTable *pTable = m_pDoc->InsertTable(
        SwInsertTableOptions(SwInsertTableFlags::HeadlineNoBorder, 0), aPos, 1, 3, 0);
    SwTableNode* pTableNode = pTable->GetTableNode();
    SwTableFormulaTest aFormula(u"<\x12-1,0>+<Table1.A1>"_ustr, pTableNode);

    aFormula.PtrToBoxNm(pTable);

    CPPUNIT_ASSERT_EQUAL(u"<?>+<Table1.?>"_ustr, aFormula.GetFormula());

    // tdf#61228: Evaluating non-defined function should return an error
    SwCalc aCalc(*m_pDoc);
    SwSbxValue val = aCalc.Calculate(u"foobar()"_ustr);
    CPPUNIT_ASSERT(aCalc.IsCalcError());
    CPPUNIT_ASSERT(val.IsVoidValue());
    CPPUNIT_ASSERT(val.IsDouble());
    CPPUNIT_ASSERT_EQUAL(DBL_MAX, val.GetDouble());
    // Evaluating non-defined variable should return 0 without an error
    val = aCalc.Calculate(u"foobar"_ustr);
    CPPUNIT_ASSERT(!aCalc.IsCalcError());
    CPPUNIT_ASSERT(val.IsVoidValue());
    CPPUNIT_ASSERT(val.IsLong());
    CPPUNIT_ASSERT_EQUAL(sal_Int32(0), val.GetLong());
}

void SwDocTest::testMarkMove()
{
    IDocumentMarkAccess* pMarksAccess = m_pDoc->getIDocumentMarkAccess();

    {
        SwNodeIndex aIdx(m_pDoc->GetNodes().GetEndOfContent(), -1);
        SwPaM aPaM(aIdx);
        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, u"Paragraph 1"_ustr);
        aPaM.SetMark();
        aPaM.GetMark()->nContent -= aPaM.GetMark()->GetContentIndex();
        pMarksAccess->makeMark(aPaM, SwMarkName(u"Para1"_ustr),
            IDocumentMarkAccess::MarkType::BOOKMARK, sw::mark::InsertMode::New);

        m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());
        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, u"Paragraph 2"_ustr);
        aPaM.SetMark();
        aPaM.GetMark()->nContent -= aPaM.GetMark()->GetContentIndex();
        pMarksAccess->makeMark(aPaM, SwMarkName(u"Para2"_ustr),
            IDocumentMarkAccess::MarkType::BOOKMARK, sw::mark::InsertMode::New);

        m_pDoc->getIDocumentContentOperations().AppendTextNode(*aPaM.GetPoint());
        m_pDoc->getIDocumentContentOperations().InsertString(aPaM, u"Paragraph 3"_ustr);
        aPaM.SetMark();
        aPaM.GetMark()->nContent -= aPaM.GetMark()->GetContentIndex();
        pMarksAccess->makeMark(aPaM, SwMarkName(u"Para3"_ustr),
            IDocumentMarkAccess::MarkType::BOOKMARK, sw::mark::InsertMode::New);
    }

    // join paragraph 2 and 3 and check
    {
        SwNodeIndex aIdx(m_pDoc->GetNodes().GetEndOfContent(), -2);
        SwTextNode& rParaNode2 = dynamic_cast<SwTextNode&>(aIdx.GetNode());
        rParaNode2.JoinNext();
    }
    ::sw::mark::MarkBase* pBM1 = *pMarksAccess->findMark(SwMarkName(u"Para1"_ustr));
    ::sw::mark::MarkBase* pBM2 = *pMarksAccess->findMark(SwMarkName(u"Para2"_ustr));
    ::sw::mark::MarkBase* pBM3 = *pMarksAccess->findMark(SwMarkName(u"Para3"_ustr));

    CPPUNIT_ASSERT_EQUAL(sal_Int32(0) , pBM1->GetMarkStart().GetContentIndex());
    CPPUNIT_ASSERT_EQUAL(sal_Int32(11), pBM1->GetMarkEnd().GetContentIndex());
    CPPUNIT_ASSERT_EQUAL(
        pBM1->GetMarkStart().GetNodeIndex(),
        pBM1->GetMarkEnd().GetNodeIndex());

    CPPUNIT_ASSERT_EQUAL(sal_Int32(0) , pBM2->GetMarkStart().GetContentIndex());
    CPPUNIT_ASSERT_EQUAL(sal_Int32(11), pBM2->GetMarkEnd().GetContentIndex());
    CPPUNIT_ASSERT_EQUAL(
        pBM2->GetMarkStart().GetNodeIndex(),
        pBM2->GetMarkEnd().GetNodeIndex());

    CPPUNIT_ASSERT_EQUAL(sal_Int32(11), pBM3->GetMarkStart().GetContentIndex());
    CPPUNIT_ASSERT_EQUAL(sal_Int32(22), pBM3->GetMarkEnd().GetContentIndex());
    CPPUNIT_ASSERT_EQUAL(
        pBM3->GetMarkStart().GetNodeIndex(),
        pBM3->GetMarkEnd().GetNodeIndex());

    CPPUNIT_ASSERT_EQUAL(
        pBM1->GetMarkStart().GetNodeIndex()+1,
        pBM2->GetMarkStart().GetNodeIndex());
    CPPUNIT_ASSERT_EQUAL(
        pBM2->GetMarkStart().GetNodeIndex(),
        pBM3->GetMarkStart().GetNodeIndex());

    // cut some text
    {
        SwNodeIndex aIdx(m_pDoc->GetNodes().GetEndOfContent(), -1);
        SwPaM aPaM(aIdx, aIdx, SwNodeOffset(-1));
        aPaM.GetPoint()->nContent += 5;
        aPaM.GetMark()->nContent += 6;
        m_pDoc->getIDocumentContentOperations().DeleteAndJoin(aPaM);
    }
    pBM1 = *pMarksAccess->findMark(SwMarkName(u"Para1"_ustr));
    pBM2 = *pMarksAccess->findMark(SwMarkName(u"Para2"_ustr));
    pBM3 = *pMarksAccess->findMark(SwMarkName(u"Para3"_ustr));

    CPPUNIT_ASSERT_EQUAL(sal_Int32(0), pBM1->GetMarkStart().GetContentIndex());
    CPPUNIT_ASSERT_EQUAL(sal_Int32(6), pBM1->GetMarkEnd().GetContentIndex());
    CPPUNIT_ASSERT_EQUAL(
        pBM1->GetMarkStart().GetNodeIndex(),
        pBM1->GetMarkEnd().GetNodeIndex());

    CPPUNIT_ASSERT_EQUAL(sal_Int32(6), pBM2->GetMarkStart().GetContentIndex());
    CPPUNIT_ASSERT_EQUAL(sal_Int32(12), pBM2->GetMarkEnd().GetContentIndex());
    CPPUNIT_ASSERT_EQUAL(
        pBM2->GetMarkStart().GetNodeIndex(),
        pBM2->GetMarkEnd().GetNodeIndex());

    CPPUNIT_ASSERT_EQUAL(sal_Int32(12), pBM3->GetMarkStart().GetContentIndex());
    CPPUNIT_ASSERT_EQUAL(sal_Int32(23), pBM3->GetMarkEnd().GetContentIndex());
    CPPUNIT_ASSERT_EQUAL(
        pBM3->GetMarkStart().GetNodeIndex(),
        pBM3->GetMarkEnd().GetNodeIndex());

    CPPUNIT_ASSERT_EQUAL(
        pBM1->GetMarkStart().GetNodeIndex(),
        pBM2->GetMarkStart().GetNodeIndex());
    CPPUNIT_ASSERT_EQUAL(
        pBM2->GetMarkStart().GetNodeIndex(),
        pBM3->GetMarkStart().GetNodeIndex());

    // split the paragraph
    {
        SwNodeIndex aIdx(m_pDoc->GetNodes().GetEndOfContent(), -1);
        SwPosition aPos(aIdx);
        aPos.nContent += 8;
        m_pDoc->getIDocumentContentOperations().SplitNode(aPos, false);
    }
    pBM1 = *pMarksAccess->findMark(SwMarkName(u"Para1"_ustr));
    pBM2 = *pMarksAccess->findMark(SwMarkName(u"Para2"_ustr));
    pBM3 = *pMarksAccess->findMark(SwMarkName(u"Para3"_ustr));

    CPPUNIT_ASSERT_EQUAL(sal_Int32(0), pBM1->GetMarkStart().GetContentIndex());
    CPPUNIT_ASSERT_EQUAL(sal_Int32(6), pBM1->GetMarkEnd().GetContentIndex());
    CPPUNIT_ASSERT_EQUAL(
        pBM1->GetMarkStart().GetNodeIndex(),
        pBM1->GetMarkEnd().GetNodeIndex());

    CPPUNIT_ASSERT_EQUAL(sal_Int32(6), pBM2->GetMarkStart().GetContentIndex());
    CPPUNIT_ASSERT_EQUAL(sal_Int32(4), pBM2->GetMarkEnd().GetContentIndex());
    CPPUNIT_ASSERT_EQUAL(
        pBM2->GetMarkStart().GetNodeIndex()+1,
        pBM2->GetMarkEnd().GetNodeIndex());

    CPPUNIT_ASSERT_EQUAL(sal_Int32(4), pBM3->GetMarkStart().GetContentIndex());
    CPPUNIT_ASSERT_EQUAL(sal_Int32(15), pBM3->GetMarkEnd().GetContentIndex());
    CPPUNIT_ASSERT_EQUAL(
        pBM3->GetMarkStart().GetNodeIndex(),
        pBM3->GetMarkEnd().GetNodeIndex());

    CPPUNIT_ASSERT_EQUAL(
        pBM1->GetMarkStart().GetNodeIndex(),
        pBM2->GetMarkStart().GetNodeIndex());
    CPPUNIT_ASSERT_EQUAL(
        pBM2->GetMarkEnd().GetNodeIndex(),
        pBM3->GetMarkEnd().GetNodeIndex());
}

namespace
{
    struct TestRing : public sw::Ring<TestRing>
    {
        TestRing() : sw::Ring<TestRing>() {};
        TestRing* GetNext()
            { return GetNextInRing(); }
        TestRing* GetPrev()
            { return GetPrevInRing(); }
        bool lonely() const
            { return unique(); }
    };
}

void SwDocTest::testIntrusiveRing()
{
    TestRing aRing1, aRing2, aRing3, aRing4, aRing5;
    std::vector<TestRing*> vRings
    {
        &aRing1,
        &aRing2,
        &aRing3,
        &aRing4,
        &aRing5
    };
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), aRing1.GetRingContainer().size());
    CPPUNIT_ASSERT(aRing1.lonely());
    CPPUNIT_ASSERT(aRing2.lonely());
    CPPUNIT_ASSERT(aRing3.lonely());
    aRing2.MoveTo(&aRing1);
    aRing3.MoveTo(&aRing1);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), aRing1.GetRingContainer().size());
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), aRing2.GetRingContainer().size());
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(3), aRing3.GetRingContainer().size());
    CPPUNIT_ASSERT(!aRing1.lonely());
    CPPUNIT_ASSERT(!aRing2.lonely());
    CPPUNIT_ASSERT(!aRing3.lonely());
    aRing5.MoveTo(&aRing4);
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), aRing4.GetRingContainer().size());
    aRing4.GetRingContainer().merge(aRing1.GetRingContainer());
    for(TestRing* pRing : vRings)
    {
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(5), pRing->GetRingContainer().size());
    }
    for(std::vector<TestRing*>::iterator ppRing = vRings.begin(); ppRing != vRings.end(); ++ppRing)
    {
        std::vector<TestRing*>::iterator ppNext = ppRing+1;
        if(ppNext==vRings.end())
            ppNext = vRings.begin();
        CPPUNIT_ASSERT_EQUAL((*ppRing)->GetNext(), *ppNext);
        CPPUNIT_ASSERT_EQUAL((*ppNext)->GetPrev(), *ppRing);
    }
    for(TestRing& r: aRing1.GetRingContainer())
    {
        TestRing* pRing = &r;
        CPPUNIT_ASSERT(pRing);
    }
    const TestRing* pConstRing = &aRing1;
    for(const TestRing& r: pConstRing->GetRingContainer()) // this should fail without r being const
    {
        const TestRing* pRing = &r;
        CPPUNIT_ASSERT(pRing);
    }
    TestRing foo, bar;
    foo.MoveTo(&bar);
    CPPUNIT_ASSERT_EQUAL(&foo, bar.GetNext());
    CPPUNIT_ASSERT_EQUAL(&foo, bar.GetPrev());
    CPPUNIT_ASSERT_EQUAL(&bar, foo.GetNext());
    CPPUNIT_ASSERT_EQUAL(&bar, foo.GetPrev());
    foo.MoveTo(&foo);
    CPPUNIT_ASSERT_EQUAL(&bar, bar.GetNext());
    CPPUNIT_ASSERT_EQUAL(&bar, bar.GetPrev());
    CPPUNIT_ASSERT_EQUAL(&foo, foo.GetNext());
    CPPUNIT_ASSERT_EQUAL(&foo, foo.GetPrev());
}

namespace
{
    struct TestHint final : SfxHint {};
    struct TestModify : sw::BroadcastingModify
    {
    };
    struct TestClient : SwClient
    {
        int m_nModifyCount;
        int m_nNotifyCount;
        int m_nModifyChangedCount;
        const SwModify* m_pLastChangedModify;
        TestClient() : m_nModifyCount(0), m_nNotifyCount(0), m_nModifyChangedCount(0), m_pLastChangedModify(nullptr) {};
        virtual void SwClientNotify(const SwModify&, const SfxHint& rHint) override
        {
            if(typeid(TestHint) == typeid(rHint))
                ++m_nNotifyCount;
            else if(dynamic_cast<const sw::LegacyModifyHint*>(&rHint))
                ++m_nModifyCount;
            else if(dynamic_cast<const sw::AttrSetChangeHint*>(&rHint))
                ++m_nModifyCount;
            else if(auto pModifyChangedHint = dynamic_cast<const sw::ModifyChangedHint*>(&rHint))
            {
                ++m_nModifyChangedCount;
                m_pLastChangedModify = pModifyChangedHint->m_pNew;
            }
        }
    };
    struct OtherTestClient : SwClient
    {
        int m_nModifyCount;
        OtherTestClient() : m_nModifyCount(0) {};
        virtual void SwClientNotify(const SwModify&, const SfxHint& rHint) override
        {
            if(dynamic_cast<const sw::LegacyModifyHint*>(&rHint))
                ++m_nModifyCount;
            else if(dynamic_cast<const sw::AttrSetChangeHint*>(&rHint))
                ++m_nModifyCount;
        }
    };
    struct TestListener : SvtListener
    {
        int m_nNotifyCount;
        TestListener() : m_nNotifyCount(0) {};
        virtual void Notify( const SfxHint& ) override
        {
            ++m_nNotifyCount;
        }
    };
}
void SwDocTest::testClientModify()
{
    (void) OtherTestClient(); // avoid loplugin:unreffun
    TestModify aMod;
    TestClient aClient1, aClient2;
    OtherTestClient aOtherClient1;
    // test client registration
    CPPUNIT_ASSERT(!aMod.HasWriterListeners());
    CPPUNIT_ASSERT(!aMod.HasOnlyOneListener());
    CPPUNIT_ASSERT_EQUAL(static_cast<SwModify*>(nullptr),aClient1.GetRegisteredIn());
    CPPUNIT_ASSERT_EQUAL(static_cast<SwModify*>(nullptr),aClient2.GetRegisteredIn());
    CPPUNIT_ASSERT_EQUAL(static_cast<SwModify*>(nullptr),aClient2.GetRegisteredIn());
    aMod.Add(aClient1);
    CPPUNIT_ASSERT(aMod.HasWriterListeners());
    CPPUNIT_ASSERT(aMod.HasOnlyOneListener());
    aMod.Add(aClient2);
    CPPUNIT_ASSERT_EQUAL(static_cast<SwModify*>(&aMod),aClient1.GetRegisteredIn());
    CPPUNIT_ASSERT_EQUAL(static_cast<SwModify*>(&aMod), aClient2.GetRegisteredIn());
    CPPUNIT_ASSERT(aMod.HasWriterListeners());
    CPPUNIT_ASSERT(!aMod.HasOnlyOneListener());
    // test broadcast
    aMod.CallSwClientNotify(sw::LegacyModifyHint(nullptr, nullptr));
    CPPUNIT_ASSERT_EQUAL(1,aClient1.m_nModifyCount);
    CPPUNIT_ASSERT_EQUAL(1,aClient2.m_nModifyCount);
    CPPUNIT_ASSERT_EQUAL(0,aClient1.m_nNotifyCount);
    CPPUNIT_ASSERT_EQUAL(0,aClient2.m_nNotifyCount);
    aMod.CallSwClientNotify(sw::LegacyModifyHint(nullptr, nullptr));
    CPPUNIT_ASSERT_EQUAL(2,aClient1.m_nModifyCount);
    CPPUNIT_ASSERT_EQUAL(2,aClient2.m_nModifyCount);
    CPPUNIT_ASSERT_EQUAL(0,aClient1.m_nNotifyCount);
    CPPUNIT_ASSERT_EQUAL(0,aClient2.m_nNotifyCount);
    // test notify
    {
        TestHint aHint;
        aMod.CallSwClientNotify(aHint);
        CPPUNIT_ASSERT_EQUAL(2,aClient1.m_nModifyCount);
        CPPUNIT_ASSERT_EQUAL(2,aClient2.m_nModifyCount);
        CPPUNIT_ASSERT_EQUAL(1,aClient1.m_nNotifyCount);
        CPPUNIT_ASSERT_EQUAL(1,aClient2.m_nNotifyCount);
    }
    // test typed iteration
    CPPUNIT_ASSERT(typeid(aClient1) != typeid(OtherTestClient));
    {
        SwIterator<OtherTestClient,SwModify> aIter(aMod);
        for(OtherTestClient* pClient = aIter.First(); pClient ; pClient = aIter.Next())
            CPPUNIT_ASSERT(false);
    }
    {
        int nCount = 0;
        SwIterator<TestClient,SwModify> aIter(aMod);
        for(TestClient* pClient = aIter.First(); pClient ; pClient = aIter.Next())
        {
            CPPUNIT_ASSERT_EQUAL(2,pClient->m_nModifyCount);
            ++nCount;
        }
        CPPUNIT_ASSERT_EQUAL(2,nCount);
    }
    aMod.Add(aOtherClient1);
    CPPUNIT_ASSERT_EQUAL(0,aOtherClient1.m_nModifyCount);
    {
        int nCount = 0;
        SwIterator<TestClient,SwModify> aIter(aMod);
        for(TestClient* pClient = aIter.First(); pClient ; pClient = aIter.Next())
        {
            CPPUNIT_ASSERT_EQUAL(2,pClient->m_nModifyCount);
            ++nCount;
        }
        CPPUNIT_ASSERT_EQUAL(2,nCount);
    }
    CPPUNIT_ASSERT_EQUAL(0,aOtherClient1.m_nModifyCount);
    aMod.Remove(aOtherClient1);
    CPPUNIT_ASSERT_EQUAL(static_cast<SwModify*>(&aMod),aClient1.GetRegisteredIn());
    CPPUNIT_ASSERT_EQUAL(static_cast<SwModify*>(&aMod),aClient2.GetRegisteredIn());
    CPPUNIT_ASSERT_EQUAL(static_cast<SwModify*>(nullptr),aOtherClient1.GetRegisteredIn());
    // test client self-deregistration during iteration
    {
        int nCount = 0;
        SwIterator<TestClient,SwModify> aIter(aMod);
        for(TestClient* pClient = aIter.First(); pClient ; pClient = aIter.Next())
        {
            aMod.Remove(*pClient);
            ++nCount;
        }
        CPPUNIT_ASSERT_EQUAL(2,nCount);
    }
    CPPUNIT_ASSERT_EQUAL(static_cast<SwModify*>(nullptr),aClient1.GetRegisteredIn());
    CPPUNIT_ASSERT_EQUAL(static_cast<SwModify*>(nullptr),aClient2.GetRegisteredIn());
    {
        SwIterator<TestClient,SwModify> aIter(aMod);
        for(TestClient* pClient = aIter.First(); pClient ; pClient = aIter.Next())
        {
            CPPUNIT_ASSERT(false);
        }
    }
    aMod.CallSwClientNotify(sw::LegacyModifyHint(nullptr, nullptr));
    CPPUNIT_ASSERT_EQUAL(2,aClient1.m_nModifyCount);
    CPPUNIT_ASSERT_EQUAL(2,aClient2.m_nModifyCount);
    CPPUNIT_ASSERT_EQUAL(1,aClient1.m_nNotifyCount);
    CPPUNIT_ASSERT_EQUAL(1,aClient2.m_nNotifyCount);
}
void SwDocTest::testBroadcastingModify()
{
    sw::BroadcastingModify aMod;
    TestClient aClient;
    TestListener aListener;

    aMod.Add(aClient);
    aListener.StartListening(aMod.GetNotifier());

    aMod.CallSwClientNotify(sw::LegacyModifyHint(nullptr, nullptr));
    CPPUNIT_ASSERT_EQUAL(1,aClient.m_nModifyCount);
    CPPUNIT_ASSERT_EQUAL(1,aClient.m_nModifyCount);
    CPPUNIT_ASSERT_EQUAL(1,aListener.m_nNotifyCount);
}
void SwDocTest::testWriterMultiListener()
{
    TestModify aMod;
    TestClient aClient;
    sw::WriterMultiListener aMulti(aClient);
    CPPUNIT_ASSERT(!aMulti.IsListeningTo(&aMod));
    aMulti.StartListening(&aMod);
    CPPUNIT_ASSERT(aMulti.IsListeningTo(&aMod));
    aMulti.EndListeningAll();
    CPPUNIT_ASSERT(!aMulti.IsListeningTo(&aMod));
    aMulti.StartListening(&aMod);
    aMulti.EndListening(&aMod);
    CPPUNIT_ASSERT(!aMulti.IsListeningTo(&aMod));
    int nPreDeathChangedCount;
    {
        TestModify aTempMod;
        aMod.Add(aTempMod);
        aMulti.StartListening(&aTempMod);
        nPreDeathChangedCount = aClient.m_nModifyChangedCount;
    }
    CPPUNIT_ASSERT(aMulti.IsListeningTo(&aMod));
    CPPUNIT_ASSERT_EQUAL(nPreDeathChangedCount+1, aClient.m_nModifyChangedCount);
    CPPUNIT_ASSERT_EQUAL(static_cast<const SwModify*>(&aMod),aClient.m_pLastChangedModify);
}

void SwDocTest::test64kPageDescs()
{
    size_t nPageDescCount = 65536; // USHRT_MAX + 1

    for (size_t i = 0; i < nPageDescCount; ++i)
    {
        OUString aName = "Page" + OUString::number(i);
        m_pDoc->MakePageDesc( UIName(aName) );
    }

    size_t nCount = m_pDoc->GetPageDescCnt();
    // +1 because Writer always creates a dummy page desc
    // in a new SwDoc
    CPPUNIT_ASSERT_EQUAL( nPageDescCount + 1, nCount );

    const SwPageDesc &rDesc = m_pDoc->GetPageDesc( nPageDescCount );
    SwPageDesc &rZeroDesc = m_pDoc->GetPageDesc( 0 );
    CPPUNIT_ASSERT_EQUAL( u"Page65535"_ustr, rDesc.GetName().toString() );

    SwPageDesc aDesc( rDesc );
    static constexpr OUString aChanged(u"Changed01"_ustr);
    aDesc.SetName( UIName(aChanged) );
    m_pDoc->ChgPageDesc( nPageDescCount, aDesc );

    size_t nPos;
    SwPageDesc *pDesc = m_pDoc->FindPageDesc( UIName(aChanged), &nPos );
    CPPUNIT_ASSERT( pDesc != nullptr );
    CPPUNIT_ASSERT_EQUAL( nPageDescCount, nPos );

    // check if we didn't mess up PageDesc at pos 0
    // (happens with 16bit int overflow)
    UIName aZeroName = rZeroDesc.GetName();
    rZeroDesc = m_pDoc->GetPageDesc( 0 );
    CPPUNIT_ASSERT_EQUAL( aZeroName.toString(), rZeroDesc.GetName().toString() );

    m_pDoc->DelPageDesc( UIName(aChanged), /*bBroadcast*/true );
    pDesc = m_pDoc->FindPageDesc( UIName(aChanged), &nPos );
    // not there anymore
    CPPUNIT_ASSERT( !pDesc );
    CPPUNIT_ASSERT_EQUAL( std::numeric_limits<size_t>::max(), nPos);

    // check if PageDesc at pos 0 is still there
    pDesc = m_pDoc->FindPageDesc( aZeroName, &nPos );
    CPPUNIT_ASSERT( pDesc != nullptr );
    CPPUNIT_ASSERT_EQUAL( static_cast<size_t>(0), nPos );
}

void SwDocTest::testTdf92308()
{
    CPPUNIT_ASSERT_EQUAL(false, m_pDoc->HasInvisibleContent());
}

void SwDocTest::testTableCellComparison()
{
    CPPUNIT_ASSERT_EQUAL( -1, sw_CompareCellsByColFirst(u"A1", u"Z1") );
    CPPUNIT_ASSERT_EQUAL( +1, sw_CompareCellsByColFirst(u"Z1", u"A1") );
    CPPUNIT_ASSERT_EQUAL(  0, sw_CompareCellsByColFirst(u"A1", u"A1") );

    CPPUNIT_ASSERT_EQUAL( +1, sw_CompareCellsByColFirst(u"A2", u"A1") );
    CPPUNIT_ASSERT_EQUAL( +1, sw_CompareCellsByColFirst(u"Z3", u"A2") );
    CPPUNIT_ASSERT_EQUAL( -1, sw_CompareCellsByColFirst(u"A3", u"Z1") );

    CPPUNIT_ASSERT_EQUAL( -1, sw_CompareCellsByRowFirst(u"A1", u"Z1") );
    CPPUNIT_ASSERT_EQUAL( +1, sw_CompareCellsByRowFirst(u"Z1", u"A1") );
    CPPUNIT_ASSERT_EQUAL(  0, sw_CompareCellsByRowFirst(u"A1", u"A1") );

    CPPUNIT_ASSERT_EQUAL( +1, sw_CompareCellsByRowFirst(u"A2", u"A1") );
    CPPUNIT_ASSERT_EQUAL( +1, sw_CompareCellsByRowFirst(u"Z3", u"A2") );
    CPPUNIT_ASSERT_EQUAL( +1, sw_CompareCellsByRowFirst(u"A3", u"Z1") );

    CPPUNIT_ASSERT_EQUAL(  0, sw_CompareCellRanges(u"A1", u"A1", u"A1", u"A1", true) );
    CPPUNIT_ASSERT_EQUAL(  0, sw_CompareCellRanges(u"A1", u"Z1", u"A1", u"Z1", true) );
    CPPUNIT_ASSERT_EQUAL(  0, sw_CompareCellRanges(u"A1", u"Z1", u"A1", u"Z1", false) );

    CPPUNIT_ASSERT_EQUAL( -1, sw_CompareCellRanges(u"A1", u"Z1", u"B1", u"Z1", true) );
    CPPUNIT_ASSERT_EQUAL( -1, sw_CompareCellRanges(u"A1", u"Z1", u"A2", u"Z2", false) );
    CPPUNIT_ASSERT_EQUAL( -1, sw_CompareCellRanges(u"A1", u"Z1", u"A2", u"Z2", true) );
    CPPUNIT_ASSERT_EQUAL( -1, sw_CompareCellRanges(u"A1", u"Z1", u"A6", u"Z2", true) );

    CPPUNIT_ASSERT_EQUAL( +1, sw_CompareCellRanges(u"B1", u"Z1", u"A1", u"Z1", true) );
    CPPUNIT_ASSERT_EQUAL( +1, sw_CompareCellRanges(u"A2", u"Z2", u"A1", u"Z1", false) );
    CPPUNIT_ASSERT_EQUAL( +1, sw_CompareCellRanges(u"A2", u"Z2", u"A1", u"Z1", true) );
    CPPUNIT_ASSERT_EQUAL( +1, sw_CompareCellRanges(u"A6", u"Z2", u"A1", u"Z1", true) );

    OUString rCell1(u"A1"_ustr);
    OUString rCell2(u"C5"_ustr);

    sw_NormalizeRange(rCell1, rCell2);
    CPPUNIT_ASSERT_EQUAL( u"A1"_ustr, rCell1 );
    CPPUNIT_ASSERT_EQUAL( u"C5"_ustr, rCell2 );

    sw_NormalizeRange(rCell2, rCell1);
    CPPUNIT_ASSERT_EQUAL( u"C5"_ustr, rCell1 );
    CPPUNIT_ASSERT_EQUAL( u"A1"_ustr, rCell2 );

    rCell1 = u"A5"_ustr;
    rCell2 = u"C1"_ustr;

    sw_NormalizeRange(rCell1, rCell2);
    CPPUNIT_ASSERT_EQUAL( u"A1"_ustr, rCell1 );
    CPPUNIT_ASSERT_EQUAL( u"C5"_ustr, rCell2 );

    sw_NormalizeRange(rCell2, rCell1);
    CPPUNIT_ASSERT_EQUAL( u"C5"_ustr, rCell1 );
    CPPUNIT_ASSERT_EQUAL( u"A1"_ustr, rCell2 );

    CPPUNIT_ASSERT_EQUAL( OUString(), sw_GetCellName(-1, -1) );
}

void SwDocTest::setUp()
{
    BootstrapFixture::setUp();

    SwGlobals::ensure();
    m_pDoc = new SwDoc;
    m_xDocShRef = new SwDocShell(*m_pDoc, SfxObjectCreateMode::EMBEDDED);
    m_xDocShRef->DoInitNew();
}

void SwDocTest::tearDown()
{
    m_pDoc = nullptr; // deleted by DoClose()
    m_xDocShRef->DoClose();
    m_xDocShRef.clear();

    BootstrapFixture::tearDown();
}

void SwDocTest::testFillRubyList()
{
    SwNodeIndex aIdx(m_pDoc->GetNodes().GetEndOfContent(), -1);
    SwPaM aPaM(aIdx);

    SwTextNode* pTextNode = aPaM.GetPointNode().GetTextNode();
    CPPUNIT_ASSERT(pTextNode);

    auto& rOps = m_pDoc->getIDocumentContentOperations();

    auto fnAppendJapanese = [&](const OUString& rText)
    {
        rOps.AppendTextNode(*aPaM.GetPoint());

        SvxLanguageItem aCJKLangItem(LANGUAGE_JAPANESE, RES_CHRATR_CJK_LANGUAGE);
        SvxLanguageItem aWestLangItem(LANGUAGE_ENGLISH_US, RES_CHRATR_LANGUAGE);
        rOps.InsertPoolItem(aPaM, aCJKLangItem);
        rOps.InsertPoolItem(aPaM, aWestLangItem);

        rOps.InsertString(aPaM, rText);

        aPaM.SetMark();
        aPaM.GetPoint()->nContent = 0;

        CPPUNIT_ASSERT_EQUAL(rText, aPaM.GetText());
    };

    auto fnAppendRuby = [](SwRubyList* rList, OUString aBase, OUString aRuby)
    {
        auto pEnt = std::make_unique<SwRubyListEntry>();
        pEnt->SetText(std::move(aBase));
        pEnt->SetRubyAttr(SwFormatRuby{ std::move(aRuby) });

        rList->push_back(std::move(pEnt));
    };

    auto fnGetCombinedString = [](SwPaM& rPaM)
    {
        SwRubyList aRubies;
        SwDoc::FillRubyList(rPaM, aRubies);

        OUStringBuffer aTemp;

        for (auto const& rRuby : aRubies)
        {
            aTemp.append(rRuby->GetText() + u"["_ustr + rRuby->GetRubyAttr().GetText() + u"]"_ustr);
        }

        return aTemp.toString();
    };

    // Single word without existing rubies
    {
        fnAppendJapanese(u"学校"_ustr);
        CPPUNIT_ASSERT_EQUAL(u"学校[]"_ustr, fnGetCombinedString(aPaM));
    }

    // Compound word without existing rubies
    {
        fnAppendJapanese(u"自動販売機"_ustr);
        CPPUNIT_ASSERT_EQUAL(u"自動[]販売[]機[]"_ustr, fnGetCombinedString(aPaM));
    }

    // Single word with existing rubies
    {
        fnAppendJapanese(u"学校"_ustr);

        SwRubyList rList;
        fnAppendRuby(&rList, u"学校"_ustr, u"がっこう"_ustr);

        m_pDoc->SetRubyList(aPaM, rList);

        CPPUNIT_ASSERT_EQUAL(u"学校[がっこう]"_ustr, fnGetCombinedString(aPaM));
    }

    // Compound word with existing rubies
    {
        fnAppendJapanese(u"自動販売機"_ustr);

        SwRubyList rList;
        fnAppendRuby(&rList, u"自動"_ustr, u"じどう"_ustr);
        fnAppendRuby(&rList, u"販売"_ustr, u"はんばい"_ustr);
        fnAppendRuby(&rList, u"機"_ustr, u"き"_ustr);

        m_pDoc->SetRubyList(aPaM, rList);

        CPPUNIT_ASSERT_EQUAL(u"自動[じどう]販売[はんばい]機[き]"_ustr, fnGetCombinedString(aPaM));
    }

    // Compound word with existing rubies treated as a single word
    {
        fnAppendJapanese(u"自動販売機"_ustr);

        SwRubyList rList;
        fnAppendRuby(&rList, u"自動販売機"_ustr, u"じどうはんばいき"_ustr);
        fnAppendRuby(&rList, u""_ustr, u""_ustr);
        fnAppendRuby(&rList, u""_ustr, u""_ustr);

        m_pDoc->SetRubyList(aPaM, rList);

        CPPUNIT_ASSERT_EQUAL(u"自動販売機[じどうはんばいき]"_ustr, fnGetCombinedString(aPaM));
    }

    // tdf#141466: Characteristic test from bug
    {
        fnAppendJapanese(u"学校に行きます。"_ustr);

        SwRubyList rList;
        fnAppendRuby(&rList, u"学校"_ustr, u"がっこう"_ustr);
        fnAppendRuby(&rList, u"に"_ustr, u""_ustr);
        fnAppendRuby(&rList, u"行"_ustr, u"い"_ustr);
        fnAppendRuby(&rList, u"きます"_ustr, u""_ustr);
        fnAppendRuby(&rList, u"。"_ustr, u""_ustr);

        m_pDoc->SetRubyList(aPaM, rList);

        CPPUNIT_ASSERT_EQUAL(u"学校[がっこう]に[]行[い]き[]ます[]。[]"_ustr,
                             fnGetCombinedString(aPaM));
    }

    // tdf#107184: Characteristic test for ruby group mode editing
    {
        fnAppendJapanese(u"学校に行きます"_ustr);

        SwRubyList rList;
        fnAppendRuby(&rList, u"学校に行きます"_ustr, u"がっこうにいきます"_ustr);

        m_pDoc->SetRubyList(aPaM, rList);

        CPPUNIT_ASSERT_EQUAL(u"学校に行きます[がっこうにいきます]"_ustr, fnGetCombinedString(aPaM));
    }

    // tdf#156543: Characteristic test for ruby mono mode editing
    {
        fnAppendJapanese(u"学校に行きます"_ustr);

        SwRubyList rList;
        fnAppendRuby(&rList, u"学"_ustr, u"がっ"_ustr);
        fnAppendRuby(&rList, u"校"_ustr, u"こう"_ustr);
        fnAppendRuby(&rList, u"に"_ustr, u""_ustr);
        fnAppendRuby(&rList, u"行"_ustr, u"い"_ustr);
        fnAppendRuby(&rList, u"き"_ustr, u""_ustr);
        fnAppendRuby(&rList, u"ま"_ustr, u""_ustr);
        fnAppendRuby(&rList, u"す"_ustr, u""_ustr);

        m_pDoc->SetRubyList(aPaM, rList);

        CPPUNIT_ASSERT_EQUAL(u"学[がっ]校[こう]に[]行[い]き[]ます[]"_ustr,
                             fnGetCombinedString(aPaM));
    }

    // tdf#156543: Characteristic test for ruby mono mode editing
    {
        fnAppendJapanese(u"学校に行きます"_ustr);

        SwRubyList rList;
        fnAppendRuby(&rList, u"学"_ustr, u"がっ"_ustr);
        fnAppendRuby(&rList, u"校"_ustr, u"こう"_ustr);
        fnAppendRuby(&rList, u"に"_ustr, u""_ustr);
        fnAppendRuby(&rList, u"行"_ustr, u"い"_ustr);
        fnAppendRuby(&rList, u"き"_ustr, u""_ustr);
        fnAppendRuby(&rList, u"ま"_ustr, u""_ustr);
        fnAppendRuby(&rList, u"す"_ustr, u""_ustr);

        m_pDoc->SetRubyList(aPaM, rList);

        CPPUNIT_ASSERT_EQUAL(u"学[がっ]校[こう]に[]行[い]き[]ます[]"_ustr,
                             fnGetCombinedString(aPaM));
    }

    // Partial PaM
    {
        fnAppendJapanese(u"学校に行こう。"_ustr);

        SwPaM aAdjPaM{ *aPaM.GetPoint(), *aPaM.GetMark() };
        aAdjPaM.Normalize();
        aAdjPaM.GetMark()->AdjustContent(-1);

        CPPUNIT_ASSERT_EQUAL(u"学校[]に[]行[]こう[]"_ustr, fnGetCombinedString(aAdjPaM));
    }

    // Empty PaM
    {
        fnAppendJapanese(u"学校"_ustr);

        aPaM.DeleteMark();

        CPPUNIT_ASSERT_EQUAL(u"学校[]"_ustr, fnGetCombinedString(aPaM));
    }
}

void SwDocTest::testSetRubyList()
{
    SwNodeIndex aIdx(m_pDoc->GetNodes().GetEndOfContent(), -1);
    SwPaM aPaM(aIdx);

    SwTextNode* pTextNode = aPaM.GetPointNode().GetTextNode();
    CPPUNIT_ASSERT(pTextNode);

    auto& rOps = m_pDoc->getIDocumentContentOperations();

    auto fnAppendJapanese = [&](const OUString& rText)
    {
        rOps.AppendTextNode(*aPaM.GetPoint());

        SvxLanguageItem aCJKLangItem(LANGUAGE_JAPANESE, RES_CHRATR_CJK_LANGUAGE);
        SvxLanguageItem aWestLangItem(LANGUAGE_ENGLISH_US, RES_CHRATR_LANGUAGE);
        rOps.InsertPoolItem(aPaM, aCJKLangItem);
        rOps.InsertPoolItem(aPaM, aWestLangItem);

        rOps.InsertString(aPaM, rText);

        aPaM.SetMark();
        aPaM.GetPoint()->nContent = 0;

        CPPUNIT_ASSERT_EQUAL(rText, aPaM.GetText());
    };

    auto fnAppendRuby = [](SwRubyList* rList, OUString aBase, OUString aRuby)
    {
        auto pEnt = std::make_unique<SwRubyListEntry>();
        pEnt->SetText(std::move(aBase));
        pEnt->SetRubyAttr(SwFormatRuby{ std::move(aRuby) });

        rList->push_back(std::move(pEnt));
    };

    auto fnGetCombinedString = [&]
    {
        OUStringBuffer aTemp;

        auto* pPos = aPaM.GetPoint();
        const auto* pTNd = pPos->GetNode().GetTextNode();
        const auto& rText = pTNd->GetText();
        const auto* pHts = pTNd->GetpSwpHints();

        for (sal_Int32 i = 0; i < rText.getLength(); ++i)
        {
            aTemp.append(rText[i]);

            if (pHts)
            {
                for (size_t j = 0; j < pHts->Count(); ++j)
                {
                    const auto* pHt = pHts->Get(j);
                    if (pHt->Which() == RES_TXTATR_CJK_RUBY && pHt->GetAnyEnd() == (i + 1))
                    {
                        aTemp.append(u"["_ustr + pHt->GetRuby().GetText() + u"]"_ustr);
                    }
                }
            }
        }

        return aTemp.toString();
    };

    // Trivial characteristic test
    {
        fnAppendJapanese(u"学校"_ustr);

        SwRubyList rList;
        fnAppendRuby(&rList, u"学校"_ustr, u"がっこう"_ustr);

        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(2), aPaM.GetMark()->GetContentIndex());
        m_pDoc->SetRubyList(aPaM, rList);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(2), aPaM.GetMark()->GetContentIndex());

        CPPUNIT_ASSERT_EQUAL(u"学校[がっこう]"_ustr, fnGetCombinedString());

        // Operation should be idempotent
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(2), aPaM.GetMark()->GetContentIndex());
        m_pDoc->SetRubyList(aPaM, rList);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(2), aPaM.GetMark()->GetContentIndex());

        CPPUNIT_ASSERT_EQUAL(u"学校[がっこう]"_ustr, fnGetCombinedString());
    }

    // tdf#141466: Characteristic test from bug
    {
        fnAppendJapanese(u"学校に行きます。"_ustr);

        SwRubyList rList;
        fnAppendRuby(&rList, u"学校"_ustr, u"がっこう"_ustr);
        fnAppendRuby(&rList, u"に"_ustr, u""_ustr);
        fnAppendRuby(&rList, u"行"_ustr, u"い"_ustr);
        fnAppendRuby(&rList, u"きます"_ustr, u""_ustr);
        fnAppendRuby(&rList, u"。"_ustr, u""_ustr);

        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(8), aPaM.GetMark()->GetContentIndex());
        m_pDoc->SetRubyList(aPaM, rList);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(8), aPaM.GetMark()->GetContentIndex());

        CPPUNIT_ASSERT_EQUAL(u"学校[がっこう]に行[い]きます。"_ustr, fnGetCombinedString());

        // Operation should be idempotent
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(8), aPaM.GetMark()->GetContentIndex());
        m_pDoc->SetRubyList(aPaM, rList);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(8), aPaM.GetMark()->GetContentIndex());

        CPPUNIT_ASSERT_EQUAL(u"学校[がっこう]に行[い]きます。"_ustr, fnGetCombinedString());
    }

    // Base text merging/deletion at end of selection
    {
        fnAppendJapanese(u"学校に行こう。"_ustr);

        SwPaM aAdjPaM{ *aPaM.GetPoint(), *aPaM.GetMark() };
        aAdjPaM.GetPoint()->AdjustContent(-1);

        SwRubyList rList;
        fnAppendRuby(&rList, u"学校"_ustr, u"がっこう"_ustr);
        fnAppendRuby(&rList, u"に"_ustr, u""_ustr);
        fnAppendRuby(&rList, u"行こう"_ustr, u"いこう"_ustr);
        fnAppendRuby(&rList, u""_ustr, u""_ustr);
        fnAppendRuby(&rList, u""_ustr, u"this ruby should not appear"_ustr);
        fnAppendRuby(&rList, u""_ustr, u""_ustr);

        CPPUNIT_ASSERT_EQUAL(sal_Int32(6), aAdjPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aAdjPaM.GetMark()->GetContentIndex());
        m_pDoc->SetRubyList(aAdjPaM, rList);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aAdjPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(6), aAdjPaM.GetMark()->GetContentIndex());

        CPPUNIT_ASSERT_EQUAL(u"学校[がっこう]に行こう[いこう]。"_ustr, fnGetCombinedString());

        // Operation should be idempotent
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aAdjPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(6), aAdjPaM.GetMark()->GetContentIndex());
        m_pDoc->SetRubyList(aAdjPaM, rList);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aAdjPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(6), aAdjPaM.GetMark()->GetContentIndex());

        CPPUNIT_ASSERT_EQUAL(u"学校[がっこう]に行こう[いこう]。"_ustr, fnGetCombinedString());
    }

    // Base text deletion
    {
        fnAppendJapanese(u"学校に行きます。"_ustr);

        SwRubyList rList;
        fnAppendRuby(&rList, u"学校"_ustr, u"がっこう"_ustr);
        fnAppendRuby(&rList, u"に"_ustr, u""_ustr);
        fnAppendRuby(&rList, u"行"_ustr, u"い"_ustr);
        fnAppendRuby(&rList, u"きます"_ustr, u""_ustr);
        fnAppendRuby(&rList, u"。"_ustr, u""_ustr);

        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(8), aPaM.GetMark()->GetContentIndex());
        m_pDoc->SetRubyList(aPaM, rList);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(8), aPaM.GetMark()->GetContentIndex());

        CPPUNIT_ASSERT_EQUAL(u"学校[がっこう]に行[い]きます。"_ustr, fnGetCombinedString());

        SwRubyList rList2;
        fnAppendRuby(&rList2, u"学校"_ustr, u"がっこう"_ustr);
        fnAppendRuby(&rList2, u"に"_ustr, u""_ustr);
        fnAppendRuby(&rList2, u""_ustr, u"い"_ustr);
        fnAppendRuby(&rList2, u"来"_ustr, u"き"_ustr);
        fnAppendRuby(&rList2, u"ます"_ustr, u""_ustr);
        fnAppendRuby(&rList2, u"。"_ustr, u""_ustr);

        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(8), aPaM.GetMark()->GetContentIndex());
        m_pDoc->SetRubyList(aPaM, rList2);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aPaM.GetMark()->GetContentIndex());

        CPPUNIT_ASSERT_EQUAL(u"学校[がっこう]に来[き]ます。"_ustr, fnGetCombinedString());

        // Operation should be idempotent
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aPaM.GetMark()->GetContentIndex());
        m_pDoc->SetRubyList(aPaM, rList2);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aPaM.GetMark()->GetContentIndex());

        CPPUNIT_ASSERT_EQUAL(u"学校[がっこう]に来[き]ます。"_ustr, fnGetCombinedString());
    }

    // tdf#107184: Characteristic test for ruby group mode editing
    {
        fnAppendJapanese(u"学校に行きます"_ustr);

        SwRubyList rList;
        fnAppendRuby(&rList, u"学校に行きます"_ustr, u"がっこうにいきます"_ustr);

        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aPaM.GetMark()->GetContentIndex());
        m_pDoc->SetRubyList(aPaM, rList);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aPaM.GetMark()->GetContentIndex());

        CPPUNIT_ASSERT_EQUAL(u"学校に行きます[がっこうにいきます]"_ustr, fnGetCombinedString());

        // Operation should be idempotent
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aPaM.GetMark()->GetContentIndex());
        m_pDoc->SetRubyList(aPaM, rList);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aPaM.GetMark()->GetContentIndex());

        CPPUNIT_ASSERT_EQUAL(u"学校に行きます[がっこうにいきます]"_ustr, fnGetCombinedString());
    }

    // tdf#107184: Delete ruby in group mode after populating
    {
        fnAppendJapanese(u"学校に行きます"_ustr);

        SwRubyList rList;
        fnAppendRuby(&rList, u"学校に行きます"_ustr, u"がっこうにいきます"_ustr);

        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aPaM.GetMark()->GetContentIndex());
        m_pDoc->SetRubyList(aPaM, rList);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aPaM.GetMark()->GetContentIndex());

        CPPUNIT_ASSERT_EQUAL(u"学校に行きます[がっこうにいきます]"_ustr, fnGetCombinedString());

        SwRubyList rList2;
        fnAppendRuby(&rList2, u"学校に行きます"_ustr, u""_ustr);

        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aPaM.GetMark()->GetContentIndex());
        m_pDoc->SetRubyList(aPaM, rList2);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aPaM.GetMark()->GetContentIndex());

        CPPUNIT_ASSERT_EQUAL(u"学校に行きます"_ustr, fnGetCombinedString());

        // Operation should be idempotent
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aPaM.GetMark()->GetContentIndex());
        m_pDoc->SetRubyList(aPaM, rList2);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aPaM.GetMark()->GetContentIndex());

        CPPUNIT_ASSERT_EQUAL(u"学校に行きます"_ustr, fnGetCombinedString());
    }

    // tdf#156543: Characteristic test for ruby mono mode editing
    {
        fnAppendJapanese(u"学校に行きます"_ustr);

        SwRubyList rList;
        fnAppendRuby(&rList, u"学"_ustr, u"がっ"_ustr);
        fnAppendRuby(&rList, u"校"_ustr, u"こう"_ustr);
        fnAppendRuby(&rList, u"に"_ustr, u""_ustr);
        fnAppendRuby(&rList, u"行"_ustr, u"い"_ustr);
        fnAppendRuby(&rList, u"き"_ustr, u""_ustr);
        fnAppendRuby(&rList, u"ま"_ustr, u""_ustr);
        fnAppendRuby(&rList, u"す"_ustr, u"す"_ustr);

        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aPaM.GetMark()->GetContentIndex());
        m_pDoc->SetRubyList(aPaM, rList);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aPaM.GetMark()->GetContentIndex());

        CPPUNIT_ASSERT_EQUAL(u"学[がっ]校[こう]に行[い]きます[す]"_ustr, fnGetCombinedString());

        // Operation should be idempotent
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aPaM.GetMark()->GetContentIndex());
        m_pDoc->SetRubyList(aPaM, rList);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aPaM.GetMark()->GetContentIndex());

        CPPUNIT_ASSERT_EQUAL(u"学[がっ]校[こう]に行[い]きます[す]"_ustr, fnGetCombinedString());
    }

    // Offset PaM - Combination of insert and replace
    {
        fnAppendJapanese(u"学校員"_ustr);

        SwPosition aNewPos{ aPaM.GetPoint()->nNode, aPaM.GetPoint()->nContent };
        const SwTextNode* pTNd = aNewPos.GetNode().GetTextNode();
        pTNd->GoNext(aNewPos, SwCursorSkipMode::Chars);

        SwPaM aEmptyPaM{ aNewPos };
        aEmptyPaM.SetMark();
        aEmptyPaM.GetMark()->AdjustContent(1);

        SwRubyList rList;
        fnAppendRuby(&rList, u"森林"_ustr, u"しんりん"_ustr);
        fnAppendRuby(&rList, u"海上"_ustr, u"かいじょう"_ustr);
        fnAppendRuby(&rList, u"地面"_ustr, u"じめん"_ustr);

        CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aEmptyPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(2), aEmptyPaM.GetMark()->GetContentIndex());
        m_pDoc->SetRubyList(aEmptyPaM, rList);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aEmptyPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aEmptyPaM.GetMark()->GetContentIndex());

        CPPUNIT_ASSERT_EQUAL(u"学森林[しんりん]海上[かいじょう]地面[じめん]員"_ustr,
                             fnGetCombinedString());

        // Operation should be idempotent
        CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aEmptyPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aEmptyPaM.GetMark()->GetContentIndex());
        m_pDoc->SetRubyList(aEmptyPaM, rList);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aEmptyPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aEmptyPaM.GetMark()->GetContentIndex());

        CPPUNIT_ASSERT_EQUAL(u"学森林[しんりん]海上[かいじょう]地面[じめん]員"_ustr,
                             fnGetCombinedString());
    }

    // Partial PaM with mono replacement
    {
        fnAppendJapanese(u"学校に行こう。"_ustr);

        SwPaM aAdjPaM{ *aPaM.GetPoint(), *aPaM.GetMark() };
        aAdjPaM.Normalize();
        aAdjPaM.GetMark()->AdjustContent(-1);

        SwRubyList rList;
        fnAppendRuby(&rList, u"学"_ustr, u"がっ"_ustr);
        fnAppendRuby(&rList, u"校"_ustr, u"こう"_ustr);
        fnAppendRuby(&rList, u"に"_ustr, u""_ustr);
        fnAppendRuby(&rList, u"行"_ustr, u"い"_ustr);
        fnAppendRuby(&rList, u"こ"_ustr, u""_ustr);
        fnAppendRuby(&rList, u"う"_ustr, u""_ustr);

        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aAdjPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(6), aAdjPaM.GetMark()->GetContentIndex());
        m_pDoc->SetRubyList(aAdjPaM, rList);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aAdjPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(6), aAdjPaM.GetMark()->GetContentIndex());

        CPPUNIT_ASSERT_EQUAL(u"学[がっ]校[こう]に行[い]こう。"_ustr, fnGetCombinedString());

        // Operation should be idempotent
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aAdjPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(6), aAdjPaM.GetMark()->GetContentIndex());
        m_pDoc->SetRubyList(aAdjPaM, rList);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), aAdjPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(6), aAdjPaM.GetMark()->GetContentIndex());

        CPPUNIT_ASSERT_EQUAL(u"学[がっ]校[こう]に行[い]こう。"_ustr, fnGetCombinedString());
    }

    // Empty PaM - Should insert
    {
        fnAppendJapanese(u"学校"_ustr);

        SwPosition aNewPos{ aPaM.GetPoint()->nNode, aPaM.GetPoint()->nContent };
        const SwTextNode* pTNd = aNewPos.GetNode().GetTextNode();
        pTNd->GoNext(aNewPos, SwCursorSkipMode::Chars);

        SwPaM aEmptyPaM{ aNewPos };
        aEmptyPaM.SetMark();

        SwRubyList rList;
        fnAppendRuby(&rList, u"森林"_ustr, u"しんりん"_ustr);
        fnAppendRuby(&rList, u"海上"_ustr, u"かいじょう"_ustr);
        fnAppendRuby(&rList, u"地面"_ustr, u"じめん"_ustr);

        CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aEmptyPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aEmptyPaM.GetMark()->GetContentIndex());
        m_pDoc->SetRubyList(aEmptyPaM, rList);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aEmptyPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aEmptyPaM.GetMark()->GetContentIndex());

        CPPUNIT_ASSERT_EQUAL(u"学森林[しんりん]海上[かいじょう]地面[じめん]校"_ustr,
                             fnGetCombinedString());

        // Operation should be idempotent
        CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aEmptyPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aEmptyPaM.GetMark()->GetContentIndex());
        m_pDoc->SetRubyList(aEmptyPaM, rList);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(1), aEmptyPaM.GetPoint()->GetContentIndex());
        CPPUNIT_ASSERT_EQUAL(sal_Int32(7), aEmptyPaM.GetMark()->GetContentIndex());

        CPPUNIT_ASSERT_EQUAL(u"学森林[しんりん]海上[かいじょう]地面[じめん]校"_ustr,
                             fnGetCombinedString());
    }
}

CPPUNIT_TEST_SUITE_REGISTRATION(SwDocTest);

CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
