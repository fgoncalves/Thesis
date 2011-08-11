#ifndef __TRANSPORT_H__
#define __TRANSPORT_H__

//###  Utility macro, which avoids name clashes. ###
//---
//> Each word wrapped in \_\_tp(W) will automatically be converted to transport_layer_W.
//> Developpers are free to use this macro as well as the "transport_layer_" version.
#define __tp(NAME) transport_layer_ ## NAME







#define MAX_ID_LEN 20

#endif
