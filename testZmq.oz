functor
import
    ZeroMQ
    %ZN at 'z14.so{native}'
    System
    Application
define
    Version
    Context
in
    Version = ZeroMQ.version
    {System.show Version}

    Context = {ZeroMQ.init}
    {System.show Context}

    case {Application.getArgs plain}
    of "server"|_ then
        Socket = {Context socket(rep $)}
        VS
    in
        /**/ {System.showInfo 'server'}
        {Socket bind('tcp://*:5555')}
        /**/ {System.showInfo 'bound'}
        VS = {Socket recv($)}
        /**/ {System.showInfo 'received'}
        {System.show 'server: '#VS}
        {Delay 1000}
        {Socket send("World")}
        {Socket close}

    [] "client"|_ then
        Socket = {Context socket(req $)}
        VS
    in
        {System.showInfo 'client'}
        {Socket connect('tcp://localhost:5555')}
        {Socket send('Hello')}
        VS = {Socket recv($)}
        {System.showInfo VS}
        {Socket close}

    else
        {System.showInfo 'Usage: ./testZmq.exe [server|client]'}
    end

    {Context close}

    {Application.exit 0}
end


