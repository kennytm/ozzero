functor
import
    ZeroMQ
    ZN at 'z14.so{native}'
    System
    Application
define
    Version
    Context
    Socket
    Message
in
    Version = {ZeroMQ.getVersion}
    {System.show Version}

    Context = {ZN.init 1}
    {System.show Context}

    case {Application.getArgs plain}
    of "server"|_ then
            {System.showInfo 'server'}

            Socket = {ZN.socket Context 4}  % 4 = ZMQ_REP
            {ZN.bind Socket 'inproc://*:5555'}
            {System.show Socket}

            {System.showInfo '... bound'}

            Message = {ZN.msgCreate}
            {ZN.msgInit Message}
            _ = {ZN.recvmsg Socket Message 0}
            {System.show 'server: '#{ZN.msgData Message}}
            {ZN.msgClose Message}

            {System.showInfo '... received'}

            {Delay 1000}

            {ZN.msgInitSize Message 5}
            {ZN.msgSetData Message {ByteString.make "World"}}
            _ = {ZN.sendmsg Socket Message 0}
            {ZN.msgClose Message}

            {System.showInfo '... sent'}

            {ZN.close Socket}

            {System.showInfo '... closed'}

    [] "client"|_ then
            {System.showInfo 'client'}

            Socket = {ZN.socket Context 3}  % 3 = ZMQ_REQ
            {ZN.connect Socket 'inproc://localhost:5555'}

            Message = {ZN.msgCreate}
            {ZN.msgInitSize Message 5}
            {ZN.msgSetData Message {ByteString.make "Hello"}}
            {System.show Message}
            _ = {ZN.sendmsg Socket Message 0}
            {ZN.msgClose Message}

            {ZN.msgInit Message}
            _ = {ZN.recvmsg Socket Message 0}
            {System.show 'client: '#{ZN.msgData Message}}
            {ZN.msgClose Message}

            {ZN.close Socket}

    else
        {System.showInfo 'Usage: ./testZmq.exe [server|client]'}
    end

    {ZN.term Context}

    {Application.exit 0}
end


