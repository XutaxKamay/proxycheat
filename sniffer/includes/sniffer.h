#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <tins/tins.h>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/make_default.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/copy.hpp>
#include <entity.h>
#include <deque>
#include <lz4.h>
#include <socket_buffer.h>

void process_packet(u_char*, const struct pcap_pkthdr*, const u_char*);
void process_ip_packet(const u_char*, int);
void print_ip_packet(const u_char*, int);
void print_tcp_packet(const u_char*, int);
void print_udp_packet(const u_char*, int);
void print_icmp_packet(const u_char*, int);
void PrintData(const u_char*, int);
int start_sniffer();