//===-- MCAsmInfoCOFF.cpp - COFF asm properties -----------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MCAsmInfoCOFF.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file defines target asm properties related what form asm statements  //
// should take in general on COFF-based targets                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/MC/MCAsmInfoCOFF.h"
using namespace llvm;

void MCAsmInfoCOFF::anchor() { }

MCAsmInfoCOFF::MCAsmInfoCOFF() {
  // MingW 4.5 and later support .comm with log2 alignment, but .lcomm uses byte
  // alignment.
  COMMDirectiveAlignmentIsInBytes = false;
  LCOMMDirectiveAlignmentType = LCOMM::ByteAlignment;
  HasDotTypeDotSizeDirective = false;
  HasSingleParameterDotFile = false;
  WeakRefDirective = "\t.weak\t";
  HasLinkOnceDirective = true;

  // Doesn't support visibility:
  HiddenVisibilityAttr = HiddenDeclarationVisibilityAttr = MCSA_Invalid;
  ProtectedVisibilityAttr = MCSA_Invalid;

  // Set up DWARF directives
  SupportsDebugInformation = true;
  NeedsDwarfSectionOffsetDirective = true;

  UseIntegratedAssembler = true;

  // FIXME: For now keep the previous behavior, AShr. Need to double-check
  // other COFF-targeting assemblers and change this if necessary.
  UseLogicalShr = false;
}

void MCAsmInfoMicrosoft::anchor() { }

MCAsmInfoMicrosoft::MCAsmInfoMicrosoft() {
}

void MCAsmInfoGNUCOFF::anchor() { }

MCAsmInfoGNUCOFF::MCAsmInfoGNUCOFF() {

}