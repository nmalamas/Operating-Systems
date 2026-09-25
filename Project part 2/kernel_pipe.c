
#include "tinyos.h"
#include "util.h"
#include "kernel_proc.h"
#include "kernel_streams.h"
#include "kernel_cc.h"

int falseCall(){
	return -1;
}

file_ops reader_file_ops = {
	.Open = NULL,
	.Read = pipe_read,
	.Write = falseCall,
	.Close = pipe_writer_close
};
file_ops writer_file_ops = {
	.Open = NULL,
	.Read = falseCall,
	.Write = pipe_write,
	.Close = pipe_writer_close

};
int sys_Pipe(pipe_t* pipe)
{
	Fid_t fid[2];
	FCB* fcb[2];
	if(!FCB_reserve(2,fid,fcb))return -1;	//Gets 2 fcbs if they are available 
	pipe->read = fid[0];
	pipe->write = fid[1];

	pipe_cb* picb =(pipe_cb*)xmalloc(sizeof(pipe_cb));
	picb->reader = fcb[0];
	picb->writer = fcb[1];
	picb->has_data = COND_INIT;
	picb->has_space = COND_INIT;
	picb->r_position = 0;
	picb->w_position = 0;
	picb->total_data = 0;

	picb->reader->streamobj = picb;
	picb->writer->streamobj = picb;
	picb->reader->streamfunc = &reader_file_ops;
	picb->writer->streamfunc = &writer_file_ops;
	return 0;
}

int pipe_read(void* pipecb_t, char* buf, unsigned int n){
	pipe_cb* pipe = (pipe_cb*)pipecb_t;	//Gets the pipe control block
	if(pipe->reader == NULL) return -1;	//Only reader has to be open 
	if(pipe->total_data == 0 && pipe->writer == NULL) return 0;	//Empty buffer and wirter closed so cant read

	while(pipe->total_data == 0 ){	//Empty buffer and writer open so wait for writer to put data in the buffer
		kernel_wait(&pipe->has_data,SCHED_USER);	//Wait until buffer has data
	}
	int bites_red;
	for(bites_red = 0; bites_red<n; bites_red++){
		
		buf[bites_red] = pipe->BUFFER[pipe->r_position]; //Read from buffer

		if(pipe->r_position == PIPE_BUFFER_SIZE -1){	
			pipe->r_position = 0;	//Reach end of cyclic buffer. Go to 0 position 
		}else{
			pipe->r_position++;
		}
		pipe->total_data--;	//After reading a char from the buffer we decrease the total_data by 1 

		if(pipe->total_data == 0){
			bites_red++;
			kernel_broadcast(&pipe->has_space);
			return bites_red;
		}
	}
	kernel_broadcast(&pipe->has_space);
	return bites_red;
}
int pipe_write(void* pipecb_t, const char* buf, unsigned int n){

	pipe_cb* pipe = (pipe_cb*)pipecb_t;
	if(pipe->writer == NULL || pipe->reader == NULL) return -1;	//Writer and reader need to be open

	while(pipe->total_data == PIPE_BUFFER_SIZE){
		kernel_wait(&pipe->has_space, SCHED_USER);
	}

	int bites_writen;
	for(bites_writen = 0; bites_writen < n; bites_writen++){
		
		pipe->BUFFER[pipe->w_position] = buf[bites_writen];

		if(pipe->w_position == PIPE_BUFFER_SIZE - 1){
			pipe->w_position = 0;
		}else{
			pipe->w_position++;
		}

		pipe->total_data++;

		if(pipe->total_data == PIPE_BUFFER_SIZE){	//Buffer is full. Can't write any more 
			bites_writen++;
			kernel_broadcast(&pipe->has_data);
			return bites_writen;
		}

	}
	kernel_broadcast(&pipe->has_data);
	return bites_writen;
}
int pipe_reader_close(void* _pipecb){
	pipe_cb* pipe = (pipe_cb*) _pipecb;
	pipe->reader = NULL;

	if(pipe->writer == NULL){
		pipe = NULL;	//Free the pipe because reader and writed are both closed 
	}
	return 0;
}
int pipe_writer_close(void* _pipecb){
	pipe_cb* pipe = (pipe_cb*) _pipecb;
	pipe->writer = NULL;

	if(pipe->reader == NULL){
		pipe = NULL;
	}
	return 0;
}