////////////////////////////////////////////////////////////
// X88000 Printer CG character polygon data
//
// Extracted from X88PrinterDrawer.cpp so that code that does not
// pull in the GUI-dependent drawing backends can still access the
// CG character shape data.
//
// Written by Manuke

#include "StdHeader.h"

#include "X88PrinterDrawer.h"

#include "X88Utility.h"

using namespace NX88Utility;

////////////////////////////////////////////////////////////
// create & destroy

CX88PrinterDrawer::CX88PrinterDrawer() {
}

CX88PrinterDrawer::~CX88PrinterDrawer() {
}

////////////////////////////////////////////////////////////
// operation

uint16_t CX88PrinterDrawer::GetCGCharacterData(
	uint16_t wText,
	int anPoints[64][2],
	int anPointCounts[16],
	int& nPolygonCount, int& nTotal)
{
	nPolygonCount = 0, nTotal = 0;
	int nTotalPrev = 0;
	uint16_t wChar = 0;
	switch (wText) {
	case 0x20:
		SetPoint(anPoints[nTotal++], 0, 22);
		SetPoint(anPoints[nTotal++], 16, 22);
		SetPoint(anPoints[nTotal++], 16, 24);
		SetPoint(anPoints[nTotal++], 0, 24);
		anPointCounts[nPolygonCount++] = nTotal-nTotalPrev;
		break;
	case 0x21:
	case 0x22:
	case 0x23:
	case 0x24:
	case 0x25:
	case 0x26:
	case 0x27:
		SetPoint(anPoints[nTotal++], 0, 18-(wText-0x21)*3);
		SetPoint(anPoints[nTotal++], 16, 18-(wText-0x21)*3);
		SetPoint(anPoints[nTotal++], 16, 24);
		SetPoint(anPoints[nTotal++], 0, 24);
		anPointCounts[nPolygonCount++] = nTotal-nTotalPrev;
		break;
	case 0x28:
	case 0x29:
	case 0x2A:
	case 0x2B:
	case 0x2C:
	case 0x2D:
	case 0x2E:
		SetPoint(anPoints[nTotal++], 0, 0);
		SetPoint(anPoints[nTotal++], 2+(wText-0x28)*2, 0);
		SetPoint(anPoints[nTotal++], 2+(wText-0x28)*2, 24);
		SetPoint(anPoints[nTotal++], 0, 24);
		anPointCounts[nPolygonCount++] = nTotal-nTotalPrev;
		break;
	case 0x2F:
	case 0x30:
	case 0x31:
	case 0x32:
	case 0x33:
	case 0x38:
	case 0x39:
	case 0x3A:
	case 0x3B:
		{ // dummy block
			uint8_t btFlags = 0;
			switch (wText) {
			case 0x2F:
				btFlags = 0x0F;
				break;
			case 0x30:
				btFlags = 0x0B;
				break;
			case 0x31:
				btFlags = 0x0E;
				break;
			case 0x32:
				btFlags = 0x0D;
				break;
			case 0x33:
				btFlags = 0x07;
				break;
			case 0x38:
				btFlags = 0x06;
				break;
			case 0x39:
				btFlags = 0x0C;
				break;
			case 0x3A:
				btFlags = 0x03;
				break;
			case 0x3B:
				btFlags = 0x09;
				break;
			}
			SetPoint(anPoints[nTotal++], 7, 11);
			if ((btFlags & 0x01) != 0) {
				SetPoint(anPoints[nTotal++], 7, 0);
				SetPoint(anPoints[nTotal++], 9, 0);
			}
			SetPoint(anPoints[nTotal++], 9, 11);
			if ((btFlags & 0x02) != 0) {
				SetPoint(anPoints[nTotal++], 16, 11);
				SetPoint(anPoints[nTotal++], 16, 13);
			}
			SetPoint(anPoints[nTotal++], 9, 13);
			if ((btFlags & 0x04) != 0) {
				SetPoint(anPoints[nTotal++], 9, 24);
				SetPoint(anPoints[nTotal++], 7, 24);
			}
			SetPoint(anPoints[nTotal++], 7, 13);
			if ((btFlags & 0x08) != 0) {
				SetPoint(anPoints[nTotal++], 0, 13);
				SetPoint(anPoints[nTotal++], 0, 11);
			}
			anPointCounts[nPolygonCount++] = nTotal-nTotalPrev;
		}
		break;
	case 0x34:
	case 0x35:
		SetPoint(anPoints[nTotal++], 0, (wText-0x34)*11);
		SetPoint(anPoints[nTotal++], 16, (wText-0x34)*11);
		SetPoint(anPoints[nTotal++], 16, 2+(wText-0x34)*11);
		SetPoint(anPoints[nTotal++], 0, 2+(wText-0x34)*11);
		anPointCounts[nPolygonCount++] = nTotal-nTotalPrev;
		break;
	case 0x36:
	case 0x37:
		SetPoint(anPoints[nTotal++], 7+(wText-0x36)*7, 0);
		SetPoint(anPoints[nTotal++], 9+(wText-0x36)*7, 0);
		SetPoint(anPoints[nTotal++], 9+(wText-0x36)*7, 24);
		SetPoint(anPoints[nTotal++], 7+(wText-0x36)*7, 24);
		anPointCounts[nPolygonCount++] = nTotal-nTotalPrev;
		break;
	case 0x3C:
	case 0x3D:
	case 0x3E:
	case 0x3F:
		{ // dummy block
			SetPoint(anPoints[nTotal++], 9, 0);
			SetPoint(anPoints[nTotal++], 8, 5);
			SetPoint(anPoints[nTotal++], 6, 9);
			SetPoint(anPoints[nTotal++], 3, 12);
			SetPoint(anPoints[nTotal++], 0, 13);
			SetPoint(anPoints[nTotal++], 0, 11);
			SetPoint(anPoints[nTotal++], 3, 10);
			SetPoint(anPoints[nTotal++], 5, 7);
			SetPoint(anPoints[nTotal++], 6, 5);
			SetPoint(anPoints[nTotal++], 7, 0);
			anPointCounts[nPolygonCount++] = nTotal-nTotalPrev;
			for (int i = 0; i < nTotal; i++) {
				if ((wText == 0x3C) || (wText == 0x3E)) {
					anPoints[i][0] = 16-anPoints[i][0];
				}
				if ((wText == 0x3C) || (wText == 0x3D)) {
					anPoints[i][1] = 24-anPoints[i][1];
				}
			}
		}
		break;
	case 0x40:
		SetPoint(anPoints[nTotal++], 0, 7);
		SetPoint(anPoints[nTotal++], 16, 7);
		SetPoint(anPoints[nTotal++], 16, 9);
		SetPoint(anPoints[nTotal++], 0, 9);
		anPointCounts[nPolygonCount++] = nTotal-nTotalPrev;
		nTotalPrev = nTotal;
		SetPoint(anPoints[nTotal++], 0, 15);
		SetPoint(anPoints[nTotal++], 16, 15);
		SetPoint(anPoints[nTotal++], 16, 17);
		SetPoint(anPoints[nTotal++], 0, 17);
		anPointCounts[nPolygonCount++] = nTotal-nTotalPrev;
		break;
	case 0x41:
	case 0x42:
	case 0x43:
		{ // dummy block
			uint8_t btFlags = 0;
			switch (wText) {
			case 0x41:
				btFlags = 0x01;
				break;
			case 0x42:
				btFlags = 0x03;
				break;
			case 0x43:
				btFlags = 0x02;
				break;
			}
			SetPoint(anPoints[nTotal++], 7, 7);
			SetPoint(anPoints[nTotal++], 7, 0);
			SetPoint(anPoints[nTotal++], 9, 0);
			SetPoint(anPoints[nTotal++], 9, 7);
			if ((btFlags & 0x01) != 0) {
				SetPoint(anPoints[nTotal++], 16, 7);
				SetPoint(anPoints[nTotal++], 16, 9);
				SetPoint(anPoints[nTotal++], 9, 9);
				SetPoint(anPoints[nTotal++], 9, 15);
				SetPoint(anPoints[nTotal++], 16, 15);
				SetPoint(anPoints[nTotal++], 16, 17);
			}
			SetPoint(anPoints[nTotal++], 9, 17);
			SetPoint(anPoints[nTotal++], 9, 24);
			SetPoint(anPoints[nTotal++], 7, 24);
			SetPoint(anPoints[nTotal++], 7, 17);
			if ((btFlags & 0x02) != 0) {
				SetPoint(anPoints[nTotal++], 0, 17);
				SetPoint(anPoints[nTotal++], 0, 15);
				SetPoint(anPoints[nTotal++], 7, 15);
				SetPoint(anPoints[nTotal++], 7, 9);
				SetPoint(anPoints[nTotal++], 0, 9);
				SetPoint(anPoints[nTotal++], 0, 7);
			}
			anPointCounts[nPolygonCount++] = nTotal-nTotalPrev;
		}
		break;
	case 0x44:
		SetPoint(anPoints[nTotal++], 16, 0);
		SetPoint(anPoints[nTotal++], 16, 24);
		SetPoint(anPoints[nTotal++], 0, 24);
		anPointCounts[nPolygonCount++] = nTotal-nTotalPrev;
		break;
	case 0x45:
		SetPoint(anPoints[nTotal++], 0, 0);
		SetPoint(anPoints[nTotal++], 16, 24);
		SetPoint(anPoints[nTotal++], 0, 24);
		anPointCounts[nPolygonCount++] = nTotal-nTotalPrev;
		break;
	case 0x46:
		SetPoint(anPoints[nTotal++], 0, 0);
		SetPoint(anPoints[nTotal++], 16, 0);
		SetPoint(anPoints[nTotal++], 16, 24);
		anPointCounts[nPolygonCount++] = nTotal-nTotalPrev;
		break;
	case 0x47:
		SetPoint(anPoints[nTotal++], 0, 0);
		SetPoint(anPoints[nTotal++], 16, 0);
		SetPoint(anPoints[nTotal++], 0, 24);
		anPointCounts[nPolygonCount++] = nTotal-nTotalPrev;
		break;
	case 0x4E:
	case 0x4F:
	case 0x50:
		if (wText != 0x4F) {
			SetPoint(anPoints[nTotal++], 16, 0);
			SetPoint(anPoints[nTotal++], 16, 1);
			SetPoint(anPoints[nTotal++], 1, 24);
			SetPoint(anPoints[nTotal++], 0, 24);
			SetPoint(anPoints[nTotal++], 0, 23);
			SetPoint(anPoints[nTotal++], 15, 0);
			anPointCounts[nPolygonCount++] = nTotal-nTotalPrev;
			nTotalPrev = nTotal;
		}
		if (wText != 0x4E) {
			SetPoint(anPoints[nTotal++], 0, 0);
			SetPoint(anPoints[nTotal++], 1, 0);
			SetPoint(anPoints[nTotal++], 16, 23);
			SetPoint(anPoints[nTotal++], 16, 24);
			SetPoint(anPoints[nTotal++], 15, 24);
			SetPoint(anPoints[nTotal++], 0, 1);
			anPointCounts[nPolygonCount++] = nTotal-nTotalPrev;
		}
		break;
	case 0x48:
	case 0x49:
	case 0x4A:
	case 0x4B:
		{ // dummy block
			switch (wText) {
			case 0x48:
				SetPoint(anPoints[nTotal++], 8, 3);
				SetPoint(anPoints[nTotal++], 15, 10);
				SetPoint(anPoints[nTotal++], 16, 12);
				SetPoint(anPoints[nTotal++], 16, 15);
				SetPoint(anPoints[nTotal++], 15, 17);
				SetPoint(anPoints[nTotal++], 14, 18);
				SetPoint(anPoints[nTotal++], 12, 18);
				SetPoint(anPoints[nTotal++], 9, 17);
				SetPoint(anPoints[nTotal++], 11, 21);
				break;
			case 0x49:
				SetPoint(anPoints[nTotal++], 8, 5);
				SetPoint(anPoints[nTotal++], 9, 4);
				SetPoint(anPoints[nTotal++], 11, 3);
				SetPoint(anPoints[nTotal++], 14, 4);
				SetPoint(anPoints[nTotal++], 15, 5);
				SetPoint(anPoints[nTotal++], 16, 7);
				SetPoint(anPoints[nTotal++], 16, 11);
				SetPoint(anPoints[nTotal++], 15, 14);
				SetPoint(anPoints[nTotal++], 8, 21);
				break;
			case 0x4A:
				SetPoint(anPoints[nTotal++], 8, 3);
				SetPoint(anPoints[nTotal++], 16, 12);
				SetPoint(anPoints[nTotal++], 8, 21);
				break;
			case 0x4B:
				SetPoint(anPoints[nTotal++], 8, 3);
				SetPoint(anPoints[nTotal++], 10, 4);
				SetPoint(anPoints[nTotal++], 12, 6);
				SetPoint(anPoints[nTotal++], 12, 8);
				SetPoint(anPoints[nTotal++], 11, 11);
				SetPoint(anPoints[nTotal++], 13, 10);
				SetPoint(anPoints[nTotal++], 15, 11);
				SetPoint(anPoints[nTotal++], 16, 14);
				SetPoint(anPoints[nTotal++], 15, 17);
				SetPoint(anPoints[nTotal++], 13, 18);
				SetPoint(anPoints[nTotal++], 11, 18);
				SetPoint(anPoints[nTotal++], 9, 17);
				SetPoint(anPoints[nTotal++], 11, 21);
				break;
			}
			int i = nTotal-1;
			if (( i >= 0) && (anPoints[i][0] > 8)) {
				SetPoint(
					anPoints[nTotal++],
					16-anPoints[i][0], anPoints[i][1]);
			}
			for (; i >= 0; i--) {
				SetPoint(
					anPoints[nTotal++],
					16-anPoints[i][0], anPoints[i][1]);
			}
			anPointCounts[nPolygonCount++] = nTotal-nTotalPrev;
		}
		break;
	case 0x4C: // filled circle-mark
		wChar = 0x217C;
		break;
	case 0x4D: // unfilled circle-mark
		wChar = 0x217B;
		break;
	case 0x51: // yen-mark
		wChar = 0x315F;
		break;
	case 0x52: // year-kanji
		wChar = 0x472F;
		break;
	case 0x53: // month-kanji
		wChar = 0x376E;
		break;
	case 0x54: // day-kanji
		wChar = 0x467C;
		break;
	case 0x55: // hour-kanji
		wChar = 0x3B7E;
		break;
	case 0x56: // minute-kanji
		wChar = 0x4A2C;
		break;
	case 0x57: // second-kanji
		wChar = 0x4943;
		break;
	}
	return wChar;
}

////////////////////////////////////////////////////////////
// extract text
//     bCRLF
//         true  : line-break = CR+LF
//         false : line-break = LF

void CX88PrinterDrawer::ExtractText(
	const CParallelPrinter* pPrinter,
	int nPage,
	std::string& jstrText,
	bool bCRLF)
{
	jstrText = "";
	if (nPage >= (int)pPrinter->size()) {
		return;
	}
	CParallelPrinter::const_iterator itPage = pPrinter->begin();
	std::advance(itPage, nPage);
	bool bPageTop = true, bLineTop = true;
	int yPrev = 0;
	for (
		CPrinterPage::const_iterator itObject = (*itPage)->begin();
		true;
		itObject++)
	{
		bool bReturn = false;
		if (itObject == (*itPage)->end()) {
			if (!bLineTop) {
				bReturn = true;
			}
		} else if (
			(*itObject)->GetObjectType() == CPrinterObject::POBJ_TEXT)
		{
			if (!bPageTop && ((*itObject)->GetY() != yPrev)) {
				bReturn = true;
			}
		}
		if (bReturn) {
			if (bCRLF) {
				jstrText += "\r\n";
			} else {
				jstrText += '\n';
			}
			bPageTop = false;
			bLineTop = true;
		}
		if (itObject == (*itPage)->end()) {
			break;
		} else if (
			(*itObject)->GetObjectType() == CPrinterObject::POBJ_TEXT)
		{
			const CPrinterTextObject* pobjText =
				(const CPrinterTextObject*)(*itObject);
			yPrev = pobjText->GetY();
			int nLength = pobjText->GetTextLength();
			for (int i = 0; i < nLength; i++) {
				uint16_t wText = pobjText->GetText()[i],
					wChar = 0x20;
				switch (pobjText->GetCharType(i)) {
				case CPrinterTextObject::CHAR_NULL:
					wChar = 0;
					break;
				case CPrinterTextObject::CHAR_ANK:
					if (((wText >= 0x20) && (wText <= 0x7E)) ||
						((wText >= 0xA1) && (wText <= 0xDF)))
					{
						wChar = wText;
					}
					break;
				case CPrinterTextObject::CHAR_ASCII:
					if ((wText >= 0x20) && (wText <= 0x7E)) {
						wChar = wText;
					}
					break;
				case CPrinterTextObject::CHAR_KATAKANA:
				case CPrinterTextObject::CHAR_HIRAGANA:
					if ((wText >= 0x21) && (wText <= 0x5F)) {
						wChar = (uint16_t)(wText+0x80);
					}
					break;
				case CPrinterTextObject::CHAR_CG:
					break;
				case CPrinterTextObject::CHAR_KANJI:
					wChar = (uint16_t)JIS2SJIS(wText);
					break;
				case CPrinterTextObject::CHAR_GAIJI:
					if (((wText >= 0x20) && (wText <= 0x7E)) ||
						((wText >= 0xA1) && (wText <= 0xDF)))
					{
						wChar = wText;
					} else if (wText > 0xFF) {
						wChar = (uint16_t)JIS2SJIS(0x2121);
					}
					break;
				}
				if (wChar != 0) {
					if (wChar > 0xFF) {
						jstrText += (char)(wChar >> 8);
						jstrText += (char)(wChar & 0xFF);
					} else {
						jstrText += (char)wChar;
					}
				}
			}
			bPageTop = false;
			bLineTop = false;
		}
	}
}
