/*********************************************************************
 *
 * Copyright (C) 2003-2004,  Karlsruhe University
 *
 * File path:     resourcemon/include/freq_acpi.h
 * Description:   
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 ********************************************************************/
#ifndef __RESOURCEMON__FREQ_ACPI_H__
#define __RESOURCEMON__FREQ_ACPI_H__

#include <common/string.h>
#include <common/debug.h>

extern L4_Word_t get_device( L4_Word_t addr, L4_Word_t size );
void acpi_map_rest(word_t phys_addr, word_t size);

#define MAX_PSTATES 10

#define ACPI20_PC99_RSDP_START	0x0e0000
#define ACPI20_PC99_RSDP_END	0x100000
#define ACPI20_PC99_RSDP_SIZE   (ACPI20_PC99_RSDP_END - ACPI20_PC99_RSDP_START)

struct acpi_pss_t {
	u32_t core_freq;
	u32_t power;
	u32_t transition_latency;
	u32_t bus_master_latency;
	u32_t control;
	u32_t status;
};

struct acpi_dword_t {
	u8_t prefix;
	u32_t value;
} __attribute__((packed));

class acpi_thead_t {
	public:
		char	sig[4];
		u32_t	len;
		u8_t	rev;
		u8_t	csum;
		char	oem_id[6];
		char	oem_tid[8];
		u32_t	oem_rev;
		u32_t	creator_id;
		u32_t	creator_rev;

		void show(void) {
		    printf("ACPI DVFS: signature %C length %d revision %d\n",
			   DEBUG_TO_4CHAR(sig), len, rev);
		    printf("ACPI DVFS: oem id %C%c%c oem tid %C%c%c oem rev %d\n", 
			   DEBUG_TO_4CHAR(oem_id), oem_id+4, oem_id+5,
			   DEBUG_TO_4CHAR(oem_tid), oem_tid+4, oem_tid+5, rev);
		};
} __attribute__((packed));

class acpi_fadt_t {
	public:
		acpi_thead_t header;
		u32_t firmware_ctrl;
		u32_t dsdt_ptr;
		u8_t unused_1[4];
		u32_t smi_cmd;
		u8_t unused_2[3];
		u8_t pstate_cnt; // must be 0x00 for K10 cpus
		u8_t unused_3[188];

		acpi_thead_t* get_dsdt() {
			acpi_thead_t *t = (acpi_thead_t*)get_device(dsdt_ptr, sizeof(acpi_thead_t));
			acpi_map_rest(dsdt_ptr, t->len);
			return t;
		};
} __attribute__((packed));

/* DSDT and SSDT are actually the same */
class acpi_dsdt_t {
	public:
		acpi_thead_t header;
		char code[0];

		void spill(void) {
			for (u32_t i = 0; i < (header.len - sizeof(acpi_thead_t)); i++)
			    printf("%d", code[i]);
			printf("\n");
		}
		// returns a pointer to the n-th occurence of name or 0
		u8_t* get_package(const char* name /*4 letters + \0*/, u32_t n);
} __attribute__((packed));

/*
   RSDT and XSDT differ in their pointer size only
rsdt: 32bit, xsdt: 64bit
 */
template<typename T> class acpi__sdt_t {
	acpi_thead_t	header;
	T			ptrs[0];
	public:
	/* find table with a given signature */
	// MANUEL: There could be several SSDTs. Maybe change find(sig) to find(sig, index) so
	// one could loop through all SSDTs until the right oem_id etc is found.
	acpi_thead_t* find(const char sig[4]) {
		acpi_thead_t *head = NULL;
		for (word_t i = 0; i < ((header.len-sizeof(header))/sizeof(ptrs[0])) && !head; i++)
		{
			acpi_thead_t* t= (acpi_thead_t*)get_device(ptrs[i], sizeof(acpi_thead_t));
			if (t == 0) L4_KDB_Enter("mapping failed!");

			if (t->sig[0] == sig[0] && t->sig[1] == sig[1] && 
					t->sig[2] == sig[2] && t->sig[3] == sig[3]){
				head = t;
				acpi_map_rest(ptrs[i], t->len);
			}
		}
		return head;
	};
	void list(void) {
		for (word_t i = 0; i < ((header.len-sizeof(header))/sizeof(ptrs[0])); i++)
		{
			acpi_thead_t* t = (acpi_thead_t*)get_device(ptrs[i], sizeof(acpi_thead_t)) ;
			if (t == 0)	
			    L4_KDB_Enter("mapping failed!");
			printf("%C %x\n", DEBUG_TO_4CHAR(t->sig), ptrs[i]);
		}
	};
} __attribute__((packed));

typedef acpi__sdt_t<u32_t> acpi_rsdt_t;
typedef acpi__sdt_t<u64_t> acpi_xsdt_t;

class acpi_rsdp_t {
	char	sig[8];
	u8_t	csum;
	char	oemid[6];
	u8_t	rev;
	u32_t	rsdt_ptr;
	u32_t	rsdt_len;
	u64_t	xsdt_ptr;
	u8_t	xcsum;
	u8_t	_rsvd_33[3];
	private:
	public:
	static acpi_rsdp_t* locate(word_t addr = NULL) {
		/** @todo checksum, check version */
		for (word_t p = addr;
				p < addr + ACPI20_PC99_RSDP_SIZE;
				p = p + 16)
		{
			acpi_rsdp_t* r = (acpi_rsdp_t*) p;
			if (r->sig[0] == 'R' &&
					r->sig[1] == 'S' &&
					r->sig[2] == 'D' &&
					r->sig[3] == ' ' &&
					r->sig[4] == 'P' &&
					r->sig[5] == 'T' &&
					r->sig[6] == 'R' &&
					r->sig[7] == ' ')
				return r;
		};
		/* not found */
		return NULL;
	};

	acpi_rsdt_t* rsdt() {
		/* verify checksum */
		u8_t csum = 0;
		for (int i = 0; i < 20; i++)
			csum += ((char*)this)[i];
		if (csum != 0)
			return NULL;
		acpi_rsdt_t* ret = (acpi_rsdt_t*) get_device(rsdt_ptr, sizeof(acpi_rsdt_t));
		acpi_map_rest(rsdt_ptr, sizeof(acpi_rsdt_t));
		return ret;
	};
	acpi_xsdt_t* xsdt() {
		/* check version - only ACPI 2.0 knows about an XSDT */
		if (rev != 2)
			return NULL;
		/* verify checksum
		   hopefully it's wrong if there's no xsdt pointer*/
		u8_t csum = 0;
		for (int i = 0; i < 36; i++)
			csum += ((char*)this)[i];
		if (csum != 0)
			return NULL;
		//return (acpi_xsdt_t*) (word_t)xsdt_ptr;
		return (acpi_xsdt_t*) get_device(xsdt_ptr, sizeof(acpi_xsdt_t));
	};

	friend class kdb_t;
} __attribute__((packed));

#endif // !__RESOURCEMON__FREQ_ACPI_H__
