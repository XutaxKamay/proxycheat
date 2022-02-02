#include <reversed_structs.h>

static bool g_bLoadedCSGO_DUMP_DATATABLES = false;
static std::vector<ServerClass_t> vecServerClasses;
static std::vector<CSVCMsg_SendTable> vecDataTables;

bool ReadFromBuffer(CBitRead& buffer, void** pBuffer, int& size)
{
    size = buffer.ReadVarInt32();
    if (size < 0 || size > NET_MAX_PAYLOAD)
    {
        return false;
    }

    // Check its valid
    if (size > buffer.GetNumBytesLeft())
    {
        return false;
    }

    *pBuffer = malloc(size);

    // If the read buffer is byte aligned, we can parse right out of it
    if ((buffer.GetNumBitsRead() % 8) == 0)
    {
        memcpy(*pBuffer,
               buffer.GetBasePointer() + buffer.GetNumBytesRead(),
               size);
        buffer.SeekRelative(size * 8);
        return true;
    }

    // otherwise we have to ReadBytes() it out
    if (!buffer.ReadBytes(*pBuffer, size))
    {
        return false;
    }

    return true;
}

void dumpDataTables()
{
    auto serverInterface = reinterpret_cast<CreateInterfaceFn>(
        dlsym(dlopen("csgo/bin/server.so", RTLD_LAZY), "CreateInterface"));

    auto serverGameDLL = reinterpret_cast<IServerGameDLL*>(
        serverInterface("ServerGameDLL005", NULL));

    auto serverClass = serverGameDLL->GetAllClasses();

    auto lm = reinterpret_cast<link_map_t*>(
        dlopen("bin/engine_client.so", RTLD_LAZY));

    auto DataTable_WriteSendTablesBuffer = (void (*)(void*, void*))(lm->l_addr +
                                                                    0x45AE60);

    auto DataTable_WriteClassInfosBuffer = (void (*)(void*, void*))(lm->l_addr +
                                                                    0x45AF00);

    int alloc = 0x400000;
    void* buffer = malloc(alloc);

    CBitWrite_reversed bufWrite("CDemoRecorder::RecordServerClasses",
                                buffer,
                                alloc);

    DataTable_WriteSendTablesBuffer(serverClass, &bufWrite);
    DataTable_WriteClassInfosBuffer(serverClass, &bufWrite);

    CBitRead buf(buffer, alloc);

    CSVCMsg_SendTable msg;
    while (1)
    {
        buf.ReadVarInt32();
        void* pBuffer = NULL;
        int size = 0;
        if (!ReadFromBuffer(buf, &pBuffer, size))
        {
            printf("ParseDataTable: ReadFromBuffer failed.\n");
            return;
        }
        msg.ParseFromArray(pBuffer, size);
        free(pBuffer);

        if (msg.is_end())
            break;

        vecDataTables.push_back(msg);
    }

    printf("Got %i tables\n", vecDataTables.size());

    short nServerClasses = buf.ReadShort();

    for (int i = 0; i < nServerClasses; i++)
    {
        ServerClass_t entry;
        entry.nClassID = buf.ReadShort();
        if (entry.nClassID >= nServerClasses)
        {
            printf("ParseDataTable: invalid class index (%d).\n",
                   entry.nClassID);
            return;
        }

        int nChars;
        buf.ReadString(entry.strName, sizeof(entry.strName), false, &nChars);
        buf.ReadString(entry.strDTName, sizeof(entry.strDTName), false, &nChars);

        // find the data table by name
        entry.nDataTable = -1;
        for (unsigned int j = 0; j < vecDataTables.size(); j++)
        {
            if (strcmp(entry.strDTName,
                       vecDataTables[j].net_table_name().c_str()) == 0)
            {
                entry.nDataTable = j;
                break;
            }
        }

        vecServerClasses.push_back(entry);
    }

    printf("Got %i serverclasses\n", vecDataTables.size());

    free(buffer);

    std::ofstream file;

    file.open("/tmp/dump.txt", std::ios::trunc | std::ios::out);
    file.close();
    file.open("/tmp/dump.txt", std::ios::app | std::ios::binary);
    int size = vecDataTables.size();
    file.write(reinterpret_cast<const char*>(&size), sizeof(size));

    for (auto&& dataTable : vecDataTables)
    {
        auto array = dataTable.SerializeAsString();
        size = array.size();
        file.write(reinterpret_cast<const char*>(&size), sizeof(size));
        file.write(reinterpret_cast<const char*>(array.data()), size);
    }

    size = vecServerClasses.size();
    file.write(reinterpret_cast<const char*>(&size), sizeof(size));
    for (auto&& serverClasses : vecServerClasses)
    {
        file.write(reinterpret_cast<const char*>(&serverClasses.nClassID),
                   sizeof(serverClasses.nClassID));
        file.write(reinterpret_cast<const char*>(&serverClasses.nDataTable),
                   sizeof(serverClasses.nDataTable));
        file.write(reinterpret_cast<const char*>(serverClasses.strDTName),
                   sizeof(serverClasses.strDTName));
        file.write(reinterpret_cast<const char*>(serverClasses.strName),
                   sizeof(serverClasses.strName));
    }

    file.close();
}

void __attribute__((constructor)) init_module(void)
{
    g_bLoadedCSGO_DUMP_DATATABLES = true;

    std::cout << "Injected" << std::endl;

    dumpDataTables();
}

void __attribute__((destructor)) cleanup_module(void)
{}
