/*  Copyright 2014 Theo Berkau

    This file is part of yssloader.

    yssloader is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    yssloader is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ida.hpp>
#include <idp.hpp>
#include <loader.hpp>
#include <name.hpp>
#include <search.hpp>
#include <diskio.hpp>

#define f_YSS 0x4444
#define PLFM_SCUDSP 0x8124
#define PLFM_SCSPDSP 0x8125

bool yssEndian;

void create_load_seg(linput_t *li, ea_t start, ea_t end, int type, const char *name);

//--------------------------------------------------------------------------
//
//      check input file format. if recognized, then return 1
//      and fill 'fileformatname'.
//      otherwise return 0
//
static int idaapi accept_yss_file(linput_t *li, char fileformatname[MAX_FILE_FORMAT_NAME], int n) 
{
		// read as much of the file as you need to to determine whether
		// it is something that you recognize
	   char id[3];

	   if (n || qlread(li, id, 3) == -1) 
			return 0;

		if (memcmp(id, "YSS", 3) != 0) {
			return 0;
		}

		//if you recognize the file, then say so
		qsnprintf(fileformatname, MAX_FILE_FORMAT_NAME, "YSS File");
		return 1;
}

static INLINE int StateCheckRetrieveHeader(linput_t *li, const char *name, int *version, int *size) {
	char id[4];
	size_t ret;

	if ((ret = qlread(li, id, 4)) != 4)
		return -1;

	if (strncmp(name, id, 4) != 0)
		return -2;

	if ((ret = qlread(li, version, 4)) != 4)
		return -1;

	if (qlread(li, size, 4) != 4)
		return -1;

	return 0;
}

typedef struct
{
	uint32 R[16];
   uint32 SR;
	uint32 GBR;
	uint32 VBR;
	uint32 MACH;
	uint32 MACL;
	uint32 PR;
	uint32 PC;
} sh2regs_struct;

void SH2LoadState(linput_t *li, bool isslave, sh2regs_struct *regs, int size)
{
	if (isslave == 1)
	   qlseek(li, 1, SEEK_CUR);

	// Read registers
	qlread(li, (void *)regs, sizeof(sh2regs_struct));
	qlseek(li, size-sizeof(sh2regs_struct), SEEK_CUR);
}

typedef struct
{
	uint8 vector;
	uint8 level;
	uint16 mask;
	uint32 statusbit;
} scuinterrupt_struct;

typedef struct
{
	/* DMA registers */
	uint32 D0R;
	uint32 D0W;
	uint32 D0C;
	uint32 D0AD;
	uint32 D0EN;
	uint32 D0MD;

	uint32 D1R;
	uint32 D1W;
	uint32 D1C;
	uint32 D1AD;
	uint32 D1EN;
	uint32 D1MD;

	uint32 D2R;
	uint32 D2W;
	uint32 D2C;
	uint32 D2AD;
	uint32 D2EN;
	uint32 D2MD;

	uint32 DSTP;
	uint32 DSTA;

	/* DSP registers */
	uint32 PPAF;
	uint32 PPD;
	uint32 PDA;
	uint32 PDD;

	/* Timer registers */
	uint32 T0C;
	uint32 T1S;
	uint32 T1MD;

	/* Interrupt registers */
	uint32 IMS;
	uint32 IST;

	/* A-bus registers */
	uint32 AIACK;
	uint32 ASR0;
	uint32 ASR1;
	uint32 AREF;

	/* SCU registers */
	uint32 RSEL;
	uint32 VER;

	/* internal variables */
	uint32 timer0;
	uint32 timer1;
	scuinterrupt_struct interrupts[30];
	uint32 NumberOfInterrupts;
} scuregs_struct;

typedef struct {
	uint32 ProgramRam[256];
	uint32 MD[4][64];
	union {
		struct {
			uint32 P:8;  // Program Ram Address
			uint32 unused3:7;
			uint32 LE:1; // Program counter load enable bit
			uint32 EX:1; // Program execute control bit
			uint32 ES:1; // Program step execute control bit
			uint32 E:1;  // Program end interrupt flag
			uint32 V:1;  // Overflow flag
			uint32 C:1;  // Carry flag
			uint32 Z:1;  // Zero flag
			uint32 S:1;  // Sine flag
			uint32 T0:1; // D0 bus use DMA execute flag
			uint32 unused2:1;
			uint32 EP:1; // Temporary stop execution flag
			uint32 PR:1; // Pause cancel flag
			uint32 unused1:5;
		} part;
		uint32 all;
	} ProgControlPort;
	uint8 PC;
	uint8 TOP;
	uint16 LOP;
	int32 jmpaddr;
	int32 delayed;
	uint8 DataRamPage;
	uint8 DataRamReadAddress;
	uint8 CT[4];
	uint32 RX;
	uint32 RY;
	uint32 RA0;
	uint32 WA0;

	union {
		struct {
			int64 L:32;
			int64 H:16;
			int64 unused:16;
		} part;
		int64 all;
	} AC;

	union {
		struct {
			int64 L:32;
			int64 H:16;
			int64 unused:16;
		} part;
		int64 all;
	} P;

	union {
		struct {
			int64 L:32;
			int64 H:16;
			int64 unused:16;
		} part;
		int64 all;
	} ALU;

	union {
		struct {
			int64 L:32;
			int64 H:16;
			int64 unused:16;
		} part;
		int64 all;
	} MUL;

} scudspregs_struct;

void ScuLoadState (linput_t *li, ea_t *pc, int size)
{
	scuregs_struct Scu;
	scudspregs_struct ScuDsp;

	qlread(li, (void *)&Scu, sizeof(Scu));
	qlread(li, (void *)&ScuDsp, sizeof(ScuDsp));

	if (pc)
	{
		*pc = ScuDsp.PC;
		ea_t start=0, end=0x100;
		add_segm(0, start, end, "RAM", "");
		for (ea_t i = 0; i < end-start; i++)
			put_long(start+i, ScuDsp.ProgramRam[i]);
	}
}

void SoundLoadState (linput_t *li, ea_t *pc, int size)
{
	char IsM68KRunning;   
	int pos=qltell(li);

	qlread(li, (void *)&IsM68KRunning, 1);
	qlseek(li, 4 * (8+8+1), SEEK_CUR);

	if (pc)
		qlread(li, (void *)pc, 4);
	else
		qlseek(li, 4, SEEK_CUR);

	qlseek (li, 0x1000, SEEK_CUR);

	if (pc)
	   create_load_seg(li, 0, 0x80000, 2, "RAM");
	else
		create_load_seg(li, 0x25A00000, 0x25A80000, 2, "SOUNDRAM");

   qlseek(li, size-(qltell(li)-pos), SEEK_CUR);
}

void Vdp1LoadState(linput_t *li, int size)
{
	int pos=qltell(li);

	// Skip registers
	qlseek(li, 52, SEEK_CUR);

	// Read VDP1 ram
	create_load_seg(li, 0x25C00000, 0x25C80000, 1, "VDP1RAM");

	qlseek(li, size-(qltell(li)-pos), SEEK_CUR);
}

void Vdp2LoadState(linput_t *li, int size)
{
	int pos=qltell(li);

	// Skip registers
	qlseek(li, 288, SEEK_CUR);

	// Read VDP2 ram
	create_load_seg(li, 0x25E00000, 0x25E80000, 1, "VDP2RAM");

	// Read CRAM
	create_load_seg(li, 0x25F00000, 0x25F01000, 2, "VDP2CRAM");
	qlseek(li, size-(qltell(li)-pos), SEEK_CUR);
}

void make_vector(ea_t addr, char *name)
{
	doDwrd(addr, 4);
	create_insn(get_long(addr));
	add_func(get_long(addr), BADADDR);
   add_cref(addr, get_long(addr), fl_CF);
	if (name != NULL)
		set_name(addr, name);
}

void identify_vector_table()
{
	int i;

	// MSH2 vector table
	for (i = 0x06000000; i < 0x06000200; i+=4)
		make_vector(i, NULL);

	// SSH2 vector table
	for (i = 0x06000400; i < 0x06000600; i+=4)
		make_vector(i, NULL);
}

void find_align(ea_t ea, ea_t maxea, asize_t length, int alignment)
{
	ea_t ret = next_unknown(ea, maxea);
	msg("next_unknown: %08X. get_word: %04X", ret, get_word(ret));
	if (ret != BADADDR && get_word(ret) == 0x0009)
		doAlign(ret, length, alignment);
}

void find_bios_funcs()
{
	ea_t i;
	make_ascii_string(0x06000200, 16, ASCSTR_C);
	doByte(0x06000210, 36);
	make_vector(0x06000234, NULL);
	make_vector(0x06000238, NULL);
	make_vector(0x0600023C, NULL);
	make_ascii_string(0x06000240, 4, ASCSTR_C);
	make_ascii_string(0x06000244, 4, ASCSTR_C);
	doDwrd(0x06000248, 4);
	doDwrd(0x0600024C, 4);
	make_vector(0x06000250, NULL);
	doDwrd(0x06000264, 4);
	make_vector(0x06000268, NULL);
	make_vector(0x0600026C, "bios_run_cd_player");
	make_vector(0x06000270, NULL);
	make_vector(0x06000274, "bios_is_mpeg_card_present");
	doDwrd(0x06000278, 4);
	doDwrd(0x0600027C, 4);
	make_vector(0x06000280, NULL);
	make_vector(0x06000284, NULL);
	make_vector(0x06000288, NULL);
	make_vector(0x0600028C, NULL);
	doDwrd(0x06000290, 4);
	doDwrd(0x06000294, 4);
	make_vector(0x06000298, "bios_get_mpeg_rom");
	make_vector(0x0600029C, NULL);
	doDwrd(0x060002A0, 4);
	doDwrd(0x060002A4, 4);
	doDwrd(0x060002A8, 4);
	doDwrd(0x060002AC, 4);
	make_vector(0x060002B0, NULL);
	doDwrd(0x060002B4, 4);
	doDwrd(0x060002B8, 4);
	doDwrd(0x060002BC, 4);
	doDwrd(0x060002C0, 4);
	for (i = 0x060002C4; i < 0x06000324; i+=4)
		make_vector(i, NULL);
	set_name(0x06000300, "bios_set_scu_interrupt");
	set_name(0x06000304, "bios_get_scu_interrupt");
	set_name(0x06000310, "bios_set_sh2_interrupt");
	set_name(0x06000314, "bios_get_sh2_interrupt");
	set_name(0x06000320, "bios_set_clock_speed");
	doDwrd(0x06000324, 4);
	set_name(0x06000324, "bios_get_clock_speed");
	for (i = 0x06000328; i < 0x06000348; i+=4)
		make_vector(i, NULL);
	set_name(0x06000340, "bios_set_scu_interrupt_mask");
	set_name(0x06000344, "bios_change_scu_interrupt_mask");
	doDwrd(0x06000348, 4);
	set_name(0x06000348, "bios_get_scu_interrupt_mask");
	make_vector(0x0600034C, NULL);
	doDwrd(0x06000350, 4);
	doDwrd(0x06000354, 4);
	doDwrd(0x06000358, 4);
	doDwrd(0x0600035C, 4);
	for (i = 0x06000360; i < 0x06000380; i+=4)
		make_vector(i, NULL);
	doByte(0x06000380, 16);
	doWord(0x06000390, 16);
	doDwrd(0x060003A0, 32);
	make_ascii_string(0x060003C0, 0x40, ASCSTR_C);
	add_func(0x06000600, BADADDR);
	add_func(0x06000646, BADADDR);
	make_ascii_string(0x0600065C, 0x4, ASCSTR_C);
	add_func(0x06000678, BADADDR);
	add_func(0x0600067C, BADADDR);
	add_func(0x06000690, BADADDR);
	doDwrd(0x06000A80, 0x80);
}

bool find_parse_ip(ea_t ea, bool parsecode)
{
	char id[16];
	// Attempt to identify program start from ip
	get_many_bytes(ea, id, 16);
	if (memcmp(id, "SEGA SEGASATURN ", 16) != 0)
		return false;

	make_ascii_string(ea, 16, ASCSTR_C);
   make_ascii_string(ea+0x10, 16, ASCSTR_C);
	make_ascii_string(ea+0x20, 10, ASCSTR_C);
	make_ascii_string(ea+0x2A, 6, ASCSTR_C);
	make_ascii_string(ea+0x30, 8, ASCSTR_C);
	make_ascii_string(ea+0x38, 8, ASCSTR_C);
	make_ascii_string(ea+0x40, 10, ASCSTR_C);
	make_ascii_string(ea+0x4A, 6, ASCSTR_C);
	make_ascii_string(ea+0x50, 16, ASCSTR_C);
	make_ascii_string(ea+0x60, 0x70, ASCSTR_C);
	doByte(ea+0xD0, 16);
	doDwrd(ea+0xE0, 4);
	doDwrd(ea+0xE4, 4);
	doDwrd(ea+0xE8, 4);
	doDwrd(ea+0xEC, 4);
	doDwrd(ea+0xF0, 4);
	add_func(get_long(ea+0xF0), BADADDR);
	doDwrd(ea+0xF4, 4);
	doDwrd(ea+0xF8, 4);
	doDwrd(ea+0xFC, 4);
	if (parsecode)
	   add_func(ea+0x100, BADADDR);
	return true;
}

void create_load_seg(linput_t *li, ea_t start, ea_t end, int type, const char *name)
{
	add_segm(0, start, end, name, "");
	switch (type)
	{
	   case 1:
			for (ea_t i = 0; i < end-start; i+=4)
			{
				uint32 data;
				qlread(li, &data, 4);
				put_long(start+i, data);
			}
			break;
	   case 2: // Word Swap
			for (ea_t i = 0; i < end-start; i+=4)
			{
				uint16 data, data2;
				qlread(li, &data, 2);
				qlread(li, &data2, 2);
				put_long(start+i, (data << 16) | data2);
			}
			break;
		case 3: // Reverse order
			for (ea_t i = 0; i < end-start; i+=4)
			{
				uint32 data;
				qlread(li, &data, 4);
				data = swap32(data);
				put_long(end-i-1, data);
			}
			break;
		default: break;
	}
}

bool load_header(linput_t *li)
{
	int headerversion;
	int size;

	qlseek(li, 3);
	if (qlread(li, &yssEndian, 1) != 1)
	{
		error("Truncated file");
		return false;
	}

	qlread(li, &headerversion, 4);
	qlread(li, &size, 4);
	int headersize=0xC;
	if (headerversion == 2)
	{
		qlseek(li, 8, SEEK_CUR);
		headersize+=8;
	}

	// Make sure size variable matches actual size minus header
	if (size != (qlsize(li) - headersize))
	{
		error("Header size isn't valid");
		return false;
	}

	return true;
}

ea_t find_string(char *search_str)
{
	char text[512];
	qsnprintf(text, sizeof(text), "\"%s\"", search_str);
	ea_t ret = find_binary(0x06000000, 0x06100000, text, 0, SEARCH_DOWN | SEARCH_CASE);
	if (ret == BADADDR)
		return find_binary(0x00200000, 0x00300000, text, 0, SEARCH_DOWN | SEARCH_CASE);
	return ret;
}

void get_lib_version(ea_t addr, int str_offset, char *version_str, size_t version_str_size)
{
	char text[512];
	for (int i = 0; i < 512; i++)
	{
		text[i] = get_byte(addr+str_offset+i);
		if (text[i] == '\0')
			break;
	}
	sscanf_s(text, "Version %s *%s", version_str, version_str_size);
}

void load_scudsp_data(linput_t *li)
{
	int version;
	int csize;
	ea_t pc;

	if (!load_header(li))
		return;

	if (StateCheckRetrieveHeader(li, "CART", &version, &csize) != 0)
	{
		error("Invalid CART chunk");
		return;
	}
	qlseek(li, csize, SEEK_CUR);

	if (StateCheckRetrieveHeader(li, "CS2 ", &version, &csize) != 0)
	{
		error("Invalid CS2 chunk");
		return;
	}
	qlseek(li, csize, SEEK_CUR);

	if (StateCheckRetrieveHeader(li, "MSH2", &version, &csize) != 0)
	{
		error("Invalid MSH2 chunk");
		return;
	}
	qlseek(li, csize, SEEK_CUR);

	if (StateCheckRetrieveHeader(li, "SSH2", &version, &csize) != 0)
	{
		error("Invalid SSH2 chunk");
		return;
	}
	qlseek(li, csize, SEEK_CUR);

	if (StateCheckRetrieveHeader(li, "SCSP", &version, &csize) != 0)
	{
		error("Invalid SCSP chunk");
		return;
	}
	qlseek(li, csize, SEEK_CUR);

	if (StateCheckRetrieveHeader(li, "SCU ", &version, &csize) != 0)
	{
		error("Invalid SCU chunk");
		return;
	}
	ScuLoadState(li, &pc, csize);

	if (StateCheckRetrieveHeader(li, "SMPC", &version, &csize) != 0)
	{
		error("Invalid SMPC chunk");
		return;
	}
	qlseek(li, csize, SEEK_CUR);

	if (StateCheckRetrieveHeader(li, "VDP1", &version, &csize) != 0)
	{
		error("Invalid VDP1 chunk");
		return;
	}
	qlseek(li, csize, SEEK_CUR);

	if (StateCheckRetrieveHeader(li, "VDP2", &version, &csize) != 0)
	{
		error("Invalid VDP2 chunk");
		return;
	}
	qlseek(li, csize, SEEK_CUR);

	if (StateCheckRetrieveHeader(li, "OTHR", &version, &csize) != 0)
	{
		error("Invalid OTHR chunk");
		return;
	}

	// move cursor to current SCU DSP PC
	jumpto(pc);
}

void load_scspdsp_data(linput_t *li)
{

}

void load_68k_data(linput_t *li)
{
	int version;
	int csize;
	ea_t pc;

	if (!load_header(li))
		return;

	if (StateCheckRetrieveHeader(li, "CART", &version, &csize) != 0)
	{
		error("Invalid CART chunk");
		return;
	}
	qlseek(li, csize, SEEK_CUR);

	if (StateCheckRetrieveHeader(li, "CS2 ", &version, &csize) != 0)
	{
		error("Invalid CS2 chunk");
		return;
	}
	qlseek(li, csize, SEEK_CUR);

	if (StateCheckRetrieveHeader(li, "MSH2", &version, &csize) != 0)
	{
		error("Invalid MSH2 chunk");
		return;
	}
	qlseek(li, csize, SEEK_CUR);

	if (StateCheckRetrieveHeader(li, "SSH2", &version, &csize) != 0)
	{
		error("Invalid SSH2 chunk");
		return;
	}
	qlseek(li, csize, SEEK_CUR);

	if (StateCheckRetrieveHeader(li, "SCSP", &version, &csize) != 0)
	{
		error("Invalid SCSP chunk");
		return;
	}
	SoundLoadState(li, &pc, csize);

	if (StateCheckRetrieveHeader(li, "SCU ", &version, &csize) != 0)
	{
		error("Invalid SCU chunk");
		return;
	}
	qlseek(li, csize, SEEK_CUR);

	if (StateCheckRetrieveHeader(li, "SMPC", &version, &csize) != 0)
	{
		error("Invalid SMPC chunk");
		return;
	}
	qlseek(li, csize, SEEK_CUR);

	if (StateCheckRetrieveHeader(li, "VDP1", &version, &csize) != 0)
	{
		error("Invalid VDP1 chunk");
		return;
	}
	qlseek(li, csize, SEEK_CUR);

	if (StateCheckRetrieveHeader(li, "VDP2", &version, &csize) != 0)
	{
		error("Invalid VDP2 chunk");
		return;
	}
	qlseek(li, csize, SEEK_CUR);

	if (StateCheckRetrieveHeader(li, "OTHR", &version, &csize) != 0)
	{
		error("Invalid OTHR chunk");
		return;
	}

	for (int i = 0x000004; i < 0x0000EC; i+=4)
		make_vector(i, NULL);

	// move cursor to current 68K PC
	jumpto(pc);
}

void load_sh2_data(linput_t *li)
{
	int version;
	int csize;
	ea_t result;
	sh2regs_struct sh2regs;

	if (!load_header(li))
		return;

	if (StateCheckRetrieveHeader(li, "CART", &version, &csize) != 0)
	{
		error("Invalid CART chunk");
		return;
	}
	qlseek(li, csize, SEEK_CUR);

	if (StateCheckRetrieveHeader(li, "CS2 ", &version, &csize) != 0)
	{
		error("Invalid CS2 chunk");
		return;
	}
	qlseek(li, csize, SEEK_CUR);

	if (StateCheckRetrieveHeader(li, "MSH2", &version, &csize) != 0)
	{
		error("Invalid MSH2 chunk");
		return;
	}
	SH2LoadState(li, false, &sh2regs, csize);

	if (StateCheckRetrieveHeader(li, "SSH2", &version, &csize) != 0)
	{
		error("Invalid SSH2 chunk");
		return;
	}
	qlseek(li, csize, SEEK_CUR);

	if (StateCheckRetrieveHeader(li, "SCSP", &version, &csize) != 0)
	{
		error("Invalid SCSP chunk");
		return;
	}
	SoundLoadState(li, NULL, csize);

	if (StateCheckRetrieveHeader(li, "SCU ", &version, &csize) != 0)
	{
		error("Invalid SCU chunk");
		return;
	}
	qlseek(li, csize, SEEK_CUR);

	if (StateCheckRetrieveHeader(li, "SMPC", &version, &csize) != 0)
	{
		error("Invalid SMPC chunk");
		return;
	}
	qlseek(li, csize, SEEK_CUR);

	if (StateCheckRetrieveHeader(li, "VDP1", &version, &csize) != 0)
	{
		error("Invalid VDP1 chunk");
		return;
	}
	Vdp1LoadState(li, csize);

	if (StateCheckRetrieveHeader(li, "VDP2", &version, &csize) != 0)
	{
		error("Invalid VDP2 chunk");
		return;
	}
	Vdp2LoadState(li, csize);

	if (StateCheckRetrieveHeader(li, "OTHR", &version, &csize) != 0)
	{
		error("Invalid OTHR chunk");
		return;
	}

	qlseek(li, 0x10000, SEEK_CUR); // BUP Ram
	create_load_seg(li, 0x06000000, 0x06100000, 2, "HWRAM");
	create_load_seg(li, 0x00200000, 0x00300000, 2, "LWRAM");
	identify_vector_table();
	find_bios_funcs();
	find_parse_ip(0x06000C00, false);
	find_parse_ip(0x06002000, true);

	// Look for SBL/SGL and load correct sig if available
	if ((result = find_string("GFS_SGL ")) != BADADDR)
	{
		char version_str[512];
		get_lib_version(result, 8, version_str, sizeof(version_str));
	   msg("SGL detected\n");

		if (atof(version_str) <= 2.10)
			plan_to_apply_idasgn("sgl20a.sig");
		else if (atof(version_str) == 2.11)
		{
			// SGL 2.1, 3.00, 3.02j
			plan_to_apply_idasgn("sgl302j.sig");
		}
		else if (atof(version_str) >= 2.12)
	      plan_to_apply_idasgn("sgl302j.sig");

		if ((result = find_string("CPK Version")) != BADADDR)
		{
			get_lib_version(result, 4, version_str, sizeof(version_str));
			plan_to_apply_idasgn("cpksgl.sig");
		}
	}
	else if ((result = find_string("GFS_SBL ")) != BADADDR)
	{
		char version_str[512];
		get_lib_version(result, 8, version_str, sizeof(version_str));
		msg("SBL detected\n");
		plan_to_apply_idasgn("sbl601.sig");

		if ((result = find_string("CPK Version")) != BADADDR)
		{
			get_lib_version(result, 4, version_str, sizeof(version_str));
			plan_to_apply_idasgn("cpksbl.sig");
		}
	}

	// move cursor to current MSH2 PC
	jumpto(sh2regs.PC);
}

static void idaapi load_yss_file(linput_t *li, ushort neflags,
                      const char * /*fileformatname*/) 
{
	switch (ph.id)
	{
	   case PLFM_68K:
			set_processor_type("68000", SETPROC_ALL|SETPROC_FATAL);
			load_68k_data(li);
			break;
		case PLFM_SCUDSP:
			set_processor_type("scudsp", SETPROC_ALL|SETPROC_FATAL);
			load_scudsp_data(li);
			break;
		case PLFM_SCSPDSP:
			set_processor_type("scspdsp", SETPROC_ALL|SETPROC_FATAL);
			load_scspdsp_data(li);
			break;
	   case PLFM_SH:
		default:
			set_processor_type("sh3b", SETPROC_ALL|SETPROC_FATAL);
			load_sh2_data(li);
			break;
	}		
}

idaman loader_t ida_module_data LDSC;

//----------------------------------------------------------------------
//
//      LOADER DESCRIPTION BLOCK
//
//----------------------------------------------------------------------
loader_t LDSC = {
	IDP_INTERFACE_VERSION,
	0,                     // loader flags
	accept_yss_file, // test yss format.
	load_yss_file,   // load file into the database.
	NULL,   // yss save
	NULL,                  // no special handling for moved segments
	NULL                   // no special handling for File->New
};

//----------------------------------------------------------------------
