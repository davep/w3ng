/*

     W3NG CGI-BIN NORTON GUIDE FILTER.
     Copyright (C) 1996 David A Pearson
   
     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published by
     the Free Software Foundation; either version 2 of the license, or 
     (at your option) any later version.
     
     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
     GNU General Public License for more details.
     
     You should have received a copy of the GNU General Public License
     along with this program; if not, write to the Free Software
     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/*
 * Modification history:
 *
 * $Log: w3ng.c,v $
 * Revision 1.12  1996/04/01 15:10:16  davep
 * Changed to work with Norton Guides that have no menu at all.
 *
 * Revision 1.11  1996/03/29 14:09:15  davep
 * Added the ability to produce frames aware HTML code.
 *
 * Revision 1.10  1996/03/28 18:35:12  davep
 * Added the config file allowing the setting of the <BODY> token. This
 * will allow you to specify (for example) a background image for the body
 * of the message.
 *
 * Revision 1.9  1996/03/25 11:53:17  davep
 * Fixed one more HTML silly.
 *
 * Revision 1.8  1996/03/22 17:35:40  davep
 * Fixed Hex2Byte() which was badly broken. Also did some more work on
 * producing correct HTML. The & characters was breaking some viewers
 * such as Lynx, Explorer and Mosaic. This now produces a &amp;.
 *
 * Revision 1.7  1996/03/21 16:45:31  davep
 * Changed ReadLong() to work correctly for 16bit compilers.
 *
 * Revision 1.6  1996/03/20 19:59:46  davep
 * Small changes to the style of HTML generated. This fixed the problem
 * of some browsers not being able to handle the &lt and &gt symbols.
 *
 * Revision 1.5  1996/03/18 08:00:08  davep
 * Fixed the problem with drop menus that have no menu items.
 *
 * Revision 1.4  1996/02/26 21:14:48  davep
 * Added the -href switch.
 * Fixed the "no null" bug with the NG header.
 *
 * Revision 1.3  1996/02/19 20:11:05  davep
 * Made some changes so that it should now compile clean under Linux (GCC),
 * OS/2 (GCC/EMX) and MS-Dos (Borland, DJGPP). The code still needs a good
 * tidy (too much hang-over from Expect Guide) and a comment or two wouldn't
 * hurt I guess.
 *
 * Revision 1.2  1996/02/16 10:40:52  davep
 * Now compiles clean under Linux (GCC), OS/2 (EMX) and Dos (BCC).
 * Also added a header to each page that gives the user the ability
 * to navigate the guide a bit more like NG or EH would.
 *
 * Revision 1.1  1996/02/15 18:57:13  davep
 * Initial revision
 *
 */

/* Account for some of the messy stuff we have to do with DOS compilers. */

#if defined( __MSDOS__ )
#include <dir.h>
#if defined( __GNUC__ )
#define _MAX_PATH  MAXPATH
#define _MAX_FNAME MAXFILE
#define _MAX_EXT   MAXEXT
#define _splitpath fnsplit
#define _makepath  fnmerge
#endif
#endif

/* Usual standard header files. */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Include file for the cfg*() functions. */

#include "cfgfile.h"

/* Structure of the header record of a Norton Guide database. */

typedef struct 
{
    unsigned short usMagic;
    short          sUnknown1;
    short          sUnknown2;
    unsigned short usMenuCount;
    char           szTitle[ 40 ];
    char           szCredits[ 5 ][ 66 ];
} NGHEADER;

/* Structure to hold a tidy version of the header. */

typedef struct 
{
    unsigned short usMenuCount;
    char           szTitle[ 41 ];
    char           szCredits[ 5 ][ 67 ];
} NGTIDYHEAD;

/* Handy defines. */

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

/* Prototype functions found in this file. */

void ProcessNgRequest( int, char ** );
int  LoadWhat( char * );
void ReadNG( FILE *, int, long );
void ShowMainMenu( NGTIDYHEAD *, FILE *, int );
void SkipSection( FILE * );
long FindFirstShort( FILE * );
void ShowMenu( FILE *, int );
void GetStrZ( FILE *, char *, int );
int  ReadWord( FILE * );
long ReadLong( FILE * );
void ShowText( FILE *, long );
void ShowShort( FILE * );
void ShowLong( FILE * );
void ExpandText( char * );
unsigned char SaneText( unsigned char );
int  Hex2Byte( char * );
void StandardButtons( void );
void TidyHeader( NGHEADER *, NGTIDYHEAD * );
void cgiHeader( void );
void cgiFooter( void );
void PrintHtmlChar( char );
void LoadConfig( void );
#if defined( __MSDOS__ ) || defined( __EMX__ )
char *GetBaseName( char * );
char *UnDosifyPath( char * );
#endif

/* The version number of w3ng. */

#define W3NG_VERSION "1.03"

/* If you are compiling under Dos in 16bit mode, then un-comment the
   following: */

/* #define _16_BIT_COMPILER_ */

/* Types of items that can be loaded. */

#define LOAD_INFO    1
#define LOAD_MENU    2
#define LOAD_FMENU   3
#define LOAD_TEXT    4
#define LOAD_HREF    5
#define LOAD_UNKNOWN 6

#if defined( __MSDOS__ ) || defined( __EMX__ )
#define strncasecmp( x, y, z ) strnicmp( x, y, z )
#endif

/* Decrypt the NGs "encryption". */

#define NG_DECRYPT( x ) ( (unsigned char) ( x ^ 0x1A ) )

/* Read a byte and decrypt it. */

#define NG_READBYTE( f ) ( NG_DECRYPT( getc( f ) ) )

/* A couple of file wide variables because I was feeling lazy. */

char *pszBasename;     /* The basename of this program. */
char *pszFile;         /* The name of the NG we are reading. */

/* Change this to point to the location of the config file. */

#define CONFIG_FILE    "/usr/local/etc/httpd/conf/w3ng.conf"

/* Variable settinsg that affect the look of the generated HTML. */

char *pszBodyString = "<BODY>";
int  iFrames        = FALSE;
char *pszFrameCols  = "30%,70%";

/*
 * Function.: main()
 * Prototype: int main( int argc, char **argv )
 * Arguments: You need me to explain that?
 * Returns..: Program exit status.
 * Notes....: It's main!
 */

int main( int argc, char **argv )
{
    if ( argc > 1 )
    {

#if defined( __MSDOS__ ) || defined( __EMX__ )

	/* 
	 * If we are running on an OS that gives the full path to the exe
	 * attempt to strip if off, we only want the name of the EXE. I
	 * could have hard coded this, but what if you change the name of
	 * the executable?
	 */

	pszBasename = GetBaseName( argv[ 0 ] );

	/*
	 * Tidy up the name of the Norton Guide database. This means
	 * tunring all the \s into /s.
	 */

	pszFile = UnDosifyPath( argv[ 1 ] );

#else

	/*
	 * With luck we are running on some kind of Unix clone, in which
	 * case, unless I'm wrong, argv[ 0 ] is just the basename of
	 * the executable.
	 */

	pszBasename = argv[ 0 ];
	pszFile     = argv[ 1 ];
#endif
	
	LoadConfig();
	ProcessNgRequest( argc, argv );
	cfgReset();

#if defined( __MSDOS__ ) || defined( __EMX__ )

	free( pszBasename );
	free( pszFile );

#endif
    }
    else
    {
	cgiHeader();
	printf( "usage: %s &lt;file&gt; [ -menu | -info | -href |"
	       " &lt;offset&gt; ]\n", argv[ 0 ] );
	cgiFooter();
    }
    
    return( 0 );
}

/*
 * Function.: cgiHeader()
 * Prototype: void cgiHeader( void )
 * Arguments: None.
 * Returns..: Nothing.
 * Notes....: Display the usual cgi header thingy.
 */

void cgiHeader( void )
{
    printf( "Content-type: text/html\n\n<HTML>\n" );
}


/*
 * Function.: cgiFooter()
 * Prototype: void cgiFooter( void )
 * Arguments: None.
 * Returns..: Nothing.
 * Notes....: Display a footer that says what was used to generate the 
 * .........: page and display my email address just in case. I won't
 * .........: blame you if you change this or take it out.
 */

void cgiFooter( void )
{
    printf( "<HR>This page created by w3ng v" W3NG_VERSION 
	   ", the Norton Guide to HTML conversion utility. "
	   "Written by <A HREF=\"http://www.acemake.com/hagbard\">"
	   "Dave Pearson</A><HR>\n</HTML>\n" );
}

/*
 * Function.: ProcessNgRequest()
 * Prototype: void ProcessNgRequest( int argc, char **argv )
 * Arguments: Guess.
 * Returns..: Nothing.
 * Notes....: Take the command line, work out what needs doing and farm
 * .........: out the work.
 */

void ProcessNgRequest( int argc, char **argv )
{
    int  iLoadUp = LOAD_UNKNOWN;
    long lOffset = 0;
    FILE *fleNG  = fopen( pszFile, "rb" );
    
    if ( fleNG )
    {
	if ( argc > 2 )
	{
	    /* If argv[ 2 ] starts with a '-'.... */

	    if ( *argv[ 2 ] == '-' )
	    {
		/* ....we expect it to be a "command". */

		iLoadUp = LoadWhat( &argv[ 2 ][ 1 ] );
	    }
	    else
	    {

		/*
		 * otherwise it should be a value that is the byte offset
		 * of a page to view from the NG.
		 */

		lOffset = atol( argv[ 2 ] );
		iLoadUp = LOAD_TEXT;
	    }
	}

	/* Now we know what we are suposed to be doing, go read the guide. */

	ReadNG( fleNG, iLoadUp, lOffset );
	fclose( fleNG );
    }
    else
    {
	printf( "Error: Can't find %s for opening\n", pszFile );
    }
}

/*
 * Function.: LoadWhat()
 * Prototype: int LoadWhat( char *pszCmd )
 * Arguments: <pszCmd> is a pointer to a null terminated string. This will
 * .........: contain a "command".
 * Returns..: One of the LOAD_* constants to tell the caller what the
 * .........: command asked for.
 * Notes....: Takes a string command and works out what the caller is asking
 * .........: for.
 */

int LoadWhat( char *pszCmd )
{
    int iWhat = LOAD_UNKNOWN;
    
    if ( strncasecmp( pszCmd, "INFO", 4 ) == 0 )
    {
	/* Display the "info" or "about" info for the guide. */
	iWhat = LOAD_INFO;
    }
    else if ( strncasecmp( pszCmd, "MENU", 4 ) == 0 )
    {
	/* Display the top level menu. */
	iWhat = LOAD_MENU;
    }
    else if ( strncasecmp( pszCmd, "FMENU", 5 ) == 0 )
    {
	/* Display the framed menu. */
	iWhat = LOAD_FMENU;
    }
    else if ( strncasecmp( pszCmd, "HREF", 4 ) == 0 )
    {
	/* Display a href for the guide. */
	iWhat = LOAD_HREF;
    }
    
    return( iWhat );
}

/*
 * Function.: ReadNG()
 * Prototype: void ReadNG( FILE *fleNG, int iLoadUp, long lOffset )
 * Arguments: <fleNG> is the (FILE *) handle for the Norton Guide.
 * .........: <iLoadUp> is the ID of what we should load.
 * .........: <lOffset> is the offset if this is a page and not a menu.
 * Returns..: Nothing.
 * Notes....: Based on what we are suppose to do, this function 
 * .........: DoesTheRightThing(TM).
 */

void ReadNG( FILE *fleNG, int iLoadUp, long lOffset )
{
    NGHEADER   ngHeader;
    NGTIDYHEAD ngTidyHead;

    /* 
     * Read in the header. Note that for the moment I don't check the
     * magic number, so if you try to read a file that isn't a guide
     * things could get strange. I will tidy this up at some point.
     */

    (void) fread( &ngHeader, sizeof( NGHEADER ), 1, fleNG );

    /* Now tidy up the header. */

    TidyHeader( &ngHeader, &ngTidyHead );

    /* We could be looking at a guide that has no menu, if so, find the
       first short entry and use that instead. */

    if ( !ngTidyHead.usMenuCount && ( iLoadUp == LOAD_MENU || 
				     iLoadUp == LOAD_FMENU ))
    {
	iLoadUp = LOAD_TEXT;
	if ( !( lOffset = FindFirstShort( fleNG ) ) )
	{
	    printf( "w3ng error: Can't find a menu or short entry\n" );
	    exit( 1 );
	}
    }
    
    switch ( iLoadUp )
    {

    case LOAD_INFO : /* Load the "info" or "about" details for the guide. */
	cgiHeader();
	printf( "<HEAD><TITLE>%s - Information</TITLE></HEAD>\n", 
	       ngTidyHead.szTitle );
	printf( "%s<PRE>  Title: %s\n", pszBodyString, ngTidyHead.szTitle );
	printf( "Credits: %s\n"
	       "         %s\n"
	       "         %s\n"
	       "         %s\n"
	       "         %s\n</PRE></BODY>", 
	       ngTidyHead.szCredits[ 0 ],
	       ngTidyHead.szCredits[ 1 ],
	       ngTidyHead.szCredits[ 2 ],
	       ngTidyHead.szCredits[ 3 ],
	       ngTidyHead.szCredits[ 4 ] );
	cgiFooter();
	break;

    case LOAD_MENU : /* Load the top level menu for the guide */
	cgiHeader();
	printf( "<HEAD><TITLE>%s - Menu</TITLE></HEAD>\n", 
	       ngTidyHead.szTitle );
	if ( iFrames )
	{
	    printf( "<FRAMESET COLS=\"%s\">\n<NOFRAMES>", pszFrameCols );
	}
	printf( "%s\n", pszBodyString );
	ShowMainMenu( &ngTidyHead, fleNG, FALSE );
	printf( "</BODY>" );
	if ( iFrames )
	{
	    printf( "</NOFRAMES>\n<FRAME SRC=\"/cgi-bin/%s?%s+-fmenu\" "
		   "NAME=\"menu\">\n<FRAME SRC=\"/cgi-bin/%s?%s+-info\" "
		   "NAME=\"display\">\n</FRAMESET>\n</HTML>\n", pszBasename, 
		   pszFile, pszBasename, pszFile );
	}
	else
	{
	    cgiFooter();
	}
	break;

    case LOAD_FMENU : /* Load the framed menu. */
	cgiHeader();
	printf( "<HEAD><TITLE>%s - Menu</TITLE></HEAD>\n", 
	       ngTidyHead.szTitle );
	printf( "%s\n", pszBodyString );
	ShowMainMenu( &ngTidyHead, fleNG, TRUE );
	printf( "</BODY>" );
	cgiFooter();
	break;

    case LOAD_TEXT : /* Load a page from the guide. */
	cgiHeader();
	printf( "<HEAD><TITLE>%s - Text</TITLE></HEAD>\n", 
	       ngTidyHead.szTitle );
	printf( "%s<PRE>\n", pszBodyString );
	ShowText( fleNG, lOffset );
	printf( "</PRE></BODY>\n" );
	cgiFooter();
	break;

    case LOAD_HREF : /* Display a href for the guide. */
	printf( "<A HREF=\"/cgi-bin/%s?%s+-menu\">%s</A>",
	       pszBasename, pszFile, ngTidyHead.szTitle );

    }
}

/*
 * Function.: ShowMainMenu()
 * Prototype: void ShowMainMenu( NGTIDYHEAD *ngHeader, FILE *fleNG )
 * Arguments: <ngHeader> is a NGTIDYHEAD structure populated with cleaned
 * .........: up contents of the NG header.
 * .........: <fleNG> is the (FILE *) handle of the NG.
 * Returns..: Nothing.
 * Notes....: Scans thru the NG, finds the main menu and displays it.
 */

void ShowMainMenu( NGTIDYHEAD *ngHeader, FILE *fleNG, int iFramed )
{
    int iID;
    int i = 0;

    printf( "<OL>\n" );
    
    do
    {
	iID = ReadWord( fleNG );
	
	switch ( iID )
	{
	case 0 :
	case 1 :
	    SkipSection( fleNG );
	    break;
	case 2 :
	    ShowMenu( fleNG, iFramed );
	    ++i;
	    break;
	default :
	    iID = 5;
	}
    } while ( iID != 5 && i != ngHeader->usMenuCount );
    
    printf( "</OL>\n" );
}

/*
 */

long FindFirstShort( FILE *f )
{
    long lOffset = 0L;
    int  iID;

    do
    {
	iID = ReadWord( f );
	
	switch( iID )
	{
	case 0 :
	    lOffset = ftell( f ) - (long) sizeof( short );
	    iID = 5;
	    break;
	case 1 :
	    SkipSection( f );
	    break;
	case 2 :
	    SkipSection( f );
	    break;
	default :
	    iID = 5;
	} 
    } while ( iID != 5 );

    return( lOffset );
}

/*
 */

void SkipSection( FILE *fleNG )
{
    int iLen = ReadWord( fleNG );
    
    fseek( fleNG, (long) 22L + iLen, SEEK_CUR );
}

/*
 */

void ShowMenu( FILE *fleNG, int iFramed )
{
    int iItems;
    int i;
    long l;
    char szTitle[ 51 ];
    long *lpOffsets;
    
    (void) ReadWord( fleNG );
    iItems = ReadWord( fleNG );
    
    fseek( fleNG, 20L, SEEK_CUR );
    
    lpOffsets = (long *) calloc( iItems, sizeof( long ) );
    
    assert( lpOffsets ); /* Ouch! */
    
    for ( i = 1; i < iItems; i++ )
    {
	lpOffsets[ i - 1 ] = ReadLong( fleNG );
    }
    
    l = ftell( fleNG );
    l += ( 8 * iItems );
    
    fseek( fleNG, l, SEEK_SET );
    
    GetStrZ( fleNG, szTitle, 40 );

    printf( "<H2><LI>%s</LI></H2><P>\n", szTitle );
    printf( "<OL>\n" );
    
    for ( i = 1; i < iItems; i++ )
    {
	GetStrZ( fleNG, szTitle, 50 );

	if ( !*szTitle )
	{
	    strcpy( szTitle, "(No Text For Menu Item)" );
	}
	
	printf( "<H3><LI><A HREF=\"/cgi-bin/%s?%s+%ld\""
	       "%s>%s</H3></A></LI><P>\n", pszBasename, pszFile,
	       lpOffsets[ i - 1 ], iFramed ? " TARGET=\"display\"" : "",
	       szTitle );
    }
    
    printf( "</OL>\n" );
    
    (void) getc( fleNG );
    
    free( lpOffsets );
}

/*
 */

void ShowText( FILE *fleNG, long lOffset )
{
    int iID;
    
    fseek( fleNG, lOffset, SEEK_SET );
    
    iID = ReadWord( fleNG );
    
    if ( iID == 0 )
    {
	ShowShort( fleNG );
    }
    else if ( iID == 1 )
    {
	ShowLong( fleNG );
    }
    else
    {
	printf( "%s error: Unknown type ID of #%d", pszBasename, iID );
    }
}

/*
 */

void ShowShort( FILE *fleNG )
{
    int      iItems;
    long     *plOffsets;
    int      i;
    char     szBuffer[ 512 ];
    long     lParent;
    unsigned uParentLine;
    
    (void) ReadWord( fleNG );
    iItems = ReadWord( fleNG );
    (void) ReadWord( fleNG );
    
    uParentLine = (unsigned) ReadWord( fleNG );
    lParent     = ReadLong( fleNG );
    
    fseek( fleNG, 12L, SEEK_CUR );
    
    plOffsets = (long *) calloc( iItems, sizeof( long ) );
    
    assert( plOffsets ); /* Ouch! */
    
    for ( i = 0; i < iItems; i++ )
    {
	ReadWord( fleNG );
	plOffsets[ i ] = ReadLong( fleNG );
    }
    
    printf( "</PRE>" );
    
    if ( lParent > 0 && uParentLine < 0xFFFF )
    {
	printf( "<A HREF=\"/cgi-bin/%s?%s+%ld\">[^^Up^^]</A>", 
	       pszBasename, pszFile, lParent );
    }
    else
    {
	printf( "[^^Up^^] " );
    }
    
    StandardButtons();
    
    printf( "<HR><PRE>" );
    
    for ( i = 0; i < iItems; i++ )
    {
	GetStrZ( fleNG, szBuffer, sizeof( szBuffer ) );
	if ( plOffsets[ i ] > 0 )
	{
	    printf( "<A HREF=\"/cgi-bin/%s?%s+%ld\">",
		   pszBasename, pszFile, plOffsets[ i ] );
	    ExpandText( szBuffer );
	    printf( "</A>\n" );
	}
	else
	{
	    ExpandText( szBuffer );
	    printf( "\n" );
	}
    }
    
    free( plOffsets );
}

/*
 */

void ShowLong( FILE *fleNG )
{
    int      iItems;
    int      iSeeAlsos;
    int      i;
    int      iCnt;
    long     lPrevious;
    long     lNext;
    char     szBuffer[ 512 ];
    long     *plOffsets;
    long     lParent;
    unsigned uParentLine;
    
    (void) ReadWord( fleNG );
    
    iItems      = ReadWord( fleNG );
    iSeeAlsos   = ReadWord( fleNG );
    uParentLine = (unsigned) ReadWord( fleNG );
    lParent     = ReadLong( fleNG );
    
    fseek( fleNG, 4L, SEEK_CUR );
    
    lPrevious = ReadLong( fleNG );
    lNext     = ReadLong( fleNG );
    
    lPrevious = ( lPrevious == -1L ? 0L : lPrevious );
    lNext     = ( lNext == -1L ? 0L : lNext );
    
    printf( "</PRE>" );
    if ( lPrevious )
    {
	printf( "<A HREF=\"/cgi-bin/%s?%s+%ld\">[&lt;&lt;Previous Entry]"
	       "</A> ", pszBasename, pszFile, lPrevious );
    }
    else
    {
	printf( "[&lt;&lt;Previous Entry] " );
    }
    
    if ( lParent > 0 && uParentLine < 0xFFFF )
    {
	printf( "<A HREF=\"/cgi-bin/%s?%s+%ld\">[^^Up^^]</A> ", 
	       pszBasename, pszFile, lParent );
    }
    else
    {
	printf( "[^^Up^^] " );
    }
    
    if ( lNext )
    {
	printf( "<A HREF=\"/cgi-bin/%s?%s+%ld\">[Next Entry&gt;&gt;]</A>",
	       pszBasename, pszFile, lNext );
    }
    else
    {
	printf( "[Next Entry&gt;&gt;] " );
    }
    
    StandardButtons();
    
    printf( "<HR><PRE>" );
    
    for ( i = 0; i < iItems; i++ )
    {
	GetStrZ( fleNG, szBuffer, sizeof( szBuffer ) );
	ExpandText( szBuffer );
	printf( "\n" );
    }
    
    if ( iSeeAlsos )
    {
	printf( "</PRE><HR><B>See Also:</B> " );
	
	iCnt      = ReadWord( fleNG );
	plOffsets = (long *) calloc( iCnt, sizeof( long ) );
	
	for ( i = 0; i < iCnt; i++ )
	{
	    /* There is a reason for this < 20 check, but
	       I can't remember what it is, so I'll leave
	       it in for the moment. */
	    
	    if ( i < 20 )
	    {
		plOffsets[ i ] = ReadLong( fleNG );
	    }
	    else
	    {
		fseek( fleNG, 4L, SEEK_SET );
	    }
	}
	
	for ( i = 0; i < iCnt; i++ )
	{
	    if ( i < 20 )
	    {
		GetStrZ( fleNG, szBuffer, sizeof( szBuffer ) );
		printf( "<A HREF=\"/cgi-bin/%s?%s+%ld\">"
		       "%s</A> ",
		       pszBasename, pszFile,
		       plOffsets[ i ], szBuffer );
	    }
	}
	
	printf( "<PRE>" );
    }
}

/*
 */

int ReadWord( FILE *f )
{
    unsigned char b1 = NG_READBYTE( f );
    unsigned char b2 = NG_READBYTE( f );
    
    return( ( b2 << 8 ) + b1 );
}

/*
 */

long ReadLong( FILE *f )
{
    int i1 = ReadWord( f );
    int i2 = ReadWord( f );

#ifdef _16_BIT_COMPILER_
    long l  = 0;
    int  *i = (int *) &l;

    i[ 0 ] = i1;
    i[ 1 ] = i2;

    return( (long) l );
#else
    return( ( i2 << 16 ) + i1 );
#endif
}

/*
 */

void GetStrZ( FILE *f, char *pszBuffer, int iMax )
{
    long lSavPos = ftell( f );
    int  fEOS = 0;
    int  i;
    
    (void) fread( pszBuffer, iMax, 1, f );
    
    for ( i = 0; i < iMax && !fEOS; i++, pszBuffer++ )
    {
	*pszBuffer = NG_DECRYPT( *pszBuffer );
	fEOS       = ( *pszBuffer == 0 );
    }
    
    fseek( f, (long) lSavPos + i, SEEK_SET );
}

/*
 */

void ExpandText( char *pszText )
{
    char *psz   = pszText;
    int  iBold  = 0;
    int  iUnder = 0;
    int  iSpaces;
    int  i;
    
    while ( *psz )
    {
	switch ( *psz )
	{
	case '^' :
	    ++psz;
	    
	    switch ( *psz )
	    {
	    case 'a' :
	    case 'A' :
		++psz;
		/* I suppose I could use the colour attribute, but for
		   the moment just skip it. */
		(void) Hex2Byte( psz );
		++psz;
		break;
	    case 'b' :
	    case 'B' :
		printf( iBold ? "</B>" : "<B>" );
		iBold = !iBold;
		break;
	    case 'c' :
	    case 'C' :
		++psz;
		PrintHtmlChar( SaneText( Hex2Byte( psz ) ) );
		++psz;
		break;
	    case 'n' :
	    case 'N' :
		if ( iBold ) printf( "</B>" );
		if ( iUnder ) printf( "</U>" );
		iBold = 0;
		iUnder = 0;
		break;
	    case 'r' :
	    case 'R' :
		/* Turn on reverse */
		break;
	    case 'u' :
	    case 'U' :
		printf( iUnder ? "</U>" : "<U>" );
		iUnder = !iUnder;
		break;
	    case '^' :
		putchar( '^' );
		break;
	    default :
		--psz;
	    }
	    break;
	case (char) 0xFF :
	    ++psz;
	    
	    iSpaces = *psz;
	    
	    for ( i = 0; i < iSpaces; i++ )
	    {
		putchar( ' ' );
	    }
	    break;
	default :
	    PrintHtmlChar( SaneText( *psz ) );
	    break;
	}
	
	++psz;
    }
    
    if ( iBold ) printf( "</B>" );
    if ( iUnder ) printf( "</U>" );
}

/*
 */

unsigned char SaneText( unsigned char bChar )
{
    switch ( bChar )
    {
    case 0xB3 :            /* Vertical graphics. */
    case 0xB4 :
    case 0xB5 :
    case 0xB6 :
    case 0xB9 :
    case 0xBA :
    case 0xC3 :
    case 0xCC :
	bChar = '|';
	break;
    case 0xC4 :            /* Horizontal graphics. */
    case 0xC1 :
    case 0xC2 :
    case 0xC6 :
    case 0xC7 :
    case 0xCA :
    case 0xCB :
    case 0xCD :
    case 0xCF :
    case 0xD0 :
    case 0xD1 :
    case 0xD2 :
	bChar = '-';
	break;
    case 0xB7 :            /* Corner graphics. */
    case 0xB8 :
    case 0xBB :
    case 0xBC :
    case 0xBD :
    case 0xBE :
    case 0xBF :
    case 0xC0 :
    case 0xC5 :
    case 0xC8 :
    case 0xC9 :
    case 0xCE :
    case 0xD3 :
    case 0xD4 :
    case 0xD5 :
    case 0xD6 :
    case 0xD7 :
    case 0xD8 :
    case 0xD9 :
    case 0xDA :
	bChar = '+';
	break;
    case 0xDB :            /* Block graphics */
    case 0xDC :
    case 0xDD :
    case 0xDE :
    case 0xDF :
    case 0xB0 :
    case 0xB1 :
    case 0xB2 :
	bChar = '#';
	break;
    default :
	if ( ( bChar < ' ' || bChar > 0x7E ) && bChar )
	{
	    bChar = '.';
	}
    }
    
    return( bChar );
}

/*
 */

int Hex2Byte( char *psz )
{
    int iByte;
    int i;
    unsigned char bByte = 0;
    
    for ( i = 0; i < 2; i++, psz++ )
    {
	*psz = (char) toupper( *psz );
	
	if ( *psz > '/' && *psz < ':' )
	{
	    iByte = ( ( (int) *psz ) - '0' );
	}
	else if ( *psz > '@' && *psz < 'G' )
	{
	    iByte = ( ( (int) *psz ) - '7' );
	}
	
	bByte += ( iByte * ( !i ? 16 : 1 ) );
    }
    
    return( (int) bByte );
}

/*
 */

void StandardButtons( void )
{
    printf( " <A HREF=\"/cgi-bin/%s?%s+-%s\"%s>[Menu]</A>",
	   pszBasename, pszFile, iFrames ? "fmenu" : "menu",
	   iFrames ? " TARGET=\"menu\"" : "" );
    printf( " <A HREF=\"/cgi-bin/%s?%s+-info\">"
	   "[About The Guide]</A>", pszBasename, pszFile );
}

/*
 */

void TidyHeader( NGHEADER *pngHeader, NGTIDYHEAD *pngTidy )
{
    int i;
    int n;

    for ( i = 0; i < 40; i++ )
    {
	pngHeader->szTitle[ i ] = SaneText( pngHeader->szTitle[ i ] );
    }

    for ( i = 0; i < 5; i++ )
    {
	for ( n = 0; n < 66; n++ )
	{
	    pngHeader->szCredits[ i ][ n ] = 
		SaneText( pngHeader->szCredits[ i ][ n ] );
	}
    }

    pngTidy->usMenuCount = pngHeader->usMenuCount;
    strncpy( pngTidy->szTitle, pngHeader->szTitle, 40 );

    for ( i = 0; i < 5; i++ )
    {
	strncpy( pngTidy->szCredits[ i ], pngHeader->szCredits[ i ], 66 );
    }
}

/*
 */

void PrintHtmlChar( char c )
{
    switch ( c )
    {
    case '<' :
	printf( "&lt;" );
	break;
    case '>' :
	printf( "&gt;" );
	break;
    case '&' :
        printf( "&amp;" );
	break;
    case 0x0 :
	printf( " " );
	break;
    default :
	printf( "%c", c );
    }
}

/*
 */

void LoadConfig( void )
{
    char *p;

    if ( cfgReadFile( CONFIG_FILE ) )
    {
	if ( cfgGetSetting( "BODY" ) )
	{
	    pszBodyString = cfgGetSetting( "BODY" );
	}
	if ( cfgGetSetting( "FRAMECOLS" ) )
	{
	    pszFrameCols  = cfgGetSetting( "FRAMECOLS" );
	}

	if ( ( p = cfgGetSetting( "FRAMES" ) ) )
	{
	    iFrames = ( *p == '1' || *p == 'Y' || *p == 'y' );
	}
    }
}

#if defined( __MSDOS__ ) || defined( __EMX__ )

/*
 */

char *GetBaseName( char *pszExePath )
{
    char szFile[ _MAX_FNAME ];
    char szExt[ _MAX_EXT ];
    char szWork[ _MAX_PATH ];
    char *pszBase;
    
    _splitpath( pszExePath, NULL, NULL, szFile, szExt );
    _makepath( szWork, "", "", szFile, szExt );
    
    pszBase = malloc( strlen( szWork ) + 1 );
    
    assert( pszBase ); /* Ouch! */
    
    strcpy( pszBase, szWork );
    
    return( pszBase );
}

/*
 */

char *UnDosifyPath( char *pszPath )
{
    char *pszSanePath = malloc( strlen( pszPath ) + 1 );
    char *p           = pszSanePath;
    
    assert( pszSanePath ); /* Ouch! */
    
    strcpy( pszSanePath, pszPath );
    
    while ( *p )
    {
	if ( *p == '\\' )
	{
	    *p = '/';
	}
	++p;
    }
    
    return( pszSanePath );
}

#endif
