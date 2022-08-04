#pragma once

#include "Library\Core\RStrMap.h"
#include "Library\Spatial\GeometryTypes.h"

interface ISimbeyInterchangeFile;
interface IJSONArray;
interface IJSONObject;

class CSIFPackage;

#define	TYPE_EMPTY						0
#define	TYPE_TUNNEL_START				1
#define	TYPE_CONNECTOR					2
#define	TYPE_ANCHOR						3
#define	TYPE_ANY_FLOOR					4
#define	TYPE_ANY_WALL					5
#define	TYPE_ANY_DECORATION				6
#define	TYPE_ANY_DOOR					7
#define	TYPE_ANY_ELEVATOR_DOOR			8
#define	TYPE_ANY_LOCKED_DOOR_GOLD		9
#define	TYPE_ANY_LOCKED_DOOR_SILVER		10
#define	TYPE_ANY_RAILING				11
#define	TYPE_ANY_SWITCH					12

class CWallNamespace
{
private:
	RSTRING m_rstrNamespace;
	ISimbeyInterchangeFile* m_pSIF;
	TRStrMap<sysint> m_mapWalls;

public:
	CWallNamespace (RSTRING rstrNamespace, ISimbeyInterchangeFile* pSIF);
	~CWallNamespace ();

	HRESULT Add (RSTRING rstrTypeW, sysint idxType);
	HRESULT Find (RSTRING rstrTextureW, __out sysint* pidxType);

	RSTRING GetNamespace (VOID) { return m_rstrNamespace; }
	HRESULT GetWalls (__deref_out ISimbeyInterchangeFile** ppSIF, __out TArray<sysint>& aWalls);
};

struct WALL_TEXTURE
{
	DWORD idxLayer;
	FRECT rcPos;
};

struct DOOR_DEF
{
	sysint idxTexture;
	sysint idxTrack;
	bool fLockable;
};

struct ELEVATOR_DEF
{
	sysint idxDown;
	sysint idxUp;
};

class CWallTextures
{
private:
	TRStrMap<CWallNamespace*> m_mapNamespaces;
	TArray<WALL_TEXTURE> m_aWalls;
	UINT m_nTextures;

public:
	TRStrMap<DOOR_DEF> m_mapDoorDefs;
	TRStrMap<ELEVATOR_DEF> m_mapElevatorDefs;
	TRStrMap<sysint> m_mapRailingDefs;

public:
	CWallTextures ();
	~CWallTextures ();

	HRESULT Initialize (VOID);
	HRESULT Load (CSIFPackage* pPackage, BOOL fRequired);
	HRESULT RebuildTextures (VOID);
	VOID BindTexture (VOID);
	HRESULT Resolve (RSTRING rstrTextureW, __out sysint* pidxType);
	HRESULT GetPosition (sysint idxType, __out FRECT* pfrcPos);

private:
	HRESULT LoadTextures (CSIFPackage* pPackage, BOOL fRequired, PCWSTR pcwzDir, INT cchDir, PCWSTR pcwzDefs, INT cchDefs, __deref_out IJSONObject** ppData, __deref_out CWallNamespace** ppNamespace);
	HRESULT AddType (CWallNamespace* pNamespace, sysint idxType, PCWSTR pcwzType, INT cchType, const WALL_TEXTURE& texture);

	HRESULT ParseDoors (CWallNamespace* pNamespace, IJSONArray* pDefs);
	HRESULT ParseSwitches (CWallNamespace* pNamespace, IJSONArray* pDefs);
	HRESULT ParseRailing (CWallNamespace* pNamespace, IJSONArray* pDefs);

	static HRESULT ReadAndResolveName (CWallNamespace* pNamespace, IJSONObject* pDef, PCWSTR pcwzName, __out sysint* pidxType);
	static HRESULT GetNamespaceKey (CWallNamespace* pNamespace, IJSONObject* pDef, __out RSTRING* prstrKeyW);
};
