% Hello World server
% Connects REP socket to tcp://*:5560
% Expects "Hello" from client, replies with "World"

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    Application
    System

define
    Context = {ZeroMQ.init}

    % Socket to talk to clients
    Responder = {Context.connect rep('tcp://localhost:5560')}

    proc {ServerLoop}
        % Wait for next request from client
        Str = {Responder.recv}
    in
        {System.showInfo 'Received request: ['#Str#']'}
        % Do some 'work'
        {Delay 1000}
        % Send reply back to client
        {Responder.send 'World'}

        {ServerLoop}
    end

in
    {ServerLoop}

    % We never get here but clean up anyhow
    {Responder.close}
    {Context.close}
    {Application.exit 0}
end

