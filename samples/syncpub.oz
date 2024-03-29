% Synchronized publisher

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    System
    Application

define
    % We wait for 10 subscribers
    SubscribersExpected = 10

    Context = {ZeroMQ.init}

    % Socket to talk to clients
    Publisher = {Context bind(pub('tcp://*:5561' sndhwm:400002) $)}

    % Socket to receive signals
    Syncservice = {Context bind(rep('tcp://*:5562') $)}

in
    % Get synchronization from subscribers
    {System.showInfo 'Waiting for subscribers'}

    for _ in 1..SubscribersExpected do
        % - wait for synchronization request
        {Syncservice recv(_)}
        % - send synchronization reply
        {Syncservice send(nil)}
    end

    % Now broadcast exactly 0.5M updates followed by END
    {System.showInfo 'Broadcasting messages'}
    for I in 1..500000 do
        {Publisher send(I)}
    end
    {Publisher send('END')}

    {System.showInfo 'Broadcast done, quitting'}

    {Publisher close}
    {Syncservice close}
    {Context close}
    {Application.exit 0}
end

