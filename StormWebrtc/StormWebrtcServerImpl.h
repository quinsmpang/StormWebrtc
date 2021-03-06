#pragma once

#ifdef _MSC_VER
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#endif

#include <thread>
#include <chrono>
#include <mutex>
#include <queue>
#include <unordered_map>

#include "asio/asio.hpp"

#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/certs.h"
#include "mbedtls/x509.h"
#include "mbedtls/ssl.h"
#include "mbedtls/ssl_cookie.h"
#include "mbedtls/net.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"
#include "mbedtls/timing.h"

#if defined(MBEDTLS_SSL_CACHE_C)
#include "mbedtls/ssl_cache.h"
#endif

#define SCTP_DEBUG
#define SCTP_SIMPLE_ALLOCATOR
#define SCTP_PROCESS_LEVEL_LOCKS
#define INET
#define INET6

#include "usrsctplib/usrsctp.h"

#include "StormWebrtc/StormWebrtcServer.h"

//#define STORMWEBRTC_USE_THREADS


class StormWebrtcServerImpl;

struct StormWebrtcConnection
{
#ifdef STORMWEBRTC_USE_THREADS
  std::recursive_mutex m_Mutex;
#endif

  bool m_Allocated = false;
  bool m_Connected = false;
  bool m_CreatedDataChannels = false;
  bool m_HasAssoc = false;
  uint16_t m_SlotIndex = 0;
  uint16_t m_Generation = 0;
  mbedtls_ssl_context m_SSLContext;
  mbedtls_timing_delay_context m_Timer;
  uint32_t m_RemoteIp;
  uint16_t m_RemotePort;
  StormWebrtcServerImpl * m_ServerImpl;
  struct socket * m_SctpSocket;
  sctp_assoc_t m_SctpAssoc;

  std::vector<bool> m_IncStreamCreated;
  std::vector<bool> m_OutStreamCreated;
};

struct DataChannelOpenHeader
{
  uint8_t m_MessageType;
  uint8_t m_ChannelType;
  uint16_t m_Priority;
  uint32_t m_ReliabilityParameter;
  uint16_t m_LabelLength;
  uint16_t m_ProtocolLength;
};

enum DataMessageType 
{
  kNone,
  kControl,
  kBinary,
  kText,
};

class StormWebrtcServerImpl : public StormWebrtcServer
{
public:
  StormWebrtcServerImpl(const StormWebrtcServerSettings & settings);
  ~StormWebrtcServerImpl() override;

  void Update() override;
  bool PollEvent(StormWebrtcEvent & outp_event) override;
  void SendPacket(const StormWebrtcConnectionHandle & handle, const void * data, std::size_t length, std::size_t stream) override;
  void ForceDisconnect(const StormWebrtcConnectionHandle & handle) override;

protected:

  std::string LoadCertificateFile(const char * filename);

  uint64_t GetLookupIdForRemoteHost(uint32_t remote_ip, uint16_t remote_port);

  void InitConnection(StormWebrtcConnection & connection, std::size_t slot_index, uint32_t remote_ip, uint16_t remote_port);
  void CleanupConnection(StormWebrtcConnection & connection, std::size_t slot_index);

  void UpdateHandshake(StormWebrtcConnection & connection, std::size_t slot_index);

  void NotifySocketConnected(StormWebrtcConnection & connection);
  void NotifySocketDisconnected(StormWebrtcConnection & connection);

  void HandleSctpPacket(StormWebrtcConnection & connection, void * buffer, std::size_t length, int stream, int ppid);
  void HandleSctpAssociationChange(StormWebrtcConnection & connection, const sctp_assoc_change & change);

  void SendInitialDataChannels(StormWebrtcConnection & connection);
  void BootstrapConnection(StormWebrtcConnection & connection);
  void CheckConnectedState(StormWebrtcConnection & connection);

  void PrepareToRecv();
  void SendData(StormWebrtcConnection & connection, DataMessageType type, int sid, bool reliable, const void * data, std::size_t length);

private:

  friend void StormWebrtcStaticInit();

private:

  asio::io_service m_IoService;
  asio::ip::udp::socket m_ServerSocket;

  std::vector<StormWebrtcStreamType> m_InStreams;
  std::vector<StormWebrtcStreamType> m_OutStreams;

  std::queue<StormWebrtcEvent> m_InputQueue;
  std::unique_ptr<StormWebrtcConnection[]> m_Connections;
  std::unordered_map<uint64_t, std::size_t> m_ConnectionMap;
  uint32_t m_NumConnections;

#ifndef STORMWEBRTC_USE_THREADS
  std::chrono::system_clock::time_point m_LastUpdate;
#endif

  mbedtls_ssl_config m_Config;
  mbedtls_x509_crt m_Cert;
  mbedtls_pk_context m_Pk;
  mbedtls_ssl_cookie_ctx m_CookieCtx;
  mbedtls_entropy_context m_Entropy;
  mbedtls_ctr_drbg_context m_CtrDrbg;

#if defined(MBEDTLS_SSL_CACHE_C)
  mbedtls_ssl_cache_context m_Cache;
#endif

  struct socket * m_SctpListenSocket;

  void * m_SctpPacket = nullptr;
  uint32_t m_SctpPacketLength = 0;

  int m_NextConnection = 0;

  asio::ip::udp::endpoint m_RecvEndpoint;
  uint32_t m_RecvDataLen = 0;
  uint32_t m_RecvDataOffset = 0;
  bool m_GotData = false;
  static const int kRecvBufferSize = 4096;
  uint8_t m_RecvBuffer[kRecvBufferSize];
};

