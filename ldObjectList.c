//
// FILE            ldObjectList.c
//
// AUTHOR          Ken Zangelin
//
// Copyright 2026 Seamware
// SPDX-License-Identifier: Apache-2.0
//
#include <stddef.h>                                       // NULL
#include <string.h>                                       // strcmp

#include "corAlloc/CorAlloc.h"                           // CorAlloc
#include "corTree/CorNode.h"                              // CorNode
#include "corTree/corTreeBuilder.h"                       // corTreeString

#include "corNgsild/ldTermId.h"                           // ldTermId, CorTerm*
#include "corNgsild/ldObjectList.h"                       // Own interface



// -----------------------------------------------------------------------------
//
// isDataMember - a member whose value is the user's data, never walked into
//
// A Property's value, a JsonProperty's json, a ListProperty's valueList may hold objects with any
// keys - "objectList" among them - and none of those is the broker's to touch.
//
static bool isDataMember(CorNode* nodeP)
{
  CorTerm t = ldTermId(nodeP);

  return (t == CorTermValue) || (t == CorTermJson) || (t == CorTermValueList) || (t == CorTermLanguageMap) || (t == CorTermVocab);
}



// -----------------------------------------------------------------------------
//
// itemUri - the URI of an objectList item {"object": <URI>}, NULL if the item is not of that form
//
static CorNode* itemUri(CorNode* itemP)
{
  if (itemP->type != CorObject)
    return NULL;

  CorNode* memberP = itemP->value.head;

  if ((memberP == NULL) || (memberP->next != NULL))     // exactly one member
    return NULL;

  if ((ldTermId(memberP) != CorTermObject) || (memberP->type != CorString))
    return NULL;

  return memberP;
}



// -----------------------------------------------------------------------------
//
// ldObjectListUnwrap -
//
void ldObjectListUnwrap(CorNode* listP)
{
  if ((listP == NULL) || (listP->type != CorArray))
    return;

  for (CorNode* itemP = listP->value.head; itemP != NULL; itemP = itemP->next)
  {
    CorNode* uriP = itemUri(itemP);

    if (uriP != NULL)
    {
      itemP->type    = CorString;
      itemP->value.s = uriP->value.s;
    }
  }
}



// -----------------------------------------------------------------------------
//
// nullListReduce - a list of the one element "urn:ngsi-ld:null" is the NGSI-LD Null
//
static void nullListReduce(CorNode* listP)
{
  CorNode* onlyP = listP->value.head;

  if ((onlyP == NULL) || (onlyP->next != NULL) || (onlyP->type != CorString))
    return;

  if (strcmp(onlyP->value.s, "urn:ngsi-ld:null") != 0)
    return;

  listP->type    = CorString;
  listP->value.s = onlyP->value.s;
}



// -----------------------------------------------------------------------------
//
// ldListInputReduce -
//
void ldListInputReduce(CorNode* treeP)
{
  if ((treeP == NULL) || ((treeP->type != CorObject) && (treeP->type != CorArray)))
    return;

  for (CorNode* childP = treeP->value.head; childP != NULL; childP = childP->next)
  {
    if (childP->name != NULL)
    {
      CorTerm t = ldTermId(childP);

      if ((t == CorTermObjectList) && (childP->type == CorArray))
      {
        ldObjectListUnwrap(childP);
        nullListReduce(childP);
        continue;
      }

      if ((t == CorTermValueList) && (childP->type == CorArray))
      {
        nullListReduce(childP);
        continue;
      }

      if (isDataMember(childP))
        continue;
    }

    ldListInputReduce(childP);
  }
}



// -----------------------------------------------------------------------------
//
// ldObjectListWrap -
//
void ldObjectListWrap(CorNode* treeP, CorAlloc* allocP)
{
  if ((treeP == NULL) || ((treeP->type != CorObject) && (treeP->type != CorArray)))
    return;

  for (CorNode* childP = treeP->value.head; childP != NULL; childP = childP->next)
  {
    if (childP->name != NULL)
    {
      CorTerm t       = ldTermId(childP);
      bool    objList = (t == CorTermObjectList) || (t == CorTermPreviousObjectList);
      bool    valList = (t == CorTermValueList)  || (t == CorTermPreviousValueList);

      // the NGSI-LD Null of a deleted list: a list, as the member always is - ["urn:ngsi-ld:null"]
      if ((objList || valList) && (childP->type == CorString) && (strcmp(childP->value.s, "urn:ngsi-ld:null") == 0))
      {
        CorNode* nullP = corTreeString(allocP, NULL, childP->value.s);

        childP->type       = CorArray;
        childP->value.head = nullP;
        childP->value.tail = nullP;
        continue;
      }

      if (objList && (childP->type == CorArray))
      {
        for (CorNode* itemP = childP->value.head; itemP != NULL; itemP = itemP->next)
        {
          if (itemP->type != CorString)
            continue;

          if (strcmp(itemP->value.s, "urn:ngsi-ld:null") == 0)   // the Null, not a target object
            continue;

          CorNode* uriP = corTreeString(allocP, "object", itemP->value.s);

          itemP->type       = CorObject;
          itemP->value.head = uriP;
          itemP->value.tail = uriP;
        }
        continue;
      }

      if (valList || isDataMember(childP))
        continue;
    }

    ldObjectListWrap(childP, allocP);
  }
}
