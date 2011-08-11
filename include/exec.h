#ifndef __EXEC_H__
#define __EXEC_H__

#include "communication.h"

extern void serve_request(registry_msg* r, int cl_fd, struct sockaddr_un cl_addr, int cl_len);
#endif
