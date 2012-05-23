% Publisher for durable subscriber

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    Application

define
    Context = {ZeroMQ.init}

    % Subscriber tells us when it's ready here
    Sync = {Context bind(pull('tcp://*:5564') $)}

    % We send updates via this socket
    Publisher = {Context bind(pub('tcp://*:5565') $)}

in
    % Wait for synchronization request
    {Sync recv(_)}

    % Now broadcast exactly 10 updates with pause
    for UpdateNbr in 1..10 do
        {Publisher send('Update '#UpdateNbr)}
        {Delay 1000}
    end

    {Publisher send('END')}

    {Sync close}
    {Publisher close}
    {Context close}

    {Application.exit 0}
end


