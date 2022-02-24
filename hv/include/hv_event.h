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

#ifndef __HV_EVENT_H__
#define __HV_EVENT_H__
#include <lk/console_cmd.h>

enum hv_event_id {
	HV_EVENT_VCPU_STOP = 1,
	HV_EVENT_VCPU_START,
	HV_EVENT_VCPU_POWEROFF,
	HV_EVENT_VCPU_POWERON,
	HV_EVENT_VM_POWEROFF,
	HV_EVENT_VCPU_MAX,
};

#define	HV_EVENT_SGI_NO   13                    /* use sgi 13 as hv event interrupts */	 


struct hv_event {
	short  e_id;     /*  event id  */
	char   buf[8];      /*  event len max  */
};

int  hv_send_event(uint32_t cpu, struct hv_event *evt);   /*  send event to dest cpu     */
enum handler_return hv_event_handler(void *args);       /*  hv event call back handler */
enum handler_return hv_event_empty_handler(void *args);
int hv_event_init(void);
int hv_event_test(int argc, const console_cmd_args *argv);
void  hv_empty_event(uint32_t cpu);
#endif 
