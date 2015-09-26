/**
 * Copyright (C) 2015 Shreepad Shukla
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
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "passwords.h"
#include "pattern.h"

#define NUMBERCLASSES 6
#define MAXCLASSCHARS 200

// static variables and functions to handle password patterns
static const char characterClassLiterals[NUMBERCLASSES][2][MAXCLASSCHARS] = {
	{
		":alnum:",
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
	},
	{
		":alpha:",
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
	},
	{
		":digit:",
		"0123456789"
	},
	{
		":lower:",
		"abcdefghijklmnopqrstuvwxyz"
	},
	{
		":upper:",
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	},
	{
		":punct:",
		"!\"#$%&'()*+,./:;<=>?@^_`|~-"
	}
};

/*
static const uint8_t
iso88591chars[] = {"abcdefghijklmnopqrstuvwxyz"
	      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	      "0123456789"};
static const char* anyliteral = ":any:";

static const uint8_t alnumchars[] = {"A-Za-z0-9"};
static const char* alnumliteral = ":alnum:";

static const uint8_t alphachars[] = {"A-Za-z"};
static const char* alphaliteral = ":alpha:";

static const uint8_t digitchars[] = {"0-9"};
static const char* digitliteral = ":digit:";

static const uint8_t lowerchars[] = {"a-z"};
static const char* lowerliteral = ":lower:";

static const uint8_t upperchars[] = {"A-Z"};
static const char* upperliteral = ":upper:";

static const uint8_t punctchars[] = {"][!\"#$%&'()*+,./:;<=>?@\\^_`{|}~-"};
static const char* punctliteral = ":punct:";

static const uint8_t spacechars[] = {" \t\r\n\v\f"};
static const char* spaceliteral = ":space:";

*/



// Validate pattern

int validPattern(const char* pat)
{
	if (!pat)
		return 0;
		
	return 1;
}


/**
 * str_replace.c implements a str_replace PHP like function
 * Copyright (C) 2010  chantra <chantra__A__debuntu__D__org>
 */
// Replace all occurences of a string in another string
// From http://coding.debuntu.org/c-implementing-str_replace-replace-all-occurrences-substring 
char *
str_replace ( const char *string, const char *substr, const char *replacement )
{
      char *tok = NULL;
      char *newstr = NULL;
      char *oldstr = NULL;
      char *head = NULL;
     
      /* if either substr or replacement is NULL, duplicate string a let caller handle it */
      if ( substr == NULL || replacement == NULL ) return strdup (string);
      newstr = strdup (string);
      head = newstr;

      while ( (tok = strstr ( head, substr ))){
		oldstr = newstr;
		newstr = malloc ( strlen ( oldstr ) - strlen ( substr ) + strlen ( replacement ) + 1 );
		/*failed to alloc mem, free old string and return NULL */
		if ( newstr == NULL ){
		  free (oldstr);
		  return NULL;
		}
		memcpy ( newstr, oldstr, tok - oldstr );
		memcpy ( newstr + (tok - oldstr), replacement, strlen ( replacement ) );
		memcpy ( newstr + (tok - oldstr) + strlen( replacement ), tok + strlen ( substr ), strlen ( oldstr ) - strlen ( substr ) - ( tok - oldstr ) );
		memset ( newstr + strlen ( oldstr ) - strlen ( substr ) + strlen ( replacement ) , 0, 1 );
		/* move back head right after the last replacement */
		head = newstr + (tok - oldstr) + strlen( replacement );
		free (oldstr);
      }
      
      return newstr;
}



// Replace character classes in the :class: format as per Issue #3
// See https://github.com/shreepads/pdfcrack-mp/issues/3
char* replaceClasses(char* pat)
{
	if (!pat)
		return NULL;
		
	// Check if there are any classes
	if (!strchr(pat, ':'))
		return pat;


	char *classReplacedPat = pat;
	
	for (int i=0; i<NUMBERCLASSES; i++)
	{
		//printf("Replacing %s with %s in %s\n", 
		//	characterClassLiterals[i][0], characterClassLiterals[i][1], classReplacedPat);
		
		classReplacedPat = str_replace(classReplacedPat, 
			characterClassLiterals[i][0], characterClassLiterals[i][1]);
		
		//printf("New string %s\n", classReplacedPat);
	}

	return classReplacedPat;
}


// Replace ranges in the {n,m} format as per Issue #4
// See https://github.com/shreepads/pdfcrack-mp/issues/4
char* replaceRanges(char* pat)
{
	if (!pat)
		return NULL;

	// Check if there are any ranges
	if (!strchr(pat, '}'))
		return pat;
		
	
	// For each occurence of a range {min,max}, pick up the following [] and replace it multiple times
	// Obviously this can be much improved by someone with better knowledge of C string hanlding
	char *rangeReplacedPat = NULL;
	int done = 0;
	
	while (!done)
	{
		// Allocate previous rangeReplacedPat - dropping a few pointers on the floor here
		if (rangeReplacedPat)
		{
			pat = rangeReplacedPat;
		}
		
		// Check if there are any ranges left
		if (!strchr(pat, '}'))
			break;

		char *rangestart = strchr(pat, '{');
		rangestart++;
	
		char *rangemid = strchr(rangestart, ',');
		rangemid++;
	
		char* rangeend = strchr(rangemid, '}');
	
		char minstr[rangemid - rangestart];
		strncpy(minstr, rangestart, rangemid - rangestart - 1);
		minstr[rangemid - rangestart - 1] = '\0';
	
		char maxstr[rangeend - rangemid + 1];
		strncpy(maxstr, rangemid, rangeend - rangemid);
		maxstr[rangeend - rangemid + 1] = '\0';
	
		//printf("Maxstr: %s; Minstr: %s;\n", maxstr, minstr);
	
		long int max = strtol(maxstr, NULL, 10);
		long int min = strtol(minstr, NULL, 10);
	
		// Validate max and min limits - assume that validatePattern only validates structurally
		if ( (max < 0)  ||  (min < 0)  ||  (min > max)  || (max > PASSLENGTH) )
		{
			printf("Invalid Range: max=%li, min=%li:\n", max, min);
			return NULL;
		}
	
		//printf("Max: %li; Min: %li;\n", max, min);
	
		char *patstart = rangeend + 1;
		char *patend = strchr(rangeend, ']');
	
		char rangepat[patend - patstart + 2];
		strncpy(rangepat, patstart, patend - patstart + 1);
		rangepat[patend - patstart + 1] = '\0';
	
		// Alternate pattern with optional char
		char rangepatopt[strlen(rangepat) + 2];
		memcpy(rangepatopt, rangepat, strlen(rangepat) - 1);
		rangepatopt[strlen(rangepat) - 1] = OPTPATCHAR;
		rangepatopt[strlen(rangepat)] = ']';
		rangepatopt[strlen(rangepat) + 1] = '\0';
	
		//printf("Pat:%s; Optpat:%s;\n", rangepat, rangepatopt);
	
		char newPattern[strlen(pat) + (strlen(rangepatopt) * max)];
		newPattern[strlen(pat) + (strlen(rangepatopt) * max) - 1] = '\0';
		char *newPatternPtr = newPattern;
	
		memcpy(newPatternPtr, pat, (rangestart - pat - 1));
		newPatternPtr += rangestart - pat - 1;
	
		//printf("Newpat:%s;\n", newPattern);
	
		for (int i=0; i<min; i++)
		{
			memcpy(newPatternPtr, rangepat, strlen(rangepat));
			newPatternPtr += strlen(rangepat);
		}
	
		//printf("Newpat:%s;\n", newPattern);
		
		for (int j=min; j<max; j++)
		{
			memcpy(newPatternPtr, rangepatopt, strlen(rangepatopt));
			newPatternPtr += strlen(rangepatopt);
		}
	
		//printf("Newpat:%s;\n", newPattern);
	
		strcpy(newPatternPtr, (patend + 1));
	
		//printf("Newpat:%s;\n", newPattern);
	
		rangeReplacedPat = strdup(newPattern);
	}
	
	return rangeReplacedPat;
}


// Calculate length of pattern string - only to be used after complete parsing is done

unsigned int patternLength(const char* pat)
{
	if (!pat)
		return 0;

	unsigned int i;
	
	for (i=0; pat[i]; pat[i] == '[' ? i++ : *pat++);
	
	return i;
}


// Take the pattern, validate it and return a string with all possible charsets indiviudally listed within []

char* parsePattern(const char* pat)
{
	char* declassedPattern = NULL;
	char* parsedPattern = NULL;
	
	if (!pat)
		return NULL;
				
	// Check the pattern is valid
	if (!validPattern(pat))
		return NULL;
	
	// Replace repeated occurences - Not needed, will be handled as part of ranges
	
	// Replace character classes
	declassedPattern = replaceClasses((char *) pat);
	printf("De-classed pattern: %s\n", declassedPattern);
	
	// Replace NOT operator
	
	// Replace ranges
	parsedPattern = replaceRanges(declassedPattern);
	
	//printf("Parsed pattern: %s\n", parsedPattern);
	
	return parsedPattern;
}


// Turn a parsed patern string into an array of password chars and associated lengths of each
void setPatternArray(const char* pat, unsigned int patLen, 
	uint8_t* passwordPatArray[], unsigned int passwordPatLengths[], unsigned long long int passwordPatDivs[])
{
	printf("Parsed pattern: %s, length: %i\n", pat, patLen);
	
	// Create temp pattern holder for tokenisation, 2 less for the ends, 1 for the /0
	char tokPat[strlen(pat)-2+1];
	strncpy(tokPat, &pat[1], strlen(pat)-2);
	tokPat[strlen(pat)-2] = '\0';
	
	//printf("tokPat: %s\n", tokPat);
	
	
	// Use string tokenizer with separator ][ to pick individual charsets
	
	char *scratch, *token, *delimiter = "][";
	
	int i = 0;
	token = strtok_r(tokPat, delimiter, &scratch);
	
	do
	{
		if (token)
		{
			//passwordPatArray[i] = malloc(strlen(token));
			//strncpy(&passwordPatArray[i], token, strlen); 
			
			asprintf((char **)&passwordPatArray[i], "%s", token);
			
			passwordPatLengths[i] = strlen((char *) passwordPatArray[i]);
			
			if (i==0)
				passwordPatDivs[i]=1LL;
			else
				passwordPatDivs[i]=passwordPatLengths[i-1]*passwordPatDivs[i-1];
				
			i++;
		}
	}
	while((token = strtok_r(NULL, delimiter, &scratch)));
	
	return;
	
}

