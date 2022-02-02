CXX = g++

# GENERATE PROTOs
GENERATE_OUT_DIR=decoder/generated
GENERATE_SRC_DIR=decoder/proto
GENERATE_SRC=$(wildcard decoder/proto/*.proto)
GENERATED_SRC_OBJ=$(subst proto/,generated/,$(subst .proto,.pb.o,$(GENERATE_SRC)))
GENERATED_SRC_OBJ64=$(subst proto/,generated/,$(subst .proto,.pb.o64,$(GENERATE_SRC)))

# DETOUR
DETOUR32_OBJ=$(subst .cpp,.o,$(wildcard detour/src/*.cpp))
DETOUR64_OBJ=$(subst .cpp,.o64,$(wildcard detour/src/*.cpp))

# DECODER OBJS
DECODER_SRC=$(wildcard decoder/src/*.cpp)
DECODER_OBJ=$(subst .cpp,.o,$(DECODER_SRC))
DECODER_OBJ64=$(subst .cpp,.o64,$(DECODER_SRC))

# READPACKETS
READPACKETS=readpackets.out
READPACKETS_OBJ=$(subst .cpp,.o,$(wildcard readpackets/src/*.cpp))

# SNIFFER
SNIFFER=sniffer.out
SNIFFER_OBJ=$(subst .cpp,.o,$(wildcard sniffer/src/*.cpp))

# CSGO_CAPTURE
CSGO_CAPTURE=csgo_capture.so
CSGO_CAPTURE_OBJ=$(subst .cpp,.o,$(wildcard csgo_capture/src/*.cpp))

# CSGO_DUMP_DATATABLES
CSGO_DUMP_DATATABLES=csgo_dump_datatables.so
CSGO_DUMP_DATATABLES_OBJ=$(subst .cpp,.o,$(wildcard csgo_dump_datatables/src/*.cpp))

# RADAR
RADAR=radar.out
RADAR_OBJ=$(subst .cpp,.o,$(wildcard radar/src/*.cpp))
CPPFLAGS_RADAR=-std=c++17 -m64 -g -Wextra -W -Wall -Werror -Wl,--no-undefined -I/usr/local/include/ -Iradar/includes -Isocket_buffer/includes -Idecoder/includes -Idecoder/generated
RADAR_PCH=radar/includes/radar_pch.h
RADAR_PCH_OUT=$(RADAR_PCH).gch

# SOCKET_BUFFER CHECKS
SOCKET_BUFFER=socket_buffer.out
SOCKET_BUFFER_OBJ=$(subst .cpp,.o,$(wildcard socket_buffer/src/*.cpp))
SOCKET_BUFFER_CPPFLAGS=-std=c++17 -m64 -g -Wextra -W -Wall -Werror -Wl,--no-undefined -I/usr/local/include/ -Isocket_buffer/includes
# Link statically. Not dynamically.
# LIBS_64=/usr/lib/x86_64-linux-gnu/libprotobuf.a /usr/lib/i386-linux-gnu/libpthread.a /usr/lib/gcc/x86_64-linux-gnu/8/libstdc++.a /usr/lib/x86_64-linux-gnu/libm.a
# LIBS_32=/usr/lib/i386-linux-gnu/libprotobuf.a /usr/lib/i386-linux-gnu/libpthread.a /usr/lib/gcc/i386-linux-gnu/8/libstdc++.a /usr/lib/i386-linux-gnu/libm.a

# Normal FLAGS
CPPFLAGS_64=-fPIC -std=c++17 -m64 -g -Wextra -W -Wall -Werror -Wl,--no-undefined -Wno-unused-parameter -Idetour/includes -I/usr/local/include/ -Idecoder/includes/ -Idecoder/generated/ -Iradar/includes -Isniffer/includes -Isocket_buffer/includes
CPPFLAGS_32=-fPIC -std=c++17 -m32 -g -Wextra -W -Wall -Werror -Wl,--no-undefined -Wno-unused-parameter -Idetour/includes -I/usr/local/include/ -Idecoder/includes/ -Idecoder/generated/ -Iradar/includes -Isniffer/includes -Isocket_buffer/includes
CXXFLAGS=-lpthread -lprotobuf -ltins -lm -lstdc++ -ldl -lboost_system -llz4

all: socket_buffer sniffer readpackets csgo_capture csgo_dump_datatables radar

socket_buffer: $(SOCKET_BUFFER)
sniffer: $(SNIFFER)
readpackets: $(READPACKETS)
csgo_dump_datatables: $(CSGO_DUMP_DATATABLES)
csgo_capture: $(CSGO_CAPTURE)
radar: $(RADAR_PCH_OUT) $(RADAR)

generate_proto:
	mkdir -p $(GENERATE_OUT_DIR)
	protoc --proto_path $(GENERATE_SRC_DIR) --cpp_out=$(GENERATE_OUT_DIR) $(GENERATE_SRC) 
	
.PHONY: all clean

$(SOCKET_BUFFER): $(SOCKET_BUFFER_OBJ)
	$(CXX) $(SOCKET_BUFFER_CPPFLAGS) -o $@ $^ -llz4

$(SOCKET_BUFFER_OBJ): %.o: %.cpp
	$(CXX) -c $(SOCKET_BUFFER_CPPFLAGS) $< -o $@

$(CSGO_CAPTURE): $(DETOUR64_OBJ) $(GENERATED_SRC_OBJ64) $(CSGO_CAPTURE_OBJ)
	$(CXX) $(CPPFLAGS_64) -o $@ $^ -lpthread -lprotobuf -lm -ldl -lstdc++ -shared

$(CSGO_CAPTURE_OBJ): %.o: %.cpp
	$(CXX) -c $(CPPFLAGS_64) $< -o $@


$(RADAR): $(RADAR_OBJ)
	$(CXX) $(CPPFLAGS_RADAR) -o $@ $^ -lpthread -lsfml-graphics -lsfml-window -lsfml-system -lboost_system -llz4

$(RADAR_OBJ): %.o: %.cpp
	$(CXX) -c $(CPPFLAGS_RADAR) $< -o $@

$(RADAR_PCH_OUT): $(RADAR_PCH)
	$(CXX) -c $(CPPFLAGS_RADAR) $(RADAR_PCH) -o $(RADAR_PCH_OUT)


$(CSGO_DUMP_DATATABLES): $(DETOUR32_OBJ) $(GENERATED_SRC_OBJ) $(DECODER_OBJ) $(CSGO_DUMP_DATATABLES_OBJ)  
	$(CXX) $(CPPFLAGS_32) -o $@ $^ -lpthread -lprotobuf -lm -ldl -lstdc++ -shared

$(CSGO_DUMP_DATATABLES_OBJ): %.o: %.cpp
	$(CXX) -c $(CPPFLAGS_32) $< -o $@



$(READPACKETS): $(DECODER_OBJ64) $(GENERATED_SRC_OBJ64) $(READPACKETS_OBJ) radar/src/entity.o
	$(CXX) $(CPPFLAGS_64) -o $@ $^ $(CXXFLAGS)

$(READPACKETS_OBJ): %.o: %.cpp
	$(CXX) -c $(CPPFLAGS_64) $< -o $@



$(SNIFFER): $(DECODER_OBJ64) $(GENERATED_SRC_OBJ64) $(SNIFFER_OBJ) radar/src/entity.o
	$(CXX) $(CPPFLAGS_64) -o $@ $^ $(CXXFLAGS)

$(SNIFFER_OBJ): %.o: %.cpp
	$(CXX) -c $(CPPFLAGS_64) $< -o $@



$(DECODER_OBJ): %.o: %.cpp
	$(CXX) -c $(CPPFLAGS_32) $< -o $@

$(GENERATED_SRC_OBJ): %.o: %.cc
	$(CXX) -c $(CPPFLAGS_32) $< -o $@

$(DETOUR32_OBJ): %.o: %.cpp
	$(CXX) -c $(CPPFLAGS_32) $< -o $@



$(DECODER_OBJ64): %.o64: %.cpp
	$(CXX) -c $(CPPFLAGS_64) $< -o $@

$(GENERATED_SRC_OBJ64): %.o64: %.cc
	$(CXX) -c $(CPPFLAGS_64) $< -o $@

$(DETOUR64_OBJ): %.o64: %.cpp
	$(CXX) -c $(CPPFLAGS_64) $< -o $@

clean:
	# ${RM} $(GENERATE_OUT_DIR)/*
	${RM} $(DECODER_OBJ) $(DECODER_OBJ64)
	${RM} $(READPACKETS_OBJ) $(SNIFFER_OBJ) $(CSGO_CAPTURE_OBJ) $(CSGO_DUMP_DATATABLES_OBJ) $(RADAR_OBJ)
	${RM} $(CSGO_CAPTURE) $(SNIFFER) $(READPACKETS) $(CSGO_DUMP_DATATABLES) $(RADAR)
	${RM} $(DETOUR32_OBJ) $(DETOUR64_OBJ)
	${RM} $(RADAR_PCH_OUT)
	${RM} $(SOCKET_BUFFER) $(SOCKET_BUFFER_OBJ)
