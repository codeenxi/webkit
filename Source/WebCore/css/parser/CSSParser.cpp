/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2012, 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "CSSParser.h"

#include "CSSCustomPropertyValue.h"
#include "CSSKeyframeRule.h"
#include "CSSParserFastPaths.h"
#include "CSSParserImpl.h"
#include "CSSPendingSubstitutionValue.h"
#include "CSSPropertyParser.h"
#include "CSSSelectorParser.h"
#include "CSSSupportsParser.h"
#include "CSSTokenizer.h"
#include "CSSVariableData.h"
#include "CSSVariableReferenceValue.h"
#include "Document.h"
#include "Element.h"
#include "Page.h"
#include "RenderStyle.h"
#include "RenderTheme.h"
#include "Settings.h"
#include "StyleBuilder.h"
#include "StyleColor.h"
#include "StyleResolver.h"
#include "StyleRule.h"
#include "StyleSheetContents.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

CSSParser::CSSParser(const CSSParserContext& context)
    : m_context(context)
{
}

CSSParser::~CSSParser() = default;

void CSSParser::parseSheet(StyleSheetContents* sheet, const String& string, RuleParsing ruleParsing)
{
    return CSSParserImpl::parseStyleSheet(string, m_context, sheet, ruleParsing);
}

void CSSParser::parseSheetForInspector(const CSSParserContext& context, StyleSheetContents* sheet, const String& string, CSSParserObserver& observer)
{
    return CSSParserImpl::parseStyleSheetForInspector(string, context, sheet, observer);
}

RefPtr<StyleRuleBase> CSSParser::parseRule(const CSSParserContext& context, StyleSheetContents* sheet, const String& string)
{
    return CSSParserImpl::parseRule(string, context, sheet, CSSParserImpl::AllowImportRules);
}

RefPtr<StyleRuleKeyframe> CSSParser::parseKeyframeRule(const String& string)
{
    RefPtr<StyleRuleBase> keyframe = CSSParserImpl::parseRule(string, m_context, nullptr, CSSParserImpl::KeyframeRules);
    return downcast<StyleRuleKeyframe>(keyframe.get());
}

bool CSSParser::parseSupportsCondition(const String& condition)
{
    CSSParserImpl parser(m_context, condition);
    return CSSSupportsParser::supportsCondition(parser.tokenizer()->tokenRange(), parser, CSSSupportsParser::ForWindowCSS) == CSSSupportsParser::Supported;
}

Color CSSParser::parseColor(const String& string, bool strict)
{
    if (string.isEmpty())
        return Color();
    
    // Try named colors first.
    Color namedColor { string };
    if (namedColor.isValid())
        return namedColor;
    
    // Try the fast path to parse hex and rgb.
    RefPtr<CSSValue> value = CSSParserFastPaths::parseColor(string, strict ? HTMLStandardMode : HTMLQuirksMode);
    
    // If that fails, try the full parser.
    if (!value)
        value = parseSingleValue(CSSPropertyColor, string, strictCSSParserContext());
    if (!value || !value->isPrimitiveValue())
        return Color();
    const auto& primitiveValue = downcast<CSSPrimitiveValue>(*value);
    if (!primitiveValue.isRGBColor())
        return Color();
    return primitiveValue.color();
}

Color CSSParser::parseSystemColor(const String& string, const CSSParserContext* context)
{
    CSSValueID id = cssValueKeywordID(string);
    if (!StyleColor::isSystemColor(id))
        return Color();

    OptionSet<StyleColor::Options> options;
    if (context && context->useSystemAppearance)
        options.add(StyleColor::Options::UseSystemAppearance);
    return RenderTheme::singleton().systemColor(id, options);
}

RefPtr<CSSValue> CSSParser::parseSingleValue(CSSPropertyID propertyID, const String& string, const CSSParserContext& context)
{
    if (string.isEmpty())
        return nullptr;
    if (RefPtr<CSSValue> value = CSSParserFastPaths::maybeParseValue(propertyID, string, context.mode))
        return value;
    CSSTokenizer tokenizer(string);
    return CSSPropertyParser::parseSingleValue(propertyID, tokenizer.tokenRange(), context);
}

CSSParser::ParseResult CSSParser::parseValue(MutableStyleProperties& declaration, CSSPropertyID propertyID, const String& string, bool important, const CSSParserContext& context)
{
    ASSERT(!string.isEmpty());
    RefPtr<CSSValue> value = CSSParserFastPaths::maybeParseValue(propertyID, string, context.mode);
    if (value)
        return declaration.addParsedProperty(CSSProperty(propertyID, WTFMove(value), important)) ? CSSParser::ParseResult::Changed : CSSParser::ParseResult::Unchanged;

    CSSParser parser(context);
    return parser.parseValue(declaration, propertyID, string, important);
}

CSSParser::ParseResult CSSParser::parseCustomPropertyValue(MutableStyleProperties& declaration, const AtomString& propertyName, const String& string, bool important, const CSSParserContext& context)
{
    return CSSParserImpl::parseCustomPropertyValue(&declaration, propertyName, string, important, context);
}

CSSParser::ParseResult CSSParser::parseValue(MutableStyleProperties& declaration, CSSPropertyID propertyID, const String& string, bool important)
{
    return CSSParserImpl::parseValue(&declaration, propertyID, string, important, m_context);
}

void CSSParser::parseSelector(const String& string, CSSSelectorList& selectorList)
{
    CSSTokenizer tokenizer(string);
    selectorList = CSSSelectorParser::parseSelector(tokenizer.tokenRange(), m_context, nullptr);
}

Ref<ImmutableStyleProperties> CSSParser::parseInlineStyleDeclaration(const String& string, const Element* element)
{
    return CSSParserImpl::parseInlineStyleDeclaration(string, element);
}

bool CSSParser::parseDeclaration(MutableStyleProperties& declaration, const String& string)
{
    return CSSParserImpl::parseDeclarationList(&declaration, string, m_context);
}

void CSSParser::parseDeclarationForInspector(const CSSParserContext& context, const String& string, CSSParserObserver& observer)
{
    CSSParserImpl::parseDeclarationListForInspector(string, context, observer);
}

RefPtr<CSSValue> CSSParser::parseValueWithVariableReferences(CSSPropertyID propID, const CSSValue& value, Style::BuilderState& builderState)
{
    ASSERT((propID == CSSPropertyCustom && value.isCustomPropertyValue()) || (propID != CSSPropertyCustom && !value.isCustomPropertyValue()));
    auto& style = builderState.style();
    auto direction = style.direction();
    auto writingMode = style.writingMode();

    if (value.isPendingSubstitutionValue()) {
        // FIXME: Should have a resolvedShorthands cache to stop this from being done
        // over and over for each longhand value.
        const CSSPendingSubstitutionValue& pendingSubstitution = downcast<CSSPendingSubstitutionValue>(value);
        CSSPropertyID shorthandID = pendingSubstitution.shorthandPropertyId();
        if (CSSProperty::isDirectionAwareProperty(shorthandID))
            shorthandID = CSSProperty::resolveDirectionAwareProperty(shorthandID, direction, writingMode);
        CSSVariableReferenceValue* shorthandValue = pendingSubstitution.shorthandValue();

        auto resolvedData = shorthandValue->resolveVariableReferences(builderState);
        if (!resolvedData)
            return nullptr;
        Vector<CSSParserToken> resolvedTokens = resolvedData->tokens();
        
        ParsedPropertyVector parsedProperties;
        if (!CSSPropertyParser::parseValue(shorthandID, false, resolvedTokens, m_context, parsedProperties, StyleRuleType::Style))
            return nullptr;
        
        for (auto& property : parsedProperties) {
            if (property.id() == propID)
                return property.value();
        }
        
        return nullptr;
    }

    if (value.isVariableReferenceValue()) {
        const CSSVariableReferenceValue& valueWithReferences = downcast<CSSVariableReferenceValue>(value);
        auto resolvedData = valueWithReferences.resolveVariableReferences(builderState);
        if (!resolvedData)
            return nullptr;
        return CSSPropertyParser::parseSingleValue(propID, resolvedData->tokens(), m_context);
    }

    const auto& customPropValue = downcast<CSSCustomPropertyValue>(value);
    const auto& valueWithReferences = WTF::get<Ref<CSSVariableReferenceValue>>(customPropValue.value()).get();

    auto& name = downcast<CSSCustomPropertyValue>(value).name();
    auto* registered = builderState.document().getCSSRegisteredCustomPropertySet().get(name);
    auto& syntax = registered ? registered->syntax : "*";
    auto resolvedData = valueWithReferences.resolveVariableReferences(builderState);

    if (!resolvedData)
        return nullptr;

    // FIXME handle REM cycles.
    HashSet<CSSPropertyID> dependencies;
    CSSPropertyParser::collectParsedCustomPropertyValueDependencies(syntax, false, dependencies, resolvedData->tokens(), m_context);

    for (auto id : dependencies)
        builderState.builder().applyProperty(id);

    return CSSPropertyParser::parseTypedCustomPropertyValue(name, syntax, resolvedData->tokens(), builderState, m_context);
}

std::unique_ptr<Vector<double>> CSSParser::parseKeyframeKeyList(const String& selector)
{
    return CSSParserImpl::parseKeyframeKeyList(selector);
}

RefPtr<CSSValue> CSSParser::parseFontFaceDescriptor(CSSPropertyID propertyID, const String& propertyValue, const CSSParserContext& context)
{
    String string = makeString("@font-face { ", getPropertyNameString(propertyID), " : ", propertyValue, "; }");
    RefPtr<StyleRuleBase> rule = parseRule(context, nullptr, string);
    if (!rule || !rule->isFontFaceRule())
        return nullptr;
    return downcast<StyleRuleFontFace>(*rule.get()).properties().getPropertyCSSValue(propertyID);
}

}
