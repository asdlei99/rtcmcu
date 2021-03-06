﻿#include "connection_manager/RtpConnectionManager.h"

#include "connection_manager/RtpConnection.h"
#include "../util/log.h"
#include "../util/util.h"
#include "cmd_protocol/proto.h"
#include "../util/flv.h"
#include "rtp_trans/rtp_trans_manager.h"
#include "avformat/rtcp.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include "../util/access.h"
#include "../util/util.h"
#include "assert.h"
#include "perf.h"
#include "config/RtpUploaderConfig.h"
#include "config/RtpPlayerConfig.h"
#include "media_manager/rtp2flv_remuxer.h"
#include "media_manager/rtp_block_cache.h"
#include "network/base_http_server.h"
#include "common_defs.h"

#define MAX_LEN_PER_READ (1024 * 128)

namespace {
  int bindUdpSocket(const char *local_ip, uint16_t local_port) {
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(local_ip);
    addr.sin_port = htons(local_port);
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
      return -1;
    }

    fcntl(fd, F_SETFD, FD_CLOEXEC);

    int ret = util_set_nonblock(fd, TRUE);
    if (ret < 0) {
      close(fd);
      return -1;
    }

    int opt = 1;
    ret = setsockopt(fd, IPPROTO_IP, IP_PKTINFO, &opt, sizeof(opt));
    if (ret == -1) {
      close(fd);
      return -1;
    }

    ret = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret == -1) {
      close(fd);
      return -1;
    }
    return fd;
  }

  class RtpSdpHttpHandler : public RtpCacheManager::RtpCacheWatcher {
  public:
    static RtpSdpHttpHandler* Instance();
    static void DestroyInstance();
    RtpSdpHttpHandler();

  protected:
    static void HttpPutSdpHander(struct evhttp_request *req, void *arg);
    static void HttpDownloadSdpHander(struct evhttp_request *req, void *arg);
    static void OnHttpConnectionClosed(struct evhttp_connection *connection, void *arg);
    virtual void OnSdp(const StreamId_Ext& streamid, const char *sdp);

    static std::map<uint32_t, std::set<struct evhttp_request*> > m_sdp_requests;
    static RtpSdpHttpHandler* m_inst;
  };

  RtpSdpHttpHandler* RtpSdpHttpHandler::m_inst = NULL;

  RtpSdpHttpHandler* RtpSdpHttpHandler::Instance() {
    if (m_inst) {
      return m_inst;
    }
    m_inst = new RtpSdpHttpHandler();
    return m_inst;
  }

  void RtpSdpHttpHandler::DestroyInstance() {
    delete m_inst;
    m_inst = NULL;
  }

  RtpSdpHttpHandler::RtpSdpHttpHandler() {
    HttpServerManager::Instance()->AddHandler("/upload/sdp", &RtpSdpHttpHandler::HttpPutSdpHander, NULL);
    HttpServerManager::Instance()->AddHandler("/download/sdp", &RtpSdpHttpHandler::HttpDownloadSdpHander, NULL);
    RTPTransManager::Instance()->GetRtpCacheManager()->AddWatcher(this);
  }

  void RtpSdpHttpHandler::HttpPutSdpHander(struct evhttp_request *req, void *arg) {
    const struct evhttp_uri *uri = evhttp_request_get_evhttp_uri(req);
    if (uri == NULL) {
      return;
    }
    const char *query = evhttp_uri_get_query(uri);
    if (query == NULL) {
      return;
    }
    struct evkeyvalq headers;
    if (evhttp_parse_query_str(query, &headers) < 0) {
      return;
    }
    const char *streamid = evhttp_find_header(&headers, "streamid");
    if (streamid == NULL || !streamid[0]) {
      return;
    }

    StreamId_Ext stream_id;
    if (!util_check_hex(streamid, 32) || (stream_id.parse(streamid, 16) != 0)) {
      ERR("wrong path, cannot get stream id.");
      evhttp_send_reply(req, HTTP_NOTFOUND, "Not Found", NULL);
      return;
    }

    struct evbuffer *req_data = evhttp_request_get_input_buffer(req);
    std::string sdp((char*)evbuffer_pullup(req_data, -1), evbuffer_get_length(req_data));
    RTPTransManager::Instance()->set_sdp_str(stream_id, sdp);
    INF("http put sdp complete, stream_id=%s, sdp=%s", stream_id.unparse().c_str(), sdp.c_str());
    evhttp_send_reply(req, HTTP_OK, "OK", NULL);
    return;
  }

  static void ReplySdpHttpDownload(struct evhttp_request *req, const char *sdp) {
    struct evkeyvalq *response_headers = evhttp_request_get_output_headers(req);
    evhttp_add_header(response_headers, "Server", "zzy21");
    evhttp_add_header(response_headers, "Content-Type", "application/sdp");
    evhttp_add_header(response_headers, "Expires", "-1");
    evhttp_add_header(response_headers, "Cache-Control", "private, max-age=0");
    evhttp_add_header(response_headers, "Pragma", "no-cache");
    evhttp_add_header(response_headers, "Connection", "Close");
    struct evbuffer *buf = evbuffer_new();
    evbuffer_add(buf, sdp, strlen(sdp));
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
    evbuffer_free(buf);
  }

  static int GetStreamidFromHttpRequest(struct evhttp_request *req, StreamId_Ext &stream_id) {
    const struct evhttp_uri *uri = evhttp_request_get_evhttp_uri(req);
    if (uri == NULL) {
      return -1;
    }
    const char *query = evhttp_uri_get_query(uri);
    if (query == NULL) {
      return -1;
    }
    struct evkeyvalq query_headers;
    if (evhttp_parse_query_str(query, &query_headers) < 0) {
      return -1;
    }
    const char *streamid = evhttp_find_header(&query_headers, "streamid");
    if (streamid == NULL || !streamid[0]) {
      return -1;
    }

    if (!util_check_hex(streamid, 32) || (stream_id.parse(streamid, 16) != 0)) {
      ERR("wrong path, cannot get stream id.");
      evhttp_send_reply(req, HTTP_NOTFOUND, "Not Found", NULL);
      return -1;
    }
    return 0;
  }

  void RtpSdpHttpHandler::HttpDownloadSdpHander(struct evhttp_request *req, void *arg) {
    StreamId_Ext stream_id;
    if (GetStreamidFromHttpRequest(req, stream_id) < 0) {
      ERR("wrong path, cannot get stream id.");
      evhttp_send_reply(req, HTTP_NOTFOUND, "Not Found", NULL);
      return;
    }

    std::string sdp = RTPTransManager::Instance()->get_sdp_str(stream_id);
    if (sdp.length() > 0) {
      ReplySdpHttpDownload(req, sdp.c_str());
    }
    else {
      struct evhttp_connection *connection = evhttp_request_get_connection(req);
      if (connection == NULL) {
        return;
      }
      evhttp_connection_set_closecb(connection, OnHttpConnectionClosed, req);
      INF("no sdp info, wait for relay, streamid = %u", stream_id.get_32bit_stream_id());
      m_sdp_requests[stream_id.get_32bit_stream_id()].insert(req);
    }
  }

  void RtpSdpHttpHandler::OnHttpConnectionClosed(struct evhttp_connection *connection, void *arg) {
    struct evhttp_request *req = (struct evhttp_request *)arg;
    StreamId_Ext streamid;
    if (GetStreamidFromHttpRequest(req, streamid) >= 0) {
      auto it = m_sdp_requests.find(streamid.get_32bit_stream_id());
      if (it != m_sdp_requests.end()) {
        std::set<struct evhttp_request*> &requests = it->second;
        requests.erase(req);
        if (requests.empty()) {
          m_sdp_requests.erase(it);
        }
      }
    }
    evhttp_request_free(req);
  }

  void RtpSdpHttpHandler::OnSdp(const StreamId_Ext& streamid, const char *sdp) {
    auto it = m_sdp_requests.find(streamid.get_32bit_stream_id());
    if (it == m_sdp_requests.end()) {
      return;
    }
    std::set<struct evhttp_request*> &requests = it->second;
    if (sdp) {
      for (auto it2 = requests.begin(); it2 != requests.end(); it2++) {
        struct evhttp_connection *connection = evhttp_request_get_connection(*it2);
        if (connection) {
          evhttp_connection_set_closecb(connection, NULL, NULL);
        }
        ReplySdpHttpDownload(*it2, sdp);
      }
    }
    else {
      for (auto it2 = requests.begin(); it2 != requests.end(); it2++) {
        struct evhttp_connection *connection = evhttp_request_get_connection(*it2);
        if (connection) {
          evhttp_connection_set_closecb(connection, NULL, NULL);
        }
        evhttp_send_reply(*it2, HTTP_NOTFOUND, "Not Found", NULL);
        // TODO: zhangle, 可以增加信息，告知什么失败（例如未开播等）
      }
    }
    m_sdp_requests.erase(it);
  }

  std::map<uint32_t, std::set<struct evhttp_request*> > RtpSdpHttpHandler::m_sdp_requests;
}

//////////////////////////////////////////////////////////////////////////

int RtpManagerBase::SendRtcp(RtpConnection *c, const avformat::RtcpPacket *rtcp) {
  unsigned char buf[2048];

  unsigned char *rtcp_buf = buf + sizeof(proto_header)+STREAM_ID_LEN;
  int rtcp_max_len = sizeof(buf)-sizeof(proto_header)-STREAM_ID_LEN;
  size_t rtcp_len = 0;
  rtcp->Build(rtcp_buf, &rtcp_len, (size_t)rtcp_max_len);

  int total_len = sizeof(proto_header)+STREAM_ID_LEN + rtcp_len;
  uint32_t len = 0;
  encode_header_uint8(buf, len, CMD_RTCP_U2R_PACKET, total_len);
  memcpy(buf + sizeof(proto_header), &(c->streamid), STREAM_ID_LEN);

  SendData(c, buf, total_len);
  return 0;
}

int RtpManagerBase::SendRtp(RtpConnection *c, const unsigned char *rtp, int rtp_len, uint32_t ssrc) {
  unsigned char buf[2048];
  int total_len = sizeof(proto_header)+STREAM_ID_LEN + rtp_len;
  uint32_t len = 0;
  encode_header_uint8(buf, len, CMD_RTP_D2P_PACKET, total_len);
  memcpy(buf + sizeof(proto_header), &(c->streamid), STREAM_ID_LEN);

  unsigned char *rtp_buf = buf + sizeof(proto_header)+STREAM_ID_LEN;
  memcpy(rtp_buf, rtp, rtp_len);

  if (ssrc != 0) {
    // hack ssrc for player
    avformat::RTP_FIXED_HEADER* rtp_header = (avformat::RTP_FIXED_HEADER*)rtp_buf;
    rtp_header->set_ssrc(ssrc);
  }

  SendData(c, buf, total_len);
  return 0;
}

int RtpManagerBase::SendData(RtpConnection *c, const unsigned char *data, int len) {
  if (len <= 0) {
    return 0;
  }
  if (c->udp) {
    ((RtpUdpServerManager*)c->manager)->SendUdpData(c, data, len);
  }
  else {
    buffer_expand_capacity(c->wb, len);
    buffer_append_ptr(c->wb, data, len);
  }
  c->EnableWrite();
  return 0;
}

int RtpManagerBase::OnReadPacket(RtpConnection *c, buffer *buf) {
  if (buffer_data_len(buf) < sizeof(proto_header)) {
    if (c->udp) {
      return -1;
    }
    else {
      return 0;
    }
  }
  proto_header h;
  if (0 != decode_header(buf, &h)) {
    WRN("recv, decode packet header failed.");
    return -2;
  }
  if (h.size >= buffer_max(buf)) {
    WRN("recv, pakcet too len, size = %u, (%s:%d)",
      h.size, c->remote_ip, (int)c->remote.sin_port);
    return -3;
  }
  if (c->udp) {
    if (buffer_data_len(buf) != h.size) {
      return -4;
    }
  }
  else {
    if (buffer_data_len(buf) < h.size) {
      return 0;
    }
  }

  c->recv_bytes += h.size;
  c->recv_bytes_by_min += h.size;

  switch (h.cmd) {
  case CMD_RTP_U2R_REQ_STATE:
    c->type = RtpConnection::CONN_TYPE_UPLOADER;
    {
      rtp_u2r_req_state req;
      rtp_u2r_rsp_state rsp;
      if (0 != decode_rtp_u2r_req_state(&req/*, NULL*/, buf)) {
        return -5;
      }
      rsp.version = req.version;
      memmove(&rsp.streamid, &req.streamid, sizeof(req.streamid));

      c->streamid = req.streamid;
      INF("receive upload request, streamid=%s, remote_ip=%s", c->streamid.c_str(), c->remote_ip);

      RTPUploaderConfig *config = (RTPUploaderConfig *)ConfigManager::get_inst_config_module("rtp_uploader");
      if (0 != RTPTransManager::Instance()->_open_trans(c, &config->get_rtp_trans_config())) {
        return -6;
      }

      rsp.result = U2R_RESULT_SUCCESS;

      uint8_t rsp_buf[256];
      uint16_t rsp_len = sizeof(rsp_buf);
      if (0 != encode_rtp_u2r_rsp_state_uint8(&rsp, rsp_buf, rsp_len)) {
        return -7;
      }
      SendData(c, rsp_buf, rsp_len);
    }
    break;
  case CMD_RTP_D2P_REQ_STATE:
    c->type = RtpConnection::CONN_TYPE_PLAYER;
    {
      rtp_d2p_req_state req;
      rtp_d2p_rsp_state rsp;
      if (0 != decode_rtp_d2p_req_state(&req/*, NULL*/, buf)) {
        return -1;
      }
      rsp.version = req.version;
      memmove(&rsp.streamid, &req.streamid, sizeof(req.streamid));

      c->streamid = req.streamid;
      INF("receive download request, streamid=%s, remote_ip=%s", c->streamid.c_str(), c->remote_ip);

      RTPPlayerConfig *config = (RTPPlayerConfig *)ConfigManager::get_inst_config_module("rtp_player");
      if (0 != RTPTransManager::Instance()->_open_trans(c, &config->get_rtp_trans_config())) {
        return -5;
      }

      rsp.result = U2R_RESULT_SUCCESS;

      uint8_t rsp_buf[256];
      uint16_t rsp_len = sizeof(rsp_buf);
      if (0 != encode_rtp_d2p_rsp_state_uint8(&rsp, rsp_buf, rsp_len)) {
        return -6;
      }
      SendData(c, rsp_buf, rsp_len);
    }
    break;
  case CMD_RTP_D2P_RSP_STATE:   // pull client
  case CMD_RTP_U2R_RSP_STATE:   // push client
    break;
  case CMD_RTP_D2P_PACKET:      // pull client
  case CMD_RTP_U2R_PACKET:
    if (c->type == RtpConnection::CONN_TYPE_UNKOWN) {
      return -5;
    }
    {
      int head_len = sizeof(proto_header)+sizeof(rtp_u2r_packet_header);
      RTPTransManager::Instance()->OnRecvRtp(c, buffer_data_ptr(buf) + head_len, h.size - head_len);
    }
    break;
  case CMD_RTCP_U2R_PACKET:
  case CMD_RTCP_D2P_PACKET:
    if (c->type == RtpConnection::CONN_TYPE_UNKOWN) {
      return -5;
    }
    {
      int head_len = sizeof(proto_header)+sizeof(rtp_u2r_packet_header);
      RTPTransManager::Instance()->OnRecvRtcp(c, buffer_data_ptr(buf) + head_len, h.size - head_len);
    }
    break;
  default:
    return -6;
  }

  if (!c->udp) {
    buffer_ignore(buf, h.size);
  }
  return 1;
}

RtpManagerBase::RtpManagerBase() {
  RtpSdpHttpHandler::Instance();
}

RtpManagerBase::~RtpManagerBase() {
}

//////////////////////////////////////////////////////////////////////////

RtpTcpManager::RtpTcpManager() {
  //memset(&m_ev_timer, 0, sizeof(m_ev_timer);
}

RtpTcpManager::~RtpTcpManager() {
}

RtpConnection* RtpTcpManager::CreateConnection(struct sockaddr_in *remote, int socket) {
  RtpConnection *c = new RtpConnection();
  c->udp = false;
  c->remote = *remote;
  strcpy(c->remote_ip, inet_ntoa(c->remote.sin_addr));
  c->rb = buffer_create_max(2048, 5 * 1024 * 1024/*m_config->buffer_max*/);
  c->wb = buffer_create_max(2048, 5 * 1024 * 1024/*m_config->buffer_max*/);
  c->ev_socket.Start(m_ev_base, socket, &RtpTcpManager::OnSocketData, c);
  c->manager = this;
  c->create_time = time(NULL);
  //c->set_active();
  //INF("uploader accepted. socket=%d, remote=%s:%d", c->fd_socket, c->remote_ip,
  //  (int)c->remote.sin_port);

  if (m_connections.empty()) {
    StartTimer();
  }
  m_connections.insert(c);
  return c;
}

void RtpTcpManager::OnConnectionClosed(RtpConnection *c) {
  m_connections.erase(c);
  if (m_connections.empty()) {
    evtimer_del(&m_ev_timer);
  }
  RTPTransManager::Instance()->_close_trans(c);
}

void RtpTcpManager::OnSocketData(const int fd, const short which, void *arg) {
  RtpConnection *c = (RtpConnection*)arg;
  ((RtpTcpManager*)(c->manager))->OnSocketDataImpl(c, which);
}

void RtpTcpManager::OnSocketDataImpl(RtpConnection *c, const short which) {
  if (which & EV_READ) {
    int len = buffer_read_fd_max(c->rb, c->ev_socket.fd, MAX_LEN_PER_READ);
    if (len <= 0) {
      RtpConnection::Destroy(c);
      return;
    }

    int ret = 0;
    while (true) {
      ret = OnReadPacket(c, c->rb);
      if (ret < 0) {
        RtpConnection::Destroy(c);
        return;
      }
      else if (ret == 0) {
        break;
      }
      // 收到包了，接着循环
    }
  }

  if (which & EV_WRITE) {
    if (buffer_data_len(c->wb) > 0) {
      int len = buffer_write_fd(c->wb, c->ev_socket.fd);
      if (len < 0) {
        // TODO: zhangle, destroy c in c->OnSocket function, maybe cause crash
        RtpConnection::Destroy(c);
        return;
      }
      buffer_try_adjust(c->wb);
    }

    if (buffer_data_len(c->wb) == 0) {
      c->DisableWrite();
    }
  }
}

void RtpTcpManager::StartTimer() {
  struct timeval tv;
  evtimer_set(&m_ev_timer, OnTimer, (void *)this);
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  event_base_set(m_ev_base, &m_ev_timer);
  evtimer_add(&m_ev_timer, &tv);
}

void RtpTcpManager::OnTimer(const int fd, short which, void *arg) {
  RtpTcpManager *pThis = (RtpTcpManager*)arg;
  pThis->OnTimerImpl(fd, which);
}

void RtpTcpManager::OnTimerImpl(const int fd, short which) {
  unsigned int now = time(NULL);
  std::set<RtpConnection*> trash;
  for (auto it = m_connections.begin(); it != m_connections.end();) {
    RtpConnection *c = *it;
    if (c->type != RtpConnection::CONN_TYPE_UNKOWN) {
      it = m_connections.erase(it);
      continue;
    }
    if (now - (*it)->create_time > 5) {
      trash.insert(*it);
    }
    it++;
  }
  for (auto it = trash.begin(); it != trash.end(); it++) {
    RtpConnection::Destroy(*it);
  }
  if (!m_connections.empty()) {
    StartTimer();
  }
}

//////////////////////////////////////////////////////////////////////////

RtpUdpServerManager* RtpUdpServerManager::Instance() {
  if (m_inst) {
    return m_inst;
  }
  m_inst = new RtpUdpServerManager();
  return m_inst;
}

void RtpUdpServerManager::DestroyInstance() {
  delete m_inst;
  m_inst = NULL;
}

RtpUdpServerManager::RtpUdpServerManager() {
  m_recv_buf = buffer_create_max(MAX_RTP_UDP_BUFFER_SIZE, MAX_RTP_UDP_BUFFER_SIZE);
}

RtpUdpServerManager::~RtpUdpServerManager() {
  buffer_free(m_recv_buf);
}

int RtpUdpServerManager::Init(struct event_base *ev_base) {
  m_ev_base = ev_base;

  RTPUploaderConfig *config = (RTPUploaderConfig *)ConfigManager::get_inst_config_module("rtp_uploader");
  if (NULL == config) {
    ERR("rtp uploader failed to get corresponding config information.");
    return -1;
  }

  int fd_socket = bindUdpSocket(config->listen_ip, config->listen_udp_port);
  if (fd_socket == -1) {
    ERR("rtp uploader failed to bind udp socket.");
    return -1;
  }
  m_ev_socket.Start(ev_base, fd_socket, &RtpUdpServerManager::OnSocketData, this);

  return 0;
}

void RtpUdpServerManager::OnSocketData(const int fd, const short which, void *arg) {
  RtpUdpServerManager* pThis = (RtpUdpServerManager*)arg;
  pThis->OnSocketDataImpl(fd, which);
}

void RtpUdpServerManager::OnSocketDataImpl(const int fd, const short which) {
  if (which & EV_READ) {
    OnRead();
  }
  if (which & EV_WRITE) {
    OnWrite();
  }
}

void RtpUdpServerManager::OnRead() {
  struct sockaddr_in remote;
  struct in_addr local_ip;

  int loop = 1000;
  while (loop-- > 0) {
    buffer_reset(m_recv_buf);
    if (buffer_udp_recvmsg_fd(m_recv_buf, m_ev_socket.fd, remote, local_ip) <= 0) {
      break;
    }

    RtpConnection *c = NULL;
    auto it = m_connections.find(remote);
    if (it == m_connections.end()) {
      c = new RtpConnection();
      c->udp = true;
      c->remote = remote;
      strcpy(c->remote_ip, inet_ntoa(c->remote.sin_addr));
      c->local_ip = local_ip;
      c->manager = this;
      m_connections[c->remote] = c;
    }
    else {
      c = it->second;
    }

    if (OnReadPacket(c, m_recv_buf) < 0) {
      RtpConnection::Destroy(c);
    }
  }
}

void RtpUdpServerManager::OnWrite() {
  m_send_queue.send(m_ev_socket.fd);
  DisableWrite();
}

void RtpUdpServerManager::DisableWrite() {
  m_ev_socket.DisableWrite();
}

void RtpUdpServerManager::EnableWrite() {
  m_ev_socket.EnableWrite();
}

int RtpUdpServerManager::SendUdpData(RtpConnection *c, const unsigned char *data, int len) {
  return m_send_queue.add(c->remote.sin_addr.s_addr, c->remote.sin_port, (const char *)data, len);
}

void RtpUdpServerManager::OnConnectionClosed(RtpConnection *c) {
  m_connections.erase(c->remote);
  RTPTransManager::Instance()->_close_trans(c);
}

RtpUdpServerManager* RtpUdpServerManager::m_inst = NULL;

//////////////////////////////////////////////////////////////////////////

void LibEventSocket::Start(struct event_base* ev_base, int fd, LibEventCb cb, void *cb_arg) {
  this->ev_base = ev_base;
  this->fd = fd;
  this->cb = cb;
  this->cb_arg = cb_arg;
  util_set_nonblock(fd, TRUE);
  event_set(&ev_socket, fd, EV_READ | EV_WRITE | EV_PERSIST, cb, cb_arg);
  event_base_set(ev_base, &ev_socket);
  event_add(&ev_socket, NULL);
  w_enabled = true;
}

void LibEventSocket::Stop() {
  event_del(&ev_socket);
  close(fd);
  fd = -1;
}

void LibEventSocket::EnableWrite() {
  if (!w_enabled) {
    event_del(&ev_socket);
    event_set(&ev_socket, fd, EV_READ | EV_WRITE | EV_PERSIST, cb, cb_arg);
    event_base_set(ev_base, &ev_socket);
    event_add(&ev_socket, NULL);
    w_enabled = true;
  }
}

void LibEventSocket::DisableWrite() {
  event_del(&ev_socket);
  event_set(&ev_socket, fd, EV_READ | EV_PERSIST, cb, cb_arg);
  event_base_set(ev_base, &ev_socket);
  event_add(&ev_socket, NULL);
  w_enabled = false;
}

//////////////////////////////////////////////////////////////////////////

RtpTcpServerManager* RtpTcpServerManager::Instance() {
  if (m_inst) {
    return m_inst;
  }
  m_inst = new RtpTcpServerManager();
  return m_inst;
}

void RtpTcpServerManager::DestroyInstance() {
  delete m_inst;
  m_inst = NULL;
}

int32_t RtpTcpServerManager::Init(struct event_base *ev_base) {
  RTPUploaderConfig *config = (RTPUploaderConfig *)ConfigManager::get_inst_config_module("rtp_uploader");
  if (NULL == config) {
    ERR("rtp uploader failed to get corresponding config information.");
    return -1;
  }

  m_ev_base = ev_base;

  int fd_socket = util_create_listen_fd(config->listen_ip, config->listen_tcp_port, 128);
  if (fd_socket < 0)	{
    ERR("create rtp tcp listen fd failed. ret = %d", fd_socket);
    return -1;
  }
  m_ev_socket.Start(ev_base, fd_socket, &RtpTcpServerManager::OnSocketAccept, this);

  //start_timer();

  return 0;
}

void RtpTcpServerManager::OnSocketAccept(const int fd, const short which, void *arg) {
  RtpTcpServerManager *pThis = (RtpTcpServerManager*)arg;
  pThis->OnSocketAcceptImpl(fd, which);
}

void RtpTcpServerManager::OnSocketAcceptImpl(const int fd, const short which) {
  if (which & EV_READ) {
    struct sockaddr_in remote;
    socklen_t len = sizeof(struct sockaddr_in);
    memset(&remote, 0, len);
    int fd_socket = ::accept(fd, (struct sockaddr *)&remote, &len);
    if (fd_socket < 0) {
      WRN("uploader bin accept failed. fd_socket = %d, error = %s",
        fd_socket, strerror(errno));
      return;
    }

    CreateConnection(&remote, fd_socket);
  }
}

RtpTcpServerManager* RtpTcpServerManager::m_inst = NULL;

//////////////////////////////////////////////////////////////////////////
