#include<functional>
#include<sys/socket.h> 
#include<netinet/in.h> 
#include<arpa/inet.h> 
#include<stdio.h>
#include<stdlib.h>
#include<string>
#include <cstring>
#include<sys/types.h>
#include<sys/stat.h>    
#include<fcntl.h>
#include<sys/timeb.h>
#include<netdb.h>
#include<unistd.h>
using namespace std;

int packet_num=0;
int time_out=1000;
int nextone=1;

long long gettime(){//https://blog.csdn.net/kangruihuan/article/details/59055801
	timeb t;
	ftime(&t);
	return t.time * 1000 + t.millitm;
}



int main(int argc, char *argv[]){
	if(strcmp(argv[1],"-n")==0){
		packet_num=stoi(argv[2]); nextone=3;
	}
	else if(strcmp(argv[1],"-t")==0){
		time_out=stoi(argv[2]); nextone=3;
	}
	if(argc>3){
		if(strcmp(argv[3],"-n")==0){
			packet_num=stoi(argv[4]); nextone=5;
		}
		else if(strcmp(argv[3],"-t")==0){
			time_out=stoi(argv[4]); nextone=5;
		}
	}
	for(int i=nextone;i<argc;i++){//依序處理參數裡的 [ip_addr : port]
		char help[256];
		strcpy(help,argv[i]);
		char delim[] = ":";
		char *hostip;
		hostip=strtok(help,delim);
		int hostport=stoi(strtok(NULL,delim));
		int fd;
		if((fd=socket(AF_INET,SOCK_STREAM,0))<0){//create a socket
			perror("socket");
			exit(1);
		}
		struct sockaddr_in srv;
		srv.sin_family=AF_INET;
		srv.sin_port=hostport;
		srv.sin_addr.s_addr=inet_addr(hostip);
		if(srv.sin_addr.s_addr==-1){
			struct hostent *host=new struct hostent;
			host=gethostbyname(hostip);
			if(host==NULL) perror("oops");
			srv.sin_addr=*((struct in_addr *)host->h_addr);
		}
		char *t=inet_ntoa(srv.sin_addr);
		//printf("%s",t);
		if((connect(fd, (struct sockaddr*) &srv,sizeof(srv)))<0){//connect the socket
			printf("timeout when connect to %s:%hu\n",t,srv.sin_port); 
			continue;
		}
		int nbytes;
		char buf[512];
		sprintf(buf, "%d", packet_num);
		long long start=gettime();
		for(int i=(packet_num==0)? -1:0;i<packet_num;i++){
			long long ith_packet=gettime();
			if((nbytes=write(fd, buf, sizeof(buf)))<0){//"read" will block until receive data 
				printf("timeout when connect to %s:%hu\n",t,srv.sin_port); 
				break;
			}
			if((nbytes=read(fd, buf, sizeof(buf)))<0){//"read" will block until receive data 
				printf("timeout when connect to %s:%hu\n",t,srv.sin_port); 
				break;
			}
			long long end=gettime();
			if(end-start<=time_out)
				printf("recv from %s:%hu, RTT = %lld msec\n",t,srv.sin_port,end-ith_packet);
			else{
				printf("timeout when connect to %s:%hu\n",t,srv.sin_port);
				break;
			}
			if(packet_num==0) i--;
		} 	
		close(fd);
	}

}