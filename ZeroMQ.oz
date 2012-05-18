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

functor
import
    ZN at 'z14.so{native}'
    Finalize
    %ByteString

export
    version: Version
    context: Context
    init: Init
    poll: Poll

define
    RegisterContext = {Finalize.guardian ZN.ctxDestroy}
    RegisterSocket = {Finalize.guardian ZN.close}

    Version = {ZN.version}

    InternalInit = {NewName}
    NativeSocket = {NewName}

    fun {SelectType V2Type V3Type}
        if Version.major >= 3 then
            V3Type
        else
            V2Type
        end
    end

    %---------------------------------------------------------------------------

    SockOptTypes = r(
        type: int
        rcvmore: {SelectType int64 int}
        hwm: {SelectType uint64 int}
        sndhwm: {SelectType uint64 int}
        rcvhwm: {SelectType uint64 int}
        affinity: uint64
        rcvtimeo: int
        sndtimeo: int
        swap: int64
        identity: 255
        rate: {SelectType int64 int}
        recoveryIvl: {SelectType int64 int}
        recoveryIvlMsec: {SelectType int64 int}
        mcastLoop: int64
        sndbuf: {SelectType uint64 int}
        rcvbuf: {SelectType uint64 int}
        linger: int
        reconnectIvl: int
        reconnectIvlMax: int
        backlog: int
        fd: int
        events: {SelectType uint32 int}
        maxmsgsize: int64
        multicastHops: int
        ipv4only: int
        subscribe: 255
        unsubscribe: 255
        lastEndpoint: 255   % this can be arbitrarily long...
        failUnroutable: int
        tcpKeepalive: int
        tcpKeepaliveIdle: int
        tcpKeepaliveCnt: int
        tcpKeepaliveIntvl: int
        tcpAcceptFilter: 255    % this can be arbitrarily long...
        % 'monitor' is not supported yet.
    )

    % Wrapper of a ZeroMQ socket
    class Socket
        feat
            !NativeSocket

        meth !InternalInit(NativeContext Type)
            self.NativeSocket = {ZN.socket NativeContext Type}
            {RegisterSocket self.NativeSocket}
        end

        % close this socket
        meth close
            {ZN.close self.NativeSocket}
        end

        % set socket options
        meth set(...) = M
            {Record.forAllInd M proc {$ I A}
                OptType = SockOptTypes.I
                Value = if {IsInt OptType} then {ByteString.make A} else A end
            in
                {ZN.setsockopt self.NativeSocket I OptType Value}
            end}
        end

        % get socket options
        meth get(...) = M
            {Record.forAllInd M fun {$ I}
                {ZN.getsockopt self.NativeSocket I SockOptTypes.I $}
            end}
        end

        % send a virtual string or byte string
        meth send(VS  more:SndMore<=false)
            BS = if {IsByteString VS} then VS else {ByteString.make VS} end
            NativeMessage = {ZN.msgCreateWithData {ByteString.make BS}}
            Options = if SndMore then sndmore else nil end
        in
            {ZN.msgSend NativeMessage self.NativeSocket Options _}
            {ZN.msgClose NativeMessage}
        end

        % receive a byte string
        meth recv(?BS  more:?RcvMore<=false)
            NativeMessage = {ZN.msgCreate}
        in
            {ZN.msgInit NativeMessage}
            {ZN.msgRecv NativeMessage self.NativeSocket nil _}
            BS = {ZN.msgData NativeMessage}
            {ZN.msgClose NativeMessage}
            if {Not {IsDet RcvMore}} then
                RcvMore = {self get(rcvmore:$)} \= 0
            end
        end

        % send a multipart message
        meth sendMulti(VSL)
            case VSL
            of H|nil then
                {self send(H)}
            [] H|T then
                {self send(H  more:true)}
                {self sendMulti(T)}
            end
        end

        % receive a multipart message
        meth recvMulti($)
            BS  More  Tail
        in
            {self recv(BS  more:More)}
            Tail = if More then {self recvMulti($)} else nil end
            BS|Tail
        end

        % receive a byte string without waiting. If there is no messages yet,
        % returns 'unit'.
        meth recvDontWait(?MaybeBS)
            NativeMessage = {ZN.msgCreate}
            Completed
        in
            {ZN.msgInit NativeMessage}
            Completed = {ZN.msgRecv NativeMessage self.NativeSocket [dontwait]}
            MaybeBS = if Completed then {ZN.msgData NativeMessage} else unit end
            {ZN.msgClose NativeMessage}
        end

        % bind to an address
        meth bind(VS)
            {ZN.bind self.NativeSocket VS}
        end

        % connect to an address
        meth connect(VS)
            {ZN.connect self.NativeSocket VS}
        end

        % unbind from an address
        meth unbind(VS)
            {ZN.unbind self.NativeSocket VS}
        end

        % disconnect from an address
        meth disconnect(VS)
            {ZN.disconnect self.NativeSocket VS}
        end
    end

    %---------------------------------------------------------------------------

    % Wrapper of a ZeroMQ context
    class Context
        feat
            NativeContext

        % Initialize a context
        meth init(iothreads: IoThreads <= 1)
            self.NativeContext = {ZN.ctxNew IoThreads}
            {RegisterContext self.NativeContext}
        end

        % Create a socket
        meth socket(Type $)
            {New Socket InternalInit(self.NativeContext Type)}
        end

        % Create a socket and bind many addresses to it.
        meth bindSocket(Type AddrVSL $)
            Socket = {self socket(Type $)}
        in
            if {IsVirtualString AddrVSL} then
                {Socket bind(AddrVSL)}
            else
                for AddrVS in AddrVSL do
                    {Socket bind(AddrVS)}
                end
            end
            Socket
        end

        % Create a socket and connect it to many addresses.
        meth connectSocket(Type AddrVSL $)
            Socket = {self socket(Type $)}
        in
            if {IsVirtualString AddrVSL} then
                {Socket connect(AddrVSL)}
            else
                for AddrVS in AddrVSL do
                    {Socket connect(AddrVS)}
                end
            end
            Socket
        end

        % Terminate the context
        meth close
            {ZN.ctxDestroy self.NativeContext}
        end

        % Assign options to the context. (Call this before 'socket')
        meth set(...) = M
            {Record.forAllInd M proc {$ I A}
                {ZN.ctxSet self.NativeContext I A}
            end}
        end

        % Get options from the context.
        meth get(...) = M
            {Record.forAllInd M fun {$ I}
                {ZN.ctxGet self.NativeContext I}
            end}
        end
    end

    % Obtain a default ZeroMQ context
    fun {Init}
        {New Context init}
    end

    /*
    Example usage:

        {ZeroMQ.poll  r(
            r(socket:Socket  events:[pollin pollout]  action: proc {$ Socket Events} ... end)
            r(socket:Socket  events:pollin  action: proc {$ Socket Events} ... end)
            timeout: 5000
            completed: Completed
        )}
    */
    proc {Poll PollSpec}
        PollItems
        Timeout
        Completed
        SocketSpecs

        RealPollSpec = if {IsList PollSpec} then {List.toTuple r PollSpec} else PollSpec end

        fun {ToNative SocketSpec}
            Socket = SocketSpec.socket
            NSocket = Socket.NativeSocket
        in
            '#'(NSocket SocketSpec.events Socket#SocketSpec.action)
        end
    in
        o(PollItems Timeout Completed) = {Record.foldRInd RealPollSpec
            fun {$ I A o(PollItemsP TimeoutP CompletedP)}
                case I
                of timeout then
                    o(PollItemsP A CompletedP)
                [] completed then
                    o(PollItemsP TimeoutP A)
                else
                    if {IsInt I} then
                        if {IsList A} then
                            o({Append {Map ToNative A} PollItemsP} TimeoutP CompletedP)
                        else
                            o({ToNative A}|PollItemsP TimeoutP CompletedP)
                        end
                    else
                        {Exception.raiseError zmqError(typeError 'poll: Unexpected feature '#I)}
                        o(PollItemsP TimeoutP CompletedP)
                    end
                end
            end
        o(nil ~1 _)}

        {ZN.poll PollItems Timeout Completed SocketSpecs}

        for EventsL#(Socket#Action) in SocketSpecs do
            {Action Socket EventsL}
        end
    end
end

