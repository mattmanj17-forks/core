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

#include "SvgNumber.hxx"
#include "svgtoken.hxx"
#include "svgtools.hxx"
#include <com/sun/star/xml/sax/XAttributeList.hpp>
#include <drawinglayer/primitive2d/Primitive2DContainer.hxx>
#include <memory>
#include <string_view>
#include <vector>
#include <optional>

// predefines
namespace svgio::svgreader
{
    class SvgDocument;
    class SvgStyleAttributes;
}



namespace svgio::svgreader
    {
        enum class XmlSpace
        {
            NotSet,
            Default,
            Preserve
        };

        // display property (see SVG 1.1. 11.5), not inheritable
        enum class Display // #i121656#
        {
            Inline, // the default
            Block,
            ListItem,
            RunIn,
            Compact,
            Marker,
            Table,
            InlineTable,
            TableRowGroup,
            TableHeaderGroup,
            TableFooterGroup,
            TableRow,
            TableColumnGroup,
            TableColumn,
            TableCell,
            TableCaption,
            None,
            Inherit
        };

        // helper to convert a string associated with a token of type SVGTokenDisplay
        // to the enum Display. Empty strings return the default 'Display_inline' with
        // which members should be initialized
        Display getDisplayFromContent(std::u16string_view aContent);

      class Visitor;

        class SvgNode : public InfoProvider
        {
        private:
            /// basic data, Type, document we belong to and parent (if not root)
            SVGToken                    maType;
            SvgDocument&                mrDocument;
            const SvgNode*              mpParent;
            const SvgNode*              mpAlternativeParent;

            /// sub hierarchy
            std::vector< std::unique_ptr<SvgNode> >  maChildren;

            /// Id svan value
            std::optional<OUString>   mpId;

            /// Class svan value
            std::optional<OUString>   mpClass;

            /// systemLanguage values
            SvgStringVector  maSystemLanguage;

            /// XmlSpace value
            XmlSpace                    maXmlSpace;

            /// Display value #i121656#
            Display                     maDisplay;

            // CSS style vector chain, used in decompose phase and built up once per node.
            // It contains the StyleHierarchy for the local node. Independent from the
            // node hierarchy itself which also needs to be used in style entry solving
            ::std::vector< const SvgStyleAttributes* > maCssStyleVector;

            /// possible local CssStyle, e.g. style="fill:red; stroke:red;"
            std::unique_ptr<SvgStyleAttributes>        mpLocalCssStyle;

            mutable bool                mbDecomposing;

            // flag if maCssStyleVector is already computed (done only once)
            bool                        mbCssStyleVectorBuilt : 1;

        protected:
            /// helper to evtl. link to css style
            const SvgStyleAttributes* checkForCssStyle(const SvgStyleAttributes& rOriginal) const;

            /// helper for filling the CssStyle vector once dependent on mbCssStyleVectorBuilt
            void fillCssStyleVector(const SvgStyleAttributes& rOriginal);
            void addCssStyle(
                const SvgDocument& rDocument,
                const OUString& aConcatenated);
            void fillCssStyleVectorUsingHierarchyAndSelectors(
                const SvgNode& rCurrent,
                std::u16string_view aConcatenated);
            void fillCssStyleVectorUsingParent(
                const SvgNode& rCurrent);

        public:
            SvgNode(
                SVGToken aType,
                SvgDocument& rDocument,
                SvgNode* pParent);
            virtual ~SvgNode() override;
            SvgNode(const SvgNode&) = delete;
            SvgNode& operator=(const SvgNode&) = delete;

            void accept(Visitor& rVisitor);

            /// scan helper to read and interpret a local CssStyle to mpLocalCssStyle
            void readLocalCssStyle(std::u16string_view aContent);

            /// style helpers
            void parseAttributes(const css::uno::Reference< css::xml::sax::XAttributeList >& xAttribs);
            virtual const SvgStyleAttributes* getSvgStyleAttributes() const;
            virtual void parseAttribute(SVGToken aSVGToken, const OUString& aContent);
            virtual void decomposeSvgNode(drawinglayer::primitive2d::Primitive2DContainer& rTarget, bool bReferenced) const;

            /// #i125258# tell if this node is allowed to have a parent style (e.g. defs do not)
            virtual bool supportsParentStyle() const;

            /// basic data read access
            SVGToken getType() const { return maType; }
            const SvgDocument& getDocument() const { return mrDocument; }
            const SvgNode* getParent() const { if(mpAlternativeParent) return mpAlternativeParent; return mpParent; }
            const std::vector< std::unique_ptr<SvgNode> > & getChildren() const { return maChildren; }

            /// InfoProvider support for %, em and ex values
            virtual basegfx::B2DRange getCurrentViewPort() const override;
            virtual double getCurrentFontSize() const override;
            virtual double getCurrentXHeight() const override;

            /// Id access
            std::optional<OUString> const & getId() const { return mpId; }
            void setId(OUString const &);

            /// Class access
            std::optional<OUString> const & getClass() const { return mpClass; }
            void setClass(OUString const &);

            /// SystemLanguage access
            std::vector<OUString> const & getSystemLanguage() const { return maSystemLanguage; }

            /// XmlSpace access
            XmlSpace getXmlSpace() const;
            void setXmlSpace(XmlSpace eXmlSpace) { maXmlSpace = eXmlSpace; }

            /// Display access #i121656#
            Display getDisplay() const { return maDisplay; }
            void setDisplay(Display eDisplay) { maDisplay = eDisplay; }

            /// alternative parent
            void setAlternativeParent(const SvgNode* pAlternativeParent = nullptr) { mpAlternativeParent = pAlternativeParent; }
        };

      class Visitor
      {
      public:
            virtual ~Visitor() = default;
            virtual void visit(SvgNode const & pNode) = 0;
      };

} // end of namespace svgio::svgreader

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
