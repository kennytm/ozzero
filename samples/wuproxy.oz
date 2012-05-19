% Weather proxy device

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    Application

define
    Context = {ZeroMQ.init}

    % This is where the weather server sits
    Frontend = {Context connectSocket(sub 'tcp://localhost:5556' $)}

    % This is our public endpoint for subscribers
    Backend = {Context bindSocket(pub 'tcp://*:8100' $)}
in
    % Subscribe on everything
    {Frontend set(subscribe:nil)}

    % Shunt messages out to our own subscribers
    for _ in _;_ do
        % Process all parts of the message
        {Backend sendMulti({Frontend recvMulti($)})}
    end

    % We don't actually get here but if we did, we'd shut down neatly
    {Frontend close}
    {Backend close}
    {Context close}
    {Application.exit 0}
end

