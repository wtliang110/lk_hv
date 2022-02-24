#ifndef __HV_REG_H__
#define __HV_REG_H__

/* bit of HCR_EL2  */
#define HCR_AMO (1 << 5)
#define HCR_IMO (1 << 4)
#define HCR_FMO (1 << 3)
#define HCR_PTW (1 << 2)
#define HCR_SWIO (1 << 1)
#define HCR_VM   (1 << 0)
#define HCR_VSE  (1 << 8)

#define HCR_INT_TRAP (HCR_AMO|HCR_IMO|HCR_FMO)

#endif 
