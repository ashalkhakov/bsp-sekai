/*
===========================================================================
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of Spearmint Source Code.
Modified to fit into BSP Sekai.

Spearmint Source Code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 3 of the License,
or (at your option) any later version.

Spearmint Source Code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Spearmint Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, Spearmint Source Code is also subject to certain additional terms.
You should have received a copy of these additional terms immediately following
the terms and conditions of the GNU General Public License.  If not, please
request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional
terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc.,
Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/

#include <ctype.h>
#include <string.h>
#include "sekai.h"
#include "shaders.h"
#include "image.h"

/*
===========================================================================

PARSING

===========================================================================
*/

#define MAX_TOKEN_CHARS 1024
static	char	com_token[MAX_TOKEN_CHARS];
static	char	com_parsename[MAX_TOKEN_CHARS];
static	int		com_lines;
static	int		com_tokenline;

static void COM_BeginParseSession( const char *name ) {
	com_lines = 1;
	com_tokenline = 0;
	snprintf( com_parsename, sizeof( com_parsename ), "%s", name );
}

static int COM_GetCurrentParseLine( void ) {
	if ( com_tokenline ) {
		return com_tokenline;
	}

	return com_lines;
}

static char *SkipWhitespace( char *data, int *linesSkipped ) {
	int c;

	while ( ( c = *data ) <= ' ' ) {
		if ( !c ) {
			return NULL;
		}
		if ( c == '\n' ) {
			*linesSkipped += 1;
		}
		data++;
	}

	return data;
}

static char *COM_ParseExt2( char **data_p, qboolean allowLineBreaks, char delimiter ) {
	int c = 0, len;
	int linesSkipped = 0;
	char *data;

	data = *data_p;
	len = 0;
	com_token[0] = 0;
	com_tokenline = 0;

	// make sure incoming data is valid
	if ( !data ) {
		*data_p = NULL;
		return com_token;
	}

	while ( 1 ) {
		// skip whitespace
		data = SkipWhitespace( data, &linesSkipped );
		if ( !data ) {
			*data_p = NULL;
			return com_token;
		}
		if ( data && linesSkipped && !allowLineBreaks ) {
			// ZTM: Don't move the pointer so that calling SkipRestOfLine afterwards works as expected
			//*data_p = data;
			return com_token;
		}

		com_lines += linesSkipped;

		c = *data;

		// skip double slash comments
		if ( c == '/' && data[1] == '/' ) {
			data += 2;
			while ( *data && *data != '\n' ) {
				data++;
			}
		}
		// skip /* */ comments
		else if ( c == '/' && data[1] == '*' ) {
			data += 2;
			while ( *data && ( *data != '*' || data[1] != '/' ) ) {
				if ( *data == '\n' ) {
					com_lines++;
				}
				data++;
			}
			if ( *data ) {
				data += 2;
			}
		} else {
			break;
		}
	}

	// token starts on this line
	com_tokenline = com_lines;

	// handle quoted strings
	if ( c == '\"' ) {
		data++;
		while ( 1 ) {
			c = *data++;
			if ( c =='\"' || !c ) {
				com_token[len] = 0;
				*data_p = (char *)data;
				return com_token;
			}
			if ( c == '\n' ) {
				com_lines++;
			}
			if ( len < MAX_TOKEN_CHARS - 1 ) {
				com_token[len] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do {
		if ( len < MAX_TOKEN_CHARS - 1 ) {
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while ( c > 32 && c != delimiter );

	com_token[len] = 0;

	*data_p = (char *)data;
	return com_token;
}

static char *COM_ParseExt( char **data_p, qboolean allowLineBreaks ) {
	return COM_ParseExt2(data_p, allowLineBreaks, 0);
}

static qboolean SkipBracedSection( char **program, int depth ) {
	char			*token;

	do {
		token = COM_ParseExt( program, qtrue );
		if ( token[1] == 0 ) {
			if ( token[0] == '{' ) {
				depth++;
			}
			else if ( token[0] == '}' ) {
				depth--;
			}
		}
	} while ( depth && *program );

	return ( depth == 0 );
}

/*
===========================================================================

SHADER GENERATION

===========================================================================
*/

static qboolean			initialized;

static char				shaderBasePath[1024];

static infoParm_t		infoParms[MAX_INFOPARMS];
static int				numInfoParms;

#define	MAX_SHADER_FILES	4096
#define FILE_HASH_SIZE		1024

static shaderSource_t		*sourceHashTable[FILE_HASH_SIZE];
static shaderSource_t       shaderSources[MAX_SHADER_FILES];
static int                  numShaderSources;
static char					mapName[MAX_QPATH];
static shaderSource_t		*mapShaderSource;

static shader_t	        	*hashTable[FILE_HASH_SIZE];
static shader_t             shaders[MAX_SHADERS];
static int                  numShaders;

/*
================
return a hash value for the filename
================
*/
static long generateHashValue( const char *fname, const int size ) {
	int		i;
	long	hash;
	char	letter;

	hash = 0;
	i = 0;
	while ( fname[i] != '\0' ) {
		letter = tolower( fname[i] );
		if ( letter =='.' ) break;				// don't include extension
		if ( letter =='\\' ) letter = '/';		// damn path names
		if ( letter == '/' ) letter = '/';		// damn path names
		hash += ( long )( letter ) * ( i + 119 );
		i++;
	}
	hash = ( hash ^ ( hash >> 10 ) ^ ( hash >> 20 ));
	hash &= ( size - 1 );
	return hash;
}

static shaderSource_t *FindShaderSourceByName( const char *name ) {
	int			    hash;
	shaderSource_t	*source;

    if ( !name || !name[0] ) {
        return NULL;
	}

	hash = generateHashValue( name, FILE_HASH_SIZE );

	//
	// see if the shader source is already loaded
	//
	for ( source = sourceHashTable[hash]; source; source = source->next ) {
		if ( Q_stricmp( source->filename, name ) == 0 ) {
			// match found
			return source;
		}
	}

    if ( numShaderSources == MAX_SHADER_FILES ) {
        return NULL;
    }
	source = &shaderSources[ numShaderSources++ ];
    Q_strncpyz( source->filename, name, sizeof( source->filename ) );
    source->shaders = NULL;
    source->newShaders = NULL;
	hash = generateHashValue( source->filename, FILE_HASH_SIZE );
	source->next = sourceHashTable[ hash ];
	sourceHashTable[ hash ] = source;
}

static shader_t *FindShaderByName( const char *name ) {
	char		strippedName[MAX_QPATH];
	int			hash;
	shader_t	*sh;

	if ( ( name == NULL ) || ( name[0] == 0 ) ) {
		return NULL;
	}

	COM_StripExtension( name, strippedName );

	hash = generateHashValue( strippedName, FILE_HASH_SIZE );

	//
	// see if the shader is already loaded
	//
	for ( sh = hashTable[ hash ]; sh; sh = sh->next ) {
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if ( Q_stricmp( sh->name, strippedName ) == 0 ) {
			// match found
			return sh;
		}
	}

    if ( numShaders == MAX_SHADERS ) {
        return NULL;
    }
	sh = &shaders[ numShaders++ ];
    Q_strncpyz( sh->name, strippedName, sizeof( sh->name ) );
	hash = generateHashValue( sh->name, FILE_HASH_SIZE );
	sh->next = hashTable[ hash ];
	hashTable[ hash ] = sh;
	sh->nextInFile = NULL;
    sh->file = NULL;
	sh->shaderType = ST_CLEAN;

	return sh;
}

/*
====================
ScanAndLoadShaderFiles

Finds and loads all .shader files, combining them into
a single large text block that can be scanned for shader names
=====================
*/
static void ScanAndLoadShaderFiles( void ) {
	char filename[MAX_QPATH];
    char shaderDir[1024];
	char **shaderFiles;
	char *buffer;
	char *p;
	int numShaderFiles;
	int i;
	char *oldp, *token, *hashMem;
	int hash;
    int size;
    char shaderName[MAX_QPATH];
    int shaderLine;
    shader_t *shader;
    shaderSource_t *shaderSource;

	// scan for shader files
    snprintf( shaderDir, sizeof( shaderDir ), "%s", shaderBasePath );
	shaderFiles = FS_ListFiles( shaderDir, ".shader", &numShaderFiles );

	if ( !shaderFiles || !numShaderFiles ) {
		Com_Printf("no shader files found\n");
        return;
	}

	if ( numShaderFiles > MAX_SHADER_FILES ) {
		numShaderFiles = MAX_SHADER_FILES;
	}
    
	// load and parse shader files
	for ( i = 0; i < numShaderFiles; i++ ) {
		snprintf( filename, sizeof( filename ), "%s/%s", shaderDir, shaderFiles[i] );

        shaderSource = FindShaderSourceByName(shaderFiles[i]);
        if ( !shaderSource ) {
            break;
        }
        shaderSource->sourced = qtrue;

		size = FS_ReadFile( filename, (void **)&buffer );
        if ( !size ) {
            break;
        }

		p = buffer;
		COM_BeginParseSession( filename );
		while (1) {
			token = COM_ParseExt( &p, qtrue );
			
			if ( !*token ) {
				break;
			}

			Q_strncpyz( shaderName, token, sizeof( shaderName ) );
			shaderLine = COM_GetCurrentParseLine( );

			token = COM_ParseExt( &p, qtrue );
			if( token[0] != '{' || token[1] != '\0' ) {
				Com_Printf( "WARNING: Ignoring shader file %s. Shader \"%s\" on line %d missing opening brace",
							filename, shaderName, shaderLine );
				if ( token[0] ) {
					Com_Printf( " (found \"%s\" on line %d)", token, COM_GetCurrentParseLine( ) );
				}
				Com_Printf( ".\n" );
				break;
			}

			if ( !SkipBracedSection( &p, 1 ) ) {
				Com_Printf( "WARNING: Ignoring shader file %s. Shader \"%s\" on line %d missing closing brace.\n",
							filename, shaderName, shaderLine );
				break;
			}

            shader = FindShaderByName( shaderName );
            if ( shader == NULL ) {
				Com_Printf( "not enough space\n" );
                break; // not enough space
            }

            if ( shader->file != NULL ) {
                // it's a duplicate sourced shader...
                Com_Printf("Shader \"%s\" on line %d of file %s previously defined in file %s.\n",
                	    shaderName, shaderLine, com_parsename, shader->file->filename );
            } else {
                shader->shaderType = ST_SOURCED;
                shader->file = shaderSource;
                shader->nextInFile = shaderSource->shaders;
                shaderSource->shaders = shader;
            }
		}

		if ( buffer ) {
			free( buffer );
			buffer = NULL;
		}
	}

	// free up memory
	FS_FreeFileList( shaderFiles, numShaderFiles );
}

// ==========================================

void InitShaders( const char *basepath, const char *inputMapName /*, infoParm_t *parms, int numParms */ ) {
	char		mapShaderSourceName[MAX_QPATH];

    if ( initialized || !basepath || !*basepath ) {
        return;
	}

	/*
	if ( numParms > MAX_INFOPARMS ) {
		Com_Printf( "MAX_INFOPARMS hit\n" );
		return;
	}
	*/

    Q_strncpyz( shaderBasePath,  basepath, sizeof( shaderBasePath ) );

	memset( hashTable, 0, sizeof( hashTable ) );
    memset( sourceHashTable, 0, sizeof( sourceHashTable ) );

	/*
	numInfoParms = numParms;
	memcpy( infoParms, parms, sizeof( infoParms[ 0 ] ) * numInfoParms );
	*/

	ScanAndLoadShaderFiles( );

	Q_strncpyz( mapName, inputMapName, sizeof( mapName ) );
	snprintf( mapShaderSourceName, sizeof( mapShaderSourceName ), "%s_gen.shader", mapName );

    mapShaderSource = FindShaderSourceByName( mapShaderSourceName );

    initialized = qtrue;
}

void DefineSkyBoxShader( skyBoxShader_t *skyBox, float fogColor[3], float distance, const char *editorImage ) {
    shader_t        *sh;
    int             i;
	char			name[MAX_QPATH];

    if ( !initialized ) {
        return;
	}

	snprintf( name, sizeof( name ), "textures/%s/sky", mapName );

    sh = FindShaderByName( name );
    if ( sh == NULL ) {
        // can't doo much
        return;
    }
    if ( sh->shaderType == ST_SOURCED ) {
        return;
    }

	sh->fogParms[0] = fogColor[0];
	sh->fogParms[1] = fogColor[1];
	sh->fogParms[2] = fogColor[2];
	sh->fogParms[3] = distance;

	if ( editorImage && editorImage[0] ) {
		Q_strncpyz( sh->editorImage, editorImage, sizeof( sh->editorImage ) );
	}

	memcpy( &sh->skyBox, skyBox, sizeof( sh->skyBox ) );

    sh->shaderType = ST_SKY;
    sh->file = mapShaderSource;
    sh->nextInFile = mapShaderSource->newShaders;
    mapShaderSource->newShaders = sh;
}

void DefineFogShader( const char *name, float color[3], float distance ) {
    shader_t        *sh;
    int             i;

    if ( !initialized ) {
        return;
	}

    sh = FindShaderByName( name );
    if ( sh == NULL ) {
        // can't doo much
        return;
    }
    if ( sh->shaderType == ST_SOURCED ) {
        return;
    }

	sh->fogParms[0] = color[0];
	sh->fogParms[1] = color[1];
	sh->fogParms[2] = color[2];
	sh->fogParms[3] = distance;

    sh->shaderType = ST_FOG;
    sh->file = mapShaderSource;
    sh->nextInFile = mapShaderSource->newShaders;
    mapShaderSource->newShaders = sh;
}

void DefineSimplifiedShader( const char *name, const char *diffuseImage, simplifiedShaderParms_t *parms ) {
    shader_t        *sh;
    int             i;

    if ( !initialized ) {
        return;
	}

	if ( numInfoParms > MAX_SURFACEPARMS_PER_SHADER ) {
		Com_Printf( "MAX_SURFACEPARMS_PER_SHADER: %s\n", name );
		return;
	}

    sh = FindShaderByName( name );
    if ( sh == NULL ) {
        // can't doo much
        return;
    }
    if ( sh->shaderType != ST_CLEAN ) {
        return;
    }
    Q_strncpyz( sh->diffuseMap, diffuseImage, sizeof( sh->diffuseMap ) );
	memcpy( &sh->simplifiedShaderParms, parms, sizeof( sh->simplifiedShaderParms ) );

	//sh->numSurfaceParms = numParms;
	//memcpy( sh->surfaceParms, parms, numParms * sizeof( sh->surfaceParms[0] ) );

    sh->shaderType = ST_SIMPLIFIED;
    sh->file = mapShaderSource;
    sh->nextInFile = mapShaderSource->newShaders;
    mapShaderSource->newShaders = sh;
}

static void fprint_shader( FILE *fp, shader_t *shader ) {
	int		i;

	if ( shader->shaderType == ST_CLEAN || shader->shaderType == ST_SOURCED ) {
		return;
	}

    fprintf( fp, "\n%s\n", shader->name );
    fprintf( fp, "{\n" );
	switch ( shader->shaderType ) {
	case ST_FOG:
		{
			fprintf( fp, "surfaceparm trans\nsurfaceparm nonsolid\nsurfaceparm fog\nsurfaceparm nolightmap\n" );
			fprintf( fp, "fogparms ( %f %f %f ) %f\n",
				shader->fogParms[0], shader->fogParms[1], shader->fogParms[2], shader->fogParms[3] );
		}
		break;
	case ST_SKY:
		{
			skyBoxShader_t		*box;
			skyBoxShaderLayer_t	*layer;

 			box = &shader->skyBox;

			fprintf( fp, "\tqer_editorimage %s\n", shader->editorImage );
			fprintf( fp, "\tsurfaceparm noimpact\n" "\tsurfaceparm nolightmap\n" "\tsurfaceparm sky\n");
			fprintf( fp, "\tskyparms - %f %s\n", box->height, box->sky );

			if ( shader->fogParms[3] > 0.0f ) {
				fprintf( fp, "\tfogvars ( %f %f %f ) %f\n", shader->fogParms[0], shader->fogParms[1], shader->fogParms[2], shader->fogParms[3] );
			}

			layer = box->layers;
			for ( i = 0; i < box->numLayers ; i++, layer++ ) {
				fprintf( fp, "\t\t{\n" );
				fprintf( fp, "\t\t\tmap %s\n", layer->map );

				if ( layer->alphaGenConst != 1 ) {
					fprintf( fp, "\t\t\talphaGen const %f\n", layer->alphaGenConst );
					fprintf( fp, "\t\t\tblendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA\n" );
				}

				fprintf( fp, "\t\t\ttcmod scale %f %f\n", layer->tcScale[0], layer->tcScale[1] );
				fprintf( fp, "\t\t\ttcmod scroll %f %f\n", layer->tcScroll[0], layer->tcScroll[1] );

				fprintf( fp, "\t\t}\n" );
			}
		}
		break;
	case ST_SIMPLIFIED:
		switch ( shader->simplifiedShaderParms.cull ) {
			case CT_BACK:
				fprintf( fp, "\tcull back\n" );
				break;
			case CT_FRONT:
				fprintf( fp, "\tcull front\n" );
				break;
			case CT_NONE:
				fprintf( fp, "\tcull none\n" );
				break;
		}
		switch ( shader->simplifiedShaderParms.blend ) {
			case BT_NONE:
				break;
			case BT_ALPHATEST:
				fprintf( fp, "\tsort seethrough\n" );
				break;
			case BT_BLEND:
				fprintf( fp, "\tsort additive\n" );
				break;
		}

		fprintf( fp, "\t{\n" );
		fprintf( fp, "\t\tmap %s\n", shader->diffuseMap );
		switch ( shader->simplifiedShaderParms.blend ) {
			case BT_NONE:
				break;
			case BT_ALPHATEST:
				fprintf( fp, "\t\talphaFunc GE128\n" );
				fprintf( fp, "\t\tdepthWrite\n" );
        		fprintf( fp, "\t\tdepthFunc lequal\n" );
				break;
			case BT_BLEND:
				fprintf( fp, "\t\tblendfunc blend\n" );
				break;
		}
		if ( shader->simplifiedShaderParms.alphaGenConst != 1.0f ) {
			fprintf( fp, "\t\talphaGen const %f\n", shader->simplifiedShaderParms.alphaGenConst );
		}
		if ( shader->simplifiedShaderParms.texScroll[0] || shader->simplifiedShaderParms.texScroll[1] ) {
			fprintf( fp, "\t\ttcMod scroll %f %f\n", shader->simplifiedShaderParms.texScroll[0], shader->simplifiedShaderParms.texScroll[1] );
		}
		fprintf( fp, "\t}\n" );

		if ( shader->simplifiedShaderParms.lightmapped ) {
			fprintf( fp, "\t{\n" );
			fprintf( fp, "\t\tmap $lightmap\n" );
			fprintf( fp, "\t\tblendFunc filter\n");
			fprintf( fp, "\t\tdepthFunc equal\n" );
			fprintf( fp, "\t\ttcGen lightmap\n" );
			fprintf( fp, "\t}\n" );
		}
		break;
	default:
		break;
	}
    fprintf( fp, "}\n" );
}

void WriteShaders( void ) {
    int             i;
    shaderSource_t *source;
    shader_t        *shader;
    FILE            *fp;
    char            shaderFileName[1024];

    if ( !initialized ) {
        return;
	}

	source = mapShaderSource;
	if ( !source ) {
		return;
	}

	shader = source->newShaders;
	if ( !shader ) {
		return;
	}

	snprintf( shaderFileName, sizeof( shaderFileName ), "%s/%s", shaderBasePath, source->filename );

	if ( !( fp = fopen( shaderFileName, "ab" ) ) ) {
		// unable to open the file for writing
		return;
	}

	while ( shader ) {
		Com_Printf( "Writing shader %s\n", shader->name );

		fprint_shader( fp, shader );
		shader = shader->nextInFile;
	}

	fclose( fp );
}
