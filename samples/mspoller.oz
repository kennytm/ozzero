% Reading from multiple sockets
% This version uses {ZeroMQ.poll}

functor
import
    ZeroMQ at '../ZeroMQ.ozf'
    Application
    System

define
    Context = {ZeroMQ.init}

    % Connect to task ventilator
    Receiver = {Context connectSocket(pull 'tcp://localhost:5557' $)}

    % Connect to weather server
    Subscriber = {Context connectSocket(sub 'tcp://localhost:5556' $)}

    proc {ProcessLoop}
        fun {MakeAction SocketName}
            proc {$ Socket Events}
                {System.showInfo 'Process '#SocketName#': '#{Socket recv($)}}
            end
        end
    in
        % Process messages from both sockets
        {ZeroMQ.poll r(
            r(socket:Receiver  events:pollin  action:{MakeAction 'task'})
            r(socket:Subscriber  events:pollin  action:{MakeAction 'weather update'})
        )}
        {ProcessLoop}
    end
in
    {Subscriber set(subscribe:'10001 ')}

    {ProcessLoop}

    % We never get here
    {Receiver close}
    {Subscriber close}
    {Context close}
    {Application.exit 0}
end

