#include "q_shared.h"
#include "qcommon.h"
#include "bsp.h"
#include <math.h>
#include "image.h"
#include "entity.h"
#include <ctype.h> // tolower
#include "strbuf.h"

#define IDBSPHEADER	(('P'<<24)+('S'<<16)+('B'<<8)+'I')
		// little-endian "IBSP"

#define	BSPVERSION				41	//	planepolys lump (only adjusted so -planypolys BSP 
									//	switch can detect versions and update the header
									//	appropriately
#define BSPVERSION_COLORLUMP	40	//	color lump
#define BSPVERSION_TEXINFOOLD	39	//	color info in texinfo struct
#define	BSPVERSION_Q2			38	//	original Quake 2 format BSP


// upper design bounds
// leaffaces, leafbrushes, planes, and verts are still bounded by
// 16 bit short limits
#define	MAX_MAP_MODELS		1024
#define	MAX_MAP_BRUSHES		8192
#define	MAX_MAP_ENTITIES	2048
#define	MAX_MAP_ENTSTRING	0x40000
#define	MAX_MAP_TEXINFO		8192

#define	MAX_MAP_AREAS		256
#define	MAX_MAP_AREAPORTALS	1024
#define	MAX_MAP_PLANES		65536
#define	MAX_MAP_NODES		65536
#define	MAX_MAP_BRUSHSIDES	65536
#define	MAX_MAP_LEAFS		65536
#define	MAX_MAP_VERTS		65536
#define	MAX_MAP_FACES		65536
#define	MAX_MAP_LEAFFACES	65536
#define	MAX_MAP_LEAFBRUSHES 65536
#define	MAX_MAP_PORTALS		65536
#define	MAX_MAP_EDGES		128000
#define	MAX_MAP_SURFEDGES	256000
//	For development only!!  Change before release!
#define	MAX_MAP_LIGHTING	0x600000
//#define	MAX_MAP_LIGHTING	0x400000

//	For development only!!  Change before release!
#define	MAX_MAP_VISIBILITY	0x600000
//#define	MAX_MAP_VISIBILITY	0x400000
//#define	MAX_MAP_VISIBILITY	0x200000

#define MAX_MAP_SURFCOLORINFO 8192

// key / value pair sizes

#define	MAX_KEY		32
#define	MAX_VALUE	1024

//=============================================================================

// lower bits are stronger, and will eat weaker brushes completely
#define	CONTENTS_SOLID			0x00000001		// an eye is never valid in a solid
#define	CONTENTS_WINDOW			0x00000002		// translucent, but not watery
#define	CONTENTS_AUX			0x00000004
#define	CONTENTS_LAVA			0x00000008
#define	CONTENTS_SLIME			0x00000010
#define	CONTENTS_WATER			0x00000020
#define	CONTENTS_MIST			0x00000040
//#define	LAST_VISIBLE_CONTENTS	0x00000040
#define	CONTENTS_CLEAR			0x00000080
#define	CONTENTS_NOTSOLID		0x00000100
#define	CONTENTS_NOSHOOT		0x00000200
#define	CONTENTS_FOG			0x00000400
#define	CONTENTS_NITRO			0x00000800
#define	LAST_VISIBLE_CONTENTS	0x00000800

// remaining contents are non-visible, and don't eat brushes

#define	CONTENTS_AREAPORTAL		0x00008000

#define	CONTENTS_PLAYERCLIP		0x00010000
#define	CONTENTS_MONSTERCLIP	0x00020000

// currents can be added to any other contents, and may be mixed
#define	CONTENTS_CURRENT_0		0x00040000
#define	CONTENTS_CURRENT_90		0x00080000
#define	CONTENTS_CURRENT_180	0x00100000
#define	CONTENTS_CURRENT_270	0x00200000
#define	CONTENTS_CURRENT_UP		0x00400000
#define	CONTENTS_CURRENT_DOWN	0x00800000

#define	CONTENTS_ORIGIN			0x01000000	// removed before bsping an entity

#define	CONTENTS_MONSTER		0x02000000	// should never be on a brush, only in game
#define	CONTENTS_DEADMONSTER	0x04000000
#define	CONTENTS_DETAIL			0x08000000	// brushes to be added after vis leafs
#define	CONTENTS_TRANSLUCENT	0x10000000	// auto set if any surface has trans
#define	CONTENTS_LADDER			0x20000000

//	Nelno:	FL_BOT considers this solid
#define	CONTENTS_NPCCLIP		0x40000000

#define	SURF_LIGHT		0x00000001	// value will hold the light strength

#define SURF_FULLBRIGHT	0x00000002	// lightmaps are not generated for this surface

#define	SURF_SKY		0x00000004	// don't draw, but add to skybox
#define	SURF_WARP		0x00000008	// turbulent water warp
#define	SURF_TRANS33	0x00000010
#define	SURF_TRANS66	0x00000020
#define	SURF_FLOWING	0x00000040	// scroll towards angle
#define	SURF_NODRAW		0x00000080	// don't bother referencing the texture

#define	SURF_HINT		0x00000100	// make a primary bsp splitter
#define	SURF_SKIP		0x00000200	// completely ignore, allowing non-closed brushes

#define	SURF_WOOD 		0x00000400	//	The following surface flags affect game physics and sounds
#define	SURF_METAL		0x00000800
#define	SURF_STONE		0x00001000
#define	SURF_GLASS		0x00002000
#define	SURF_ICE		0x00004000
#define	SURF_SNOW		0x00008000

#define	SURF_MIRROR		0x00010000	//	a mirror surface.  Not implemented yet.
#define	SURF_TRANSTHING	0x00020000	//	???
#define	SURF_ALPHACHAN	0x00040000	//	Nelno: 32 bit alpha channel (GL) or mid-texture with transparency (software)
#define SURF_MIDTEXTURE	0x00080000

#define	SURF_PUDDLE		0x00100000	//	for puddle sounds when walking
#define	SURF_SURGE		0x00200000  //  for water that surges (slow surge)
#define	SURF_BIGSURGE	0x00400000  //  faster, bigger surge
#define	SURF_BULLETLIGHT	0x00800000	//	light comes out of bullet holes
#define	SURF_FOGPLANE	0x01000000	// SCG[3/30/99]: for volumetric fog
#define	SURF_SAND		0x02000000	//	for puddle sounds when walking

//=============================================================================

typedef struct
{
	int		fileofs, filelen;
} lump_t;

#define	LUMP_ENTITIES		0
#define	LUMP_PLANES			1
#define	LUMP_VERTEXES		2
#define	LUMP_VISIBILITY		3
#define	LUMP_NODES			4
#define	LUMP_TEXINFO		5
#define	LUMP_FACES			6
#define	LUMP_LIGHTING		7
#define	LUMP_LEAFS			8
#define	LUMP_LEAFFACES		9
#define	LUMP_LEAFBRUSHES	10
#define	LUMP_EDGES			11
#define	LUMP_SURFEDGES		12
#define	LUMP_MODELS			13
#define	LUMP_BRUSHES		14
#define	LUMP_BRUSHSIDES		15
#define	LUMP_POP			16
#define	LUMP_AREAS			17
#define	LUMP_AREAPORTALS	18
#define	LUMP_EXTSURFINFO	19
#define	LUMP_PLANEPOLYS		20
#define	HEADER_LUMPS		21

typedef struct
{
	int			ident;
	int			version;	
	lump_t		lumps[HEADER_LUMPS];
} dheader_t;

typedef struct
{
	vec3_t		mins;
	vec3_t		maxs;
	vec3_t		origin;		// for sounds or lights
	int			headnode;
	int			firstface, numfaces;	// submodels just draw faces
										// without walking the bsp tree
} realDmodel_t;


typedef struct
{
	float	point[3];
} realDvertex_t;


// 0-2 are axial planes
#define	PLANE_X			0
#define	PLANE_Y			1
#define	PLANE_Z			2

// 3-5 are non-axial planes snapped to the nearest
#define	PLANE_ANYX		3
#define	PLANE_ANYY		4
#define	PLANE_ANYZ		5

// planes (x&~1) and (x&~1)+1 are allways opposites

typedef struct
{
	vec3_t	normal;
	float	dist;
	int		type;		// PLANE_X - PLANE_ANYZ ?remove? trivial to regenerate
} realDplane_t;


// contents flags are seperate bits
// a given brush can contribute multiple content bits
// multiple brushes can be in a single leaf

#define	Q3_CONTENTS_SOLID			1		// an eye is never valid in a solid
#define	Q3_CONTENTS_LAVA			8
#define	Q3_CONTENTS_SLIME			16
#define	Q3_CONTENTS_WATER			32
#define	Q3_CONTENTS_FOG			64

#define Q3_CONTENTS_NOTTEAM1		0x0080
#define Q3_CONTENTS_NOTTEAM2		0x0100
#define Q3_CONTENTS_NOBOTCLIP		0x0200

#define	Q3_CONTENTS_AREAPORTAL		0x8000

#define	Q3_CONTENTS_PLAYERCLIP		0x10000
#define	Q3_CONTENTS_MONSTERCLIP	0x20000
//bot specific contents types
#define	Q3_CONTENTS_TELEPORTER		0x40000
#define	Q3_CONTENTS_JUMPPAD		0x80000
#define Q3_CONTENTS_CLUSTERPORTAL	0x100000
#define Q3_CONTENTS_DONOTENTER		0x200000
#define Q3_CONTENTS_BOTCLIP		0x400000
#define Q3_CONTENTS_MOVER			0x800000

#define	Q3_CONTENTS_ORIGIN			0x1000000	// removed before bsping an entity

#define	Q3_CONTENTS_BODY			0x2000000	// should never be on a brush, only in game
#define	Q3_CONTENTS_CORPSE			0x4000000
#define	Q3_CONTENTS_DETAIL			0x8000000	// brushes not used for the bsp
#define	Q3_CONTENTS_STRUCTURAL		0x10000000	// brushes used for the bsp
#define	Q3_CONTENTS_TRANSLUCENT	0x20000000	// don't consume surface fragments inside
#define	Q3_CONTENTS_TRIGGER		0x40000000
#define	Q3_CONTENTS_NODROP			0x80000000	// don't leave bodies or items (death fog, lava)

#define	Q3_SURF_NODAMAGE			0x1		// never give falling damage
#define	Q3_SURF_SLICK				0x2		// effects game physics
#define	Q3_SURF_SKY				0x4		// lighting from environment map
#define	Q3_SURF_LADDER				0x8
#define	Q3_SURF_NOIMPACT			0x10	// don't make missile explosions
#define	Q3_SURF_NOMARKS			0x20	// don't leave missile marks
#define	Q3_SURF_FLESH				0x40	// make flesh sounds and effects
#define	Q3_SURF_NODRAW				0x80	// don't generate a drawsurface at all
#define	Q3_SURF_HINT				0x100	// make a primary bsp splitter
#define	Q3_SURF_SKIP				0x200	// completely ignore, allowing non-closed brushes
#define	Q3_SURF_NOLIGHTMAP			0x400	// surface doesn't need a lightmap
#define	Q3_SURF_POINTLIGHT			0x800	// generate lighting info at vertexes
#define	Q3_SURF_METALSTEPS			0x1000	// clanking footsteps
#define	Q3_SURF_NOSTEPS			0x2000	// no footstep sounds
#define	Q3_SURF_NONSOLID			0x4000	// don't collide against curves with this set
#define	Q3_SURF_LIGHTFILTER		0x8000	// act as a light filter during q3map -light
#define	Q3_SURF_ALPHASHADOW		0x10000	// do per-pixel light shadow casting in q3map
#define	Q3_SURF_NODLIGHT			0x20000	// don't dlight even if solid (solid lava, skies)
#define Q3_SURF_DUST				0x40000 // leave a dust trail when walking on this surface

#define JKA_SURF_NONE				(0x00000000u)
#define	JKA_SURF_SKY				(0x00002000u)	// lighting from environment map
#define	JKA_SURF_SLICK				(0x00004000u)	// affects game physics
#define	JKA_SURF_METALSTEPS			(0x00008000u)	// CHC needs this since we use same tools (though this flag is temp?)
#define JKA_SURF_FORCEFIELD			(0x00010000u)	// CHC ""			(but not temp)
#define	JKA_SURF_NODAMAGE			(0x00040000u)	// never give falling damage
#define	JKA_SURF_NOIMPACT			(0x00080000u)	// don't make missile explosions
#define	JKA_SURF_NOMARKS			(0x00100000u)	// don't leave missile marks
#define	JKA_SURF_NODRAW				(0x00200000u)	// don't generate a drawsurface at all
#define	JKA_SURF_NOSTEPS			(0x00400000u)	// no footstep sounds
#define	JKA_SURF_NODLIGHT			(0x00800000u)	// don't dlight even if solid (solid lava, skies)
#define	JKA_SURF_NOMISCENTS			(0x01000000u)	// no client models allowed on this surface
#define	JKA_SURF_FORCESIGHT			(0x02000000u)	// not visible without Force Sight

#define JKA_CONTENTS_NONE			(0x00000000u)
#define	JKA_CONTENTS_SOLID			(0x00000001u)	// Default setting. An eye is never valid in a solid
#define	JKA_CONTENTS_LAVA			(0x00000002u)	//
#define	JKA_CONTENTS_WATER			(0x00000004u)	//
#define	JKA_CONTENTS_FOG			(0x00000008u)	//
#define	JKA_CONTENTS_PLAYERCLIP		(0x00000010u)	// Player physically blocked
#define	JKA_CONTENTS_MONSTERCLIP	(0x00000020u)	// NPCs cannot physically pass through
#define JKA_CONTENTS_BOTCLIP		(0x00000040u)	// do not enter - NPCs try not to enter these
#define JKA_CONTENTS_SHOTCLIP		(0x00000080u)	// shots physically blocked
#define	JKA_CONTENTS_BODY			(0x00000100u)	// should never be on a brush, only in game
#define	JKA_CONTENTS_CORPSE			(0x00000200u)	// should never be on a brush, only in game
#define	JKA_CONTENTS_TRIGGER		(0x00000400u)	//
#define	JKA_CONTENTS_NODROP			(0x00000800u)	// don't leave bodies or items (death fog, lava)
#define JKA_CONTENTS_TERRAIN		(0x00001000u)	// volume contains terrain data
#define JKA_CONTENTS_LADDER			(0x00002000u)	//
#define JKA_CONTENTS_ABSEIL			(0x00004000u)	// used like ladder to define where an NPC can abseil
#define JKA_CONTENTS_OPAQUE			(0x00008000u)	// defaults to on, when off, solid can be seen through
#define JKA_CONTENTS_OUTSIDE		(0x00010000u)	// volume is considered to be in the outside (i.e. not indoors)
#define JKA_CONTENTS_SLIME			(0x00020000u)	// CHC needs this since we use same tools
#define JKA_CONTENTS_LIGHTSABER		(0x00040000u)	// ""
#define JKA_CONTENTS_TELEPORTER		(0x00080000u)	// ""
#define JKA_CONTENTS_ITEM			(0x00100000u)	// ""
#define JKA_CONTENTS_UNUSED00200000	(0x00200000u)	//
#define JKA_CONTENTS_UNUSED00400000	(0x00400000u)	//
#define JKA_CONTENTS_UNUSED00800000	(0x00800000u)	//
#define JKA_CONTENTS_UNUSED01000000	(0x01000000u)	//
#define JKA_CONTENTS_UNUSED02000000	(0x02000000u)	//
#define JKA_CONTENTS_UNUSED04000000	(0x04000000u)	//
#define	JKA_CONTENTS_DETAIL			(0x08000000u)	// brushes not used for the bsp
#define	JKA_CONTENTS_INSIDE			(0x10000000u)	// volume is considered to be inside (i.e. indoors)
#define JKA_CONTENTS_UNUSED20000000	(0x20000000u)	//
#define JKA_CONTENTS_UNUSED40000000	(0x40000000u)	//
#define	JKA_CONTENTS_TRANSLUCENT	(0x80000000u)	// don't consume surface fragments inside
#define JKA_CONTENTS_ALL			(0xFFFFFFFFu)

typedef struct
{
	int			planenum;
	int			children[2];	// negative numbers are -(leafs+1), not nodes
	short		mins[3];		// for frustom culling
	short		maxs[3];
	unsigned short	firstface;
	unsigned short	numfaces;	// counting both sides
} realDnode_t;

typedef struct
{
	float		vecs[2][4];		// [s/t][xyz offset]
	int			flags;			// miptex flags + overrides
	int			value;			// light emission, etc
	char		texture[32];	// texture name (textures/*.wal)
	int			nexttexinfo;	// for animations, -1 = end of chain
} realDtexinfo_t;

// note that edge 0 is never used, because negative edge nums are used for
// counterclockwise use of the edge in a face
typedef struct
{
	unsigned short	v[2];		// vertex numbers
} realDedge_t;

#define	DK_MAXLIGHTMAPS	4
#if DK_MAXLIGHTMAPS == MAXLIGHTMAPS
#else
#error "DK_MAXLIGHTMAPS == MAXLIGHTMAPS failed"
#endif
typedef struct
{
	unsigned short	planenum;
	short		side;

	int			firstedge;		// we must support > 64k edges
	short		numedges;	
	short		texinfo;

// lighting info
	byte		styles[DK_MAXLIGHTMAPS];
	int			lightofs;		// start of [numstyles*surfsize] samples
} realDface_t;

typedef struct
{
	int				contents;			// OR of all brushes (not needed?)

	short			cluster;
	short			area;

	short			mins[3];			// for frustum culling
	short			maxs[3];

	unsigned short	firstleafface;
	unsigned short	numleaffaces;

	unsigned short	firstleafbrush;
	unsigned short	numleafbrushes;

	int				brushnum;
} realDleaf_t;

typedef struct
{
	unsigned short	planenum;		// facing out of the leaf
	short	texinfo;
} realDbrushside_t;

typedef struct
{
	int			firstside;
	int			numsides;
	int			contents;
} realDbrush_t;

#define	ANGLE_UP	-1
#define	ANGLE_DOWN	-2


// the visibility lump consists of a header with a count, then
// byte offsets for the PVS and PHS of each cluster, then the raw
// compressed bit vectors
#define	DVIS_PVS	0
#define	DVIS_PHS	1
typedef struct
{
	int			numclusters;
	int			bitofs[8][2];	// bitofs[numclusters][2]
} realDvis_t;

// each area has a list of portals that lead into other areas
// when portals are closed, other areas may not be visible or
// hearable even if the vis info says that it should be
typedef struct
{
	int		portalnum;
	int		otherarea;
} realDareaportal_t;

typedef struct
{
	int		numareaportals;
	int		firstareaportal;
} realDarea_t;

typedef struct
{
	vec3_t	color;
} extsurfinfo_t;

/****************************************************
*/

typedef struct
{
	unsigned short	v[2];
	unsigned int	cachededgeoffset;
} medge_t;

typedef struct mtexinfo_s
{
	vec3_t s, t;
	float s_offset, t_offset;
	int			flags;
	int			numframes;
	struct mtexinfo_s	*next;		// animation chain
	image_rgba_t		*image;
	byte	color[3];
	int value;
	int					duplicate;
	struct mtexinfo_s	*hash_next;
	qboolean			skip; // this should not be rendered at all (e.g. clip)
	int					shaderNum;
	qboolean			generatedShader;	// shader name comes from DK_GeneratedShaderName
} mtexinfo_t;

typedef struct glpoly_s
{
	struct	glpoly_s	*next;
	struct	glpoly_s	*chain;
	int		numverts;
	int		flags;			// for SURF_UNDERWATER (not needed anymore?)
	drawVert_t	*verts;	// variable sized (xyz s1t1 s2t2)
} glpoly_t;

typedef struct msurface_s
{
	int			visframe;		// should be drawn when node is crossed

	dplane_t	*plane;
	int			side;
	int			flags;

	int			firstedge;	// look up in model->surfedges[], negative numbers
	int			numedges;	// are backwards edges
	
	short		texturemins[2];
	short		extents[2];

	int			light_s, light_t;	// gl lightmap coordinates
	int			dlight_s, dlight_t; // gl lightmap coordinates for dynamic lightmaps

	glpoly_t	*polys;				// multiple if warped
	struct	msurface_s	*texturechain;
	struct  msurface_s	*lightmapchain;

	mtexinfo_t	*texinfo;
	int shaderNum;
	int fogNum;
	
// lighting info
	int			dlightframe;
	int			dlightbits;

	int			lightmaptexturenum;
	byte		styles[DK_MAXLIGHTMAPS];
	float		cached_light[DK_MAXLIGHTMAPS];	// values currently used in lightmap
	byte		*samples;		// [numstyles*surfsize]
} msurface_t;

#define LIGHTMAP_SIZE 128

typedef struct mnode_s
{
// common with leaf
	int			contents;		// -1, to differentiate from leafs
	int			visframe;		// node needs to be traversed if current
	
	float		mins[3];		// for bounding box culling
	float		maxs[3];

	struct mnode_s	*parent;

// node specific
	dplane_t	*plane;
	struct mnode_s	*children[2];	

	unsigned short		firstsurface;
	unsigned short		numsurfaces;
} mnode_t;

typedef struct mleaf_s
{
// common with node
	int			contents;		// wil be a negative contents number
	int			visframe;		// node needs to be traversed if current

	float		mins[3];		// for bounding box culling
	float		maxs[3];

	struct mnode_s	*parent;

// leaf specific
	int			cluster;
	int			area;

	int			firstmarksurface;
	int			nummarksurfaces;

	int			firstleafbrush;
	int			numleafbrushes;
} mleaf_t;

typedef struct
{
	dplane_t	*plane;
	msurface_t	*surface;
} mbrushside_t;


typedef struct mbrush_s
{
	int			contents;
	int			numsides;
	int			firstbrushside;
	int			checkcount;		// to avoid repeated testings
} mbrush_t;

typedef struct
{
	int		numareaportals;
	int		firstareaportal;
	int		floodnum;			// if two areas have equal floodnums, they are connected
	int		floodvalid;
} marea_t;

typedef struct
{
	vec3_t		mins, maxs;
	vec3_t		origin;		// for sounds or lights
	float		radius;
	int			headnode;
	int			visleafs;		// not including the solid leaf 0
	int			firstface, numfaces;
} mmodel_t;

#define VIEWFOGS 0

typedef struct
{
	char		shader[MAX_QPATH];
	int			brushNum;
	int			visibleSide;	// the brush side that ray tests need to clip against (-1 == none)

	mtexinfo_t	*texinfo;
} mfog_t;

/****************************************************
*/

static int GetLumpElements( dheader_t *header, int lump, int size ) {
	/* check for odd size */
	if ( header->lumps[ lump ].filelen % size ) {
		Com_Printf( "GetLumpElements: odd lump size (%d) in lump %d\n", header->lumps[ lump ].filelen, lump );
		return 0;
	}

	/* return element count */
	return header->lumps[ lump ].filelen / size;
}

static void CopyLump( dheader_t *header, int lump, const void *src, void *dest, int size, qboolean swap ) {
	int length;

	length = GetLumpElements( header, lump, size ) * size;

	/* handle erroneous cases */
	if ( length <= 0 ) {
		return;
	}

	if ( swap ) {
		BSP_SwapBlock( dest, (int *)((byte*) src + header->lumps[lump].fileofs), length );
	} else {
		Com_Memcpy( dest, (byte*) src + header->lumps[lump].fileofs, length );
	}
}

static void *GetLump( dheader_t *header, const void *src, int lump ) {
	return (void*)( (byte*) src + header->lumps[ lump ].fileofs );
}

/****************************************************
*/

float DotProduct(vec3_t a, vec3_t b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

glpoly_t *AllocGLPoly( int numverts ){
	glpoly_t	*poly;
	int			allocSize;
	byte*		ptr;

	if ( numverts < 3 )
	{
		Com_Error(ERR_DROP, "something is wrong here");
	}

	// allocate once.. assign pointers..
	allocSize = sizeof(glpoly_t) + numverts * sizeof(drawVert_t);

	ptr = (byte*)malloc( allocSize );

	memset( ptr, 0, sizeof( glpoly_t ) );

	poly = (glpoly_t *)	ptr;
	poly->verts = (drawVert_t*)(ptr + sizeof(glpoly_t));

	return poly;
}

void CalcSurfaceExtents( float *vertexes, medge_t *edges, int *surfEdges, msurface_t *s )
{
	float	mins[2], maxs[2], val;
	int		i, e;
	float	*v;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;
	
	for( i = 0; i < s->numedges; i++ )
	{
		e = surfEdges[s->firstedge+i];
		if( e >= 0 )
		{
			v = &vertexes[edges[e].v[0] * 3];
		}
		else
		{
			v = &vertexes[edges[-e].v[1] * 3];
		}
		
		val = v[0] * tex->s[0] + 
			v[1] * tex->s[1] + 
			v[2] * tex->s[2] + 
			tex->s_offset;

		if(val < mins[0] )
		{
			mins[0] = val;
		}
		if( val > maxs[0] )
		{
			maxs[0] = val;
		}

		val = v[0] * tex->t[0] + 
			v[1] * tex->t[1] + 
			v[2] * tex->t[2] + 
			tex->t_offset;

		if(val < mins[1])
		{
			mins[1] = val;
		}
		if( val > maxs[1] )
		{
			maxs[1] = val;
		}
	}

	for( i = 0; i < 2; i++ )
	{	
		bmins[i] = floor( mins[i]/16 );
		bmaxs[i] = ceil( maxs[i]/16 );

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = ( bmaxs[i] - bmins[i] ) * 16;
		if( s->extents[i] < 16 )
		{
			s->extents[i] = 16;	// take at least one cache block
		}
		if ( !(tex->flags & (SURF_WARP|SURF_SKY)) && s->extents[i] > 256)
			Com_Error (ERR_DROP,"Bad surface extents");
	}
}

/*
================
DK_SetPolyVertex

Texture and lightmap coordinates, normal and colour of a point on a surface.
The surface's lightmap block must be allocated already.
================
*/
static void DK_SetPolyVertex( const msurface_t *pSurf, const vec3_t vec, drawVert_t *out )
{
	float s, t;

	VectorCopy( vec, out->xyz );

	s = DotProduct( vec, pSurf->texinfo->s ) + pSurf->texinfo->s_offset;
	t = DotProduct( vec, pSurf->texinfo->t ) + pSurf->texinfo->t_offset;
	out->st[0] = s / pSurf->texinfo->image->width;
	out->st[1] = t / pSurf->texinfo->image->height;

	// luxel centres sit 8 units into each 16 unit cell of the 128 luxel block
	out->lightmap[0][0] = ( s - pSurf->texturemins[0] + pSurf->light_s * 16 + 8 ) / ( LIGHTMAP_SIZE * 16 );
	out->lightmap[0][1] = ( t - pSurf->texturemins[1] + pSurf->light_t * 16 + 8 ) / ( LIGHTMAP_SIZE * 16 );

	VectorCopy( pSurf->plane->normal, out->normal );

	out->color[0][0] = pSurf->texinfo->color[0];
	out->color[0][1] = pSurf->texinfo->color[1];
	out->color[0][2] = pSurf->texinfo->color[2];
	out->color[0][3] = 255;
}

void GL_BuildPolygonFromSurface( float *vertexes, medge_t *pedges, int *surfedges, msurface_t *pSurf )
{
	int			i, lindex, lnumverts;
	medge_t		*r_pedge;
//	int			vertpage;
	vec3_t		vec;
	glpoly_t	*poly;
	vec3_t		total;

// reconstruct the polygon
	lnumverts = pSurf->numedges;
//	vertpage = 0;

	total[0] = total[1] = total[2] = 0.0f;
	//
	// draw texture
	//

	poly = AllocGLPoly( lnumverts );

	poly->next = pSurf->polys;
//	poly->flags = pSurf->flags;
	pSurf->polys = poly;
	poly->numverts = lnumverts;

	for (i=0 ; i<lnumverts ; i++)
	{
		lindex = surfedges[pSurf->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec[0] = vertexes[r_pedge->v[0] * 3 + 0];
			vec[1] = vertexes[r_pedge->v[0] * 3 + 1];
			vec[2] = vertexes[r_pedge->v[0] * 3 + 2];
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec[0] = vertexes[r_pedge->v[1] * 3 + 0];
			vec[1] = vertexes[r_pedge->v[1] * 3 + 1];
			vec[2] = vertexes[r_pedge->v[1] * 3 + 2];
		}

		total[0] = total[0] + vec[0];
		total[1] = total[1] + vec[1];
		total[2] = total[2] + vec[2];
		DK_SetPolyVertex( pSurf, vec, &poly->verts[i] );
	}
}

/*
================
DK_SubdividePolygon

Quake 2's SubdividePolygon: cut a convex polygon on a grid until no piece
crosses a grid line, adding the pieces to the surface's poly chain.
================
*/
#define DK_SUBDIVIDE_SIZE	64		// gl_subdivide_size, what Daikatana cuts warping faces into
#define DK_SUBDIVIDE_VERTS	64

static void DK_SubdividePolygon( msurface_t *surf, int numverts, vec3_t *verts )
{
	vec3_t		mins, maxs, front[DK_SUBDIVIDE_VERTS], back[DK_SUBDIVIDE_VERTS];
	float		dist[DK_SUBDIVIDE_VERTS + 1], m, frac;
	int			i, j, f, b;
	glpoly_t	*poly;

	if ( numverts > DK_SUBDIVIDE_VERTS - 4 )
		Com_Error( ERR_DROP, "DK_SubdividePolygon: too many vertexes (%d)", numverts );

	VectorCopy( verts[0], mins );
	VectorCopy( verts[0], maxs );
	for ( i = 1; i < numverts; i++ )
	{
		for ( j = 0; j < 3; j++ )
		{
			mins[j] = verts[i][j] < mins[j] ? verts[i][j] : mins[j];
			maxs[j] = verts[i][j] > maxs[j] ? verts[i][j] : maxs[j];
		}
	}

	for ( i = 0; i < 3; i++ )
	{
		m = DK_SUBDIVIDE_SIZE * floor( ( mins[i] + maxs[i] ) * 0.5f / DK_SUBDIVIDE_SIZE + 0.5f );
		if ( maxs[i] - m < 8 || m - mins[i] < 8 )
			continue;

		for ( j = 0; j < numverts; j++ )
			dist[j] = verts[j][i] - m;
		dist[j] = dist[0];

		f = b = 0;
		for ( j = 0; j < numverts; j++ )
		{
			vec3_t *next = &verts[( j + 1 ) % numverts];

			if ( dist[j] >= 0 ) {
				VectorCopy( verts[j], front[f] );
				f++;
			}
			if ( dist[j] <= 0 ) {
				VectorCopy( verts[j], back[b] );
				b++;
			}
			if ( dist[j] == 0 || dist[j + 1] == 0 )
				continue;
			if ( ( dist[j] > 0 ) != ( dist[j + 1] > 0 ) )
			{
				frac = dist[j] / ( dist[j] - dist[j + 1] );
				front[f][0] = back[b][0] = verts[j][0] + frac * ( (*next)[0] - verts[j][0] );
				front[f][1] = back[b][1] = verts[j][1] + frac * ( (*next)[1] - verts[j][1] );
				front[f][2] = back[b][2] = verts[j][2] + frac * ( (*next)[2] - verts[j][2] );
				f++;
				b++;
			}
		}

		DK_SubdividePolygon( surf, f, front );
		DK_SubdividePolygon( surf, b, back );
		return;
	}

	poly = AllocGLPoly( numverts );
	poly->numverts = numverts;
	poly->next = surf->polys;
	surf->polys = poly;
	for ( i = 0; i < numverts; i++ )
		DK_SetPolyVertex( surf, verts[i], &poly->verts[i] );
}

/*
================
DK_SubdivideWarpSurface

Daikatana cuts warping faces into 64 unit pieces at load. Their vertexes
carry the surge waves (deformVertexes) and the vertex lighting of
translucent water.
================
*/
static void DK_SubdivideWarpSurface( msurface_t *surf )
{
	glpoly_t	*outline = surf->polys;
	vec3_t		verts[DK_SUBDIVIDE_VERTS];
	int			i;

	vec3_t		area, e1, e2;
	int			j;

	if ( outline->numverts > DK_SUBDIVIDE_VERTS - 4 )
		return;

	for ( i = 0; i < outline->numverts; i++ )
		VectorCopy( outline->verts[i].xyz, verts[i] );

	// skip faces without area, the subdivision can't cut them
	area[0] = area[1] = area[2] = 0;
	for ( i = 2; i < outline->numverts; i++ )
	{
		for ( j = 0; j < 3; j++ )
		{
			e1[j] = verts[i - 1][j] - verts[0][j];
			e2[j] = verts[i][j] - verts[0][j];
		}
		area[0] += e1[1] * e2[2] - e1[2] * e2[1];
		area[1] += e1[2] * e2[0] - e1[0] * e2[2];
		area[2] += e1[0] * e2[1] - e1[1] * e2[0];
	}
	if ( DotProduct( area, area ) < 1.0f )
		return;

	surf->polys = outline->next;
	DK_SubdividePolygon( surf, outline->numverts, verts );
	free( outline );
}


static void DecompressVis (byte *visibility, int numvisibility, realDvis_t *vis, int clusterBytes, byte * out_real) {
	int c;
	byte *out_p;
	int row;
	int i;

	if (!numvisibility) {	// no vis info, so make all visible
		row = vis->numclusters * clusterBytes;
		while (row) {
			*out_p++ = 0xff;
			row--;
		}
		return;
	}

	for (i = 0; i < vis->numclusters; i++)
	{
		byte *out = out_real + i * clusterBytes;
		byte *in = visibility + vis->bitofs[i][DVIS_PVS];

		row = clusterBytes;
		out_p = out;

		do {
			if (*in) {
				*out_p++ = *in++;
				continue;
			}

			c = in[1];
			in += 2;
			if ((out_p - out) + c > row) {
				c = row - (out_p - out);
				Com_Error (ERR_DROP, "Vis decompression overrun\n");
			}
			while (c) {
				*out_p++ = 0;
				c--;
			}
		} while (out_p - out < row);
	}
}

static void ConvertVisibility( bspFile_t *bsp, const void *data, dheader_t *header, realDvis_t *visibility, int visibilityLength ) {
	int		i;
	byte	*in;

	if ( !visibilityLength ) {
		return;
	}

	in = GetLump( header, data, LUMP_VISIBILITY );

	memcpy (visibility, in, visibilityLength);
	visibility->numclusters = LittleLong( visibility->numclusters );
	for (i=0 ; i<visibility->numclusters ; i++)
	{
		visibility->bitofs[i][0] = LittleLong (visibility->bitofs[i][0]);
		visibility->bitofs[i][1] = LittleLong (visibility->bitofs[i][1]);
	}

	bsp->clusterBytes = ( visibility->numclusters + 7 ) >> 3;
	bsp->visibilityLength = visibility->numclusters * bsp->clusterBytes;
	bsp->visibility = malloc( bsp->visibilityLength );
	
	bsp->numClusters = visibility->numclusters;
	DecompressVis( in, visibilityLength, visibility, bsp->clusterBytes, bsp->visibility );
}

/****************************************************
*/

#define	LIGHTMAP_SIZE		128
#define LM_BLOCKLIGHTS_SIZE LIGHTMAP_SIZE * LIGHTMAP_SIZE * 3

typedef struct mlightmap_s {
	byte buffer[LM_BLOCKLIGHTS_SIZE];
	struct mlightmap_s *next;
} mlightmap_t;

typedef struct {
	int		texnum;

	mlightmap_t *lightmaps;

	byte lightmap_buffer[LM_BLOCKLIGHTS_SIZE];
	int allocated[LIGHTMAP_SIZE];
} gllightmapstate_t;

void GL_BeginBuildingLightmaps(gllightmapstate_t *lms)
{
	lms->texnum = 0;
	lms->lightmaps = NULL;
	memset (lms->allocated, 0, sizeof(lms->allocated));
}

/*
===============
R_BuildLightMap

Combine & scale multiple lightmaps into the floating-point format in s_blocklights,
then normalize into GL format in gl_lms.lightmap_buffer.
===============
*/

static float s_blocklights[LM_BLOCKLIGHTS_SIZE];		// intermediate RGB buffer for combining multiple lightmaps

#define DK_LIGHTMAP_MODULATE	2.0f	// gl_modulate default
#define DK_LIGHTMAP_OVERBRIGHT	4.0f	// 1 << r_mapOverBrightBits default

// scale a colour down so its brightest channel is at most 255
static void DK_ClampLight( float *c ) {
	float cmax = c[0] > c[1] ? c[0] : c[1];

	cmax = c[2] > cmax ? c[2] : cmax;
	if ( cmax > 255 ) {
		c[0] *= 255 / cmax;
		c[1] *= 255 / cmax;
		c[2] *= 255 / cmax;
	}
}

void R_BuildLightMap (gllightmapstate_t *lms, msurface_t *surf, int stride) {
	int smax, tmax;
	int r, g, b, cmax;
	int i, j, k, size;
	vec3_t scale;
	float *bl;
	byte *lm, *dest;
	int numlightmaps;

	smax = (surf->extents[0] / 16) + 1;
	tmax = (surf->extents[1] / 16) + 1;

	size = smax * tmax;
	stride -= smax * 3;

	if (size > ((sizeof(float) * LM_BLOCKLIGHTS_SIZE) / 16))
		Com_Error(ERR_DROP, "R_BuildLightMap(): bad lms->blocklights size.");

	// Daikatana's renderer adds up every lightmap block of a face (all style 0
	// in practice) and scales the sum by gl_modulate (2). Faces without light
	// data are drawn at full brightness.
	if (surf->samples)
	{
		numlightmaps = 0;
		while ( numlightmaps < DK_MAXLIGHTMAPS && surf->styles[numlightmaps] != 255 )
			numlightmaps++;

		memset( s_blocklights, 0, sizeof( s_blocklights[0] ) * size * 3 );
		lm = surf->samples;
		for ( k = 0; k < numlightmaps; k++ )
		{
			bl = s_blocklights;
			for (i = 0; i < size; i++, bl += 3, lm += 3)
			{
				bl[0] += lm[0] * DK_LIGHTMAP_MODULATE;
				bl[1] += lm[1] * DK_LIGHTMAP_MODULATE;
				bl[2] += lm[2] * DK_LIGHTMAP_MODULATE;
			}
		}
	}
	else
	{
		for (i = 0; i < size * 3; i++)
			s_blocklights[i] = 255.f;
	}

	// Daikatana clamps the light at 1.0 keeping its hue, and the maps are lit
	// for that: brighter spots saturate textures badly with overbright.
	// Quake 3 draws lightmaps at 2^r_mapOverBrightBits (4 by default) times
	// their stored value, so store a quarter of it.
	bl = s_blocklights;
	for (i = 0; i < size; i++, bl += 3)
		DK_ClampLight( bl );
	bl = s_blocklights;
	for (i = 0; i < size * 3; i++)
		bl[i] *= 1.0f / DK_LIGHTMAP_OVERBRIGHT;

	//
	// put into texture format
	//

	bl = s_blocklights;

	dest = lms->lightmap_buffer;
	dest += (surf->light_t * LIGHTMAP_SIZE + surf->light_s) * 3;

	for (i = 0; i < tmax; i++, dest += stride) {
		for (j = 0; j < smax; j++, bl += 3, dest += 3) {
			r = (long) (bl[0]);
			g = (long) (bl[1]);
			b = (long) (bl[2]);

			// catch negative lights
			r = r > 0 ? r : 0;
			g = g > 0 ? g : 0;
			b = b > 0 ? b : 0;

			// determine the brightest of the three color components
			cmax = g > b ? g : b;
			cmax = r > cmax ? r : cmax;

			// rescale all the color components if the intensity of the greatest channel exceeds 1.0
			if (cmax > 255) {
				float t = 255.f / cmax;

				r *= t;
				g *= t;
				b *= t;
			}

			dest[0] = r;
			dest[1] = g;
			dest[2] = b;
		}
	}
}

static void LM_InitBlock (gllightmapstate_t *lms)
{
	memset( lms->allocated, 0, sizeof( lms->allocated ) );
}

static void LM_UploadBlock (gllightmapstate_t *lms)
{
	mlightmap_t *map = malloc( sizeof(*map) );
	memcpy(map->buffer, lms->lightmap_buffer, LM_BLOCKLIGHTS_SIZE);
	if (lms->lightmaps == NULL)
	{
		map->next = NULL;
		lms->lightmaps = map;
	}
	else
	{
		map->next = lms->lightmaps;
		lms->lightmaps = map;
	}
	lms->texnum++;
}

// returns a texture number and the position inside it
static qboolean LM_AllocBlock (gllightmapstate_t *lms, int w, int h, int *x, int *y) {
	int i, j;
	int best, best2;

	best = LIGHTMAP_SIZE;

	for (i = 0; i < LIGHTMAP_SIZE - w; i++) {
		best2 = 0;

		for (j = 0; j < w; j++) {
			if (lms->allocated[i + j] >= best)
				break;
			if (lms->allocated[i + j] > best2)
				best2 = lms->allocated[i + j];
		}

		if (j == w) {			// this is a valid spot
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > LIGHTMAP_SIZE)
		return qfalse;

	for (i = 0; i < w; i++)
		lms->allocated[*x + i] = best + h;

	return qtrue;
}

/*
================
DK_LightPolyVertexes

Vertex colours from the surface's lightmap, already scaled like it, for
shaders that light with "rgbGen vertex" (translucent water) and r_vertexLight.
The surface's lightmap block must still be the one being built.
================
*/
static void DK_LightPolyVertexes( const gllightmapstate_t *lms, msurface_t *surf )
{
	glpoly_t	*poly;
	int			i, c, x0, y0, x1, y1;
	int			smin, tmin, smax, tmax;
	float		x, y, fx, fy;
	const byte	*block = lms->lightmap_buffer;

	if ( surf->lightmaptexturenum < 0 )
		return;

	// stay inside this surface's luxels so neighbours in the atlas don't bleed in
	smin = surf->light_s;
	tmin = surf->light_t;
	smax = smin + surf->extents[0] / 16;
	tmax = tmin + surf->extents[1] / 16;

	for ( poly = surf->polys; poly; poly = poly->next )
	{
		for ( i = 0; i < poly->numverts; i++ )
		{
			drawVert_t *v = &poly->verts[i];

			x = v->lightmap[0][0] * LIGHTMAP_SIZE - 0.5f;
			y = v->lightmap[0][1] * LIGHTMAP_SIZE - 0.5f;
			x = x < smin ? smin : ( x > smax ? smax : x );
			y = y < tmin ? tmin : ( y > tmax ? tmax : y );
			x0 = (int)x;
			y0 = (int)y;
			x1 = x0 < smax ? x0 + 1 : x0;
			y1 = y0 < tmax ? y0 + 1 : y0;
			fx = x - x0;
			fy = y - y0;

			for ( c = 0; c < 3; c++ )
			{
				float top = block[( y0 * LIGHTMAP_SIZE + x0 ) * 3 + c] * ( 1 - fx ) + block[( y0 * LIGHTMAP_SIZE + x1 ) * 3 + c] * fx;
				float bottom = block[( y1 * LIGHTMAP_SIZE + x0 ) * 3 + c] * ( 1 - fx ) + block[( y1 * LIGHTMAP_SIZE + x1 ) * 3 + c] * fx;

				v->color[0][c] = (byte)( top * ( 1 - fy ) + bottom * fy + 0.5f );
			}
		}
	}
}

void GL_CreateSurfaceLightmap (gllightmapstate_t *lms, msurface_t * surf)
{
	int smax, tmax;

	smax = (surf->extents[0] / 16) + 1;
	tmax = (surf->extents[1] / 16) + 1;

	if (!LM_AllocBlock (lms, smax, tmax, &surf->light_s, &surf->light_t))
	{
		LM_UploadBlock(lms);
		LM_InitBlock(lms);
		if (!LM_AllocBlock(lms, smax, tmax, &surf->light_s, &surf->light_t))
			Com_Error (ERR_DROP, "GL_CreateSurfaceLightmap(): consecutive calls to LM_AllocBlock(%d, %d) failed.\n", smax, tmax);
	}
	
	surf->lightmaptexturenum = lms->texnum;

	R_BuildLightMap (lms, surf, LIGHTMAP_SIZE * 3);
}

/* ******************************************** */

// Daikatana sky: a flattened cloud dome with up to two scrolling layers of
// the "cloudname" texture, and a skybox with alpha drawn over the clouds
// (gl_warp.cpp R_DrawSkyBox). The defaults are the renderer's gl_cloud* cvars.
#define DK_CLOUD_XDIR		1.0f
#define DK_CLOUD_YDIR		0.8f
#define DK_CLOUD1_TILE		8.0f
#define DK_CLOUD1_SPEED		1.0f
#define DK_CLOUD2_TILE		2.0f
#define DK_CLOUD2_SPEED		4.0f
#define DK_CLOUD2_ALPHA		0.7f
#define DK_CLOUD_TIMESCALE	0.002f	// scroll offset grows by speed * tile1 * this per second

// The DK dome rises a quarter of its half width; this Quake 3 cloud height
// gives the cloud sphere about the same curvature.
#define SKY_CLOUD_HEIGHT	512

// Quake 3 draws "rgbGen identity" and the skybox at tr.identityLight (0.5 with
// the default r_overBrightBits 1); "rgbGen const" is not scaled, so cloud
// layer weights are premultiplied by it.
#define SKY_IDENTITY_LIGHT	0.5f

typedef struct {
	char			*sky;
	char			*cloudname;

	float			cloudxdir, cloudydir;
	float			cloud1tile, cloud1speed;
	float			cloud2tile, cloud2speed;
	float			cloud2alpha;

	int				fogValue;
	float			fogStart, fogEnd, fogSkyEnd;
	float			fogColor[3];
} skybox_t;

static void SkyBoxPrintCloudLayer( const skybox_t *box, float tile, float speed, float weight, strbuf_t *strbuf ) {
	char	line[2048];
	float	rate = speed * box->cloud1tile * DK_CLOUD_TIMESCALE;

	AppendToBuffer( strbuf, "\t{\n" );
	sprintf( line, "\t\tmap env/32bit/%s.tga\n", box->cloudname );
	AppendToBuffer( strbuf, line );
	// add the clouds where the skybox is see-through, leaving the
	// framebuffer alpha alone for the next layer
	AppendToBuffer( strbuf, "\t\tblendFunc GL_ONE_MINUS_DST_ALPHA GL_ONE\n" );
	sprintf( line, "\t\trgbGen const ( %f %f %f )\n", weight, weight, weight );
	AppendToBuffer( strbuf, line );
	AppendToBuffer( strbuf, "\t\talphaGen const 0\n" );
	sprintf( line, "\t\ttcMod scale %f %f\n", tile, tile );
	AppendToBuffer( strbuf, line );
	sprintf( line, "\t\ttcMod scroll %f %f\n", box->cloudxdir * rate, box->cloudydir * rate );
	AppendToBuffer( strbuf, line );
	AppendToBuffer( strbuf, "\t}\n" );
}

static void SkyBoxPrint( const char *mapfilename, skybox_t *box, strbuf_t *strbuf ) {
	char	line[2048];
	float	cloud2weight;

	if ( !box->sky )
		return;

	sprintf( line, "textures/%s/sky\n", mapfilename );
	AppendToBuffer( strbuf, line );

	sprintf( line, "{\n" "\tqer_editorimage env/32bit/%s_up.tga\n", box->sky );
	AppendToBuffer( strbuf, line );
	AppendToBuffer( strbuf, "\tsurfaceparm noimpact\n" "\tsurfaceparm nolightmap\n" "\tsurfaceparm sky\n");
	sprintf( line, "\tskyparms env/32bit/%s %d -\n", box->sky, SKY_CLOUD_HEIGHT );
	AppendToBuffer( strbuf, line );

	if ( box->fogValue && box->fogEnd > 1 ) {
		// worldspawn fog is linear GL fog; a fogvars distance above 1 makes
		// Spearmint's global fog linear too, opaque at that distance
		sprintf( line, "\tfogvars ( %f %f %f ) %f\n", box->fogColor[0], box->fogColor[1], box->fogColor[2], box->fogEnd );
		AppendToBuffer( strbuf, line );
	}

	if ( box->cloudname ) {
		// The skybox was drawn opaque with its alpha left in the framebuffer.
		// Premultiply it by that alpha so the clouds can be added behind it.
		AppendToBuffer( strbuf, "\t{\n" "\t\tmap $whiteimage\n" "\t\tblendFunc GL_ZERO GL_DST_ALPHA\n" "\t}\n" );

		cloud2weight = box->cloud2alpha < 1 ? box->cloud2alpha : 1;
		SkyBoxPrintCloudLayer( box, box->cloud1tile, box->cloud1speed, ( 1 - cloud2weight ) * SKY_IDENTITY_LIGHT, strbuf );
		if ( cloud2weight > 0 ) {
			SkyBoxPrintCloudLayer( box, box->cloud2tile, box->cloud2speed, cloud2weight * SKY_IDENTITY_LIGHT, strbuf );
		}
	}

	AppendToBuffer( strbuf, "}\n" );
}

static void SkyBoxFree( skybox_t *box ) {
	if ( box->sky )
		free( box->sky );
	if ( box->cloudname )
		free( box->cloudname );
}

static float SkyFloatForKey( entity_t *ent, const char *key, float defaultValue ) {
	return *ValueForKey( ent, key ) ? FloatForKey( ent, key ) : defaultValue;
}

static void SkyboxFromEntity( skybox_t *box, entity_t *ent ) {
	const char *sky;
	const char *cloudname;

	memset( box, 0, sizeof( *box ) );

	sky = ValueForKey( ent, "sky" );
	cloudname = ValueForKey( ent, "cloudname" );

	if ( *sky ) {
		box->sky = copystring( sky );

		if ( *cloudname ) {
			box->cloudname = copystring( cloudname );
			box->cloudxdir = SkyFloatForKey( ent, "cloudxdir", DK_CLOUD_XDIR );
			box->cloudydir = SkyFloatForKey( ent, "cloudydir", DK_CLOUD_YDIR );
			box->cloud1tile = SkyFloatForKey( ent, "cloud1tile", DK_CLOUD1_TILE );
			box->cloud1speed = SkyFloatForKey( ent, "cloud1speed", DK_CLOUD1_SPEED );
			box->cloud2tile = SkyFloatForKey( ent, "cloud2tile", DK_CLOUD2_TILE );
			box->cloud2speed = SkyFloatForKey( ent, "cloud2speed", DK_CLOUD2_SPEED );
			box->cloud2alpha = SkyFloatForKey( ent, "cloud2alpha", DK_CLOUD2_ALPHA );
		}
	}

	box->fogValue = IntegerForKey( ent, "fog_value" );
	if ( box->fogValue ) {
		box->fogStart = FloatForKey( ent, "fog_start" );
		box->fogEnd = FloatForKey( ent, "fog_end" );
		box->fogSkyEnd = FloatForKey( ent, "fog_skyend" );
		// Daikatana divides fog_color by 255, but the maps give it as 0-1
		// fractions, so its fog comes out black. Use the colour the mapper
		// picked; without one the fog stays black.
		if ( GetVectorForKey( ent, "fog_color", box->fogColor ) &&
			( box->fogColor[0] > 1 || box->fogColor[1] > 1 || box->fogColor[2] > 1 ) ) {
			box->fogColor[0] *= 1.0f / 255.0f;
			box->fogColor[1] *= 1.0f / 255.0f;
			box->fogColor[2] *= 1.0f / 255.0f;
		}
		GetVectorForKey( ent, "_color", box->fogColor );
	}
}

static void dkent_trigger_hurt( entity_t *ent )
{
	char	buf[32];
	int		spawnFlags, q3SpawnFlags;

// "spawnflags" "4"
// DK flags:
//#define		ALLOW_TOGGLE				0x01
//#define		START_DISABLED				0x02
//#define		FLOWTHRU_DMG				0x04
// Q3 flags?
// START_OFF 2
// SILENT 4
// SLOW 16
// NO_PROTECTION 8
// no customizable sound!
	spawnFlags = IntegerForKey( ent, "spawnflags" );
	q3SpawnFlags = 0;

	if (spawnFlags & 0x01/*ALLOW_TOGGLE*/)
		q3SpawnFlags |= 1;
	if (spawnFlags & 0x02/*START_DISABLED*/)
		q3SpawnFlags |= 2/*START_OFF*/;

	sprintf( buf, "%d", q3SpawnFlags );

	SetKeyValue( ent, "spawnflags", buf );
}

static void dkent_target_speaker( entity_t *ent )
{
	const char *sound1;

	// add "noise" key
	// for JK: map sound1..sound6 to soundGroup:
	//
	// "sounds" va() min max, so if your sound string is borgtalk%d.wav, and you set a "sounds" value of 4, it will randomly play borgtalk1.wav - borgtalk4.wav when triggered
	// to use this, you must store the wav name in "soundGroup", NOT "noise"
	// MISSING: volume support in quake3

	// fixup paths for all sounds
	AppendKeyValueMatching( ent, "sound", "sounds\\" );

	// set the noise key for compat with quake3 and jka
	sound1 = ValueForKey( ent, "sound1" );
	if ( !*sound1 )
	{
		sound1 = ValueForKey( ent, "sound" );
	}
	SetKeyValue( ent, "noise", sound1 );

	// only the looping bits mean the same thing; the higher Quake 3 bits
	// (global, activator, vis_multiple) would misfire, e.g. vis_multiple
	// needs a target and aborts the map load without one
	{
		char	flags[16];

		sprintf( flags, "%d", IntegerForKey( ent, "spawnflags" ) & 3 );
		SetKeyValue( ent, "spawnflags", flags );
	}

	sound1 = ValueForKey( ent, "noise" );
	if ( !*sound1 ) {
		// remove useless speaker
		FreeEntity( ent );
	}
}

static void dkent_sound_ambient( entity_t *ent )
{
	// classname: target_speaker
	SetKeyValue( ent, "classname", "target_speaker" );
	SetKeyValue( ent, "spawnflags", "1" ); // looped

	dkent_target_speaker( ent );
}

typedef enum
{
	SOLID_NOT,			// no interaction with other objects
	SOLID_BBOX,			// touch on edge
} solid_t;

typedef enum
{
	MOVETYPE_NONE,			// never moves
	MOVETYPE_TOSS,			// gravity
	MOVETYPE_BOUNCE,
	MOVETYPE_FLOAT,			//	acts like MOVETYPE_BOUNCE until it hits a liquid, then mass/density takes over
} movetype_t;

#define	MAX_DECO_ANIM_SEQS	5		// maximum number of deco animation sequences


#define		FRAME_LOOP			0x0001		// loop from startFrame to endFrame
#define		FRAME_ONCE			0x0002		// go from startFrame to endFrame then stop

typedef struct {
	int		startFrame;
	int		endFrame;
	int		frameFlags;
} decoAnimSeq_t;

typedef struct decoInfo_s {
	char				name[MAX_QPATH];

	char				path[MAX_QPATH];
	movetype_t			moveType;
	solid_t				solidType;
	float				mass;
	qboolean			exploding;
	float				hitPoints;
	float				mins[3];
	float				maxs[3];
	int					gibType;
	int					numAnimSequences;
	decoAnimSeq_t		animSequences[MAX_DECO_ANIM_SEQS];

    struct decoInfo_s	*next;
} decoInfo_t;


//strips leading and trailing spaces from a string.
static void CsvStripSpaces(char *str) {
    if (str == NULL) return;

    //the src and dest positions we read/write to.
    int dest = 0;
    int src = 0;
    
    //the length of the string without the spaces.
    int len = 0;

    //skip past any leading spaces.
    while (str[src] == ' ') {
        src++;
    }

    //copy the string 
    while (str[src] != '\0') {
        //copy the character.
        str[dest] = str[src];

        //increment src and dest.
        dest++;
        src++;

        //increment the length
        len++;
    }

    //null terminate the string.
    str[len] = '\0';

    //remove any spaces at the end of the string.
    while (str[len - 1] == ' ') {
        //get rid of the space.
        str[len - 1] = '\0';
        //decrement the length.
        len--;
    }
}

// DK deco spawnflags
#define		DK_DECO_FLAGS_EXPLODE		0x0001
#define		DK_DECO_FLAGS_NO_BREAK		0x0002
#define		DK_DECO_FLAGS_PUSHABLE		0x0004
#define		DK_DECO_FLAGS_WOOD     		0x0008
#define		DK_DECO_FLAGS_METAL    		0x0010
#define		DK_DECO_FLAGS_GLASS    		0x0020
#define		DK_DECO_FLAGS_GIB    		0x0040
#define		DK_DECO_FLAGS_ROTATE			0x0080
#define     DK_DECO_FLAGS_TRANSLUCENT    0x0100      // 1.19 dsn

#define NUM_DECO_FIELDS 22

#define MAX_CSV_FIELDS 32
#define MAX_CSV_LINE_LENGTH 512

void ParseCsvLine( const char *line, int length, char *fields, int numFields ) {
	int		cur_field;
	char	*field_ptr;

	if ( length >= MAX_CSV_LINE_LENGTH ) {
		Com_Error( ERR_DROP, "CSV line too long" );
	}

	cur_field = 0;
	field_ptr = &fields[cur_field * MAX_CSV_LINE_LENGTH];
	while ( length ) {
		char c = *line++;
		length--;

		if (c == ',') {
			if (cur_field >= numFields) {
				Com_Error( ERR_DROP, "too many fields in a csv" );
			}
			*field_ptr++ = 0;
			cur_field++;
			field_ptr = &fields[cur_field * MAX_CSV_LINE_LENGTH];
			continue;
		} else if (c == '\n' || c == '\r') {
			*field_ptr++ = 0;
			break;
		}

		*field_ptr++ = c;
	}

	for (int i = 0; i < numFields; i++) {
		CsvStripSpaces(&fields[i * MAX_CSV_LINE_LENGTH]);
	}
}

int ParseCsvFromMemory( const char *buffer, int bufferSize, int numFields, void *env, void (*cb)(void *, char *) )
{
	char	fields[MAX_CSV_FIELDS * MAX_CSV_LINE_LENGTH];
	int		line_pos;
	int		in_comment;
	int		cur_field;
	int		numDecos = 0;
	int		exploding;
	int		i;
	const char	*line_start;
	const char	*end;

	if (numFields > MAX_CSV_FIELDS) {
		Com_Error( ERR_DROP, "Please increase MAX_CSV_FIELDS\n" );
	}

	line_start = buffer;
	end = buffer + bufferSize;
	while ( buffer < end && *buffer ) {
		char c = *buffer++;

		if (c == '\n' || c == '\r') {
			if ( buffer > end ) {
				Com_Error( ERR_DROP, "csv file line is unterminated" );
			}

			// end of line
			if (buffer - line_start > MAX_CSV_LINE_LENGTH ) {
				Com_Error( ERR_DROP, "csv file line too long" );
			}
			if ( *line_start == ';' ) {
				// comment line
				line_start = buffer;
				continue;
			}

			// parse the line
			ParseCsvLine( line_start, buffer - line_start, fields, numFields );
			line_start = buffer;

			// process the line
			cb(env, fields);
		}
	}
}

typedef struct {
	char	name[MAX_QPATH];
	char	musicfile[MAX_QPATH];
} mapMusic_t;

typedef struct {
	mapMusic_t *maps;
	int		maxMaps;
	int		numMaps;
} mapMusicEnv_t;

void ParseMusicInfoFromLine( void *env, char *fields ) {
	mapMusicEnv_t	*denv = env;
	char			*input;

	// save the map music info
	if ( denv->numMaps == denv->maxMaps ) {
		Com_Error( ERR_DROP, "Too many map infos!" );
	}

	mapMusic_t *map = &denv->maps[denv->numMaps++];

	Q_strncpyz( map->name, &fields[0 * MAX_CSV_LINE_LENGTH], sizeof( map->name ) );

	input = &fields[1 * MAX_CSV_LINE_LENGTH];

	if ( strlen(input) + strlen("music/.mp3") > MAX_QPATH ) {
		Com_Error( ERR_DROP, "music filename is too long\n" );
	}
	sprintf( map->musicfile, "music/%s.mp3", input );
}

#define MAX_MAPS_PER_MUSIC_FILE 256
#define MAP_MUSIC_FIELDS 2

static mapMusic_t	mapMusic[MAX_MAPS_PER_MUSIC_FILE];
static int			numMapMusic;
static int			mapMusicLoaded = 0;

int ParseMusicInfoFromFile( const char *qpath ) {
	void	*buffer;
	int		length;
	mapMusicEnv_t env;

	env.maps = &mapMusic[0];
	env.numMaps = 0;
	env.maxMaps = MAX_MAPS_PER_MUSIC_FILE;

	length = FS_ReadFile( qpath, &buffer );
	ParseCsvFromMemory( (char *)buffer, length, MAP_MUSIC_FIELDS, &env, &ParseMusicInfoFromLine );
	return env.numMaps;
}

typedef struct {
	decoInfo_t *decos;
	int maxDecos;
	int	numDecos;
} decoInfoEnv_t;

void ParseDecoInfoFromLine( void *env, char *fields ) {
	int				i, exploding;
	decoInfoEnv_t	*denv = env;

	// save the deco info
	if ( denv->numDecos == denv->maxDecos ) {
		Com_Error( ERR_DROP, "Too many decos!" );
	}

	decoInfo_t *deco = &denv->decos[denv->numDecos++];
	// TODO: add to hash?

	Q_strncpyz( deco->name, &fields[0 * MAX_CSV_LINE_LENGTH], sizeof( deco->name ) );
	Q_strncpyz( deco->path, &fields[1 * MAX_CSV_LINE_LENGTH], sizeof( deco->path ) );

	if ( !Q_stricmp( &fields[2 * MAX_CSV_LINE_LENGTH], "toss" ) ) {
		deco->moveType = MOVETYPE_TOSS;
	} else if ( !Q_stricmp( &fields[2 * MAX_CSV_LINE_LENGTH], "none" ) ) {
		deco->moveType = MOVETYPE_NONE;
	} else if ( !Q_stricmp( &fields[2 * MAX_CSV_LINE_LENGTH], "bounce" ) ) {
		deco->moveType = MOVETYPE_BOUNCE;
	} else if ( !Q_stricmp( &fields[2 * MAX_CSV_LINE_LENGTH], "float" ) ) {
		deco->moveType = MOVETYPE_FLOAT;
	} else {
		deco->moveType = MOVETYPE_TOSS;
	}

	if ( !Q_stricmp( &fields[3 * MAX_CSV_LINE_LENGTH], "bbox" ) ) {
		deco->solidType = SOLID_BBOX;
	} else if ( !Q_stricmp( &fields[3 * MAX_CSV_LINE_LENGTH], "not" ) ) {
		deco->solidType = SOLID_NOT;
	} else {
		deco->solidType = SOLID_BBOX;
	}

	deco->mass = atof( &fields[4 * MAX_CSV_LINE_LENGTH] );

	exploding = atoi( &fields[5 * MAX_CSV_LINE_LENGTH] );
	if ( exploding == 0 ) {
		deco->exploding = 0;
	} else {
		deco->exploding = 1;
	}

	deco->hitPoints = atof( &fields[6 * MAX_CSV_LINE_LENGTH] );

	deco->mins[0] = atof( &fields[7 * MAX_CSV_LINE_LENGTH] );
	deco->mins[1] = atof( &fields[8 * MAX_CSV_LINE_LENGTH] );
	deco->mins[2] = atof( &fields[9 * MAX_CSV_LINE_LENGTH] );
	deco->maxs[0] = atof( &fields[10 * MAX_CSV_LINE_LENGTH] );
	deco->maxs[1] = atof( &fields[11 * MAX_CSV_LINE_LENGTH] );
	deco->maxs[2] = atof( &fields[12 * MAX_CSV_LINE_LENGTH] );

	if ( !Q_stricmp( &fields[13 * MAX_CSV_LINE_LENGTH], "WOOD" ) ) {
		deco->gibType = DK_DECO_FLAGS_WOOD;
	} else if ( !Q_stricmp( &fields[13 * MAX_CSV_LINE_LENGTH], "METAL" ) ) {
		deco->gibType = DK_DECO_FLAGS_METAL;
	} else if ( !Q_stricmp( &fields[13 * MAX_CSV_LINE_LENGTH], "GLASS" ) ) {
		deco->gibType = DK_DECO_FLAGS_GLASS;
	} else if ( !Q_stricmp( &fields[13 * MAX_CSV_LINE_LENGTH], "GIBS" ) ) {
		deco->gibType = DK_DECO_FLAGS_GIB;
	} else {
		deco->gibType = DK_DECO_FLAGS_NO_BREAK;
	}

	deco->numAnimSequences = atoi( &fields[14 * MAX_CSV_LINE_LENGTH] );
	if ( deco->numAnimSequences < 0 ) {
		deco->numAnimSequences = 0;
	}
	if ( deco->numAnimSequences > MAX_DECO_ANIM_SEQS ) {
		deco->numAnimSequences = MAX_DECO_ANIM_SEQS;
	}

	for ( i = 0; i < deco->numAnimSequences; i++ ) {
		// parse anim seq
		decoAnimSeq_t *seq = deco->animSequences + i;

		if ( sscanf( &fields[(15+i) * MAX_CSV_LINE_LENGTH], "%d~%d", &seq->startFrame, &seq->endFrame) == 2 ) {
			seq->frameFlags = FRAME_LOOP;
		} else if ( sscanf( &fields[(15+i) * MAX_CSV_LINE_LENGTH], "%d %d", &seq->startFrame, &seq->endFrame ) == 2 ) {
			seq->frameFlags = FRAME_ONCE;
		} else {
			seq->startFrame = atoi( &fields[(15+i) * MAX_CSV_LINE_LENGTH] );
			seq->endFrame = seq->startFrame;
			seq->frameFlags = FRAME_LOOP;
		}
	}
}

#define MAX_DECOS_PER_FILE 256

static decoInfo_t	decos[4][MAX_DECOS_PER_FILE];
static int			num_decos[4];
static int			decos_loaded = 0;

int ParseDecoInfoFromFile( const char *qpath, int episode ) {
	void	*buffer;
	int		length;
	decoInfoEnv_t env;

	if ( episode < 0 || episode > 3 ) {
		Com_Error( ERR_DROP, "wrong episode" );
	}

	env.decos = &decos[episode][0];
	env.numDecos = 0;
	env.maxDecos = MAX_DECOS_PER_FILE;

	length = FS_ReadFile( qpath, &buffer );
	ParseCsvFromMemory( (char *)buffer, length, NUM_DECO_FIELDS, &env, &ParseDecoInfoFromLine );
	return env.numDecos;
}

decoInfo_t *GetDeco( int episode, const char *name ) {
	if ( episode < 0 || episode > 3 ) {
		Com_Error( ERR_DROP, "wrong episode" );
	}

	for ( int i = 0; i < num_decos[episode]; i++ ) {
		decoInfo_t *info = &decos[episode][i];

		if ( !Q_stricmp( name, info->name ) ) {
			return info;
		}
	}

	return NULL;	
}

// JKA misc_model_breakable spawnflags
#define BR_FLAGS_SOLID			1 // Movement is blocked by it, if not set, can still be broken by explosions and shots if it has health
#define BR_FLAGS_AUTOANIMATE	2 // Will cycle it's anim
#define BR_FLAGS_DEADSOLID		4 // Stay solid even when destroyed (in case damage model is rather large).
#define BR_FLAGS_NO_DMODEL		8 // Makes it NOT display a damage model when destroyed, even if one exists
#define BR_FLAGS_USE_MODEL		16 // When used, will toggle to it's usemodel (model + "_u1.md3")... this obviously does nothing if USE_NOT_BREAK is not checked
#define BR_FLAGS_USE_NOT_BREAK	32 // Using it, doesn't make it break, still can be destroyed by damage
#define BR_FLAGS_PLAYER_USE		64 // Player can use it with the use button
#define BR_FLAGS_NO_EXPLOSION	128 // By default, will explode when it dies...this is your override.
#define BR_FLAGS_START_OFF		256 // Will start off and will not appear until used.

// deco_e1 .. deco_e4: the model and its properties come from the episode's decoinfo.csv
static void dkent_deco( entity_t *ent, int episode )
{
	// NOTE: this entity is only for JKA

	epair_t pairs[] = {
		{ NULL, "scale", "modelscale" }
	};

	int		spawnflags = IntegerForKey( ent, "spawnflags" );

	RenameKeys( ent, sizeof( pairs ) / sizeof( *pairs ), pairs );

	// JKA: misc_model_breakable
	// mint-arena: misc_gamemodel
	SetKeyValue( ent, "classname", "misc_gamemodel" );

	int		jkaSpawnFlags = 0;

/*
"health"	how much health to have - default is zero (not breakable)  If you don't set the SOLID flag, but give it health, it can be shot but will not block NPCs or players from moving
"targetname" when used, dies and displays damagemodel (model + "_d1.md3"), if any (if not, removes itself)
"target" What to use when it dies
"target2" What to use when it's repaired
"target3" What to use when it's used while it's broken
"paintarget" target to fire when hit (but not destroyed)
"count"  the amount of armor/health/ammo given (default 50)
"radius"  Chunk code tries to pick a good volume of chunks, but you can alter this to scale the number of spawned chunks. (default 1)  (.5) is half as many chunks, (2) is twice as many chunks
"NPC_targetname" - Only the NPC with this name can damage this
"forcevisible" - When you turn on force sight (any level), you can see these draw through the entire level...
"redCrosshair" - crosshair turns red when you look at this

"gravity"	if set to 1, this will be affected by gravity
"throwtarget" if set (along with gravity), this thing, when used, will throw itself at the entity whose targetname matches this string
"mass"		if gravity is on, this determines how much damage this thing does when it hits someone.  Default is the size of the object from one corner to the other, that works very well.  Only override if this is an object whose mass should be very high or low for it's size (density)

Damage: default is none
"splashDamage" - damage to do (will make it explode on death)
"splashRadius" - radius for above damage

"team" - This cannot take damage from members of this team:
	"player"
	"neutral"
	"enemy"
*/

	const char *name;
	char		modelname[MAX_QPATH];
	decoInfo_t *deco;

	name =  ValueForKey( ent, "model" );
	deco = GetDeco( episode, name );
	if ( deco == NULL ) {
		Com_Printf( "WARNING: unknown deco %s in episode %d, removed\n", name, episode + 1 );
		FreeEntity( ent );
		return;
	}

	COM_StripExtension( deco->path, modelname );
	strcat( modelname, ".md3" );
	SetKeyValue( ent, "model", modelname );

	// set other properties

	if ( deco->solidType == SOLID_BBOX ) {
		jkaSpawnFlags |= BR_FLAGS_SOLID;
		jkaSpawnFlags |= BR_FLAGS_DEADSOLID;
	}

	if ( deco->moveType == MOVETYPE_NONE ) {
		
	} else {
		SetKeyInteger( ent, "gravity", 1 );
	}

	float health = FloatForKey( ent, "health" );
	if ( !health ) {
		health = deco->hitPoints;
	}
	if ( spawnflags & DK_DECO_FLAGS_NO_BREAK ) {
		health = 0;
	}

    if( ( ( spawnflags & ( DK_DECO_FLAGS_WOOD | DK_DECO_FLAGS_METAL | DK_DECO_FLAGS_GLASS | DK_DECO_FLAGS_GIB ) )
		 || deco->exploding == 1
		 || spawnflags & DK_DECO_FLAGS_EXPLODE )
		&& !( spawnflags & DK_DECO_FLAGS_NO_BREAK ) ) {
		// it explodes
		// TODO: set chunk material
		/*
		"material" - default is "8 - MAT_NONE" - choose from this list:
		0 = MAT_METAL		(grey metal)
		1 = MAT_GLASS		
		2 = MAT_ELECTRICAL	(sparks only)
		3 = MAT_ELEC_METAL	(METAL chunks and sparks)
		4 =	MAT_DRK_STONE	(brown stone chunks)
		5 =	MAT_LT_STONE	(tan stone chunks)
		6 =	MAT_GLASS_METAL (glass and METAL chunks)
		7 = MAT_METAL2		(blue/grey metal)
		8 = MAT_NONE		(no chunks-DEFAULT)
		9 = MAT_GREY_STONE	(grey colored stone)
		10 = MAT_METAL3		(METAL and METAL2 chunk combo)
		11 = MAT_CRATE1		(yellow multi-colored crate chunks)
		12 = MAT_GRATE1		(grate chunks--looks horrible right now)
		13 = MAT_ROPE		(for yavin_trial, no chunks, just wispy bits )
		14 = MAT_CRATE2		(red multi-colored crate chunks)
		15 = MAT_WHITE_METAL (white angular chunks for Stu, NS_hideout )
		*/
		jkaSpawnFlags |= BR_FLAGS_NO_DMODEL;
	} else {
		jkaSpawnFlags |= BR_FLAGS_NO_EXPLOSION;
	}

	// set up the deco's frame animation
#if 1
	/*
	if ( ( deco->animSequences > 0 ) ) {
		// TODO: add the necessary logic to JKA
		jkaSpawnFlags |= BR_FLAGS_AUTOANIMATE;
	}
	*/
#elif 0
	if ((info.animSequences > 0) && (hook->animseq != -1))
	{
		if (hook->animseq > (info.animSequences - 1))
		{
	        gstate->Con_Printf ("animseq specified for deco at %s is invalid!!\n", com->vtos(self->s.origin));
			self->s.frame = 0;
		}
		else
		{
			LPANIMSEQ lpSeq = &info.seq[hook->animseq];
			com->AnimateEntity(self, lpSeq->start, lpSeq->end, lpSeq->flags, 0.10);
		}
	}
	else
	{
		//set the frame
		//self->s.frame = hook->frame;
		entAnimate(self,0,hook->frame,FRAME_ONCE,0.05);
	}
#endif

	SetKeyInteger( ent, "health",  health );
	SetKeyFloat( ent, "mass", deco->mass );
	SetVectorForKey( ent, "mins", deco->mins );
	SetVectorForKey( ent, "maxs", deco->maxs );
	SetKeyInteger( ent, "spawnflags", jkaSpawnFlags );
}

static void dkent_effect_rain( entity_t *ent ) {
	// NOTE: this is only for JKA
	// TODO: add misc_weather_zone also!
	//SetKeyValue( ent, "classname", "fx_rain" );

	// TODO: the brushmodel needs to have CONTENTS_OUTSIDE
	// - actually no. the leaves of the worldmodel have to be modified:
	//   - add brushes of the submodel with CONTENTS_OUTSIDE to them.
	//   - if these brushes are ONLY CONTENTS_OUTSIDE then we shouldn't break anything.
	//SetKeyValue( ent, "spawnflags", "1" ); // light
}

/*
===============================================================================

ITEMS

Daikatana's weapons, ammo and pickups become the Quake 3 item that plays the
same role, and keep their own look through the "model" key (mint-arena draws
it instead of the item's model). Health and armor models depend on the episode.

===============================================================================
*/

typedef struct {
	const char	*dkClass;
	const char	*q3Class;	// NULL: not in deathmatch, removed
	const char	*model;		// %d is the episode
} dkItem_t;

static const dkItem_t dkItems[] = {
	// melee
	{ "weapon_gashands",	"weapon_gauntlet",			"models/e1/a_gashand.md3" },
	{ "weapon_hammer",		"weapon_gauntlet",			"models/e2/a_hammer.md3" },
	{ "weapon_silverclaw",	"weapon_gauntlet",			"models/e3/a_claw.md3" },
	{ "weapon_daikatana",	"weapon_gauntlet",			"models/global/a_daikatana.md3" },
	// episode 1
	{ "weapon_ionblaster",	"weapon_plasmagun",			"models/e1/a_ion.md3" },
	{ "weapon_c4viz",		"weapon_grenadelauncher",	"models/e1/a_c4.md3" },
	{ "weapon_shotcycler",	"weapon_shotgun",			"models/e1/a_shot.md3" },
	{ "weapon_sidewinder",	"weapon_rocketlauncher",	"models/e1/a_swindr.md3" },
	{ "weapon_shockwave",	"weapon_bfg",				"models/e1/a_shokwv.md3" },
	// episode 2
	{ "weapon_discus",		"weapon_grenadelauncher",	"models/e2/a_discus.md3" },
	{ "weapon_venomous",	"weapon_plasmagun",			"models/e2/a_venom.md3" },
	{ "weapon_sunflare",	"weapon_rocketlauncher",	"models/e2/a_sflare.md3" },
	{ "weapon_trident",		"weapon_shotgun",			"models/e2/a_tri.md3" },
	{ "weapon_zeus",		"weapon_lightning",			"models/e2/a_zeus.md3" },
	// episode 3
	{ "weapon_bolter",		"weapon_machinegun",			"models/e3/a_bolter.md3" },
	{ "weapon_ballista",	"weapon_railgun",			"models/e3/a_bal.md3" },
	{ "weapon_stavros",		"weapon_rocketlauncher",	"models/e3/a_stav.md3" },
	{ "weapon_wyndrax",		"weapon_lightning",			"models/e3/a_wyndrx.md3" },
	{ "weapon_nightmare",	"weapon_bfg",				"models/e3/a_nmare.md3" },
	// episode 4
	{ "weapon_glock",		"weapon_machinegun",		"models/e4/a_glock.md3" },
	{ "weapon_slugger",		"weapon_shotgun",			"models/e4/a_slug.md3" },
	{ "weapon_ripgun",		"weapon_plasmagun",			"models/e4/a_ripgun.md3" },
	{ "weapon_kineticore",	"weapon_grenadelauncher",	"models/e4/a_kcore.md3" },
	{ "weapon_novabeam",	"weapon_railgun",			"models/e4/a_nova.md3" },
	{ "weapon_metamaser",	"weapon_bfg",				"models/e4/a_mmaser.md3" },

	// ammo goes with the weapon that uses it; only base Quake 3 weapons, the
	// Team Arena ones are missing outside the missionpack build
	{ "ammo_ionpack",		"ammo_cells",				"models/e1/wa_ion.md3" },
	{ "ammo_c4",			"ammo_grenades",			"models/e1/wa_c4.md3" },
	{ "ammo_shells",		"ammo_shells",				"models/e1/wa_shot6.md3" },
	{ "ammo_rockets",		"ammo_rockets",				"models/e1/wa_swindr.md3" },
	{ "ammo_shocksphere",	"ammo_bfg",					"models/e1/wa_shokwv.md3" },
	{ "ammo_discus",		"ammo_grenades",			"models/e2/a_discus.md3" },	// no ammo model, DK reuses the weapon
	{ "ammo_venomous",		"ammo_cells",				"models/e2/wa_venom.md3" },
	{ "ammo_sunflare",		"ammo_rockets",				"models/e2/a_sflare.md3" },
	{ "ammo_tritips",		"ammo_shells",				"models/e2/wa_trident.md3" },
	{ "ammo_zeus",			"ammo_lightning",			"models/e2/wa_zeus.md3" },
	{ "ammo_bolts",			"ammo_bullets",				"models/e3/wa_bolt.md3" },
	{ "ammo_ballista",		"ammo_slugs",				"models/e3/wa_bal.md3" },
	{ "ammo_stavros",		"ammo_rockets",				"models/e3/wa_stav.md3" },
	{ "ammo_wisp",			"ammo_lightning",			"models/e3/a_wyndrx.md3" },
	{ "ammo_bullets",		"ammo_bullets",				"models/e4/wa_glock.md3" },
	{ "ammo_slugger",		"ammo_shells",				"models/e4/wa_rip.md3" },
	{ "ammo_cordite",		"ammo_shells",				"models/e4/wa_rip.md3" },
	{ "ammo_ripgun",		"ammo_cells",				"models/e4/wa_slug.md3" },
	{ "ammo_kineticore",	"ammo_grenades",			"models/e4/wa_kcore.md3" },
	{ "ammo_novabeam",		"ammo_slugs",				"models/e4/wa_nova.md3" },
	{ "ammo_metamaser",		"ammo_bfg",					"models/e4/a_mmaser.md3" },

	// health and armor
	{ "item_health_25",		"item_health",				"models/e%d/a%d_hlth.md3" },
	{ "item_health_50",		"item_health_large",		"models/e%d/a%d_hlth2.md3" },
	{ "item_goldensoul",	"item_health_mega",			"models/global/a_gsoul.md3" },
	{ "item_plasteel_armor",	"item_armor_combat",	"models/e%d/a%d_ar1.md3" },
	{ "item_silver_armor",		"item_armor_combat",	"models/e%d/a%d_ar1.md3" },
	{ "item_chainmail_armor",	"item_armor_combat",	"models/e%d/a%d_ar1.md3" },
	{ "item_kevlar_armor",		"item_armor_combat",	"models/e%d/a%d_ar1.md3" },
	{ "item_chromatic_armor",	"item_armor_body",		"models/e%d/a%d_ar2.md3" },
	{ "item_gold_armor",		"item_armor_body",		"models/e%d/a%d_ar2.md3" },
	{ "item_black_adamant_armor",	"item_armor_body",	"models/e%d/a%d_ar2.md3" },
	{ "item_ebonite_armor",		"item_armor_body",		"models/e%d/a%d_ar2.md3" },

	// artifacts: Daikatana's permanent boosts become timed powerups
	{ "item_wraithorb",		"item_invis",				"models/global/a_wraithorb.md3" },
	{ "item_power_boost",	"item_quad",				"models/global/a_pwrb.md3" },
	{ "item_attack_boost",	"item_haste",				"models/global/a_atkb.md3" },
	{ "item_speed_boost",	"item_haste",				"models/global/a_spdb.md3" },
	{ "item_acro_boost",	"item_haste",				"models/global/a_acrb.md3" },
	{ "item_vita_boost",	"item_regen",				"models/global/a_vtlb.md3" },
	{ "item_megashield",	"item_enviro",				"models/global/a_mshield.md3" },
	{ "item_invincibility",	"item_enviro",				"models/global/a_invincibility.md3" },
	{ "item_antidote",		"holdable_medkit",			"models/global/a_antidote.md3" },

	// treasure chests only matter in single player
	{ "item_black_chest",	NULL,						NULL },
	{ "item_wood_chest",	NULL,						NULL },
};

static int dk_episode = 1;	// from worldspawn, picks health and armor models

static qboolean dkent_item( entity_t *ent, const char *className )
{
	char	model[MAX_QPATH];
	int		i;

	for ( i = 0; i < (int)( sizeof( dkItems ) / sizeof( dkItems[0] ) ); i++ ) {
		const dkItem_t *item = &dkItems[i];

		if ( Q_stricmp( className, item->dkClass ) ) {
			continue;
		}
		if ( !item->q3Class ) {
			FreeEntity( ent );
			return qtrue;
		}
		if ( item->model ) {
			snprintf( model, sizeof( model ), item->model, dk_episode, dk_episode );
			SetKeyValue( ent, "model", model );
		}
		SetKeyValue( ent, "classname", item->q3Class );
		return qtrue;
	}
	return qfalse;
}

static void dkent_worldspawn( const char *mapname, vec3_t worldMins, vec3_t worldMaxs, entity_t *ent )
{
	int			i;
	mapMusic_t	*music;
	epair_t		pairs[] = {
		{ NULL, "mapname", "message" }
	};

	RenameKeys( ent, sizeof( pairs ) / sizeof( *pairs ), pairs );

	// item models are per episode; maps name it, or it's the digit in "eNdm1"
	dk_episode = IntegerForKey( ent, "episode" );
	if ( dk_episode < 1 || dk_episode > 4 ) {
		dk_episode = ( mapname[0] == 'e' && mapname[1] >= '1' && mapname[1] <= '4' ) ? mapname[1] - '0' : 1;
	}

	// loadscreen: says which load screen to use
	// but Q3 always picks map name instead

	for ( i = 0; i < numMapMusic; i++ ) {
		music = &mapMusic[i];

		if ( Q_stricmp( mapname, music->name ) ) {
			continue;
		}

		SetKeyValue( ent, "music", music->musicfile );
		break;
	}

	// 1. for atmospheric effects, need to generate tracemap
	// 2. for tracemap, need world bounds

	float mins[2] = { worldMins[0], worldMins[1] };
	float maxs[2] = { worldMaxs[0], worldMaxs[1] };

	SetKeyVector2D( ent, "mapcoordsmins", mins );
	SetKeyVector2D( ent, "mapcoordsmaxs", maxs );

	// ignore fog_value, fog_color, fog_start, fog_end
	// JKA supports "global fog", does it not?

	// let q3map2 know that light entities are left intact
	SetKeyInteger( ent, "_keeplights", 1 );
}

static void ProcessEntity( const char *mapname, vec3_t worldMins, vec3_t worldMaxs, int entityNum, entity_t *ent )
{
	const char *className = ValueForKey(ent, "classname");

	if (!strcmp(className, "worldspawn")) {
		dkent_worldspawn( mapname, worldMins, worldMaxs, ent );
	}
	else if (!strcmp(className, "info_player_start") || !strcmp(className, "info_player_deathmatch")
		|| !strcmp(className, "info_player_intermission") || !strcmp(className, "trigger_teleport")) {
		// same entity in Quake 3
	}
	else if (dkent_item( ent, className )) {
		// converted or removed
	}
	else if (!strcmp(className, "info_teleport_destination")) {
		SetKeyValue( ent, "classname", "misc_teleporter_dest" );
	}
	else if (!strcmp(className, "effect_rain")) {
		dkent_effect_rain( ent );
	}
	else if (!strcmp(className, "effect_snow")) {
		// mint-arena's cgame makes snow volumes from it as is
	}
	else if (!strncmp(className, "deco_e", 6) && className[6] >= '1' && className[6] <= '4' && !className[7]) {
		dkent_deco( ent, className[6] - '1' );
	}
	else if (!strcmp(className, "light")) {
		// keep it
	}
	else if (!strcmp(className, "trigger_hurt")) {
		dkent_trigger_hurt( ent );
	}
	else if (!strcmp(className, "target_speaker")) {
		dkent_target_speaker( ent );
	}
	else if (!strcmp(className, "sound_ambient")) {
		dkent_sound_ambient( ent );
	}
	else if (!strcmp(className, "info_null")) {
		// keep it
	}
	else if (!strcmp(className, "info_not_null")) {
		SetKeyValue( ent, "classname", "info_notnull" );
		// keep it
	}
	//else if (!strcmp(className, "func_areaportal")) {
	//	// keep it
	//}
	else if (!strcmp(className, "func_button")) {
		// keep it
	}
	else if (!strcmp(className, "func_door")) {
		// keep it
		// TODO: quake3 does not support custom sounds!
		// TODO: in DK touching doors will be linked unless turned off via a flag..
		char door_name[32];

		door_name[0] = '\0';
		sprintf( door_name, "door%d", entityNum );

		SetKeyValue( ent, "team", door_name );
	}
	else if (!strcmp(className, "func_timer")) {
		// keep it
		/* DK:
///////////////////////////////////////////////////////////////////////////////
//	func_timer (0.3 0.1 0.6) (-8 -8 -8) (8 8 8) FUNC_DYNALIGHT_START_ON
//	wait		- base time between triggering all targets, default is 1
//	random		- wait variance, default is 0
//
//	so, the basic time between firing is a random time between
//	(wait - random) and (wait + random)
//
//	delay		- delay before first firing when turned on, default is 0
//
//	pausetime	- additional delay used only the very first time
//				  and only if spawned with FUNC_DYNALIGHT_START_ON
//
///////////////////////////////////////////////////////////////////////////////
		*/
	}
	else {
		// remove it
		FreeEntity( ent );
	}
}

static void ConvertEntityString(const char *mapname, bspFile_t *bsp, const void *data, dheader_t *header, skybox_t *skybox, vec3_t worldMins, vec3_t worldMaxs)
{
	static char mapEntityString[MAX_MAP_ENTSTRING];
	static int mapEntityStringSize;

	mapEntityStringSize = GetLumpElements( header, LUMP_ENTITIES, 1 );
	if ( mapEntityStringSize >= MAX_MAP_ENTSTRING ) {
		Com_Error( ERR_DROP, "Entity string too long" );
	}

	CopyLump( header, LUMP_ENTITIES, data, mapEntityString, mapEntityStringSize, qfalse ); /* NO SWAP */	

	// dump the entities next to the output for debugging, if the directory exists
	{
		FILE *fp = fopen( "maps/entities.txt", "wb" );
		if ( fp ) {
			fwrite( mapEntityString, 1, mapEntityStringSize, fp );
			fclose( fp );
		}
	}

	ParseEntities( mapEntityString, mapEntityStringSize );

	int numEntities = NumEntities();
	for (int i = 0; i < numEntities; i++) {
		ProcessEntity( mapname, worldMins, worldMaxs, i, GetEntityNum( i ) );
		// PrintEntity( GetEntityNum( i ) );
	}
	if ( numEntities >= 1)
	{
		SkyboxFromEntity( skybox, GetEntityNum( 0 ) );
	}

	// TODO: JKA only. add this entity IF there's at least one effect_rain on the map
	/*{
		entity_t *fx_rain_ent = GetEmptyEntity();
		SetKeyValue( fx_rain_ent, "classname", "fx_rain" );
		SetKeyValue( fx_rain_ent, "spawnflags", "1" );
	}*/

	UnparseEntities( mapEntityString, &mapEntityStringSize, MAX_MAP_ENTSTRING );

	bsp->entityStringLength = mapEntityStringSize;
	bsp->entityString = malloc( bsp->entityStringLength );
	memcpy( bsp->entityString, mapEntityString, bsp->entityStringLength );
}

int DK_SurfaceFlagsToJKA( int flags )
{
	int result = 0;

	if (flags & SURF_SKY)
		result |= JKA_SURF_SKY;
	if (flags & SURF_WARP)
		result |= JKA_SURF_NOMARKS;
	
	if (flags & SURF_NODRAW)
		result |= JKA_SURF_NODRAW;
	
	return result;
}

int DK_SurfaceFlagsToQuake3( int flags )
{
	int result = 0;

	if (flags & SURF_FULLBRIGHT)
		result |= Q3_SURF_NOLIGHTMAP;
	if (flags & SURF_SKY)
		result |= Q3_SURF_SKY;
	if (flags & SURF_WARP)
		result |= Q3_SURF_NONSOLID | Q3_SURF_NOMARKS | Q3_SURF_NOLIGHTMAP;

	if (flags & SURF_NODRAW)
		result |= Q3_SURF_NODRAW;
	if (flags & SURF_HINT)
		result |= Q3_SURF_HINT;
	if (flags & SURF_SKIP)
		result |= Q3_SURF_SKIP;
	
	return result;
}

int DK_ContentsToJKA( int contents ) {
	int result = 0;

	if (contents & CONTENTS_SOLID)
		result |= JKA_CONTENTS_SOLID;
//WINDOW
//AUX
	if (contents & CONTENTS_LAVA)
		result |= JKA_CONTENTS_LAVA;
	if (contents & CONTENTS_SLIME)
		result |= JKA_CONTENTS_SLIME;
	if (contents & CONTENTS_WATER)
		result |= JKA_CONTENTS_WATER;
	if (contents & CONTENTS_FOG)
		result |= JKA_CONTENTS_FOG;

	if (contents & CONTENTS_PLAYERCLIP)
		result |= JKA_CONTENTS_PLAYERCLIP;
	
	if (contents & CONTENTS_MONSTERCLIP)
		result |= JKA_CONTENTS_MONSTERCLIP;

	if (contents & CONTENTS_NPCCLIP)
		result |= JKA_CONTENTS_BOTCLIP;

	if (contents & CONTENTS_TRANSLUCENT)
		result |= JKA_CONTENTS_TRANSLUCENT;
	if (contents & CONTENTS_DETAIL)
		result |= JKA_CONTENTS_DETAIL;

	return result;
}

int DK_ContentsToQuake3( int contents )
{
	int result = 0;

	if (contents & CONTENTS_SOLID)
		result |= Q3_CONTENTS_SOLID;
//WINDOW
//AUX
	if (contents & CONTENTS_LAVA)
		result |= Q3_CONTENTS_LAVA;
	if (contents & CONTENTS_SLIME)
		result |= Q3_CONTENTS_SLIME;
	if (contents & CONTENTS_WATER)
		result |= Q3_CONTENTS_WATER;
	if (contents & CONTENTS_FOG)
		result |= Q3_CONTENTS_FOG;

	if (contents & CONTENTS_ORIGIN)
		result |= Q3_CONTENTS_ORIGIN;

	if (contents & CONTENTS_AREAPORTAL)
		result |= Q3_CONTENTS_AREAPORTAL;

	if (contents & CONTENTS_PLAYERCLIP)
		result |= Q3_CONTENTS_PLAYERCLIP;
	
	if (contents & CONTENTS_MONSTERCLIP)
		result |= Q3_CONTENTS_MONSTERCLIP;

	if (contents & CONTENTS_NPCCLIP)
		result |= Q3_CONTENTS_BOTCLIP;

	if (contents & CONTENTS_TRANSLUCENT)
		result |= Q3_CONTENTS_TRANSLUCENT;
	if (contents & CONTENTS_DETAIL)
		result |= Q3_CONTENTS_DETAIL;

	return result;
}

static int StringEndsWith(const char *str, const char *suffix)
{
    size_t len = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  len)
        return 0;

    return strncmp(str + len - lensuffix, suffix, lensuffix) == 0;
}

#define FILE_HASH_SIZE 256
static long generateHashValue( const char *fname ) {
	int		i;
	long	hash;
	char	letter;

	hash = 0;
	i = 0;
	while (fname[i] != '\0') {
		letter = tolower(fname[i]);
		if (letter =='\\') letter = '/';		// damn path names
		hash+=(long)(letter)*(i+119);
		i++;
	}
	hash &= (FILE_HASH_SIZE-1);
	return hash;
}

static void OutputGeneratedFogShaders( const char *basename, mfog_t *fogs, int numFogs, strbuf_t *sb )
{
	int		i;
	mfog_t	*fog;
	char	line[2048];

	if (VIEWFOGS)
	{
		return;
	}

	for (i=0 ; i< numFogs ; i++)
	{
		fog = &fogs[i];

		sprintf (line, "%s\n", fog->shader);
		AppendToBuffer( sb, line );

		AppendToBuffer( sb, "{\n" );

		float	color[3];
		float	distance;

		color[0] = ( float )( fog->texinfo->color[0] ) / 255.0;
		color[1] = ( float )( fog->texinfo->color[1] ) / 255.0;
		color[2] = ( float )( fog->texinfo->color[2] ) / 255.0;

		distance = fog->texinfo->value;
		if( distance == 0 )
		{
			distance = 128;
		}

		sprintf( line, "surfaceparm trans\nsurfaceparm nonsolid\nsurfaceparm fog\nsurfaceparm nolightmap\n" );
		AppendToBuffer( sb, line );

		sprintf(line, "fogparms ( %f %f %f ) %f\n",
			color[0], color[1], color[2], distance);
		AppendToBuffer( sb, line );

		AppendToBuffer( sb, "}\n" );
	}
}

/*
===============================================================================

Generated surface shaders

Daikatana draws its surface effects from texinfo flags. Every texture and
effect flag combination gets a shader named <texture>_dk<flags> that
reproduces the effect, with constants from the Daikatana GL renderer.
Names only depend on the texture and flags so they're the same in every map.

===============================================================================
*/

// flags that change how a surface is drawn
#define DK_SHADER_FLAGS ( SURF_FULLBRIGHT | SURF_WARP | SURF_TRANS33 | SURF_TRANS66 | SURF_FLOWING | SURF_NODRAW \
						| SURF_ALPHACHAN | SURF_MIDTEXTURE | SURF_SURGE | SURF_BIGSURGE )
#define DK_ANIMATED_FLAG	0x80000000	// marks texture animation in the shader name
#define DK_GLOW_FLAG		0x40000000	// the texture has a _glow companion

#define DK_MAX_ANIM_FRAMES	8			// animMap limit
#define DK_ANIM_FPS			2			// world texture animations run at 2 frames per second
#define DK_WAVE_FREQUENCY	0.159155f	// warp and surge run at 1 radian per second
#define DK_WARP_AMPLITUDE	4.0f		// texels
#define DK_TRANS33_ALPHA	0.33f
#define DK_TRANS66_ALPHA	0.66f

static void DK_TextureName( const mtexinfo_t *tex, char *out, int size ) {
	COM_StripExtension( tex->image->name, out );
}

static qboolean DK_HasGlowMap( const mtexinfo_t *tex ) {
	char texname[MAX_QPATH], name[MAX_QPATH + 16];
	FILE *f;

	DK_TextureName( tex, texname, sizeof( texname ) );
	snprintf( name, sizeof( name ), "%s_glow.tga", texname );
	f = fopen( name, "rb" );
	if ( !f ) {
		return qfalse;
	}
	fclose( f );
	return qtrue;
}

/*
================
DK_HasGlow

Daikatana ships _glow masks for lamps, screens and indicators but never draws
them. Lightmapped surfaces add them on top, so lights stay lit in the dark.
Animated textures need one for every frame.
================
*/
static qboolean DK_HasGlow( const mtexinfo_t *tex ) {
	const mtexinfo_t *frame;
	int numFrames;

	if ( tex->flags & ( SURF_FULLBRIGHT | SURF_NODRAW | SURF_TRANS33 | SURF_TRANS66 ) ) {
		return qfalse;
	}
	for ( frame = tex, numFrames = 0; frame && numFrames < DK_MAX_ANIM_FRAMES; frame = frame->next, numFrames++ ) {
		if ( numFrames && frame == tex ) {
			break;
		}
		if ( !DK_HasGlowMap( frame ) ) {
			return qfalse;
		}
	}
	return qtrue;
}

static int DK_ShaderFlags( const mtexinfo_t *tex ) {
	int flags = tex->flags & DK_SHADER_FLAGS;

	if ( tex->next ) {
		flags |= DK_ANIMATED_FLAG;
	}
	if ( DK_HasGlow( tex ) ) {
		flags |= DK_GLOW_FLAG;
	}
	return flags;
}

/*
================
DK_GeneratedShaderName

Plain textures keep their own name, texture lookups find them without a shader.
================
*/
static qboolean DK_GeneratedShaderName( const mtexinfo_t *tex, char *out, int size ) {
	char texname[MAX_QPATH];
	int flags = DK_ShaderFlags( tex );

	DK_TextureName( tex, texname, sizeof( texname ) );
	if ( !flags ) {
		Q_strncpyz( out, texname, size );
		return qfalse;
	}
	snprintf( out, size, "%s_dk%x", texname, flags );
	return qtrue;
}

/*
================
DK_AppendTextureMap

map, or animMap for texture animation chains
================
*/
static void DK_AppendTextureMapSuffix( strbuf_t *sb, const mtexinfo_t *tex, const char *suffix ) {
	const mtexinfo_t *frame;
	char line[2048], texname[MAX_QPATH];
	int numFrames;

	DK_TextureName( tex, texname, sizeof( texname ) );
	if ( !tex->next ) {
		snprintf( line, sizeof( line ), "\t\tmap %s%s.tga\n", texname, suffix );
		AppendToBuffer( sb, line );
		return;
	}

	snprintf( line, sizeof( line ), "\t\tanimMap %d", DK_ANIM_FPS );
	AppendToBuffer( sb, line );
	for ( frame = tex, numFrames = 0; frame && numFrames < DK_MAX_ANIM_FRAMES; frame = frame->next, numFrames++ ) {
		if ( numFrames && frame == tex ) {
			break;
		}
		DK_TextureName( frame, texname, sizeof( texname ) );
		snprintf( line, sizeof( line ), " %s%s.tga", texname, suffix );
		AppendToBuffer( sb, line );
	}
	AppendToBuffer( sb, "\n" );
}

static void DK_AppendTextureMap( strbuf_t *sb, const mtexinfo_t *tex ) {
	DK_AppendTextureMapSuffix( sb, tex, "" );
}

/*
================
DK_AppendTexCoordMods

SURF_WARP: s' = s + 4 sin( t / 8 + time ), in texels. Quake 3's turb can't
match the ~50 texel wavelength but the amplitude and speed are the same.
SURF_FLOWING scrolls along s at a fixed speed, texinfo value isn't used:
32 texels a second on warping faces, half the texture a second otherwise.
================
*/
static void DK_AppendTexCoordMods( strbuf_t *sb, const mtexinfo_t *tex ) {
	char line[2048];
	float width = tex->image->width > 0 ? tex->image->width : 64;

	if ( tex->flags & SURF_WARP ) {
		snprintf( line, sizeof( line ), "\t\ttcMod turb 0 %f 0 %f\n", DK_WARP_AMPLITUDE / width, DK_WAVE_FREQUENCY );
		AppendToBuffer( sb, line );
	}
	if ( tex->flags & SURF_FLOWING ) {
		snprintf( line, sizeof( line ), "\t\ttcMod scroll %f 0\n", ( tex->flags & SURF_WARP ) ? -32.0f / width : -0.5f );
		AppendToBuffer( sb, line );
	}
}

/*
================
DK_AppendGlowStage
================
*/
static void DK_AppendGlowStage( strbuf_t *sb, const mtexinfo_t *tex, qboolean depthEqual ) {
	if ( !DK_HasGlow( tex ) ) {
		return;
	}
	AppendToBuffer( sb, "\t{\n" );
	DK_AppendTextureMapSuffix( sb, tex, "_glow" );
	DK_AppendTexCoordMods( sb, tex );
	AppendToBuffer( sb, "\t\tblendFunc add\n\t\trgbGen identity\n" );
	if ( depthEqual ) {
		AppendToBuffer( sb, "\t\tdepthFunc equal\n" );
	}
	AppendToBuffer( sb, "\t}\n" );
}

/*
================
DK_VertexLit

Translucent lit surfaces take their light from vertex colours.
================
*/
static qboolean DK_VertexLit( const mtexinfo_t *tex ) {
	return ( tex->flags & ( SURF_TRANS33 | SURF_TRANS66 ) ) && !( tex->flags & ( SURF_FULLBRIGHT | SURF_NODRAW ) );
}

/*
================
DK_AppendGeneratedShader
================
*/
static void DK_AppendGeneratedShader( strbuf_t *sb, const mtexinfo_t *tex, const char *name ) {
	char line[2048], texname[MAX_QPATH];
	int flags = tex->flags;
	float width = tex->image->width > 0 ? tex->image->width : 64;
	qboolean translucent = ( flags & ( SURF_TRANS33 | SURF_TRANS66 ) ) != 0;
	qboolean fullbright = ( flags & SURF_FULLBRIGHT ) != 0;
	qboolean hasHoles = tex->image->num_alpha_pixels > 0;
	qboolean alphaTest = !translucent && ( ( flags & ( SURF_MIDTEXTURE | SURF_ALPHACHAN ) ) || ( fullbright && hasHoles ) );

	DK_TextureName( tex, texname, sizeof( texname ) );

	snprintf( line, sizeof( line ), "%s\n{\n\tqer_editorimage %s.tga\n", name, texname );
	AppendToBuffer( sb, line );

	if ( flags & SURF_NODRAW ) {
		AppendToBuffer( sb, "\tsurfaceparm nodraw\n\tsurfaceparm nonsolid\n}\n\n" );
		return;
	}

	if ( fullbright ) {
		AppendToBuffer( sb, "\tsurfaceparm nolightmap\n" );
	}
	if ( translucent ) {
		AppendToBuffer( sb, "\tsurfaceparm trans\n" );
	}
	if ( alphaTest ) {
		AppendToBuffer( sb, "\tsurfaceparm alphashadow\n" );
	}

	// SURF_SURGE and SURF_BIGSURGE move warping faces up and down by 4 and 16 units,
	// the wave repeats every 2pi / k texture widths ( k = 0.25 and 0.5 )
	if ( ( flags & SURF_WARP ) && ( flags & ( SURF_SURGE | SURF_BIGSURGE ) ) ) {
		qboolean big = ( flags & SURF_BIGSURGE ) != 0;
		float div = width * ( 2.0f * M_PI / ( big ? 0.5f : 0.25f ) );

		AppendToBuffer( sb, "\tq3map_tessSize 64\n" );
		snprintf( line, sizeof( line ), "\tdeformVertexes wave %f sin 0 %f 0 %f\n", div, big ? 16.0f : 4.0f, DK_WAVE_FREQUENCY );
		AppendToBuffer( sb, line );
	}

	if ( fullbright ) {
		// no lightmap, full texture brightness
		AppendToBuffer( sb, "\t{\n" );
		DK_AppendTextureMap( sb, tex );
		DK_AppendTexCoordMods( sb, tex );
		AppendToBuffer( sb, "\t\trgbGen identity\n" );
		if ( translucent ) {
			snprintf( line, sizeof( line ), "\t\tblendFunc blend\n\t\talphaGen const %f\n", ( flags & SURF_TRANS33 ) ? DK_TRANS33_ALPHA : DK_TRANS66_ALPHA );
			AppendToBuffer( sb, line );
		} else if ( alphaTest ) {
			AppendToBuffer( sb, "\t\talphaFunc GE128\n" );
		}
		AppendToBuffer( sb, "\t}\n}\n\n" );
		return;
	}

	if ( translucent ) {
		// Daikatana multiplies the lightmap over the whole blended polygon, which
		// darkens everything seen through it. Light only the surface itself,
		// from the lightmap sampled into its (subdivided) vertexes.
		AppendToBuffer( sb, "\t{\n" );
		DK_AppendTextureMap( sb, tex );
		DK_AppendTexCoordMods( sb, tex );
		snprintf( line, sizeof( line ), "\t\trgbGen exactVertex\n\t\tblendFunc blend\n\t\talphaGen const %f\n", ( flags & SURF_TRANS33 ) ? DK_TRANS33_ALPHA : DK_TRANS66_ALPHA );
		AppendToBuffer( sb, line );
		AppendToBuffer( sb, "\t}\n}\n\n" );
		return;
	}

	if ( alphaTest ) {
		// MIDTEXTURE and ALPHACHAN: palette index 255 is see through
		AppendToBuffer( sb, "\t{\n" );
		DK_AppendTextureMap( sb, tex );
		DK_AppendTexCoordMods( sb, tex );
		AppendToBuffer( sb, "\t\talphaFunc GE128\n\t\tdepthWrite\n\t}\n" );
		AppendToBuffer( sb, "\t{\n\t\tmap $lightmap\n\t\tblendFunc filter\n\t\tdepthFunc equal\n\t}\n" );
		DK_AppendGlowStage( sb, tex, qtrue );
		AppendToBuffer( sb, "}\n\n" );
		return;
	}

	// opaque, lightmapped: warp, flowing and animated surfaces
	AppendToBuffer( sb, "\t{\n\t\tmap $lightmap\n\t\trgbGen identity\n\t}\n\t{\n" );
	DK_AppendTextureMap( sb, tex );
	DK_AppendTexCoordMods( sb, tex );
	AppendToBuffer( sb, "\t\tblendFunc filter\n\t}\n" );
	DK_AppendGlowStage( sb, tex, qfalse );
	AppendToBuffer( sb, "}\n\n" );
}

/*
================
DK_OutputGeneratedShaders
================
*/
static void DK_OutputGeneratedShaders( mtexinfo_t *texInfos, int numTexInfos, strbuf_t *sb ) {
	int i, j;
	char name[MAX_QPATH], other[MAX_QPATH];

	for ( i = 0; i < numTexInfos; i++ ) {
		if ( !texInfos[i].generatedShader ) {
			continue;
		}
		DK_GeneratedShaderName( &texInfos[i], name, sizeof( name ) );

		// one shader per name
		for ( j = 0; j < i; j++ ) {
			if ( texInfos[j].generatedShader ) {
				DK_GeneratedShaderName( &texInfos[j], other, sizeof( other ) );
				if ( !Q_stricmp( name, other ) ) {
					break;
				}
			}
		}
		if ( j < i ) {
			continue;
		}

		DK_AppendGeneratedShader( sb, &texInfos[i], name );
	}
}

static void OutputTexInfoShaders( const char *basename, msurface_t *faces, int numFaces, mtexinfo_t *texInfos, int numTexInfos, skybox_t *skybox, mfog_t *fogs, int numFogs, bspFile_t *bsp )
{
	int		i, j, len, size;
	static char	filedata[16384 * 1024];
	int		filedatasize;
	msurface_t	*face;
	mtexinfo_t	*tex, *htex, *frametex;
	char	line[2048];
	char	texname[MAX_QPATH];
	strbuf_t	stringbuf;

	StringBuffer( &stringbuf, filedata, sizeof(filedata) - 1 );

	OutputGeneratedFogShaders( basename, fogs, numFogs, &stringbuf );

	SkyBoxPrint( basename, skybox, &stringbuf );

	DK_OutputGeneratedShaders( texInfos, numTexInfos, &stringbuf );

	// TODO: collect all e.g. fullbright texinfos (same surface flags + texture)
	// - assign to each group a distinct shaderNum
	// - if FULLBRIGHT: there should be a glow texture present, so use it
	// - 
/*
	for ( i = 0, tex = texInfos; i < numTexInfos; i++, tex++ ) {
		if ( !(tex->flags & SURF_FULLBRIGHT) )
		{
			continue;
		}

		sprintf (line, "texinfo_%d\n", tex->shaderNum);
		AppendToBuffer( &stringbuf, line );

		AppendToBuffer( &stringbuf, "{\n" );

		float	color[3];
		float	distance;

		color[0] = ( float )( tex->color[0] ) / 255.0;
		color[1] = ( float )( tex->color[1] ) / 255.0;
		color[2] = ( float )( tex->color[2] ) / 255.0;

		distance = tex->value;
		if( distance == 0 )
		{
			distance = 128;
		}

		sprintf( line, "surfaceparm trans\nsurfaceparm nonsolid\nsurfaceparm nolightmap\n" );
		AppendToBuffer( &stringbuf, line );

		sprintf(line, "viewfogvars ( %f %f %f ) %f\n",
			color[0], color[1], color[2], distance);
		AppendToBuffer( &stringbuf, line );

		AppendToBuffer( &stringbuf, "}\n" );
	}*/

	if (VIEWFOGS)
	{
		for ( i = 0, tex = texInfos; i < numTexInfos; i++, tex++ ) {
			if ( !(tex->flags & SURF_FOGPLANE) )
			{
				continue;
			}

			sprintf (line, "texinfo_%d\n", i);
			AppendToBuffer( &stringbuf, line );

			AppendToBuffer( &stringbuf, "{\n" );

			float	color[3];
			float	distance;

			color[0] = ( float )( tex->color[0] ) / 255.0;
			color[1] = ( float )( tex->color[1] ) / 255.0;
			color[2] = ( float )( tex->color[2] ) / 255.0;

			distance = tex->value;
			if( distance == 0 )
			{
				distance = 128;
			}

			sprintf( line, "surfaceparm trans\nsurfaceparm nonsolid\nsurfaceparm nolightmap\n" );
			AppendToBuffer( &stringbuf, line );

			sprintf(line, "viewfogvars ( %f %f %f ) %f\n",
				color[0], color[1], color[2], distance);
			AppendToBuffer( &stringbuf, line );

			AppendToBuffer( &stringbuf, "}\n" );
		}
	}

#if 0
	// deduplicate all texinfos. if two texinfos have same texture, flags -- they are the same shader
	//    - skip a texinfo that is not the first frame of animation
	static mtexinfo_t*		hashTable[FILE_HASH_SIZE];

	// TODO: need to take surfaces into account!
	// e.g. if two surfs use same texinfo, but have different surface flags,
	//   we should generate a shader for each one.
	memset( hashTable, 0, sizeof( hashTable ) );
	for ( i = 0, face = faces; i < numFaces; i++, face++ ) {
		if ( ( !face->flags && face->texinfo->image->num_alpha_pixels == 0 )
			|| ( face->flags & SURF_SKY ) )
			continue; // does not require a shader

		long hash = generateHashValue( tex->image->name );

		for ( htex = hashTable[hash]; htex; htex = htex->hash_next ) {
			if ( !strcmp( tex->image->name, htex->image->name ) ) {
				// TODO: go over dupes and figure out which of those flags to use?
				// - but then, need to group them somehow
				tex->duplicate = 1;
				break;
			}
		}
		if ( !htex ) {
			tex->hash_next = hashTable[hash];
			hashTable[hash] = tex;
		}
	}

	for ( i = 0, tex = texInfo; i < numTexInfo; i++, tex++ ) {
		if ( ( !tex->flags && tex->image->num_alpha_pixels == 0 )
			|| ( tex->flags & SURF_SKY )
			|| tex->duplicate )
			continue;

		// SURF_FLOWING + value
		// SURF_LIGHT + value
		// SURF_TRANS33, SURF_TRANS66: just set 0.33 and 0.66 alphaGen constant. disable culling. this is a transparent surf.
		// SURF_MIDTEXTURE.
		// SURF_FULLBRIGHT --> no lightmap!
		// SURF_WARP. this is warping surface. needs subdivision.
#if 0
		// print out flags
		Com_Printf("%s %d ", tex->image->name, tex->value );
#define PRINTFLAG(f) do { if (tex->flags & (f)) Com_Printf( "%s ", #f ); } while (0)
		PRINTFLAG(SURF_LIGHT);
		PRINTFLAG(SURF_FULLBRIGHT);
		PRINTFLAG(SURF_SKY);
		PRINTFLAG(SURF_WARP);
		PRINTFLAG(SURF_TRANS33);
		PRINTFLAG(SURF_TRANS66);
		PRINTFLAG(SURF_FLOWING);
		PRINTFLAG(SURF_NODRAW);
		PRINTFLAG(SURF_HINT);
		PRINTFLAG(SURF_SKIP);
		PRINTFLAG(SURF_WOOD);
		PRINTFLAG(SURF_METAL);
		PRINTFLAG(SURF_STONE);
		PRINTFLAG(SURF_GLASS);
		PRINTFLAG(SURF_ICE);
		PRINTFLAG(SURF_SNOW);
		PRINTFLAG(SURF_MIRROR);
		PRINTFLAG(SURF_TRANSTHING);
		PRINTFLAG(SURF_ALPHACHAN);
		PRINTFLAG(SURF_MIDTEXTURE);
		PRINTFLAG(SURF_PUDDLE);
		PRINTFLAG(SURF_SURGE);
		PRINTFLAG(SURF_BIGSURGE);
		PRINTFLAG(SURF_BULLETLIGHT);
		PRINTFLAG(SURF_FOGPLANE);
		PRINTFLAG(SURF_SAND);
		Com_Printf("\n");
#undef PRINTFLAG
#endif
		COM_StripExtension( tex->image->name, texname );

		sprintf( line, "%s\n", texname );
		AppendToBuffer( &stringbuf, line );
		AppendToBuffer( &stringbuf, "{\n" );

		if (tex->flags & SURF_LIGHT)
		{
			sprintf( line, "\tq3map_surfaceLight %d\n", tex->value );
			AppendToBuffer( &stringbuf, line );
		}
		if (tex->flags & SURF_FULLBRIGHT)
		{
			AppendToBuffer( &stringbuf, "\tsurfaceparm nolightmap\n" );
		}
		if (tex->flags & (SURF_TRANS33|SURF_TRANS66)) {
			AppendToBuffer( &stringbuf, "\tsurfaceparm trans\n" );
			//AppendToBuffer( &stringbuf, "\tcull none\n" );
		}
		// SURF_FLOWING
		//if (tex->flags & SURF_FLOWING) {
		//	printf("its thinking!\n");
		//}
		if (tex->flags & SURF_NODRAW) {
			AppendToBuffer( &stringbuf, "\tsurfaceparm nodraw\n" );
		}

		// isn't this surface flag???
		if ( tex->flags & SURF_WARP ) {
			AppendToBuffer( &stringbuf, "\ttessSize 256\n" ); // gl_subdivide_size default value

			if( tex->flags & ( SURF_SURGE | SURF_BIGSURGE ) )
			{
				float fWaveMultiplier, fWaveModifier;

				if( tex->flags & SURF_SURGE )
				{
					fWaveModifier = 0.250;		// small surge
					fWaveMultiplier = 1;
				}
				else
				{
					fWaveModifier = 0.500;		// big surge
					fWaveMultiplier = 4;
				}

				sprintf( line, "\tdeformVertexes wave %f sin 0 %f 0 1\n", fWaveMultiplier, fWaveModifier );
				AppendToBuffer( &stringbuf, line );
			}
		}
/*
float TURBSCALE = (256.0 / (2 * M_PI));
float ONE_DIV_SIXTYFOUR = 0.015625;
static void Foo(mtexinfo_t *texinfo, float fWarpTime) {
	float		fScroll;
	int			nWidth = texinfo->image->width;
	int			nHeight = texinfo->image->height;
	float		fOrigS, fOrigT, fWarpS, fWarpT, fS, fT, zWave;
	float		fWaveModifier = 0;
	int			fWaveMultiplier = 0;
	qboolean	bSurge = qfalse;

	if( texinfo->flags & SURF_FLOWING )
		fScroll = 64 * ( ( fWarpTime * 0.5 ) - ( int )( fWarpTime * 0.5) );
	else
		fScroll = 0;

	if( texinfo->flags & SURF_WARP ) // NOTE: actually surf flags!
	{
		if( texinfo->flags & ( SURF_SURGE | SURF_BIGSURGE ) )
		{
			bSurge = qtrue;

			if( texinfo->flags & SURF_SURGE )
			{
				fWaveModifier = 0.250;		// small surge
				fWaveMultiplier = 1;
			}
			else
			{
				fWaveModifier = 0.500;		// big surge
				fWaveMultiplier = 4;
			}
		}

		// 

		// fOrigS = texcoord.s
		// fOrigT = texcoord.t
		// NOTE: r_turbsin is 256 values of sine from the 360 degrees rotation, recorded up to 6 decimal digits
		//     for (int i = 0; i < 256; i++) {
        //       double f = sin( 360.0 * (double)(i/256.0) * M_PI/180.0 ) * 8.0;
		//		 fprintf(fp, "%g,", f);
		//     }
		fWarpS = r_turbsin[Q_ftol( ( ( fOrigT * 0.125 + fWarpTime ) * TURBSCALE ) ) & 255];
		fWarpT = r_turbsin[Q_ftol( ( ( fOrigS * 0.125 + fWarpTime ) * TURBSCALE ) ) & 255];

		fS = fOrigS + fWarpS;
		fS -= fScroll;
		fS /= nWidth;

		fT = fOrigT + fWarpT;
		fT /= nHeight;

		if( bSurge )
		{
			zWave = r_turbsin[Q_ftol( ( ( ( fS + fT ) * fWaveModifier + fWarpTime ) * TURBSCALE ) ) & 255];
			vVertex.z += zWave * fWaveMultiplier;
		}
	}
}
*/

		AppendToBuffer( &stringbuf, "\t{\n" );
		/*if ( tex->next ) {
			// TODO: animated, but only output the FIRST frame in the chain... doesn't make sense to do others
			float frequency = tex->numframes * 0.1;

			sprintf( line, "\t\tanimMap %f", frequency );
			AppendToBuffer( &stringbuf, line );
			for ( j = 0, frametex = tex; j < 8 && frametex; j++, frametex = frametex->next ) {
				sprintf( line, " %s", frametex->image->name );
				AppendToBuffer( &stringbuf, line );
			}
			AppendToBuffer( &stringbuf, "\n" );
		} else*/ {
			sprintf( line, "\t\tmap %s\n", tex->image->name );
			AppendToBuffer( &stringbuf, line );
		}
		if ( tex->flags & ( SURF_TRANS33|SURF_TRANS66) ) {
			float	transp = tex->flags & SURF_TRANS33 ? 0.33 : 0.66;

			sprintf( line, "\t\talphaGen const %f\n", transp );
			AppendToBuffer( &stringbuf, line );
		}
		if ( tex->flags & ( SURF_TRANS33|SURF_TRANS66) || tex->image->num_alpha_pixels ) {
			AppendToBuffer( &stringbuf, "\t\tblendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA\n" );
			AppendToBuffer( &stringbuf, "\t\talphaFunc GT0\n" );
			AppendToBuffer( &stringbuf, "\t\tdepthWrite\n" );
		}
		if (tex->flags & SURF_FLOWING) {
			//float fScroll = 64 * FLOWING_SPEED;

			// TODO: need to take flowing into account
			// N shaders... seems like no way to do otherwise
			AppendToBuffer( &stringbuf, "\t\ttcmod scroll 0.5 0.5\n");
		}
		AppendToBuffer( &stringbuf, "\t\trgbGen identity\n" );
		AppendToBuffer( &stringbuf, "\t}\n" );

		if (!(tex->flags & (SURF_FULLBRIGHT|SURF_WARP|SURF_NODRAW|SURF_HINT|SURF_SKIP|SURF_SKY))) {
			// add lightmap pass
			AppendToBuffer( &stringbuf, "\t{\n" );
			AppendToBuffer( &stringbuf, "\t\tmap $lightmap\n" );
			AppendToBuffer( &stringbuf, "\t\trgbgen identityLighting\n" );
			AppendToBuffer( &stringbuf, "\t\tblendFunc filter\n" );
			AppendToBuffer( &stringbuf, "\t\tdepthFunc equal\n" );
			AppendToBuffer( &stringbuf, "\t\ttcgen lightmap\n" );
			AppendToBuffer( &stringbuf, "\t}\n" );
		}

		AppendToBuffer( &stringbuf, "}\n" );
	}
#endif
	filedatasize = stringbuf.cur - stringbuf.buf;

	// the file gets the text without its terminator
	bsp->shaderStringLength = filedatasize;
	bsp->shaderString = malloc( filedatasize + 1 );
	memcpy( bsp->shaderString, filedata, filedatasize );
	bsp->shaderString[filedatasize] = '\0';
}

#if 1
// NOTE: only for JKA
void MarkOutside( int outsideShaderNum, dbrush_t *brushes, dbrushside_t *brushSides, int *leafBrushes, dnode_t *nodes, dleaf_t *leafs, int node, dmodel_t *dm ) {
	int i;
	dleaf_t *pleaf;
	dnode_t	*pnode;

	while ( node >= 0 )
	{
		pnode = &nodes[node];
		// TODO: clip?
		// - also dmodel bounds are not good enough (need also height!)
		// - check if dmodel bounds are split by the node
		//   - only on the right side: go only the right side
		//   - only on the left side: go only the left side
		//   - split: carry on with the fragments (smaller bounds)
		// - when arrived at a leaf:
		//   - add the submodel brush to the leafbrushes of this leaf

		// look at CM_PointLeafnum_r for traversal code (but that's for points, you have a box)
		MarkOutside( outsideShaderNum, brushes, brushSides, leafBrushes, nodes, leafs, pnode->children[0], dm );
		node = pnode->children[1];
	}

	pleaf = &leafs[-1 - node];
	for ( i = 0; i < pleaf->numLeafBrushes; i++ )
	{
		int brushnum = leafBrushes[pleaf->firstLeafBrush + i];
		brushes[brushnum].shaderNum = outsideShaderNum;
		//Com_Printf( "outside: %s\n", texinfos[brushes[brushnum].shaderNum].image->name );
		// brushes[brushnum].shaderNum = // common/outside
		//brushSides[brushes[brushnum].firstSide].planeNum
	}
}

static void FindSubmodelBrushRange( int *leafBrushes, dnode_t *nodes, dleaf_t *leafs, int node, int *firstBrush, int *lastBrush ) {
	int i;
	dleaf_t *pleaf;
	dnode_t	*pnode;

	while ( node >= 0 )
	{
		pnode = &nodes[node];
		FindSubmodelBrushRange( leafBrushes, nodes, leafs, pnode->children[0], firstBrush, lastBrush );
		node = pnode->children[1];
	}

	pleaf = &leafs[-1 - node];
	for ( i = 0; i < pleaf->numLeafBrushes; i++ )
	{
		int brushnum = leafBrushes[pleaf->firstLeafBrush + i];
		if (*firstBrush > brushnum)
			*firstBrush = brushnum;
		if (*lastBrush < brushnum)
			*lastBrush = brushnum;
	}
}
#endif

static void ProcessTexInfo( bspFile_t *bsp, const void *data, dheader_t *header, mtexinfo_t *texInfo, int numTexInfo ) {
	int				i;
	realDtexinfo_t	*in;
	float			*fin;
	mtexinfo_t		*out;
	char			*err = NULL;
	char			filename[256];
	char			filename_out[256];
	int				next;

	in = GetLump( header, data, LUMP_TEXINFO );
	for ( i = 0, out = texInfo; i < numTexInfo; i++, in++, out++ )
	{
		out->s[0] = LittleFloat( in->vecs[0][0] );
		out->s[1] = LittleFloat( in->vecs[0][1] );
		out->s[2] = LittleFloat( in->vecs[0][2] );
		out->s_offset = LittleFloat( in->vecs[0][3] );

		out->t[0] = LittleFloat( in->vecs[1][0] );
		out->t[1] = LittleFloat( in->vecs[1][1] );
		out->t[2] = LittleFloat( in->vecs[1][2] );
		out->t_offset = LittleFloat( in->vecs[1][3] );

		out->value = LittleLong( in->value );
		out->flags = LittleLong (in->flags);
		next = LittleLong (in->nexttexinfo);
		if (next > 0)
			out->next = texInfo + next;
		else
			out->next = NULL;
		
		sprintf(filename, "textures/%s.tga", in->texture);
		out->image = image_load_from_file(filename, &err);
		if (err != NULL)
		{
			printf("Error loading texture %s: %s", in->texture, err);
			out->image = image_notexture;
		}
		out->hash_next = NULL;

		// skip editor-only texinfos
		if (StringEndsWith(out->image->name, "clip.tga"))
		{
			out->skip = qtrue;
		}
		else if (StringEndsWith(out->image->name, "trigger.tga"))
		{
			out->skip = qtrue;
		}
		else if (StringEndsWith(out->image->name, "hint.tga"))
		{
			out->skip = qtrue;
		}
		else if (out->flags & SURF_FOGPLANE)
		{
			// even if it has a texture, it is not meant to be drawn
			out->skip = qtrue;
		}
		else if (out->flags & SURF_NODRAW)
		{
			// Daikatana never draws these, e.g. the sides of grates. They keep
			// their brush sides for collision but get no draw surface.
			out->skip = qtrue;
		}
	}

	// count animation frames
	for ( i = 0; i < numTexInfo; i++ )
	{
		mtexinfo_t *step;

		out = &texInfo[i];
		out->numframes = 1;

		for (step = out->next ; step && step != out ; step=step->next)
		{
			out->numframes++;

			// amw - to catch corrupted .wal files..
			if (out->numframes > 1024)
			{
				break;
			}
		}
	}

	fin = GetLump( header, data, LUMP_EXTSURFINFO );

	for (i = 0, out = texInfo; i < numTexInfo; i++, out++)
	{
		out->color[0] = LittleFloat (*fin++);
		out->color[1] = LittleFloat (*fin++);
		out->color[2] = LittleFloat (*fin++);
	}
}

static void ProcessFaces(
		bspFile_t *bsp, const void *data, dheader_t *header,
		msurface_t *faces, int numFaces,
		mtexinfo_t *texInfo, int numTexInfo,
		float *verts, int numVerts,
		byte *lightmapData, int lightingLength,
		medge_t *edges, int numEdges,
		int *surfEdges, int numSurfEdges
	) {
	int i, j;
	static gllightmapstate_t gl_lms;

	GL_BeginBuildingLightmaps(&gl_lms);
	{
		realDface_t *in = GetLump( header, data, LUMP_FACES );
		msurface_t *out = faces;

		for ( i = 0; i < numFaces; i++, in++, out++ )
		{
			int planenum, side, ti;

			out->firstedge = LittleLong(in->firstedge);
			out->numedges = LittleShort(in->numedges);		
			out->flags = 0;
			out->fogNum = -1;

			planenum = LittleShort(in->planenum);
			side = LittleShort(in->side);
			if (side)
			{
				// this means normal should be flipped? or the winding reversed?
				//out->flags |= SURF_PLANEBACK;
				planenum += bsp->numPlanes / 2;
			}

			out->side = side;
			out->plane = bsp->planes + planenum;
			
			//out->nNumSurfSprites = 0;

			ti = LittleShort (in->texinfo);
			if (ti < 0 || ti >= numTexInfo)
				Com_Error (ERR_DROP, "MOD_LoadBmodel: bad texinfo number");
			out->texinfo = texInfo + ti;

			CalcSurfaceExtents (verts, edges, surfEdges, out);

		// lighting info

			for (j=0 ; j<DK_MAXLIGHTMAPS ; j++)
				out->styles[j] = in->styles[j];
			j = LittleLong(in->lightofs);
			if (j == -1)
				out->samples = NULL;
			else
				out->samples = lightmapData + j;
			
			// create lightmaps and polygons
			if ( lightingLength > 0 &&
				 !( out->texinfo->flags & ( SURF_SKY | SURF_FULLBRIGHT) ) && 
				( out->texinfo->flags != SURF_FOGPLANE ) )
			{
				GL_CreateSurfaceLightmap (&gl_lms, out);
			}
			else
			{
				out->lightmaptexturenum = -1;
			}

			// set the drawing flags
			//if (out->texinfo->flags & SURF_WARP)
			//{
			//	out->flags |= SURF_DRAWTURB;
			//	GL_SubdivideSurface (out);	// cut up polygon for warps
			//}
			//if (! (out->texinfo->flags & (SURF_WARP)) ) 
			GL_BuildPolygonFromSurface(verts, edges, surfEdges, out);
			if ( out->texinfo->flags & SURF_WARP )
				DK_SubdivideWarpSurface( out );
			DK_LightPolyVertexes( &gl_lms, out );
		}
	}

	LM_UploadBlock(&gl_lms);

	if (lightingLength > 0)
	{
		mlightmap_t *lm, *lmnext;
		byte *lmbuf;
		size_t len;

		bsp->numLightmaps = gl_lms.texnum;
		len = bsp->numLightmaps * LM_BLOCKLIGHTS_SIZE;
		bsp->lightmapData = malloc( len );

		lmbuf = bsp->lightmapData + len;
		lm = gl_lms.lightmaps;
		while (lm != NULL)
		{
			lmbuf -= LM_BLOCKLIGHTS_SIZE;
			lmnext = lm->next;
			memcpy(lmbuf, lm->buffer, LM_BLOCKLIGHTS_SIZE);
			free(lm);
			lm = lmnext;
		}
	}
}

static void ConvertSurfaces( bspFile_t *bsp, msurface_t *faces, int numFaces, mtexinfo_t *texInfo, int numTexInfo ) {
	int	i,j,k;

	int numIndexes = 0;
	int numDrawVerts = 0;
	for (i = 0; i < numFaces; i++)
	{
		msurface_t *face = &faces[i];

		if (face->texinfo->skip) {
			continue; // texinfo has no shader: skip this
		}

		for (glpoly_t *poly = face->polys; poly; poly = poly->next) {
			numDrawVerts += poly->numverts;
			numIndexes += (poly->numverts - 2) * 3;
		}
	}
	
	bsp->numDrawVerts = 0;
	bsp->drawVerts = malloc( numDrawVerts * sizeof( *bsp->drawVerts ) );
	bsp->numDrawIndexes = 0;
	bsp->drawIndexes = malloc( numIndexes * sizeof( *bsp->drawIndexes ) );
	int maxSurfaces = numFaces;
	int numSurfaces = 0;
	bsp->surfaces = malloc( maxSurfaces * sizeof( *bsp->surfaces ) );
	for (i = 0; i < numFaces; i++)
	{
		msurface_t *in = &faces[i];

		dsurface_t *out = &bsp->surfaces[numSurfaces++];
		int *index;

		//if ((in->flags & SURF_FOGPLANE) || (in->texinfo->flags & SURF_FOGPLANE)) {
		//	printf("no idea %s\n", in->texinfo->image->name);
		//}

#if 1
		out->shaderNum = in->texinfo->shaderNum;
#else
		out->shaderNum = in->shaderNum;
#endif
		if (VIEWFOGS)
		{
			out->fogNum = in->fogNum;
		}
		else
		{
			out->fogNum = in->fogNum; // 0 if no fogs
		}
		out->surfaceType = MST_PLANAR;

		// convert vertices and indices, a triangle fan per polygon
		out->firstVert = bsp->numDrawVerts;
		out->firstIndex = bsp->numDrawIndexes;
		out->numVerts = 0;
		out->numIndexes = 0;
		if (!in->texinfo->skip)
		{
			for (glpoly_t *poly = in->polys; poly; poly = poly->next)
			{
				memcpy(bsp->drawVerts + bsp->numDrawVerts, poly->verts, poly->numverts * sizeof(*bsp->drawVerts));
				index = &bsp->drawIndexes[bsp->numDrawIndexes];
				for (k = 0, j = 2; j < poly->numverts; k += 3, j++) {
					index[k + 0] = out->numVerts;
					index[k + 1] = out->numVerts + j - 1;
					index[k + 2] = out->numVerts + j;
				}
				out->numVerts += poly->numverts;
				out->numIndexes += k;
				bsp->numDrawVerts += poly->numverts;
				bsp->numDrawIndexes += k;
			}
		}

		// empty surfaces for skipped texinfos draw nothing, so give them no lightmap
		// and vertex lit ones don't use one
		out->lightmapNum[0] = ( in->texinfo->skip || DK_VertexLit( in->texinfo ) ) ? -1 : in->lightmaptexturenum;

		// TODO: is this correct? Q3A does not seem to care about this
		//out->lightmapX = in->texturemins[0];
		//out->lightmapY = in->texturemins[1];
		//out->lightmapWidth = in->extents[0];
		//out->lightmapHeight = in->extents[1];
		out->lightmapX[0] = out->lightmapY[0] = 0;
		out->lightmapWidth = out->lightmapHeight = LIGHTMAP_SIZE;

		out->lightmapOrigin[0] = out->lightmapOrigin[1] = out->lightmapOrigin[2] = 0.0f;
		// TODO: are these tangent/binormal vectors in lightmap space?
		out->lightmapVecs[0][0] = out->lightmapVecs[0][1] = out->lightmapVecs[0][2] = 0.0f;
		out->lightmapVecs[1][0] = out->lightmapVecs[1][1] = out->lightmapVecs[1][2] = 0.0f;
		// this is the normal vector basically
		out->lightmapVecs[2][0] = in->plane->normal[0];
		out->lightmapVecs[2][1] = in->plane->normal[1];
		out->lightmapVecs[2][2] = in->plane->normal[2];
	}
	bsp->numSurfaces = numSurfaces;
}

static void PrintTexInfoSurfaceFlags(mtexinfo_t *texinfo)
{
		Com_Printf("%s ", texinfo->image->name );
#define PRINTFLAG(f) do { if (texinfo->flags & (f)) Com_Printf( "%s ", #f ); } while (0)
		PRINTFLAG(SURF_LIGHT);
		PRINTFLAG(SURF_FULLBRIGHT);
		PRINTFLAG(SURF_SKY);
		PRINTFLAG(SURF_WARP);
		PRINTFLAG(SURF_TRANS33);
		PRINTFLAG(SURF_TRANS66);
		PRINTFLAG(SURF_FLOWING);
		PRINTFLAG(SURF_NODRAW);
		PRINTFLAG(SURF_HINT);
		PRINTFLAG(SURF_SKIP);
		PRINTFLAG(SURF_WOOD);
		PRINTFLAG(SURF_METAL);
		PRINTFLAG(SURF_STONE);
		PRINTFLAG(SURF_GLASS);
		PRINTFLAG(SURF_ICE);
		PRINTFLAG(SURF_SNOW);
		PRINTFLAG(SURF_MIRROR);
		PRINTFLAG(SURF_TRANSTHING);
		PRINTFLAG(SURF_ALPHACHAN);
		PRINTFLAG(SURF_MIDTEXTURE);
		PRINTFLAG(SURF_PUDDLE);
		PRINTFLAG(SURF_SURGE);
		PRINTFLAG(SURF_BIGSURGE);
		PRINTFLAG(SURF_BULLETLIGHT);
		PRINTFLAG(SURF_FOGPLANE);
		PRINTFLAG(SURF_SAND);
		Com_Printf("\n");
#undef PRINTFLAG
}

static qboolean outputJKA = qfalse; // TODO: make it truly switchable


/*
===============================================================================

LIGHT GRID

Daikatana lights models from the lightmap under them (R_LightPoint traces
straight down and samples the first lit face). Bake that into a Quake 3
light grid so models, players and items get the map's lighting.

===============================================================================
*/

#define DK_LIGHTPOINT_DEPTH		2048	// R_LightPoint traces this far down
#define DK_GRID_AMBIENT			1.0f	// share of the sample used as ambient light
#define DK_GRID_DIRECTED		0.5f	// and as light from above

typedef struct {
	const realDnode_t	*nodes;
	const realDplane_t	*planes;
	const realDleaf_t	*leafs;
	const msurface_t	*faces;
} dkLightTree_t;

/*
================
DK_RecursiveLightPoint

Quake 2's RecursiveLightPoint: 1 and the light at the first lit face the
segment hits, 0 for a face without light data, -1 for no hit.
================
*/
static int DK_RecursiveLightPoint( const dkLightTree_t *tree, int nodenum, const vec3_t start, const vec3_t end, vec3_t color )
{
	const realDnode_t	*node;
	const realDplane_t	*plane;
	float				front, back, frac;
	vec3_t				mid;
	int					side, r, i, k, map;

	if ( nodenum < 0 )
		return -1;

	node = &tree->nodes[nodenum];
	plane = &tree->planes[node->planenum];
	front = DotProduct( start, plane->normal ) - plane->dist;
	back = DotProduct( end, plane->normal ) - plane->dist;
	side = front < 0;

	if ( ( back < 0 ) == side )
		return DK_RecursiveLightPoint( tree, node->children[side], start, end, color );

	frac = front / ( front - back );
	for ( k = 0; k < 3; k++ )
		mid[k] = start[k] + ( end[k] - start[k] ) * frac;

	r = DK_RecursiveLightPoint( tree, node->children[side], start, mid, color );
	if ( r >= 0 )
		return r;

	for ( i = 0; i < node->numfaces; i++ )
	{
		const msurface_t *surf = &tree->faces[node->firstface + i];
		const byte *lightmap;
		int ds, dt, smax, tmax, size;

		if ( surf->texinfo->flags & ( SURF_SKY | SURF_WARP | SURF_FULLBRIGHT | SURF_NODRAW | SURF_FOGPLANE ) )
			continue;

		ds = (int)( DotProduct( mid, surf->texinfo->s ) + surf->texinfo->s_offset );
		dt = (int)( DotProduct( mid, surf->texinfo->t ) + surf->texinfo->t_offset );
		if ( ds < surf->texturemins[0] || dt < surf->texturemins[1] )
			continue;
		ds -= surf->texturemins[0];
		dt -= surf->texturemins[1];
		if ( ds > surf->extents[0] || dt > surf->extents[1] )
			continue;

		if ( !surf->samples )
			return 0;

		smax = ( surf->extents[0] >> 4 ) + 1;
		tmax = ( surf->extents[1] >> 4 ) + 1;
		size = smax * tmax;
		lightmap = surf->samples + 3 * ( ( dt >> 4 ) * smax + ( ds >> 4 ) );

		color[0] = color[1] = color[2] = 0;
		for ( map = 0; map < DK_MAXLIGHTMAPS && surf->styles[map] != 255; map++, lightmap += size * 3 )
		{
			color[0] += lightmap[0] * DK_LIGHTMAP_MODULATE;
			color[1] += lightmap[1] * DK_LIGHTMAP_MODULATE;
			color[2] += lightmap[2] * DK_LIGHTMAP_MODULATE;
		}
		return 1;
	}

	return DK_RecursiveLightPoint( tree, node->children[!side], mid, end, color );
}

static qboolean DK_PointInSolid( const dkLightTree_t *tree, const vec3_t p )
{
	int nodenum = 0;

	while ( nodenum >= 0 )
	{
		const realDnode_t *node = &tree->nodes[nodenum];
		const realDplane_t *plane = &tree->planes[node->planenum];

		nodenum = node->children[ DotProduct( p, plane->normal ) - plane->dist < 0 ];
	}
	return ( tree->leafs[-1 - nodenum].contents & CONTENTS_SOLID ) != 0;
}

static byte DK_GridByte( float v )
{
	v = v / DK_LIGHTMAP_OVERBRIGHT + 0.5f;
	return v > 255 ? 255 : (byte)v;
}

/*
================
DK_DilateLightGrid

The renderer blends the 8 grid points around a model and skips empty ones,
so a small room whose surrounding points all sit in walls goes black. Give
empty points the average of their lit neighbours, a few times over.
================
*/
#define DK_GRID_DILATE_PASSES	2

static void DK_DilateLightGrid( byte *grid, const int bounds[3] )
{
	int		pass, x, y, z, dx, dy, dz, i, count, sum[6];
	int		numPoints = bounds[0] * bounds[1] * bounds[2];
	byte	*src = malloc( numPoints * 8 );

	for ( pass = 0; pass < DK_GRID_DILATE_PASSES; pass++ )
	{
		memcpy( src, grid, numPoints * 8 );
		for ( z = 0; z < bounds[2]; z++ )
		for ( y = 0; y < bounds[1]; y++ )
		for ( x = 0; x < bounds[0]; x++ )
		{
			byte *out = grid + ( ( z * bounds[1] + y ) * bounds[0] + x ) * 8;

			if ( out[0] | out[1] | out[2] | out[3] | out[4] | out[5] )
				continue;

			count = 0;
			memset( sum, 0, sizeof( sum ) );
			for ( dz = -1; dz <= 1; dz++ )
			for ( dy = -1; dy <= 1; dy++ )
			for ( dx = -1; dx <= 1; dx++ )
			{
				const byte *n;

				if ( x + dx < 0 || x + dx >= bounds[0] || y + dy < 0 || y + dy >= bounds[1] || z + dz < 0 || z + dz >= bounds[2] )
					continue;
				n = src + ( ( ( z + dz ) * bounds[1] + y + dy ) * bounds[0] + x + dx ) * 8;
				if ( !( n[0] | n[1] | n[2] | n[3] | n[4] | n[5] ) )
					continue;
				for ( i = 0; i < 6; i++ )
					sum[i] += n[i];
				count++;
			}
			if ( !count )
				continue;
			for ( i = 0; i < 6; i++ )
				out[i] = sum[i] / count;
			if ( !( out[0] | out[1] | out[2] ) )
				out[0] = out[1] = out[2] = 1;
		}
	}
	free( src );
}

static void DK_BuildLightGrid( bspFile_t *bsp, dheader_t *header, const void *data, const msurface_t *faces )
{
	dkLightTree_t	tree;
	vec3_t			origin, p, end, color;
	int				bounds[3], i, x, y, z;
	byte			*out;

	tree.nodes = GetLump( header, data, LUMP_NODES );
	tree.planes = GetLump( header, data, LUMP_PLANES );
	tree.leafs = GetLump( header, data, LUMP_LEAFS );
	tree.faces = faces;

	bsp->defaultLightGridSize[0] = 64;
	bsp->defaultLightGridSize[1] = 64;
	bsp->defaultLightGridSize[2] = 128;

	// the grid layout R_LoadLightGrid derives from the world bounds
	for ( i = 0; i < 3; i++ )
	{
		float size = bsp->defaultLightGridSize[i];

		origin[i] = size * ceil( bsp->submodels[0].mins[i] / size );
		bounds[i] = (int)( ( size * floor( bsp->submodels[0].maxs[i] / size ) - origin[i] ) / size ) + 1;
	}

	bsp->numGridPoints = bounds[0] * bounds[1] * bounds[2];
	bsp->lightGridData = calloc( bsp->numGridPoints, 8 );

	out = bsp->lightGridData;
	for ( z = 0; z < bounds[2]; z++ )
	for ( y = 0; y < bounds[1]; y++ )
	for ( x = 0; x < bounds[0]; x++, out += 8 )
	{
		p[0] = origin[0] + x * bsp->defaultLightGridSize[0];
		p[1] = origin[1] + y * bsp->defaultLightGridSize[1];
		p[2] = origin[2] + z * bsp->defaultLightGridSize[2];

		// zero samples are skipped by the renderer, as points in walls should be
		if ( DK_PointInSolid( &tree, p ) )
			continue;

		VectorCopy( p, end );
		end[2] -= DK_LIGHTPOINT_DEPTH;
		if ( DK_RecursiveLightPoint( &tree, 0, p, end, color ) != 1 )
			continue;

		DK_ClampLight( color );
		for ( i = 0; i < 3; i++ )
		{
			out[i] = DK_GridByte( color[i] * DK_GRID_AMBIENT );
			out[3 + i] = DK_GridByte( color[i] * DK_GRID_DIRECTED );
		}
		// a byte of 1 keeps very dark but valid samples from being skipped
		if ( !( out[0] | out[1] | out[2] ) )
			out[0] = out[1] = out[2] = 1;
		out[6] = 0;		// longitude and latitude 0: light from straight above
		out[7] = 0;
	}

	DK_DilateLightGrid( bsp->lightGridData, bounds );

	Com_Printf( "light grid %d x %d x %d\n", bounds[0], bounds[1], bounds[2] );
}

bspFile_t *BSP_LoadDK( const bspFormat_t *format, const char *name, const void *data, int length ) {
	int				i, j, k;
	dheader_t		header;
	bspFile_t		*bsp;
	char			mapnameBuffer[MAX_QPATH];
	char			*mapname;

	COM_StripExtension( name, mapnameBuffer );
	mapname = COM_SkipPath( mapnameBuffer );

	BSP_SwapBlock( (int *) &header, (int *)data, sizeof ( dheader_t ) );

	if ( header.ident != format->ident || header.version != format->version ) {
        Com_Printf("Header mismatch!\n");
		return NULL;
	}

	image_init();

	if ( !mapMusicLoaded ) {
		numMapMusic = ParseMusicInfoFromFile( "music/music.csv" );
		mapMusicLoaded = 1;
	}

	if ( !decos_loaded) {
		num_decos[0] = ParseDecoInfoFromFile( "models/e1/e1decoinfo.csv", 0 );
		num_decos[1] = ParseDecoInfoFromFile( "models/e2/e2decoinfo.csv", 1 );
		num_decos[2] = ParseDecoInfoFromFile( "models/e3/e3decoinfo.csv", 2 );
		num_decos[3] = ParseDecoInfoFromFile( "models/e4/e4decoinfo.csv", 3 );
		decos_loaded = 1;
	}

	bsp = malloc( sizeof ( bspFile_t ) );
	Com_Memset( bsp, 0, sizeof ( bspFile_t ) );

    //
    // count and alloc
    //
	int numEdges = GetLumpElements( &header, LUMP_EDGES, sizeof ( realDedge_t ) );
	medge_t *edges = malloc( numEdges * sizeof ( *edges ) );

	int numSurfEdges = GetLumpElements( &header, LUMP_SURFEDGES, sizeof( int ) );
	int *surfEdges = malloc( numSurfEdges * sizeof( int ) );

	int lightingLength = GetLumpElements( &header, LUMP_LIGHTING, sizeof( byte ) );
	byte *lightmapData = NULL;
	if ( lightingLength > 0 )
	{
		lightmapData = malloc( lightingLength * sizeof( byte) );
	}

	int numTexInfo = GetLumpElements( &header, LUMP_TEXINFO, sizeof( realDtexinfo_t ) );
	mtexinfo_t *texInfo = calloc( numTexInfo, sizeof( *texInfo ) );

	int numExtendedSurfInfo = GetLumpElements( &header, LUMP_EXTSURFINFO, sizeof( float ) );

	int numFaces = GetLumpElements( &header, LUMP_FACES, sizeof( realDface_t ) );
	msurface_t *faces = calloc( numFaces, sizeof( *faces ) );

	int numMarkSurfaces = GetLumpElements( &header, LUMP_LEAFFACES, sizeof(unsigned short) );
	msurface_t **markSurfaces = malloc( numMarkSurfaces * sizeof(*markSurfaces) );

	int visibilityLength = GetLumpElements( &header, LUMP_VISIBILITY, 1 );
	realDvis_t *visibility = NULL;
	if ( visibilityLength > 0 )
	{
		visibility = malloc( visibilityLength );
	}

	int numLeafs = GetLumpElements( &header, LUMP_LEAFS, sizeof( realDleaf_t ) );
	mleaf_t *leafs = malloc( numLeafs * sizeof( *leafs ) );

	int numLeafBrushes = GetLumpElements( &header, LUMP_LEAFBRUSHES, sizeof( unsigned short ) );
	unsigned short *leafBrushes = malloc( numLeafBrushes * sizeof( *leafBrushes ) );

	int numBrushes = GetLumpElements( &header, LUMP_BRUSHES, sizeof( realDbrush_t ) );
	mbrush_t *brushes = malloc( numBrushes * sizeof( *brushes ) );

	int numBrushSides = GetLumpElements( &header, LUMP_BRUSHSIDES, sizeof(realDbrushside_t ) );


	int numNodes = GetLumpElements( &header, LUMP_NODES, sizeof ( realDnode_t ) );
	mnode_t *nodes = malloc( numNodes * sizeof ( *nodes ) );

	bsp->numSubmodels = GetLumpElements( &header, LUMP_MODELS, sizeof( realDmodel_t ) );
	bsp->submodels = malloc( bsp->numSubmodels * sizeof( *bsp->submodels ) );

	bsp->shaderStringLength = 0;
	bsp->shaderString = NULL;

	int		numFogs = 0;
	#define MAX_FOGS 1024
	mfog_t	fogs[MAX_FOGS];

	int numVerts = GetLumpElements( &header, LUMP_VERTEXES, sizeof ( realDvertex_t ) );
	float *verts = malloc( numVerts * sizeof( *verts ) * 3 );

	//
	// copy and swap and convert data
	//
	{
		realDvertex_t *in = GetLump( &header, data, LUMP_VERTEXES );
		float *out = verts;

		for ( i = 0; i < numVerts; i++, in++, out += 3 ) {
			for (j = 0; j < 3; j++ ) {
				out[j] = LittleFloat (in->point[j]);
			}
		}
	}

	// edges
	{
		realDedge_t *in = GetLump( &header, data, LUMP_EDGES );
		medge_t *out = edges;

		for ( i=0 ; i<numEdges ; i++, in++, out++)
		{
			out->v[0] = (unsigned short)LittleShort(in->v[0]);
			out->v[1] = (unsigned short)LittleShort(in->v[1]);
		}
	}
	// surfedges
	{
		int *in = GetLump( &header, data, LUMP_SURFEDGES );
		int *out = surfEdges;

		for ( i = 0; i < numSurfEdges; i++)
		{
			out[i] = LittleLong(in[i]);
		}
	}

	// lighting
	if ( lightingLength )
	{
		byte *in = GetLump( &header, data, LUMP_LIGHTING );
		memcpy (lightmapData, in, lightingLength);
	}

	// planes
	bsp->numPlanes = GetLumpElements( &header, LUMP_PLANES, sizeof ( realDplane_t ) ) * 2;
	bsp->planes = malloc( bsp->numPlanes * sizeof ( *bsp->planes ) );
	{
		realDplane_t *in = GetLump( &header, data, LUMP_PLANES );
		dplane_t *out = bsp->planes;

		k = bsp->numPlanes / 2;
		for ( i = 0; i < k; i++, in++) {
			for (j=0 ; j<3 ; j++) {
				out[i].normal[j] = LittleFloat (in->normal[j]);
				out[k + i].normal[j] = -out[i].normal[j];
			}

			out[i].dist = LittleFloat (in->dist);
			out[k + i].dist = out[i].dist;
		}
	}

	// textures
	ProcessTexInfo( bsp, data, &header, texInfo, numTexInfo );

	// NOTE: for JKA only, add an extra "outside" shader
	int outsideShaderNum = numTexInfo - 1;
	int maxShaders = numTexInfo + 1; // outside
	int numShaders = 0;
	bsp->shaders = malloc( maxShaders * sizeof( *bsp->shaders ) );
	for (i = 0; i < numTexInfo; i++)
	{
		mtexinfo_t *texinfo = &texInfo[i];

		// TODO:
		// group texinfos by flags + texture name
		// assign to each such group a shader
		// 

		dshader_t *out = &bsp->shaders[numShaders];
		texinfo->shaderNum = numShaders;
		numShaders++;

		// print out flags
		//PrintTexInfoSurfaceFlags(texinfo);

		// special names
		if (texinfo->flags & SURF_SKY)
		{
			if ( !outputJKA ) {
				out->surfaceFlags = Q3_SURF_SKY | Q3_SURF_NOMARKS | Q3_SURF_NOIMPACT;
				out->contentFlags = Q3_CONTENTS_PLAYERCLIP;
			}
			else {
				out->surfaceFlags = JKA_SURF_SKY | JKA_SURF_NOMARKS | JKA_SURF_NOIMPACT;
				out->contentFlags = JKA_CONTENTS_SOLID;
			}

			sprintf( out->shader, "textures/%s/sky", mapname );
		}
		else if (texinfo->flags & SURF_FOGPLANE)
		{
			if ( !outputJKA ) {
				out->surfaceFlags = Q3_SURF_NODRAW | Q3_SURF_NONSOLID | Q3_SURF_NOIMPACT;
				out->contentFlags = Q3_CONTENTS_STRUCTURAL | Q3_CONTENTS_TRANSLUCENT;
			} else {
				out->surfaceFlags = JKA_SURF_NODRAW | JKA_SURF_NOIMPACT;
				out->contentFlags = JKA_CONTENTS_TRANSLUCENT;
			}

			//Q_strncpyz(out->shader, "textures/common/nodraw", sizeof( out->shader ) );
			Q_strncpyz(out->shader, texinfo->image->name, sizeof( out->shader ) );
		}
		else if (StringEndsWith(texinfo->image->name, "clip.tga"))
		{
			if ( !outputJKA ) {
				out->surfaceFlags = Q3_SURF_NOLIGHTMAP | Q3_SURF_NOMARKS | Q3_SURF_NONSOLID | Q3_SURF_NOIMPACT;
				out->contentFlags = Q3_CONTENTS_PLAYERCLIP;
			} else {
				out->surfaceFlags = JKA_SURF_NOMARKS | JKA_SURF_NOIMPACT;
				out->contentFlags = JKA_CONTENTS_PLAYERCLIP;
			}

			//Q_strncpyz(out->shader, "textures/common/clip", sizeof( out->shader ) );
			Q_strncpyz(out->shader, texinfo->image->name, sizeof( out->shader ) );
		}
		else if (StringEndsWith(texinfo->image->name, "trigger.tga"))
		{
			if ( !outputJKA ) {
				out->surfaceFlags = Q3_SURF_NODRAW;
			} else {
				out->surfaceFlags = JKA_SURF_NODRAW;
			}
			out->contentFlags = 0;
			Q_strncpyz(out->shader, "textures/common/trigger", sizeof( out->shader ) );
		}
		else if (StringEndsWith(texinfo->image->name, "hint.tga")) {
			if ( !outputJKA ) {
				out->surfaceFlags = Q3_SURF_NODRAW | Q3_SURF_NONSOLID | Q3_SURF_NOIMPACT;
				out->contentFlags = Q3_CONTENTS_STRUCTURAL | Q3_CONTENTS_TRANSLUCENT;
			} else {
				out->surfaceFlags = JKA_SURF_NODRAW | JKA_SURF_NOIMPACT;
				out->contentFlags = JKA_CONTENTS_TRANSLUCENT;
			}
			Q_strncpyz( out->shader, "textures/common/hint" , sizeof( out->shader ) );
		}
		else
		{
			if ( !outputJKA ) {
				out->surfaceFlags = DK_SurfaceFlagsToQuake3( texinfo->flags );
			} else {
				out->surfaceFlags = DK_SurfaceFlagsToJKA( texinfo->flags );
			}

			if ( texinfo->flags & SURF_WARP ) {
				if ( !outputJKA ) {
					out->contentFlags = Q3_CONTENTS_TRANSLUCENT | Q3_CONTENTS_WATER;
				} else {
					out->contentFlags = JKA_CONTENTS_TRANSLUCENT | JKA_CONTENTS_WATER;
				}
			}
			else if ( ( texinfo->flags & SURF_TRANS33 ) || ( texinfo->flags & SURF_TRANS66 ) ) {
				if ( !outputJKA ) {
					out->contentFlags = Q3_CONTENTS_TRANSLUCENT;
				} else {
					out->contentFlags = JKA_CONTENTS_TRANSLUCENT;
				}
			}
			else {
				// by default, these should be the same as in the wal file
				// or if overridden, the contents of the face? or brush?

				if ( !outputJKA ) {
					out->contentFlags = Q3_CONTENTS_SOLID; // TODO: need some mapping
				} else {
					out->contentFlags = JKA_CONTENTS_SOLID;
				}
			}

			if ( VIEWFOGS && (texinfo->flags & SURF_FOGPLANE) ) {
				char customName[MAX_QPATH];
				sprintf(customName, "texinfo_%d", i);
				Q_strncpyz(out->shader, customName, sizeof( out->shader ) );
			}
			else
			{
				char customName[MAX_QPATH];
				texinfo->generatedShader = DK_GeneratedShaderName( texinfo, customName, sizeof( customName ) );
				Q_strncpyz(out->shader, customName, sizeof( out->shader ) );
			}
		}
	}
	// texinfos repeat the same texture and flags for every texture alignment,
	// Quake 3 BSPs have each shader once (and engines limit them to 1024)
	{
		int *remap = malloc( numShaders * sizeof( *remap ) );
		int numUnique = 0, j;

		for ( i = 0; i < numShaders; i++ ) {
			dshader_t *sh = &bsp->shaders[i];

			for ( j = 0; j < numUnique; j++ ) {
				dshader_t *other = &bsp->shaders[j];
				if ( !Q_stricmp( sh->shader, other->shader ) && sh->surfaceFlags == other->surfaceFlags && sh->contentFlags == other->contentFlags ) {
					break;
				}
			}
			if ( j == numUnique ) {
				bsp->shaders[numUnique] = *sh;
				numUnique++;
			}
			remap[i] = j;
		}
		for ( i = 0; i < numTexInfo; i++ ) {
			texInfo[i].shaderNum = remap[texInfo[i].shaderNum];
		}
		Com_Printf( "%d texinfos use %d shaders\n", numShaders, numUnique );
		numShaders = numUnique;
		free( remap );
	}

	Q_strncpyz( bsp->shaders[numShaders].shader, "textures/system/outside", sizeof( bsp->shaders[numShaders].shader ) );
	bsp->shaders[numShaders].contentFlags |= JKA_CONTENTS_OUTSIDE;
	bsp->shaders[numShaders].surfaceFlags = 0;
	numShaders++;
	bsp->numShaders = numShaders;

	// brushsides
	bsp->numBrushSides = numBrushSides;
	bsp->brushSides = malloc( bsp->numBrushSides * sizeof( *bsp->brushSides ) );
	{
		dbrushside_t	*out;
		realDbrushside_t 	*in = GetLump( &header, data, LUMP_BRUSHSIDES );

		// need to save space for box planes
		if (numBrushSides > MAX_MAP_BRUSHSIDES)
			Com_Error (ERR_DROP, "Map has too many planes");
		out = bsp->brushSides;
		for ( i=0 ; i<numBrushSides ; i++, in++, out++)
		{
			//if (i >= 171 && i <= 174 || i == 262 || i == 561)
			//{
			//	Com_Printf("found the fog brush\n");
			//}
			out->planeNum = LittleShort (in->planenum);
			j = LittleShort (in->texinfo);
			if (j < 0) {
				j = 0; // HACK
				//Com_Error(ERR_DROP, "oops");
			}
			if (j >= numTexInfo)
				Com_Error (ERR_DROP, "Bad brushside texinfo");
			out->shaderNum = texInfo[j].shaderNum;
			out->surfaceNum = j;
		}
	}

	// brushes
	bsp->numBrushes = numBrushes;
	bsp->brushes = malloc( bsp->numBrushes * sizeof( *bsp->brushes ) );
	{
		realDbrush_t	*in = GetLump( &header, data, LUMP_BRUSHES );
		dbrush_t		*out;
		int				contents;

		if (numBrushes > MAX_MAP_BRUSHES)
			Com_Error (ERR_DROP, "Map has too many brushes");
		out = bsp->brushes;
		for (i=0 ; i<numBrushes ; i++, out++, in++)
		{
			out->firstSide = LittleLong( in->firstside );
			out->numSides = LittleLong( in->numsides );
			
			contents = LittleLong( in->contents );

			// HACK: for ladders. Q2/DK use contents for a ladder brush,
			// but Q3 uses surface flags for that.
			// the issue is that we should have already defined shaders at this point.
			if ( contents & CONTENTS_LADDER ) {
				for (j = 0; j < out->numSides; j++) {
					dbrushside_t *side = &bsp->brushSides[ out->firstSide + j ];

					bsp->shaders[side->shaderNum].surfaceFlags |= Q3_SURF_LADDER;
				}
			}
			
			out->shaderNum = bsp->brushSides[out->firstSide].shaderNum;
		}
	}

	// faces
	ProcessFaces( bsp, data, &header, faces, numFaces, texInfo, numTexInfo, verts, numVerts, lightmapData, lightingLength, edges, numEdges, surfEdges, numSurfEdges );

	// marksurfaces
	bsp->numLeafSurfaces = numMarkSurfaces;
	bsp->leafSurfaces = malloc( bsp->numLeafSurfaces * sizeof( *bsp->leafSurfaces ) );
	{
		unsigned short *in = GetLump( &header, data, LUMP_LEAFFACES );
		int *out = bsp->leafSurfaces;

		for ( i=0 ; i< numMarkSurfaces ; i++)
		{
			j = LittleShort(in[i]);
			if (j < 0 ||  j >= numFaces)
				Com_Error (ERR_DROP, "ParseMarksurfaces: bad surface number");
			out[i] = j;
		}
	}

	// visibility
	ConvertVisibility( bsp, data, &header, visibility, visibilityLength );

	// leafs
	{
		realDleaf_t *in = GetLump( &header, data, LUMP_LEAFS );
		mleaf_t *out = leafs;

		for ( i=0 ; i<numLeafs ; i++, in++, out++)
		{
			int	contents;

			out->mins[0] = LittleShort( in->mins[0] );
			out->mins[1] = LittleShort( in->mins[1] );
			out->mins[2] = LittleShort( in->mins[2] );

			out->maxs[0] = LittleShort( in->maxs[0] );
			out->maxs[1] = LittleShort( in->maxs[1] );
			out->maxs[2] = LittleShort( in->maxs[2] );

			if( ( out->mins[1] > out->maxs[1] ) || 
				( out->mins[0] > out->maxs[0] ) || 
				( out->mins[2] > out->maxs[2] ) ){
				printf ("Flipped bounds on node!");
			}

			contents = LittleLong(in->contents);
			// TODO: runtime switch for JKA
			//out->contents = DK_ContentsToJKA( contents );
			out->contents = DK_ContentsToQuake3( contents );

			out->cluster = LittleShort(in->cluster);
			out->area = LittleShort(in->area);

			out->firstmarksurface =  LittleShort(in->firstleafface);
			out->nummarksurfaces = LittleShort(in->numleaffaces);

			out->firstleafbrush = LittleShort(in->firstleafbrush);
			out->numleafbrushes = LittleShort(in->numleafbrushes);

			// SCG[4/17/99]: Added for volumetric fog
			if ( contents & CONTENTS_FOG )
			{
				int	brushNum = LittleLong( in->brushnum );
				int	setShader = 0;

				//Com_Printf("fog brush num %d\n", brushNum);
				for ( k = 0; k < numFogs; k++ )
				{
					if ( fogs[k].brushNum == brushNum )
						break;
				}
				if ( k == numFogs )
				{
					if ( numFogs == MAX_FOGS )
						Com_Error( ERR_DROP, "MAX_FOGS on the map" );
					fogs[numFogs].brushNum = brushNum;

					sprintf( fogs[numFogs].shader, "fogs/%s_%d", mapname, k );

					fogs[numFogs].visibleSide = 5; // 5 looks better than -1... but tesselation artifacts exist.
					numFogs++;
					setShader = 1;
				}

				// volumeNum = R_FogVolumeForBrush( in->brushnum );
				if (VIEWFOGS)
				{
					/*for (j=0 ; j<out->nummarksurfaces ; j++)
					{
						msurface_t *surf = &faces[bsp->leafSurfaces[out->firstmarksurface + j]];

						surf->texinfo->flags |= SURF_FOGPLANE;
					}*/
				}
				else
				{
					for (j=0 ; j<out->nummarksurfaces ; j++)
					{
						msurface_t *surf = &faces[bsp->leafSurfaces[out->firstmarksurface + j]];

						// looks like we need to clip each face
						// against the brush: what's inside the brush is marked
						// with the fog num.
						// we can leave the non-fogged surface as-is. but the fogged
						// surface can be appended into the surfaces list.
						// we'd still need to fix things up...

						//surf->flags |= SURF_DRAWFOG;
						//surf->brushnum = volumeNum;
						//r_fogvolumes[volumeNum].hull.Add( ( void * ) surf );
						surf->fogNum = k;
						if ( setShader && surf->texinfo->flags & SURF_FOGPLANE )
						{
							fogs[k].texinfo = surf->texinfo;
							setShader = 0;
						}
					}
				}
			}
		}
	}

	// leafbrushes
	{
		unsigned short	*out;
		unsigned short 	*in = GetLump( &header, data, LUMP_LEAFBRUSHES );

		if (numLeafBrushes < 1)
			Com_Error (ERR_DROP, "Map with no planes");
		// need to save space for box planes
		if (numLeafBrushes > MAX_MAP_LEAFBRUSHES)
			Com_Error (ERR_DROP, "Map has too many leafbrushes");

		out = leafBrushes;
		for ( i=0 ; i<numLeafBrushes ; i++, in++, out++)
			*out = LittleShort (*in);
	}

	// nodes
	bsp->numNodes = numNodes;
	bsp->nodes = malloc( bsp->numNodes * sizeof( *bsp->nodes ) );
	{
		realDnode_t		*in = GetLump( &header, data, LUMP_NODES );
		dnode_t 	*out = bsp->nodes;

		for ( i=0 ; i<numNodes ; i++, in++, out++)
		{
			out->mins[0] = LittleShort( in->mins[0] );
			out->maxs[0] = LittleShort( in->maxs[0] );

			out->mins[1] = LittleShort( in->mins[1] );
			out->maxs[1] = LittleShort( in->maxs[1] );

			out->mins[2] = LittleShort( in->mins[2] );
			out->maxs[2] = LittleShort( in->maxs[2] );

			if( ( out->mins[1] > out->maxs[1] ) || 
				( out->mins[0] > out->maxs[0] ) || 
				( out->mins[2] > out->maxs[2] ) ){
				printf ("Flipped bounds on node!");
			}

			out->planeNum = LittleLong(in->planenum);

			out->children[0] = LittleLong(in->children[0]);
			out->children[1] = LittleLong(in->children[1]);
		}
	}

	bsp->numLeafs = numLeafs;
	bsp->leafs = malloc( bsp->numLeafs * sizeof( *bsp->leafs ) );
	for (i = 0; i < numLeafs; i++) {
		mleaf_t *in = &leafs[i];
		dleaf_t *out = &bsp->leafs[i];

		out->cluster = in->cluster;
		out->area = in->area;

		//Com_Printf("%d\n", in->contents);

		out->mins[0] = in->mins[0];
		out->mins[1] = in->mins[1];
		out->mins[2] = in->mins[2];
		out->maxs[0] = in->maxs[0];
		out->maxs[1] = in->maxs[1];
		out->maxs[2] = in->maxs[2];

		out->firstLeafSurface = in->firstmarksurface;
		out->numLeafSurfaces = in->nummarksurfaces;

		out->firstLeafBrush = in->firstleafbrush;
		out->numLeafBrushes = in->numleafbrushes;
	}

	bsp->numLeafBrushes = numLeafBrushes;
	bsp->leafBrushes = malloc( bsp->numLeafBrushes * sizeof( *bsp->leafBrushes ) );
	for (i = 0; i < numLeafBrushes; i++)
	{
		bsp->leafBrushes[i] = leafBrushes[i];
	}

	ConvertSurfaces( bsp, faces, numFaces, texInfo, numTexInfo );


	{
		realDmodel_t	*in = GetLump( &header, data, LUMP_MODELS );
		dmodel_t	*out = bsp->submodels;

		for ( i=0 ; i<bsp->numSubmodels ; i++, in++, out++)
		{
			out->mins[0] = LittleFloat (in->mins[0]);
			out->mins[1] = LittleFloat (in->mins[1]);
			out->mins[2] = LittleFloat (in->mins[2]);

			out->maxs[0] = LittleFloat (in->maxs[0]);
			out->maxs[1] = LittleFloat (in->maxs[1]);
			out->maxs[2] = LittleFloat (in->maxs[2]);

			//out->origin[0] = LittleFloat (in->origin[0]);
			//out->origin[1] = LittleFloat (in->origin[1]);
			//out->origin[2] = LittleFloat (in->origin[2]);

			out->firstSurface = LittleLong (in->firstface);
			out->numSurfaces = LittleLong (in->numfaces);

			int headnode = LittleLong (in->headnode);

			// JKA: also we want to add CONTENTS_OUTSIDE for brushes if this brushmodel is used by effect_rain
			// FIXME: hardcoded for e1m1a
			// doesn't work because JKA looks for the worldmodel to have CONTENTS_OUTSIDE in some brushes..
			//if (i >= 5 && i <= 49) {
			//	MarkOutside( outsideShaderNum, bsp->brushes, bsp->brushSides, bsp->leafBrushes, bsp->nodes, bsp->leafs, 0, out );
			//}
			int	firstBrush, lastBrush;

			// Quake 2 BSPs reach a model's brushes through its tree, Quake 3 ones
			// need the range. qbsp writes each entity's brushes in one block.
			firstBrush = bsp->numBrushes, lastBrush = 0;
			FindSubmodelBrushRange( bsp->leafBrushes, bsp->nodes, bsp->leafs, headnode, &firstBrush, &lastBrush );

			if (firstBrush <= lastBrush)
			{
				out->firstBrush = firstBrush;
				out->numBrushes = lastBrush + 1 - firstBrush;
			}
			else
			{
				out->firstBrush = 0;
				out->numBrushes = 0;
			}
		}
	}

	if (VIEWFOGS)
	{
		bsp->numFogs = 0;
		bsp->fogs = NULL;
	}
	else
	{
		bsp->numFogs = numFogs;
		bsp->fogs = malloc( numFogs * sizeof( *bsp->fogs ) );
		{
			mfog_t	*in;
			dfog_t	*out;

			in = fogs;
			out = bsp->fogs;
			for ( i = 0; i < numFogs; i++, in++, out++ )
			{
				out->brushNum = in->brushNum;
				Q_strncpyz( out->shader, in->shader, sizeof( out->shader ) );
				out->visibleSide = in->visibleSide;
			}
		}
	}

	DK_BuildLightGrid( bsp, &header, data, faces );

	skybox_t skybox;
	ConvertEntityString( mapname, bsp, data, &header, &skybox, bsp->submodels[0].mins, bsp->submodels[0].maxs );

	OutputTexInfoShaders( mapname, faces, numFaces, texInfo, numTexInfo, &skybox, fogs, numFogs, bsp );

	SkyBoxFree( &skybox );
	free(edges);
	free(surfEdges);
	free(texInfo);
	if (lightingLength > 0)
	{
		free(lightmapData);
	}
	free(markSurfaces);
	free(faces);
	if (visibilityLength > 0)
	{
		free(visibility);
	}
	free(leafs);
	free(leafBrushes);
	free(brushes);
	free(nodes);
	free(verts);
	//free(areas);
	//free(areaPortals);

	image_shutdown();

    return bsp;
}

/****************************************************
*/

bspFormat_t daikatanaBspFormat = {
	"Daikatana",
	IDBSPHEADER,
	BSPVERSION, // TODO: BSPVERSION_COLORLUMP, BSPVERSION_TEXINFOOLD, BSPVERSION_Q2?
	NULL,

	BSP_LoadDK,
};
