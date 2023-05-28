#include <net/if.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
 
 
using namespace std;
 
int main()
{	
	//得到套接字描述符
	int sockfd;		

    /* code */
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	//socket(int domain, int type, int protocol);
	//domain: 指定协议域，AF_INET（IPV4的网络协议）, AF_INET6(IPV6的网络协议), AF_LOCAL（Unix域协议）, AF_ROUTE（路由套接字）, AF_KEY（秘钥套接字）
	//type: 指定套接字类型，SOCK_STREAM（字节流套接字）, SOCK_DGRAM（数据报套接字）, SOCK_RAW（原始套接字）, SOCK_PACKET（Linux特有的数据链路层套接字）
	//protocol: 指定协议，IPPROTO_TCP（TCP传输协议）, IPPROTO_UDP（UDP传输协议）, IPPROTO_SCTP（STCP传输协议）, IPPROTO_TIPC（TIPC传输协议）, 0（自动选择协议）
	if(sockfd == -1){
		printf("Socket Error!\n");
		exit(-1);
	}
	printf("Sockfd : %d\n", sockfd);
	
	/*
	ifconf 和 ifreq结构：
	1.ifconf结构用于获取所有接口信息，它会作为输入输出参数
	传递给ioctl()系统调用，里面的ifc_len表示缓冲区的长度，ifc_buf
	是一个指向存储接口信息的缓冲区的指针，ioctl()系统调用
	会把接口信息填充到ifc_buf指向的缓冲区中。
	它含有若干个ifreq结构，每个ifreq结构用于保存一个接口的信息。
	2.ifreq结构用于获取指定接口的信息，它也会作为输入输出参数
	*/


	struct ifconf ifc;
	caddr_t buf;
	int len = 100;
	
	//初始化ifconf结构
	ifc.ifc_len = 1024;
	if ((buf = (caddr_t)malloc(1024)) == NULL)
	{
		cout << "malloc error" << endl;
		exit(-1);
	}
	ifc.ifc_buf = buf; 
	
	//获取所有接口信息
	
    /* code */
	if(ioctl(sockfd, SIOCGIFCONF, &ifc) == -1){
		printf("ioctl error!\n");
		exit(-1);
	}

	//遍历每一个ifreq结构
	struct ifreq *ifr;
	struct ifreq ifrcopy;
	ifr = (struct ifreq*)buf;
	for(int i = (ifc.ifc_len/sizeof(struct ifreq)); i>0; i--)
	{
		//接口名
		cout << "interface name: "<< ifr->ifr_name << endl;
		//ipv4地址
		ifrcopy = *ifr;
		if(ioctl(sockfd, SIOCGIFCONF, &ifrcopy) == -1){
			printf("ioctl error!\n");
			exit(-1);
		}
		cout << "inet addr: " 
			 << inet_ntoa(((struct sockaddr_in*)&(ifrcopy.ifr_addr))->sin_addr)
			 << endl;
		//inet_ntoa()函数将网络字节序的二进制IP地址转换成点分十进制IP地址
		//获取广播地址
		ifrcopy = *ifr;
		/* code */
		if(ioctl(sockfd, SIOCGIFBRDADDR, &ifrcopy) == -1){
			printf("ioctl error!\n");
			exit(-1);
		}
		cout << "broad addr: "
			 << inet_ntoa(((struct sockaddr_in*)&(ifrcopy.ifr_broadaddr))->sin_addr)
			 << endl;
		//获取mtu
		ifrcopy = *ifr;
		
        /* code */
		if(ioctl(sockfd, SIOCGIFMTU, &ifrcopy) == -1){
			printf("ioctl error!\n");
			exit(-1);
		}
		cout << "mtu: " << ifrcopy.ifr_mtu << endl;
		cout << endl;
		ifr++;
	}
	
	return 0;
}