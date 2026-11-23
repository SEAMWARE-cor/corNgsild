//
// FILE            ldLangReduce.c
//
// AUTHOR          Ken Zangelin
//
// Copyright 2026 Seamware
// SPDX-License-Identifier: Apache-2.0
// 
//
// Operates on COMPACTED trees (after corLdCompactTree) - except for the
// languageMap keys, compared as tags either way (ldLanguageTag) - so names are
// short-form: "languageMap", "value", "type", "lang", "observedAt", etc.
//
#include <stdbool.h>                                     // bool
#include <stdlib.h>                                      // strtod
#include <string.h>                                      // strcmp
#include <strings.h>                                     // strncasecmp
#include "corRest/corRest.h"                            // corRest

#include "corAlloc/CorAlloc.h"                         // CorAlloc
#include "corTree/CorNode.h"                            // CorNode
#include "corTree/corTreeBuilder.h"                     // corTreeString
#include "corTree/corTreeChildReplace.h"                // corTreeChildReplace

#include "corNgsild/ldAttrMember.h"                     // ldAttrMemberOf
#include "corNgsild/ldTermClass.h"                      // ldTermClass, LD_TC_*
#include "corNgsild/CorNgsild.h"                        // corNgsild
#include "corNgsild/ldIsEntityKeyword.h"                // ldIsEntityKeyword
#include "corNgsild/ldLanguageKey.h"                    // ldLanguageTag
#include "corNgsild/ldTermId.h"                           // ldTermId, CorTerm*
#include "corNgsild/ldLangReduce.h"                       // Own interface



// -----------------------------------------------------------------------------
//
// isAttrKeyword - (compacted names)
//
static bool isAttrKeyword(CorNode* nodeP)
{
  return ldAttrMemberOf(nodeP) != 0;   // an Attribute's own member ("lang" included) - not a sub-attribute
}



// -----------------------------------------------------------------------------
//
// LangRange - one language range of the 'lang' parameter, with its quality value
//
typedef struct LangRange
{
  const char* tag;      // not zero-terminated - 'len' chars
  int         len;
  double      q;
} LangRange;



// -----------------------------------------------------------------------------
//
// langRangesParse - the 'lang' parameter as a list of language ranges, best first
//
// TS 104 175 § 7.2.7 (example 4): "fr-CH,fr;q=0.9,en;q=0.8,*;q=0.5" - the Accept-Language syntax of
// RFC 9110 § 12.5.4: a range without 'q' has q=1, q=0 means "not acceptable" (left out), equal q keep
// their order (the sort is stable).
//
static int langRangesParse(const char* lang, LangRange* rangeV, int rangeMax)
{
  int         ranges = 0;
  const char* p      = lang;

  while ((*p != 0) && (ranges < rangeMax))
  {
    while ((*p == ' ') || (*p == '\t') || (*p == ','))
      ++p;
    if (*p == 0)
      break;

    const char* tag = p;
    while ((*p != 0) && (*p != ',') && (*p != ';') && (*p != ' ') && (*p != '\t'))
      ++p;
    int len = (int) (p - tag);

    double q = 1;
    while ((*p != 0) && (*p != ','))   // parameters - only 'q' means something
    {
      if (((p[0] == 'q') || (p[0] == 'Q')) && (p[1] == '=') && ((p[-1] == ';') || (p[-1] == ' ') || (p[-1] == '\t')))
        q = strtod(&p[2], NULL);
      ++p;
    }

    if ((len == 0) || (q <= 0))
      continue;

    int ix = ranges;                   // insertion sort: after every range with q >= this one's
    while ((ix > 0) && (rangeV[ix - 1].q < q))
    {
      rangeV[ix] = rangeV[ix - 1];
      --ix;
    }
    rangeV[ix].tag = tag;
    rangeV[ix].len = len;
    rangeV[ix].q   = q;
    ++ranges;
  }

  return ranges;
}



// -----------------------------------------------------------------------------
//
// langMapMatch - the languageMap key that the ranges pick, NULL if none does (or '*' is reached first)
//
// Per range, best first: the key with that very tag (tags are case-insensitive, RFC 5646 § 2.1.1), else the
// first key the range is a prefix of (RFC 4647 § 3.3.1 basic filtering: "fr" matches "fr-CH").
//
static CorNode* langMapMatch(CorNode* langMapP, LangRange* rangeV, int ranges)
{
  for (int ix = 0; ix < ranges; ix++)
  {
    const char* range  = rangeV[ix].tag;
    int         len    = rangeV[ix].len;
    CorNode*    prefix = NULL;

    if ((len == 1) && (range[0] == '*'))
      return NULL;

    for (CorNode* keyP = langMapP->value.head; keyP != NULL; keyP = keyP->next)
    {
      const char* tag = ldLanguageTag(keyP->name, corNgsild.contextP, &corRest.kalloc);

      if (strncasecmp(tag, range, len) == 0)
      {
        if (tag[len] == 0)
          return keyP;
        if ((tag[len] == '-') && (prefix == NULL))
          prefix = keyP;
      }
    }

    if (prefix != NULL)
      return prefix;
  }

  return NULL;
}



// -----------------------------------------------------------------------------
//
// attrLangReduce - reduce a single LanguageProperty attribute to Property
//
// 1. Find the "languageMap" child
// 2. Look up the key the language ranges pick (langMapMatch), else @none > "en" > first key
// 3. Change "type" from "LanguageProperty" to "Property"
// 4. Replace "languageMap" with "value" (the matched string)
// 5. Add a "lang" sub-property with the chosen language tag
// 6. Recurse into sub-attributes
//
static void attrLangReduce(CorNode* attrP, LangRange* rangeV, int ranges, CorAlloc* faP)
{
  if (attrP->type != CorObject)
    return;

  // Find languageMap child — if not found, this is not a LanguageProperty
  CorNode* langMapP = NULL;
  CorNode* typeP   = NULL;

  for (CorNode* childP = attrP->value.head; childP != NULL; childP = childP->next)
  {
    if (ldTermId(childP) == CorTermLanguageMap)
      langMapP = childP;
    else if (ldTermId(childP) == CorTermType)
      typeP = childP;
  }

  if (langMapP != NULL && langMapP->type == CorObject)
  {
    //
    // No range picks a key (or '*' was reached): "it shall default to any supported language" (§ 7.2.7) -
    // @none if present, then "en" as our choice, then the first key.
    //
    // Keys are compared as TAGS: on a tree that has not been compacted yet they are
    // still the broker's expanded encoding (ldLanguageKey.h); on a compacted one
    // ldLanguageTag returns them as they are.
    //
    CorNode* matchP = langMapMatch(langMapP, rangeV, ranges);
    CorNode* noneP  = NULL;
    CorNode* enP    = NULL;
    CorNode* firstP = langMapP->value.head;

    if (matchP == NULL)
    {
      for (CorNode* keyP = langMapP->value.head; keyP != NULL; keyP = keyP->next)
      {
        const char* tag = ldLanguageTag(keyP->name, corNgsild.contextP, &corRest.kalloc);

        if      ((noneP == NULL) && (strcmp(tag, "@none") == 0))  noneP = keyP;
        else if ((enP   == NULL) && (strcmp(tag, "en")    == 0))  enP   = keyP;
      }
    }

    if (matchP == NULL)
      matchP = (noneP != NULL) ? noneP : (enP != NULL) ? enP : firstP;

    if (matchP != NULL)
    {
      const char* chosenLang = ldLanguageTag(matchP->name, corNgsild.contextP, &corRest.kalloc);

      // Create "value" node with the matched value
      CorNode* valueP = corTreeString(corRest.kallocP, "value", matchP->value.s);
      valueP->type = matchP->type;
      if (matchP->type != CorString)
        valueP->value = matchP->value;

      // Replace "languageMap" with "value"
      corTreeChildReplace(attrP, langMapP, valueP);

      // Change type from "LanguageProperty" to "Property"
      if (typeP != NULL && typeP->type == CorString)
        typeP->value.s = (char*) "Property";

      // Add "lang" sub-property with the chosen language tag
      CorNode* langNodeP = corTreeString(corRest.kallocP, "lang", chosenLang);
      corTreeChildAdd(attrP, langNodeP);
    }
  }

  // Recurse into sub-attributes
  for (CorNode* childP = attrP->value.head; childP != NULL; childP = childP->next)
  {
    if (isAttrKeyword(childP) == false)
      attrLangReduce(childP, rangeV, ranges, faP);
  }
}



// -----------------------------------------------------------------------------
//
// ldLangReduce - reduce LanguageProperty attributes to Property with matching language
//
// For the temporal API (§ 5.7.3) an attribute is rendered as an array of
// instance objects rather than a single object. When the entity-level child
// is an array, walk it and reduce each instance.
//
void ldLangReduce(CorNode* entityP, const char* lang, CorAlloc* faP)
{
  if (entityP == NULL || entityP->type != CorObject)
    return;

  LangRange rangeV[16];
  int       ranges = langRangesParse(lang, rangeV, 16);

  for (CorNode* childP = entityP->value.head; childP != NULL; childP = childP->next)
  {
    if (childP->name == NULL || ldIsEntityMember(childP))
      continue;

    if (childP->type == CorArray)
    {
      for (CorNode* instP = childP->value.head; instP != NULL; instP = instP->next)
        attrLangReduce(instP, rangeV, ranges, faP);
    }
    else
    {
      attrLangReduce(childP, rangeV, ranges, faP);
    }
  }
}
