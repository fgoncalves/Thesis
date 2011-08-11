#ifndef __ADDR_CONVERT_H___
#define __ADDR_CONVERT_H___

static inline uint16_t dot2int(uint8_t node, uint8_t hid){
	return ((uint16_t) node) * 256 + ((uint16_t) hid);
}

static inline void int2dot(uint16_t dot, uint8_t *node, uint8_t *hid){
	*node = ((uint8_t) (dot / 256));
	*hid = ((uint8_t) (dot % 256));
}

#endif
