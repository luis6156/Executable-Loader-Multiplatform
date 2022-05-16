/*
 * Loader Implementation
 *
 * 2018, Operating Systems
 */

#include <stdio.h>
#include <Windows.h>
#include <signal.h>

#define DLL_EXPORTS
#include "loader.h"
#include "exec_parser.h"

static so_exec_t *exec;
int page_size = 0x10000;
HANDLE fd;

/**
 * @brief Handles the SEG FAULT signal by assigning memory to the faulty page 
 * (on-demand paging).
 *
 * @param signum type of signal
 * @param info holds various information about the error
 * @param context represents the context of the error
 */
LONG CALLBACK access_violation(PEXCEPTION_POINTERS ExceptionInfo)
{
	unsigned int page_idx;
	char *page_addr, *mapped_addr;
	so_seg_t *segment;
	DWORD pointer_pos, old;
	BOOL bRet;
	int num_chars_read = 0;
	int len = page_size, i;

	// Iterate through all segments
	for (i = 0; i < exec->segments_no; ++i) {
		segment = &exec->segments[i];

		// Check if the current segment contains the faulty memory
		if ((unsigned int)ExceptionInfo->ExceptionRecord->ExceptionInformation[1] >= segment->vaddr &&
		    (unsigned int)ExceptionInfo->ExceptionRecord->ExceptionInformation[1] < segment->vaddr + segment->mem_size) {

			// Compute page index
			page_idx =
			    ((int)ExceptionInfo->ExceptionRecord->ExceptionInformation[1] - segment->vaddr) / page_size;

			if (segment->data == NULL) {
				// No page has been hit -> initialize array of
				// page hit
				segment->data = (int *)calloc(
				    segment->mem_size / page_size + 1,
				    sizeof(int));
				if (segment->data == NULL) {
					perror("calloc");
					exit(-1);
				}
			} else if (((int *)segment->data)[page_idx] == 1) {
				// Page was already hit -> assign the default
				// handler
				return EXCEPTION_CONTINUE_SEARCH;
			}

			// Get page address
			page_addr =
			    (char *)segment->vaddr + page_idx * page_size;

			// Map page address to the correct location in the file
			mapped_addr = (char *)VirtualAlloc(page_addr, page_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			if (mapped_addr == NULL) {
				CloseHandle(fd);
				perror("VirtualAlloc");
				exit(-1);
			}
			
			// Set cursor to the adequate position in file
			pointer_pos = SetFilePointer(fd, segment->offset + page_idx * page_size, NULL, SEEK_SET);
			if (pointer_pos == INVALID_SET_FILE_POINTER) {
				CloseHandle(fd);
				perror("SetFilePointer");
				exit(-1);
			}
			
			// Check if a read of page size crosses the file_size barrier
			if (segment->file_size < (page_idx + 1) * page_size) {
				len = segment->file_size - page_idx * page_size;
			}
			
			// Copy data from file to mapped region
			bRet = ReadFile(fd, mapped_addr, len, &num_chars_read, NULL);
			if (bRet == FALSE) {
				CloseHandle(fd);
				perror("ReadFile");
				exit(-1);
			}

			// Mark the page as mapped
			((int *)segment->data)[page_idx] = 1;
			
			// Set permissions accordingly
			if (segment->perm == 1)
				VirtualProtect(mapped_addr, page_size, PAGE_READONLY, &old);
			else if (segment->perm == 2)
				VirtualProtect(mapped_addr, page_size, PAGE_READWRITE, &old);
			else if (segment->perm == 3)
				VirtualProtect(mapped_addr, page_size, PAGE_READWRITE, &old);
			else if (segment->perm == 4)
				VirtualProtect(mapped_addr, page_size, PAGE_EXECUTE, &old);
			else if (segment->perm == 5)
				VirtualProtect(mapped_addr, page_size, PAGE_EXECUTE_READ, &old);
			else if (segment->perm == 6)
				VirtualProtect(mapped_addr, page_size, PAGE_EXECUTE_READWRITE, &old);
			else if (segment->perm == 7)
				VirtualProtect(mapped_addr, page_size, PAGE_EXECUTE_READWRITE, &old);

			return EXCEPTION_CONTINUE_EXECUTION;
		}
	}

	// Segment is unknown -> assign the default handler
	return EXCEPTION_CONTINUE_SEARCH;
}

/**
 * @brief Initialize on-demand loader
 *
 * @return int returns -1 if the signal was not processed
 */
int so_init_loader(void)
{
	// Call to action for the signal received
	LPVOID access_violation_handler = AddVectoredExceptionHandler(1, access_violation);
	if (access_violation_handler == NULL) {
		perror("AddVectoredExceptionHandler");
		exit(-1);
	}

	return -1;
}

/**
 * @brief
 *
 * @param path to the file to be opened
 * @param argv arguments for executable
 * @return int returns -1 on error
 */
int so_execute(char *path, char *argv[])
{
	// Get file Handle
	fd = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (fd == INVALID_HANDLE_VALUE) {
		perror("open file");
		exit(-1);
	}
	
	exec = so_parse_exec(path);
	if (!exec)
		return -1;

	so_start_exec(exec, argv);

	return -1;
}
