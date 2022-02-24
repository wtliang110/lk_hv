
/* @author pwang
 * @mail wtliang110@aliyun.com
 *
 *  * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

#ifndef __HV_IMAGE__
#define __HV_IMAGE__


#if WITH_ELF32
typedef     struct Elf32_Ehdr eheader;    // a copy of the main elf header
typedef     struct Elf32_Phdr pheader;   // a pointer to a buffer of program headers
#else
typedef     struct Elf64_Ehdr eheader;    // a copy of the main elf header
typedef     struct Elf64_Phdr pheader;   // a pointer to a buffer of program headers
#endif

#define PT_LOAD    1

int vm_load_fit(struct lk_vm *vm);
int vm_load_elf(struct lk_vm *vm);
int vm_load_fdt(struct lk_vm *vm);

#endif 
