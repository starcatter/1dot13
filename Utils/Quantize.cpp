#include "Quantize.h"

#include <new>

CQuantizer::CQuantizer (UINT32 nMaxColors, UINT32 nColorBits)
{
	m_pTree = NULL;
	m_nLeafCount = 0;
	for (int i=0; i<=(int) nColorBits; i++)
		m_pReducibleNodes[i] = NULL;
	m_nMaxColors = nMaxColors;
	m_nColorBits = nColorBits;
}

CQuantizer::~CQuantizer ()
{
	if (m_pTree != NULL)
		DeleteTree (&m_pTree);
}

BOOLEAN CQuantizer::ProcessImage (UINT8 *pData, int iWidth, int iHeight )
{
	UINT8* pbBits;
	UINT8 r, g, b;
	int i, j;


	pbBits = pData;
	for (i=0; i<iHeight; i++) {
		for (j=0; j<iWidth; j++) {
			b = *pbBits++;
			g = *pbBits++;
			r = *pbBits++;
			AddColor (&m_pTree, r, g, b, m_nColorBits, 0, &m_nLeafCount,
				m_pReducibleNodes);
			while (m_nLeafCount > m_nMaxColors)
				ReduceTree (m_nColorBits, &m_nLeafCount, m_pReducibleNodes);
		}
				//Padding
		//pbBits ++;
	}
	return TRUE;
}

void CQuantizer::AddColor (NODE** ppNode, UINT8 r, UINT8 g, UINT8 b,
	UINT32 nColorBits, UINT32 nLevel, UINT32* pLeafCount, NODE** pReducibleNodes)
{
	static UINT8 mask[8] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };

	//
	// If the node doesn't exist, create it.
	//
	if (*ppNode == NULL)
		*ppNode = CreateNode (nLevel, nColorBits, pLeafCount,
			pReducibleNodes);

	//
	// Update color information if it's a leaf node.
	//
	if ((*ppNode)->bIsLeaf) {
		(*ppNode)->nPixelCount++;
		(*ppNode)->nRedSum += r;
		(*ppNode)->nGreenSum += g;
		(*ppNode)->nBlueSum += b;
	}

	//
	// Recurse a level deeper if the node is not a leaf.
	//
	else {
		int shift = 7 - nLevel;
		int nIndex = (((r & mask[nLevel]) >> shift) << 2) |
			(((g & mask[nLevel]) >> shift) << 1) |
			((b & mask[nLevel]) >> shift);
		AddColor (&((*ppNode)->pChild[nIndex]), r, g, b, nColorBits,
			nLevel + 1, pLeafCount, pReducibleNodes);
	}
}

NODE* CQuantizer::CreateNode (UINT32 nLevel, UINT32 nColorBits, UINT32* pLeafCount,
	NODE** pReducibleNodes)
{
	NODE* pNode = new (std::nothrow) NODE{};
	if (pNode == NULL)
		return NULL;

	pNode->bIsLeaf = (nLevel == nColorBits) ? TRUE : FALSE;
	if (pNode->bIsLeaf)
		(*pLeafCount)++;
	else {
		pNode->pNext = pReducibleNodes[nLevel];
		pReducibleNodes[nLevel] = pNode;
	}
	return pNode;
}

void CQuantizer::ReduceTree (UINT32 nColorBits, UINT32* pLeafCount,
	NODE** pReducibleNodes)
{
	//
	// Find the deepest level containing at least one reducible node.
	//
	int i = 0;

	for (i=nColorBits - 1; (i>0) && (pReducibleNodes[i] == NULL); i--);

	//
	// Reduce the node most recently added to the list at level i.
	//

	NODE* pNode = pReducibleNodes[i];
	pReducibleNodes[i] = pNode->pNext;

	UINT32 nRedSum = 0;
	UINT32 nGreenSum = 0;
	UINT32 nBlueSum = 0;
	UINT32 nChildren = 0;

	for (i=0; i<8; i++) {
		if (pNode->pChild[i] != NULL) {
			nRedSum += pNode->pChild[i]->nRedSum;
			nGreenSum += pNode->pChild[i]->nGreenSum;
			nBlueSum += pNode->pChild[i]->nBlueSum;
			pNode->nPixelCount += pNode->pChild[i]->nPixelCount;
			delete pNode->pChild[i];
			pNode->pChild[i] = NULL;
			nChildren++;
		}
	}

	pNode->bIsLeaf = TRUE;
	pNode->nRedSum = nRedSum;
	pNode->nGreenSum = nGreenSum;
	pNode->nBlueSum = nBlueSum;
	*pLeafCount -= (nChildren - 1);
}

void CQuantizer::DeleteTree (NODE** ppNode)
{
	for (int i=0; i<8; i++) {
		if ((*ppNode)->pChild[i] != NULL)
			DeleteTree (&((*ppNode)->pChild[i]));
	}
	delete *ppNode;
	*ppNode = NULL;
}

void CQuantizer::GetPaletteColors (NODE* pTree, SGPPaletteEntry* palette, UINT32* pIndex)
{
	if (pTree->bIsLeaf) {
		palette[*pIndex].peRed =
			(UINT8) ((pTree->nRedSum) / (pTree->nPixelCount));
		palette[*pIndex].peGreen =
			(UINT8) ((pTree->nGreenSum) / (pTree->nPixelCount));
		palette[*pIndex].peBlue =
			(UINT8) ((pTree->nBlueSum) / (pTree->nPixelCount));
		palette[*pIndex].peFlags = 0;
		(*pIndex)++;
	}
	else {
		for (int i=0; i<8; i++) {
			if (pTree->pChild[i] != NULL)
				GetPaletteColors (pTree->pChild[i], palette, pIndex);
		}
	}
}

UINT32 CQuantizer::GetColorCount ()
{
	return m_nLeafCount;
}

void CQuantizer::GetColorTable (SGPPaletteEntry* palette)
{
	UINT32 nIndex = 0;
	GetPaletteColors (m_pTree, palette, &nIndex);
}


