//////////////////////////////////////////////////////////////////////////////////
// nvme_main.c for Cosmos+ OpenSSD
// Copyright (c) 2016 Hanyang University ENC Lab.
// Contributed by Yong Ho Song <yhsong@enc.hanyang.ac.kr>
//				  Youngjin Jo <yjjo@enc.hanyang.ac.kr>
//				  Sangjin Lee <sjlee@enc.hanyang.ac.kr>
//				  Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
//				  Kibin Park <kbpark@enc.hanyang.ac.kr>
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
// Engineer: Sangjin Lee <sjlee@enc.hanyang.ac.kr>
//			 Jaewook Kwak <jwkwak@enc.hanyang.ac.kr>
//			 Kibin Park <kbpark@enc.hanyang.ac.kr>
//
// Project Name: Cosmos+ OpenSSD
// Design Name: Cosmos+ Firmware
// Module Name: NVMe Main
// File Name: nvme_main.c
//
// Version: v1.2.0
//
// Description:
//   - initializes FTL and NAND
//   - handles NVMe controller
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.2.0
//   - header file for buffer is changed from "ia_lru_buffer.h" to "lru_buffer.h"
//   - Low level scheduler execution is allowed when there is no i/o command
//
// * v1.1.0
//   - DMA status initialization is added
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////

#include "xil_printf.h" // For printing debug messages
#include "debug.h" // Debugging utilities
#include "io_access.h" // IO access functions

#include "nvme.h" // NVMe-specific definitions
#include "host_lld.h" // Host low-level driver
#include "nvme_main.h" // NVMe main function declaration
#include "nvme_admin_cmd.h" // NVMe admin command handling
#include "nvme_io_cmd.h" // NVMe IO command handling

#include "../memory_map.h" // Memory map definitions

// Global NVMe task context
volatile NVME_CONTEXT g_nvmeTask;

// Main NVMe function
void nvme_main()
{
    unsigned int exeLlr; // Flag to execute low-level requests
    unsigned int rstCnt = 0; // Reset counter

    xil_printf("!!! Wait until FTL reset complete !!! \r\n");	//xilinx console print

    // Initialize the Flash Translation Layer (FTL)
    InitFTL();

    xil_printf("\r\nFTL reset complete!!! \r\n");	//xilinx console print
    xil_printf("Turn on the host PC \r\n");			//xilinx console print

    while(1) // Infinite loop to handle NVMe tasks
    {
        exeLlr = 1; // Default to execute low-level requests

        // Check NVMe task status
        if(g_nvmeTask.status == NVME_TASK_WAIT_CC_EN)
        {
            unsigned int ccEn;
            ccEn = check_nvme_cc_en(); // Check if NVMe controller is enabled
            if(ccEn == 1)	// If enabled
            {
                set_nvme_admin_queue(1, 1, 1); // Initialize admin queue
                set_nvme_csts_rdy(1); // Set controller ready status
                g_nvmeTask.status = NVME_TASK_RUNNING; // Update task status
                xil_printf("\r\nNVMe ready!!!\r\n");
            }
        }
        else if(g_nvmeTask.status == NVME_TASK_RUNNING)	// host interface layer (logic) - parse host nvme requests including ...admin and io
        {
            NVME_COMMAND nvmeCmd; // NVMe command structure
            unsigned int cmdValid;
            cmdValid = get_nvme_cmd(&nvmeCmd.qID, &nvmeCmd.cmdSlotTag, &nvmeCmd.cmdSeqNum, nvmeCmd.cmdDword); // Fetch NVMe command
            if(cmdValid == 1)
            {
                rstCnt = 0; // Reset the counter
                if(nvmeCmd.qID == 0)
                {
                    handle_nvme_admin_cmd(&nvmeCmd); // Handle admin command - identify, set/get features, etc.
                }
                else
                {
                    handle_nvme_io_cmd(&nvmeCmd); // Handle IO command - read/write, flush, TRIM, etc.
                    ReqTransSliceToLowLevel(); // Request translation to low-level
                    exeLlr = 0; // Skip low-level execution
                }
            }
        }
        else if(g_nvmeTask.status == NVME_TASK_SHUTDOWN)
        {
            NVME_STATUS_REG nvmeReg;
            nvmeReg.dword = IO_READ32(NVME_STATUS_REG_ADDR); // Read NVMe status register
            if(nvmeReg.ccShn != 0)
            {
                unsigned int qID;
                set_nvme_csts_shst(1); // Set shutdown status

                // Clear IO queues
                for(qID = 0; qID < 8; qID++)
                {
                    set_io_cq(qID, 0, 0, 0, 0, 0, 0); // Clear completion queue
                    set_io_sq(qID, 0, 0, 0, 0, 0); // Clear submission queue
                }

                set_nvme_admin_queue(0, 0, 0); // Clear admin queue
                g_nvmeTask.cacheEn = 0; // Disable cache
                set_nvme_csts_shst(2); // Update shutdown status
                g_nvmeTask.status = NVME_TASK_WAIT_RESET; // Update task status

                // Flush bad block information
                UpdateBadBlockTableForGrownBadBlock(RESERVED_DATA_BUFFER_BASE_ADDR);

                xil_printf("\r\nNVMe shutdown!!!\r\n");
            }
        }
        else if(g_nvmeTask.status == NVME_TASK_WAIT_RESET)
        {
            unsigned int ccEn;
            ccEn = check_nvme_cc_en(); // Check if controller is disabled
            if(ccEn == 0)
            {
                g_nvmeTask.cacheEn = 0; // Disable cache
                set_nvme_csts_shst(0); // Clear shutdown status
                set_nvme_csts_rdy(0); // Clear ready status
                g_nvmeTask.status = NVME_TASK_IDLE; // Update task status
                xil_printf("\r\nNVMe disable!!!\r\n");
            }
        }
        else if(g_nvmeTask.status == NVME_TASK_RESET)
        {
            unsigned int qID;
            for(qID = 0; qID < 8; qID++)
            {
                set_io_cq(qID, 0, 0, 0, 0, 0, 0); // Clear completion queue
                set_io_sq(qID, 0, 0, 0, 0, 0); // Clear submission queue
            }

            if (rstCnt >= 5)
            {
                pcie_async_reset(rstCnt); // Perform PCIe asynchronous reset
                rstCnt = 0; // Reset the counter
                xil_printf("\r\nPcie link disable!!!\r\n");
                xil_printf("Wait few minute or reconnect the PCIe cable\r\n");
            }
            else
                rstCnt++;

            g_nvmeTask.cacheEn = 0; // Disable cache
            set_nvme_admin_queue(0, 0, 0); // Clear admin queue
            set_nvme_csts_shst(0); // Clear shutdown status
            set_nvme_csts_rdy(0); // Clear ready status
            g_nvmeTask.status = NVME_TASK_IDLE; // Update task status

            xil_printf("\r\nNVMe reset!!!\r\n");
        }

        // Execute low-level requests if applicable
        if(exeLlr && ((nvmeDmaReqQ.headReq != REQ_SLOT_TAG_NONE) || notCompletedNandReqCnt || blockedReqCnt))
        {
            CheckDoneNvmeDmaReq(); // Check completed NVMe DMA requests
            SchedulingNandReq(); // Schedule NAND requests
        }
    }
}


