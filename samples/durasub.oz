% Durable subscriber

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    Application
    System

define
    Context = {ZeroMQ.init}

    % Connect our subscriber socket
    Subscriber = {Context.connect sub('tcp://localhost:5565'
                                      identity: 'Hello'
                                      subscribe: nil
                                      )}

    Sync = {Context.connect push('tcp://localhost:5564')}

    proc {SubscriberLoop}
        Str = {ByteString.toString {Subscriber.recv}}
    in
        {System.showInfo Str}
        if Str \= "END" then
            {SubscriberLoop}
        end
    end

in
    if ZeroMQ.version.major >= 3 then
        {System.showInfo 'Warning: Durable sockets are not supported in ZeroMQ 3.x'}
    end

    % Synchronize with publisher
    {Sync.send nil}

    % Get updates, exit when told to do so
    {SubscriberLoop}

    {Sync.close}
    {Subscriber.close}
    {Context.close}
    {Application.exit 0}
end
