#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/netfilter_ipv4.h>
#include <thread>
#include <string>
int socket_fd;
const int one = 1;
int init_fd(const int listen_port,const int backlog) {
	socket_fd = socket(AF_INET,SOCK_STREAM,0);
	if (socket_fd < 0) return socket_fd;
	int error;
	error = setsockopt(socket_fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(int));
	if (error) return error;
	struct sockaddr_in server_sockaddr;
	server_sockaddr.sin_family = AF_INET;
	server_sockaddr.sin_port = htons(listen_port);
	server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	error = bind(socket_fd,(struct sockaddr* )&server_sockaddr,sizeof(server_sockaddr));
	if (error == -1) return error;
	error = listen(socket_fd,backlog);
	if (error == -1) return error;
	
	return 0;
}
char* newstr(const char *src) {
	int len = strlen(src);
	char *res = (char*)malloc(len+1);
	memcpy(res,src,len+1);
	return res;
}
void forward(const int fd_x,const int fd_y) {//fd_x->fd_y
	const int buf_sz = 2048;
	char buf[buf_sz];
	ssize_t sz;
	while ((sz = recv(fd_x,buf,buf_sz,0)) > 0) {
		if (send(fd_y,buf,sz,0) < 0) break;
	}
	close(fd_x);
	close(fd_y);
}
void client_socket(const int client_fd,sockaddr_in destaddr) {
	int new_socket = socket(AF_INET,SOCK_STREAM,0);
	if (connect(new_socket,(sockaddr*)&destaddr,sizeof(struct sockaddr_in))) {
		printf("[ERROR] Destnation socket failed.\n");
		close(new_socket);
		close(new_socket);
	}
	else {
		printf("[INFO] Destnation socket connected.\n");
		std::thread tx(forward,new_socket,client_fd);
		std::thread rx(forward,client_fd,new_socket);
		tx.join();
		rx.join();
		printf("[INFO] Connection Closed.\n");
	}
}
void accept_client() {
	struct sockaddr_in clientaddr,myaddr,destaddr;
	socklen_t addrlen = sizeof(struct sockaddr_in);
	int client_fd = accept(socket_fd,(struct sockaddr*)&clientaddr,&addrlen);
	if (client_fd == -1) {
		printf("[ERROR] accept return %d:",client_fd);
		perror("");
		printf("\n");
		close(socket_fd);
		exit(1);
		return;
	}
	int error;
	error = getsockname(client_fd, (struct sockaddr*)&myaddr,&addrlen);
	if (error) {
		printf("[ERROR] getsockname return %d\n",error);
		perror("");
		printf("\n");
		return;
	}
	error = getsockopt(client_fd, SOL_IP, SO_ORIGINAL_DST,&destaddr,&addrlen);
	if (error) {
		printf("[ERROR] getsockopt return %d:",error);
		perror("");
		printf("\n");
		return;
	}
	char *clientip = newstr(inet_ntoa(clientaddr.sin_addr)),*myip = newstr(inet_ntoa(myaddr.sin_addr)),*dstip = newstr(inet_ntoa(destaddr.sin_addr));
	if (myaddr.sin_port == destaddr.sin_port) {
		//to avoid fork bomb, so you should bind to an incommon port.
		printf("[ERROR] Self loop detected %s:%d -> %s:%d -> %s:%d\n",
			clientip,ntohs(clientaddr.sin_port),
			myip,ntohs(myaddr.sin_port),
			dstip,ntohs(destaddr.sin_port)
		);
		close(client_fd);
	}
	else {
		printf("[INFO] Client from %s:%d -> %s:%d -> %s:%d\n",
			clientip,ntohs(clientaddr.sin_port),
			myip,ntohs(myaddr.sin_port),
			dstip,ntohs(destaddr.sin_port)
		);
		std::thread t(client_socket,client_fd,destaddr);
		t.detach();
	}
	free(clientip);
	free(myip);
	free(dstip);
}
void signnal_process(sig_t s){
	printf("[ERROR] caught signal %d\n",s);
	close(socket_fd);
	exit(1); 
}
int main(int argc,char *argv[]) {
	int listen_port = 12345;
	for (int i=0;i<argc;i++) {
		if (std::string(argv[i]) == "-l") {
			if (i + 1 < argc && atoi(argv[i+1]) > 0 && atoi(argv[i+1]) < 65536) {
				listen_port = atoi(argv[i+1]);
			} 
		}
	}
	if (init_fd(listen_port,1024)) {
		perror("[ERROR] listen failed:");
		return -1;
	}
	printf("[INFO] Listening port %d\n",listen_port);
	signal(SIGINT,(__sighandler_t)signnal_process);
	while (1) accept_client();
	return 0;
}
