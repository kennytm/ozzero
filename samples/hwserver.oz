% Hello World server in Mozart/Oz
% Binds REP socket to tcp://*:5555
% Expects "Hello" from client, replies with "World"

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    Application

define
    % Prepare out context and socket
    Context = {ZeroMQ.init}
    Socket = {Context bindSocket(rep 'tcp://*:5555' $)}

    proc {ServerLoop}
        % Wait for next request from client
        {Socket recv(_)}
        % Do some 'work'
        {Delay 1000}
        % Send reply back to client
        {Socket send('World')}
        % Loop again
        {ServerLoop}
    end
in
    {ServerLoop}

    {Socket close}
    {Context close}

    {Application.exit 0}
end

