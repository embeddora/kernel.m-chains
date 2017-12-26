/*
 * Copyright (c) 2018 [n/a] info@embeddora.com All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *        * Redistributions of source code must retain the above copyright
 *          notice, this list of conditions and the following disclaimer.
 *        * Redistributions in binary form must reproduce the above copyright
 *          notice, this list of conditions and the following disclaimer in the
 *          documentation and/or other materials provided with the distribution.
 *        * Neither the name of The Linux Foundation nor
 *          the names of its contributors may be used to endorse or promote
 *          products derived from this software without specific prior written
 *          permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.    IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Abstract: a CPP-primitive to deploy 'adconv' driver
 */



// Type ifstream, etc
#include <fstream>
// Macro PATH_MAX
#include <linux/limits.h>
// Memory page size, a constant throughout platform life
#define PLATF_PG_SIZE			4096
// Amount of memory (in 'pages') descending which a platform is (supposed to be) out of order
#define LOWER_PG_LIM			((( 1 ) * 1024 * 1024) / PLATF_PG_SIZE)
// Max amount of memory (in 'pages') we can afford for the test
#define UPPER_PG_LIM 			((( 256 - 16 ) * 1024 * 2014) / PLATF_PG_SIZE )
// Platform's physical memory limit (in 'pages') with few terminating MBytes subtracted
#define PLATF_EMC2 			((( 256 - 8 ) * 1024 * 2014) / PLATF_PG_SIZE )
// Filesystem entry to store result of <virt-to-phys> transformation
#define PROC_V2P_FILE			"/proc/_v2p"
// Full path to where the module placed during the filesystem build phase
#define SAT_ADCONV_KO "/usr/bin/sat-adconv/stressapptest-adconv.ko"



// sample function
uint64 fn(void *vaddr)
{
// Command line placeholder
char rgCmd[PATH_MAX];
// String placeholder
std::string line;

	// Prepare to access the module
	std::ifstream input(SAT_ADCONV_KO);

	// First, check if module is available
	if ( ! input.good() )
		// return untranslated address
		return (uint64) vaddr;

	// Now it's right time to install the module

	// Prepare module load command
	sprintf(rgCmd, "insmod "SAT_ADCONV_KO" vaddr=0x%08x",	(unsigned int)vaddr);
	// Ask OS to load the module
	system(rgCmd);

	// Repeat till sure that module is here
	while (1)
	{
		// Prepare filename to be checked for read-access
		std::ifstream input(PROC_V2P_FILE);
		//
		if ( input.good() )
			// Leave - the module is here
			break;
		else
			// Wait
			sleep(1);
	}

	// Repeat until module is really loaded
	while (1)
	{
		// Prepare filename to be checked for read-access
		std::ifstream input(PROC_V2P_FILE);
		// Check if module's product file is still here
		if ( input.good() )
		{
			// Module was loaded - take first line form its product file
			std::getline (input, line);
			// Make an 'unsigned long' of a string
			vaddr = (void*)atoi(line.c_str() );
			// Leave - the result is ready
			break;
		}
		else
			sleep(1);

	}

	// Prepare module unload command
	sprintf(rgCmd, "rmmod "SAT_ADCONV_KO);
	// Ask OS to unload the module
	system(rgCmd);

	// Repeat until module really goes away
	while (1)
	{
		// Prepare filename to be checked for read-access
		std::ifstream input(PROC_V2P_FILE);
		// If there's module's product file ...
		if ( input.good() )
			// ... then wait a bit for for module to get unloaded
			sleep(1);
		else
			// ... otherwise leave - no need to wait for module to get unloaded
			break;
	}

	// We're should not potter in here eternally
	return (uint64)vaddr;
}// end - sample function

int64 sysconf$(const int iName)
{
// Strings placeholders
std::string line, val;
// Position on which a space character is located in a string being parced
std::size_t space_pos;
// Return value
int iRes = 0;

	// Parse input parameter
	switch (iName)
	{
		// For the time, not distinguishing 'physical present' and 'av physical' (will be re-done in next review)
		case _SC_AVPHYS_PAGES:
		{
			// Based on assumption that 'nr_free_pages' is amount of free pages (virtual ones)
			std::ifstream input("/proc/vmstat");
			// Take first line from file (which on OpenWRT, platf.) behaves like regular text file
			std::getline (input, line);
			// Find position of a space character
			space_pos = line.find(" ");
			// Capture all what is behind a space character
			val = line.substr(space_pos);

			// Make an integer of c_str
			iRes = atoi(val.c_str() );

			// Outcome value should not exceed real limit of paged memory and should not be less be 0,5% of it
			if ((LOWER_PG_LIM < iRes) && ( UPPER_PG_LIM > iRes))
				// Return if within limits
				return iRes;
			else
				// Or let the original 'syscall()' to return wrong value (whatever it is), otherwise
				return sysconf(iName); 
		}

		case _SC_PHYS_PAGES:
		{
			// Based on assumption that 'nr_free_pages' is amount of free pages (virtual ones)
			std::ifstream input("/proc/meminfo");
			// Take first line from file (which on OpenWRT, platf.) behaves like regular text file
			std::getline (input, line);
			// Find position of a space character
			space_pos = line.find(" ");
			// Capture all what is behind a space character
			val = line.substr(space_pos);

			// Make an integer of c_str
			iRes = atoi(val.c_str() );
			// Transform KBytes the file has supplied into 'pages'
			iRes *= 1024, iRes /= PLATF_PG_SIZE;

			// Outcome value should not be negative and should not exceed platform's available physical memory
			if ((0 < iRes) && ( PLATF_EMC2 > iRes))
				// Return if within limits
				return iRes;
			else
				// Or let the original 'syscall()' to return wrong value (whatever it is), otherwise
				return sysconf(iName); 
		}

		default: 
			// The rest cases (by now we have none) to return wrong value by means original 'syscall(...)'
			return sysconf(iName); 
	}
}
// end - int64 sysconf$(const int iName)



#if defined(1)
  // (OpenWRT platf.) The original sysconf() fails on values _SC_PHYS_PAGES, _SC_AVPHYS_PAGES - let's use alternative one
  int64 pages = sysconf$(_SC_PHYS_PAGES);
  int64 avpages = sysconf$(_SC_AVPHYS_PAGES);
#else
  int64 pages = sysconf(_SC_PHYS_PAGES);
  int64 avpages = sysconf(_SC_AVPHYS_PAGES);
#endif // defined(M5_OR_NROAMING)

