#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h> 
#include <sys/types.h> 
#include <sys/time.h> 
#include <sys/socket.h> 
#include <sys/select.h>
#include <arpa/inet.h> 
#include <netinet/in.h>
#include <time.h>
#include <netdb.h>
#include <linux/ip.h> 	/* for ipv4 header */
#include <linux/udp.h>  /* for upd header */
#include <linux/icmp.h>  /* for icmp header */

#define MY_PORT 30013
#define DEST_PORT 32164
#define TIMEOUT_TIME 1
#define PAYLOAD_SIZE 52
#define MAX_SIZE 101

int main(int argc, char *argv[])
{
    srand(time(0) + getpid());

    if (argc != 2)
    {
        fprintf(stderr, "Usage: mytraceroute <destination domain name>");
        exit(1);
    }

    struct hostent *he;
    if ((he=gethostbyname(argv[1])) == NULL) {
        perror("gethostbyname");
        exit(1);
    }

    struct in_addr dest_ip = *((struct in_addr *)he->h_addr);
    printf("IP Address of %s: %s\n", argv[1], inet_ntoa(dest_ip));

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DEST_PORT);
    dest_addr.sin_addr = dest_ip;

    int sockfd1;
    if ((sockfd1 = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) == -1)
    {
        perror("UDP socket");
        exit(1);
    }

    const int enable = 1 ;
    if (setsockopt(sockfd1, IPPROTO_IP, IP_HDRINCL, &enable, sizeof(enable)) < 0)
    {
        perror("setsockopt IP_HDRINCL");
        close(sockfd1) ;
        exit(1) ;
    }
    if (setsockopt(sockfd1, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
    {
        perror("setsockopt REUSEADDR");
        close(sockfd1);
        exit(1);
    }

    struct sockaddr_in my_addr;
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(MY_PORT);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    // Binding the socket to the address
    if (bind(sockfd1, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("bind");
        close(sockfd1);
        exit(1);
    }

    int sockfd2;
    if ((sockfd2 = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1)
    {
        perror("ICMP socket");
        exit(1);
    }
    if (bind(sockfd2, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("bind");
        close(sockfd2);
        exit(1);
    }

    for (int TTL = 1; TTL <= 16; TTL++)
    {
        for (int attempt = 0; attempt < 3; attempt++)
        {
            char payload[PAYLOAD_SIZE];
            for (int i = 0; i < PAYLOAD_SIZE; i++)
            {
                payload[i] = rand()%26 + 'a';
            }
            // payload[PAYLOAD_SIZE-1] = '\0';

            int UDP_HDRLEN = sizeof(struct udphdr);
            int DATAGRAM_LEN = UDP_HDRLEN + PAYLOAD_SIZE;

            struct udphdr UDP_HEADER;
            UDP_HEADER.source = my_addr.sin_port;
            UDP_HEADER.dest = dest_addr.sin_port;
            UDP_HEADER.len = htons(DATAGRAM_LEN);
            UDP_HEADER.check = 0;


            char UDP_DATAGRAM[DATAGRAM_LEN];
            memcpy(UDP_DATAGRAM, &UDP_HEADER, UDP_HDRLEN);
            memcpy(UDP_DATAGRAM + UDP_HDRLEN, payload, PAYLOAD_SIZE);
            // UDP_DATAGRAM[DATAGRAM_LEN-1] = '\0';

            struct iphdr IP_HEADER;
            int IP_HDRLEN = sizeof(IP_HEADER);
            int PACKET_LEN = IP_HDRLEN + DATAGRAM_LEN;
            IP_HEADER.ihl = (IP_HDRLEN >> 2);
            IP_HEADER.version = 4;
            IP_HEADER.tos = 0;
            IP_HEADER.tot_len = htons(PACKET_LEN);
            IP_HEADER.id = 0;
            IP_HEADER.frag_off = 0;
            IP_HEADER.ttl = TTL;
            IP_HEADER.protocol = IPPROTO_UDP;
            IP_HEADER.check = 0;
            IP_HEADER.saddr = my_addr.sin_addr.s_addr;
            IP_HEADER.daddr = dest_addr.sin_addr.s_addr;

            char IP_PACKET[PACKET_LEN];
            memcpy(IP_PACKET, &IP_HEADER, IP_HDRLEN);
            memcpy(IP_PACKET + IP_HDRLEN, &UDP_DATAGRAM, DATAGRAM_LEN);

            if (sendto(sockfd1, IP_PACKET, PACKET_LEN, 0 , (struct sockaddr*)&dest_addr, sizeof(struct sockaddr)) < 0)
            {
                perror("sendto");
                close(sockfd1);
                close(sockfd2);
                exit(1);
            }
            struct timeval time_of_sending;
            gettimeofday(&time_of_sending, NULL);

            fd_set fd;
            int max_fd = sockfd2 + 1;
            struct timeval tv;
            tv.tv_sec = TIMEOUT_TIME;
            tv.tv_usec = 0;

            FD_ZERO(&fd);
            FD_SET(sockfd2, &fd);

            int ready = 0;
            if ((ready = select(max_fd, &fd, NULL, NULL, &tv)) < 0)
            {
                perror("select");
                close(sockfd1);
                close(sockfd2);
                exit(1);
            }

            if (ready == 0)
            {
                // printf("Timeout Occured\n");
                if (attempt == 2)
                    fprintf(stdout, "HOP VALUE(%d)\t*\t*\n", TTL);
                continue;
            }

            if (FD_ISSET(sockfd2, &fd))
            {
                char IP_PACKET[MAX_SIZE];
                struct sockaddr_in packet_source;
                socklen_t source_len = sizeof(packet_source);

                int numbytes;
                if ((numbytes = recvfrom(sockfd2, IP_PACKET, MAX_SIZE, 0, (struct sockaddr*)&packet_source, &source_len)) < 0)
                {
                    perror("recvfrom");
                    close(sockfd1);
                    close(sockfd2);
                    exit(1);
                }
                struct timeval time_of_recving;
                gettimeofday(&time_of_recving, NULL);

                long long time_diff = ((time_of_recving.tv_sec)*1000000+(time_of_recving.tv_usec)) - ((time_of_sending.tv_sec)*1000000+(time_of_sending.tv_usec));

                struct iphdr* IP_HEADER = (struct iphdr*) IP_PACKET;
                if ( IP_HEADER->protocol != IPPROTO_ICMP ){
                    continue;
                }
                int IP_HDRLEN = sizeof(*IP_HEADER);

                struct icmphdr* ICMP_HEADER = (struct icmphdr*)(IP_PACKET+IP_HDRLEN);

                if (ICMP_HEADER->type == 3)
                {
                    if (packet_source.sin_addr.s_addr != dest_addr.sin_addr.s_addr)
                    {
                        fprintf(stderr, "Destination Source and ICMP Source are different.\n");
                        fflush(stderr);
                        close(sockfd1);
                        close(sockfd2);
                        exit(3);
                    }
                    fprintf(stdout, "Destination Unreachable\t");
                    fprintf(stdout, "HOP VALUE(%d)\t%s\t%lld\n", TTL, inet_ntoa(packet_source.sin_addr), time_diff);
                    close(sockfd1);
                    close(sockfd2);
                    exit(0);
                    break;
                }
                else if (ICMP_HEADER->type == 11)
                {
                    // fprintf(stdout, "Time Exceeded\t");
                    fprintf(stdout, "HOP VALUE(%d)\t%s\t%lld\n", TTL, inet_ntoa(packet_source.sin_addr), time_diff);
                    break;
                }
                else
                {  
                    fprintf(stderr, "Invalid\n");
                    continue;
                }
            }

        }
    }

    close(sockfd1);
    close(sockfd2);
    exit(0);
}