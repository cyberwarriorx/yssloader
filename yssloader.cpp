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

bool yssEndian;

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

/*
int SoundLoadState (FILE *fp, int version, int size)
{
	int i, i2;
	u32 temp;
	u8 nextphase;

	// Read 68k registers first
	yread (&check, (void *)&IsM68KRunning, 1, 1, fp);

	for (i = 0; i < 8; i++)
	{
		yread (&check, (void *)&temp, 4, 1, fp);
		M68K->SetDReg (i, temp);
	}

	for (i = 0; i < 8; i++)
	{
		yread (&check, (void *)&temp, 4, 1, fp);
		M68K->SetAReg (i, temp);
	}

	yread (&check, (void *)&temp, 4, 1, fp);
	M68K->SetSR (temp);
	yread (&check, (void *)&temp, 4, 1, fp);
	M68K->SetPC (temp);

	// Now for the SCSP registers
	yread (&check, (void *)scsp_reg, 0x1000, 1, fp);

	// Lastly, sound ram
	yread (&check, (void *)SoundRam, 0x80000, 1, fp);

	if (version > 1)
	{
		// Internal variables need to be regenerated
		for(i = 0; i < 32; i++)
		{
			for (i2 = 0; i2 < 0x20; i2+=2)
				scsp_slot_set_w (i, 0x1E - i2, scsp_slot_get_w (i, 0x1E - i2));
		}

		scsp_set_w (0x402, scsp_get_w (0x402));

		// Read slot internal variables
		for (i = 0; i < 32; i++)
		{
			s32 einc;

			yread (&check, (void *)&scsp.slot[i].key, 1, 1, fp);
			yread (&check, (void *)&scsp.slot[i].fcnt, 4, 1, fp);
			yread (&check, (void *)&scsp.slot[i].ecnt, 4, 1, fp);

			yread (&check, (void *)&einc, 4, 1, fp);
			switch (einc)
			{
			case 0:
				scsp.slot[i].einc = &scsp.slot[i].einca;
				break;
			case 1:
				scsp.slot[i].einc = &scsp.slot[i].eincd;
				break;
			case 2:
				scsp.slot[i].einc = &scsp.slot[i].eincs;
				break;
			case 3:
				scsp.slot[i].einc = &scsp.slot[i].eincr;
				break;
			default:
				scsp.slot[i].einc = NULL;
				break;
			}

			yread (&check, (void *)&scsp.slot[i].ecmp, 4, 1, fp);
			yread (&check, (void *)&scsp.slot[i].ecurp, 4, 1, fp);

			yread (&check, (void *)&nextphase, 1, 1, fp);
			switch (nextphase)
			{
			case 0:
				scsp.slot[i].enxt = scsp_env_null_next;
				break;
			case 1:
				scsp.slot[i].enxt = scsp_release_next;
				break;
			case 2:
				scsp.slot[i].enxt = scsp_sustain_next;
				break;
			case 3:
				scsp.slot[i].enxt = scsp_decay_next;
				break;
			case 4:
				scsp.slot[i].enxt = scsp_attack_next;
				break;
			default: break;
			}

			yread (&check, (void *)&scsp.slot[i].lfocnt, 4, 1, fp);
			yread (&check, (void *)&scsp.slot[i].lfoinc, 4, 1, fp);

			// Rebuild the buf8/buf16 variables
			if (scsp.slot[i].pcm8b)
			{
				scsp.slot[i].buf8 = (s8*)&(scsp.scsp_ram[scsp.slot[i].sa]);
				if ((scsp.slot[i].sa + (scsp.slot[i].lea >> SCSP_FREQ_LB)) >
					SCSP_RAM_MASK)
					scsp.slot[i].lea = (SCSP_RAM_MASK - scsp.slot[i].sa) <<
					SCSP_FREQ_LB;
			}
			else
			{
				scsp.slot[i].buf16 = (s16*)&(scsp.scsp_ram[scsp.slot[i].sa & ~1]);
				if ((scsp.slot[i].sa + (scsp.slot[i].lea >> (SCSP_FREQ_LB - 1))) >
					SCSP_RAM_MASK)
					scsp.slot[i].lea = (SCSP_RAM_MASK - scsp.slot[i].sa) <<
					(SCSP_FREQ_LB - 1);
			}
		}

		// Read main internal variables
		yread (&check, (void *)&scsp.mem4b, 4, 1, fp);
		yread (&check, (void *)&scsp.mvol, 4, 1, fp);

		yread (&check, (void *)&scsp.rbl, 4, 1, fp);
		yread (&check, (void *)&scsp.rbp, 4, 1, fp);

		yread (&check, (void *)&scsp.mslc, 4, 1, fp);

		yread (&check, (void *)&scsp.dmea, 4, 1, fp);
		yread (&check, (void *)&scsp.drga, 4, 1, fp);
		yread (&check, (void *)&scsp.dmfl, 4, 1, fp);
		yread (&check, (void *)&scsp.dmlen, 4, 1, fp);

		yread (&check, (void *)scsp.midinbuf, 1, 4, fp);
		yread (&check, (void *)scsp.midoutbuf, 1, 4, fp);
		yread (&check, (void *)&scsp.midincnt, 1, 1, fp);
		yread (&check, (void *)&scsp.midoutcnt, 1, 1, fp);
		yread (&check, (void *)&scsp.midflag, 1, 1, fp);

		yread (&check, (void *)&scsp.timacnt, 4, 1, fp);
		yread (&check, (void *)&scsp.timasd, 4, 1, fp);
		yread (&check, (void *)&scsp.timbcnt, 4, 1, fp);
		yread (&check, (void *)&scsp.timbsd, 4, 1, fp);
		yread (&check, (void *)&scsp.timccnt, 4, 1, fp);
		yread (&check, (void *)&scsp.timcsd, 4, 1, fp);

		yread (&check, (void *)&scsp.scieb, 4, 1, fp);
		yread (&check, (void *)&scsp.scipd, 4, 1, fp);
		yread (&check, (void *)&scsp.scilv0, 4, 1, fp);
		yread (&check, (void *)&scsp.scilv1, 4, 1, fp);
		yread (&check, (void *)&scsp.scilv2, 4, 1, fp);
		yread (&check, (void *)&scsp.mcieb, 4, 1, fp);
		yread (&check, (void *)&scsp.mcipd, 4, 1, fp);

		yread (&check, (void *)scsp.stack, 4, 32 * 2, fp);
	}

	return size;
}

int Vdp1LoadState(FILE *fp, UNUSED int version, int size)
{
	// Read registers
	yread(&check, (void *)Vdp1Regs, sizeof(Vdp1), 1, fp);

	// Read VDP1 ram
	yread(&check, (void *)Vdp1Ram, 0x80000, 1, fp);

	return size;
}

int Vdp2LoadState(FILE *fp, UNUSED int version, int size)
{
	// Read registers
	yread(&check, (void *)Vdp2Regs, sizeof(Vdp2), 1, fp);

	// Read VDP2 ram
	yread(&check, (void *)Vdp2Ram, 0x80000, 1, fp);

	// Read CRAM
	yread(&check, (void *)Vdp2ColorRam, 0x1000, 1, fp);

	// Read internal variables
	yread(&check, (void *)&Vdp2Internal, sizeof(Vdp2Internal_struct), 1, fp);

	return size;
}
*/

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

void create_load_seg(linput_t *li, ea_t start, ea_t end, const char *name)
{
	add_segm(0, start, end, name, "");
	for (ea_t i = 0; i < end-start; i+=4)
	{
		uint16 data, data2;
		qlread(li, &data, 2);
		qlread(li, &data2, 2);
		put_long(start+i, (data << 16) | data2);
	}
}

void load_scudsp_data(linput_t *li)
{

}

void load_68k_data(linput_t *li)
{

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

void load_sh2_data(linput_t *li)
{
	int headerversion;
	int size;
	int version;
	int csize;
	ea_t result;
	sh2regs_struct sh2regs;

	qlseek(li, 3);
	if (qlread(li, &yssEndian, 1) != 1)
	{
		error("Truncated file");
		return;
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
		return;
	}

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
	qlseek(li, csize, SEEK_CUR);

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

	qlseek(li, 0x10000, SEEK_CUR); // BUP Ram
	create_load_seg(li, 0x06000000, 0x06100000, "HWRAM");
	create_load_seg(li, 0x00200000, 0x00300000, "LWRAM");
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
