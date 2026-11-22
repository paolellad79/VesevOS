#include "PsychicClient.h"
#include "PsychicHttpServer.h"
#include <lwip/sockets.h>

PsychicClient::PsychicClient(httpd_handle_t server, int socket) : _server(server),
                                                                  _socket(socket),
                                                                  _friend(NULL),
                                                                  isNew(false)
{
}

PsychicClient::~PsychicClient()
{
}

httpd_handle_t PsychicClient::server()
{
  return _server;
}

int PsychicClient::socket()
{
  return _socket;
}

// I'm not sure this is entirely safe to call.  I was having issues with race conditions when highly loaded using this.
esp_err_t PsychicClient::close()
{
  esp_err_t err = httpd_sess_trigger_close(_server, _socket);
  // PsychicHttpServer::closeCallback(_server, _socket); // call this immediately so the client is taken off the list.

  return err;
}

// Indirizzo IPv4 (ordine di rete, come esp_ip4_addr_t.addr) dell'estremo locale o remoto del socket; 0 se non si legge.
// Con lwIP in IPv6 esp_http_server apre i socket in IPv6 e l'IPv4 sta in mappatura negli ultimi 4 byte; con
// CONFIG_LWIP_IPV6=n i socket sono IPv4 puri (sockaddr_in) e la struttura IPv6 non esiste nemmeno.
static uint32_t sockIp4(int sock, bool peer)
{
#if CONFIG_LWIP_IPV6
  struct sockaddr_in6 addr;
#else
  struct sockaddr_in addr;
#endif
  socklen_t addr_size = sizeof(addr);
  int rc = peer ? getpeername(sock, (struct sockaddr*)&addr, &addr_size)
                : getsockname(sock, (struct sockaddr*)&addr, &addr_size);
  if (rc < 0) {
    ESP_LOGE(PH_TAG, "Error getting client IP");
    return 0;
  }
#if CONFIG_LWIP_IPV6
  return addr.sin6_addr.un.u32_addr[3];
#else
  return addr.sin_addr.s_addr;
#endif
}

#ifdef PSYCHIC_ARDUINO
IPAddress PsychicClient::localIP()
{
  return IPAddress(sockIp4(_socket, false));
}
#else
esp_ip4_addr_t PsychicClient::localIP()
{
  esp_ip4_addr_t address = {sockIp4(_socket, false)};
  return address;
}
#endif

uint16_t PsychicClient::localPort() const
{
  struct sockaddr_storage addr;
  socklen_t len = sizeof addr;
  getsockname(_socket, (struct sockaddr*)&addr, &len);
  struct sockaddr_in* s = (struct sockaddr_in*)&addr;
  return ntohs(s->sin_port);
}

#ifdef PSYCHIC_ARDUINO
IPAddress PsychicClient::remoteIP()
{
  return IPAddress(sockIp4(_socket, true));
}
#else
esp_ip4_addr_t PsychicClient::remoteIP()
{
  esp_ip4_addr_t address = {sockIp4(_socket, true)};
  return address;
}
#endif

uint16_t PsychicClient::remotePort() const
{
  struct sockaddr_storage addr;
  socklen_t len = sizeof addr;
  getpeername(_socket, (struct sockaddr*)&addr, &len);
  struct sockaddr_in* s = (struct sockaddr_in*)&addr;
  return ntohs(s->sin_port);
}
