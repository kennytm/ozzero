% Hello World client
% Connects REQ socket to tcp://localhost:5559
% Sends "Hello" to server, expects "World" back

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    Application
    System

define
    Context = {ZeroMQ.init}

    % Socket to talk to server
    Requester = {Context.connect req('tcp://localhost:5559')}

in
    for I in 1..10 do
        Str
    in
        {Requester.send 'Hello'}
        Str = {Requester.recv}
        {System.showInfo 'Received reply '#I#' ['#Str#']'}
    end

    {Requester.close}
    {Context.close}
    {Application.exit 0}
end

