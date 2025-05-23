/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <swmodeltestbase.hxx>

#include <com/sun/star/text/XTextColumns.hpp>
#include <com/sun/star/text/XTextTablesSupplier.hpp>
#include <com/sun/star/text/XTextSectionsSupplier.hpp>
#include <com/sun/star/text/XTextField.hpp>
#include <com/sun/star/graphic/XGraphic.hpp>

#include <editeng/boxitem.hxx>
#include <editeng/lrspitem.hxx>
#include <editeng/ulspitem.hxx>
#include <sfx2/docfile.hxx>
#include <sfx2/docfilt.hxx>

#include <editsh.hxx>
#include <ndgrf.hxx>
#include <docsh.hxx>
#include <unotxdoc.hxx>
#include <viewsh.hxx>
#include <IDocumentLayoutAccess.hxx>

// tests should only be added to ww8IMPORT *if* they fail round-tripping in ww8EXPORT

namespace
{
class Test : public SwModelTestBase
{
public:
    Test() : SwModelTestBase(u"/sw/qa/extras/ww8import/data/"_ustr, u"MS Word 97"_ustr)
    {
    }
};

CPPUNIT_TEST_FIXTURE(Test, testN816593)
{
    createSwDoc("n816593.doc");
    uno::Reference<text::XTextTablesSupplier> xTextTablesSupplier(mxComponent, uno::UNO_QUERY);
    uno::Reference<container::XIndexAccess> xIndexAccess(xTextTablesSupplier->getTextTables(), uno::UNO_QUERY);
    // Make sure that even if we import the two tables as non-floating, we
    // still consider them different, and not merge them.
    CPPUNIT_ASSERT_EQUAL(sal_Int32(2), xIndexAccess->getCount());
}

CPPUNIT_TEST_FIXTURE(Test, testBnc875715)
{
    createSwDoc("bnc875715.doc");
    uno::Reference<text::XTextSectionsSupplier> xTextSectionsSupplier(mxComponent, uno::UNO_QUERY);
    uno::Reference<container::XIndexAccess> xSections(xTextSectionsSupplier->getTextSections(), uno::UNO_QUERY);
    // Was incorrectly set as -1270.
    CPPUNIT_ASSERT_EQUAL(sal_Int32(0), getProperty<sal_Int32>(xSections->getByIndex(0), u"SectionLeftMargin"_ustr));
}

CPPUNIT_TEST_FIXTURE(Test, testFloatingTableSectionColumns)
{
    createSwDoc("floating-table-section-columns.doc");
    xmlDocUniquePtr pXmlDoc = parseLayoutDump();
    OUString tableWidth = getXPath(pXmlDoc, "/root/page[1]/body/section/column[2]/body/txt/anchored/fly/tab/infos/bounds", "width");
    // table width was restricted by a column
    CPPUNIT_ASSERT( tableWidth.toInt32() > 10000 );
}

CPPUNIT_TEST_FIXTURE(Test, testTdf124601)
{
    createSwDoc("tdf124601.doc");
    // Without the accompanying fix in place, this test would have failed, as the importer lost the
    // fLayoutInCell shape property for wrap-though shapes.
    CPPUNIT_ASSERT(getProperty<bool>(getShapeByName(u"Grafik 18"), u"IsFollowingTextFlow"_ustr));
    CPPUNIT_ASSERT(getProperty<bool>(getShapeByName(u"Grafik 19"), u"IsFollowingTextFlow"_ustr));
}

CPPUNIT_TEST_FIXTURE(Test, testImageLazyRead)
{
    createSwDoc("image-lazy-read.doc");
    auto xGraphic = getProperty<uno::Reference<graphic::XGraphic>>(getShape(1), u"Graphic"_ustr);
    Graphic aGraphic(xGraphic);
    // This failed, import loaded the graphic, it wasn't lazy-read.
    CPPUNIT_ASSERT(!aGraphic.isAvailable());
}

CPPUNIT_TEST_FIXTURE(Test, testImageLazyRead0size)
{
    createSwDoc("image-lazy-read-0size.doc");
    // Load a document with a single bitmap in it: it's declared as a WMF one, but actually a TGA
    // bitmap.
    SwDoc* pDoc = getSwDoc();
    SwNode* pNode = pDoc->GetNodes()[SwNodeOffset(6)];
    SwGrfNode* pGrfNode = pNode->GetGrfNode();
    CPPUNIT_ASSERT(pGrfNode);
    // Without the accompanying fix in place, this test would have failed with:
    // - Expected: 7590x10440
    // - Actual  : 0x0
    // i.e. the size was 0, even if the actual bitmap had a non-0 size.
    CPPUNIT_ASSERT_EQUAL(Size(7590, 10440), pGrfNode->GetTwipSize());
}

CPPUNIT_TEST_FIXTURE(Test, testTdf106799)
{
    createSwDoc("tdf106799.doc");
    // Ensure that all text portions are calculated before testing.
    SwViewShell* pViewShell
        = getSwDoc()->getIDocumentLayoutAccess().GetCurrentViewShell();
    CPPUNIT_ASSERT(pViewShell);
    pViewShell->Reformat();

    sal_Int32 const nCellWidths[3][4] = { { 9528, 0, 0, 0 },{ 2382, 2382, 2382, 2382 },{ 2382, 2382, 2382, 2382 } };
    sal_Int32 const nCellTxtLns[3][4] = { { 1, 0, 0, 0 },{ 1, 0, 0, 0},{ 1, 1, 1, 1 } };
    // Table was distorted because of missing sprmPFInnerTableCell at paragraph marks (0x0D) with sprmPFInnerTtp
    xmlDocUniquePtr pXmlDoc = parseLayoutDump();
    for (sal_Int32 nRow : { 0, 1, 2 })
        for (sal_Int32 nCell : { 0, 1, 2, 3 })
        {
            OString cellXPath("/root/page/body/tab/row/cell/tab/row[" + OString::number(nRow+1) + "]/cell[" + OString::number(nCell+1) + "]/");
            CPPUNIT_ASSERT_EQUAL_MESSAGE(cellXPath.getStr(), nCellWidths[nRow][nCell], getXPath(pXmlDoc, cellXPath + "infos/bounds", "width").toInt32());
            if (nCellTxtLns[nRow][nCell] != 0)
                CPPUNIT_ASSERT_EQUAL_MESSAGE(cellXPath.getStr(), nCellTxtLns[nRow][nCell], getXPath(pXmlDoc, cellXPath + "txt/SwParaPortion/SwLineLayout", "length").toInt32());
        }
}

CPPUNIT_TEST_FIXTURE(Test, testTdf121734)
{
    createSwDoc("tdf121734.doc");
    SwDoc* pDoc = getSwDoc();
    SwPosFlyFrames aPosFlyFrames = pDoc->GetAllFlyFormats(nullptr, false);
    // There is only one fly frame in the document: the one with the imported floating table
    CPPUNIT_ASSERT_EQUAL(size_t(1), aPosFlyFrames.size());
    for (const SwPosFlyFrame& rPosFlyFrame : aPosFlyFrames)
    {
        const SwFrameFormat& rFormat = rPosFlyFrame.GetFormat();
        const SfxPoolItem* pItem = nullptr;

        // The LR and UL spacings and borders must all be set explicitly;
        // spacings and border distances must be 0; borders must be absent.

        CPPUNIT_ASSERT_EQUAL(SfxItemState::SET, rFormat.GetItemState(RES_LR_SPACE, false, &pItem));
        auto pLR = static_cast<const SvxLRSpaceItem*>(pItem);
        CPPUNIT_ASSERT(pLR);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), pLR->ResolveLeft({}));
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), pLR->ResolveRight({}));

        CPPUNIT_ASSERT_EQUAL(SfxItemState::SET, rFormat.GetItemState(RES_UL_SPACE, false, &pItem));
        auto pUL = static_cast<const SvxULSpaceItem*>(pItem);
        CPPUNIT_ASSERT(pUL);
        CPPUNIT_ASSERT_EQUAL(sal_uInt16(0), pUL->GetUpper());
        CPPUNIT_ASSERT_EQUAL(sal_uInt16(0), pUL->GetLower());

        CPPUNIT_ASSERT_EQUAL(SfxItemState::SET, rFormat.GetItemState(RES_BOX, false, &pItem));
        auto pBox = static_cast<const SvxBoxItem*>(pItem);
        CPPUNIT_ASSERT(pBox);
        for (auto eLine : { SvxBoxItemLine::TOP, SvxBoxItemLine::BOTTOM,
                            SvxBoxItemLine::LEFT, SvxBoxItemLine::RIGHT })
        {
            CPPUNIT_ASSERT_EQUAL(sal_Int16(0), pBox->GetDistance(eLine));
            CPPUNIT_ASSERT(!pBox->GetLine(eLine));
        }
    }
}

CPPUNIT_TEST_FIXTURE(Test, testTdf125281)
{
    createSwDoc("tdf125281.doc");
#if !defined(_WIN32)
    // Windows fails with actual == 26171 for some reason; also lazy load isn't lazy in Windows
    // debug builds, reason is not known at the moment.

    // Load a .doc file which has an embedded .emf image.
    SwDoc* pDoc = getSwDoc();
    SwNode* pNode = pDoc->GetNodes()[SwNodeOffset(6)];
    CPPUNIT_ASSERT(pNode->IsGrfNode());
    SwGrfNode* pGrfNode = pNode->GetGrfNode();
    const Graphic& rGraphic = pGrfNode->GetGrf();

    // Without the accompanying fix in place, this test would have failed, as pref size was 0 till
    // an actual Paint() was performed (and even then, it was wrong).
    tools::Long nExpected = 25664;
    CPPUNIT_ASSERT_EQUAL(nExpected, rGraphic.GetPrefSize().getWidth());

    // Without the accompanying fix in place, this test would have failed, as setting the pref size
    // swapped the image in.
    CPPUNIT_ASSERT(!rGraphic.isAvailable());
#endif
}

CPPUNIT_TEST_FIXTURE(Test, testTdf122425_1)
{
    createSwDoc("tdf122425_1.doc");
    // This is for header text in case we use a hack for fixed-height headers
    // (see SwWW8ImplReader::Read_HdFtTextAsHackedFrame)
    SwDoc* pDoc = getSwDoc();
    SwPosFlyFrames aPosFlyFrames = pDoc->GetAllFlyFormats(nullptr, false);
    // There are two fly frames in the document: for first page's header, and for other pages'
    CPPUNIT_ASSERT_EQUAL(size_t(2), aPosFlyFrames.size());
    for (const SwPosFlyFrame& rPosFlyFrame : aPosFlyFrames)
    {
        const SwFrameFormat& rFormat = rPosFlyFrame.GetFormat();
        const SfxPoolItem* pItem = nullptr;

        // The LR and UL spacings and borders must all be set explicitly;
        // spacings and border distances must be 0; borders must be absent

        CPPUNIT_ASSERT_EQUAL(SfxItemState::SET, rFormat.GetItemState(RES_LR_SPACE, false, &pItem));
        auto pLR = static_cast<const SvxLRSpaceItem*>(pItem);
        CPPUNIT_ASSERT(pLR);
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), pLR->ResolveLeft({}));
        CPPUNIT_ASSERT_EQUAL(sal_Int32(0), pLR->ResolveRight({}));

        CPPUNIT_ASSERT_EQUAL(SfxItemState::SET, rFormat.GetItemState(RES_UL_SPACE, false, &pItem));
        auto pUL = static_cast<const SvxULSpaceItem*>(pItem);
        CPPUNIT_ASSERT(pUL);
        CPPUNIT_ASSERT_EQUAL(sal_uInt16(0), pUL->GetUpper());
        CPPUNIT_ASSERT_EQUAL(sal_uInt16(0), pUL->GetLower());

        CPPUNIT_ASSERT_EQUAL(SfxItemState::SET, rFormat.GetItemState(RES_BOX, false, &pItem));
        auto pBox = static_cast<const SvxBoxItem*>(pItem);
        CPPUNIT_ASSERT(pBox);
        for (auto eLine : { SvxBoxItemLine::TOP, SvxBoxItemLine::BOTTOM,
                            SvxBoxItemLine::LEFT, SvxBoxItemLine::RIGHT })
        {
            CPPUNIT_ASSERT_EQUAL(sal_Int16(0), pBox->GetDistance(eLine));
            CPPUNIT_ASSERT(!pBox->GetLine(eLine));
        }
    }

    //tdf#139495: without the fix, a negative number was converted into a uInt16, overflowing to 115501
    auto nDist = getProperty<sal_uInt32>(getStyles(u"PageStyles"_ustr)->getByName(u"Standard"_ustr), u"HeaderBodyDistance"_ustr);
    CPPUNIT_ASSERT_EQUAL(sal_uInt32(0), nDist);
}

CPPUNIT_TEST_FIXTURE(Test, testTdf110987)
{
    createSwDoc("tdf110987");
    // The input document is an empty .doc, but without file name
    // extension. Check that it was loaded as a normal .doc document,
    // and not a template.
    OUString sFilterName = getSwDocShell()->GetMedium()->GetFilter()->GetFilterName();
    CPPUNIT_ASSERT(sFilterName != "MS Word 97 Vorlage");
}

CPPUNIT_TEST_FIXTURE(Test, testTdf120761_zOrder)
{
    createSwDoc("tdf120761_zOrder.dot");
    //The blue shape was covering everything (highest zorder = 2) instead of the lowest(0)
    uno::Reference<drawing::XShape> xShape(getShapeByName(u"Picture 2"), uno::UNO_QUERY);
    CPPUNIT_ASSERT_EQUAL(sal_uInt32(0), getProperty<sal_uInt32>(xShape, u"ZOrder"_ustr));
}

CPPUNIT_TEST_FIXTURE(Test, testTdf142003)
{
    createSwDoc("changes-in-footnote.doc");

    SwEditShell* const pEditShell(getSwDoc()->GetEditShell());
    CPPUNIT_ASSERT(pEditShell);
    pEditShell->AcceptRedline(0);

    //The changes were offset from where they should have been
    uno::Reference<text::XFootnotesSupplier> xFootnotesSupplier(mxComponent, uno::UNO_QUERY);
    uno::Reference<container::XIndexAccess> xFootnotes = xFootnotesSupplier->getFootnotes();
    uno::Reference<text::XTextRange> xParagraph(xFootnotes->getByIndex(0), uno::UNO_QUERY);
    //before change was incorrect, Loren ipsum , doconsectetur ...
    CPPUNIT_ASSERT(xParagraph->getString().startsWith("\tLorem ipsum , consectetur adipiscing elit."));
}

CPPUNIT_TEST_FIXTURE(Test, testTdf160301)
{
    // Without the fix in place, fields in the test doc are imported as DocInformation
    // with the content set to the variable name
    createSwDoc("tdf160301.doc");
    uno::Reference<text::XTextFieldsSupplier> xTextFieldsSupplier(mxComponent, uno::UNO_QUERY);
    uno::Reference<container::XEnumerationAccess> xFieldsAccess(xTextFieldsSupplier->getTextFields());
    uno::Reference<container::XEnumeration> xFields(xFieldsAccess->createEnumeration());
    uno::Reference<text::XTextField> xField(xFields->nextElement(), uno::UNO_QUERY);
    // Without the fix in place this fails with
    // Expected: Jeff Smith
    // Actual:   FullName
    CPPUNIT_ASSERT_EQUAL(u"Jeff Smith"_ustr, xField->getPresentation(false));
    // Without the fix in place this fails with
    // Expected: User Field FullName = Jeff Smith
    // Actual:   DocInformation:FullName
    CPPUNIT_ASSERT_EQUAL(u"User Field FullName = Jeff Smith"_ustr, xField->getPresentation(true));
}

CPPUNIT_TEST_FIXTURE(Test, testTdf127048)
{
    // Use loadComponentFromURL so MacrosTest::loadFromDesktop is not called.
    // Otherwise it sets MacroExecutionMode to ALWAYS_EXECUTE_NO_WARN
    mxComponent = mxDesktop->loadComponentFromURL(createFileURL(u"tdf127048.doc"), u"_default"_ustr, 0, {});
    CPPUNIT_ASSERT_EQUAL(false, getSwDocShell()->GetMacroCallsSeenWhileLoading());
}

CPPUNIT_TEST_FIXTURE(Test, testTdf134902)
{
    createSwDoc("tdf134902.docx");
    CPPUNIT_ASSERT_EQUAL(4, getShapes());
    uno::Reference<drawing::XShape> xShape;
    uno::Reference< beans::XPropertySet > XPropSet;
    for (int i = 3; i<= getShapes(); i++)
    {
        xShape = getShape(i);
        XPropSet.set( xShape, uno::UNO_QUERY_THROW );
        try
        {
            bool isVisible = true;
            XPropSet->getPropertyValue(u"Visible"_ustr) >>= isVisible;
            CPPUNIT_ASSERT(!isVisible);
        }
        catch (beans::UnknownPropertyException &)
        { /* ignore */ }
    }

}

// tests should only be added to ww8IMPORT *if* they fail round-tripping in ww8EXPORT

} // end of anonymous namespace
CPPUNIT_PLUGIN_IMPLEMENT();

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
