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

