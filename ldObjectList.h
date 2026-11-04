#ifndef CORNGSILD_LDOBJECTLIST_H_
#define CORNGSILD_LDOBJECTLIST_H_

//
// FILE            ldObjectList.h
//
// AUTHOR          Ken Zangelin
//
// Copyright 2026 Seamware
// SPDX-License-Identifier: Apache-2.0
//
#include "corAlloc/CorAlloc.h"                         // CorAlloc
#include "corTree/CorNode.h"                            // CorNode



// -----------------------------------------------------------------------------
//
// ldListInputReduce - the two input forms of a list, reduced to the one the broker stores
//
// TS 104-175 § 5.2.6.4.9: a ListRelationship's objectList holds, in the normalized form, objects
// {"object": <URI>} - or plain URI strings. Every {"object": <URI>} item becomes its URI.
//
// The core @context declares valueList and objectList "@container": "@list", so after expansion
// ["urn:ngsi-ld:null"] is the same as "urn:ngsi-ld:null" - the NGSI-LD Null. A list of that one
// element becomes the string.
//
// Walks the whole request body: entities, attribute fragments, batches, temporal evolutions.
//
extern void ldListInputReduce(CorNode* treeP);



// -----------------------------------------------------------------------------
//
// ldObjectListWrap - normalized output: every objectList and previousObjectList item, a URI string, as
//                    {"object": <URI>} (TS 104-175 § 5.2.6.4.9); the NGSI-LD Null of a deleted list, a bare
//                    "urn:ngsi-ld:null", as the list ["urn:ngsi-ld:null"] - valueList and objectList are lists
//
extern void ldObjectListWrap(CorNode* treeP, CorAlloc* allocP);



// -----------------------------------------------------------------------------
//
// ldObjectListUnwrap - the items of ONE objectList back to URI strings
//
extern void ldObjectListUnwrap(CorNode* listP);

#endif  // CORNGSILD_LDOBJECTLIST_H_
