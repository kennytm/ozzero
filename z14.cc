/*
    Copyright (c) 2012, Kenny Chan <kennytm@gmail.com>
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
*/

// z14.cc -- ZeroMQ Oz bridge for Mozart/Oz 1.4. This only expose the C
//           interface. See ZeroMQ.oz for the actual wrapper.

#include <mozart.h>
#include <zmq.h>
#include <pthread.h>
#include <time.h>
#include <vector>
#include <string>
#include <tr1/unordered_map>

//#pragma GCC visibility push(hidden)
#include "ozcommon.hh"
#include "m14/bytedata.hh"

namespace Ozzero {

//------------------------------------------------------------------------------
//{{{ Utility functions

/** Raise an Oz error. Use it like this inside an OZ_BI_define:

    if (some bad condition)
        return raise_error();
    else
        OZ_RETURN(etc);
*/
static OZ_Return raise_error()
{
    int error_number = errno;
    const char* error_message = strerror(error_number);
    const char* error_atom;
    switch (error_number)
    {
    #define DEF_CASE(ErrCode) case ErrCode: error_atom = #ErrCode; break
        DEF_CASE(EINVAL);
        DEF_CASE(EPROTONOSUPPORT);
        DEF_CASE(ENOCOMPATPROTO);
        DEF_CASE(EADDRINUSE);
        DEF_CASE(EADDRNOTAVAIL);
        DEF_CASE(ENODEV);
        DEF_CASE(ETERM);
        DEF_CASE(ENOTSOCK);
        DEF_CASE(EMTHREAD);
        DEF_CASE(EFAULT);
        DEF_CASE(EINTR);
        DEF_CASE(ENOMEM);
        DEF_CASE(EAGAIN);
        DEF_CASE(ENOTSUP);
        DEF_CASE(EFSM);
        DEF_CASE(EMFILE);
        DEF_CASE(ENOBUFS);
        DEF_CASE(ENETDOWN);
        DEF_CASE(ECONNREFUSED);
        DEF_CASE(EINPROGRESS);
        DEF_CASE(EAFNOSUPPORT);
        DEF_CASE(EHOSTUNREACH);
        default: error_atom = NULL; break;
    #undef DEF_CASE
    }

    OZ_Term err_code = error_atom == NULL ? OZ_int(error_number) : OZ_atom(error_atom);
    return OZ_raiseErrorC("zmqError", 2, err_code, OZ_atom(error_message));
}



/** Get an option value using the method (object->*getter). The type is
determined using the type 'type_term' at the position 'type_pos' in the
function. The return value will be put into 'ret_term'. */
template <typename T>
static inline OZ_Return get_opt_common(int type_pos, OZ_Term type_term,
                                       int opt_name, OZ_Term& ret_term,
                                       T* object, int (T::*getter)(int, void*, size_t*))
{
    bool is_eintr = false;

    if (OZ_isAtom(type_term))
    {
        const char* type = OZ_atomToC(type_term);

        #define READ_PRIMITIVE(TYPE) \
            if (strcmp(type, #TYPE) == 0) \
            { \
                TYPE##_t retval; \
                size_t length = sizeof(retval); \
                TRAPPING_SIGALRM(is_eintr, (object->*getter)(opt_name, &retval, &length)); \
                ret_term = is_eintr ? OZ_unit() : OZ_##TYPE(retval); \
                return OZ_ENTAILED; \
            }

        typedef int int_t;

        #define OZ_uint32 OZ_uint64

        READ_PRIMITIVE(int)
        READ_PRIMITIVE(int64)
        READ_PRIMITIVE(uint64)
        READ_PRIMITIVE(uint32)

        #undef OZ_uint32
        #undef READ_PRIMITIVE
    }
    else if (OZ_isInt(type_term))
    {
        size_t buffer_length = OZ_intToC(type_term);
        std::vector<char> buffer (buffer_length);
        TRAPPING_SIGALRM(is_eintr, (object->*getter)(opt_name, buffer.data(), &buffer_length));
        ret_term = is_eintr ? OZ_unit() : OZ_mkByteString(buffer.data(), buffer_length);
        return OZ_ENTAILED;
    }

    return OZ_typeError(type_pos, "an integer or the atom 'int', 'int64' or 'uint64'");
}

template <typename T>
static inline OZ_Return set_opt_common(int type_pos, OZ_Term type_term,
                                       int opt_name, OZ_Term input_term,
                                       OZ_Term& interrupted_term,
                                       T* object, int (T::*setter)(int, const void*, size_t))
{
    bool is_eintr = false;

    if (OZ_isAtom(type_term))
    {
        const char* type = OZ_atomToC(type_term);

        #define WRITE_PRIMITIVE(TYPE) \
            if (strcmp(type, #TYPE) == 0) \
            { \
                TYPE##_t value = OZ_intToC##TYPE(input_term); \
                TRAPPING_SIGALRM(is_eintr, (object->*setter)(opt_name, &value, sizeof(value))); \
                interrupted_term = is_eintr ? OZ_true() : OZ_false(); \
                return OZ_ENTAILED; \
            }

        typedef int int_t;
        #define OZ_intToCint OZ_intToC
        #define OZ_intToCuint32 (uint32_t)OZ_intToCulong

        WRITE_PRIMITIVE(int)
        WRITE_PRIMITIVE(int64)
        WRITE_PRIMITIVE(uint64)
        WRITE_PRIMITIVE(uint32)

        #undef WRITE_PRIMITIVE
        #undef OZ_intToCuint32
        #undef OZ_intToCint
    }

    ByteString* bs = tagged2ByteString(input_term);

    TRAPPING_SIGALRM(is_eintr, (object->*setter)(opt_name, bs->getData(), bs->getSize()));
    interrupted_term = is_eintr ? OZ_true() : OZ_false();
    return OZ_ENTAILED;
}

#define RETURN_WRONG_VERSION(funcname, reqver) \
    OZ_error("To use " #funcname ", please recompile with ZeroMQ v" reqver " or above."); \
    return -1


//{{{ Atom to integers

struct AtomDecoder
{
    std::tr1::unordered_map<std::string, int> socket_type_map;
    std::tr1::unordered_map<std::string, int> sockopt_map;
    std::tr1::unordered_map<std::string, int> ctx_getset_map;
    std::tr1::unordered_map<std::string, int> msg_getset_map;
    std::tr1::unordered_map<std::string, int> send_recv_flags_map;
    std::tr1::unordered_map<std::string, short> poll_events_map;
    std::tr1::unordered_map<std::string, int> device_type_map;

    AtomDecoder()
    {
        socket_type_map.insert(std::make_pair("pair", ZMQ_PAIR));
        socket_type_map.insert(std::make_pair("pub", ZMQ_PUB));
        socket_type_map.insert(std::make_pair("sub", ZMQ_SUB));
        socket_type_map.insert(std::make_pair("req", ZMQ_REQ));
        socket_type_map.insert(std::make_pair("rep", ZMQ_REP));
        socket_type_map.insert(std::make_pair("xreq", ZMQ_XREQ));
        socket_type_map.insert(std::make_pair("xrep", ZMQ_XREP));
        socket_type_map.insert(std::make_pair("pull", ZMQ_PULL));
        socket_type_map.insert(std::make_pair("push", ZMQ_PUSH));
        socket_type_map.insert(std::make_pair("router", ZMQ_ROUTER));
        socket_type_map.insert(std::make_pair("dealer", ZMQ_DEALER));
    #if ZMQ_VERSION >= 30100
        socket_type_map.insert(std::make_pair("xpub", ZMQ_XPUB));
        socket_type_map.insert(std::make_pair("xsub", ZMQ_XSUB));
    #endif

    #if ZMQ_VERSION >= 30100
        sockopt_map.insert(std::make_pair("sndhwm", ZMQ_SNDHWM));
        sockopt_map.insert(std::make_pair("rcvhwm", ZMQ_RCVHWM));
        sockopt_map.insert(std::make_pair("recoveryIvlMsec", ZMQ_RECOVERY_IVL));
        sockopt_map.insert(std::make_pair("ipv4only", ZMQ_IPV4ONLY));
        sockopt_map.insert(std::make_pair("multicastHops", ZMQ_MULTICAST_HOPS));
    #if ZMQ_VERSION >= 30101
        sockopt_map.insert(std::make_pair("lastEndpoint", ZMQ_LAST_ENDPOINT));
        sockopt_map.insert(std::make_pair("failUnroutable", ZMQ_FAIL_UNROUTABLE));
        sockopt_map.insert(std::make_pair("tcpKeepalive", ZMQ_TCP_KEEPALIVE));
        sockopt_map.insert(std::make_pair("tcpKeepaliveCnt", ZMQ_TCP_KEEPALIVE_CNT));
        sockopt_map.insert(std::make_pair("tcpKeepaliveIdle", ZMQ_TCP_KEEPALIVE_IDLE));
        sockopt_map.insert(std::make_pair("tcpKeepaliveIntvl", ZMQ_TCP_KEEPALIVE_INTVL));
        sockopt_map.insert(std::make_pair("tcpAcceptFilter", ZMQ_TCP_ACCEPT_FILTER));
        sockopt_map.insert(std::make_pair("monitor", ZMQ_MONITOR));
    #endif
    #else
        sockopt_map.insert(std::make_pair("sndhwm", ZMQ_HWM));
        sockopt_map.insert(std::make_pair("rcvhwm", ZMQ_HWM));
        sockopt_map.insert(std::make_pair("hwm", ZMQ_HWM));
        sockopt_map.insert(std::make_pair("swap", ZMQ_SWAP));
        sockopt_map.insert(std::make_pair("recoveryIvlMsec", ZMQ_RECOVERY_IVL_MSEC));
        sockopt_map.insert(std::make_pair("mcastLoop", ZMQ_MCAST_LOOP));
    #endif
        sockopt_map.insert(std::make_pair("recoveryIvl", ZMQ_RECOVERY_IVL));
        sockopt_map.insert(std::make_pair("affinity", ZMQ_AFFINITY));
        sockopt_map.insert(std::make_pair("identity", ZMQ_IDENTITY));
        sockopt_map.insert(std::make_pair("subscribe", ZMQ_SUBSCRIBE));
        sockopt_map.insert(std::make_pair("unsubscribe", ZMQ_UNSUBSCRIBE));
        sockopt_map.insert(std::make_pair("rate", ZMQ_RATE));
        sockopt_map.insert(std::make_pair("sndbuf", ZMQ_SNDBUF));
        sockopt_map.insert(std::make_pair("rcvbuf", ZMQ_RCVBUF));
        sockopt_map.insert(std::make_pair("rcvmore", ZMQ_RCVMORE));
        sockopt_map.insert(std::make_pair("fd", ZMQ_FD));
        sockopt_map.insert(std::make_pair("events", ZMQ_EVENTS));
        sockopt_map.insert(std::make_pair("type", ZMQ_TYPE));
        sockopt_map.insert(std::make_pair("linger", ZMQ_LINGER));
        sockopt_map.insert(std::make_pair("reconnectIvl", ZMQ_RECOVERY_IVL));
        sockopt_map.insert(std::make_pair("backlog", ZMQ_BACKLOG));
        sockopt_map.insert(std::make_pair("reconnectIvlMax", ZMQ_RECONNECT_IVL_MAX));
        sockopt_map.insert(std::make_pair("maxmsgsize", ZMQ_MAXMSGSIZE));
        sockopt_map.insert(std::make_pair("rcvtimeo", ZMQ_RCVTIMEO));
        sockopt_map.insert(std::make_pair("sndtimeo", ZMQ_SNDTIMEO));

    #if ZMQ_VERSION >= 30101
        ctx_getset_map.insert(std::make_pair("ioThreads", ZMQ_IO_THREADS));
        ctx_getset_map.insert(std::make_pair("maxSockets", ZMQ_MAX_SOCKETS));
    #endif

    #if ZMQ_VERSION >= 30100
        msg_getset_map.insert(std::make_pair("more", ZMQ_MORE));
    #endif

        send_recv_flags_map.insert(std::make_pair("sndmore", ZMQ_SNDMORE));
    #if ZMQ_VERSION >= 30100
        send_recv_flags_map.insert(std::make_pair("dontwait", ZMQ_DONTWAIT));
        send_recv_flags_map.insert(std::make_pair("noblock", ZMQ_DONTWAIT));
    #else
        send_recv_flags_map.insert(std::make_pair("dontwait", ZMQ_NOBLOCK));
        send_recv_flags_map.insert(std::make_pair("noblock", ZMQ_NOBLOCK));
    #endif

        poll_events_map.insert(std::make_pair("pollin", ZMQ_POLLIN));
        poll_events_map.insert(std::make_pair("pollout", ZMQ_POLLOUT));
        poll_events_map.insert(std::make_pair("pollerr", ZMQ_POLLERR));

        device_type_map.insert(std::make_pair("queue", ZMQ_QUEUE));
        device_type_map.insert(std::make_pair("forwarder", ZMQ_FORWARDER));
        device_type_map.insert(std::make_pair("streamer", ZMQ_STREAMER));
    }
};

static AtomDecoder g_atom_decoder;

//}}}

class Socket;
class Message;

static OZ_Return send_or_recv(Socket* socket, Message* msg, OZ_Term flags_term,
                              int (Message::*method)(Socket&, int),
                              OZ_Term& retval, OZ_Term& interrupted);

//}}}
//------------------------------------------------------------------------------
//{{{ Versioning

/** {ZN.version ?VersionRC} */
OZ_BI_define(ozzero_version, 0, 1)
{
    int major, minor, patch;
    zmq_version(&major, &minor, &patch);

    OZ_Term props[] = {
        OZ_pairAI("major", major),
        OZ_pairAI("minor", minor),
        OZ_pairAI("patch", patch),
    };
    OZ_Term prop_list = OZ_toList(sizeof(props)/sizeof(*props), props);
    OZ_RETURN(OZ_recordInitC("version", prop_list));
}
OZ_BI_end

//}}}
//------------------------------------------------------------------------------
//{{{ Context

int g_id_Context;
class Context : public Extension<Context, void*, g_id_Context>
{
public:
    explicit Context(void* obj) : Extension(obj) {}

    int close()
    {
        void* obj = _obj;
        if (obj == NULL)
            return 0;
        _obj = NULL;
    #if ZMQ_VERSION >= 30101
        return zmq_ctx_destroy(obj);
    #else
        return zmq_term(obj);
    #endif
    }

    bool is_valid() const { return _obj != NULL; }
    void* socket(int type) { return zmq_socket(_obj, type); }

    int get(int option)
    {
    #if ZMQ_VERSION >= 30101
        return zmq_ctx_get(_obj, option);
    #else
        RETURN_WRONG_VERSION(zmq_ctx_get, "3.1.1");
    #endif
    }

    int set(int option, int value)
    {
    #if ZMQ_VERSION >= 30101
        return zmq_ctx_set(_obj, option, value);
    #else
        RETURN_WRONG_VERSION(zmq_ctx_set, "3.1.1");
    #endif
    }

    virtual OZ_Term printV(int depth)
    {
        return OZ_mkTupleC("#", 3,
                           OZ_atom("<Z14.Context "),
                           OZ_unsignedLong(reinterpret_cast<uintptr_t>(_obj)),
                           OZ_atom(">"));
    }
};

/** {ZN.ctxNew +IOThreads ?Context} */
OZ_BI_define(ozzero_ctx_new, 1, 1)
{
    OZ_declareInt(0, io_threads);

    void* context;
#if ZMQ_VERSION >= 30101
    context = zmq_ctx_new();
#else
    context = zmq_init(io_threads);
#endif

    if (context == NULL)
        return raise_error();

#if ZMQ_VERSION >= 30101
    if (io_threads != ZMQ_IO_THREADS_DFLT)
        zmq_ctx_set(context, ZMQ_IO_THREADS, io_threads);
#endif

    OZ_RETURN(OZ_extension(new Context(context)));
}
OZ_BI_end

/** {ZN.ctxDestroy +Context ?Interrupted} */
OZ_BI_define(ozzero_ctx_destroy, 1, 1)
{
    OZ_declare(Context, 0, context);
    bool is_eintr = false;
    TRAPPING_SIGALRM(is_eintr, context->close());
    OZ_RETURN(is_eintr ? OZ_true() : OZ_false());
}
OZ_BI_end

/** {ZN.ctxGet +Context +OptA ?ResI} */
OZ_BI_define(ozzero_ctx_get, 2, 1)
{
    OZ_declare(Context, 0, context);
    ENSURE_VALID(Context, context);
    OZ_declareAndDecode(g_atom_decoder.socket_type_map, "socket type", 1, option);

    int res = context->get(option);
    if (res < 0)
        return raise_error();
    else
        OZ_RETURN_INT(res);
}
OZ_BI_end

/** {ZN.ctxSet +Context +OptA +ValI} */
OZ_BI_define(ozzero_ctx_set, 3, 0)
{
    OZ_declare(Context, 0, context);
    ENSURE_VALID(Context, context);
    OZ_declareAndDecode(g_atom_decoder.socket_type_map, "socket type", 1, option);
    OZ_declareInt(2, value);

    return checked(context->set(option, value));
}
OZ_BI_end

//}}}
//------------------------------------------------------------------------------
//{{{ Socket

int g_id_Socket;
class Socket : public Extension<Socket, void*, g_id_Socket>
{
public:
    explicit Socket(void* obj) : Extension(obj) {}

    int close() {
        void* obj = _obj;
        if (obj == NULL)
            return 0;
        _obj = NULL;
        return zmq_close(obj);
    }

    virtual OZ_Term printV(int depth)
    {
        return OZ_mkTupleC("#", 3,
                           OZ_atom("<Z14.Socket "),
                           OZ_unsignedLong(reinterpret_cast<uintptr_t>(_obj)),
                           OZ_atom(">"));
    }

    bool is_valid() const { return _obj != NULL; }

    int setsockopt(int name, const void* value, size_t length) { return zmq_setsockopt(_obj, name, value, length); }
    int getsockopt(int name, void* value, size_t* length) { return zmq_getsockopt(_obj, name, value, length); }
    int bind(const char* addr) { return zmq_bind(_obj, addr); }
    int connect(const char* addr) { return zmq_connect(_obj, addr); }

    int unbind(const char* addr)
    {
    #if ZMQ_VERSION >= 30101
        return zmq_unbind(_obj, addr);
    #else
        RETURN_WRONG_VERSION(zmq_unbind, "3.1.1");
    #endif
    }

    int disconnect(const char* addr)
    {
    #if ZMQ_VERSION >= 30101
        return zmq_disconnect(_obj, addr);
    #else
        RETURN_WRONG_VERSION(zmq_disconnect, "3.1.1");
    #endif
    }
};

/** {ZN.socket +Context +TypeA ?Socket} */
OZ_BI_define(ozzero_socket, 2, 1)
{
    OZ_declare(Context, 0, context);
    ENSURE_VALID(Context, context);
    OZ_declareAndDecode(g_atom_decoder.socket_type_map, "socket type", 1, type);

    void* socket = context->socket(type);
    if (socket == NULL)
        return raise_error();
    else
        OZ_RETURN(OZ_extension(new Socket(socket)));
}
OZ_BI_end

/** {ZN.close +Socket} */
OZ_BI_define(ozzero_close, 1, 0)
{
    OZ_declare(Socket, 0, socket);
    return checked(socket->close());
}
OZ_BI_end

/** {ZN.setsockopt +Socket +OptA +TypeA +Term}

where TypeA should be one of 'int', 'int64', 'uint64' or other things (which
will be interpreted as 'byteString').
*/
OZ_BI_define(ozzero_setsockopt, 4, 1)
{
    OZ_declare(Socket, 0, socket);
    ENSURE_VALID(Socket, socket);
    OZ_declareAndDecode(g_atom_decoder.sockopt_map, "socket option", 1, real_name);
    OZ_declareDetTerm(2, type_term);
    return set_opt_common(2, type_term, real_name, OZ_in(3), OZ_out(0),
                          socket, &Socket::setsockopt);
}
OZ_BI_end

/** {ZN.getsockopt +Socket +OptA +TypeA ?Term}

where TypeA should be one of 'int', 'int64', 'uint64' or an integer (which will
be interpreted as a 'byteString' with a maximum length).
*/
OZ_BI_define(ozzero_getsockopt, 3, 1)
{
    OZ_declare(Socket, 0, socket);
    ENSURE_VALID(Socket, socket);
    OZ_declareAndDecode(g_atom_decoder.sockopt_map, "socket option", 1, real_name);
    OZ_declareDetTerm(2, type_term);
    return get_opt_common(2, type_term, real_name, OZ_out(0),
                          socket, &Socket::getsockopt);
}
OZ_BI_end

/** {ZN.bind +Socket +AddrVS}
    {ZN.connect +Socket +AddrVS}
    {ZN.unbind +Socket +AddrVS}
    {ZN.disconnect +Socket +AddrVS}
*/
#define DEFINE_CONNECT_FUNC(NAME) \
    OZ_BI_define(ozzero_##NAME, 2, 0) \
    { \
        OZ_declare(Socket, 0, socket); \
        ENSURE_VALID(Socket, socket); \
        OZ_declareVirtualString(1, addr); \
        return checked(socket->NAME(addr)); \
    } \
    OZ_BI_end

DEFINE_CONNECT_FUNC(bind)
DEFINE_CONNECT_FUNC(connect)
DEFINE_CONNECT_FUNC(unbind)
DEFINE_CONNECT_FUNC(disconnect)

#undef DEFINE_CONNECT_FUNC

//}}}
//------------------------------------------------------------------------------
//{{{ Message

int g_id_Message;
class Message : public ExtensionBase<Message, zmq_msg_t, g_id_Message>
{
private:
    bool _closed;

    Message(zmq_msg_t* src, bool by_copy) : _closed(false)
    {
        int rc = by_copy ? zmq_msg_copy(&_obj, src) : zmq_msg_move(&_obj, src);
        if (rc == 0)
            return;
        char errstr[48];
        snprintf(errstr, sizeof(errstr), "Fail to copy message (Error #%d)", errno);
        Assert(buffer);
    }

public:
    virtual OZ_Extension* gCollectV() { return new Message(&_obj, /*by_copy*/false); }
    virtual OZ_Extension* sCloneV() { return new Message(&_obj, /*by_copy*/true); }
    // ^ should we allow this? Or do an Assert(0)?

    Message() : _closed(true) {}

    int close()
    {
        if (_closed)
            return 0;
        _closed = true;
        return zmq_msg_close(&_obj);
    }

    bool is_valid() const { return !_closed; }

    int init()
    {
        int rc = zmq_msg_init(&_obj);
        _closed = (rc != 0);
        return rc;
    }

    int init_size(size_t size)
    {
        int rc = zmq_msg_init_size(&_obj, size);
        _closed = (rc != 0);
        return rc;
    }

    // What about zmq_msg_init_data?

    size_t size() { return zmq_msg_size(&_obj); }
    void* data() { return zmq_msg_data(&_obj); }
    int copy(Message& other) { return zmq_msg_copy(&_obj, &other._obj); }
    int move(Message& other) { return zmq_msg_move(&_obj, &other._obj); }

    void set_data(const void* new_data, size_t new_size)
    {
        void* old_data = zmq_msg_data(&_obj);
        memcpy(old_data, new_data, new_size);
    }

    int get(int name)
    {
    #if ZMQ_VERSION >= 30101
        return zmq_msg_get(&_obj, name);
    #elif ZMQ_VERSION >= 30100
        int retval;
        size_t length = sizeof(retval);
        int errcode = zmq_getmsgopt(&_obj, name, &retval, &length);
        return errcode < 0 ? errcode : retval;
    #else
        RETURN_WRONG_VERSION(zmq_msg_get, "3.1.0");
    #endif
    }

    int set(int name, int value)
    {
    #if ZMQ_VERSION >= 30101
        return zmq_msg_set(&_obj, name, value);
    #else
        RETURN_WRONG_VERSION(zmq_msg_set, "3.1.1");
    #endif
    }

    int recv(Socket& socket, int flags)
    {
    #if ZMQ_VERSION >= 30101
        return zmq_msg_recv(&_obj, socket._obj, flags);
    #elif ZMQ_VERSION >= 30100
        return zmq_recvmsg(socket._obj, &_obj, flags);
    #else
        return zmq_recv(socket._obj, &_obj, flags);
    #endif
    }

    int send(Socket& socket, int flags)
    {
    #if ZMQ_VERSION >= 30101
        return zmq_msg_send(&_obj, socket._obj, flags);
    #elif ZMQ_VERSION >= 30100
        return zmq_sendmsg(socket._obj, &_obj, flags);
    #else
        return zmq_send(socket._obj, &_obj, flags);
    #endif
    }

    virtual OZ_Term printV(int depth)
    {
        return OZ_mkTupleC("#", 3,
                           OZ_atom("<Z14.Message "),
                           OZ_mkByteString(static_cast<char*>(data()), size()),
                           OZ_atom(">"));
    }
};

/** {ZN.msgInit +Message} */
OZ_BI_define(ozzero_msg_init, 1, 0)
{
    OZ_declare(Message, 0, msg);
    return checked(msg->init());
}
OZ_BI_end

/** {ZN.msgInitSize +Size +Message} */
OZ_BI_define(ozzero_msg_init_size, 2, 0)
{
    OZ_declare(Message, 0, msg);
    OZ_declareLong(1, size);
    if (size < 0)
        return OZ_typeError(0, "nonnegative integer");
    return checked(msg->init_size(static_cast<size_t>(size)));
}
OZ_BI_end

/** {ZN.msgCreate ?Message} */
OZ_BI_define(ozzero_msg_create, 0, 1)
{
    OZ_RETURN(OZ_extension(new Message()));
}
OZ_BI_end

/** {ZN.msgClose +Message} */
OZ_BI_define(ozzero_msg_close, 1, 0)
{
    OZ_declare(Message, 0, msg);
    return checked(msg->close());
}
OZ_BI_end

/** {ZN.msgSize +Message ?Size} */
OZ_BI_define(ozzero_msg_size, 1, 1)
{
    OZ_declare(Message, 0, msg);
    ENSURE_VALID(Message, msg);
    OZ_RETURN_INT(msg->size());
}
OZ_BI_end

/** {ZN.msgData +Message ?DataByteString} */
OZ_BI_define(ozzero_msg_data, 1, 1)
{
    OZ_declare(Message, 0, msg);
    ENSURE_VALID(Message, msg);
    const char* data = static_cast<const char*>(msg->data());
    OZ_RETURN(OZ_mkByteString(data, msg->size()));
}
OZ_BI_end

/** {ZN.msgMove +DestMessage +SrcMessage} */
OZ_BI_define(ozzero_msg_move, 2, 0)
{
    OZ_declare(Message, 0, dest);
    ENSURE_VALID(Message, dest);
    OZ_declare(Message, 1, src);
    ENSURE_VALID(Message, src);
    return checked(dest->move(*src));
}
OZ_BI_end

/** {ZN.msgCopy +DestMessage +SrcMessage} */
OZ_BI_define(ozzero_msg_copy, 2, 0)
{
    OZ_declare(Message, 0, dest);
    ENSURE_VALID(Message, dest);
    OZ_declare(Message, 1, src);
    ENSURE_VALID(Message, src);
    return checked(dest->copy(*src));
}
OZ_BI_end

/** {ZN.msgSetData +Message +ByteString} */
OZ_BI_define(ozzero_msg_set_data, 2, 0)
{
    OZ_declare(Message, 0, msg);
    OZ_declareByteString(1, data);
    ENSURE_VALID(Message, msg);
    size_t size = data->getSize();
    size_t required_size = msg->size();
    if (size == required_size)
    {
        msg->set_data(data->getData(), size);
        return OZ_ENTAILED;
    }

    char buffer[100];
    snprintf(buffer, sizeof(buffer),
             "Invalid data size: requiring %zu, but %zu is provided",
             required_size, size);
    return OZ_raiseErrorC("zmqError", 2, OZ_atom("invalidDataSize"), OZ_string(buffer));
}
OZ_BI_end

/** {ZN.msgGet +Message +OptA ?RetI} */
OZ_BI_define(ozzero_msg_get, 2, 1)
{
    OZ_declare(Message, 0, msg);
    ENSURE_VALID(Message, msg);
    OZ_declareAndDecode(g_atom_decoder.msg_getset_map, "message option", 1, option);

    int res = msg->get(option);
    if (res < 0)
        return raise_error();
    else
        return res;
}
OZ_BI_end

/** {ZN.msgSet +Message +OptA +ValueI} */
OZ_BI_define(ozzero_msg_set, 3, 0)
{
    OZ_declare(Message, 0, msg);
    ENSURE_VALID(Message, msg);
    OZ_declareAndDecode(g_atom_decoder.msg_getset_map, "message option", 1, option);
    OZ_declareInt(2, value);

    return checked(msg->set(option, value));
}
OZ_BI_end


/** {ZN.msgCreateWithData +ByteString ?Message} */
OZ_BI_define(ozzero_msg_create_with_data, 1, 1)
{
    OZ_declareByteString(0, data);
    Message* msg = new Message();
    size_t size = data->getSize();
    msg->init_size(size);
    msg->set_data(data->getData(), size);
    OZ_RETURN(OZ_extension(msg));
}
OZ_BI_end

/** {ZN.msgSend +Message +Socket +FlagsL ?Completed ?Interrupted} */
OZ_BI_define(ozzero_msg_send, 3, 2)
{
    OZ_declare(Message, 0, msg);
    OZ_declare(Socket, 1, socket);
    OZ_declareDetTerm(2, flags_term);
    return send_or_recv(socket, msg, flags_term, &Message::send,
                        OZ_out(0), OZ_out(1));
}
OZ_BI_end

/** {ZN.msgRecv +Message +Socket +FlagsL ?Completed ?Interrupted} */
OZ_BI_define(ozzero_msg_recv, 3, 2)
{
    OZ_declare(Message, 0, msg);
    OZ_declare(Socket, 1, socket);
    OZ_declareDetTerm(2, flags_term);
    return send_or_recv(socket, msg, flags_term, &Message::recv,
                        OZ_out(0), OZ_out(1));
}
OZ_BI_end

static OZ_Return send_or_recv(Socket* socket, Message* msg, OZ_Term flags_term,
                              int (Message::*method)(Socket&, int),
                              OZ_Term& retval, OZ_Term& interrupted)
{
    ENSURE_VALID(Socket, socket);
    ENSURE_VALID(Message, msg);

    int flags;
    PARSE_FLAGS(g_atom_decoder.send_recv_flags_map, "send/recv options",
                2, flags_term, flags);

    interrupted = OZ_false();
    if ((msg->*method)(*socket, flags) >= 0)
    {
        retval = OZ_true();
        return OZ_ENTAILED;
    }
    else {
        int error_number = errno;
        switch (error_number)
        {
            case EAGAIN:
                retval = OZ_false();
                return OZ_ENTAILED;
            case EINTR:
                if (!am.isSetSFlag(SigPending))
                {
                    interrupted = OZ_true();
                    return OZ_ENTAILED;
                }
                // else fallthrough
            default:
                return raise_error();
        }
    }
}

//}}}
//------------------------------------------------------------------------------
//{{{ Poll

/** {ZN.poll
        ['#'(+Socket +EventsL Action) ...]
        +Timeout
        ?Completed
        ['#'(?Socket ?EventsL Action) ...]
        ?InterruptedNewTimeout} */
OZ_BI_define(ozzero_poll, 2, 3)
{
    OZ_declareDetTerm(0, poll_items_term);
    OZ_declareLong(1, timeout);

    OZ_Term socket_atom = OZ_atom("socket");
    OZ_Term events_atom = OZ_atom("events");
    OZ_Term action_atom = OZ_atom("action");

    std::vector<zmq_pollitem_t> poll_items;
    std::vector<OZ_Term> actions;

    while (OZ_isCons(poll_items_term))
    {
        OZ_Term poll_item_term = OZ_head(poll_items_term);
        if (!OZ_isTuple(poll_item_term) || OZ_width(poll_item_term) != 3)
            return OZ_typeError(0, "list of 3-tuples");

        OZ_Term socket_term = OZ_getArg(poll_item_term, 0);
        OZ_Term events_term = OZ_getArg(poll_item_term, 1);
        OZ_Term action_term = OZ_getArg(poll_item_term, 2);

        Socket* socket = Socket::coerce(socket_term);
        ENSURE_VALID(Socket, socket);
        short events;
        PARSE_FLAGS(g_atom_decoder.poll_events_map, "'pollin' or 'pollout'",
                    0, events_term, events);

        zmq_pollitem_t poll_item;
        poll_item.socket = socket->_obj;
        poll_item.fd = 0;
        poll_item.events = events;
        poll_items.push_back(poll_item);

        actions.push_back(action_term);

        poll_items_term = OZ_tail(poll_items_term);
    }

    int result_count;

    size_t poll_items_count = poll_items.size();

    bool is_eintr = false;
    timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    TRAPPING_SIGALRM(is_eintr,
        result_count = zmq_poll(poll_items.data(), poll_items_count, timeout)
    );
    if (!is_eintr)
    {
        OZ_out(2) = OZ_unit();
    }
    else
    {
        if (timeout > 0)
        {
            timespec end_time;
            clock_gettime(CLOCK_MONOTONIC, &end_time);
            long time_elapsed = (end_time.tv_sec - start_time.tv_sec) * 1000;
            time_elapsed += (end_time.tv_nsec - start_time.tv_nsec) / 1000000;
            OZ_out(2) = OZ_long(timeout - time_elapsed);
        }
        else
        {
            OZ_out(2) = OZ_long(timeout);
        }
    }
    OZ_out(0) = (result_count == 0) ? OZ_false() : OZ_true();

    if (result_count <= 0)
    {
        OZ_out(1) = OZ_nil();
    }
    else
    {
        std::vector<OZ_Term> result_terms;
        result_terms.reserve(result_count);
        OZ_Term pollin_atom = OZ_atom("pollin");
        OZ_Term pollout_atom = OZ_atom("pollout");
        OZ_Term pollerr_atom = OZ_atom("pollerr");
        for (size_t i = 0; i < poll_items_count; ++ i)
        {
            short revents = poll_items[i].revents;
            if (revents == 0)
                continue;

            size_t revents_count = 0;
            OZ_Term revents_terms[3];
            if (revents & ZMQ_POLLIN)
                revents_terms[revents_count++] = pollin_atom;
            if (revents & ZMQ_POLLOUT)
                revents_terms[revents_count++] = pollout_atom;
            if (revents & ZMQ_POLLERR)
                revents_terms[revents_count++] = pollerr_atom;

            OZ_Term revents_term = OZ_toList(revents_count, revents_terms);
            result_terms.push_back(OZ_pair2(revents_term, actions[i]));
        }
        OZ_out(1) = OZ_toList(result_terms.size(), result_terms.data());
    }
    return OZ_ENTAILED;
}
OZ_BI_end

//}}}
//------------------------------------------------------------------------------
//{{{ Device

/** {ZN.device +DeviceA +FrontendSocket +BackendSocket ?Interrupted} */
OZ_BI_define(ozzero_device, 3, 1)
{
    OZ_declareAndDecode(g_atom_decoder.device_type_map, "device type", 0, device);
    OZ_declare(Socket, 1, frontend);
    ENSURE_VALID(Socket, frontend);
    OZ_declare(Socket, 2, backend);
    ENSURE_VALID(Socket, backend);

    int rc;
    bool is_eintr = false;
    TRAPPING_SIGALRM(is_eintr, rc = zmq_device(device, frontend->_obj, backend->_obj));
    if (!is_eintr && rc)
        return raise_error();
    else
        OZ_RETURN(is_eintr ? OZ_true() : OZ_false());
}
OZ_BI_end

//}}}

//#pragma GCC visibility pop

extern "C"
{
    OZ_C_proc_interface* oz_init_module(void)
    {
        static OZ_C_proc_interface interfaces[] = {
            {"version", 0, 1, ozzero_version},

            // Context
            {"ctxNew", 1, 1, ozzero_ctx_new},
            {"ctxDestroy", 1, 1, ozzero_ctx_destroy},
            {"ctxGet", 2, 1, ozzero_ctx_get},
            {"ctxSet", 3, 0, ozzero_ctx_set},

            // Socket
            {"socket", 2, 1, ozzero_socket},
            {"close", 1, 0, ozzero_close},
            {"setsockopt", 4, 1, ozzero_setsockopt},
            {"getsockopt", 3, 1, ozzero_getsockopt},
            {"bind", 2, 0, ozzero_bind},
            {"connect", 2, 0, ozzero_connect},
            {"unbind", 2, 0, ozzero_unbind},
            {"disconnect", 2, 0, ozzero_disconnect},

            // Message
            {"msgCreate", 0, 1, ozzero_msg_create},
            {"msgInit", 1, 0, ozzero_msg_init},
            {"msgInitSize", 2, 0, ozzero_msg_init_size},
            {"msgClose", 1, 0, ozzero_msg_close},
            {"msgData", 1, 1, ozzero_msg_data},
            {"msgMove", 2, 0, ozzero_msg_move},
            {"msgCopy", 2, 0, ozzero_msg_copy},
            {"msgSetData", 2, 0, ozzero_msg_set_data},
            {"msgGet", 2, 1, ozzero_msg_get},
            {"msgSet", 3, 0, ozzero_msg_set},
            {"msgCreateWithData", 1, 1, ozzero_msg_create_with_data},
            {"msgSend", 3, 2, ozzero_msg_send},
            {"msgRecv", 3, 2, ozzero_msg_recv},

            {"poll", 2, 3, ozzero_poll},
            {"device", 3, 1, ozzero_device},

            {NULL}
        };

        #define INIT(Class) g_id_##Class = oz_newUniqueId()
        INIT(Context);
        INIT(Message);
        INIT(Socket);
        #undef INIT

        return interfaces;
    }
}

}

