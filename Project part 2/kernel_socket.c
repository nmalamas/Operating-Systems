
#include "tinyos.h"
#include "kernel_cc.h"
#include "kernel_proc.h"
#include "kernel_dev.h"
#include "kernel_streams.h"

socket_cb* PORT_MAP[MAX_PORT] = {NULL};

int socket_read(void* socket_cb_t, char* buf, unsigned n){
	socket_cb* peer = (socket_cb*) socket_cb_t;
	if(peer->type != SOCKET_PEER || peer->peer_s.read_pipe == NULL){
		return -1;
	}else{
		int bytes = pipe_read(peer->peer_s.read_pipe,buf,n);
		return bytes;
	}	
}
int socket_write(void* socket_cb_t, const char* buf, unsigned n){
	socket_cb* peer = (socket_cb*) socket_cb_t;
	if(peer->type != SOCKET_PEER || peer->peer_s.write_pipe == NULL){
		return -1;
	}else{
		int bytes = pipe_write(peer->peer_s.write_pipe,buf,n);
		return bytes;
	}	
}
int socket_close(void* socket_cb_t){
	socket_cb* socket = (socket_cb*) socket_cb_t;
	if(socket->refcount == 0){	//If refcount is 0 then i can erase 
		if(socket->type == SOCKET_LISTENER){
			PORT_MAP[socket->port] = NULL;
		}else if(socket->type == SOCKET_PEER){
			//SHUTDOWN DOTH
			if(socket->peer_s.write_pipe != NULL){
				pipe_writer_close(socket->peer_s.write_pipe);
				socket->peer_s.write_pipe = NULL;
			}
			if(socket->peer_s.read_pipe != NULL){
				pipe_reader_close(socket->peer_s.read_pipe);
				socket->peer_s.read_pipe = NULL;
			}	
		}
		free(socket);
		return 0;
	}else{
		socket->refcount--;
		if(socket->type == SOCKET_LISTENER){
			PORT_MAP[socket->port] = NULL;
			kernel_broadcast(&socket->listener_s.req_available);
		}
		return 0;
	}
}

file_ops socket_file_ops = {
	.Open = NULL,
	.Read = socket_read,
	.Write = socket_write,
	.Close = socket_close
};

Fid_t sys_Socket(port_t port)
{
	if(port > MAX_PORT || port < 0){
		return NOFILE;
	}
	FCB* fcb;
	Fid_t fid;
	if(!FCB_reserve(1, &fid, &fcb)){
		return NOFILE;
	}
	socket_cb* socket = (socket_cb*)xmalloc(sizeof(socket_cb));
	socket->port = port;
	socket->refcount = 0;
	socket->type = SOCKET_UNBOUND;
	socket->fcb = fcb;

	fcb->streamfunc = &socket_file_ops;
	fcb->streamobj = socket;
	
	return fid;
}

int sys_Listen(Fid_t sock)
{

	FCB* fcb = get_fcb(sock);
	if(fcb == NULL || fcb->streamfunc != &socket_file_ops){	//Check fcb exist and the streamobj is a socket
		return -1;
	}
	socket_cb* socket = (socket_cb*) fcb->streamobj;
	if( socket->port == NOPORT || PORT_MAP[socket->port] != NULL || socket->type != SOCKET_UNBOUND){	//Check the socket can be turned to a listener
		return -1;
	}
	PORT_MAP[socket->port] = socket;	//Add the the listener socket to the port map
	socket->type = SOCKET_LISTENER;
	rlnode_init(&socket->listener_s.queue,NULL);	//Initialize list
	socket->listener_s.req_available = COND_INIT;	//Initialize condition var
	return 0;
}


Fid_t sys_Accept(Fid_t lsock)
{
	FCB* socketfcb = get_fcb(lsock);	//Gets fcb from fid
	if(socketfcb == NULL || socketfcb->streamfunc != &socket_file_ops){
		return NOFILE;
	}
	socket_cb* socket = (socket_cb*) socketfcb->streamobj;	//Get socket from fcb 
	if(socket->type != SOCKET_LISTENER){
		return NOFILE;
	}
	socket->refcount++;	
	while(is_rlist_empty(&socket->listener_s.queue) && socket->refcount!=0) {
		kernel_wait(&socket->listener_s.req_available, SCHED_USER);
	}
	if(PORT_MAP[socket->port] == NULL){
		free(socket);
		return NOFILE;
	}
	rlnode* acceptedNode = rlist_pop_front(&socket->listener_s.queue);	//Get a node from the list 
	connection_request* req = (connection_request*) acceptedNode->req;	//Turn the node to a connection request 
	socket_cb* accepted = req->peer;	//Get the socket that was sent
	if(accepted->type != SOCKET_UNBOUND){	
		return NOFILE;
	}

	Fid_t fid = sys_Socket(socket->port);	//Create a new socket 
	if(fid == NOFILE){
		kernel_signal(&req->connected_cv);
		return NOFILE;
	}
	FCB* fcb = get_fcb(fid);
	socket_cb* connected = (socket_cb*) fcb->streamobj;
	accepted->peer_s.peer = connected;
	connected->peer_s.peer = accepted;
	//Create two pipes
	pipe_cb* pipe1 = (pipe_cb*) xmalloc(sizeof(pipe_cb));
	pipe_cb* pipe2 = (pipe_cb*) xmalloc(sizeof(pipe_cb));
	pipe1->reader = accepted->fcb;
	pipe1->writer = connected->fcb;
	pipe1->r_position = 0;
	pipe1->w_position = 0;
	pipe1->has_data = COND_INIT;
	pipe1->has_space = COND_INIT;
	pipe1->total_data = 0;
	pipe2->reader = connected->fcb;
	pipe2->writer = accepted->fcb;
	pipe2->r_position = 0;
	pipe2->w_position = 0;
	pipe2->has_data = COND_INIT;
	pipe2->has_space = COND_INIT;
	pipe2->total_data = 0;
	//Connect the pipes to the sockets
	accepted->peer_s.read_pipe = pipe1;
	accepted->peer_s.write_pipe = pipe2;
	accepted->type = SOCKET_PEER;	//Now the socket type is peer 
	connected->peer_s.read_pipe = pipe2;
	connected->peer_s.write_pipe = pipe1;
	connected->type = SOCKET_PEER;

	req->admitted =1;	//Request was admitted 
	kernel_signal(&req->connected_cv);	//Signal the connection request that it was connected
	socket->refcount--;
	return fid;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	FCB* fcb = get_fcb(sock);
	if(fcb == NULL|| fcb->streamfunc !=&socket_file_ops || port > MAX_PORT || port < 0){
		return -1;
	}
	socket_cb* connect = (socket_cb*) fcb->streamobj;
	socket_cb* listener = PORT_MAP[port];	//Get listener socket from port map 
	if(connect->type != SOCKET_UNBOUND || listener == NULL || listener->type != SOCKET_LISTENER){
		return -1;
	}
	connect->refcount++;
	//Create connection request 
	connection_request* req = (connection_request*) xmalloc(sizeof(connection_request));
	req->admitted = 0;
	req->connected_cv = COND_INIT;
	req->peer = connect;	//The socket we want to send 
	rlnode_init(&req->queue_node, req);
	rlist_push_back(&listener->listener_s.queue, &req->queue_node);	//Put the con_req in the listener queue list
	kernel_signal(&listener->listener_s.req_available);	//Signal the listener 

	kernel_timedwait(&req->connected_cv,SCHED_USER,timeout);	//Wait until woken up or timeout 

	int admitted = req->admitted;
	free(req);
	if(connect->refcount == 0){
		free(connect);
		return -1;
	}else{
		connect->refcount--;
		if(admitted==0){	//if admitted is 0 then no connection 
			return -1;
		}else{	//If admitted is 1 socket was connected 
			return 0;
		}	
	}
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	FCB* fcb = get_fcb(sock);
	if(fcb == NULL || fcb->streamfunc != &socket_file_ops){
		return -1;
	}
	socket_cb* peer = (socket_cb*) fcb->streamobj;
	if(peer->type != SOCKET_PEER){
		return -1;
	}
	switch (how)
	{
	case SHUTDOWN_WRITE:
		if(peer->peer_s.write_pipe != NULL){
			pipe_writer_close(peer->peer_s.write_pipe);
			peer->peer_s.write_pipe = NULL;
		}
		break;
	case SHUTDOWN_READ:
		if(peer->peer_s.read_pipe != NULL){
			pipe_reader_close(peer->peer_s.read_pipe);
			peer->peer_s.read_pipe = NULL;
		}
		break;
	case SHUTDOWN_BOTH:
		if(peer->peer_s.write_pipe != NULL){
			pipe_writer_close(peer->peer_s.write_pipe);
			peer->peer_s.write_pipe = NULL;
		}
		if(peer->peer_s.read_pipe != NULL){
			pipe_reader_close(peer->peer_s.read_pipe);
			peer->peer_s.read_pipe = NULL;
		}
		break;	
	default:
		return -1;
		break;
	}

	return 0;
}


