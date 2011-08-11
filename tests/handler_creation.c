#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include <fred/handler.h>
#include <fred/timestamp.h>

void receive(__tp(handler)* sk, char* data, uint16_t len, int64_t timestamp, int64_t air,
	     uint16_t src_id) {
	static int i = 1;
	printf("%lld\t%d\t'%s'\n", timestamp, i++, data);
}

int main(int argc, char** argv) {
	char op;
	int i;
	struct timespec ts;
	int64_t before, after;
	__tp(handler)* handler;
	//argv[1] seria um ficheiro xml com a configuracao do handler.
	//argv[2] number of packets
	//argv[3] send
	switch (argc) {
	case 3: {
		handler = __tp(create_handler_based_on_file)(argv[1], receive);
		scanf("%c", &op);
		break;
	}
	case 5: {
		handler = __tp(create_handler_based_on_file)(argv[1], receive);
		i = atoi(argv[2]);
		while (i--) {
			send_data(handler, "hello world!!", strlen("hello world!!"), atoi(argv[4]));
			clock_gettime(CLOCK_REALTIME, &ts);
			before = timespec_to_ns(ts);
			usleep(40);
			clock_gettime(CLOCK_REALTIME, &ts);
			after = timespec_to_ns(ts);
			printf("Actually sleep time %lld us.\n", (after - before) / 1000);
		}
		scanf("%c", &op);
		printf("finished\n");
		break;
	}
	default:
		printf("Usage: %s <xml> <number of packets> [-s dst_id]", argv[0]);
		return -1;
	}

	__tp(free_handler)(handler);
	return 0;
}
