/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <swmodeltestbase.hxx>

#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/drawing/XDrawPageSupplier.hpp>
#include <com/sun/star/graphic/XGraphic.hpp>
#include <com/sun/star/text/XTextTablesSupplier.hpp>
#include <com/sun/star/text/XTextTable.hpp>
#include <com/sun/star/text/XTextDocument.hpp>

#include <vcl/graph.hxx>

#include <fmtfsize.hxx>

using namespace ::com::sun::star;

namespace
{
/// Tests for sw/source/writerfilter/rtftok/rtfdocumentimpl.cxx.
class Test : public SwModelTestBase
{
public:
    Test()
        : SwModelTestBase(u"/sw/qa/writerfilter/rtftok/data/"_ustr)
    {
    }
};

CPPUNIT_TEST_FIXTURE(Test, testPicwPich)
{
    // Given a document with a WMF file where picwgoal and picscalex is provided, so picw is not
    // relevant:
    loadFromFile(u"picw-pich.rtf");

    // Then make sure the graphic's preferred size is correct:
    uno::Reference<drawing::XDrawPageSupplier> xTextDocument(mxComponent, uno::UNO_QUERY);
    uno::Reference<drawing::XDrawPage> xDrawPage = xTextDocument->getDrawPage();
    uno::Reference<beans::XPropertySet> xShape(xDrawPage->getByIndex(0), uno::UNO_QUERY);
    uno::Reference<graphic::XGraphic> xGraphic;
    xShape->getPropertyValue(u"Graphic"_ustr) >>= xGraphic;
    Graphic aGraphic(xGraphic);
    Size aPrefSize = aGraphic.GetPrefSize();
    // Without the accompanying fix in place, this test would have failed with:
    // - Expected: 2619
    // - Actual  : 132
    // i.e. the graphic width didn't match 2.62 cm from the Word UI.
    CPPUNIT_ASSERT_EQUAL(static_cast<tools::Long>(2619), aPrefSize.Width());
}

CPPUNIT_TEST_FIXTURE(Test, testCharHiddenInTable)
{
    // Given a document with a table, and a hidden \line in it:
    loadFromFile(u"char-hidden-intbl.rtf");

    // Then make sure that line is indeed hidden:
    uno::Reference<text::XTextTablesSupplier> xTextDocument(mxComponent, uno::UNO_QUERY);
    uno::Reference<container::XIndexAccess> xTables(xTextDocument->getTextTables(), uno::UNO_QUERY);
    uno::Reference<text::XTextTable> xTable(xTables->getByIndex(0), uno::UNO_QUERY);
    uno::Reference<container::XEnumerationAccess> xCell(xTable->getCellByName(u"B1"_ustr),
                                                        uno::UNO_QUERY);
    uno::Reference<container::XEnumeration> xParagraphs = xCell->createEnumeration();
    uno::Reference<container::XEnumerationAccess> xParagraph(xParagraphs->nextElement(),
                                                             uno::UNO_QUERY);
    uno::Reference<container::XEnumeration> xPortions = xParagraph->createEnumeration();
    uno::Reference<beans::XPropertySet> xPortion(xPortions->nextElement(), uno::UNO_QUERY);
    bool bCharHidden{};
    xPortion->getPropertyValue(u"CharHidden"_ustr) >>= bCharHidden;
    // Without the accompanying fix in place, this test would have failed, the newline was not
    // hidden.
    CPPUNIT_ASSERT(bCharHidden);
}

CPPUNIT_TEST_FIXTURE(Test, testDuplicatedImage)
{
    // Given a document with 2 images:
    loadFromFile(u"duplicated-image.rtf");

    // Then make sure no duplicated images are created:
    uno::Reference<drawing::XDrawPageSupplier> xTextDocument(mxComponent, uno::UNO_QUERY);
    uno::Reference<drawing::XDrawPage> xDrawPage = xTextDocument->getDrawPage();
    // Without the accompanying fix in place, this test would have failed with:
    // - Expected: 2
    // - Actual  : 3
    // i.e. there was a 3rd, duplicated image.
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_Int32>(2), xDrawPage->getCount());
}

CPPUNIT_TEST_FIXTURE(Test, testOldParaNumLeftMargin)
{
    // Given a document with 3 paragraphs, the third one with a left indent:
    loadFromFile(u"old-para-num-left-margin.rtf");

    // Then make sure that the third paragraph has a left indent:
    uno::Reference<text::XTextDocument> xTextDocument(mxComponent, uno::UNO_QUERY);
    uno::Reference<container::XEnumerationAccess> xText(xTextDocument->getText(), uno::UNO_QUERY);
    uno::Reference<container::XEnumeration> xParagraphs = xText->createEnumeration();
    xParagraphs->nextElement();
    xParagraphs->nextElement();
    uno::Reference<beans::XPropertySet> xParagraph(xParagraphs->nextElement(), uno::UNO_QUERY);
    sal_Int32 nParaLeftMargin{};
    xParagraph->getPropertyValue(u"ParaLeftMargin"_ustr) >>= nParaLeftMargin;
    // Without the accompanying fix in place, this test would have failed with:
    // - Expected: 2101
    // - Actual  : 0
    // i.e. the left indent was 0, not 1191 twips (from the file) in mm100.
    CPPUNIT_ASSERT_EQUAL(static_cast<sal_Int32>(2101), nParaLeftMargin);
}

CPPUNIT_TEST_FIXTURE(Test, testImageSizeInShapeText)
{
    // Given an RTF document with a shape and an icon image inside that:
    // When loading this document:
    createSwDoc("image-size-in-shape-text.rtf");

    // Then make sure that the icon image has the correct (small) size:
    SwDoc* pDoc = getSwDoc();
    const sw::FrameFormats<sw::SpzFrameFormat*>& rFlyFormats = *pDoc->GetSpzFrameFormats();
    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), rFlyFormats.size());
    const sw::SpzFrameFormat& rImageFlyFormat = *rFlyFormats[1];
    const SwFormatFrameSize& rImageSize = rImageFlyFormat.GetFrameSize();
    // 255 * 0.82 in the RTF file.
    SwTwips nExpected = 209;
    // Without the accompanying fix in place, this test would have failed with:
    // - Expected: 209
    // - Actual  : 8808
    // i.e. the size of the icon was inherited from the containing shape, losing the own size.
    CPPUNIT_ASSERT_EQUAL(nExpected, rImageSize.GetWidth());
}
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
