//
// FILE            ldSubStatus.c
//
// AUTHOR          Ken Zangelin
//
// Copyright 2026 Seamware
// SPDX-License-Identifier: Apache-2.0
//
#include <stddef.h>                                      // NULL
#include <string.h>                                      // strcmp

#include "corNgsild/LdSubStatus.h"                        // Own interface



// -----------------------------------------------------------------------------
//
// ldSubStatusFromString - unknown/NULL maps to active (the default, TS 104 175 § 5.2.6.5.2)
//
LdSubStatus ldSubStatusFromString(const char* s)
{
  if (s == NULL)                   return LdSubStatusActive;
  if (strcmp(s, "active")  == 0)   return LdSubStatusActive;
  if (strcmp(s, "paused")  == 0)   return LdSubStatusPaused;
  if (strcmp(s, "expired") == 0)   return LdSubStatusExpired;

  return LdSubStatusActive;
}



// -----------------------------------------------------------------------------
//
// ldSubStatusToString -
//
const char* ldSubStatusToString(LdSubStatus status)
{
  switch (status)
  {
    case LdSubStatusActive:   return "active";
    case LdSubStatusPaused:   return "paused";
    case LdSubStatusExpired:  return "expired";
  }

  return "active";
}
