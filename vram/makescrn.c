#include	"compiler.h"
#include	"scrnmng.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"vram.h"
#include	"scrndraw.h"
#include	"palettes.h"
#include	"makescrn.h"
#include	"makesub.h"


		MAKESCRN	makescrn;
		BYTE		updatetmp[0x800+0x101];

		BYTE	scrnallflash;
		BYTE	blinktest = 0;

extern	BYTE	dispmode;

static	BYTE	lastdisp = 0;
		BYTE	dispflg = UPDATE_VRAM0;
		BYTE	*dispp = &GRP_RAM[GRAM_BANK0];
		BYTE	*dispp2 = &GRP_RAM[GRAM_BANK1];
static	BYTE	blinktime = 1;

		DWORD	drawtime = 0;


void init_draw(void) {

	makesub_initialize();
	reflesh_palette();
	lastdisp = 0;
	dispflg = UPDATE_VRAM0;
	dispp = &GRP_RAM[GRAM_BANK0];
}


void fillupdatetmp(void) {

		__asm {
				mov		ebx, offset updatetmp
				mov		ecx, 0x200
filludt_lp:		or		dword ptr [ebx], (UPDATE_TVRAM * 01010101h)
				add		ebx, 4
				loop	filludt_lp
		}
}

static LABEL void flashupdatetmp(void) {

		__asm {
				push	ebx
				push	esi
				push	edi

				movzx	esi, crtc.s.TXT_TOP
				xor		dl, dl
										// �܂��s���ׂĂ��c�{�p�����ׂ�
check_tateflag:	mov		edi, esi
				movzx	ecx, crtc.s.TXT_XL
tateflaglp_s:	and		edi, (TRAM_MAX - 1)
				test	tram[TRAM_ATR + edi], X1ATR_Yx2
				je		tatex1
				inc		edi
				loop	tateflaglp_s
										// ���ׂďc�{�p ��������

				movzx	ecx, crtc.s.TXT_XL
				shr		cl, 1

tatex2loop_s:	and		esi, (TRAM_MAX - 1)
				mov		ax, (UPDATE_TVRAM or 04h) * 0101h		// �c�{�p
				mov		bx, word ptr (tram[TRAM_ATR + esi])

				test	bl, X1ATR_Xx2
				je		tatex2_norleft
												// �{�p�t���O�������Ă�
				or		ax, 08h + 12h * 256		// ���S�{�p + �E�c�{�p�͊m��
				test	bh, X1ATR_Xx2
				je		tatex2_pcg
				or		ah, 8					// �E�����{�p�r�b�g�������Ă�
				jmp		tatex2_pcg

tatex2_norleft:	test	bh, X1ATR_Xx2			// �S�{�p�łȂ��ꍇ
				je		tatex2_pcg
				or		ah, 8					// �E���̂ݔ{�p������
tatex2_pcg:		mov		bx, word ptr (updatetmp[esi])
				and		bx, UPDATE_TVRAM * 0101h
				or		ax, bx					// UPDATE�t���O��������
				cmp		word ptr (updatetmp[esi]), ax
				je		tatex2noupdate
				or		ax, UPDATE_TVRAM * 0101h
				mov		word ptr (updatetmp[esi]), ax
tatex2noupdate:	add		esi, 2
				loop	tatex2loop_s
				jmp		nextlinecheck

tatex1:			movzx	ecx, crtc.s.TXT_XL
				shr		cl, 1
				xor		dh, dh
tatex1loop_s:	and		esi, (TRAM_MAX - 1)
				xor		ax, ax
				mov		bx, word ptr (tram[TRAM_ATR + esi])
				or		dh, dh					// �c�m�[�}���͊��ɂ��������H
				jne		tatex1_tate_e
				test	bl, X1ATR_Yx2
				je		tatex1_tate0
				mov		al, UPDATE_TVRAM or 1	// ���ׂ�c�{�p
				test	bh, X1ATR_Yx2
				je		tatex1_tate0
				mov		ah, UPDATE_TVRAM or 1	// �E�ׂ�c�{�p
				jmp		tatex1_tate_e
tatex1_tate0:	inc		dh						// �c�m�[�}���̏o��
tatex1_tate_e:
				test	bl, X1ATR_Xx2
				je		tatex1_norleft
												// �{�p�t���O�������Ă�
				or		ax, 08h + (12h * 256)	// �����{�p �͊m��
				test	bh, X1ATR_Xx2
				je		tatex1_pcg
				or		ah, 8					// �E�����{�p�r�b�g�������Ă�
				jmp		tatex1_pcg

tatex1_norleft:	test	bh, X1ATR_Xx2			// �{�p�łȂ��ꍇ
				je		tatex1_pcg
				or		ah, 8					// �E���̂ݔ{�p������
tatex1_pcg:		mov		bx, word ptr (updatetmp[esi])
				and		bx, UPDATE_TVRAM * 0101h
				or		ax, bx
				cmp		word ptr (updatetmp[esi]), ax
				je		tatex1noupdate
				or		ax, UPDATE_TVRAM * 0101h
				mov		word ptr (updatetmp[esi]), ax
tatex1noupdate:	add		esi, 2
				loop	tatex1loop_s

nextlinecheck:	inc		dl
				cmp		dl, crtc.s.TXT_YL
				jb		check_tateflag

				pop		edi
				pop		esi
				pop		ebx
				ret
	}
}

void updateblink(void) {

		__asm {
				dec		blinktime
				jns		blinkupflg_ed
				mov		blinktime, 30-1
				push	edi
				movzx	edi, crtc.s.TXT_TOP
				mov		ax, 010h
				xor		blinktest, al
				mov		cx, word ptr (makescrn.vramsize)
				or		cx, cx
				je		blinkupflg

blinkup_lp:		test	tram[TRAM_ATR + edi], al
				je		short blinknup_ed
				mov		ah, UPDATE_TVRAM
				or		updatetmp[edi], ah
blinknup_ed:	inc		edi
				dec		cx
				jne		blinkup_lp
				or		makescrn.scrnflash, ah
				shr		ah, 5
blinkupflg:		pop		edi
				mov		makescrn.existblink, ah
blinkupflg_ed:
		}
}


// ---------------------------------------------------------------------------

static void x1vram_adjust(void) {

	REG8	widthmode;
	UINT	fontcy;
	UINT	underlines;
	REG8	y2;
	UINT	charcy;
	UINT	surfcy;

	if (crtc.s.TXT_XL <= 40) {
		if (dispmode & SCRN_DRAW4096) {
			widthmode = SCRNWIDTHMODE_4096;
		}
		else {
			widthmode = SCRNWIDTHMODE_WIDTH40;
		}
	}
	else {
		widthmode = SCRNWIDTHMODE_WIDTH80;
	}
	scrndraw_changewidth(widthmode);

	makescrn.surfcx = min(80, crtc.s.TXT_XL);
	makescrn.surfrx = crtc.s.TXT_XL - makescrn.surfcx;

	fontcy = crtc.s.FNT_YL;
	if (crtc.s.SCRN_BITS & SCRN_24KHZ) {
		fontcy >>= 1;
	}
	underlines = (crtc.s.SCRN_BITS & SCRN_UNDERLINE)?2:0;
	if (fontcy > underlines) {
		fontcy -= underlines;
	}
	else {
		fontcy = 0;
	}
	y2 = (crtc.s.SCRN_BITS & SCRN_TEXTYx2)?1:0;
	fontcy >>= y2;
#if 0
	if (((dispmode & SCRN64_MASK) != SCRN64_INVALID) && (fontcy > 8)) {
		fontcy = 8;
	}
#endif
	if (!fontcy) {
		fontcy = 1;
	}
	if (fontcy > 8) {
		fontcy = 8;
	}
	charcy = fontcy + underlines;
	makescrn.fontcy = fontcy;
	makescrn.charcy = charcy;

	charcy <<= y2;
	surfcy = 200 / charcy;
	if (surfcy > crtc.s.TXT_YL) {
		surfcy = crtc.s.TXT_YL;
	}
	makescrn.surfcy = surfcy;
	makescrn.surfstep = (SURFACE_WIDTH * charcy * 2) - (makescrn.surfcx * 8);
	makescrn.vramsize = min(0x800, charcy * crtc.s.TXT_XL);
	scrnmng_setheight(0, charcy * surfcy * 2);
}


// -------------------------------------------------------------------------

typedef void (*DRAWFN)(void);

static const DRAWFN screendraw[8] = {
				width80x25_200l,	width80x25_400h,
				width80x25_200l,	width80x25_200h,
				width80x12_200l,	width80x12_400h,
				width80x12_200l,	width80x12_200h};

static const DRAWFN screendraw2[8] = {
				width80x20l,		width80x20h,
				width80x20l,		width80x20h,
				width80x10l,		width80x10h,
				width80x10l,		width80x10h};


void scrnupdate(void) {

	BRESULT	ddrawflash;

	if (!corestat.drawframe) {
		return;
	}
	corestat.drawframe = 0;

	ddrawflash = FALSE;
	if (lastdisp != dispmode) {
		lastdisp = dispmode;
		scrnallflash = 1;
		makescrn.palandply = 1;
		if (!(dispmode & SCRN_BANK1)) {
			dispp = GRP_RAM + GRAM_BANK0;
			dispp2 = GRP_RAM + GRAM_BANK1;
			dispflg = UPDATE_VRAM0;
		}
		else {
			dispp = GRP_RAM + GRAM_BANK1;
			dispp2 = GRP_RAM + GRAM_BANK0;
			dispflg = UPDATE_VRAM1;
		}
	}
	if (scrnallflash) {
		scrnallflash = 0;
		fillupdatetmp();
		x1vram_adjust();
		makescrn.scrnflash = 1;
	}
	if (makescrn.remakeattr) {
		makescrn.remakeattr = 0;
		flashupdatetmp();
	}
	if (makescrn.palandply) {
		makescrn.palandply = 0;
		palettes();
		ddrawflash = 1;
	}
	if (makescrn.existblink) {
		updateblink();
	}

	if (makescrn.scrnflash) {
		makescrn.scrnflash = 0;
		makescrn.fontycnt = 0;
		switch(lastdisp & SCRN64_MASK) {
			case SCRN64_320x200:
//				width40x25_64s();
				break;

			case SCRN64_L320x200x2:
//				width40x25_64x2();
				break;

			case SCRN64_L640x200:
//				width80x25_64s();
				break;

			case SCRN64_H320x400:
//				width40x25_64h();
				break;

			case SCRN64_320x200x4096:
//				width40x25_4096();
				break;

			case SCRN64_320x100:
//				width40x12_64l();
				break;

			case SCRN64_320x100x2:
//				width40x12_64x2();
				break;

			case SCRN64_L640x100:
//				width80x12_64s();
				break;

			case SCRN64_H320x200:
//				width40x12_64h();
				break;

			case SCRN64_320x100x4096:
//				width40x12_4096();
				break;

//			case SCRN64_INVALID:
			default:
				if (!(crtc.s.SCRN_BITS & SCRN_UNDERLINE)) {
					screendraw[crtc.s.SCRN_BITS & 7]();
				}
				else {
					screendraw2[crtc.s.SCRN_BITS & 7]();
				}
				break;
		}
		ddrawflash = 1;
	}

	if (ddrawflash) {
		ddrawflash = 0;
		scrndraw_draw(FALSE);
		drawtime++;
	}
}
