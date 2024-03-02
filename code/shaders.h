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

#ifndef SHADERS_H
#define SHADERS_H

#define MAX_SURFACEPARMS_PER_SHADER		8
#define MAX_STAGES_PER_SHADER           4
#define MAX_SKYBOXLAYERS_PER_SHADER 	2

#define MAX_INFOPARMS					64
#define MAX_SHADERS 8192

typedef enum {
    ST_CLEAN       = 0,
    ST_SOURCED     = 1, // whatever is in the file
    ST_FOG         = 2,
    ST_SKY         = 3,
    ST_GENERIC     = 4
} shaderType_t;

typedef struct {
	const char *name;
	int clearSolid, surfaceFlags, contents;
} infoParm_t;

typedef struct shaderSource_s shaderSource_t;
typedef struct shader_s shader_t;

typedef struct shaderSource_s
{
    char            filename[MAX_QPATH];
    qboolean        sourced;
    shader_t        *shaders, *newShaders;
    shaderSource_t *next;
} shaderSource_t;

typedef struct skyBoxShaderLayer_s {
    char    map[MAX_QPATH];
	float	tcScale[2];
	float	tcScroll[2];
	float	alphaGenConst;
} skyBoxShaderLayer_t;

typedef struct skyBoxShader_s {
	char				sky[MAX_QPATH];
	float				height;

	int					numLayers;
	skyBoxShaderLayer_t layers[MAX_SKYBOXLAYERS_PER_SHADER];
} skyBoxShader_t;

typedef struct shader_s
{
    char             name[MAX_QPATH];
    shaderType_t     shaderType;

    skyBoxShader_t   skyBox;
    float            fogParms[4];

    char             editorImage[MAX_QPATH];

    char             diffuseMap[MAX_QPATH];
    int              alphaTested;
    char             fullbrightMap[MAX_QPATH];

	infoParm_t		 **surfaceParms[MAX_SURFACEPARMS_PER_SHADER];
	int				 numSurfaceParms;

    shaderSource_t   *file;
    shader_t         *next, *nextInFile;
} shader_t;

void InitShaders( const char *basepath, const char *mapName );
void DefineFogShader( const char *name, float color[3], float distance );
void DefineSkyBoxShader( skyBoxShader_t *sky, float fogColor[3], float fogDistance, const char *editorImage );
void DefineShader( const char *name, const char *diffuseImage, const char *fullbrightImage, qboolean alphaTested, infoParm_t **infoParms, int numInfoParms );
void WriteShaders( void );

#endif
