/*
    This file is part of ozzero.

    ozzero is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    ozzero is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
*/

// z14.cc -- ZeroMQ Oz bridge for Mozart/Oz 1.4. This only expose the C
//           interface. See ZeroMQ.oz for the actual wrapper.

#include <mozart.h>
#include <zmq.h>
#include <vector>
#include <csignal>

//#pragma GCC visibility push(hidden)
#include "ozcommon.hh"
#include "m14/bytedata.hh"

static void s_signal_handler (int signal_value)
{
printf("signal %d received\n", signal_value);
fflush(stdout);
}

static void s_catch_signals (void)
{
struct sigaction action;
action.sa_handler = s_signal_handler;
action.sa_flags = 0;
sigemptyset (&action.sa_mask);
for (int i = 1; i < 32; ++ i)
sigaction (i, &action, NULL);
}

namespace Ozzero {

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

/** Get an option value using the method (object->*getter). The type is
determined using the type 'type_term' at the position 'type_pos' in the
function. The return value will be put into 'ret_term'. */
template <typename T>
static inline OZ_Return get_opt_common(int type_pos, OZ_Term type_term,
                                       int opt_name, OZ_Term& ret_term,
                                       T* object, int (T::*getter)(int, void*, size_t*))
{
    if (OZ_isAtom(type_term))
    {
        const char* type = OZ_atomToC(type_term);

        #define READ_PRIMITIVE(TYPE) \
            if (strcmp(type, #TYPE) == 0) \
            { \
                TYPE##_t retval; \
                size_t length = sizeof(retval); \
                while ((object->*getter)(opt_name, &retval, &length)) \
                    RETURN_RAISE_ERROR_OR_RETRY; \
                ret_term = OZ_##TYPE(retval); \
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
        while ((object->*getter)(opt_name, buffer.data(), &buffer_length))
            RETURN_RAISE_ERROR_OR_RETRY;
        ret_term = OZ_mkByteString(buffer.data(), buffer_length);
        return OZ_ENTAILED;
    }

    return OZ_typeError(type_pos, "an integer or the atom 'int', 'int64' or 'uint64'");
}

template <typename T>
static inline OZ_Return set_opt_common(int type_pos, OZ_Term type_term,
                                       int opt_name, OZ_Term input_term,
                                       T* object, int (T::*setter)(int, const void*, size_t))
{
    if (OZ_isAtom(type_term))
    {
        const char* type = OZ_atomToC(type_term);

        #define WRITE_PRIMITIVE(TYPE) \
            if (strcmp(type, #TYPE) == 0) \
            { \
                TYPE##_t value = OZ_intToC##TYPE(input_term); \
                while ((object->*setter)(opt_name, &value, sizeof(value))) \
                    RETURN_RAISE_ERROR_OR_RETRY; \
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
    while ((object->*setter)(opt_name, bs->getData(), bs->getSize()))
        RETURN_RAISE_ERROR_OR_RETRY;
    return OZ_ENTAILED;
}

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
        return zmq_term(obj);
    }

    bool is_valid() const { return _obj != NULL; }
    void* socket(int type) { return zmq_socket(_obj, type); }

    virtual OZ_Term printV(int depth)
    {
        return OZ_mkTupleC("#", 3,
                           OZ_atom("<Z14.Context "),
                           OZ_unsignedLong(reinterpret_cast<uintptr_t>(_obj)),
                           OZ_atom(">"));
    }
};

/** {ZN.init +IOThreads ?Context} */
OZ_BI_define(ozzero_init, 1, 1)
{
    OZ_declareInt(0, io_threads);

    void* context = zmq_init(io_threads);
    if (context == NULL)
        return raise_error();
    else
        OZ_RETURN(OZ_extension(new Context(context)));
}
OZ_BI_end

/** {ZN.term +Context} */
OZ_BI_define(ozzero_term, 1, 0)
{
    OZ_declare(Context, 0, context);
    while (context->close())
        RETURN_RAISE_ERROR_OR_RETRY;
    return OZ_ENTAILED;
}
OZ_BI_end

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

    int getmsgopt(int name, void* value, size_t* length)
    {
        #if ZMQ_VERSION_MAJOR >= 3
            return zmq_getmsgopt(&_obj, name, value, length);
        #else
            OZ_error("To use getmsgopt, please recompile ozzero with ZeroMQ v3.");
            return -1;
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
    return OZ_raiseErrorC("zmqError", 2, OZ_int(-1), OZ_atom(buffer));
}
OZ_BI_end

/** {ZN.getmsgopt_int +Message +OptA +TypeA ?Term} */
OZ_BI_define(ozzero_getmsgopt, 3, 1)
{
    OZ_declare(Message, 0, msg);
    ENSURE_VALID(Message, msg);

    OZ_declareAtom(1, name);
    if (strcmp(name, "more") != 0)
        return OZ_typeError(1, "message option");

    OZ_declareDetTerm(2, type_term);
    return get_opt_common(2, type_term, ZMQ_MORE, OZ_out(0),
                          msg, &Message::getmsgopt);
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

    int sendmsg(Message& msg, int flags)
    {
        #if ZMQ_VERSION_MAJOR >= 3
            return zmq_sendmsg(_obj, msg.get(), flags);
        #else
            return zmq_send(_obj, msg.get(), flags);
        #endif
    }

    int recvmsg(Message& msg, int flags)
    {
        printf("...????\n\n\n");
        fflush(stdout);

        #if ZMQ_VERSION_MAJOR >= 3
            return zmq_recvmsg(_obj, msg.get(), flags);
        #else
            return zmq_recv(_obj, msg.get(), flags);
        #endif
    }


};

static inline int decode_socket_type(const char* type)
{
    if (strcmp(type, "pair") == 0) return ZMQ_PAIR;
    if (strcmp(type, "pub") == 0) return ZMQ_PUB;
    if (strcmp(type, "sub") == 0) return ZMQ_SUB;
    if (strcmp(type, "req") == 0) return ZMQ_REQ;
    if (strcmp(type, "rep") == 0) return ZMQ_REP;
    if (strcmp(type, "xreq") == 0) return ZMQ_XREQ;
    if (strcmp(type, "xrep") == 0) return ZMQ_XREP;
    if (strcmp(type, "pull") == 0) return ZMQ_PULL;
    if (strcmp(type, "push") == 0) return ZMQ_PUSH;
#if ZMQ_VERSION_MAJOR >= 3
    if (strcmp(type, "xpub") == 0) return ZMQ_XPUB;
    if (strcmp(type, "xsub") == 0) return ZMQ_XSUB;
#endif
    if (strcmp(type, "router") == 0) return ZMQ_ROUTER;
    if (strcmp(type, "dealer") == 0) return ZMQ_DEALER;

    return -1;
}

/** {ZN.socket +Context +TypeA ?Socket} */
OZ_BI_define(ozzero_socket, 2, 1)
{
    OZ_declare(Context, 0, context);
    ENSURE_VALID(Context, context);

    OZ_declareAtom(1, type);
    int real_type = decode_socket_type(type);
    if (real_type < 0)
        return OZ_typeError(1, "socket type");

    void* socket = context->socket(real_type);
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

static inline int decode_sockopt(const char* optname)
{
#if ZMQ_VERSION_MAJOR >= 3
    if (strcmp(optname, "sndhwm") == 0) return ZMQ_SNDHWM;
    if (strcmp(optname, "rcvhwm") == 0) return ZMQ_RCVHWM;
    if (strcmp(optname, "recoveryIvl") == 0) return ZMQ_RECOVERY_IVL;
    if (strcmp(optname, "recoveryIvlMsec") == 0) return ZMQ_RECOVERY_IVL;
    if (strcmp(optname, "ipv4only") == 0) return ZMQ_IPV4ONLY;
    if (strcmp(optname, "multicastHops") == 0) return ZMQ_MULTICAST_HOPS;
#else
    if (strcmp(optname, "sndhwm") == 0) return ZMQ_HWM;
    if (strcmp(optname, "rcvhwm") == 0) return ZMQ_HWM;
    if (strcmp(optname, "hwm") == 0) return ZMQ_HWM;
    if (strcmp(optname, "swap") == 0) return ZMQ_SWAP;
    if (strcmp(optname, "recoveryIvl") == 0) return ZMQ_RECOVERY_IVL;
    if (strcmp(optname, "recoveryIvlMsec") == 0) return ZMQ_RECOVERY_IVL_MSEC;
    if (strcmp(optname, "mcastLoop") == 0) return ZMQ_MCAST_LOOP;
#endif
    if (strcmp(optname, "affinity") == 0) return ZMQ_AFFINITY;
    if (strcmp(optname, "identity") == 0) return ZMQ_IDENTITY;
    if (strcmp(optname, "subscribe") == 0) return ZMQ_SUBSCRIBE;
    if (strcmp(optname, "unsubscribe") == 0) return ZMQ_UNSUBSCRIBE;
    if (strcmp(optname, "rate") == 0) return ZMQ_RATE;
    if (strcmp(optname, "sndbuf") == 0) return ZMQ_SNDBUF;
    if (strcmp(optname, "rcvbuf") == 0) return ZMQ_RCVBUF;
    if (strcmp(optname, "rcvmore") == 0) return ZMQ_RCVMORE;
    if (strcmp(optname, "fd") == 0) return ZMQ_FD;
    if (strcmp(optname, "events") == 0) return ZMQ_EVENTS;
    if (strcmp(optname, "type") == 0) return ZMQ_TYPE;
    if (strcmp(optname, "linger") == 0) return ZMQ_LINGER;
    if (strcmp(optname, "reconnectIvl") == 0) return ZMQ_RECOVERY_IVL;
    if (strcmp(optname, "backlog") == 0) return ZMQ_BACKLOG;
    if (strcmp(optname, "reconnectIvlMax") == 0) return ZMQ_RECONNECT_IVL_MAX;
    if (strcmp(optname, "maxmsgsize") == 0) return ZMQ_MAXMSGSIZE;
    if (strcmp(optname, "rcvtimeo") == 0) return ZMQ_RCVTIMEO;
    if (strcmp(optname, "sndtimeo") == 0) return ZMQ_SNDTIMEO;

    return -1;
}

/** {ZN.setsockopt +Socket +OptA +TypeA +Term}

where TypeA should be one of 'int', 'int64', 'uint64' or other things (which
will be interpreted as 'byteString').
*/
OZ_BI_define(ozzero_setsockopt, 4, 0)
{
    OZ_declare(Socket, 0, socket);
    ENSURE_VALID(Socket, socket);
    OZ_declareAtom(1, name);
    int real_name = decode_sockopt(name);
    if (real_name < 0)
        return OZ_typeError(1, "socket option");
    OZ_declareDetTerm(2, type_term);
    return set_opt_common(2, type_term, real_name, OZ_in(3),
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
    OZ_declareAtom(1, name);
    int real_name = decode_sockopt(name);
    if (real_name < 0)
        return OZ_typeError(1, "socket option");
    OZ_declareDetTerm(2, type_term);
    return get_opt_common(2, type_term, real_name, OZ_out(0),
                          socket, &Socket::getsockopt);
}
OZ_BI_end

#undef GET_SET_SOCK_COMMON

/** {ZN.bind +Socket +AddrVS} */
OZ_BI_define(ozzero_bind, 2, 0)
{
    OZ_declare(Socket, 0, socket);
    ENSURE_VALID(Socket, socket);
    OZ_declareVirtualString(1, addr);
    return checked(socket->bind(addr));
}
OZ_BI_end

/** {ZN.connect +Socket +AddrVS} */
OZ_BI_define(ozzero_connect, 2, 0)
{
    OZ_declare(Socket, 0, socket);
    ENSURE_VALID(Socket, socket);
    OZ_declareVirtualString(1, addr);
    return checked(socket->connect(addr));
}
OZ_BI_end

/** {ZN.sendmsg +Socket +Message +Flags ?Completed} */
OZ_BI_define(ozzero_sendmsg, 3, 1)
{
    OZ_declare(Socket, 0, socket);
    ENSURE_VALID(Socket, socket);
    OZ_declare(Message, 1, msg);
    ENSURE_VALID(Message, msg);
    OZ_declareInt(2, flags);
    while (true)
    {
        if (!socket->sendmsg(*msg, flags))
            OZ_RETURN(OZ_true());
        else {
            int error_number = errno;
            switch (error_number)
            {
                case EAGAIN:
                    OZ_RETURN(OZ_false());
                case EINTR:
                    break;
                default:
                    return raise_error();
            }
        }
    }
}
OZ_BI_end

/** {ZN.recvmsg +Socket Message +Flags ?Completed} */
OZ_BI_define(ozzero_recvmsg, 3, 1)
{
    OZ_declare(Socket, 0, socket);
    ENSURE_VALID(Socket, socket);
    OZ_declare(Message, 1, msg);
    ENSURE_VALID(Message, msg);
    OZ_declareInt(2, flags);

    while (true)
    {
        if (!socket->recvmsg(*msg, flags))
            OZ_RETURN(OZ_true());
        else {
            int error_number = errno;
            switch (error_number)
            {
                case EAGAIN:
                    OZ_RETURN(OZ_false());
                case EINTR:
                    break;
                default:
                    return raise_error();
            }
        }
    }
}
OZ_BI_end

//}}}
//------------------------------------------------------------------------------
//{{{ Poll


//}}}
//------------------------------------------------------------------------------

//#pragma GCC visibility pop

extern "C"
{
    OZ_C_proc_interface* oz_init_module(void)
    {
        static OZ_C_proc_interface interfaces[] = {
            {"version", 0, 1, ozzero_version},

            // Context
            {"init", 1, 1, ozzero_init},
            {"term", 1, 0, ozzero_term},

            // Message
            {"msgCreate", 0, 1, ozzero_msg_create},
            {"msgInit", 1, 0, ozzero_msg_init},
            {"msgInitSize", 2, 0, ozzero_msg_init_size},
            {"msgClose", 1, 0, ozzero_msg_close},
            {"msgData", 1, 1, ozzero_msg_data},
            {"msgMove", 2, 0, ozzero_msg_move},
            {"msgCopy", 2, 0, ozzero_msg_copy},
            {"msgSetData", 2, 0, ozzero_msg_set_data},
            {"getmsgopt", 3, 1, ozzero_getmsgopt},

            // Socket
            {"socket", 2, 1, ozzero_socket},
            {"close", 1, 0, ozzero_close},
            {"setsockopt", 4, 0, ozzero_setsockopt},
            {"getsockopt", 3, 1, ozzero_getsockopt},
            {"bind", 2, 0, ozzero_bind},
            {"connect", 2, 0, ozzero_connect},
            {"sendmsg", 3, 1, ozzero_sendmsg},
            {"recvmsg", 3, 1, ozzero_recvmsg},

            {NULL}
        };

        #define INIT(Class) g_id_##Class = oz_newUniqueId()
        INIT(Context);
        INIT(Message);
        INIT(Socket);
        #undef INIT

        //s_catch_signals();

        return interfaces;
    }
}

}

