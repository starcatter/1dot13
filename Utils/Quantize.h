#ifndef __QUANTIZE_H_
#define __QUANTIZE_H_

#include "himage.h"
#include "types.h"

typedef struct _NODE {
	BOOLEAN bIsLeaf;			// TRUE if node has no children
	UINT32 nPixelCount;		// Number of pixels represented by this leaf
	UINT32 nRedSum;			// Sum of red components
	UINT32 nGreenSum;		// Sum of green components
	UINT32 nBlueSum;			// Sum of blue components
	struct _NODE* pChild[8];	// Pointers to child nodes
	struct _NODE* pNext;		// Pointer to next reducible node
} NODE;

class CQuantizer
{
protected:
	NODE* m_pTree;
	UINT32 m_nLeafCount;
	NODE* m_pReducibleNodes[9];
	UINT32 m_nMaxColors;
	UINT32 m_nColorBits;

public:
	CQuantizer (UINT32 nMaxColors, UINT32 nColorBits);
	virtual ~CQuantizer ();
	BOOLEAN ProcessImage (UINT8 *pData, int iWidth, int iHeight );
	UINT32 GetColorCount ();
	void GetColorTable (SGPPaletteEntry* palette);

protected:
	void AddColor (NODE** ppNode, UINT8 r, UINT8 g, UINT8 b, UINT32 nColorBits,
		UINT32 nLevel, UINT32* pLeafCount, NODE** pReducibleNodes);
	NODE* CreateNode (UINT32 nLevel, UINT32 nColorBits, UINT32* pLeafCount,
		NODE** pReducibleNodes);
	void ReduceTree (UINT32 nColorBits, UINT32* pLeafCount,
		NODE** pReducibleNodes);
	void DeleteTree (NODE** ppNode);
	void GetPaletteColors (NODE* pTree, SGPPaletteEntry* palette, UINT32* pIndex);
};

#endif
