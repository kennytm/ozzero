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
    %System

export
    version: Version
    init: Init
    poll: Poll
    device: Device

define
    RegisterContext = {Finalize.guardian ZN.ctxDestroy}
    RegisterSocket = {Finalize.guardian ZN.close}

    Version = {ZN.version}

    fun {SelectType V2Type V3Type}
        if Version.major >= 3 then
            V3Type
        else
            V2Type
        end
    end

    fun {LoopFuncUntilFalse Func}
        RealRes
    in
        if {Func RealRes} then
            {LoopFuncUntilFalse Func}
        else
            RealRes
        end
    end

    proc {LoopProcUntilFalse Proc}
        if {Proc} then
            {LoopProcUntilFalse Proc}
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
    )

    %---------------------------------------------------------------------------

    proc {SendMore NativeSocket VS SndMore}
        BS = if {IsByteString VS} then VS else {ByteString.make VS} end
        NativeMessage = {ZN.msgCreateWithData {ByteString.make BS}}
        Options = if SndMore then [dontwait sndmore] else [dontwait] end
    in
        {LoopProcUntilFalse fun {$}
            Completed  Interrupted
        in
            Interrupted = {ZN.msgSend NativeMessage NativeSocket Options Completed}
            Interrupted orelse {Not Completed}
        end}
        {ZN.msgClose NativeMessage}
    end

    proc {RecvMore NativeSocket ?BS ?RcvMore}
        NativeMessage = {ZN.msgCreate}
    in
        {ZN.msgInit NativeMessage}
        {LoopProcUntilFalse fun {$}
            Completed  Interrupted
        in
            Interrupted = {ZN.msgRecv NativeMessage NativeSocket dontwait Completed}
            Interrupted orelse {Not Completed}
        end}
        BS = {ZN.msgData NativeMessage}
        {ZN.msgClose NativeMessage}
        if {Not {IsDet RcvMore}} then
            RcvMore = {ZN.getsockopt NativeSocket rcvmore SockOptTypes.rcvmore} \= 0
        end
    end

    proc {SendMulti NativeSocket VSL}
        case VSL
        of H|T then
            HasMore = T \= nil
        in
            {SendMore NativeSocket H HasMore}
            if HasMore then
                {SendMulti NativeSocket T}
            end
        end
    end

    fun {RecvMulti NativeSocket}
        BS  RcvMore  Tail
    in
        {RecvMore NativeSocket BS RcvMore}
        Tail = if RcvMore then {RecvMulti NativeSocket} else nil end
        BS|Tail
    end

    fun {ConnectOrBindSocket NativeContext Method M}
        Type = {Label M}
        AddrVSL = M.1
        Socket = {MakeSocket NativeContext Type}
    in
        {Record.forAllInd {Record.subtract M 1} Socket.set}
        if {IsVirtualString AddrVSL} then
            {Socket.Method AddrVSL}
        else
            {ForAll AddrVSL Socket.Method}
        end
        Socket
    end

    %---------------------------------------------------------------------------

    % Wrapper of a ZeroMQ socket
    fun {MakeSocket NativeContext Type}
        NativeSocket = {ZN.socket NativeContext Type}
    in
        {RegisterSocket NativeSocket}

        zmqSocket(
            NativeSocket

            % close this socket
            close: proc {$}
                {ZN.close NativeSocket}
            end

            % set a socket option
            set: proc {$ OptName Value}
                OptType = SockOptTypes.OptName
                RealValue = if {IsInt OptType} then
                    {ByteString.make Value}
                else
                    Value
                end
            in
                {LoopProcUntilFalse fun {$}
                    {ZN.setsockopt NativeSocket OptName OptType RealValue}
                end}
            end

            % get a socket option
            get: fun {$ OptName}
                {LoopFuncUntilFalse fun {$ Res}
                    Res = {ZN.getsockopt NativeSocket OptName SockOptTypes.OptName}
                    Res == unit
                end}
            end

            % send a virtual string or byte string
            send: proc {$ VS}
                {SendMore NativeSocket VS false}
            end

            % receive a byte string
            recv: fun {$}
                {RecvMore NativeSocket $ _}
            end

            % send a multipart message
            sendMulti: proc {$ VSL}
                {SendMulti NativeSocket VSL}
            end

            % receive a multipart message
            recvMulti: fun {$}
                {RecvMulti NativeSocket}
            end

            % receive a byte string without waiting. If there is no messages yet,
            % returns 'unit'.
            recvDontWait: fun {$}
                NativeMessage = {ZN.msgCreate}
                MaybeBS  Completed
            in
                {ZN.msgInit NativeMessage}
                Completed = {LoopFuncUntilFalse fun {$ R}
                    {ZN.msgRecv NativeMessage NativeSocket [dontwait] R}
                end}
                MaybeBS = if Completed then {ZN.msgData NativeMessage} else unit end
                {ZN.msgClose NativeMessage}
                MaybeBS
            end

            % bind to an address
            bind: proc {$ VS}
                {ZN.bind NativeSocket VS}
            end

            % connect to an address
            connect: proc {$ VS}
                {ZN.connect NativeSocket VS}
            end

            % unbind from an address
            unbind: proc {$ VS}
                {ZN.unbind NativeSocket VS}
            end

            % disconnect from an address
            disconnect: proc {$ VS}
                {ZN.connect NativeSocket VS}
            end
        )
    end

    % Wrapper of a ZeroMQ context
    fun {MakeContext IoThreads}
        NativeContext = {ZN.ctxNew IoThreads}
    in
        {RegisterContext NativeContext}

        zmqContext(
            % Create a socket
            socket: fun {$ Type}
                {MakeSocket NativeContext Type}
            end

            % Create a socket and bind many addresses to it.
            bind: fun {$ M}
                {ConnectOrBindSocket NativeContext bind M}
            end

            % Create a socket and connect it to many addresses.
            connect: fun {$ M}
                {ConnectOrBindSocket NativeContext connect M}
            end

            % Terminate the context
            close: proc {$}
                {LoopProcUntilFalse fun {$}
                    {ZN.ctxDestroy NativeContext}
                end}
            end

            % Assign option to the context. (Call this before 'socket')
            set: proc {$ OptName Value}
                {ZN.ctxSet NativeContext OptName Value}
            end

            % Get options from the context
            get: fun {$ OptName}
                {ZN.ctxGet NativeContext OptName}
            end
        )
    end

    % Obtain a default ZeroMQ context
    fun {Init}
        {MakeContext 1}
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

        RealPollSpec = if {IsList PollSpec} then {List.toTuple r PollSpec} else PollSpec end

        fun {ToNative SocketSpec}
            Socket = SocketSpec.socket
            NSocket = Socket.1
        in
            '#'(NSocket SocketSpec.events Socket#SocketSpec.action)
        end

        proc {DoPoll CurTimeout ?RealCompleted ?RealSocketSpecs}
            CurCompleted
            CurSocketSpecs
            NewTimeout = {ZN.poll PollItems CurTimeout CurCompleted CurSocketSpecs}
        in
            if NewTimeout \= unit then
                TheTimeout = if (NewTimeout < 0) \= (CurTimeout < 0) then 1 else NewTimeout end
            in
                {DoPoll TheTimeout RealCompleted RealSocketSpecs}
            else
                RealCompleted = CurCompleted
                RealSocketSpecs = CurSocketSpecs
            end
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

        for EventsL#(Socket#Action) in {DoPoll Timeout Completed} do
            {Action Socket EventsL}
        end
    end


    proc {Device DeviceA FrontendSocket BackendSocket}
        {LoopProcUntilFalse fun {$}
            {ZN.device DeviceA FrontendSocket.1 BackendSocket.1}
        end}
    end
end

