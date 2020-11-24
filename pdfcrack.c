/**
 * Copyright (C) 2006-2019 Henning Norén
 * Copyright (C) 1996-2005 Glyph & Cog, LLC.
 * 
 * Multi-core (OpenMP) and pattern support by 
 *       Shreepad Shukla, Copyright (C) 2015
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, 
 * USA.
 *
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <omp.h>
#include "pdfcrack.h"
#include "md5.h"
#include "rc4.h"
#include "sha256.h"
#include "passwords.h"

/** sets the number of bytes to decrypt for partial test in revision 3.
    Three should be a good number for this as this mean a match should only
    happen every 256^3=16777216 check and that should be unique enough to 
    motivate a full retry on that entry.
 */
#define PARTIAL_TEST_SIZE 3

// Stripe size for multi-thread, default suitable for regular PCs, adjust upwards for high power processors
#define STRIPE_SIZE 10000

static const uint8_t
pad[32] = {
  0x28,0xBF,0x4E,0x5E,0x4E,0x75,0x8A,0x41,
  0x64,0x00,0x4E,0x56,0xFF,0xFA,0x01,0x08,
  0x2E,0x2E,0x00,0xB6,0xD0,0x68,0x3E,0x80,
  0x2F,0x0C,0xA9,0xFE,0x64,0x53,0x69,0x7A
};

/** buffers for stuff that we can precompute before the actual cracking */
static uint8_t *encKeyWorkSpace;
static uint8_t password_user[40];  /** need to cover 32 char + salt */
static const uint8_t *rev3TestKey;
static unsigned int ekwlen;

/** points to the current password in clear-text */
static uint8_t *currPW;
/** current length of the password we are working with */
static unsigned int currPWLen;


/** points to the current pattern password index */
static unsigned long long int currPatternPasswordIndex;

/** points to the current pattern password index */
static unsigned long long int finalPatternPasswordIndex;


/** statistics */
static unsigned int nrprocessed;
static time_t startTime;

/** pointer to the actual encoding-data from the pdf */
static const EncData *encdata;

/** some configuration switches */
static bool crackDone;
static bool knownPassword;
static bool workWithUser;

static passwordMethod passMeth;
static int numThreads;

/** Print out some statistics */
bool
printProgress(void) {
  time_t currentTime;
  char str[33];

  if(crackDone)
    return true;

  currentTime = time(NULL);
  memcpy(str,currPW,currPWLen);
  str[currPWLen] = '\0';
  printf("Average Speed: %.1f w/s. ",
	 nrprocessed/difftime(currentTime,startTime));
	 
  if (passMeth != Pattern)
  {
  	printf("Current Word: '%s'\n",str);
  }
  else
  {
  	uint8_t currPatternPassword[PASSLENGTH+1];
			
	if (getPatternPassword(currPatternPasswordIndex, currPatternPassword))
		printf("Current Pattern Word: '%s'\n",currPatternPassword);
  }
  
  fflush(stdout);
  nrprocessed = 0;
  startTime = time(NULL);
  return false;
}

/**
 * Initialisation of the encryption key workspace to manage a bit faster 
 * switching between keys
 */
static unsigned int
initEncKeyWorkSpace(const int revision, const bool encMetaData,
		    const int permissions, const uint8_t *ownerkey,
		    const uint8_t *fileID, const unsigned int fileIDLen) {
  unsigned int size;

  /** 
   *   Algorithm 3.2 Computing an encryption key (PDF Reference, v 1.7, p.125)
   *
   *   Make space for:
   *   field           | bytes
   *   -----------------------
   *   padded password | 32
   *   O entry         | 32
   *   P entry         |  4
   *   fileID          | <fileIDLEn>
   *  [extra padding]  | [4] (Special for step 6)
   **/
  size = (revision >= 3 && !encMetaData) ? 72 : 68;
  encKeyWorkSpace = malloc(size + fileIDLen);

  /** Just to be sure we have no uninitalized stuff in the workspace */
  memcpy(encKeyWorkSpace, pad, 32);

  /** 3 */ 
  memcpy(encKeyWorkSpace + 32, ownerkey, 32);

  /** 4 */
  encKeyWorkSpace[64] = permissions & 0xff;
  encKeyWorkSpace[65] = (permissions >> 8) & 0xff;
  encKeyWorkSpace[66] = (permissions >> 16) & 0xff;
  encKeyWorkSpace[67] = (permissions >> 24) & 0xff;

  /** 5 */
  memcpy(encKeyWorkSpace + 68, fileID, fileIDLen);

  /** 6 */
  if(revision >= 3 && !encMetaData) {
    encKeyWorkSpace[68+fileIDLen] = 0xff;
    encKeyWorkSpace[69+fileIDLen] = 0xff;
    encKeyWorkSpace[70+fileIDLen] = 0xff;
    encKeyWorkSpace[71+fileIDLen] = 0xff;
  }

  return size+fileIDLen;
}

#if 0
/** For debug */
static void
printHexString(const uint8_t *str, const unsigned int len) {
  unsigned int i;
  for(i=0;i<len;i++)
    printf("%x ",str[i]);
  printf("\n");
}

static void
printString(const uint8_t *str, const unsigned int len) {
  unsigned int i;
  for(i=0;i<len;i++)
    printf("%d ",str[i]);
  printf("\n");
}
#endif

/** toupper(3) but expanded to handle iso-latin-1 characters */
static uint8_t
isolat1ToUpper(const uint8_t b) {
  if(unlikely(b >= 0xe0 && b <= 0xf6))
    return b-0x20;
  else
    return toupper(b);
}

/** Really stupid permutate that needs to be replaced with a better framwork
    for smart permutations of the current password */
static bool
do_permutate(void) {
  static bool ret = false;
  uint8_t tmp;

  tmp = isolat1ToUpper(currPW[0]);
  if(tmp != currPW[0]) {
    currPW[0] = tmp;
    ret = !ret;
  }
  else
    ret = false;  

  return ret;
}

/** Dummy-function to use when no permutations are wanted */
static bool
no_permutate(void) { return false; }

/** Placeholder for the correct permutation-function to run */
static bool (*permutate)() = NULL;

/** Prints out the password found */
static void
foundPassword(void) {

  // Common vars for user-password
  int fin_search;
  size_t pad_start;
  char str[33];

  if (passMeth != Pattern)
  {
	  // Wordlist or charset

	  memcpy(str,currPW,currPWLen);
	  str[currPWLen] = '\0';
	  printf("found %s-password: '%s'\n", workWithUser?"user":"owner", str);

  }
  else
  {
  	// Pattern method
	uint8_t patternPassword[PASSLENGTH+1];
			
	if (getPatternPassword(finalPatternPasswordIndex, patternPassword))
	{
		printf("Found pattern password: %s\n", (char *) patternPassword);
	}
	else
	{
		printf("Could not find pattern password\n");
	}
  	
  }
	
  /** 
   * Print out the user-password too if we know the ownerpassword.
   * It is placed in password_user and we need to find where the pad
   * starts before we can print it out (without ugly artifacts) 
   **/
    
  if(!workWithUser && (encdata->revision < 5)) {
    fin_search=-1;
    pad_start=0;

    do {
      fin_search = memcmp(password_user+pad_start, pad, 32-pad_start);
      pad_start++;
    } while (pad_start < 32 && fin_search != 0);

    memcpy(str, password_user, pad_start);
    if(!fin_search)
      str[pad_start-1] = '\0';
    printf("found user-password: '%s'\n", str);
  }
 
}


/** Common handling of the key for all rev3-functions */
#define RC4_DECRYPT_REV3(n) {			\
    for(i = 19; i >= 0; --i) {			\
      for(j = 0; j < length; ++j)		\
	tmpkey[j] = enckey[j] ^ i;		\
      rc4Decrypt(tmpkey, test, n, test);	\
    }						\
  }
  

/** Common handling of the key for all rev3-functions */
// Duplicated and edited for handling user specific block in multi thread mode
#define RC4_DECRYPT_REV3_USER(n) {			\
    for(useri = 19; useri >= 0; --useri) {			\
      for(userj = 0; userj < userlength; ++userj)		\
	usertmpkey[userj] = userenckey[userj] ^ useri;		\
      rc4Decrypt(usertmpkey, usertest, n, usertest);	\
    }						\
  }
  

/** Checks if the rev2-password set up in encKeyWorkSpace is the correct one
    and return true if it is and false otherwise.
*/
static bool
isUserPasswordRev2(void) {
  uint8_t enckey[16];

  md5(encKeyWorkSpace, ekwlen, enckey);

  return rc4Match40b(enckey, encdata->u_string, pad);
}

/** Checks if the rev3-password set up in encKeyWorkSpace is the correct one
    and return true if it is and false otherwise.
*/
static bool
isUserPasswordRev3(void) {
  uint8_t test[16], enckey[16], tmpkey[16];
  unsigned int length, j;
  int i;

  length = encdata->length/8;
  md5(encKeyWorkSpace, ekwlen, enckey);

  md5_50(enckey, length);

  memcpy(test, encdata->u_string, PARTIAL_TEST_SIZE);

  /** Algorithm 3.5 reversed */
  RC4_DECRYPT_REV3(PARTIAL_TEST_SIZE);

  /** if partial test succeeds we make a full check to be sure */
  if(unlikely(memcmp(test, rev3TestKey, PARTIAL_TEST_SIZE) == 0)) {
    memcpy(test, encdata->u_string, length);
    RC4_DECRYPT_REV3(length);
    if(memcmp(test, rev3TestKey, length) == 0)
      return true;
  }
  return false;
}

/** Common beginning of the main-loop in all the cracking-functions */
#define BEGIN_CRACK_LOOP() {				\
    currPWLen = setPassword(currPW);			\
    if(unlikely(lpasslength != currPWLen)) {		\
      if(likely(currPWLen < 32))			\
	memcpy(currPW + currPWLen, pad, 32-currPWLen);	\
      lpasslength = currPWLen;				\
    }							\
  }

bool
runCrackRev2_o(void) {

  printf("Entered runCrackRev2_o\n");
  startTime = time(NULL);

  if (passMeth != Pattern)
  {
  	  // Generative or Wordlist
  	
	  uint8_t enckey[16];
	  unsigned int lpasslength;

	  lpasslength = 0;
	  
	  do {
	    BEGIN_CRACK_LOOP();
	  
	    do {
	      md5(currPW, 32, enckey);

	      rc4Decrypt(enckey, encdata->o_string, 32, encKeyWorkSpace);
	      md5(encKeyWorkSpace, ekwlen, enckey);
	      if(rc4Match40b(enckey, encdata->u_string, pad)) {
		memcpy(password_user, encKeyWorkSpace, 32);
		return true;
	      }

	      ++nrprocessed;
	    } while(permutate());
	  } while(nextPassword());
  }
  else
  {
  	// Pattern method

	omp_set_num_threads(numThreads);
  	
  	unsigned long long int outerindex, innerindex, lastInvalidPasswordIndex, foundPasswordIndex, numberOfPaswords, lastStripeStartIndex;
  	
  	numberOfPaswords = getMaxPatternPasswords();
  	
  	if (numberOfPaswords > STRIPE_SIZE)
	  	lastStripeStartIndex = numberOfPaswords - (numberOfPaswords % STRIPE_SIZE);
	else
		lastStripeStartIndex = numberOfPaswords-1;
  	
  	lastInvalidPasswordIndex = 0LL;
  	foundPasswordIndex = 0LL;
 	
 	// Thread specific variables
 	
	uint8_t localEncKeyWorkSpace[ekwlen];

	uint8_t enckey[16];
	
  	int threadId;
  	uint8_t currentPatternPassword[PASSLENGTH+1];	// Holds pattern password of given index
  	int currentPatternPasswordLength;

  	for (outerindex = 0; outerindex <= lastStripeStartIndex ; outerindex += STRIPE_SIZE)
  	{
  	
  		// Parrallised check for given stripe  		
  	
  		#pragma omp parallel for    \
  			private(innerindex, threadId, enckey, localEncKeyWorkSpace, \
  				currentPatternPassword, currentPatternPasswordLength) \
  			shared(outerindex, lastInvalidPasswordIndex, foundPasswordIndex)
		for (innerindex = outerindex; innerindex < outerindex+STRIPE_SIZE ; innerindex++)
		{
			// Get the index password
			currentPatternPasswordLength = 	getPatternPassword(innerindex, currentPatternPassword);		
			if (currentPatternPasswordLength)
			{

				threadId = omp_get_thread_num();
				//printf("Thread %i: Password %lli: %s, Length %i\n", 
				//	threadId, innerindex, currentPatternPassword, currentPatternPasswordLength);
				
				//currPWLen = setPassword(currPW);
				// This should only be done the first time around as initialisation
				memcpy(localEncKeyWorkSpace, encKeyWorkSpace, ekwlen);
				memcpy(localEncKeyWorkSpace, currentPatternPassword, currentPatternPasswordLength);

				//if(unlikely(lpasslength != currPWLen)) 
				{		
					if(likely(currentPatternPasswordLength < 32))			
						memcpy(localEncKeyWorkSpace + currentPatternPasswordLength, pad, \
							32-currentPatternPasswordLength);	
				   // lpasslength = currPWLen;				
				}

				//md5(currPW, 32, enckey);
				md5(localEncKeyWorkSpace, 32, enckey);
				
				//rc4Decrypt(enckey, encdata->o_string, 32, encKeyWorkSpace);
				rc4Decrypt(enckey, encdata->o_string, 32, localEncKeyWorkSpace);
				
				//md5(encKeyWorkSpace, ekwlen, enckey);
				md5(localEncKeyWorkSpace, ekwlen, enckey);

				//if(rc4Match40b(enckey, encdata->u_string, pad))
				
				if(rc4Match40b(enckey, encdata->u_string, pad))
				{
				#pragma omp critical
				   {
					foundPasswordIndex = innerindex;
					printf("Found it at %lli !!!! *****\n", innerindex);
					
					//memcpy(password_user, encKeyWorkSpace, 32)
					memcpy(password_user, localEncKeyWorkSpace, 32);
				   }				
				}

			}   // if valid currentPatternPassword	
			
		}  // for innerindex
		
		nrprocessed += STRIPE_SIZE;
		
		// Assumed that first pattern is not the password!!!!
		
		if (foundPasswordIndex)
		{
			finalPatternPasswordIndex = foundPasswordIndex;
			return true;
		}
		else
			currPatternPasswordIndex = outerindex+STRIPE_SIZE-1;
			
	}  // for outerindex
  	
  	
  }
  
  return false;
}

bool
runCrackRev3_o(void) {

  printf("Entered runCrackRev3_o\n");
  startTime = time(NULL);
  
  if (passMeth != Pattern)
  {
  	// Wordlist or Generative
  	
	  uint8_t test[32], enckey[16], tmpkey[16];
	  unsigned int j, length, lpasslength;
	  int i;

	  length = encdata->length/8;
	  lpasslength = 0;

	  do {
	    BEGIN_CRACK_LOOP();

	    do {
	      md5(currPW, 32, enckey);

	      md5_50(enckey, length);

	      memcpy(test, encdata->o_string, 32);
	      RC4_DECRYPT_REV3(32);
	      memcpy(encKeyWorkSpace, test, 32);

	      if(isUserPasswordRev3()) {
		memcpy(password_user, encKeyWorkSpace, 32);
		return true;
	      }

	      ++nrprocessed;
	    } while(permutate());
	  } while(nextPassword());
  }
  else
  {
	// Pattern method
	
	omp_set_num_threads(numThreads);
  	
  	unsigned long long int outerindex, innerindex, lastInvalidPasswordIndex, foundPasswordIndex, numberOfPaswords, lastStripeStartIndex;
  	
  	numberOfPaswords = getMaxPatternPasswords();
  	
	if (numberOfPaswords > STRIPE_SIZE)
	  	lastStripeStartIndex = numberOfPaswords - (numberOfPaswords % STRIPE_SIZE);
	else
		lastStripeStartIndex = numberOfPaswords-1;
  	
  	lastInvalidPasswordIndex = 0LL;
  	foundPasswordIndex = 0LL;
  	
  	
  	unsigned int j, length;
	int i;
	length = encdata->length/8;
  	
  	// Private fields per thread
  	
  	int threadId;
  	uint8_t currentPatternPassword[PASSLENGTH+1];	// Holds pattern password of given index
  	int currentPatternPasswordLength;
  	
  	uint8_t enckey[16];     // Holds md5 digest
  	uint8_t test[16];	// Holds test key
  	uint8_t tmpkey[16]; 	// Not sure what this does
  	
  	// thread's local copy of global variables
	uint8_t localEncKeyWorkSpace[ekwlen];
	
	// No point initialising here - will get blanked out in parallel section
	// memcpy(localEncKeyWorkSpace, encKeyWorkSpace, ekwlen);


	// Threads pvt copy of user function variables
	uint8_t usertest[16], userenckey[16], usertmpkey[16];
	unsigned int userlength, userj;
	int useri;
  	
  	
  	for (outerindex = 0; outerindex <= lastStripeStartIndex ; outerindex += STRIPE_SIZE)
  	{
  	
  		// Parrallised check for given stripe  		
  	
  		#pragma omp parallel for    \
  			private(innerindex, i, j, threadId, test, enckey, tmpkey, localEncKeyWorkSpace,    \
  				usertest, userenckey, usertmpkey, userlength, useri, userj,                \
  				currentPatternPassword, currentPatternPasswordLength)                      \
  			shared(outerindex, lastInvalidPasswordIndex, foundPasswordIndex)
		for (innerindex = outerindex; innerindex < outerindex+STRIPE_SIZE ; innerindex++)
		{
			// Get the index password
			currentPatternPasswordLength = 	getPatternPassword(innerindex, currentPatternPassword);		
			if (currentPatternPasswordLength)
			{
  
  				threadId = omp_get_thread_num();
				//printf("Thread %i: Password %lli: %s, Length %i\n",  
				//	threadId, innerindex, currentPatternPassword, currentPatternPasswordLength);
				
				// Check if password at index innerindex is the correct one
				// BEGIN_CRACK_LOOP
				
				//currPWLen = setPassword(currPW);
				// This should only be done the first time around as initialisation
				memcpy(localEncKeyWorkSpace, encKeyWorkSpace, ekwlen);
				memcpy(localEncKeyWorkSpace, currentPatternPassword, currentPatternPasswordLength);
			    			
				//if(unlikely(lpasslength != currPWLen)) 
				{		
					if(likely(currentPatternPasswordLength < 32))			
						memcpy(localEncKeyWorkSpace + currentPatternPasswordLength, pad, \
							32-currentPatternPasswordLength);	
				   // lpasslength = currPWLen;				
				}							

				md5(localEncKeyWorkSpace, 32, enckey);

				md5_50(enckey, length);

				memcpy(test, encdata->o_string, 32);
				RC4_DECRYPT_REV3(32);
				
/* Common handling of the key for all rev3-functions
#define RC4_DECRYPT_REV3(n) {			\
    for(i = 19; i >= 0; --i) {			\
      for(j = 0; j < length; ++j)		\
	tmpkey[j] = enckey[j] ^ i;		\
      rc4Decrypt(tmpkey, test, n, test);	\
    }						\
  }
*/
				
				memcpy(localEncKeyWorkSpace, test, 32);
				
/*
				if(isUserPasswordRev3()) {
					memcpy(password_user, encKeyWorkSpace, 32);
					return true;
				}
*/
				// Converted user password function call into block to avoid threading issues
				// Renamed variables to avoid conflict
				{

					userlength = encdata->length/8;
					md5(localEncKeyWorkSpace, ekwlen, userenckey);

					md5_50(userenckey, userlength);

					memcpy(usertest, encdata->u_string, PARTIAL_TEST_SIZE);

					/** Algorithm 3.5 reversed */
					// Replaced macro to use user-specific variables
					RC4_DECRYPT_REV3_USER(PARTIAL_TEST_SIZE);
					

					/** if partial test succeeds we make a full check to be sure */
					if(unlikely(memcmp(usertest, rev3TestKey, PARTIAL_TEST_SIZE) == 0)) {
						memcpy(usertest, encdata->u_string, userlength);
						RC4_DECRYPT_REV3_USER(userlength);
						if(memcmp(usertest, rev3TestKey, userlength) == 0)
						{
							// Record success
						   #pragma omp critical
							{
								foundPasswordIndex = innerindex;
								printf("Found it at %lli !!!! *****\n", innerindex);
								memcpy(password_user, localEncKeyWorkSpace, 32);
							}							
							// memcpy(password_user, encKeyWorkSpace, 32);
							
							
						}
					}

				}   // end converted function block
				
			} // end if current password validation
			
		}  // end for parallel innerindex
		
		nrprocessed += STRIPE_SIZE;
		
		// Check if the password has been found
		// Assumed that first pattern is not the password!!!!
		
		if (foundPasswordIndex)
		{
			finalPatternPasswordIndex = foundPasswordIndex;
			return true;
		}
		else
			currPatternPasswordIndex = outerindex+STRIPE_SIZE-1;

		
	}  // end for outerindex
	
  }  // end else password method

  return false;
}

bool
runCrackRev2_of(void) {

  printf("Entered runCrackRev2_of\n");

  uint8_t enckey[16];
  unsigned int lpasslength;

  lpasslength = 0;
  startTime = time(NULL);
  do {
    BEGIN_CRACK_LOOP();

    do {
      md5(encKeyWorkSpace, 32, enckey);

      /* Algorithm 3.4 reversed */
      if(rc4Match40b(enckey, encdata->o_string, password_user))
	return true;

      ++nrprocessed;
    } while(permutate());
  } while(nextPassword());
  return false;
}

bool
runCrackRev3_of(void) {

  printf("Entered runCrackRev3_of\n");
  
  uint8_t test[32], enckey[16], tmpkey[16];
  unsigned int j, length, lpasslength;
  int i;

  length = encdata->length/8;
  lpasslength = 0;
  startTime = time(NULL);
  do {
    BEGIN_CRACK_LOOP();

    do {
      md5(encKeyWorkSpace, 32, enckey);

      md5_50(enckey, length);

      memcpy(test, encdata->o_string, PARTIAL_TEST_SIZE);
      RC4_DECRYPT_REV3(PARTIAL_TEST_SIZE);

      /** if partial test succeeds we make a full check to be sure */
      if(unlikely(memcmp(test, password_user, PARTIAL_TEST_SIZE) == 0)) {
	memcpy(test, encdata->o_string, 32);
	RC4_DECRYPT_REV3(32);
	if(memcmp(test, password_user, 32) == 0)
	  return true;
      }

      ++nrprocessed;
    } while(permutate());
  } while(nextPassword());
  return false;
}

bool
runCrackRev3(void) {

  printf("Entered runCrackRev3\n");
  startTime = time(NULL);
  
  
  if (passMeth != Pattern)
  {
	// Wordlist or Generative  - yes this is the original
  
	  uint8_t test[16], enckey[16], tmpkey[16];
	  unsigned int j, length, lpasslength;
	  int i;

	  length = encdata->length/8;
	  lpasslength = 0;

	  do {
	    BEGIN_CRACK_LOOP();

	    do {
	      md5(encKeyWorkSpace, ekwlen, enckey);

	      md5_50(enckey, length);
	      memcpy(test, encdata->u_string, PARTIAL_TEST_SIZE);

	      /** Algorithm 3.5 reversed */
	      RC4_DECRYPT_REV3(PARTIAL_TEST_SIZE);

	      /** if partial test succeeds we make a full check to be sure */
	      if(unlikely(memcmp(test, rev3TestKey, PARTIAL_TEST_SIZE) == 0)) {
		memcpy(test, encdata->u_string, length);
		RC4_DECRYPT_REV3(length);
		if(memcmp(test, rev3TestKey, length) == 0)
		  return true;
	      }

	      ++nrprocessed;
	    } while(permutate());
	  } while(nextPassword());
  }
  else
  {
  	// Pattern
  	
	omp_set_num_threads(numThreads);
  	
  	unsigned long long int outerindex, innerindex, lastInvalidPasswordIndex, foundPasswordIndex, numberOfPaswords, lastStripeStartIndex;
  	
  	numberOfPaswords = getMaxPatternPasswords();
  	
  	if (numberOfPaswords > STRIPE_SIZE)
	  	lastStripeStartIndex = numberOfPaswords - (numberOfPaswords % STRIPE_SIZE);
	else
		lastStripeStartIndex = numberOfPaswords-1;
  	
  	lastInvalidPasswordIndex = 0LL;
  	foundPasswordIndex = 0LL;
  	
  	
  	unsigned int j, length;
	int i;
	length = encdata->length/8;

  	
  	// Private fields per thread
  	
  	int threadId;
  	uint8_t currentPatternPassword[PASSLENGTH+1];	// Holds pattern password of given index
  	int currentPatternPasswordLength;
  	
  	uint8_t enckey[16];     // Holds md5 digest
  	uint8_t test[16];	// Holds test key
  	uint8_t tmpkey[16]; 	// Not sure what this does
  	
  	// thread's local copy of global variables
	uint8_t localEncKeyWorkSpace[ekwlen];
	
	// No point initialising here - will get blanked out in parallel section
	// memcpy(localEncKeyWorkSpace, encKeyWorkSpace, ekwlen);
  	
  	
  	for (outerindex = 0; outerindex <= lastStripeStartIndex ; outerindex += STRIPE_SIZE)
  	{
  	
  		// Parrallised check for given stripe  		
  	
  		#pragma omp parallel for    \
  			private(innerindex, i, j, threadId, test, enckey, tmpkey, localEncKeyWorkSpace, \
  				currentPatternPassword, currentPatternPasswordLength) \
  			shared(outerindex, lastInvalidPasswordIndex, foundPasswordIndex)
		for (innerindex = outerindex; innerindex < outerindex+STRIPE_SIZE ; innerindex++)
		{
			// Get the index password
			currentPatternPasswordLength = 	getPatternPassword(innerindex, currentPatternPassword);		
			if (currentPatternPasswordLength)
			{

				threadId = omp_get_thread_num();
				//printf("Thread %i: Password %lli: %s, Length %i\n", 
				//	threadId, innerindex, currentPatternPassword, currentPatternPasswordLength);
				
				// Check if password at index innerindex is the correct one
				// BEGIN_CRACK_LOOP
				
				//currPWLen = setPassword(currPW);
				// This should only be done the first time around as initialisation
				memcpy(localEncKeyWorkSpace, encKeyWorkSpace, ekwlen);
				memcpy(localEncKeyWorkSpace, currentPatternPassword, currentPatternPasswordLength);
			    			
				//if(unlikely(lpasslength != currPWLen)) 
				{		
					if(likely(currentPatternPasswordLength < 32))			
						memcpy(localEncKeyWorkSpace + currentPatternPasswordLength, pad, \
							32-currentPatternPasswordLength);	
				   // lpasslength = currPWLen;				
				}							
				
				
				md5(localEncKeyWorkSpace, ekwlen, enckey);

				md5_50(enckey, length);
				memcpy(test, encdata->u_string, PARTIAL_TEST_SIZE);

				/** Algorithm 3.5 reversed */
				RC4_DECRYPT_REV3(PARTIAL_TEST_SIZE);
				
				/*
				#define RC4_DECRYPT_REV3(n) {			\
				    for(i = 19; i >= 0; --i) {			\
				      for(j = 0; j < length; ++j)		\
					tmpkey[j] = enckey[j] ^ i;		\
				      rc4Decrypt(tmpkey, test, n, test);	\
				    }						\
				  }
				*/

				/** if partial test succeeds we make a full check to be sure */
				if(unlikely(memcmp(test, rev3TestKey, PARTIAL_TEST_SIZE) == 0)) 
				{
					memcpy(test, encdata->u_string, length);
					RC4_DECRYPT_REV3(length);
					
					// If found passsword set to break out
					if(memcmp(test, rev3TestKey, length) == 0)
					{
					    #pragma omp critical
						{
							foundPasswordIndex = innerindex;
							printf("Found it at %lli !!!! *****\n", innerindex);
						}
					}
	
				}
				
			}    // end if (currentPatternPassword)
			
		}	// end innerindex parallel for

		nrprocessed += STRIPE_SIZE;
		
		// Assumed that first pattern is not the password!!!!
		
		if (foundPasswordIndex)
		{
			finalPatternPasswordIndex = foundPasswordIndex;
			return true;
		}
		else
			currPatternPasswordIndex = outerindex+STRIPE_SIZE-1;
  	}	// end outerindex
	
  	
  }
  
  return false;
}


bool
runCrackRev2(void) {

  printf("Entered runCrackRev2\n");

  startTime = time(NULL);  
    
  if (passMeth != Pattern)
  {
	  
	  uint8_t enckey[16];
	  unsigned int lpasslength;

	  lpasslength = 0;

	  do {
	    BEGIN_CRACK_LOOP();

	    do {
	      md5(encKeyWorkSpace, ekwlen, enckey);

	      /* Algorithm 3.4 reversed */
	      if(rc4Match40b(enckey, encdata->u_string, pad))
		return true;

	      ++nrprocessed;
	    } while(permutate());
	  } while(nextPassword());
  }
  else
  {
	// Pattern method  
	omp_set_num_threads(numThreads);
  	
  	unsigned long long int outerindex, innerindex, lastInvalidPasswordIndex, foundPasswordIndex, numberOfPaswords, lastStripeStartIndex;
  	
  	numberOfPaswords = getMaxPatternPasswords();
  	
  	if (numberOfPaswords > STRIPE_SIZE)
	  	lastStripeStartIndex = numberOfPaswords - (numberOfPaswords % STRIPE_SIZE);
	else
		lastStripeStartIndex = numberOfPaswords-1;
  	
  	lastInvalidPasswordIndex = 0LL;
  	foundPasswordIndex = 0LL;
 	
 	// Thread specific variables
 	
	uint8_t localEncKeyWorkSpace[ekwlen];

	uint8_t enckey[16];
	
  	int threadId;
  	uint8_t currentPatternPassword[PASSLENGTH+1];	// Holds pattern password of given index
  	int currentPatternPasswordLength;

  	for (outerindex = 0; outerindex <= lastStripeStartIndex ; outerindex += STRIPE_SIZE)
  	{
  	
  		// Parrallised check for given stripe  		
  	
  		#pragma omp parallel for    \
  			private(innerindex, threadId, enckey, localEncKeyWorkSpace, \
  				currentPatternPassword, currentPatternPasswordLength) \
  			shared(outerindex, lastInvalidPasswordIndex, foundPasswordIndex)
		for (innerindex = outerindex; innerindex < outerindex+STRIPE_SIZE ; innerindex++)
		{
			// Get the index password
			currentPatternPasswordLength = 	getPatternPassword(innerindex, currentPatternPassword);		
			if (currentPatternPasswordLength)
			{

				threadId = omp_get_thread_num();
				//printf("Thread %i: Password %lli: %s, Length %i\n", 
				//	threadId, innerindex, currentPatternPassword, currentPatternPasswordLength);
				
				//currPWLen = setPassword(currPW);
				// This should only be done the first time around as initialisation
				memcpy(localEncKeyWorkSpace, encKeyWorkSpace, ekwlen);
				memcpy(localEncKeyWorkSpace, currentPatternPassword, currentPatternPasswordLength);

				//if(unlikely(lpasslength != currPWLen)) 
				{		
					if(likely(currentPatternPasswordLength < 32))			
						memcpy(localEncKeyWorkSpace + currentPatternPasswordLength, pad, \
							32-currentPatternPasswordLength);	
				   // lpasslength = currPWLen;				
				}

				//md5(encKeyWorkSpace, ekwlen, enckey);
				md5(localEncKeyWorkSpace, ekwlen, enckey);

				/* Algorithm 3.4 reversed */
				//if(rc4Match40b(enckey, encdata->u_string, pad))
				
				if(rc4Match40b(enckey, encdata->u_string, pad))
				{
				#pragma omp critical
				   {
					foundPasswordIndex = innerindex;
					printf("Found it at %lli !!!! *****\n", innerindex);
				   }				
				}

			}   // if valid currentPatternPassword	
			
		}  // for innerindex
		
		nrprocessed += STRIPE_SIZE;
		
		// Assumed that first pattern is not the password!!!!
		
		if (foundPasswordIndex)
		{
			finalPatternPasswordIndex = foundPasswordIndex;
			return true;
		}
		else
			currPatternPasswordIndex = outerindex+STRIPE_SIZE-1;
			
	}  // for outerindex
	
  }  // if passMeth else
  
  return false;
}

bool
runCrackRev5(void) {

  printf("Entered runCrackRev5\n");
  startTime = time(NULL);

  if (passMeth != Pattern)
  {
  	// Wordlist or Generative

	  uint8_t enckey[32];
	  unsigned int lpasslength;

	  lpasslength = 0;

	  do {
	    currPWLen = setPassword(currPW);
	    if(unlikely(lpasslength != currPWLen)) {
	      /** Add the 8 byte user validation salt to pad */
	      if(likely(currPWLen < 32))
		memcpy(currPW + currPWLen, encdata->u_string+32, 8);
		// 	memcpy(currPW + currPWLen, pad, 32-currPWLen);	
	      lpasslength = currPWLen;
	    }

	    do {
	      sha256f(currPW, currPWLen+8, enckey);

	      if(memcmp(enckey, encdata->u_string, 32) == 0)
		return true;

	      ++nrprocessed;
	    } while(permutate());
	  } while(nextPassword());
  }
  else
  {
	// Pattern method  
	
	omp_set_num_threads(numThreads);
  	
  	unsigned long long int outerindex, innerindex, lastInvalidPasswordIndex, foundPasswordIndex, numberOfPaswords, lastStripeStartIndex;
  	
  	numberOfPaswords = getMaxPatternPasswords();
  	
  	if (numberOfPaswords > STRIPE_SIZE)
	  	lastStripeStartIndex = numberOfPaswords - (numberOfPaswords % STRIPE_SIZE);
	else
		lastStripeStartIndex = numberOfPaswords-1;
  	
  	lastInvalidPasswordIndex = 0LL;
  	foundPasswordIndex = 0LL;
 	
 	// Thread specific variables
 	
	uint8_t localEncKeyWorkSpace[ekwlen];

	uint8_t enckey[32];
	
	
  	int threadId;
  	uint8_t currentPatternPassword[PASSLENGTH+1];	// Holds pattern password of given index
  	int currentPatternPasswordLength;
  	
  	for (outerindex = 0; outerindex <= lastStripeStartIndex ; outerindex += STRIPE_SIZE)
  	{
  	
  		// Parrallised check for given stripe  		
  	
  		#pragma omp parallel for    \
  			private(innerindex, threadId, enckey, localEncKeyWorkSpace, \
  				currentPatternPassword, currentPatternPasswordLength) \
  			shared(outerindex, lastInvalidPasswordIndex, foundPasswordIndex)
		for (innerindex = outerindex; innerindex < outerindex+STRIPE_SIZE ; innerindex++)
		{
			// Get the index password
			currentPatternPasswordLength = 	getPatternPassword(innerindex, currentPatternPassword);		
			if (currentPatternPasswordLength)
			{

				threadId = omp_get_thread_num();
				//printf("Thread %i: Password %lli: %s, Length %i\n", 
				//	threadId, innerindex, currentPatternPassword, currentPatternPasswordLength);
				
				//currPWLen = setPassword(currPW);
				// This should only be done the first time around as initialisation
				memcpy(localEncKeyWorkSpace, encKeyWorkSpace, ekwlen);
				memcpy(localEncKeyWorkSpace, currentPatternPassword, currentPatternPasswordLength);
				
				//memcpy(currPW + currPWLen, encdata->u_string+32, 8);
				memcpy(localEncKeyWorkSpace + currentPatternPasswordLength, encdata->u_string+32, 8);

				//sha256f(currPW, currPWLen+8, enckey);
				sha256f(localEncKeyWorkSpace, currentPatternPasswordLength+8, enckey);

				if(memcmp(enckey, encdata->u_string, 32) == 0)
				{
				#pragma omp critical
				   {
					foundPasswordIndex = innerindex;
					printf("Found it at %lli !!!! *****\n", innerindex);
				   }
				}
	
			}    // end if (currentPatternPassword)
			
		}	// end innerindex parallel for

		nrprocessed += STRIPE_SIZE;
		
		// Assumed that first pattern is not the password!!!!
		
		if (foundPasswordIndex)
		{
			finalPatternPasswordIndex = foundPasswordIndex;
			return true;
		}
		else
			currPatternPasswordIndex = outerindex+STRIPE_SIZE-1;
			
  	}	// end outerindex

  }
  
  return false;
}


bool
runCrackRev5_o(void) {
  uint8_t enckey[32];
  unsigned int lpasslength;

  lpasslength = 0;
  startTime = time(NULL);
  do {
    currPWLen = setPassword(currPW);
    if(unlikely(lpasslength != currPWLen)) {
      /** Add the 8 byte user validation and 48 byte u-key salt to pad */
      if(likely(currPWLen < 32)) {
	memcpy(currPW + currPWLen, encdata->o_string+32, 8);
       	memcpy(currPW + currPWLen+8, encdata->u_string, 48);
      }
      lpasslength = currPWLen;
    }

    do {
      sha256(currPW, currPWLen+56, enckey);

      if(memcmp(enckey, encdata->o_string, 32) == 0)
	return true;

      ++nrprocessed;
    } while(permutate());
  } while(nextPassword());
  return false;
}


/** Start cracking and does not stop until it has either been interrupted by
    a signal or the password either is found or wordlist or charset is exhausted
*/
void
runCrack(void) {
  bool found = false;
  uint8_t cpw[88];

  if(encdata->revision == 5) {
    if(workWithUser) {
      memcpy(currPW, encdata->u_string+32, 8);
      found = runCrackRev5();
    }
    else {
      memcpy(currPW, encdata->o_string+32, 8);
      memcpy(currPW + 8, encdata->u_string, 48);
      found = runCrackRev5_o();
    }
  } 
  else if(!workWithUser && !knownPassword) {
    memcpy(cpw, pad, 32);
    currPW = cpw;
    if(encdata->revision == 2)
      found = runCrackRev2_o();
    else
      found = runCrackRev3_o();
  }  
  else if(encdata->revision == 2) {
    if(workWithUser)
      found = runCrackRev2();
    else /** knownPassword */
      found = runCrackRev2_of();
  }
  else {
    if(workWithUser)
      found = runCrackRev3();
    else /** knownPassword */
      found = runCrackRev3_of();
  }
  crackDone = true;
  if(!found)
    printf("Could not find password\n");
  else
    foundPassword();
  currPW = NULL;
}

/** returns the number of processed passwords */
unsigned int
getNrProcessed(void) { return nrprocessed; }

/** These are shared variables between loading and initialisation and controls
    how to do the initialisation. Should not be touched by anything except 
    loadState and cleanPDFCrack.
*/
static bool recovery = false;
static bool permutation = false;


/** cleans up everything as is needed to do a any initPDFCrack-calls after the
    first one.
*/
void
cleanPDFCrack(void) {
  if(rev3TestKey) {
    /** Do a really ugly const to non-const cast but this one time it should
	be safe
    */
    free((uint8_t*)rev3TestKey);
    rev3TestKey = NULL;
  }
  if(encKeyWorkSpace) {
    free(encKeyWorkSpace);
    encKeyWorkSpace = NULL;
  }
  knownPassword = false;
  recovery = false;
  permutation = false;
}

/** initPDFCrack is doing all the initialisations before you are able to call
    runCrack(). Make sure that you run cleanPDFCrack before you call this 
    after the first time.
*/
bool
initPDFCrack(const EncData *e, const uint8_t *upw, const bool user,
	     const char *wl, const passwordMethod pm, FILE *file,
	     const char *cs, const char *pat, const unsigned int minPw,
	     const unsigned int maxPw, const bool perm, const bool qt, const int numth) {
  uint8_t *buf;
  unsigned int upwlen;
  uint8_t *tmp;

  ekwlen = initEncKeyWorkSpace(e->revision, e->encryptMetaData, e->permissions,
			       e->o_string, e->fileID, e->fileIDLen);

  encdata = e;
  currPW = encKeyWorkSpace;
  currPWLen = 0;
  nrprocessed = 0;
  workWithUser = user;
  crackDone = false;
  
  // Set new vars
  passMeth = pm;
  numThreads = numth;
  
  if(e->revision == 5) {
    permutation = (perm || permutation);
    if(permutation)
      permutate = do_permutate;
    else
      permutate = no_permutate;
    initPasswords(pm, file, wl, cs, pat, minPw, maxPw, qt, numth);
    return true;
  }

  md5_50_init(encdata->length/8);

  if(!setrc4DecryptMethod((const unsigned int)e->length))
    exit(234);

  if(upw) {
    upwlen = strlen((const char*)upw);
    if(upwlen > 32)
      upwlen = 32;
    memcpy(password_user, upw, upwlen);
    memcpy(password_user+upwlen, pad, 32-upwlen);
    memcpy(encKeyWorkSpace, password_user, 32);
    knownPassword = true;
  }
  /** Workaround to set password_user when loading state from file */
  if(recovery) 
    memcpy(encKeyWorkSpace, password_user, 32);

  if(encdata->revision == 2) {
    if(knownPassword) {
      if(!isUserPasswordRev2())
	return false;
      memcpy(encKeyWorkSpace, pad, 32);
    }
    else {
      memcpy(password_user, pad, 32);
      knownPassword = isUserPasswordRev2();
    }
  }
  else if(e->revision >= 3) {
    buf = malloc(32+sizeof(uint8_t)*e->fileIDLen);
    memcpy(buf, pad, 32);
    memcpy(buf + 32, e->fileID, e->fileIDLen);
    tmp = malloc(sizeof(uint8_t)*16);
    md5(buf, 32+e->fileIDLen, tmp);
    free(buf);
    rev3TestKey = tmp;
    if(knownPassword) {
      if(!isUserPasswordRev3())
	return false;
      memcpy(encKeyWorkSpace, pad, 32);
    }
    else {
      memcpy(password_user, pad, 32);
      knownPassword = isUserPasswordRev3();
    }
  }

  permutation = (perm || permutation);
  if(permutation)
    permutate = do_permutate;
  else
    permutate = no_permutate;

  initPasswords(pm, file, wl, cs, pat, minPw, maxPw, qt, numth);
  return true;
}

/** Some common patterns between the loadState and saveState */
static const char string_PRVPL[] = 
  "PDF: %d.%d\nR: %d\nV: %d\nP: %d\nL: %d\n"
  "MetaData: %d\nFileID(%d):";
static const char string_FILTER[] = "\nFilter(%zu): ";
static const char string_UUPWP[] = 
  "\nUser: %d\nUserPw: %d\nPermutate: %d\n";


/** Reads from file and tries to set up the state that it contains.
    Returns true on success and false otherwise.
    Very little checks for data validitiy is made.
*/
bool
loadState(FILE *file, EncData *e, char **wl, bool *user) {
  unsigned int i;
  int tmp, tmp2, tmp3;
  size_t len;

  /** Load all the simple values bound to the document */
  if(fscanf(file,string_PRVPL, &e->version_major, &e->version_minor,
	    &e->revision, &e->version, &e->permissions, &e->length,
	    &tmp, &e->fileIDLen) < 8)
    return false;

  /** Unsupported revision might be indication of corrupt file */
  if(e->revision > 5 || e->revision < 2)
    return false;
  
  /** bork out if length is insanely high */
  if(e->fileIDLen > 256)
    return false;

  e->encryptMetaData = (tmp == true);

  /** Load the FileID */
  e->fileID = malloc(sizeof(uint8_t)*e->fileIDLen);
  for(i=0;i<e->fileIDLen;i++) {
    if(fscanf(file, " %d", &tmp) < 1)
      return false;
    e->fileID[i] = tmp;
  }

  /** Load the Security Handler */
  if(fscanf(file,string_FILTER, &len) < 1)
    return false;

  /** bork out if length is obviously wrong */
  if(len > 256 || len <= 0)
    return false;

  e->s_handler = malloc((sizeof(uint8_t)*len)+1);

  for(i=0;i<(unsigned int)len;i++) {
    e->s_handler[i] = getc(file);
  }

  /** Make sure we null-terminate the string */
  e->s_handler[i] = '\0';

  /** Currently we only handle Standard, so probably corrupt otherwise */
  if(strcmp(e->s_handler,"Standard") != 0)
     return false;
  
  /** Load the U- and O-strings */
  if(fscanf(file, "\nO:") == EOF)
    return false;
  if(e->revision == 5)
    len = 48;
  else
    len = 32;
  e->o_string = malloc(sizeof(uint8_t)*len);
  e->u_string = malloc(sizeof(uint8_t)*len);

  for(i=0;i<len;i++) {
    if(fscanf(file, " %d", &tmp) < 1)
      return false;
    e->o_string[i] = tmp;
  }
  if(fscanf(file, "\nU:") == EOF)
    return false;
  for(i=0;i<len;i++) {
    if(fscanf(file, " %d", &tmp) < 1)
      return false;
    e->u_string[i] = tmp;
  }
 
  /** Load the simple values bound to the state */
  if(fscanf(file, string_UUPWP, &tmp, &tmp2, &tmp3) < 3)
    return false;
  *user = (tmp == true);
  knownPassword = (tmp2 == true);
  permutation = (tmp3 == true);

  /** Load the saved userpassword if that is used */
  if(knownPassword) {
    for(i=0;i<32;i++) {
      if(fscanf(file, " %d", &tmp) < 1)
	return false;
      password_user[i] = tmp;
    }
  }

  /** Load the password-specific stuff for the state */
  if(!pw_loadState(file, wl))
    return false;

  /** Remember to set the recovery-boolean to make sure the right things 
      happen in initPDFCrack */
  recovery = true;

  return true;
}

/** Saves the current state in the engine to file */
void
saveState(FILE *file) {
  unsigned int i;
  size_t len;

  fprintf(file, string_PRVPL,
	  encdata->version_major, encdata->version_minor, encdata->revision,
	  encdata->version, encdata->permissions, encdata->length,
	  (int)encdata->encryptMetaData, encdata->fileIDLen);
  for(i=0;i<encdata->fileIDLen;i++)
    fprintf(file, " %d", encdata->fileID[i]);
  fprintf(file, string_FILTER, strlen(encdata->s_handler));
  fprintf(file, "%s", encdata->s_handler);
  fprintf(file, "\nO:");
  if(encdata->revision == 5)
    len = 48;
  else
    len = 32;

  for(i=0;i<len;i++)
    fprintf(file, " %d", encdata->o_string[i]);
  fprintf(file,"\nU:");
  for(i=0;i<len;i++)
    fprintf(file, " %d", encdata->u_string[i]);
  fprintf(file, string_UUPWP, (int)workWithUser,
	  (int)knownPassword, (int)permutation);
  if(knownPassword) {
    for(i=0;i<32;i++)
      fprintf(file, " %d", password_user[i]);
  }

  /** Save the password-specific stuff */
  pw_saveState(file);
}
