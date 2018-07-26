/* This file coded by CRaFT4ik (Eldar Timraleev) */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"

void test_max_mem_malloc(void) 
{
	int *t; int counter = 0;
  	while (t = malloc(256)) counter++;
	
	msg("max size: %d bytes [256 * %d]", 256 * counter, counter);
	msg("max calls: %d", counter);
}

void test_max_mem_calloc(void) 
{
	int *t; int counter = 0;
  	while (t = calloc(128, sizeof(int))) counter++;
	
	msg("sizeof(int) = %d", sizeof(int));
	msg("max size: %d bytes [sizeof(int) * %d elements * %d]", sizeof(int) * 128 * counter, 128, counter);
	msg("max calls: %d", counter);
}

void test_max_mem_palloc(void) 
{
	int *t; int counter = 0;
	while (t = palloc_get_page(PAL_ZERO)) counter++; // kernel
	
	msg("max size: %d pages for kernel pool", counter);
	
	counter = 0;
	while (t = palloc_get_page(PAL_USER)) counter++; // user
	
	msg("max size: %d pages for user pool", counter);
}
