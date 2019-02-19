#include <cubez/cubez.h>
#include "concurrentqueue.h"

#include <iostream>
#include <string>
#include <thread>
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <mutex>

#ifdef __COMPILE_AS_WINDOWS__
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <Ws2tcpip.h>

WSADATA wsa_data;
#else
#endif

void initialize() {
  std::cout << "initializing network\n";
#ifdef __COMPILE_AS_WINDOWS__
  int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (result != NO_ERROR) {
    std::cout << "WSAStartup failed with error:" << result << std::endl;
    return;
  }
#endif
}
#if 0
enum class Command : int32_t {
  UNKNOWN = 0,
  ACK,
  CONNECT,
  DISCONNECT,
  INPUT,
  STATE_UPDATE,
};

struct ConnectPacket {
  char user_name[32];
};

struct DisconnectPacket {
  char user_name[32];
};

struct InputPacket {
};

struct ChatPacket {
	char message[128];
};

struct Packet {
public:
  static int Serialize(const Packet& packet, uint8_t* buffer) {
	  uint8_t* writer = buffer;
	  writer += network::pack::Int16(writer, (int32_t)packet.packet_id);
	  writer += network::pack::Int32(writer, (int32_t)packet.command);

	  switch (packet.command) {
		  case Command::CONNECT:
			  writer += network::pack::String(writer, 32, packet.connect_packet.user_name);
			  break;
		  case Command::DISCONNECT:
			  writer += network::pack::String(writer, 32, packet.disconnect_packet.user_name);
			  break;
      case Command::STATE_UPDATE:
      {
        const auto& state_update = packet.state_update;
        writer += network::pack::Uint64(writer, state_update.frame_number);
        writer += network::pack::Uint8(writer, state_update.components_length);
        for (auto i = 0; i < state_update.components_length; ++i) {
          const auto& component_header = state_update.components[i];
          writer += network::pack::Uint32(writer, component_header.component_id);
          writer += network::pack::Uint32(writer, component_header.instance_size);
          writer += network::pack::Uint32(writer, component_header.instances_length);
          for (auto j = 0; j < component_header.instances_length; ++j) {
            const auto& instance_header = component_header.instances[j];
            writer += network::pack::Uint32(writer, instance_header.entity_id);
            writer += network::pack::Bytes(writer, instance_header.data,
                                           component_header.instance_size);
          }
        }
      } break;
		  default:
			  break;
	  }
	  return writer - buffer;
  }
  static int Deserialize(uint8_t* buffer, Packet* packet) {
	  uint8_t* reader = buffer;
	  reader += network::unpack::Int16(reader, (int16_t*)&packet->packet_id);
	  reader += network::unpack::Int32(reader, (int32_t*)&packet->command);

	  switch (packet->command) {
		  case Command::CONNECT:
			  reader += network::unpack::String(reader, packet->connect_packet.user_name, 32);
			  break;
		  case Command::DISCONNECT:
			  reader += network::unpack::String(reader, packet->disconnect_packet.user_name, 32);
			  break;
      case Command::STATE_UPDATE:
      {
        auto* state_update = &packet->state_update;
        reader += network::unpack::Uint64(reader, &state_update->frame_number);
        reader += network::unpack::Uint8(reader, &state_update->components_length);
        for (auto i = 0; i < state_update->components_length; ++i) {
          auto component_header = &state_update->components[i];
          reader += network::unpack::Uint32(reader, &component_header->component_id);
          reader += network::unpack::Uint32(reader, &component_header->instance_size);
          reader += network::unpack::Uint32(reader, &component_header->instances_length);
          for (auto j = 0; j < component_header->instances_length; ++j) {
            auto* instance_header = &component_header->instances[j];
            reader += network::unpack::Uint32(reader, &instance_header->entity_id);
            reader += network::unpack::Bytes(reader, &instance_header->data,
                                             component_header->instance_size);
          }
        }
      }
		  default:
			  break;
	  }
	  return reader - buffer;
  }

  uint16_t packet_id;
  uint16_t packet_sequence;
  Command command;

  union {
    ConnectPacket connect_packet;
    DisconnectPacket disconnect_packet;
	  ChatPacket chat_packet;
    qbStateUpdateHeader state_update;
  };
};

class Runnable {
public:
  Runnable() : running_(false), started_(false) {}

  void Start() {
    if (!running_) {
      DoStart();
      main_ = new std::thread([this]() {
        started_ = true;
        std::cout << "started\n";
        while (running_) {
          Loop();
        }
      });
      running_ = true;
      while (!started_);
    }
  }

  void Stop() {
    if (running_) {
      running_ = false;
      started_ = false;
      if (main_->joinable()) {
        main_->join();
      }
      delete main_;
      DoStop();
    }
  }

private:
  virtual void Loop() = 0;
  virtual void DoStart() {}
  virtual void DoStop() {}

  volatile bool running_;
  volatile bool started_;
  std::thread* main_;
};

class GameState {
public:
  int Serialize(Packet* packet) {

  }
private:
  std::vector<glm::vec2> pos_;
};

class Multiplexer : public Runnable {
public:
  typedef std::function<void(Packet, const SocketInterface::SockAddr&)> ReceivedCallback;

  Multiplexer(std::unique_ptr<SocketInterface> socket,
              ReceivedCallback callback)
    : socket_(std::move(socket)), callback_(callback) {}

  void Loop() override {
    uint8_t buff[1024] = { '\0' };
    SocketInterface::SockAddr addr;
    int received = socket_->Receive((char*)buff, 256, &addr);
    
	  Packet packet;
	  Packet::Deserialize(buff, &packet);
    callback_(std::move(packet), addr);
  }

  int Send(const Packet& packet) {
    std::unique_ptr<uint8_t[]> buff(new uint8_t[sizeof(Packet)]);

    int size = Packet::Serialize(packet, buff.get());

    return socket_->Send((char*)buff.get(), size);
  }

  int Send(const Packet& packet, SocketInterface::SockAddr* addr) {
    std::unique_ptr<uint8_t[]> buff(new uint8_t[sizeof(Packet)]);

    int size = Packet::Serialize(packet, buff.get());

    return socket_->Send((char*)buff.get(), size, addr);
  }

private:
  std::unique_ptr<SocketInterface> socket_;
  ReceivedCallback callback_;
};

class Sender {
public:
  Sender(std::unique_ptr<SocketInterface> socket)
    : socket_(std::move(socket)) {}

  int Send(const Packet& packet) {
    std::unique_ptr<uint8_t[]> buff(new uint8_t[sizeof(Packet)]);
    
    int size = Packet::Serialize(packet, buff.get());

    return socket_->Send((char*)buff.get(), size);
  }

private:
  std::unique_ptr<SocketInterface> socket_;
};

class Receiver : public Runnable {
public:
  typedef std::function<void(Packet, const SocketInterface::SockAddr&)> ReceivedCallback;

  Receiver(std::unique_ptr<SocketInterface> socket,
           ReceivedCallback callback)
    : socket_(std::move(socket)), callback_(callback) {}

  void Loop() override {
    uint8_t buff[1024] = { '\0' };
    SocketInterface::SockAddr addr;
    int received = socket_->Receive((char*)buff, 256, &addr);

    Packet packet;
	Packet::Deserialize(buff, &packet);

    callback_(std::move(packet), addr);
  }

private:
  std::unique_ptr<SocketInterface> socket_;
  ReceivedCallback callback_;
};

class Client : public Runnable {
public:
  Client(const char user_name[32], const char* server_address, uint16_t server_port, uint16_t listen_port) {
    memcpy(user_name_, user_name, 32);
	  TestSocket::Options opts;
	  opts.latency_ms = 200;
	  opts.packet_loss = 0.25;
	  auto socket = TestSocket::Create(server_address, server_port, opts);
	  socket->Bind(listen_port);

    socket_ = std::make_unique<Multiplexer>(
      std::move(socket),
      [this](Packet packet, const SocketInterface::SockAddr& addr) {
        Received(std::move(packet), addr);
      });
  }

  bool ConnectToServer() {
	  std::cout << "Connecting to server...\n";
    Packet connect;
	  memset(&connect, 0, sizeof(Packet));
    connect.packet_id = NewPacketId();
    connect.command = Command::CONNECT;
    memcpy(connect.connect_packet.user_name, user_name_, 32);
    
    {
      std::lock_guard<std::mutex> lock(unacked_mu_);
      unacked_[connect.packet_id] = connect;
    }

    socket_->Send(connect);

	  while (!connected_);
	  std::cout << "Connected to server\n";
	  return connected_;
  }

private:
  void Loop() override {
    Sleep(100);
    std::lock_guard<std::mutex> lock(unacked_mu_);
    for (auto unacked_pair : unacked_) {
      std::cout << "Client '" << user_name_ << "' trying packet " << unacked_pair.first << " again\n";
      socket_->Send(unacked_pair.second);
    }
  }

  void DoStart() override {   
    std::cout << "Client started\n";
	  socket_->Start();
  }

  void DoStop() override {
	  socket_->Stop();
  }

  void Received(Packet packet, const SocketInterface::SockAddr& addr) {
    switch (packet.command) {
      case Command::ACK:
        std::cout << "Got ACK from server for packet: "
                  << packet.packet_id << "\n";
        {
          std::lock_guard<std::mutex> lock(unacked_mu_);
		      if (unacked_[packet.packet_id].command == Command::CONNECT) {
			      connected_ = true;
		      }
          unacked_.erase(packet.packet_id);
        }
        break;
      default:
        std::cout << "Client received unknown packet type: " << (int)packet.command << "\n";
        break;
    }
  }

  int16_t NewPacketId() {
    return packet_id_++;
  }

  char user_name_[32];
  std::mutex unacked_mu_;
  std::unordered_map<int16_t, Packet> unacked_;
  std::unique_ptr<Multiplexer> socket_;
  int16_t packet_id_;
  
  volatile bool connected_ = false;
};

class Server : public Runnable {
public:
  Server(uint16_t listen_port) {
	  TestSocket::Options opts;
	  opts.latency_ms = 200;
	  opts.packet_loss = 0.25;
    receiver_ = std::make_unique<Multiplexer>(TestSocket::Create(listen_port, opts),
        [this](Packet packet, const SocketInterface::SockAddr& addr) {
          Received(std::move(packet), addr);
        });
  }

private:
  void Loop() override {}

  void DoStart() override {
    std::cout << "Server started\n";
    receiver_->Start();
  }

  void DoStop() override {
    receiver_->Stop();
  }

  void Received(Packet packet, const SocketInterface::SockAddr& addr) {
    switch (packet.command) {
      case Command::CONNECT:
        std::cout << "Got connection request from user: '"
                  << packet.connect_packet.user_name << "'\n";
        connections_[packet.connect_packet.user_name] = addr;
        Packet ret;
        ret.packet_id = packet.packet_id;
        ret.command = Command::ACK;
        
        std::cout << "Sending back ACK to user '" << packet.connect_packet.user_name << "'\n";
        receiver_->Send(ret, &connections_[packet.connect_packet.user_name]);
        break;
      case Command::DISCONNECT:
        std::cout << "Got disconnection request from user: "
                  << packet.disconnect_packet.user_name << "\n";
        connections_.erase(packet.connect_packet.user_name);
        break;
      default:
        std::cout << "Server received unknown packet type: " << (int)packet.command << "\n";
        break;
    }
  }

  std::unordered_map<std::string, SocketInterface::SockAddr> connections_;
  std::unique_ptr<Multiplexer> receiver_;
};

int main() {
  initialize();

  const char* server_address = "127.0.0.1";
  int server_port = 25000;
  int client_port = 25001;

  Server server(server_port);
  Client client("client", server_address, server_port, client_port);
  server.Start();
  client.Start();
  client.ConnectToServer();
  
  while (1) {
    std::string line;
    std::getline(std::cin, line);
    //network::pack::String((uint8_t*)buff, 256, line.c_str());
    //socket->Send(buff, strlen(buff));
  }
  return 0;
}

#endif

int main()
{
  return 0;
}