% Simple request-reply broker

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    Application

define
    % Prepare our context and sockets
    Context = {ZeroMQ.init}
    Frontend = {Context bindSocket(router 'tcp://*:5559' $)}
    Backend = {Context bindSocket(dealer 'tcp://*:5560' $)}

    % Switch messages between sockets
    fun {MessageForwarder Source Target}
        proc {$ _ _}
            {Target sendMulti({Source recvMulti($)})}
        end
    end

    % Initialize poll set
    PollSet = [
        r(socket:Frontend  events:pollin  action:{MessageForwarder Frontend Backend})
        r(socket:Backend  events:pollin  action:{MessageForwarder Backend Frontend})
    ]
in
    for _ in _;_ do
        {ZeroMQ.poll PollSet}
    end

    % We never get here but clean up anyhow
    {Frontend close}
    {Backend close}
    {Context close}
    {Application.exit 0}
end

