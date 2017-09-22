#include <sys/socket.h>
#include <stdlib.h>
#include <malloc.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <assert.h>

#include "common/proto_define.h"
#include <common/proto.h>

#include "util/util.h"

#include "tracker_def.h"
#include "portal.h"

#include <set>
#include <vector>
#include <algorithm>

#include "common/protobuf/st2t_fpinfo_packet.pb.h"

#define FPINFO_BUFFER_SIZE 1024
using namespace std;

Portal* Portal::instance = NULL;

static void portal_handler(const int fd, const short which, void *arg)
{
    assert(Portal::instance != NULL);
    Portal::instance->socket_handler(fd, which, arg);
}

Portal::Portal(event_base* main_base, bool enable_uploader, StreamManager* stream_manager, ForwardServerManager *forward_manager)
    : _read_buf(NULL),
      _write_buf(NULL),
      _fd(-1),
      _state(PORTAL_CLOSED),
      _fkeepalive(NULL),
      _ev_base(main_base),
      _enable_uploader(enable_uploader),
      _info_stream_beats_cnt(0),
      _stream_manager(stream_manager),
      _forward_manager(forward_manager),
      _seq(0)
{
    instance = this;
}

Portal::~Portal()
{
    if (-1 != _fd)
        close(_fd);

    _fd = -1;
    levent_del(&_ev);

    if (_fkeepalive != NULL)
    {
        delete[] _fkeepalive;
    }

    if (_read_buf != NULL)
    {
        buffer_free(_read_buf);
        _read_buf = NULL;
    }

    if (_write_buf != NULL)
    {
        buffer_free(_write_buf);
        _write_buf = NULL;
    }
}

void Portal::destroy()
{
    if (instance != NULL)
    {
        delete instance;
        instance = NULL;
    }
}

int Portal::init()
{
    _fkeepalive = new char[sizeof(f2p_keepalive)+MAX_STREAM_CNT * sizeof(forward_stream_status)];

    _read_buf = buffer_create_max(2048, PORTAL_BUF_MAX);
    if (!_read_buf)
    {
        ERR("portal read buffer create failed.");
        return -1;
    }

    _write_buf = buffer_create_max(2048, PORTAL_BUF_MAX);
    if (!_write_buf)
    {
        ERR("portal write buffer create failed.");
        return -1;
    }

    _fd = -1;
    _state = PORTAL_CLOSED;
    levent_set(&_ev, -1, EV_READ | EV_WRITE | EV_PERSIST, NULL, NULL);

    return 0;
}

Portal* Portal::get_instance()
{
    assert(Portal::instance != NULL);
    return Portal::instance;
}

void Portal::reset()
{
    TRC("portal_reset");
    if (-1 != _fd)
        close(_fd);

    _fd = -1;
    levent_del(&_ev);
    buffer_reset(_read_buf);
    buffer_reset(_write_buf);
}

void Portal::inf_stream(stream_info* info, uint32_t cnt)
{
    set<uint32_t> temp_set;

    for (uint32_t i = 0; i < cnt; i++)
    {
        temp_set.insert(info[i].streamid);
    }

    vector<uint32_t> removal_vec;

    StreamMap& stream_map = _stream_manager->stream_map();

    for (StreamMap::iterator it = stream_map.begin(); it != stream_map.end(); it++)
    {
        uint32_t stream_id = it->first;
        if (temp_set.find(stream_id) == temp_set.end())
        {
            removal_vec.push_back(stream_id);
        }
    }

    for (uint32_t i = 0; i < removal_vec.size(); i++)
    {
        uint32_t stream_id = removal_vec[i];
        _stream_manager->erase(stream_id);
    }

    for (uint32_t i = 0; i < cnt; i++)
    {
        StreamItem item;
        item.stream_id = info[i].streamid;
        item.start_time.tv_sec = info[i].start_time.tv_sec;
        item.start_time.tv_usec = info[i].start_time.tv_usec;

        DBG("Portal::inf_stream: streamid: %032x", item.stream_id);
        _stream_manager->insert(item);
    }
    INF("Portal::inf_stream:_stream_manager->size: %d", _stream_manager->stream_map().size());
}

void Portal::parse_cmd()
{
    if (buffer_data_len(_read_buf) >= sizeof(proto_header))
    {
        proto_header h;

        if (0 != decode_header(_read_buf, &h))
        {
            WRN("portal decode header failed. reseting...");
            reset();
            return;
        }

        if (buffer_data_len(_read_buf) < h.size)
            return;

        switch (h.cmd)
        {
        case CMD_P2F_CLOSE_STREAM:
        {
            if (0 != decode_p2f_close_stream(_read_buf))
            {
                WRN("decode p2f_close_stream failed. reseting...");
                reset();
                return;
            }
            p2f_close_stream *body =
                (p2f_close_stream *)(buffer_data_ptr(_read_buf) +
                sizeof(proto_header));

            _stream_manager->erase(body->streamid);
            _destroy_stream_handler(body->streamid);
            INF("portal close stream. streamid = %u", body->streamid);
            break;
        }
        case CMD_P2F_INF_STREAM_V2:
        {
            int ret = decode_p2f_inf_stream_v2(_read_buf);
            if (ret)
            {
                if (ret == -3)
                {
                    return;
                }
                else
                {
                    WRN("decode p2f_inf_stream_v2 failed. reseting...");
                    reset();
                    return;
                }
            }

            p2f_inf_stream_v2 *body =
                (p2f_inf_stream_v2 *)(buffer_data_ptr(_read_buf) + sizeof(proto_header));

            if (body->cnt > MAX_STREAM_CNT)
            {
                WRN("portal inf stream cnt too large. cnt = %hu, max = %d",
                    body->cnt, MAX_STREAM_CNT);
            }

            uint32_t count = min(MAX_STREAM_CNT, (int)body->cnt);
            inf_stream(body->stream_infos, count);
            DBG("portal inf stream. cnt = %hu", count);
            _stream_manager->make_stream_manager_ready();

            _info_stream_beats_cnt++;
            if (_info_stream_beats_cnt % 100 == 0)
            {
                INF("portal inf stream for 100 times. cnt = %hu", body->cnt);
            }
            break;
        }
        case CMD_P2F_START_STREAM_V2:
        {
            if (0 != decode_p2f_start_stream_v2(_read_buf))
            {
                WRN("decode p2f_start_stream_v2 failed. reseting...");
                reset();
                return;
            }
            p2f_start_stream_v2 *body =
                (p2f_start_stream_v2 *)(buffer_data_ptr(_read_buf) +
                sizeof(proto_header));

            StreamItem item;
            item.stream_id = body->streamid;
            item.start_time.tv_sec = body->start_time.tv_sec;
            item.start_time.tv_usec = body->start_time.tv_usec;

            _stream_manager->insert(item);
            _create_stream_handler(item);
            INF("portal start stream. streamid = %u", body->streamid);
            break;
        }
        case CMD_T2ST_FPINFO_RSP:
        {
            if (0 != decode_t2st_fpinfo_rsp(_read_buf))
            {
                WRN("Portal::parse_cmd:decode_t2st_fpinfo_rsp failed. reseting...");
                reset();
                return;
            }

            st2t_fpinfo_packet fpinfos;

            bool ret = fpinfos.ParseFromArray(buffer_data_ptr(_read_buf)+sizeof(proto_header), h.size-sizeof(proto_header));
            if (!ret)
            {
                WRN("Portal::parse_cmd:fpinfos.ParseFromArray failed.");
            }
            else
            {
                if (NULL != _forward_manager)
                {
                    _forward_manager->fm_fpinfo_update(fpinfos);
                }
            }

            break;
        }
        default:
        {
            ERR("unexpected portal cmd = %hu, reseting...", h.cmd);
            break;
        }
        }

        buffer_eat(_read_buf, h.size);
    }
}

void Portal::_disable_write()
{
    TRC("disable portal write");
    if (PORTAL_CONNECTED == _state) {
        levent_del(&_ev);
        levent_set(&_ev, _fd, EV_READ | EV_PERSIST,
            portal_handler, NULL);
        event_base_set(_ev_base, &_ev);
        levent_add(&_ev, 0);
    }
}

void Portal::_enable_write()
{
    TRC("enable portal write");
    if (PORTAL_CONNECTED == _state) {
        levent_del(&_ev);
        levent_set(&_ev, _fd, EV_READ | EV_WRITE | EV_PERSIST,
            portal_handler, NULL);
        event_base_set(_ev_base, &_ev);
        levent_add(&_ev, 0);
    }
}

void Portal::keepalive()
{
    if (PORTAL_CONNECTED != _state)
    {
        return;
    }

    TRC("keepalive to portal");

    int ret = 0;
    uint32_t *psize = NULL;
    uint32_t size = 0;
    mt2t_proto_header tracker_header;

    memset(&tracker_header, 0, sizeof(mt2t_proto_header));

    ret = encode_header_v3_s(_write_buf, CMD_MT2T_REQ_RSP, psize);
    if (ret < 0 || psize == NULL)
    {
        ERR("Portal::keepalive: encode_header_v3_s error");
        buffer_reset(_write_buf);
        return;
    }
    size = sizeof(proto_header);

    tracker_header.seq = _seq++;
    ret = encode_mt2t_header(_write_buf, tracker_header);
    if (ret < 0)
    {
        ERR("Portal::keepalive: encode_mt2t_header error");
        buffer_reset(_write_buf);
        return;
    }
    size += sizeof(mt2t_proto_header);

    // get all stream
    ret = encode_f2p_white_list_req(_write_buf);
    if (ret < 0)
    {
        ERR("Portal::keepalive: encode_f2p_white_list_req error");
        buffer_reset(_write_buf);
        return;
    }
    size += sizeof(proto_header);
    *psize = htonl(size);

    psize = NULL;
    ret = encode_header_v3_s(_write_buf, CMD_MT2T_REQ_RSP, psize);
    if (ret < 0 || psize == NULL)
    {
        ERR("Portal::keepalive: encode_header_v3_s error");
        buffer_reset(_write_buf);
        return;
    }
    size = sizeof(proto_header);

    tracker_header.seq = _seq++;
    ret = encode_mt2t_header(_write_buf, tracker_header);
    if (ret < 0)
    {
        ERR("Portal::keepalive: encode_mt2t_header error");
        buffer_reset(_write_buf);
        return;
    }
    size += sizeof(mt2t_proto_header);

    if (0 != encode_st2t_fpinfo_req(_write_buf))
    {
        ERR("Portal::get_fp_info: encode_st2t_fpinfo_req failed.");
        buffer_reset(_write_buf);
        return;
    }
    size += sizeof(proto_header);

    *psize = htonl(size);

    _enable_write();
}

void Portal::register_create_stream(void(*handler)(StreamItem&))
{
    _create_stream_handler = handler;
}

void Portal::register_destroy_stream(void(*handler)(uint32_t))
{
    _destroy_stream_handler = handler;
}

void Portal::register_f2p_keepalive_handler(f2p_keepalive* (*handler)(f2p_keepalive*))
{
    _f2p_keepalive_handler = handler;
}

void Portal::socket_handler(const int fd, const short which, void *arg)
{
    TRC("portal handler fd = %d", fd);
    int r = 0;

    if (which & EV_READ)
    {
        r = buffer_read_fd_max(_read_buf, fd, 1024 * 4);
        if (r < 0)
        {
            reset();
        }
        else
        {
            uint32_t ph_and_mt2tph_size = sizeof(proto_header)+sizeof(mt2t_proto_header);

            DBG("Portal::socket_handler: before while, buffer_data_len: %d", \
                        buffer_data_len(_read_buf));
            while (buffer_data_len(_read_buf) >= ph_and_mt2tph_size+sizeof(proto_header))
            {
                buffer_eat(_read_buf, ph_and_mt2tph_size);

                DBG("Portal::socket_handler: buffer_data_len: %d", \
                        buffer_data_len(_read_buf));
                parse_cmd();
            }
            buffer_adjust(_read_buf);
        }
    }
    else if (which & EV_WRITE)
    {
        if (PORTAL_CONNECTING == _state)
        {
            char c;

            if (0 != read(_fd, &c, 0))
            {
                INF("connect to portal failed. error = %s", strerror(errno));
                reset();
            }
            else {
                INF("portal connected...");
                _state = PORTAL_CONNECTED;
                levent_del(&_ev);
                levent_set(&_ev, _fd, EV_READ | EV_WRITE
                    | EV_PERSIST, portal_handler, NULL);
                event_base_set(_ev_base, &_ev);
                levent_add(&_ev, 0);

                buffer_reset(_read_buf);
                buffer_reset(_write_buf);

                keepalive();
                return;
            }
        }
        else if (PORTAL_CONNECTED == _state)
        {
            TRC("portal connected write...");
            if (buffer_data_len(_write_buf) == 0)
            {
                _disable_write();
                return;
            }
            r = buffer_write_fd(_write_buf, _fd);
            if (r < 0)
            {
                INF("portal disconnected...");
                reset();
            }

            TRC("portal writed fd...remain len = %zu", buffer_data_len(_write_buf));
            buffer_try_adjust(_write_buf);
        }
        else
        {
            WRN("unexpected write event when PORTAL_START, reseting...");
            buffer_reset(_write_buf);
        }
    }
}

void Portal::check_connection(tracker_config* config)
{
    TRC("check portal connection");
    if (_fd >= 0)
        return;

    _fd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == _fd)
    {
        ERR("portal_fd socket failed. error = %s", strerror(errno));
        return;
    }

    if (0 != util_set_nonblock(_fd, TRUE))
    {
        close(_fd);
        _fd = -1;
        ERR("set nonblock failed.");
        return;
    }
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    if (0 == inet_aton(config->portal_ip, &addr.sin_addr))
    {
        close(_fd);
        _fd = -1;
        ERR("inet_aton failed. portal_host = %s", config->portal_ip);
        return;
    }
    addr.sin_port = htons(config->portal_port);
    int r = connect(_fd, (struct sockaddr *) &addr, sizeof(addr));

    if (-1 == r
        && EINPROGRESS != errno
        && EALREADY != errno
        && EWOULDBLOCK != errno)
    {
        ERR("connect failed. error = %s", strerror(errno));
        close(_fd);
        _fd = -1;
        return;
    }
    if (-1 == r)
    {
        INF("portal connecting...");
        _state = PORTAL_CONNECTING;
        levent_set(&_ev, _fd, EV_WRITE | EV_PERSIST, portal_handler, NULL);
    }
    else
    {
        // r == 0, connection success
        INF("portal connected success.");
        _state = PORTAL_CONNECTED;
        levent_set(&_ev, _fd, EV_READ | EV_WRITE | EV_PERSIST, portal_handler, NULL);
        //keepalive();
    }
    event_base_set(_ev_base, &_ev);
    levent_add(&_ev, 0);
}