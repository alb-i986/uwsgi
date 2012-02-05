#include "../../uwsgi.h"

extern struct uwsgi_server uwsgi;

int uwsgi_routing_func_uwsgi_simple(struct wsgi_request *wsgi_req, struct uwsgi_route *ur) {

	struct uwsgi_header *uh = (struct uwsgi_header *) ur->data;

	wsgi_req->uh.modifier1 = uh->modifier1;
	wsgi_req->uh.modifier2 = uh->modifier2;

	return 0;
}

int uwsgi_routing_func_uwsgi_remote(struct wsgi_request *wsgi_req, struct uwsgi_route *ur) {

	char buf[8192];
	ssize_t len;
	struct uwsgi_header *uh = (struct uwsgi_header *) ur->data;
	char *addr = ur->data + sizeof(struct uwsgi_header);
	
	// mark a route request
        wsgi_req->status = -17;

	int uwsgi_fd = uwsgi_connect(addr, uwsgi.shared->options[UWSGI_OPTION_SOCKET_TIMEOUT], 0);
	if (uwsgi_fd < 0) {
		uwsgi_log("unable to connect to host %s\n", addr);
		return 1;
	}

	if (uwsgi_send_message(uwsgi_fd, uh->modifier1, uh->modifier2, wsgi_req->buffer, wsgi_req->uh.pktsize, wsgi_req->poll.fd, wsgi_req->post_cl, 0) < 0) {
		uwsgi_log("unable to send uwsgi request to host %s", addr);
		return 1;
	}

	for(;;) {
		int ret = uwsgi_waitfd(uwsgi_fd, uwsgi.shared->options[UWSGI_OPTION_SOCKET_TIMEOUT]);
        	if (ret > 0) {
          		len = read(uwsgi_fd, buf, 8192);
			if (len == 0) {
				break;
			}
			else if (len < 0) {
				uwsgi_error("read()");
				break;
			}

			if (write(wsgi_req->poll.fd, buf, len) != len) {
				uwsgi_error("write()");
				break;
			}	
		}
		else {
			uwsgi_log("timeout !!!\n");
			break;
		}
	}


	close(uwsgi_fd);
	return 1;

}

int uwsgi_router_uwsgi(struct uwsgi_route *ur, char *args) {

	// check for commas
	char *comma1 = strchr(args, ',');
	if (!comma1) {
		uwsgi_log("invalid route syntax\n");
		exit(1);
	}

	char *comma2 = strchr(comma1+1, ',');
	if (!comma2) {
		uwsgi_log("invalid route syntax\n");
		exit(1);
	}

	*comma1 = 0;
	*comma2 = 0;

	// simple modifier remapper
	if (*args == 0) {
		struct uwsgi_header *uh = uwsgi_calloc(sizeof(struct uwsgi_header));
		ur->func = uwsgi_routing_func_uwsgi_simple;
		ur->data = (void *) uh;

		uh->modifier1 = atoi(comma1+1);
		uh->modifier2 = atoi(comma2+1);
		return 0;
	}
	else {
		struct uwsgi_header *uh = uwsgi_calloc(sizeof(struct uwsgi_header) + strlen(args) + 1);
		ur->func = uwsgi_routing_func_uwsgi_remote;
		ur->data = (void *) uh;

		uh->modifier1 = atoi(comma1+1);
		uh->modifier2 = atoi(comma2+1);

		void *ptr = (void *) uh;
		strcpy(ptr+sizeof(struct uwsgi_header), args);
		return 0;

	}

	return -1;
}


void router_uwsgi_register(void) {

	uwsgi_register_router("uwsgi", uwsgi_router_uwsgi);
}

struct uwsgi_plugin router_uwsgi_plugin = {

	.name = "router_uwsgi",
	.on_load = router_uwsgi_register,
};