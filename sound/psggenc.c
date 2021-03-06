/**
 * @file	psggenc.c
 * @brief	Implementation of the PSG generator
 */

#include "compiler.h"
#include <math.h>
#include "psggen.h"

	PSGGENCFG	psggencfg;

static const SINT32 psgvolrate[16] = {
				   0,  508,  719, 1017, 1440, 2037, 2883, 4079,
				5772, 8167,11556,16351,23135,32735,46317,65536};

static const UINT8 psggen_deftbl[0x10] =
				{0, 0, 0, 0, 0, 0, 0, 0xbf, 0, 0, 0, 0, 0, 0, 0xff, 0xff};

static const UINT8 psgenv_pat[16] = {
					PSGENV_ONESHOT,
					PSGENV_ONESHOT,
					PSGENV_ONESHOT,
					PSGENV_ONESHOT,
					PSGENV_ONESHOT | PSGENV_INC,
					PSGENV_ONESHOT | PSGENV_INC,
					PSGENV_ONESHOT | PSGENV_INC,
					PSGENV_ONESHOT | PSGENV_INC,
					PSGENV_ONECYCLE,
					PSGENV_ONESHOT,
					0,
					PSGENV_ONESHOT | PSGENV_LASTON,
					PSGENV_ONECYCLE | PSGENV_INC,
					PSGENV_ONESHOT | PSGENV_INC | PSGENV_LASTON,
					PSGENV_INC,
					PSGENV_ONESHOT | PSGENV_INC};


void psggen_initialize(UINT rate) {

	UINT	i;

	memset(&psggencfg, 0, sizeof(psggencfg));
	psggencfg.rate = rate;
	for (i=0; i<16; i++) {
		psggencfg.voltbl[i] = (0x0c00 * psgvolrate[i]) >> 16;
	}
	psggencfg.puchidec = (UINT16)(rate * 2 / 11025);
	if (psggencfg.puchidec == 0) {
		psggencfg.puchidec = 1;
	}
	if (rate) {
		psggencfg.base = (5000 * (1 << (32 - PSGFREQPADBIT - PSGADDEDBIT)))
															/ (rate / 25);
	}
}

void psggen_setvol(UINT vol) {

	UINT	i;

	for (i=1; i<16; i++) {
		psggencfg.volume[i] = (psggencfg.voltbl[i] * vol) >> 
															(6 + PSGADDEDBIT);
	}
}

void psggen_reset(PSGGEN psg) {

	UINT	i;

	memset(psg, 0, sizeof(*psg));
	for (i=0; i<3; i++) {
		psg->tone[i].pvol = psggencfg.volume + 0;
	}
	psg->noise.lfsr = 1;
	for (i=0; i<sizeof(psggen_deftbl); i++) {
		psggen_setreg(psg, (REG8)i, psggen_deftbl[i]);
	}
}

void psggen_restore(PSGGEN psg) {

	REG8	i;

	for (i=0; i<0x0e; i++) {
		psggen_setreg(psg, i, ((UINT8 *)&psg->reg)[i]);
	}
}

void psggen_setreg(PSGGEN psg, REG8 reg, REG8 value) {

	UINT	freq;
	UINT	ch;

	if (reg >= 14) {
		return;
	}
	sound_sync();
	((UINT8 *)&psg->reg)[reg] = value;
	switch(reg) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
			ch = reg >> 1;
			freq = LOADINTELWORD(psg->reg.tune[ch]) & 0xfff;
			if (freq > 9) {
				psg->tone[ch].freq = (psggencfg.base / freq) << PSGFREQPADBIT;
			}
			else {
				psg->tone[ch].freq = 0;
				psg->tone[ch].count = 0;	// としておかないとノイズ出ない
			}
			break;

		case 6:
			freq = value & 0x1f;
			if (freq == 0) {
				freq = 1;
			}
			psg->noise.freq = (psggencfg.base / freq) << PSGFREQPADBIT;
			break;

		case 7:
			psg->mixer = ~value;
			psg->puchicount = psggencfg.puchidec;
			break;

		case 8:
		case 9:
		case 10:
			ch = reg - 8;
			if (value & 0x10) {
				psg->tone[ch].pvol = &psg->evol;
			}
			else {
				psg->tone[ch].pvol = psggencfg.volume + (value & 15);
			}
			psg->tone[ch].puchi = psggencfg.puchidec;
			psg->puchicount = psggencfg.puchidec;
			break;

		case 11:
		case 12:
			freq = LOADINTELWORD(psg->reg.envtime);
			freq = psggencfg.rate * freq / 125000;
			if (freq == 0) {
				freq = 1;
			}
			psg->envmax = freq;
			break;

		case 13:
			psg->envmode = psgenv_pat[value & 0x0f];
			psg->envvolcnt = 16;
			psg->envcnt = 0;
			break;
	}
}

REG8 psggen_getreg(PSGGEN psg, REG8 reg) {

	return(((UINT8 *)&psg->reg)[reg & 15]);
}

void psggen_setpan(PSGGEN psg, UINT ch, REG8 pan) {

	if ((psg) && (ch < 3)) {
		psg->tone[ch].pan = pan;
	}
}

