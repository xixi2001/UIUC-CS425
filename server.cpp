#include  <functional>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include<stdio.h>
#include<stdlib.h>
#include<string>
#include <sys/types.h>
#include <sys/stat.h>    
#include <fcntl.h>
#include <unistd.h>

using namespace std;
char buf[512];
int packet_num=0;

int main(int argc, char *argv[]){
	int listen_port=stoi(argv[1]);
	int fd;
	if((fd=socket(AF_INET,SOCK_STREAM,0))<0){//open a socket
		perror("socket");
		exit(1);
	}
	struct sockaddr_in srv;
	srv.sin_family=AF_INET;
	srv.sin_port=listen_port;
	srv.sin_addr.s_addr=htonl(INADDR_ANY);//client連到這台電腦的任意網卡，只要port對，server就收
	if((bind(fd, (struct sockaddr*) &srv,sizeof(srv)))<0){
		perror("bind");
		exit(1);
	}
	if( listen(fd,10)<0 ){
		perror("listen");
		exit(1);
	}
	struct sockaddr_in cli;
	int clifd;//新的file descripter: 不佔用最初連線的那個socket=>可以再accept別的connection
	socklen_t cli_len=sizeof(cli);
	while(1){
		clifd=accept(fd, (struct sockaddr*) &cli, &cli_len);//block until accept connection
		if(clifd<0){
			perror("accept");
			exit(1);
		}
		int nbytes;
		if((nbytes=read(clifd, buf, sizeof(buf)))<0){//"read" will block until receive data 
			perror("read");
		}
		packet_num=stoi(buf);
		if((nbytes=write(clifd, buf, sizeof(buf)))<0){//"read" will block until receive data 
			perror("write");
		}
		char *t=inet_ntoa(cli.sin_addr);
		printf("recv from %s:%hu\n",t, cli.sin_port);//lu: unsigned long; hu: unsigned short	
		for(int i=(packet_num==0)? -2:0;i<packet_num-1;i++){
			if((nbytes=read(clifd, buf, sizeof(buf)))<0){//"read" will block until receive data 
				break;
			}
			packet_num=stoi(buf);
			if((nbytes=write(clifd, buf, sizeof(buf)))<0){//"read" will block until receive data 
				break;
			}
			printf("recv from %s:%hu\n",t, cli.sin_port);//lu: unsigned long; hu: unsigned short		
			if(packet_num==0) i--;
		}
		close(clifd);
	}
	close(fd);
	return 0;
}


