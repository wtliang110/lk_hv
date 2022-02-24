/* @author pwang
 * @mail wtliang110@aliyun.com
 *
 *  * this program is free software; you can redistribute it and/or modify
 * it under the terms of the gnu general public license as published by
 * the free software foundation; either version 2 of the license, or
 * (at your option) any later version.
 *
 * this program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * merchantability or fitness for a particular purpose.  see the
 * gnu general public license for more details.
*/

#ifndef __VM_CONFIG__
#define __VM_CONFIG__
#include <sys/types.h>
#include <libfdt.h>

char *get_root_config(int *len);
int lkhv_parse_vmcfg(char *vmcfg);
int fdt_get_int_value(void *fdt, int offset, char *prop, int *val);
u64 fdt_next_cell(int s, fdt32_t **cellp);
int get_node_offset(void *dtb, char *path, char *nd_name);
#endif 
