% Hello World client in Mozart/Oz
% Connects REQ socket to tcp://*:5555
% Sends "Hello" to server, expects "World" back

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    Application
    System

define
    % Socket to talk to server
    Context = {ZeroMQ.init}
    Socket = {Context connect(req('tcp://localhost:5555') $)}
in
    for I in 1..10 do
        {System.showInfo "Sending Hello "#I#"..."}
        {Socket send('Hello')}
        {Socket recv(_)}
        {System.showInfo "Received World "#I#"..."}
    end

    {Socket close}
    {Context close}
    {Application.exit 0}
end

