/*
 * Copyright (c) 2021 SUSE LLC
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 */

#include <elf.h>
#include <endian.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/procfs.h>		/* for prstatus_t */

#if __BYTE_ORDER == __LITTLE_ENDIAN
# define ELF_DATA	ELFDATA2LSB
#elif __BYTE_ORDER == __BIG_ENDIAN
# define ELF_DATA	ELFDATA2MSB
#else
# error "Unimplemented endianity!"
#endif

#if defined(__x86_64__)
# define ELF_MACHINE	EM_X86_64
#elif defined(__i386__)
# define ELF_MACHINE	EM_386
#elif defined(__powerpc64__)
# define ELF_MACHINE	EM_PPC64
#elif defined(__powerpc__)
# define ELF_MACHINE	EM_PPC
#elif defined(__s390x__) || defined(__s390__)
# define ELF_MACHINE	EM_S390
#elif defined(__ia64__)
# define ELF_MACHINE	EM_IA_64
#elif defined(__aarch64__)
# define ELF_MACHINE	EM_AARCH64
#elif defined(__arm__)
# define ELF_MACHINE	EM_ARM
#elif defined(__alpha__)
# define ELF_MACHINE	EM_ALPHA
#else
# error "Unimplemented architecture!"
#endif

#define elf_aligned	__attribute__((aligned(4)))

static const char core_name[] = "CORE";

struct prstatus_note {
	Elf64_Nhdr hdr;
	elf_aligned char name[sizeof(core_name)];
	elf_aligned prstatus_t status;
	elf_aligned char align[];
} __attribute__((packed));

static const char vmcoreinfo_name[] = "VMCOREINFO";
static const char vmcoreinfo[] =
	"CRASHTIME=0";

struct vmcoreinfo_note {
	Elf64_Nhdr hdr;
	elf_aligned char name[sizeof(vmcoreinfo_name)];
	elf_aligned char data[sizeof(vmcoreinfo)];
	elf_aligned char align[];
} __attribute__((packed));

static int write_elfcorehdr(FILE *f, unsigned long long addr)
{
	static Elf64_Ehdr ehdr = {
		.e_ident = {
			ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3,
			ELFCLASS64, ELF_DATA, EV_CURRENT, ELFOSABI_NONE,
		},
		.e_type = ET_CORE,
		.e_machine = ELF_MACHINE,
		.e_version = EV_CURRENT,
		.e_phoff = sizeof(Elf64_Ehdr),
		.e_ehsize = sizeof(Elf64_Ehdr),
		.e_phentsize = sizeof(Elf64_Phdr),
		.e_phnum = 4,
	};
	Elf64_Phdr phdr;
	size_t sz;

	if (fwrite(&ehdr, sizeof(ehdr), 1, f) != 1) {
		perror("Ehdr");
		return 1;
	}

	/* Address just after program headers */
	addr += ehdr.e_phoff + ehdr.e_phnum * ehdr.e_phentsize;

	/* PRSTATUS note */
	sz = sizeof(struct prstatus_note);
	memset(&phdr, 0, sizeof(phdr));
	phdr.p_type = PT_NOTE;
	phdr.p_offset = addr;
	phdr.p_paddr = addr;
	phdr.p_filesz = sz;
	phdr.p_memsz = sz;
	if (fwrite(&phdr, sizeof(phdr), 1, f) != 1) {
		perror("PRSTATUS");
		return 1;
	}
	addr += phdr.p_filesz;

	/* VMCOREINFO note */
	sz = sizeof(struct vmcoreinfo_note);
	memset(&phdr, 0, sizeof(phdr));
	phdr.p_type = PT_NOTE;
	phdr.p_offset = addr;
	phdr.p_paddr = addr;
	phdr.p_filesz = sz;
	phdr.p_memsz = sz;
	if (fwrite(&phdr, sizeof(phdr), 1, f) != 1) {
		perror("VMCOREINFO");
		return 1;
	}
	addr += phdr.p_filesz;

	/* kernel text LOAD - not checked */
	memset(&phdr, 0, sizeof(phdr));
	phdr.p_type = PT_LOAD;
	phdr.p_flags = PF_R | PF_W | PF_X;
	if (fwrite(&phdr, sizeof(phdr), 1, f) != 1) {
		perror("kernel text LOAD");
		return 1;
	}

	/* directmap LOAD - not checked */
	memset(&phdr, 0, sizeof(phdr));
	phdr.p_type = PT_LOAD;
	phdr.p_flags = PF_R | PF_W | PF_X;
	if (fwrite(&phdr, sizeof(phdr), 1, f) != 1) {
		perror("directmap LOAD");
		return 1;
	}

	/* PRSTATUS content - not checked */
	struct prstatus_note prstatus_note;
	memset(&prstatus_note, 0, sizeof(prstatus_note));
	prstatus_note.hdr.n_namesz = sizeof(core_name);
	prstatus_note.hdr.n_descsz = sizeof(prstatus_t);
	prstatus_note.hdr.n_type = NT_PRSTATUS;
	strcpy(prstatus_note.name, core_name);
	if (fwrite(&prstatus_note, sizeof(prstatus_note), 1, f) != 1) {
		perror("PRSTATUS note");
		return 1;
	}

	/* VMCOREINFO content - not checked? */
	struct vmcoreinfo_note vmcoreinfo_note;
	memset(&vmcoreinfo_note, 0, sizeof(vmcoreinfo_note));
	vmcoreinfo_note.hdr.n_namesz = sizeof(vmcoreinfo_name);
	vmcoreinfo_note.hdr.n_descsz = sizeof(vmcoreinfo);
	strcpy(vmcoreinfo_note.name, vmcoreinfo_name);
	memcpy(vmcoreinfo_note.data, vmcoreinfo, sizeof(vmcoreinfo));
	if (fwrite(&vmcoreinfo_note, sizeof(vmcoreinfo_note), 1, f) != 1) {
		perror("VMCOREINFO note");
		return 1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	unsigned long long addr;
	char *endptr;
	FILE *f;
	int ret;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <filename> <load_address>\n",
			argv[0]);
		return 1;
	}

	addr = strtoull(argv[2], &endptr, 0);
	if (*endptr) {
		fprintf(stderr, "Invalid number: %s\n", argv[2]);
		return 1;
	}

	f = fopen(argv[1], "w");
	if (!f) {
		perror("fopen");
		return 1;
	}

	ret = write_elfcorehdr(f, addr);

	fclose(f);
	return ret;
}
