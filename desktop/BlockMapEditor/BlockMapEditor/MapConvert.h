#ifndef	_H_CONVERT
#define	_H_CONVERT

interface ISeekableStream;

struct TEXTURE;

class CBlockMap;
class CDoorObject;
class CWallCage;
class CConfigDlg;

#define	LINE_LEFT			1
#define	LINE_RIGHT			2
#define	LINE_ABOVE			3
#define	LINE_BELOW			4

#define	DOOR_UNLOCKED		1
#define	DOOR_SILVER			2
#define	DOOR_GOLD			3
#define	DOOR_RUBY			4
#define	DOOR_SECRET			5
#define	DOOR_ELEVATOR		6
#define	DOOR_CAGE			7

typedef struct
{
	INT xSize, zSize;
	CBlockMap* pMap;
	SHORT yFloor;
} WOLFDATA, *LPWOLFDATA;

// Doom WAD structures

typedef struct
{
	CHAR ID[4];
	LONG cLumps;
	LONG iDir;
} HEADER, *LPHEADER;

typedef struct
{
	LONG iOffset;
	LONG iSize;
	CHAR Name[8];
} DIRECTORY, *LPDIRECTORY;

struct VERTEX
{
	bool operator== (const VERTEX& other) const
	{
		return this->x == other.x && this->y == other.y;
	}

	SHORT x;
	SHORT y;
};

typedef VERTEX* LPVERTEX;

typedef struct
{
	SHORT xOffset;
	SHORT yOffset;
	CHAR szUpper[8];
	CHAR szLower[8];
	CHAR szMiddle[8];
	SHORT Sector;
} SIDEDEF, *LPSIDEDEF;

typedef struct
{
	SHORT Floor;
	SHORT Ceiling;
	CHAR szFloor[8];
	CHAR szCeiling[8];
	SHORT Light;
	SHORT Special;
	SHORT Tag;
} SECTOR, *LPSECTOR;

// Format Independent Classes (03/12/2006)

class CMapThing;
class CMapLinedef;
class CMapLine;
class CMapBox;

// ZDoom types

typedef struct
{
	SHORT ID;
	SHORT x;
	SHORT y;
	SHORT z;
	SHORT Angle;
	SHORT Type;
	SHORT Options;
	BYTE Special;
	BYTE Args[5];
} THING, *LPTHING;

typedef struct
{
	SHORT vFrom;
	SHORT vTo;
	SHORT Flags;
	BYTE Special;
	BYTE Args[5];
	SHORT sRight;
	SHORT sLeft;
} LINEDEF, *LPLINEDEF;

typedef struct
{
	VERTEX vFrom;
	VERTEX vTo;
	SHORT Flags;
	BYTE Special;
	BYTE Args[5];
	SIDEDEF sRight;
	SIDEDEF sLeft;
} MAPLINE, *LPMAPLINE;

typedef struct tagPOLY_QUEUE
{
	MAPLINE Door1;
	MAPLINE Door2;
	INT iType;
	INT bDir;
	INT iPoly;
	const TEXTURE* pTexture;
	const TEXTURE* pAltTexture;
	struct tagPOLY_QUEUE* Next;
} POLY_QUEUE, *LPPOLY_QUEUE;

// Linked lists

typedef struct tagLINE_LIST
{
	CMapLine* lpLine;
	struct tagLINE_LIST* Next;
} LINE_LIST, *LPLINE_LIST;

typedef struct tagSECTOR_LIST
{
	SECTOR Sector;
	struct tagSECTOR_LIST* Next;
} SECTOR_LIST, *LPSECTOR_LIST;

typedef struct tagVERTEX_LIST
{
	VERTEX Vertex;
	struct tagVERTEX_LIST* Next;
} VERTEX_LIST, *LPVERTEX_LIST;

typedef struct tagTHING_LIST
{
	CMapThing* lpThing;
	struct tagTHING_LIST* Next;
} THING_LIST, *LPTHING_LIST;

interface IMapConvertProgress
{
	virtual VOID ReportAddLine (CMapLine* pLine) = 0;
	virtual VOID ReportMergeDone (CMapLine* pLine) = 0;
	virtual VOID ReportAddThing (CMapThing* pThing) = 0;
};

class CMapConvert
{
private:
	IMapConvertProgress* m_pProgress;
	USHORT* m_lpSectorTable;

	LPLINE_LIST m_lpList;
	LPSECTOR_LIST m_lpSectors;
	LPVERTEX_LIST m_lpVertices;
	LPTHING_LIST m_lpThings;

	SHORT m_iNextSector;
	USHORT* m_lpAssign;

	CHAR m_szCeiling[16];
	CHAR m_szFloor[16];

	// ZDoom specific
	SHORT m_iVertStrip;
	SHORT m_iHorzStrip;
	INT m_xPolyStart;
	INT m_yPolyStart;
	INT m_xNextDoor;
	INT m_yNextDoor;
	INT m_iNextPoly;
	SHORT m_sHorizontal;
	SHORT m_sVertical;
	SHORT m_xSpan, m_ySpan;
	LPPOLY_QUEUE m_lpQueue;

	INT m_iLightLevel;

	// Guards/Monsters can open doors (ZDoom)
	BOOL m_bUseAutoDoors;

public:
	CMapConvert ();
	~CMapConvert ();

	VOID ResetConversion (VOID);

	HRESULT RunConversion (IMapConvertProgress* pProgress, PCSTR pcszLevel, ISeekableStream* pFile, LPWOLFDATA lpMap, CConfigDlg* pdlgConfig);

	SHORT GetNewSector (VOID);

	CMapLine* FindLine (LPVERTEX lpvFrom, LPVERTEX lpvTo);
	VOID RemoveLine (CMapLine* pLine);
	LPLINE_LIST FindNext (LPVERTEX lpvFrom, LPLINE_LIST* lplpPrev);
	USHORT FindVertex (LPVERTEX lpVertex);
	USHORT FindSector (WOLFDATA* pData, INT x, INT y);
	SECTOR* FindSector (USHORT iSector);

	VOID PositionLine (WOLFDATA* pData, CMapLine* lpLine, INT x, INT y, INT iDir);
	VOID PositionThing (WOLFDATA* pData, CMapThing* lpThing, INT x, INT y);
	VOID FlipLinedef (CMapLine* lpLine, BOOL bAll = TRUE);
	VOID FillSector (LPSECTOR lpSector, SHORT yFloor);

	HRESULT AddLine (CMapLine* lpLine);
	HRESULT AddImpassible (LPWOLFDATA lpMap, CMapLine* lpLine, INT x, INT y);
	VOID AddSidedLine (CMapLine* lpLine, USHORT iSector, PCSTR pcszTexture);
	SHORT AddSector (LPSECTOR lpSector);
	VOID AddThing (CMapThing* lpThing);

	VOID GetMapSize (__out SHORT& x, __out SHORT& y);

protected:
	static VOID CopyTexture (PSTR pszTarget, PCWSTR pcwzTexture);
	static VOID RotatePoints (VERTEX* prgvPoints, INT cPoints);

	VOID DrawBox (INT x1, INT y1, INT x2, INT y2, SHORT nSector, BOOL fOuterFacing = FALSE);
	VOID PrepareBox (CMapBox* pBox, INT x1, INT y1, INT x2, INT y2, SHORT nSector, BOOL fOuterFacing = FALSE);
	VOID CommitBox (CMapBox* pBox);

	HRESULT BuildLineStructure (LPWOLFDATA lpMap);
	HRESULT BuildCages (LPWOLFDATA lpMap);
	HRESULT BuildSecretDoors (LPWOLFDATA lpMap);
	HRESULT EndSpotTriggers (LPWOLFDATA lpMap);
	HRESULT BuildSkyLights (LPWOLFDATA lpMap);
	HRESULT AssignEndSpotTrigger (LPWOLFDATA lpMap, BOOL* pfEndMap, INT x, INT y, SHORT nSector);
	HRESULT AssignSkyLightTrigger (LPWOLFDATA lpMap, BOOL* pfSkyMap, INT x, INT y, SHORT nSector);
	VOID BuildDoor (LPWOLFDATA lpMap, INT x, INT y, INT iType, INT bDir, INT bSilence, CDoorObject* pDoor);
	VOID BuildPolyDoor (LPWOLFDATA lpMap, CMapLine* lpDoor1, CMapLine* lpDoor2, INT iType, INT bDir, const TEXTURE* pTexture1, const TEXTURE* pTexture2, INT iPoly);
	VOID BuildSectors (LPWOLFDATA lpMap);
	VOID BuildOutsideViews (LPWOLFDATA lpMap);
	VOID BuildPlayerStarts (LPWOLFDATA lpMap);
	VOID BuildThings (LPWOLFDATA lpMap);

	BOOL AssignSector (LPWOLFDATA lpMap, SHORT nSector, INT x, INT y);
	VOID MergeDone (CMapLine* lpLine);
	VOID MergeLines (VOID);
	VOID BuildPolys (LPWOLFDATA lpMap);
	VOID FixSidedefs (VOID);
	VOID BuildVertexList (VOID);
	VOID CalculateMapSize (LPWOLFDATA lpMap);

	VOID ExtendOutside (SECTOR* pAdjacent, CMapLine* lpLine, INT iDir, PCWSTR pcwzTexture);
	HRESULT BuildCage (LPWOLFDATA lpMap, INT x, INT y, INT nLine, CMapLine* pLine, CWallCage* pCage);
	HRESULT BuildWadFile (PCSTR pcszMapName, ISeekableStream* pFile, CConfigDlg* pdlgConfig);

	ULONG SerializeThings (ISeekableStream* pFile);
	ULONG SerializeLinedefs (ISeekableStream* pFile);
	ULONG SerializeSidedefs (ISeekableStream* pFile);
	ULONG SerializeVertices (ISeekableStream* pFile);
	ULONG SerializeSectors (ISeekableStream* pFile);
	ULONG SerializeLump (ISeekableStream* pFile, PCWSTR pcwzPath, LPSTR lpszFile);

	static VOID CompareVertexForMapSize (const VERTEX& v, SHORT& xMin, SHORT& yMin, SHORT& xMax, SHORT& yMax);
};

#endif
