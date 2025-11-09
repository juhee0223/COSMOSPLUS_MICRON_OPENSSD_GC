//////////////////////////////////////////////////////////////////////////////////
// garbage_collection.h for Cosmos+ OpenSSD
// Copyright (c) 2017 Hanyang University ENC Lab.
// Contributed by Yong Ho Song <yhsong@enc.hanyang.ac.kr>
//				  Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
//
// This file is part of Cosmos+ OpenSSD.
//
// Cosmos+ OpenSSD is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3, or (at your option)
// any later version.
//
// Cosmos+ OpenSSD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Cosmos+ OpenSSD; see the file COPYING.
// If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Company: ENC Lab. <http://enc.hanyang.ac.kr>
// Engineer: Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
//
// Project Name: Cosmos+ OpenSSD
// Design Name: Cosmos+ Firmware
// Module Name: Garbage Collector
// File Name: garbage_collection.h
//
// Version: v1.0.0
//
// Description:
//   - define parameters, data structure and functions of garbage collector
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////


#ifndef GARBAGE_COLLECTION_H_
#define GARBAGE_COLLECTION_H_

#include "ftl_config.h"

/* -----------------------------------------------------------------------------
 * GC Policy
 * - Only the Cost-Benefit policy remains; switching at build time is removed.
 * ---------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/* Victim list structures (unchanged ABI) */
typedef struct _GC_VICTIM_LIST_ENTRY {
    unsigned int headBlock : 16;
    unsigned int tailBlock : 16;
} GC_VICTIM_LIST_ENTRY, *P_GC_VICTIM_LIST_ENTRY;

typedef struct _GC_VICTIM_MAP {
    GC_VICTIM_LIST_ENTRY gcVictimList[USER_DIES][SLICES_PER_BLOCK + 1];
} GC_VICTIM_MAP, *P_GC_VICTIM_MAP;

/* Public API (함수/이름 동일 유지) */
void InitGcVictimMap(void);
void GarbageCollection(unsigned int dieNo);

void PutToGcVictimList(unsigned int dieNo, unsigned int blockNo, unsigned int invalidSliceCnt);
unsigned int GetFromGcVictimList(unsigned int dieNo);
void SelectiveGetFromGcVictimList(unsigned int dieNo, unsigned int blockNo);

/* Public globals (기존 그대로) */
extern P_GC_VICTIM_MAP gcVictimMapPtr;
extern unsigned int gcTriggered;
extern unsigned int copyCnt;

#ifdef __cplusplus
}
#endif

#endif /* GARBAGE_COLLECTION_H_ */
