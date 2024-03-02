/*
===========================================================================
Copyright (C) 2015 Zack Middleton

This file is part of BSP sekai Source Code.

BSP sekai Source Code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 3 of the License,
or (at your option) any later version.

BSP sekai Source Code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with BSP sekai Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, BSP sekai Source Code is also subject to certain additional terms.
You should have received a copy of these additional terms immediately following
the terms and conditions of the GNU General Public License.  If not, please
request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional
terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc.,
Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/

#include <dirent.h>
#include <sys/stat.h>
#include "sekai.h"
#include "bsp.h"
#include "shaders.h"

// convert_nsco.c
void ConvertNscoToNscoET( bspFile_t *bsp );
void ConvertNscoETToNsco( bspFile_t *bsp );

int main( int argc, char **argv ) {
	char baseFile[1024];
	char shaderOutputFile[1024];
	char *mapName;
	bspFile_t *bsp;
	int saveLength;
	void *saveData;
	char *conversion, *inputFile, *formatName, *outputFile;
	bspFormat_t *outFormat;
	void (*convertFunc)( bspFile_t *bsp );

	if ( argc < 5 ) {
		Com_Printf( "bspsekai <conversion> <input-BSP> <format> <output-BSP>\n" );
		Com_Printf( "BSP sekai - v0.2\n" );
		Com_Printf( "Convert a BSP for use on a different engine\n" );
		Com_Printf( "BSP conversion can lose data, keep the original BSP!\n" );
		Com_Printf( "\n" );
		Com_Printf( "<conversion> specifies how surface and content flags are remapped.\n" );
		Com_Printf( "Conversion list:\n" );
		Com_Printf( "  none      - No conversion.\n" );
		Com_Printf( "  nsco2et   - Convert Navy SEALS: Covert Operation surface/content flags to ET values.\n" );
		Com_Printf( "  et2nsco   - Convert ET surface/content flags to Navy SEALS: Covert Operation values.\n" );
		Com_Printf( "\n" );
		Com_Printf( "The format of <input-BSP> is automatically determined from the file.\n" );
		Com_Printf( "Input BSP formats: (not all are fully supported)\n" );
		Com_Printf( "  Quake 3 (including pre-releases formats), RTCW, ET, EF, EF2, FAKK, Alice, Dark Salvation, MOHAA, SoF2, JK2, JA, Iron-Grid: Warlord, Daikatana\n" );
		Com_Printf( "\n" );
		Com_Printf( "<format> is used to determine output BSP format.\n" );
		Com_Printf( "BSP format list:\n" );
		Com_Printf( "  quake3    - Quake 3.\n" );
		//Com_Printf( "  q3test106 - Q3Test 1.06/1.07/1.08. Later Q3Test versions use 'quake3' format.\n" );
		Com_Printf( "  rtcw      - Return to Castle Wolfenstein.\n" );
		Com_Printf( "  et        - Wolfenstein: Enemy Territory.\n" );
		Com_Printf( "  darks     - Dark Salvation.\n" );
		//Com_Printf( "  rbsp      - Raven's BSP format used by SoF2, Jedi Knight 2, and Jedi Academy.\n" );
		//Com_Printf( "  fakk      - Heavy Metal: FAKK2.\n" );
		//Com_Printf( "  alice     - American McGee's Alice.\n" );
		//Com_Printf( "  ef2       - Elite Force 2.\n" );
		//Com_Printf( "  mohaa     - Medal of Honor Allied Assult.\n" );
		Com_Printf( "  dk        - Daikatana.\n" );
		return 0;
	}

	conversion = argv[1];
	inputFile = argv[2];
	formatName = argv[3];
	outputFile = argv[4];

	COM_StripExtension( outputFile, baseFile );
	mapName = COM_SkipPath( baseFile );

	if ( Q_stricmp( conversion, "none" ) == 0 ) {
		convertFunc = NULL;
	} else if ( Q_stricmp( conversion, "nsco2et" ) == 0 ) {
		convertFunc = ConvertNscoToNscoET;
	} else if ( Q_stricmp( conversion, "et2nsco" ) == 0 ) {
		convertFunc = ConvertNscoETToNsco;
	} else {
		Com_Printf( "Error: Unknown conversion '%s'.\n", conversion );
		return 1;
	}

	if ( Q_stricmp( formatName, "quake3" ) == 0 ) {
		outFormat = &quake3BspFormat;
	} else if ( Q_stricmp( formatName, "rtcw" ) == 0 ) {
		outFormat = &wolfBspFormat;
	} else if ( Q_stricmp( formatName, "et" ) == 0 ) {
		// ZTM: TODO: This need to be a different format than RTCW so that there is a different save function; so that converting et maps to rtcw can convert foliage
		outFormat = &wolfBspFormat;
	} else if ( Q_stricmp( formatName, "darks" ) == 0 ) {
		outFormat = &darksBspFormat;
	} else if ( Q_stricmp( formatName, "rbsp" ) == 0 ) {
		outFormat = &sof2BspFormat;
	} else if ( Q_stricmp( formatName, "fakk" ) == 0 ) {
		outFormat = &fakkBspFormat;
	} else if ( Q_stricmp( formatName, "alice" ) == 0 ) {
		outFormat = &aliceBspFormat;
	} else if ( Q_stricmp( formatName, "ef2" ) == 0 ) {
		outFormat = &ef2BspFormat;
	} else if ( Q_stricmp( formatName, "mohaa" ) == 0 ) {
		outFormat = &mohaaBspFormat;
	} else if ( Q_stricmp( formatName, "q3test106" ) == 0 ) {
		outFormat = &q3Test106BspFormat;
	} else if ( Q_stricmp( formatName, "dk" ) == 0 ) {
		outFormat = &daikatanaBspFormat;
	} else {
		Com_Printf( "Error: Unknown format '%s'.\n", formatName );
		return 1;
	}

	if ( Q_stricmp( inputFile, "-" ) == 0 || Q_stricmp( outputFile, "-" ) == 0 ) {
		Com_Printf( "Error: reading / writing to stdout is not supported.\n" );
		return 1;
	}

	// this will work, but might result in user overwritting original BSP without backup. so let's baby the user. >.>
	if ( Q_stricmp( inputFile, outputFile ) == 0 ) {
		Com_Printf( "Error: same input and output file (exiting to avoid data lose)\n" );
		return 1;
	}

	if ( outFormat->shaderDir && *outFormat->shaderDir ) {
		InitShaders( outFormat->shaderDir, mapName );
	}
	bsp = BSP_Load( inputFile );

	if ( !bsp ) {
		Com_Printf( "Error: Could not read file '%s'\n", inputFile );
		return 1;
	}

	Com_Printf( "Loaded BSP '%s' successfully.\n", inputFile );

	if ( outFormat->saveFunction ) {
		if ( convertFunc ) {
			convertFunc( bsp );
		}

		saveData = NULL;
		saveLength = outFormat->saveFunction( outFormat, outputFile, bsp, &saveData );

		if ( saveData && FS_WriteFile( outputFile, saveData, saveLength ) == saveLength ) {
			Com_Printf( "Saved BSP '%s' successfully.\n", outputFile );

			WriteShaders();
			/*
			sprintf( shaderOutputFile, "%s/%s_gen.shader", outFormat->shaderDir ? outFormat->shaderDir : "scripts", mapName );

			if  ( bsp->shaderString && FS_WriteFile( shaderOutputFile, bsp->shaderString, bsp->shaderStringLength ) ) {
				Com_Printf( "Saved generated shaders to '%s' successfully.\n", shaderOutputFile );
			} else {
				Com_Printf( "Saving generated shaders to '%s' failed.\n", shaderOutputFile );
			}
			*/
		} else {
			Com_Printf( "Saving BSP '%s' failed.\n", outputFile );
		}

		if ( saveData ) {
			free( saveData );
		}
	} else {
		Com_Printf( "BSP format for '%s' does not support saving.\n", outFormat->gameName );
	}

	BSP_Free( bsp );

	return 0;
}

long FS_WriteFile( const char *filename, void *buf, long length ) {
	FILE *f;

	f = fopen( filename, "wb" );

	if ( !f ) {
		return 0;
	}

	if ( fwrite( buf, length, 1, f ) != 1 ) {
		fclose( f );
		return 0;
	}

	fclose( f );

	return length;
}

long FS_ReadFile( const char *filename, void **buffer ) {
	FILE *f;
	long length;
	void *buf;

	*buffer = NULL;

	f = fopen( filename, "rb" );

	if ( !f ) {
		return 0;
	}

	fseek( f, 0, SEEK_END );
	length = ftell( f );
	fseek( f, 0, SEEK_SET );

	buf = malloc( length );

	if ( fread( buf, length, 1, f ) != 1 ) {
		fclose( f );
		free( buf );
		return 0;
	}

	fclose( f );

	*buffer = buf;
	return length;
}

void FS_FreeFile( void *buffer ) {
	if ( buffer ) {
		free( buffer );
	}
}

static char *copystring(char *s)
{
	char	*b;
	b = malloc( strlen(s) + 1 );
	strcpy( b, s );
	return b;
}

#define MAX_OSPATH 1024
#define MAX_FOUND_FILES 4096

char **FS_ListFiles( const char *directory, const char *extension, int *numFiles ) {
	struct dirent *d;
	DIR           *fdir;
	char          search[MAX_OSPATH];
	int           nfiles;
	static char   *list[MAX_FOUND_FILES];
	char		  **copyList;
	int           i;
	struct stat   st;
	int			  dironly = 0;

	int           extLen;

	memset( list, 0, sizeof( list ) );

	if ( !directory || directory[0] == '\0' ) {
		*numFiles = 0;
		return 0;
	}

	if ( !extension ) {
		extension = "";
	}

	if ( extension[0] == '/' && extension[1] == 0 ) {
		extension = "";
		dironly = 1;
	}

	extLen = strlen( extension );

	// search
	nfiles = 0;

	if ( ( fdir = opendir( directory ) ) == NULL ) {
		*numFiles = 0;
		return 0;
	}

	while ( ( d = readdir( fdir ) ) != NULL ) {
		snprintf( search, sizeof(search), "%s/%s", directory, d->d_name );
		if ( stat( search, &st ) == -1) {
			continue;
		}
		if ( ( dironly && !( st.st_mode & S_IFDIR ) ) ||
			( !dironly && ( st.st_mode & S_IFDIR ) ) ) {
			continue;
		}
		if ( *extension ) {
			if ( strlen( d->d_name ) < extLen ||
				Q_stricmp(
					d->d_name + strlen( d->d_name ) - extLen,
					extension ) ) {
				continue; // didn't match
			}
		}

		if ( nfiles == MAX_FOUND_FILES - 1 ) {
			break;
		}
		list[ nfiles ] = copystring( d->d_name );
		nfiles++;
	}

	list[ nfiles ] = NULL;

	closedir( fdir );

	*numFiles = nfiles;

	if ( !nfiles ) {
		return NULL;
	}

	copyList = malloc( nfiles * sizeof(char*) );
	for ( i = 0 ; i < nfiles ; i++ ) {
		copyList[ i ] = list[ i ];
	}

	return copyList;
}

void FS_FreeFileList( char **list, int numFiles ) {
	int i;

	if ( !list || !numFiles ) {
		return;
	}

	for ( i = 0 ; i < numFiles ; i++ ) {
		if ( list[i] ) {
			free( list[i] );
			list[i] = 0;
		}
	}
	free( list );
}
