#ifndef CORNGSILD_LDORDERSORT_H_
#define CORNGSILD_LDORDERSORT_H_

//
// FILE            ldOrderSort.h
//
// AUTHOR          Ken Zangelin
//
// Copyright 2026 Seamware
// SPDX-License-Identifier: Apache-2.0
//
#include "corTree/CorNode.h"                            // CorNode
#include "corNgsild/LdOrder.h"                            // LdOrderTerm



// ldOrderSort - sort an entity array in-place per orderBy terms
//
// 'collation' is the § 7.6.2.3 collation= parameter (a BCP-47 tag, NULL = the
// § 7.6.2.1 default "root" order). It only affects string ordering: an ICU
// build honours it via ICU's collator, for every locale ICU knows; a non-ICU
// build orders by corBase's corCollate - root for ASCII, Latin-1 and Latin
// Extended-A, the sv and de-u-co-phonebk tailorings and the -u-kn key, root for
// any other tag.
extern void ldOrderSort(CorNode* arrayP, LdOrderTerm* terms, int termCount, const char* collation);

#endif  // CORNGSILD_LDORDERSORT_H_
