% Pubsub envelope subscriber

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    Application
    System

define
    % Prepare our context and subscriber
    Context = {ZeroMQ.init}
    Subscriber = {Context connectSocket(sub 'tcp://localhost:5563' $)}

    proc {SubscriberLoop}
        Address  Contents
    in
        [Address Contents] = {Subscriber recvMulti($)}
        {System.showInfo '['#Address#'] '#Contents}
        {SubscriberLoop}
    end

in
    {Subscriber set(subscribe:'B')}
    {SubscriberLoop}

    % We never get here but clean up anyhow
    {Subscriber close}
    {Context close}
    {Application.exit 0}
end

