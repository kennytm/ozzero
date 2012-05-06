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

/** {ZN.version ?Major ?Minor ?Patch} */
OZ_BI_define(ozzero_version, 0, 3)
{
    int major, minor, patch;
    zmq_version(&major, &minor, &patch);
    return
	(OZ_out(0) = OZ_int(major)) &&
	(OZ_out(1) = OZ_int(minor)) &&
	(OZ_out(2) = OZ_int(patch));
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
    return checked(context->close());
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

/** {ZN.getmsgopt_int +Message +Name ?Int} */
OZ_BI_define(ozzero_getmsgopt_int, 2, 1)
{
    OZ_declare(Message, 0, msg);
    OZ_declareInt(1, name);
    ENSURE_VALID(Message, msg);

    int value;
    size_t length = sizeof(value);
    if (msg->getmsgopt(name, &value, &length))
        return raise_error();
    else
        OZ_RETURN_INT(value);
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

/** {ZN.socket +Context +IntType ?Socket} */
OZ_BI_define(ozzero_socket, 2, 1)
{
    OZ_declare(Context, 0, context);
    OZ_declareInt(1, type);
    ENSURE_VALID(Context, context);

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

/** {ZN.setsockopt +Socket +IntName +ByteString} */
OZ_BI_define(ozzero_setsockopt, 3, 0)
{
    OZ_declare(Socket, 0, socket);
    ENSURE_VALID(Socket, socket);
    OZ_declareInt(1, name);
    OZ_declareByteString(2, value);
    return checked(socket->setsockopt(name, value->getData(), value->getSize()));
}
OZ_BI_end

/** {ZN.setsockopt_int +Socket +IntName +Int} */
OZ_BI_define(ozzero_setsockopt_int, 3, 0)
{
    OZ_declare(Socket, 0, socket);
    ENSURE_VALID(Socket, socket);
    OZ_declareInt(1, name);
    OZ_declareInt(2, value);
    return checked(socket->setsockopt(name, &value, sizeof(value)));
}
OZ_BI_end

/** {ZN.setsockopt_int64 +Socket +IntName +Int64} */
OZ_BI_define(ozzero_setsockopt_int64, 3, 0)
{
    OZ_declare(Socket, 0, socket);
    ENSURE_VALID(Socket, socket);
    OZ_declareInt(1, name);
    OZ_declareDetTerm(2, term);
    int64_t value = OZ_intToCint64(term);
    return checked(socket->setsockopt(name, &value, sizeof(value)));
}
OZ_BI_end

/** {ZN.setsockopt_uint64 +Socket +IntName +UInt64} */
OZ_BI_define(ozzero_setsockopt_uint64, 3, 0)
{
    OZ_declare(Socket, 0, socket);
    ENSURE_VALID(Socket, socket);
    OZ_declareInt(1, name);
    OZ_declareDetTerm(2, term);
    uint64_t value = OZ_intToCuint64(term);
    return checked(socket->setsockopt(name, &value, sizeof(value)));
}
OZ_BI_end

/** {ZN.getsockopt +Socket +IntName +MaxLength ?ByteString} */
OZ_BI_define(ozzero_getsockopt, 3, 1)
{
    OZ_declare(Socket, 0, socket);
    ENSURE_VALID(Socket, socket);
    OZ_declareInt(1, name);
    OZ_declareInt(2, max_length);

    size_t buffer_length = max_length;
    std::vector<char> buffer (buffer_length);
    if (socket->getsockopt(name, buffer.data(), &buffer_length))
        return raise_error();
    else
        OZ_RETURN(OZ_mkByteString(buffer.data(), buffer_length));
}
OZ_BI_end

/** {ZN.getsockopt_int +Socket +IntName ?Int} */
OZ_BI_define(ozzero_getsockopt_int, 2, 1)
{
    OZ_declare(Socket, 0, socket);
    ENSURE_VALID(Socket, socket);
    OZ_declareInt(1, name);

    int retval;
    size_t length = sizeof(retval);
    if (socket->getsockopt(name, &retval, &length))
        return raise_error();
    else
        OZ_RETURN_INT(retval);


}
OZ_BI_end

/** {ZN.getsockopt_int64 +Socket +IntName ?BigInt} */
OZ_BI_define(ozzero_getsockopt_int64, 2, 1)
{
    OZ_declare(Socket, 0, socket);
    ENSURE_VALID(Socket, socket);
    OZ_declareInt(1, name);

    int64_t retval;
    size_t length = sizeof(retval);
    if (socket->getsockopt(name, &retval, &length))
        return raise_error();
    else
        OZ_RETURN(OZ_int64(retval));
}
OZ_BI_end

/** {ZN.getsockopt_uint64 +Socket +IntName ?BigInt} */
OZ_BI_define(ozzero_getsockopt_uint64, 2, 1)
{
    OZ_declare(Socket, 0, socket);
    ENSURE_VALID(Socket, socket);
    OZ_declareInt(1, name);

    uint64_t retval;
    size_t length = sizeof(retval);
    if (socket->getsockopt(name, &retval, &length))
        return raise_error();
    else
        OZ_RETURN(OZ_uint64(retval));
}
OZ_BI_end

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
    if (!socket->sendmsg(*msg, flags))
        OZ_RETURN(OZ_true());
    else if (errno == EAGAIN)
        OZ_RETURN(OZ_false());
    else
        return raise_error();
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

    if (!socket->recvmsg(*msg, flags))
        OZ_RETURN(OZ_true());
    else if (errno == EAGAIN)
        OZ_RETURN(OZ_false());
    else
        return raise_error();
}
OZ_BI_end

//------------------------------------------------------------------------------

//#pragma GCC visibility pop

extern "C"
{
    OZ_C_proc_interface* oz_init_module(void)
    {
        static OZ_C_proc_interface interfaces[] = {
            {"version", 0, 3, ozzero_version},

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
            {"getmsgopt_int", 2, 1, ozzero_getmsgopt_int},

            // Socket
            {"socket", 2, 1, ozzero_socket},
            {"close", 1, 0, ozzero_close},
            {"setsockopt", 3, 0, ozzero_setsockopt},
            {"setsockopt_int", 3, 0, ozzero_setsockopt_int},
            {"setsockopt_int64", 3, 0, ozzero_setsockopt_int64},
            {"setsockopt_uint64", 3, 0, ozzero_setsockopt_uint64},
            {"getsockopt", 3, 1, ozzero_getsockopt},
            {"getsockopt_int", 2, 1, ozzero_getsockopt_int},
            {"getsockopt_int64", 2, 1, ozzero_getsockopt_int64},
            {"getsockopt_uint64", 2, 1, ozzero_getsockopt_uint64},
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

        s_catch_signals();

        return interfaces;
    }
}

}

