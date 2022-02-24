/* timer virtulization  
 * @author pwang
 * @mail wtliang110@aliyun.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

#ifndef __HV_VTIMER_H__
#define __HV_VTIMER_H__

/* vtimer context  */
struct arch_vtimer {
	uint64_t cntpct;    	/* system counter register */
	uint64_t cntvoff_el2;   /* cntvoff_el2  */
      	uint64_t cntvctl_el1;   /* cntvctl_el1 */
	uint64_t cntvcval_el1;	/* cntvcval_el1 timer value */
	uint64_t cntvtval_el1;  /* cntvcval_el1 compared value */
};

int hv_vtimer_init(void);
void hv_vcpu_vtimer_init(struct lk_vcpu *vcpu);
void hv_vtimer_context_save(struct lk_vcpu *vcpu);
void hv_vtimer_context_restore(struct lk_vcpu *vcpu);

#endif 
