#include "postgres.h"
#include "fmgr.h"
#include "libpq/pqformat.h"
#include "access/hash.h"
#include "utils/builtins.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>

typedef struct PersonName
{
	int32 length;
	char		name[FLEXIBLE_ARRAY_MEMBER];
}			PersonName;	

int checkCommaAndNull(char *str);
int checkGivenAndFamily(char *name);

int checkName(char *name);

char *getFamily(char *name);
char *getGiven(char *name);
char *showName(char *family, char *given);

int runChecks(char *PersonName);

static int pname_abs_cmp_internal(PersonName * pn1, PersonName * pn2);

PG_MODULE_MAGIC;

/*****************************************************************************
 * C functions
 *****************************************************************************/

// PersonName comparison function
static int
pname_abs_cmp_internal(PersonName * pn1, PersonName * pn2)
{
    return strcmp(pn1->name,pn2->name);
}

// Checks PersonName entered is valid by running check functions
int runChecks(char *pname){
    char *g;
    char *f;

    if (checkCommaAndNull(pname) == 0) {
        return 0;
    }
    
    if (checkGivenAndFamily(pname) == 0) {
        return 0;
    }
    
    g = getGiven(pname);
    f = getFamily(pname);
    
    if (checkName(g) == 0){
        return 0;
    }
    
    if (checkName(f) == 0){
        return 0;
    }
    
    pfree(g);
    pfree(f);
    
    return 1;
}


// Check string is not NULL and has a single comma
int checkCommaAndNull(char *str) {
    int i, count;
    
    if (str == NULL) {
        return 0;
    }
    
    for (i = 0, count = 0; str[i]; i++)
      count += (str[i] == ',');

    if (count == 1) {
        return 1;
    } else {
        return 0;
    }
}

// Check to see if that there are capital alphabetical chars before and after ","
int checkGivenAndFamily(char *name) {
    int found;
    char *commastring;

    // Check if name is NULL
    if (name == NULL) {
        return 0;
    }
    
    // Check if name is not just a comma
    if (strlen(name) <= 1) {
        return 0;
    }
    
    // Try to find at capital letter in family
    found = 0;
    for (int i = 0; name[i] != ','; i++){
        if (name[i] >= 'A' && name[i] <= 'Z'){
            found = 1;
            break;
        } else {
            continue;
        }
    }
    
    if (found  == 0) {
        return 0;
    }
    
    // Try to find at capital letter in given
    found = 0;
    commastring = strchr(name, ',');
    for (int i = 0; commastring[i] != '\0'; i++){
        if (commastring[i] >= 'A' && commastring[i] <= 'Z'){
            found = 1;
            break;
        } else {
            continue;
        }
    }
    
    if (found  == 0) {
        return 0;
    }
    
    return 1;
}

// Extract given name from PersonName string
char *getGiven(char *name) {
    char *given;
    
    char *str = strchr(name, ',');
    str = str + 1;
    
    // Find if there is a space after comma
    if (*str == ' '){
        str++;
    }

    given = (char *) palloc0(strlen(str) + 1);
    memcpy(given, str, strlen(str));
    
    return given;
}

// Extract family name from PersonName string
char *getFamily(char *name) {
    int i = 0;
    char *family;

    // find position of comma
    while (name[i] != ','){
        i++;
    }

    // Allocate memory for string and copy string up to i position
    family = (char *) palloc0(i + 1);
    memcpy(family, name, i);

    return family;
}


// Checks if name is valid for both family and given names. Name input must not have , or leading space for given name 
int checkName(char *name){
    
    char first_letter;
    char last_letter;
    char *ptr;
    char *cpy;
    
    if (name == NULL) {
        return 0;
    }
    
    // Check for leading space char
    if (*name == ' '){
        return 0;
    }
    
    // Check for trailing trailing space char
    last_letter = name[strlen(name) - 1];
    
    if (last_letter == ' '){
        return 0;
    }
    
    cpy = (char *) palloc0(strlen(name) + 1);
    strcpy(cpy, name);
    
    ptr = strtok(cpy, " ");

    // Check each name is valid
    while(ptr != NULL)
    {
        
        // Check if name is too small
        if (strlen(ptr) < 2){
            pfree(cpy);
            return 0;
        }

        // Check if first letter is not capitalised
        first_letter = ptr[0];

        if (first_letter < 'A' || first_letter > 'Z'){
            pfree(cpy);
            return 0;
        }

        //Check for any non-valid characters
        for (int i = 1; i < strlen(ptr); i++){
            if ((ptr[i] >= 'a' && ptr[i] <= 'z') || (ptr[i] >= 'A' && ptr[i] <= 'Z') || ptr[i] == '-' || ptr[i] == '\''){
                continue;
            } else {
                pfree(cpy);
                return 0;
            }
            
        }

        ptr = strtok(NULL, " ");
    }
    
    pfree(cpy);
    return 1;
}


// Returns a readable version of the name
char *showName(char *family, char *given) {
    char *show;
    int i = 0;
    
    // Find length of first given name
    for (i = 0; given[i] != '\0'; i++){
        if (given[i] == ' '){
            break;
        } else {
            continue;
        }
    }
    
    show = (char *) palloc0(i + strlen(family) + 2);
    memcpy(show, given, i);
    strcat(show, " ");
    strcat(show, family);
    
    return show;
}



/*****************************************************************************
 * Input/Output functions
 *****************************************************************************/

// Input function for PersonName
PG_FUNCTION_INFO_V1(pname_in);

Datum
pname_in(PG_FUNCTION_ARGS)
{
	char	   *str = PG_GETARG_CSTRING(0);
	PersonName *pn;
	char *g;
	char *f; 
	char *temp= (char *) palloc0(strlen(str)+1);
	
	if (runChecks(str) == 0){
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for type %s: \"%s\"",
						"PersonName", str)));
	}

	// Get given and family names
	g = getGiven(str);
	f = getFamily(str);

	// Format PersonName
	sprintf(temp, "%s,%s", f,g);

	pn = (PersonName *) palloc0(strlen(temp) + 1 + VARHDRSZ);
	SET_VARSIZE(pn, strlen(temp) + 1 + VARHDRSZ);
	
	memcpy(pn->name, temp, strlen(temp));
	
	pfree(f);
	pfree(g);
	
	PG_RETURN_POINTER(pn);
}

// Output function for PersonName
PG_FUNCTION_INFO_V1(pname_out);

Datum
pname_out(PG_FUNCTION_ARGS)
{
	PersonName    *pn = (PersonName *) PG_GETARG_POINTER(0);
	char	   *result;
	
	char *g = getGiven(pn->name);
	char *f = getFamily(pn->name);

	result = psprintf("%s,%s", f, g);
	
	pfree(f);
	pfree(g);
	
	PG_RETURN_CSTRING(result);
}

// Function to print only family name
PG_FUNCTION_INFO_V1(family);

Datum
family(PG_FUNCTION_ARGS)
{
	PersonName    *pn = (PersonName *) PG_GETARG_POINTER(0);

	char *f = getFamily(pn->name);

	PG_RETURN_TEXT_P(cstring_to_text(psprintf("%s", f)));
}

// Function to print only given name
PG_FUNCTION_INFO_V1(given);

Datum
given(PG_FUNCTION_ARGS)
{
	PersonName    *pn = (PersonName *) PG_GETARG_POINTER(0);

	char *g = getGiven(pn->name);

	PG_RETURN_TEXT_P(cstring_to_text(psprintf("%s", g)));

}

// Function to prince PersonName in a readable format
PG_FUNCTION_INFO_V1(show);

Datum
show(PG_FUNCTION_ARGS)
{
	PersonName    *pn = (PersonName *) PG_GETARG_POINTER(0);
	char	   *result;
	
	char *g = getGiven(pn->name);
	char *f = getFamily(pn->name);
	
	char *show = showName(f, g);

	result = psprintf("%s", show);
	
	pfree(g);
	pfree(f);
	
	PG_RETURN_TEXT_P(cstring_to_text(result));
}


/*****************************************************************************
 * Operator class for defining hash index
 *****************************************************************************/

// Function to create a hash index 
PG_FUNCTION_INFO_V1(hash_pname);

Datum
hash_pname(PG_FUNCTION_ARGS){
	PersonName    *pn = (PersonName *) PG_GETARG_POINTER(0);
	char *hashStr;
	
	// Create copy of PersonName for use in hash_any()
	hashStr = (char *) palloc0(strlen(pn->name) + 1);
	memcpy(hashStr, pn->name, strlen(pn->name));
	
	PG_RETURN_INT32(DatumGetInt32(hash_any((const unsigned char *)hashStr,strlen(pn->name))));
}

/*****************************************************************************
 * Operator class for defining B-tree index
 *****************************************************************************/

// Comparison functions for <, <=, =, != (<>), >= and >
PG_FUNCTION_INFO_V1(pname_abs_lt);

Datum
pname_abs_lt(PG_FUNCTION_ARGS)
{
	PersonName    *a = (PersonName *) PG_GETARG_POINTER(0);
	PersonName    *b = (PersonName *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(pname_abs_cmp_internal(a, b) < 0);
}

PG_FUNCTION_INFO_V1(pname_abs_le);

Datum
pname_abs_le(PG_FUNCTION_ARGS)
{
	PersonName    *a = (PersonName *) PG_GETARG_POINTER(0);
	PersonName    *b = (PersonName *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(pname_abs_cmp_internal(a, b) <= 0);
}

PG_FUNCTION_INFO_V1(pname_abs_eq);

Datum
pname_abs_eq(PG_FUNCTION_ARGS)
{
	PersonName    *a = (PersonName *) PG_GETARG_POINTER(0);
	PersonName    *b = (PersonName *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(pname_abs_cmp_internal(a, b) == 0);
}

PG_FUNCTION_INFO_V1(pname_abs_not_eq);

Datum
pname_abs_not_eq(PG_FUNCTION_ARGS)
{
	PersonName    *a = (PersonName *) PG_GETARG_POINTER(0);
	PersonName    *b = (PersonName *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(pname_abs_cmp_internal(a, b) != 0);
}

PG_FUNCTION_INFO_V1(pname_abs_ge);

Datum
pname_abs_ge(PG_FUNCTION_ARGS)
{
	PersonName    *a = (PersonName *) PG_GETARG_POINTER(0);
	PersonName    *b = (PersonName *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(pname_abs_cmp_internal(a, b) >= 0);
}

PG_FUNCTION_INFO_V1(pname_abs_gt);

Datum
pname_abs_gt(PG_FUNCTION_ARGS)
{
	PersonName    *a = (PersonName *) PG_GETARG_POINTER(0);
	PersonName    *b = (PersonName *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(pname_abs_cmp_internal(a, b) > 0);
}

PG_FUNCTION_INFO_V1(pname_abs_cmp);

Datum
pname_abs_cmp(PG_FUNCTION_ARGS)
{
	PersonName    *a = (PersonName *) PG_GETARG_POINTER(0);
	PersonName    *b = (PersonName *) PG_GETARG_POINTER(1);

	PG_RETURN_INT32(pname_abs_cmp_internal(a, b));
}