#include <bits/sockaddr.h>
#include <asm/types.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
int main()
{

	struct nt_req {
		struct nlmsghdr nl;
		struct rtmsg rt;
		char buf[8192];
	};

	int sock;
	struct sockaddr_nl src_addr;
	struct sockaddr_nl dst_addr;
	struct msghdr msg;
	struct iovec iov;
	int rtn;

	char buf[8192];

	struct nlmsghdr *nlp;
	int bytes_received;
	struct rtmsg *rtp;
	int rtl;
	struct rtattr *rtap;
	struct nt_req req;
	/* Open socket for NETLINK_ROUTE */
	sock=socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if(sock < 0) {
		puts("Failed to create socket");
	}
	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	/* Bind using the local process id */
	src_addr.nl_pid = getpid(); 
	src_addr.nl_pad = 0;
	/* If multicast is needed */
	src_addr.nl_groups = 0;


	if(bind(sock, (struct sockaddr*)&src_addr, sizeof(src_addr)) < 0) {
		puts("Failed to bind to socket");
	};


	// initialize the nt_request buffer
	memset(&req, 0, sizeof(req));
	// set the NETLINK header
	req.nl.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	req.nl.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	req.nl.nlmsg_type = RTM_GETROUTE;

	// set the routing message header
	req.rt.rtm_family = AF_INET;
	req.rt.rtm_table = RT_TABLE_MAIN;
	memset(&dst_addr, 0, sizeof(dst_addr));

	dst_addr.nl_family = AF_NETLINK;

	// initialize & create the struct msghdr supplied
	// to the sendmsg() function
	memset(&msg, 0, sizeof(msg));
	msg.msg_name = (void *) &dst_addr;
	msg.msg_namelen = sizeof(dst_addr);

	// place the pointer & size of the RTNETLINK
	// message in the struct msghdr
	iov.iov_base = (void *) &req.nl;
	iov.iov_len = req.nl.nlmsg_len;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	// send the RTNETLINK message to kernel
	if(sendmsg(sock, &msg, 0) < 0) {
		puts("sendmsg() failed");
	};
	char *p;

	// initialize the socket read buffer
	memset(buf, 0, sizeof(buf));

	p = buf;
	bytes_received = 0;

	// read from the socket until the NLMSG_DONE is
	// returned in the type of the RTNETLINK message
	// or if it was a monitoring socket
	while(1) {
		rtn = recv(sock, p, sizeof(buf) - bytes_received, 0);

		nlp = (struct nlmsghdr *) p;

		if(nlp->nlmsg_type == NLMSG_DONE)
			break;

		// increment the buffer pointer to place
		// next message
		p += rtn;

		// increment the total size by the size of
		// the last received message
		bytes_received += rtn;

		if((src_addr.nl_groups & RTMGRP_IPV4_ROUTE) == RTMGRP_IPV4_ROUTE) {
			break;
		};
	}
	printf("Received %d bytes back from kernel\n",bytes_received);
	char dsts[24], gws[24], ifs[16], mtrcs[16], ms[24];

	nlp = (struct nlmsghdr *) buf;
	char *ip;
	for(;NLMSG_OK(nlp, bytes_received);nlp=NLMSG_NEXT(nlp, bytes_received))
	{
		rtp = (struct rtmsg *) NLMSG_DATA(nlp);

		if(rtp->rtm_table != RT_TABLE_MAIN) {
			continue;
		}
		memset(dsts, 0, sizeof(dsts));
		memset(gws, 0, sizeof(gws));
		memset(ifs, 0, sizeof(ifs));
		memset(mtrcs, 0, sizeof(mtrcs));
		memset(ms, 0, sizeof(ms));

		rtap = (struct rtattr *) RTM_RTA(rtp);
		rtl = RTM_PAYLOAD(nlp);
		for(;RTA_OK(rtap, rtl);rtap=RTA_NEXT(rtap,rtl))
		{
			switch(rtap->rta_type)
			{
				/* IPv4 Destinations */
				case RTA_DST:
					inet_ntop(AF_INET, RTA_DATA(rtap),dsts, 24); 
					break;
				/* IPv4 Next-hops */
				case RTA_GATEWAY:
					inet_ntop(AF_INET, RTA_DATA(rtap),gws, 24);
					break;
				/* Output Interface/Device ID's */
				case RTA_OIF:
					sprintf(ifs, "%d",*((int *) RTA_DATA(rtap)));
				/* Route mtrcs */
				case RTA_METRICS:
					sprintf(mtrcs, "%d",*((int *) RTA_DATA(rtap)));
				default:
					break;
			}
		}
		sprintf(ms, "%d", rtp->rtm_dst_len);
		if(strlen(gws) == 0) {
			printf("%s/%s     [*Direct/%s]\n  > via interface %s\n",dsts, ms, mtrcs, ifs);
		} else {
			printf("%s/%s     [*Static/%s]\n  > to %s\n",dsts, ms, mtrcs, gws);
		};
	}
	close(sock);
}
