% Multithreaded Hello World server

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    System
    Application

define
    Context = {ZeroMQ.init}

    proc {WorkerRoutine Receiver}
        Str = {Receiver.recv}
    in
        {System.showInfo 'Received Request: ['#Str#']'}
        % Do some 'work'
        {Delay 1000}
        % Send reply back to client
        {Receiver.send 'World'}
        {WorkerRoutine Receiver}
    end

    % Socket to talk to clients
    Clients = {Context.bind router('tcp://*:5555')}

    % Socket to talk to workers
    Workers = {Context.bind dealer('inproc://workers')}

in
    % Launch pool of worker threads
    for _ in 1..5 do
        thread
            % Socket to talk to dispatcher
            Receiver = {Context.connect rep('inproc://workers')}
        in
            {WorkerRoutine Receiver}
            {Receiver.close}
        end
    end

    % Connect work threads to client threads via a queue
    {ZeroMQ.device queue Clients Workers}

    % We never get here but clean up anyhow
    {Clients.close}
    {Workers.close}
    {Context.close}
    {Application.exit 0}
end


