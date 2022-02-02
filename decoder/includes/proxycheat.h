#ifndef PROXYCHEAT_H
#define PROXYCHEAT_H

struct EntityEntry;
struct netadr_s;

#include <byteswap.h>
#include <cstrike15_usermessages.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/reflection_ops.h>
#include <netmessages.pb.h>

#include <iostream>
#include <vector>

#include <bitread.h>
#include <crc.h>
#include <decoder.h>
#include <ice.h>

static constexpr unsigned char g_publicIceKey[] = {0x43,
                                                   0x53,
                                                   0x47,
                                                   0x4f,
                                                   0x87,
                                                   0x35,
                                                   0x00,
                                                   0x00,
                                                   0x61,
                                                   0x0d,
                                                   0x00,
                                                   0x00,
                                                   0x58,
                                                   0x03,
                                                   0x00,
                                                   0x00};

typedef netadr_s netadr_t;

#define NET_HEADER_FLAG_QUERY -1
#define NET_HEADER_FLAG_SPLITPACKET -2
#define NET_HEADER_FLAG_COMPRESSEDPACKET -3
// This is used, unless overridden in the registry
#define VALVE_MASTER_ADDRESS "207.173.177.10:27011"

#define PORT_RCON 27015        // defualt RCON port, TCP
#define PORT_MASTER 27011      // Default master port, UDP
#define PORT_CLIENT 27005      // Default client port, UDP/TCP
#define PORT_SERVER 27015      // Default server port, UDP/TCP
#define PORT_HLTV 27020        // Default hltv port
#define PORT_MATCHMAKING 27025 // Default matchmaking port
#define PORT_SYSTEMLINK 27030  // Default system link port
#define PORT_RPT 27035         // default RPT (remote perf testing) port, TCP
#define PORT_RPT_LISTEN \
    27036 // RPT connection listener (remote perf testing) port, TCP

// out of band message id bytes

// M = master, S = server, C = client, A = any
// the second character will allways be \n if the message isn't a single
// byte long (?? not true anymore?)

// Requesting for full server list from Server Master
#define A2M_GET_SERVERS 'c' // no params

// Master response with full server list
#define M2A_SERVERS 'd' // + 6 byte IP/Port list.

// Request for full server list from Server Master done in batches
#define A2M_GET_SERVERS_BATCH 'e' // + in532 uniqueID ( -1 for first batch )

// Master response with server list for channel
#define M2A_SERVER_BATCH \
    'f' // + int32 next uniqueID( -1 for last batch ) + 6 byte IP/Port list.

// Request for MOTD from Server Master  (Message of the Day)
#define A2M_GET_MOTD 'g' // no params

// MOTD response Server Master
#define M2A_MOTD 'h' // + string

// Generic Ping Request
#define A2A_PING 'i' // respond with an A2A_ACK

// Generic Ack
#define A2A_ACK 'j' // general acknowledgement without info

#define C2S_CONNECT 'k' // client requests to connect

// Print to client console.
#define A2A_PRINT 'l' // print a message on client

// info request
#define S2A_INFO_DETAILED \
    'm' // New Query protocol, returns dedicated or not, + other performance
        // info.

// Another user is requesting a challenge value from this machine
// NOTE: this is currently duplicated in SteamClient.dll but for a different
// purpose, so these can safely diverge anytime. SteamClient will be using a
// different protocol to update the master servers anyway.
#define A2S_GETCHALLENGE 'q' // Request challenge # from another machine

#define A2S_RCON 'r' // client rcon command

#define A2A_CUSTOM \
    't' // a custom command, follow by a string for 3rd party tools

// A user is requesting the list of master servers, auth servers, and titan dir
// servers from the Client Master server
#define A2M_GETMASTERSERVERS \
    'v' // + byte (type of request, TYPE_CLIENT_MASTER or TYPE_SERVER_MASTER)

// Master server list response
#define M2A_MASTERSERVERS 'w' // + byte type + 6 byte IP/Port List

#define A2M_GETACTIVEMODS \
    'x' // + string Request to master to provide mod statistics ( current usage
        // ).  "1" for first mod.

#define M2A_ACTIVEMODS 'y' // response:  modname\r\nusers\r\nservers

#define M2M_MSG 'z' // Master peering message

// SERVER TO CLIENT/ANY

// Client connection is initiated by requesting a challenge value
//  the server sends this value back
#define S2C_CHALLENGE 'A' // + challenge value

// Server notification to client to commence signon process using challenge
// value.
#define S2C_CONNECTION 'B' // no params

// Response to server info requests

// Request for detailed server/rule information.
#define S2A_INFO_GOLDSRC 'm' // Reserved for use by goldsrc servers

#define S2M_GETFILE 'J'  // request module from master
#define M2S_SENDFILE 'K' // send module to server

#define S2C_REDIRECT \
    'L' // + IP x.x.x.x:port, redirect client to other server/proxy

#define C2M_CHECKMD5 \
    'M' // player client asks secure master if Module MD5 is valid
#define M2C_ISVALIDMD5 'N' // secure servers answer to C2M_CHECKMD5

// MASTER TO SERVER
#define M2A_ACTIVEMODS3 'P'    // response:  keyvalues struct of mods
#define A2M_GETACTIVEMODS3 'Q' // get a list of mods and the stats about them

#define S2A_LOGSTRING 'R' // send a log string
#define S2A_LOGKEY 'S'    // send a log event as key value

#define A2S_SERVERQUERY_GETCHALLENGE \
    'W' // Request challenge # from another machine

#define A2S_KEY_STRING \
    "Source Engine Query" // required postfix to a A2S_INFO query

#define A2M_GET_SERVERS_BATCH2 '1' // New style server query

#define A2M_GETACTIVEMODS2 '2' // New style mod info query

#define C2S_AUTHREQUEST1 '3'   //
#define S2C_AUTHCHALLENGE1 '4' //
#define C2S_AUTHCHALLENGE2 '5' //
#define S2C_AUTHCOMPLETE '6'
#define C2S_AUTHCONNECT \
    '7' // Unused, signals that the client has
        // authenticated the server

#define S2C_CONNREJECT '9' // Special protocol for rejected connections.

#define LOWORD(x) (*((uint16_t*)&(x)))  // low word
#define LODWORD(x) (*((uint32_t*)&(x))) // low dword
// each channel packet has 1 byte of FLAG bits
#define PACKET_FLAG_RELIABLE (1 << 0) // packet contains subchannel stream data
#define PACKET_FLAG_COMPRESSED (1 << 1) // packet is compressed
#define PACKET_FLAG_ENCRYPTED (1 << 2)  // packet is encrypted
#define PACKET_FLAG_SPLIT (1 << 3)      // packet is split
#define PACKET_FLAG_CHOKED (1 << 4)     // packet was choked by sender
#define PACKET_FLAG_CHALLENGE (1 << 5)  // packet is a challenge
#define PACKET_FLAG_UNKNOWN1 (1 << 6)
#define PACKET_FLAG_UNKNOWN2 (1 << 7)
#define PACKET_FLAG_UNKNOWN3 (1 << 8)
#define PACKET_FLAG_UNKNOWN4 (1 << 9)
#define PACKET_FLAG_TABLES (1 << 10) // custom flag, request string tables
#define MAX_STREAMS 2
#define NET_MAX_MESSAGE 524303

#define NETMSG_TYPE_BITS 8
#define FRAGMENT_BITS 8
#define FRAGMENT_SIZE (1 << FRAGMENT_BITS)
#define MAX_FILE_SIZE_BITS 26
#define MAX_FILE_SIZE \
    ((1 << MAX_FILE_SIZE_BITS) - 1) // maximum transferable size is	64MB
#define BYTES2FRAGMENTS(i) ((i + FRAGMENT_SIZE - 1) / FRAGMENT_SIZE)
// This is the packet payload without any header bytes (which are attached for
// actual sending)
#define NET_MAX_PALYLOAD_BITS 19 // 2^NET_MAX_PALYLOAD_BITS > NET_MAX_PAYLOAD
// This is just the client_t->netchan.datagram buffer size (shouldn't ever need
// to be huge)
#define NET_MAX_DATAGRAM_PAYLOAD 4000 // = maximum unreliable playload size
#define FILESYSTEM_INVALID_HANDLE (FileHandle_t)0
#define MIN_USER_MAXROUTABLE_SIZE 576 // ( X.25 Networks )
#define MIN_SPLIT_SIZE (MIN_USER_MAXROUTABLE_SIZE - sizeof(SPLITPACKET))
#define MAX_SPLITPACKET_SPLITS (NET_MAX_MESSAGE / MIN_SPLIT_SIZE)
#define MAX_ROUTABLE_PAYLOAD 1200
#define SPLIT_PACKET_STALE_TIME 15.0f
#define NET_MAX_PAYLOAD 524284
#define MAX_USER_MAXROUTABLE_SIZE MAX_ROUTABLE_PAYLOAD
#define MAX_SPLIT_SIZE (MAX_USER_MAXROUTABLE_SIZE - sizeof(SPLITPACKET))

typedef void* FileHandle_t;

enum netadrtype_t
{
    NA_NULL = 0,
    NA_LOOPBACK,
    NA_BROADCAST,
    NA_IP,
};

struct netadr_s
{
    bool CompareAdr(netadr_t* b)
    {
        if (type != b->type)
        {
            return false;
        }

        if (type == NA_LOOPBACK)
        {
            return true;
        }

        if (type == NA_IP && ip[0] == b->ip[0] && ip[1] == b->ip[1] &&
            ip[2] == ip[2] && ip[3] == b->ip[3] && port == b->port)
        {
            return true;
        }

        return false;
    }

    netadrtype_t type;
    unsigned char ip[4];
    unsigned short port;
};

#pragma pack(1)
typedef struct
{
    int netID;
    int sequenceNumber;
    int packetID : 16;
    int nSplitSize : 16;
} SPLITPACKET;
#pragma pack()

typedef struct
{
    int currentSequence;
    int splitCount;
    int totalSize;
    int nExpectedSplitSize;
    char buffer[NET_MAX_MESSAGE]; // This has to be big enough to hold the
                                  // largest message
} LONGPACKET;

class CSplitPacketEntry
{
 public:
    CSplitPacketEntry()
    {
        memset(&from, 0, sizeof(from));

        int i;
        for (i = 0; i < (int)MAX_SPLITPACKET_SPLITS; i++)
        {
            splitflags[i] = -1;
        }

        memset(&netsplit, 0, sizeof(netsplit));
        lastactivetime = 0.0f;
    }

 public:
    netadr_t from;
    int splitflags[MAX_SPLITPACKET_SPLITS];
    LONGPACKET netsplit;
    // host_time the last time any entry was received for this entry
    float lastactivetime;
};

typedef std::vector<CSplitPacketEntry> vecSplitPacketEntries_t;

struct dataFragments_s
{
    FileHandle_t file;              // open file handle
    char filename[MAX_OSPATH];      // filename
    char* buffer;                   // if NULL it's a file
    unsigned int bytes;             // size in bytes
    unsigned int bits;              // size in bits
    unsigned int transferID;        // only for files
    bool isCompressed;              // true if data is bzip compressed
    unsigned int nUncompressedSize; // full size in bytes
    bool asTCP;                     // send as TCP stream
    bool isDemo;                    // is demo?
    int numFragments;               // number of total fragments
    int ackedFragments;             // number of fragments send & acknowledged
    int pendingFragments; // number of fragments send, but not acknowledged yet
};

typedef struct dataFragments_s dataFragments_t;

class netchan_t
{
 public:
    byte g_iceKey[16];
    std::vector<EntityEntry*> m_vecEntities;
    static double m_netTime;
    dataFragments_t m_ReceiveList[MAX_STREAMS];
    vecSplitPacketEntries_t m_netSplitPacket;
    netadr_t m_netAdr;
    bool m_connected;
    int m_nInSequenceNr;
    std::string m_strMapName;
    bool m_bUpdatedEntities;

    static std::vector<netchan_t*> g_vecNetChannels;

    netchan_t(std::vector<byte> iceKey =
                  std::vector<byte>(g_publicIceKey,
                                    g_publicIceKey + sizeof(g_publicIceKey)))
    {
        if (iceKey.size() == sizeof(g_iceKey))
            memcpy(g_iceKey, iceKey.data(), sizeof(g_iceKey));
        clear();
        m_vecEntities.clear();
        memset(&m_netAdr, 0, sizeof(m_netAdr));
    }

    netchan_t(byte* iceKey)
    {
        memcpy(g_iceKey, iceKey, sizeof(g_iceKey));
        memset(&m_netAdr, 0, sizeof(m_netAdr));
        clear();
        m_vecEntities.clear();
    }

    void clear()
    {
        memset(m_ReceiveList, 0, sizeof(m_ReceiveList));
        m_netSplitPacket.clear();
        m_netTime = 0.0;
        m_connected = false;
        m_nInSequenceNr = 0;
    }

    // NET* stuffs.
    CSplitPacketEntry* NET_FindOrCreateSplitPacketEntry(netadr_t* from);

    bool NET_GetLong(unsigned char*& packet,
                     uint32_t& packet_size,
                     netadr_t* netadr);

    static void NET_UpdateTime();

    void NET_DiscardStaleSplitpackets();

    static bool NET_BufferToBufferDecompress(char* dest,
                                             unsigned int* destLen,
                                             char* source,
                                             unsigned int sourceLen);

    bool parsePayload(const unsigned char* payload, const int size);

    // Entity stuffs
    EntityEntry* FindEntity(int nEntity);
    EntityEntry* AddEntity(int nEntity, uint32 uClass, uint32 uSerialNum);
    int FindHighestEntity();
    void RemoveEntity(int nEntity);
    static void ParseDeltaHeader(EntityHeaderInfo& u, CBitRead* pBuf);
    void parseEnts(CSVCMsg_PacketEntities& msg);

    // Cmds stuffs.
    bool processCmd(CBitRead& buf);
    int ReadHeader(CBitRead& buf);
    bool ReadPacketServer(unsigned char* packetData, int size);

    // Fragments stuff
    bool ReadSubDataChannel(CBitRead& buf, int stream);
    void UncompressFragments(dataFragments_t* data);
    bool CheckReceivingList(int nList);

    static netchan_t*
    findOrCreateChannel(uint32_t ip,
                        unsigned short port,
                        std::vector<byte> iceKey = std::vector<byte>(
                            g_publicIceKey,
                            g_publicIceKey + sizeof(g_publicIceKey)));
};

auto processPayload(uint32_t ip,
                    unsigned short port,
                    const unsigned char* payload,
                    const int size,
                    std::vector<byte> iceKey = std::vector<byte>(
                        g_publicIceKey,
                        g_publicIceKey + sizeof(g_publicIceKey))) -> netchan_t*;

#endif
