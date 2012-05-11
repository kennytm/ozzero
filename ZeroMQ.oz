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

define
    RegisterContext = {Finalize.guardian ZN.term}
    RegisterSocket = {Finalize.guardian ZN.close}

    Version = {ZN.version}

    InternalInit = {NewName}

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
    )

    % Wrapper of a ZeroMQ socket
    class Socket
        feat
            NativeSocket

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
                {ZN.setsockopt self.NativeSocket I SockOptTypes.I A}
            end}
        end

        % get socket options
        meth get(...) = M
            {Record.forAllInd M proc {$ I A}
                A = {ZN.getsockopt self.NativeSocket I SockOptTypes.I $}
            end}
        end

        % send a virtual string or byte string
        meth send(VS)
            BS = if {IsByteString VS} then VS else {ByteString.make VS} end
            NativeMessage = {ZN.msgCreateWithData {ByteString.make BS}}
        in
            {ZN.sendmsg self.NativeSocket NativeMessage nil _}
            {ZN.msgClose NativeMessage}
        end

        % receive a virtual string
        meth recv(?BS)
            NativeMessage = {ZN.msgCreate}
        in
            {ZN.msgInit NativeMessage}
            {ZN.recvmsg self.NativeSocket NativeMessage nil _}
            BS = {ZN.msgData NativeMessage}
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
    end

    %---------------------------------------------------------------------------

    % Wrapper of a ZeroMQ context
    class Context
        feat
            NativeContext

        % Initialize a context
        meth init(iothreads: IoThreads <= 1)
            self.NativeContext = {ZN.init IoThreads}
            {RegisterContext self.NativeContext}
        end

        % Create a socket
        meth socket(Type $)
            {New Socket InternalInit(self.NativeContext Type)}
        end

        % Terminate the context
        meth close
            {ZN.term self.NativeContext}
        end
    end

    % Obtain a default ZeroMQ context
    fun {Init}
        {New Context init}
    end
end

