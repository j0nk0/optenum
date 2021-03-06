/*
Copyright (C) 2013 Matt Boyer.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of the project nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
*/

#include "getopt.h"

struct parsed_option_list *iterate_short_opts(bfd *binary_bfd, const bfd_vma shortopts) {
	asection *shortopt_section = find_vma_section(binary_bfd, shortopts);

	// Prepare a list of options for this call
	struct parsed_option_list *options_found = NULL;

	size_t sec_size = bfd_get_section_size(shortopt_section);
	bfd_byte *section_data = (bfd_byte *) xmalloc (sec_size);
	bfd_get_section_contents(binary_bfd, shortopt_section, section_data, 0, sec_size);
	debug("Loading the contents of section %s (%ld bytes)\n", shortopt_section->name, sec_size);

	size_t shortopt_offset = (shortopts - shortopt_section->vma);
	info("shortopts live in section %s at offset %ld\n", shortopt_section->name, shortopt_offset);


	bfd_byte *optstring = &section_data[shortopt_offset];
	info("Short opts %s\n", optstring);

	unsigned int opt_idx = 0;
	while (0x0 != optstring[opt_idx]) {

		char opt_char[2] = {optstring[opt_idx], 0x0};

		bool has_arg = false;
		// Does this option require an argument?
		if (':' == optstring[opt_idx+1]) {
			has_arg = false;
			opt_idx++;
		}
		options_found = append_option(options_found, opt_char, has_arg, ONE_DASH);
		opt_idx++;
	}

	free(section_data);
	return options_found;
}

struct parsed_option_list *iterate_long_opts(bfd *binary_bfd, const bfd_vma longopts) {

	asection *longopt_section = find_vma_section(binary_bfd, longopts);

	// Prepare a list of options for this call
	struct parsed_option_list *options_found = NULL;

	size_t sec_size = bfd_get_section_size(longopt_section);
	bfd_byte *section_data = (bfd_byte *) xmalloc (sec_size);
	bfd_get_section_contents(binary_bfd, longopt_section, section_data, 0, sec_size);
	debug("Loading the contents of section %s (%ld bytes)\n", longopt_section->name, sec_size);

	size_t longopt_offset = (longopts - longopt_section->vma);
	info("longopts live in section %s at offset %ld\n", longopt_section->name, longopt_offset);

	const char *all_zeroes = calloc(1, sizeof(struct option));

	while (longopt_offset < longopt_section->size) {

		/* As per the man page for getopt_long(3), the last element in
		 * the array of struct option elements has to be all zeroes
		 */
		if (0==memcmp(&section_data[longopt_offset], all_zeroes, sizeof(struct option)))
			break;

		struct option *long_option = (struct option*) &section_data[longopt_offset];

		if (NULL==long_option->name)
			break;


		debug("struct option @ section offset %ld\t loaded into %016" PRIXPTR "\n", longopt_offset, (uintptr_t) long_option);

		// FIXME We're assuming that pointers hardcoded into the binary can
		// safely be cast into bfd_vma, which is unsafe

		// We can't assume member 'name' of the struct option will point to an
		// area in the same section of the ELF binary as the struct option
		// itself
		bfd_byte *option_name_section_data = NULL;
		asection *option_name_section = find_vma_section(binary_bfd, (bfd_vma) long_option->name);
		if (!option_name_section)
			break;
		debug("option name lives in section %s\n", option_name_section->name);
		if (option_name_section==longopt_section) {
			option_name_section_data = section_data;
		} else {
			size_t option_name_sec_size = bfd_get_section_size(option_name_section);
			option_name_section_data = (bfd_byte *) xmalloc (option_name_sec_size);
			bfd_get_section_contents(binary_bfd, option_name_section, option_name_section_data, 0, option_name_sec_size);
		}

		size_t name_offset = ( (bfd_vma)long_option->name - option_name_section->vma);

		debug("option name @ %016" PRIXPTR ": %s\n", (uintptr_t) long_option->name, &option_name_section_data[name_offset]);

		options_found = append_option(options_found, 
			(const char*) &option_name_section_data[name_offset], 
			(bool) (1==long_option->has_arg), TWO_DASH);

		if (option_name_section != longopt_section)
			free(option_name_section_data);

		longopt_offset += sizeof(struct option);
	}

	debug("Last struct option in array at offset %ld vma %016" PRIXPTR "\n", longopt_offset, longopt_section->vma+longopt_offset);
	free(section_data);

	return options_found;
}

/* vim:set tabstop=4 softtabstop=4 shiftwidth=4 noexpandtab : */
